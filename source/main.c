#include <ppu-types.h>
#include <sys/thread.h>
#include <sys/systime.h>
#include <sys/prx.h>
#include <io/kb.h>
#include <io/pad.h>

/* Embed .sys_proc_prx_param directly — required by sprxlinker */
extern u32 __libentstart, __libentend;
extern u32 __libstubstart, __libstubend;

__attribute__((section(".lib.ent.top")))   static u8 _ent_top[0]  = {};
__attribute__((section(".lib.ent.btm")))   static u8 _ent_btm[0]  = {};
__attribute__((section(".lib.stub.top")))  static u8 _stub_top[0] = {};
__attribute__((section(".lib.stub.btm")))  static u8 _stub_btm[0] = {};

typedef struct {
    u32 size;
    u32 magic;
    u32 version;
    u32 unk0;
    u32 libent_start;
    u32 libent_end;
    u32 libstub_start;
    u32 libstub_end;
    u16 ver_major;
    u16 ver_minor;
    u32 unk1;
} SysPrxParam;

__attribute__((section(".sys_proc_prx_param")))
static const SysPrxParam prx_param = {
    0x28,
    0x1b434cec,
    2,
    0,
    (u32)(u64)&__libentstart,
    (u32)(u64)&__libentend,
    (u32)(u64)&__libstubstart,
    (u32)(u64)&__libstubend,
    0x0101,
    0,
    0
};

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
