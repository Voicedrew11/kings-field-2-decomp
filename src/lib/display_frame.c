#include <kf/lib/address.h>
#include <kf/lib/display.h>

DATA_AT("OPEN", 0x800ad170, 0x4, ".bss")
DATA_AT("END", 0x800a80b4, 0x4, ".bss")
POLY_FT4 *current_poly_ft4;

ADDRESS_AT("OPEN", 0x80013aa0, 0x64)
ADDRESS_AT("END", 0x80011bf4, 0x60)
void display_begin_frame(void)
{
    display_current = (display_current == &display_buffers[0]) ? &display_buffers[1] : &display_buffers[0];
    ClearOTag(display_current->ordering_table, KF_ORDERING_TABLE_LENGTH);
#ifdef KF_OPEN
    display_fade_level = 0;
#endif
    current_poly_ft4 = display_current->primitives;
}

ADDRESS_AT("OPEN", 0x80013b04, 0x58)
ADDRESS_AT("END", 0x80011c54, 0x58)
void display_present_frame(void)
{
    DrawSync(0);
    VSync(0);
    PutDrawEnv(&display_current->draw);
    PutDispEnv(&display_current->disp);
    DrawOTag(display_current->ordering_table);
}
