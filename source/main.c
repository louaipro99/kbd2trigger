#include <ppu-types.h>
#include <sys/thread.h>
#include <sys/systime.h>
#include <sys/prx.h>
#include <io/kb.h>
#include <io/pad.h>

/* PRX parameter section - mirrors lv2-sprx.S */
__asm__(
    "   .align 2\n"
    "   .section \".lib.ent\",\"a\"\n"
    "   .align 2\n"
    "   .section \".lib.stub\",\"a\"\n"
    "   .align 2\n"
    "   .section \".sys_proc_prx_param\",\"a\"\n"
    "   .long 0x28\n"
    "   .long 0x1b434cec\n"
    "   .long 2\n"
    "   .long 0\n"
    "   .long __libentstart\n"
    "   .long __libentend\n"
    "   .long __libstubstart\n"
    "   .long __libstubend\n"
    "   .short 0x0101\n"
    "   .short 0\n"
    "   .long 0\n"
    "   .text\n"
);

#define TRIGGER_MAX        255
#define CELLPAD_READ_FNID  0x6E63AF67

/* Safe scan range - only valid game memory, avoid kernel/VSH areas */
#define SCAN_START  0x00010000UL
#define SCAN_END    0x0FF00000UL

typedef s32 (*cellPadRead_t)(u32 port, padData *data);
typedef struct { u32 func; u32 toc; } Opd32;

static volatile int     running      = 0;
static sys_ppu_thread_t tid;
static Opd32            orig_opd     = {0, 0};
static Opd32           *patched_addr = NULL;
static volatile int     hooked       = 0;

static u8 inject_l2 = 0;
static u8 inject_l1 = 0;
static u8 inject_r1 = 0;
static u8 inject_r2 = 0;

static s32 hooked_cellPadRead(u32 port, padData *data);
static Opd32 hook_opd;

/* Safe memory probe - check if address is readable without crashing */
static int mem_probe(u32 addr)
{
    /* Skip known bad ranges */
    if (addr < SCAN_START || addr >= SCAN_END) return 0;
    if (addr & 3) return 0; /* must be 4-byte aligned */
    return 1;
}

static int patch_cellpad_read(void)
{
    Opd32 *self_opd = (Opd32 *)(u64)hooked_cellPadRead;
    hook_opd.func = self_opd->func;
    hook_opd.toc  = self_opd->toc;

    for (u32 addr = SCAN_START; addr < SCAN_END; addr += 4) {
        if (!mem_probe(addr)) continue;
        u32 *p = (u32 *)(u64)addr;
        if (*p == CELLPAD_READ_FNID) {
            u32 opd_addr = *(p + 1);
            if (!mem_probe(opd_addr)) continue;
            Opd32 *opd_ptr = (Opd32 *)(u64)opd_addr;
            if (!mem_probe(opd_ptr->func)) continue;
            orig_opd     = *opd_ptr;
            patched_addr = opd_ptr;
            *opd_ptr     = hook_opd;
            __asm__ volatile("dcbst 0,%0; sync; icbi 0,%0; isync"
                             :: "r"(opd_ptr) : "memory");
            hooked = 1;
            return 1;
        }
    }
    return 0;
}

static void unpatch_cellpad_read(void)
{
    if (patched_addr && hooked) {
        *patched_addr = orig_opd;
        __asm__ volatile("dcbst 0,%0; sync; icbi 0,%0; isync"
                         :: "r"(patched_addr) : "memory");
        patched_addr = NULL;
        hooked = 0;
    }
}

static s32 hooked_cellPadRead(u32 port, padData *data)
{
    cellPadRead_t orig = (cellPadRead_t)(u64)&orig_opd;
    s32 ret = orig(port, data);

    if (ret == 0 && data && data->len > 0) {
        if (inject_l2) {
            data->button[PAD_BUTTON_OFFSET_DIGITAL2] |= PAD_CTRL_L2;
            data->button[PAD_BUTTON_OFFSET_PRESS_L2]  = TRIGGER_MAX;
        }
        if (inject_l1)
            data->button[PAD_BUTTON_OFFSET_DIGITAL2] |= PAD_CTRL_L1;
        if (inject_r1)
            data->button[PAD_BUTTON_OFFSET_DIGITAL2] |= PAD_CTRL_R1;
        if (inject_r2) {
            data->button[PAD_BUTTON_OFFSET_DIGITAL2] |= PAD_CTRL_R2;
            data->button[PAD_BUTTON_OFFSET_PRESS_R2]  = TRIGGER_MAX;
        }
    }
    return ret;
}

static void kbd_thread(void *arg)
{
    KbInfo kbInfo;
    KbData kbData;

    ioKbInit(1);
    ioKbSetReadMode(0, KB_RMODE_PACKET);
    ioKbSetCodeType(0, KB_CODETYPE_RAW);

    /* Wait for game to finish loading, retry hook every second until found */
    while (running && !hooked) {
        patch_cellpad_read();
        sysUsleep(1000000); /* retry every 1s */
    }

    while (running) {
        inject_l2 = inject_l1 = inject_r1 = inject_r2 = 0;

        if (ioKbGetInfo(&kbInfo) == 0 && kbInfo.status[0] == 1) {
            if (ioKbRead(0, &kbData) == 0 && kbData.nb_keycode > 0) {
                for (int i = 0; i < kbData.nb_keycode; i++) {
                    u16 k = kbData.keycode[i];
                    if (k == KB_RAWKEY_A)         inject_l2 = 1;
                    if (k == KB_RAWKEY_S)         inject_l1 = 1;
                    if (k == KB_RAWKEY_L)         inject_r1 = 1;
                    if (k == KB_RAWKEY_SEMICOLON) inject_r2 = 1;
                }
            }
        }
        sysUsleep(8333);
    }

    ioKbEnd();
    sysThreadExit(0);
}

s32 module_start(size_t args, const void *argp)
{
    running = 1;
    sysThreadCreate(&tid, kbd_thread, NULL, 1001, 0x10000,
                    THREAD_JOINABLE, "kbd2trigger");
    return SYS_PRX_RESIDENT;
}

s32 module_stop(void)
{
    u64 retval;
    running = 0;
    unpatch_cellpad_read();
    sysThreadJoin(tid, &retval);
    return SYS_PRX_STOP_SUCCESS;
}
