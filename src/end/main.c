#include <kf/lib/address.h>
#include <kf/end/ending.h>
#include <kf/lib/audio.h>
#include <kf/lib/display.h>
#include <kf/lib/overlay.h>
#include <psyq/cd.h>
#include <psyq/kernel.h>
#include <psyq/libc.h>
#include <psyq/pad.h>

/* Heap placement literals; the original definitions are unresolved. */
#define ENDING_HEAP_BASE ((u_long *)0x80100000)
#define ENDING_HEAP_BYTES 0xf8000

DATA(0x8003975c, 0x4, ".sdata")
u8 *overlay_next_request = (u8 *)0x800102f0;

/* US END: plays ED0.S, then ED1.S, then waits forever. */
ADDRESS(0x800119a4, 0xc4)
void main(void)
{
    /* Unused: retail reserves this 8-byte slot in main's frame, where OPEN's
     * copy of main keeps its RECT. The declaration survives from that shared
     * template. */
    RECT rect;

    ResetCallback();
    InitHeap((void *)ENDING_HEAP_BASE, ENDING_HEAP_BYTES);
    CdInit();
    PadInit(0);
    ExitCriticalSection();
    audio_initialize();
    display_initialize();
    display_current = &display_buffers[0];
    PutDrawEnv(&display_current->draw);
    PutDispEnv(&display_current->disp);
    display_current = &display_buffers[1];
    PutDrawEnv(&display_current->draw);
    PutDispEnv(&display_current->disp);
    ending_play_movie(0);
    ending_play_movie(1);
    for (;;) {
    }
}
