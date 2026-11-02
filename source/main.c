#include <ppu-types.h>
#include <sys/thread.h>
#include <sys/systime.h>
#include <sys/prx.h>
#include <io/kb.h>
#include <io/pad.h>
#include <sysutil/msg.h>
#include <string.h>

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

static volatile int     running    = 0;
static sys_ppu_thread_t tid;
static s32              ldd_handle = -1;

static void kbd_thread(void *arg)
{
    KbInfo kbInfo;
    KbData kbData;
    padData pad;

    /* Init keyboard */
    ioKbInit(1);
    ioKbSetReadMode(0, KB_RMODE_PACKET);
    ioKbSetCodeType(0, KB_CODETYPE_RAW);

    /* Init pad and register virtual LDD controller */
    ioPadInit(5);
    ldd_handle = ioPadLddRegisterController();

    /* Wait 5 seconds then show notification to confirm plugin is running */
    sysUsleep(5000000);
    msgDialogOpen2(MSG_DIALOG_NORMAL | MSG_DIALOG_BTN_TYPE_OK,
                   "kbd2trigger loaded!\nA=Left  S=Down  L=Up  ;=Right",
                   NULL, NULL, NULL);
    sysUsleep(3000000);
    msgDialogClose(0.0f);

    while (running) {
        memset(&pad, 0, sizeof(padData));
        pad.len = 24;

        if (ioKbGetInfo(&kbInfo) == 0 && kbInfo.status[0] == 1) {
            if (ioKbRead(0, &kbData) == 0 && kbData.nb_keycode > 0) {
                for (int i = 0; i < kbData.nb_keycode; i++) {
                    u16 k = kbData.keycode[i];
                    if (k == KB_RAWKEY_A)
                        pad.button[PAD_BUTTON_OFFSET_DIGITAL1] |= PAD_CTRL_LEFT;
                    if (k == KB_RAWKEY_S)
                        pad.button[PAD_BUTTON_OFFSET_DIGITAL1] |= PAD_CTRL_DOWN;
                    if (k == KB_RAWKEY_L)
                        pad.button[PAD_BUTTON_OFFSET_DIGITAL1] |= PAD_CTRL_UP;
                    if (k == KB_RAWKEY_SEMICOLON)
                        pad.button[PAD_BUTTON_OFFSET_DIGITAL1] |= PAD_CTRL_RIGHT;
                }
            }
        }

        if (ldd_handle >= 0)
            ioPadLddDataInsert(ldd_handle, &pad);

        sysUsleep(8333);
    }

    if (ldd_handle >= 0) {
        ioPadLddUnregisterController(ldd_handle);
        ldd_handle = -1;
    }
    ioPadEnd();
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
    sysThreadJoin(tid, &retval);
    return SYS_PRX_STOP_SUCCESS;
}
