#include <kf/lib/address.h>
#include <kf/lib/null.h>
#include <kf/lib/audio.h>
#include <kf/lib/display.h>
#include <kf/open/opening.h>
#include <kf/lib/movie_stream.h>
#include <psyq/kernel.h>
#include <psyq/libc.h>
#include <psyq/pad.h>

#include "../lib/movie_stream_state.inc"

enum {
    OPENING_VOLUME_RAMP_END = 128,
    OPENING_VOLUME_STEP = 2,
    OPENING_FADE_OUT_START = 127,
    OPENING_MDEC_16BIT_MODE = 2,
    OPENING_MOVIE_DISPLAY_WIDTH = 320,
    OPENING_TITLE_DISPLAY_WIDTH = 640
};

/* Results: the movie played through, PADstart skipped the sequence, or
 * another stop button skipped only this movie. */
enum {
    OPENING_MOVIE_PLAYED = 0,
    OPENING_MOVIE_START_PRESSED = 1,
    OPENING_MOVIE_BUTTON_PRESSED = 2
};

RODATA(0x80011008, 0x38)

/* The tutorial's anim(): decodes one opening movie (OP0-OP2.S) at 320
 * pixels, stops on a button, then restores the 640-pixel title display. */
ADDRESS(0x80013dc8, 0x3a8)
s32 opening_play_movie(s32 movie)
{
    CdlFILE file;
    u_char mode;
    s32 volume;
    s32 frame = 0;
    s32 result = 0;
    u_long pad;
    CdlFILE *found;

    if (movie == 0) {
        found = CdSearchFile(&file, "\\OP\\OP0.S;1");
    } else if (movie == 1) {
        found = CdSearchFile(&file, "\\OP\\OP1.S;1");
    } else {
        found = CdSearchFile(&file, "\\OP\\OP2.S;1");
    }
    if (found == NULL) {
        printf("\n__ file not found");
        StopCallback();
        /* Retail returns here without setting $v0, so the result is unspecified. */
        return;
    }
    if (movie == 0) {
        movie_last_frame = 0x89;
    } else if (movie == 1) {
        movie_last_frame = 0x10c;
    } else {
        movie_last_frame = 0x43f;
    }
    for (volume = 0; volume < OPENING_VOLUME_RAMP_END; volume += OPENING_VOLUME_STEP) {
        VSync(0);
        SsSetMVol(volume, volume);
    }
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
        pad = PadRead(1);
        if (pad & PADstart) {
            result = OPENING_MOVIE_START_PRESSED;
            break;
        }
        if ((pad & PADRup) || (pad & PADRleft) || (pad & PADRright) || (pad & PADRdown) || (pad & PADselect)) {
            result = OPENING_MOVIE_BUTTON_PRESSED;
            break;
        }
        if (StrFrame > movie_last_frame) {
            break;
        }
        frame++;
    } while (frame <= movie_last_frame);
    for (volume = OPENING_FADE_OUT_START; volume >= 0; volume -= OPENING_VOLUME_STEP) {
        VSync(0);
        SsSetMVol(volume, volume);
    }
    mode = CdlModeSpeed;
    CdControlB(CdlSetmode, &mode, NULL);
    DecDCToutCallback(NULL);
    CdDataCallback(NULL);
    CdReadyCallback(NULL);
    CdControlB(CdlPause, NULL, NULL);
    while (PadRead(1) != 0) {
    }
    SetDefDrawEnv(&display_buffers[0].draw, 0, 0, OPENING_TITLE_DISPLAY_WIDTH, KF_DISPLAY_HEIGHT);
    SetDefDrawEnv(&display_buffers[1].draw, 0, KF_DISPLAY_HEIGHT, OPENING_TITLE_DISPLAY_WIDTH, KF_DISPLAY_HEIGHT);
    SetDefDispEnv(&display_buffers[0].disp, 0, KF_DISPLAY_HEIGHT, OPENING_TITLE_DISPLAY_WIDTH, KF_DISPLAY_HEIGHT);
    SetDefDispEnv(&display_buffers[1].disp, 0, 0, OPENING_TITLE_DISPLAY_WIDTH, KF_DISPLAY_HEIGHT);
    display_buffers[0].draw.isbg = display_buffers[1].draw.isbg = 1;
    display_current = &display_buffers[0];
    return result;
}

#include "../lib/movie_stream.inc"
