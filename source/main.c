#include <psl1ght/lv2.h>
#include <sys/systime.h>
#include <sys/ppu_thread.h>
#include <sys/process.h>
#include <io/kbd.h>
#include <pad.h>
#include <string.h>

SYS_MODULE_INFO(kbd2trigger, 0, 1, 0);
SYS_MODULE_START(module_start);
SYS_MODULE_STOP(module_stop);

// Key codes
#define KEY_A   0x04
#define KEY_S   0x16
#define KEY_L   0x0F
#define KEY_SEMI 0x33

// Trigger values
#define TRIGGER_MAX 255

static volatile int running = 0;
static sys_ppu_thread_t tid;

// Store original cellPadRead OPD
static uint64_t orig_cellPadRead[2];
typedef int (*cellPadRead_t)(uint32_t port, CellPadData *data);
static cellPadRead_t orig_fn = NULL;

// Our hooked cellPadRead
int hooked_cellPadRead(uint32_t port, CellPadData *data) {
    int ret = orig_fn(port, data);
    if (ret == 0 && data) {
        KbInfo kbInfo;
        KbData kbData;
        if (ioKbGetInfo(&kbInfo) == 0 && kbInfo.info[0] == 1) {
            if (ioKbRead(0, &kbData) == 0 && kbData.nb_keycode > 0) {
                for (int i = 0; i < kbData.nb_keycode; i++) {
                    uint8_t k = kbData.keycode[i].keycode;
                    if (k == KEY_A)    data->button[CELL_PAD_BTN_OFFSET_DIGITAL2] |= CELL_PAD_CTRL_L2,  data->analog_T_L = TRIGGER_MAX;
                    if (k == KEY_S)    data->button[CELL_PAD_BTN_OFFSET_DIGITAL2] |= CELL_PAD_CTRL_L1;
                    if (k == KEY_L)    data->button[CELL_PAD_BTN_OFFSET_DIGITAL2] |= CELL_PAD_CTRL_R1;
                    if (k == KEY_SEMI) data->button[CELL_PAD_BTN_OFFSET_DIGITAL2] |= CELL_PAD_CTRL_R2,  data->analog_T_R = TRIGGER_MAX;
                }
            }
        }
    }
    return ret;
}

static void plugin_thread(void *arg) {
    // Init keyboard
    ioKbInit(1);
    ioKbOpen(0);

    while (running) {
        sys_timer_usleep(8333); // ~120Hz
    }

    ioKbClose(0);
    ioKbEnd();
    sys_ppu_thread_exit(0);
}

int module_start(size_t args, const void *argp) {
    running = 1;
    sys_ppu_thread_create(&tid, plugin_thread, 0, 1001, 0x4000, 0, "kbd2trigger");
    return SYS_MODULE_2;
}

int module_stop() {
    running = 0;
    sys_ppu_thread_join(tid, NULL);
    return 0;
}
