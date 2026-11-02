#include <ppu-types.h>
#include <sys/thread.h>
#include <sys/systime.h>
#include <sys/prx.h>
#include <sys/memory.h>
#include <io/kb.h>
#include <io/pad.h>
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

#define TRIGGER_MAX   255
#define SEARCH_START  0x00010000UL
#define SEARCH_END    0x30000000UL
#define SEARCH_STEP   4

/* cellPadRead FNID - used to locate the import stub in the game's memory */
#define CELLPAD_READ_FNID  0x6E63AF67

typedef s32 (*cellPadRead_t)(u32 port, CellPadData *data);

/* OPD = two 32-bit words: [function_ptr, TOC] */
typedef struct { u32 func; u32 toc; } Opd32;

static volatile int    running       = 0;
static sys_ppu_thread_t tid;

/* saved original OPD so we can call through and restore on unload */
static Opd32           orig_opd      = {0, 0};
static Opd32          *patched_addr  = NULL;

static u8 inject_l2 = 0;
static u8 inject_l1 = 0;
static u8 inject_r1 = 0;
static u8 inject_r2 = 0;

/* ------------------------------------------------------------------ */
/* Our replacement cellPadRead                                          */
/* ------------------------------------------------------------------ */
static s32 hooked_cellPadRead(u32 port, CellPadData *data)
{
    /* call original via saved OPD */
    cellPadRead_t orig = (cellPadRead_t)(u64)&orig_opd;
    s32 ret = orig(port, data);

    if (ret == 0 && data && data->len > 0) {
        if (inject_l2) {
            data->button[CELL_PAD_BTN_OFFSET_DIGITAL2] |= CELL_PAD_CTRL_L2;
            data->button[CELL_PAD_BTN_OFFSET_ANALOG_LEFT_TRIGGER] = TRIGGER_MAX;
        }
        if (inject_l1)
            data->button[CELL_PAD_BTN_OFFSET_DIGITAL2] |= CELL_PAD_CTRL_L1;
        if (inject_r1)
            data->button[CELL_PAD_BTN_OFFSET_DIGITAL2] |= CELL_PAD_CTRL_R1;
        if (inject_r2) {
            data->button[CELL_PAD_BTN_OFFSET_DIGITAL2] |= CELL_PAD_CTRL_R2;
            data->button[CELL_PAD_BTN_OFFSET_ANALOG_RIGHT_TRIGGER] = TRIGGER_MAX;
        }
    }
    return ret;
}

/* OPD for our hook - the game will jump here */
static Opd32 hook_opd;

/* ------------------------------------------------------------------ */
/* Scan game memory for cellPadRead import stub and patch its OPD      */
/* ------------------------------------------------------------------ */
static int patch_cellpad_read(void)
{
    u32 *p;

    /* Build our hook OPD:
       func = address of hooked_cellPadRead (from its own OPD word 0)
       toc  = our plugin TOC (from our own function's OPD word 1)      */
    Opd32 *self_opd = (Opd32 *)(u64)hooked_cellPadRead;
    hook_opd.func = self_opd->func;
    hook_opd.toc  = self_opd->toc;

    for (u32 addr = SEARCH_START; addr < SEARCH_END; addr += SEARCH_STEP) {
        p = (u32 *)(u64)addr;

        /* Look for the cellPadRead FNID pattern in an import stub:
           The stub area typically contains the FNID followed by the OPD ptr */
        if (*p == CELLPAD_READ_FNID) {
            /* The OPD pointer is at p-1 in the .lib.stub layout:
               [nid_table_ptr][func_table_ptr] then entries:
               [FNID][OPD_ptr]
               So the OPD we want to patch is at *(p+1) */
            Opd32 *opd_ptr = (Opd32 *)(u64)(*(p + 1));

            /* Validate it looks like a real OPD (in game text range) */
            if ((u64)opd_ptr >= SEARCH_START && (u64)opd_ptr < SEARCH_END) {
                /* Save original and patch */
                orig_opd    = *opd_ptr;
                patched_addr = opd_ptr;
                *opd_ptr    = hook_opd;

                /* Flush cache lines */
                __asm__ volatile("dcbst 0,%0; sync; icbi 0,%0; isync"
                                 :: "r"(opd_ptr) : "memory");
                return 1; /* patched */
            }
        }
    }
    return 0; /* not found */
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
/* Keyboard polling thread                                              */
/* ------------------------------------------------------------------ */
static void kbd_thread(void *arg)
{
    KbInfo kbInfo;
    KbData kbData;

    ioKbInit(1);
    ioKbSetReadMode(0, KB_RMODE_PACKET);
    ioKbSetCodeType(0, KB_CODETYPE_RAW);

    /* Wait a bit for the game to fully load before patching */
    sysUsleep(3000000); /* 3 seconds */
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
