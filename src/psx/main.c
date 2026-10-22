#include <kf/lib/address.h>
#include <kf/lib/null.h>
#include <kf/lib/overlay.h>
#include <kf/lib/types.h>
#include <psyq/kernel.h>

RODATA(0x80010000, 0x35)

DATA(0x80010254, 0xc, ".data")
const char *overlay_path_table[3] = {
    "cdrom:OPEN.EXE;1",
    "cdrom:GAME.EXE;1",
    "cdrom:END.EXE;1",
};

DATA(0x80010268, 0x4, ".sdata")
long overlay_index = KF_OVERLAY_OPEN;

/* Fixed mailbox byte that GAME.EXE writes to select the next overlay.
 * Unresolved: how the original spelled or obtained this address. */
DATA(0x8001026c, 0x4, ".sdata")
u8 *overlay_next_request = (u8 *)0x800102f0;

DATA(0x80010274, 0x3c, ".bss")
struct EXEC overlay_header;

/* Loads and runs the overlay named by overlay_index forever; each overlay
 * leaves the index of its successor in the mailbox byte. */
ADDRESS(0x80010038, 0xe8)
void main(void)
{
    SetMem(KF_MAIN_RAM_MEGABYTES);
    _96_remove();
    _96_init();
    *overlay_next_request = KF_OVERLAY_OPEN;
    for (;;) {
        if (Load(overlay_path_table[overlay_index], &overlay_header) == 1) {
            _96_remove();
            overlay_header.s_addr = 0;
            overlay_header.s_size = 0;
            EnterCriticalSection();
            Exec(&overlay_header, 0, NULL);
            overlay_index = *overlay_next_request;
            _96_init();
        }
    }
}
