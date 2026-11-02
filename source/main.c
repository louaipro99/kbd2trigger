#include <ppu-types.h>
#include <sys/thread.h>
#include <sys/systime.h>
#include <sys/prx.h>
#include <io/kb.h>
#include <io/pad.h>

/* PRX parameter section — mirrors lv2-sprx.S exactly */
__asm__ (
    "    .align 2
"
    "    .section \".lib.ent\",\"a\"
"
    "    .align 2
"
    "    .section \".lib.stub\",\"a\"
"
    "    .align 2
"
    "    .section \".sys_proc_prx_param\",\"a\"
"
    "    .long 0x28
"
    "    .long 0x1b434cec
"
    "    .long 2
"
    "    .long 0
"
    "    .long __libentstart
"
    "    .long __libentend
"
    "    .long __libstubstart
"
    "    .long __libstubend
"
    "    .short 0x0101
"
    "    .short 0
"
    "    .long 0
"
    "    .text
"
);

#define TRIGGER_MAX 255

static volatile int running = 0;
static sys_ppu_thread_t tid;

static u8 inject_l2 = 0;
static u8 inject_l1 = 0;
static u8 inject_r1 = 0;
static u8 inject_r2 = 0;

static void kbd_thread(void *arg)
{
    KbInfo kbInfo;
    KbData kbData;

    ioKbInit(1);
    ioKbSetReadMode(0, KB_RMODE_PACKET);
    ioKbSetCodeType(0, KB_CODETYPE_RAW);

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
    sysThreadCreate(&tid, kbd_thread, NULL, 1001, 0x4000, THREAD_JOINABLE, "kbd2trigger");
    return SYS_PRX_RESIDENT;
}

s32 module_stop(void)
{
    u64 retval;
    running = 0;
    sysThreadJoin(tid, &retval);
    return SYS_PRX_STOP_SUCCESS;
}
