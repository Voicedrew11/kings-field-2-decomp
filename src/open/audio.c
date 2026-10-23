#include <kf/lib/address.h>
#include <kf/open/opening.h>
#include <kf/lib/audio.h>
#include <kf/lib/display.h>
#include <psyq/libc.h>

/* OPEN-only sweep state: a fade level, a phase (0 fades in, 127 is done, and
 * 1..12 sweep rows), and a frame counter. */
DATA(0x8003e718, 0x4, ".sdata")
s32 DAT_8003e718 = 0;
DATA(0x8003e71c, 0x4, ".sdata")
s32 DAT_8003e71c = 0;
DATA(0x8003e720, 0x4, ".sdata")
s32 DAT_8003e720 = 0;

/* OP.D holds the title TIM images, the VAB header and body, then the title
 * and movie sequences. */
enum {
    OPENING_IMAGE_BYTES = 0x4e400,
    OPENING_VAB_BODY_BYTES = 0x34ca0
};

DATA(0x800a6628, 0x4, ".bss")
u_long *audio_title_sequence_data;
DATA(0x800a6630, 0x4, ".bss")
u_long *audio_movie_sequence_data;
DATA(0x800a6650, 0x2, ".bss")
short audio_vab_id;
DATA(0x800a6660, 0x4, ".bss")
u8 *audio_vab_header;
DATA(0x800a6f58, 0x2, ".bss")
short audio_title_sequence_id;
DATA(0x800a6f60, 0x2, ".bss")
short audio_movie_sequence_id;

ADDRESS(0x80012348, 0x2f0)
void opening_open_audio(void)
{
    u8 *cursor = opening_data;

    tim_upload_images(cursor);
    cursor += OPENING_IMAGE_BYTES;
    memcpy((void *)audio_vab_header, (const void *)cursor, KF_OPENING_VAB_HEADER_BYTES);
    cursor += KF_OPENING_VAB_HEADER_BYTES;
    audio_vab_id = SsVabOpenHead(audio_vab_header, -1);
    if (audio_vab_id != -1 && SsVabTransBody(cursor, audio_vab_id) == audio_vab_id) {
        SsVabTransCompleted(SS_WAIT_COMPLETED);
    }
    cursor += OPENING_VAB_BODY_BYTES;
    memcpy((void *)audio_title_sequence_data, (const void *)cursor, KF_OPENING_TITLE_SEQUENCE_BYTES);
    cursor += KF_OPENING_TITLE_SEQUENCE_BYTES;
    memcpy((void *)audio_movie_sequence_data, (const void *)cursor, KF_OPENING_MOVIE_SEQUENCE_BYTES);
    audio_title_sequence_id = SsSeqOpen(audio_title_sequence_data, audio_vab_id);
    audio_movie_sequence_id = SsSeqOpen(audio_movie_sequence_data, audio_vab_id);
    DrawSync(0);
    free((void *)opening_data);
}

enum {
    SWEEP_FADE_STEP = 2,
    SWEEP_FADE_IN_END = 65,
    SWEEP_ROWS = 12,
    SWEEP_PHASES = 13,
    SWEEP_PHASE_DONE = 127,
    SWEEP_FRAME_LIMIT = 88,
    SWEEP_OT_DEPTH = 200
};

/* The sweep rows spell the CLUT as row * 64 + 1 (getClut(16, row)); retail
 * adds the x term with addiu where getClut's `|` emits ori. */
ADDRESS(0x80012638, 0x6f0)
s32 func_80012638(s32 mode)
{
    s32 i;

    if (mode == 2) {
        DAT_8003e718 = 0;
        DAT_8003e71c = 0;
        DAT_8003e720 = 0;
        return 0;
    }
    if (mode == 0) {
        if (DAT_8003e71c == 0) {
            DAT_8003e718 += SWEEP_FADE_STEP;
            if (DAT_8003e718 >= SWEEP_FADE_IN_END) {
                DAT_8003e71c = 1;
            }
        }
    } else if (mode == 1) {
        DAT_8003e718 -= SWEEP_FADE_STEP;
        if (DAT_8003e718 < 0) {
            DAT_8003e718 = 0;
        }
    }
    if (mode == 0 && DAT_8003e71c == 0) {
        for (i = 0; i < SWEEP_ROWS; i++) {
            primitive_buffer_begin_poly_ft4();
            setTPage(current_poly_ft4, 0, 1, 896, 0);
            current_poly_ft4->clut = (0x1eb - i) * 64 + 1;
            setRGB0(current_poly_ft4, (u8)DAT_8003e718, (u8)DAT_8003e718, (u8)DAT_8003e718);
            setXYWH(current_poly_ft4, 0xc0 + i * 2, 102 + i, 255, 36);
            setUVWH(current_poly_ft4, 0, 160, 255, 72);
            primitive_buffer_commit_poly_ft4(SWEEP_OT_DEPTH - i);
        }
    } else if (mode == 0 && DAT_8003e71c != SWEEP_PHASE_DONE) {
        for (i = 0; i < SWEEP_PHASES - DAT_8003e71c; i++) {
            primitive_buffer_begin_poly_ft4();
            setTPage(current_poly_ft4, 0, 1, 896, 0);
            current_poly_ft4->clut = (0x1ec - DAT_8003e71c - i) * 64 + 1;
            setRGB0(current_poly_ft4, (u8)DAT_8003e718, (u8)DAT_8003e718, (u8)DAT_8003e718);
            setXYWH(current_poly_ft4, 0xc0 + i * 2, 102 + i, 255, 36);
            setUVWH(current_poly_ft4, 0, 160, 255, 72);
            primitive_buffer_commit_poly_ft4(SWEEP_OT_DEPTH - i);
        }
        DAT_8003e71c++;
        if (DAT_8003e71c == SWEEP_PHASES) {
            DAT_8003e71c = SWEEP_PHASE_DONE;
        }
        VSync(0);
    } else {
        primitive_buffer_begin_poly_ft4();
        setTPage(current_poly_ft4, 0, 1, 896, 0);
        current_poly_ft4->clut = getClut(16, 480);
        setRGB0(current_poly_ft4, (u8)DAT_8003e718, (u8)DAT_8003e718, (u8)DAT_8003e718);
        setXYWH(current_poly_ft4, 0xc0, 0x66, 255, 36);
        setUVWH(current_poly_ft4, 0, 160, 255, 72);
        primitive_buffer_commit_poly_ft4(SWEEP_OT_DEPTH);
        if (mode == 1 || DAT_8003e720 >= SWEEP_FRAME_LIMIT) {
            primitive_buffer_begin_poly_ft4();
            setTPage(current_poly_ft4, 0, 1, 896, 0);
            current_poly_ft4->clut = getClut(16, 480);
            SetSemiTrans(current_poly_ft4, 1);
            if (mode == 0) {
                i = DAT_8003e720 - (SWEEP_FRAME_LIMIT - 1);
                if (DAT_8003e718 + 12 < i) {
                    i = DAT_8003e718 + 12;
                }
            } else {
                i = DAT_8003e718;
            }
            setRGB0(current_poly_ft4, i, i, i);
            setXYWH(current_poly_ft4, 0xc0, 0x8a, 255, 12);
            setUVWH(current_poly_ft4, 0, 0xe7, 255, 24);
            primitive_buffer_commit_poly_ft4(SWEEP_OT_DEPTH);
        }
    }
    DAT_8003e720++;
    if (mode == 0) {
        return DAT_8003e720;
    }
    if (mode == 1) {
        return DAT_8003e718 == 0;
    }
    return 1;
}
