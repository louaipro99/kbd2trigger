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

/* PSL1GHT uses ioPadGetData, not ioPadRead */
typedef struct { u32 func; u32 toc; } Opd32;
typedef s32 (*ioPadGetData_t)(u32 port, padData *data);

static volatile int     running       = 0;
static sys_ppu_thread_t tid;

static u8 inject_left  = 0;
static u8 inject_down  = 0;
static u8 inject_up    = 0;
static u8 inject_right = 0;

static s32 hooked_ioPadGetData(u32 port, padData *data);
static Opd32  hook_opd;
static Opd32  orig_opd    = {0, 0};
static Opd32 *opd_ptr     = NULL;

static s32 hooked_ioPadGetData(u32 port, padData *data)
{
    ioPadGetData_t orig = (ioPadGetData_t)(u64)&orig_opd;
    s32 ret = orig(port, data);

    if (ret == 0 && data && data->len > 0) {
        if (inject_left)  data->button[PAD_BUTTON_OFFSET_DIGITAL1] |= PAD_CTRL_LEFT;
        if (inject_down)  data->button[PAD_BUTTON_OFFSET_DIGITAL1] |= PAD_CTRL_DOWN;
        if (inject_up)    data->button[PAD_BUTTON_OFFSET_DIGITAL1] |= PAD_CTRL_UP;
        if (inject_right) data->button[PAD_BUTTON_OFFSET_DIGITAL1] |= PAD_CTRL_RIGHT;
    }
    return ret;
}

static void patch_iopad(void)
{
    Opd32 *import_opd = (Opd32 *)(u64)ioPadGetData;
    Opd32 *hook       = (Opd32 *)(u64)hooked_ioPadGetData;

    hook_opd.func = hook->func;
    hook_opd.toc  = hook->toc;

    orig_opd = *import_opd;
    opd_ptr  = import_opd;
    *import_opd = hook_opd;

    __asm__ volatile("dcbst 0,%0; sync; icbi 0,%0; isync"
                     :: "r"(import_opd) : "memory");
}

static void unpatch_iopad(void)
{
    if (opd_ptr) {
        *opd_ptr = orig_opd;
        __asm__ volatile("dcbst 0,%0; sync; icbi 0,%0; isync"
                         :: "r"(opd_ptr) : "memory");
        opd_ptr = NULL;
    }
}

static void kbd_thread(void *arg)
{
    KbInfo kbInfo;
    KbData kbData;

    ioKbInit(1);
    ioKbSetReadMode(0, KB_RMODE_PACKET);
    ioKbSetCodeType(0, KB_CODETYPE_RAW);

    patch_iopad();

    while (running) {
        inject_left = inject_down = inject_up = inject_right = 0;

        if (ioKbGetInfo(&kbInfo) == 0 && kbInfo.status[0] == 1) {
            if (ioKbRead(0, &kbData) == 0 && kbData.nb_keycode > 0) {
                for (int i = 0; i < kbData.nb_keycode; i++) {
                    u16 k = kbData.keycode[i];
                    if (k == KB_RAWKEY_A)         inject_left  = 1;
                    if (k == KB_RAWKEY_S)         inject_down  = 1;
                    if (k == KB_RAWKEY_L)         inject_up    = 1;
                    if (k == KB_RAWKEY_SEMICOLON) inject_right = 1;
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
    unpatch_iopad();
    sysThreadJoin(tid, &retval);
    return SYS_PRX_STOP_SUCCESS;
}
