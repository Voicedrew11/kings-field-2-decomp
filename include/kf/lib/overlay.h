#ifndef KF_OVERLAY_H
#define KF_OVERLAY_H
#include <kf/lib/types.h>
#include <kf/lib/memory_layout.h>

/* Byte at 0x800102f0 through which an overlay names the next program for the
 * PSX.EXE loader (0 = OPEN, 1 = GAME, 2 = END). Each program reaches it
 * through its own initialized pointer; how the original spelled the address
 * is unresolved. */
enum {
    KF_OVERLAY_OPEN = 0,
    KF_OVERLAY_GAME = 1,
    KF_OVERLAY_END = 2
};

extern u8 *overlay_next_request;
/* US GAME stores the request and a flag byte through relocated symbols. */
extern u8 DAT_800102f0;
extern u8 DAT_800102f8;

/* End of the program's linked .bss. GAME retail loads it as a relocated
 * symbol (lui/addiu, run-time subtraction), so it is a link-time label, not a
 * folded number; the original object that defined it is unrecovered
 * (config/link/overlay_bounds.asm stands in for it). */
extern u8 BSS_END[];

enum {
    OVERLAY_STACK_BYTES = 0x8000
};

#define OVERLAY_RAM_END (0x80000000u + KF_MAIN_RAM_BYTES)
#define OVERLAY_STACK_BOTTOM (OVERLAY_RAM_END - OVERLAY_STACK_BYTES)

#endif
