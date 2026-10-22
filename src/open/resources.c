#include <kf/lib/address.h>
#include <kf/open/opening.h>
#include <kf/lib/audio.h>
#include <kf/lib/display.h>
#include <kf/lib/null.h>

ADDRESS(0x80013cb8, 0x70)
void tim_upload_images(u8 *tim_data)
{
    TIM_IMAGE image;

    OpenTIM((u_long *)tim_data);
    while (ReadTIM(&image) != NULL) {
        if (image.caddr != NULL) {
            LoadImage(image.crect, image.caddr);
        }
        if (image.paddr != NULL) {
            LoadImage(image.prect, image.paddr);
        }
    }
}

ADDRESS(0x80013d28, 0x88)
void audio_play_voice(
    s16 vab_id, s16 program, s16 tone, s16 note, s16 left_volume, s16 right_volume)
{
    /* Retail reserves an unreferenced 24-byte frame slot. */
    s16 frame_reserve[12];

    if (program == 0 && tone == 0 && note == 0) {
        return;
    }
    SsUtKeyOn(vab_id, program, tone, note, 0, left_volume, right_volume);
}
