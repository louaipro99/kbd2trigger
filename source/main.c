#include <ppu-types.h>
#include <sys/thread.h>
#include <sys/systime.h>
#include <sys/prx.h>
#include <io/kb.h>
#include <io/pad.h>

SYS_MODULE_INFO(kbd2trigger, 0, 1, 0);
SYS_MODULE_START(module_start);
SYS_MODULE_STOP(module_stop);

#define TRIGGER_MAX 255

static volatile int running = 0;
static sys_ppu_thread_t tid;

/* Pad data we inject into — we hook by wrapping padRead */
static u8 inject_l2 = 0;
static u8 inject_l1 = 0;
static u8 inject_r1 = 0;
static u8 inject_r2 = 0;

/* Original padRead function pointer (resolved via import) */
typedef s32 (*padRead_fn)(u32 port, padData *data);
extern s32 ioPadRead(u32 port, padData *data);

static void kbd_thread(void *arg)
{
    KbInfo  kbInfo;
    KbData  kbData;

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
        sys_timer_usleep(8333); /* ~120 Hz */
    }

    ioKbEnd();
    sys_ppu_thread_exit(0);
}

/* Hook: called instead of ioPadRead by patching the import table OPD */
s32 hooked_ioPadRead(u32 port, padData *data)
{
    s32 ret = ioPadRead(port, data);
    if (ret == 0 && data && data->len > 0) {
        if (inject_l2) { data->button[CELL_PAD_BTN_OFFSET_DIGITAL2] |= PAD_CTRL_L2;  data->button[CELL_PAD_BTN_OFFSET_ANALOG_LEFT_TRIGGER]  = TRIGGER_MAX; }
        if (inject_l1) { data->button[CELL_PAD_BTN_OFFSET_DIGITAL2] |= PAD_CTRL_L1; }
        if (inject_r1) { data->button[CELL_PAD_BTN_OFFSET_DIGITAL2] |= PAD_CTRL_R1; }
        if (inject_r2) { data->button[CELL_PAD_BTN_OFFSET_DIGITAL2] |= PAD_CTRL_R2;  data->button[CELL_PAD_BTN_OFFSET_ANALOG_RIGHT_TRIGGER] = TRIGGER_MAX; }
    }
    return ret;
}

s32 module_start(size_t args, const void *argp)
{
    running = 1;
    sys_ppu_thread_create(&tid, kbd_thread, 0, 1001, 0x4000, SYS_PPU_THREAD_CREATE_JOINABLE, "kbd2trigger");
    return SYS_PRX_RESIDENT;
}

s32 module_stop(void)
{
    running = 0;
    sys_ppu_thread_join(tid, NULL);
    return 0;
}
