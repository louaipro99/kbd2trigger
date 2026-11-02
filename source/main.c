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

#define TRIGGER_MAX          255
#define SEARCH_START         0x00010000UL
#define SEARCH_END           0x30000000UL
#define CELLPAD_READ_FNID    0x6E63AF67

/* PSL1GHT uses padData, not CellPadData */
typedef s32 (*cellPadRead_t)(u32 port, padData *data);

/* OPD = [function_ptr, TOC] on PPC64 */
typedef struct { u32 func; u32 toc; } Opd32;

static volatile int     running      = 0;
static sys_ppu_thread_t tid;
static Opd32            orig_opd     = {0, 0};
static Opd32           *patched_addr = NULL;

static u8 inject_l2 = 0;
static u8 inject_l1 = 0;
static u8 inject_r1 = 0;
static u8 inject_r2 = 0;

/* Forward declaration so patch_cellpad_read can reference it */
static s32 hooked_cellPadRead(u32 port, padData *data);

/* OPD entry for our hook function */
static Opd32 hook_opd;

/* ------------------------------------------------------------------ */
/* Scan game memory for cellPadRead import stub and patch its OPD      */
/* ------------------------------------------------------------------ */
static int patch_cellpad_read(void)
{
    /* Build our hook OPD from hooked_cellPadRead's own OPD */
    Opd32 *self_opd = (Opd32 *)(u64)hooked_cellPadRead;
    hook_opd.func = self_opd->func;
    hook_opd.toc  = self_opd->toc;

    u32 *p;
    for (u32 addr = SEARCH_START; addr < SEARCH_END; addr += 4) {
        p = (u32 *)(u64)addr;
        if (*p == CELLPAD_READ_FNID) {
            /* Import stub layout: FNID at p, OPD ptr at p+1 */
            Opd32 *opd_ptr = (Opd32 *)(u64)(*(p + 1));
            if ((u64)opd_ptr >= SEARCH_START && (u64)opd_ptr < SEARCH_END) {
                orig_opd     = *opd_ptr;
                patched_addr = opd_ptr;
                *opd_ptr     = hook_opd;
                __asm__ volatile("dcbst 0,%0; sync; icbi 0,%0; isync"
                                 :: "r"(opd_ptr) : "memory");
                return 1;
            }
        }
    }
    return 0;
}

static void unpatch_cellpad_read(void)
{
    if (patched_addr) {
        *patched_addr = orig_opd;
        __asm__ volatile("dcbst 0,%0; sync; icbi 0,%0; isync"
                         :: "r"(patched_addr) : "memory");
        patched_addr = NULL;
    }
}

/* ------------------------------------------------------------------ */
/* Our hooked cellPadRead - injects keyboard inputs into pad data       */
/* ------------------------------------------------------------------ */
static s32 hooked_cellPadRead(u32 port, padData *data)
{
    cellPadRead_t orig = (cellPadRead_t)(u64)&orig_opd;
    s32 ret = orig(port, data);

    if (ret == 0 && data && data->len > 0) {
        if (inject_l2) {
            data->button[CELL_PAD_BTN_OFFSET_DIGITAL2] |= PAD_CTRL_L2;
            data->button[CELL_PAD_BTN_OFFSET_ANALOG_LEFT_TRIGGER] = TRIGGER_MAX;
        }
        if (inject_l1)
            data->button[CELL_PAD_BTN_OFFSET_DIGITAL2] |= PAD_CTRL_L1;
        if (inject_r1)
            data->button[CELL_PAD_BTN_OFFSET_DIGITAL2] |= PAD_CTRL_R1;
        if (inject_r2) {
            data->button[CELL_PAD_BTN_OFFSET_DIGITAL2] |= PAD_CTRL_R2;
            data->button[CELL_PAD_BTN_OFFSET_ANALOG_RIGHT_TRIGGER] = TRIGGER_MAX;
        }
    }
    return ret;
}

/* ------------------------------------------------------------------ */
/* Keyboard polling thread                                              */
/* ------------------------------------------------------------------ */
static void kbd_thread(void *arg)
{
    KbInfo kbInfo;
    KbData kbData;

    ioKbInit(1);
    ioKbSetReadMode(0, KB_RMODE_PACKET);
    ioKbSetCodeType(0, KB_CODETYPE_RAW);

    /* Wait 3s for game to fully load before patching */
    sysUsleep(3000000);
    patch_cellpad_read();

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

/* ------------------------------------------------------------------ */
/* Module entry / exit                                                  */
/* ------------------------------------------------------------------ */
s32 module_start(size_t args, const void *argp)
{
    running = 1;
    sysThreadCreate(&tid, kbd_thread, NULL, 1001, 0x4000,
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
