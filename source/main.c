
#include <sys/prx.h>
#include <sys/syscall.h>
#include <sys/ppu_thread.h>
#include <sys/timer.h>
#include <sys/return_code.h>
#include <cell/pad.h>
#include <cell/keyboard.h>
#include <string.h>

SYS_MODULE_INFO(kbd2trigger, 0, 1, 0);
SYS_MODULE_START(module_start);
SYS_MODULE_STOP(module_stop);

/* ── key codes (USB HID via cellKb) ──────────────────────────────────────── */
#define KEY_A   0x04   /* HID usage: Keyboard a */
#define KEY_S   0x16   /* HID usage: Keyboard s */
#define KEY_L   0x0F   /* HID usage: Keyboard l */
#define KEY_SEMI 0x33  /* HID usage: Keyboard ; */

/* ── PS3 pad button byte offsets in CellPadData.button[] ─────────────────── */
/* button[2] = digital buttons 1, button[3] = digital buttons 2             */
/* Analog axes: button[14]=L2, button[15]=R2, button[6]=L1, button[7]=R1   */
#define PAD_BTN_L2   14
#define PAD_BTN_R2   15
#define PAD_BTN_L1    6
#define PAD_BTN_R1    7

static sys_ppu_thread_t g_thread;
static volatile int     g_running = 1;

/* ── keyboard poll thread ─────────────────────────────────────────────────── */
static void kbd_thread(uint64_t arg)
{
    (void)arg;

    /* open keyboard port 0 */
    cellKbInit(1);
    cellKbOpen(0);

    CellKbData   kbdata;
    CellPadData  paddata;

    while (g_running)
    {
        memset(&kbdata,  0, sizeof(kbdata));
        memset(&paddata, 0, sizeof(paddata));

        /* read keyboard */
        if (cellKbRead(0, &kbdata) != CELL_OK || kbdata.len == 0)
        {
            sys_timer_usleep(8000);
            continue;
        }

        /* read current pad state for port 0 */
        cellPadGetData(0, &paddata);

        /* map keys → triggers/bumpers */
        for (int i = 0; i < kbdata.len; i++)
        {
            uint16_t key = kbdata.keycode[i];
            switch (key)
            {
                case KEY_A:    paddata.button[PAD_BTN_L2] = 0xFF; break; /* A -> L2 */
                case KEY_S:    paddata.button[PAD_BTN_L1] = 0xFF; break; /* S -> L1 */
                case KEY_L:    paddata.button[PAD_BTN_R1] = 0xFF; break; /* L -> R1 */
                case KEY_SEMI: paddata.button[PAD_BTN_R2] = 0xFF; break; /* ; -> R2 */
                default: break;
            }
        }

        sys_timer_usleep(8000); /* ~125 Hz poll */
    }

    cellKbClose(0);
    cellKbEnd();
    sys_ppu_thread_exit(0);
}

/* ── PRX entry / exit ─────────────────────────────────────────────────────── */
SYS_MODULE_START_FUNC(module_start)
{
    g_running = 1;
    sys_ppu_thread_create(&g_thread, kbd_thread, 0,
                          1001,       /* priority */
                          0x4000,     /* stack size */
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
