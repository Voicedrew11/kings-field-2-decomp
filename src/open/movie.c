#include <kf/lib/address.h>
#include <kf/lib/null.h>
#include <kf/open/opening.h>
#include <kf/lib/audio.h>
#include <kf/lib/display.h>
#include <kf/lib/movie_stream.h>
#include <psyq/libc.h>
#include <psyq/pad.h>

#include "../lib/movie_stream_state.inc"

/* The opening STR runs to frame 1085; its music replaces the title music. */
enum {
    OPENING_LAST_FRAME = 1085,
    OPENING_VOLUME_RAMP_END = 128,
    OPENING_VOLUME_STEP = 2,
    OPENING_MDEC_16BIT_MODE = 2,
    OPENING_MOVIE_DISPLAY_WIDTH = 320,
    OPENING_TITLE_DISPLAY_WIDTH = 640,
    /* Frames held after an unskipped movie before the music fades. */
    OPENING_HOLD_FRAMES = 90
};

RODATA(0x80011000, 0x1f)

/* The tutorial's anim(): decodes the opening movie at 320 pixels, stops when
 * a button is pressed, then restores the 640-pixel title display. */
ADDRESS(0x80013dc8, 0x3a8)
void opening_play_movie(void)
{
    CdlFILE file;
    u_char mode;
    s32 volume;
    b32 skipped = KF_FALSE;

    SsSeqStop(audio_title_sequence_id);
    if (CdSearchFile(&file, "\\OP\\OP.S;1") == NULL) {
        printf("\n__ file not found");
        return;
    }
    for (volume = 0; volume < OPENING_VOLUME_RAMP_END; volume += OPENING_VOLUME_STEP) {
        VSync(0);
        SsSetMVol(volume, volume);
    }
    SsSeqSetVol(audio_movie_sequence_id, KF_OPENING_SEQUENCE_VOLUME, KF_OPENING_SEQUENCE_VOLUME);
    SsSeqPlay(audio_movie_sequence_id, SSPLAY_PLAY, 1);
    strSetDefDecEnv();
    strInit(&file.pos);
    strNextVlc();
    SetDefDrawEnv(&display_buffers[0].draw, 0, 0, OPENING_MOVIE_DISPLAY_WIDTH, KF_DISPLAY_HEIGHT);
    SetDefDrawEnv(&display_buffers[1].draw, 0, KF_DISPLAY_HEIGHT, OPENING_MOVIE_DISPLAY_WIDTH, KF_DISPLAY_HEIGHT);
    SetDefDispEnv(&display_buffers[0].disp, 0, KF_DISPLAY_HEIGHT, OPENING_MOVIE_DISPLAY_WIDTH, KF_DISPLAY_HEIGHT);
    SetDefDispEnv(&display_buffers[1].disp, 0, 0, OPENING_MOVIE_DISPLAY_WIDTH, KF_DISPLAY_HEIGHT);
    display_current = &display_buffers[0];
    do {
        display_begin_frame();
        DecDCTin(dec.vlcbuf[dec.vlcid], OPENING_MDEC_16BIT_MODE);
        DecDCTout((u_long *)dec.imgbuf, dec.slice.w * dec.slice.h / 2);
        strNextVlc();
        strSync(&dec);
        display_present_frame();
        if (Rewind_Switch == KF_TRUE) {
            break;
        }
        if (PadRead(1) != 0) {
            skipped = KF_TRUE;
            while (PadRead(1) != 0) {
            }
            break;
        }
    } while (StrFrame < OPENING_LAST_FRAME);
    if (!skipped) {
        for (volume = 0; volume < OPENING_HOLD_FRAMES; volume++) {
            VSync(0);
        }
    }
    for (volume = KF_OPENING_SEQUENCE_VOLUME; volume >= 0; volume--) {
        VSync(0);
        SsSeqSetVol(audio_movie_sequence_id, volume, volume);
    }
    SsSeqSetVol(audio_movie_sequence_id, 0, 0);
    mode = CdlModeSpeed;
    CdControlB(CdlSetmode, &mode, NULL);
    DecDCToutCallback(NULL);
    CdDataCallback(NULL);
    CdReadyCallback(NULL);
    CdControlB(CdlPause, NULL, NULL);
    SetDefDrawEnv(&display_buffers[0].draw, 0, 0, OPENING_TITLE_DISPLAY_WIDTH, KF_DISPLAY_HEIGHT);
    SetDefDrawEnv(&display_buffers[1].draw, 0, KF_DISPLAY_HEIGHT, OPENING_TITLE_DISPLAY_WIDTH, KF_DISPLAY_HEIGHT);
    SetDefDispEnv(&display_buffers[0].disp, 0, KF_DISPLAY_HEIGHT, OPENING_TITLE_DISPLAY_WIDTH, KF_DISPLAY_HEIGHT);
    SetDefDispEnv(&display_buffers[1].disp, 0, 0, OPENING_TITLE_DISPLAY_WIDTH, KF_DISPLAY_HEIGHT);
    display_current = &display_buffers[0];
    SsSeqStop(audio_movie_sequence_id);
}

#include "../lib/movie_stream.inc"
