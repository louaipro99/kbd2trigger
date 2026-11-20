#include <sys/prx.h>
#include <sys/ppu_thread.h>
#include <sys/timer.h>
#include <sys/return_code.h>
#include <io/pad.h>
#include <io/keyboard.h>
#include <string.h>

SYS_MODULE_INFO(kbd2trigger, 0, 1, 0);
SYS_MODULE_START(module_start);
SYS_MODULE_STOP(module_stop);

/* USB HID keycodes */
#define KEY_A     0x04
#define KEY_S     0x16
#define KEY_L     0x0F
#define KEY_SEMI  0x33

/* CellPadData.button[] analog indices */
#define PAD_L2   14
#define PAD_R2   15
#define PAD_L1    6
#define PAD_R1    7

static sys_ppu_thread_t g_thread;
static volatile int     g_running = 1;

static void kbd_thread(uint64_t arg)
{
    (void)arg;

    ioPadInit(1);
    ioKbInit(1);
    ioKbOpen(0);

    IoKbData   kbdata;
    IoKbConfig kbcfg;
    ioPadData  paddata;

    /* set raw keycode mode */
    kbcfg.mapping = 0;
    ioKbSetCodeType(0, KB_CODETYPE_RAW);

    while (g_running)
    {
        memset(&kbdata,  0, sizeof(kbdata));
        memset(&paddata, 0, sizeof(paddata));

        if (ioKbRead(0, &kbdata) == 0 && kbdata.nb_keycode > 0)
        {
            ioPadGetData(0, &paddata);

            for (int i = 0; i < kbdata.nb_keycode; i++)
            {
                switch (kbdata.keycode[i])
                {
                    case KEY_A:    paddata.button[PAD_L2] = 0xFF; break;
                    case KEY_S:    paddata.button[PAD_L1] = 0xFF; break;
                    case KEY_L:    paddata.button[PAD_R1] = 0xFF; break;
                    case KEY_SEMI: paddata.button[PAD_R2] = 0xFF; break;
                    default: break;
                }
            }
        }

        sys_timer_usleep(8000);
    }

    ioKbClose(0);
    ioKbEnd();
    ioPadEnd();
    sys_ppu_thread_exit(0);
}

SYS_MODULE_START_FUNC(module_start)
{
    g_running = 1;
    sys_ppu_thread_create(&g_thread, kbd_thread, 0,
                          1001, 0x4000,
                          SYS_PPU_THREAD_CREATE_JOINABLE,
                          "kbd2trigger");
    return SYS_PRX_RESIDENT;
}

SYS_MODULE_STOP_FUNC(module_stop)
{
    g_running = 0;
    uint64_t exit_code;
    sys_ppu_thread_join(g_thread, &exit_code);
    return SYS_PRX_STOP_SUCCESS;
}
