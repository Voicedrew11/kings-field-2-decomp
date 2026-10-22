#include <kf/lib/address.h>
#include <kf/lib/null.h>
#include <kf/lib/display.h>
#include <kf/end/ending.h>
#include <kf/lib/audio.h>
#include <kf/lib/movie_stream.h>
#include <psyq/libc.h>

#include "../lib/movie_stream_state.inc"

/* The ending STR runs to frame 2136; the music fades out from frame 1336. */
enum {
    ENDING_FADE_FRAME = 1336,
    ENDING_LAST_FRAME = 2136,
    ENDING_MASTER_VOLUME = 127,
    ENDING_SEQUENCE_VOLUME = 64,
    ENDING_MDEC_16BIT_MODE = 2
};

RODATA(0x80011000, 0x1f)

/* TODO(us-port): not yet matched against US END; JP body with the sequence
 * audio removed, as US END links no LIBSND sequence player. */
ADDRESS(0x80011cc4, 0x1dc)
void ending_play_movie(s32 movie)
{
    CdlFILE file;
    u_char mode;

    if (CdSearchFile(&file, movie == 0 ? "\\OP\\ED0.S;1" : "\\OP\\ED1.S;1") == NULL) {
        printf("\n__ file not found");
        return;
    }
    strSetDefDecEnv();
    strInit(&file.pos);
    strNextVlc();
    display_current = &display_buffers[0];
    do {
        display_begin_frame();
        DecDCTin(dec.vlcbuf[dec.vlcid], ENDING_MDEC_16BIT_MODE);
        DecDCTout((u_long *)dec.imgbuf, dec.slice.w * dec.slice.h / 2);
        strNextVlc();
        strSync(&dec);
        display_present_frame();
        if (Rewind_Switch == KF_TRUE) {
            break;
        }
    } while (StrFrame < ENDING_LAST_FRAME);
    CdStop();
    for (;;) {
    }
    /* Unreachable: the cleanup OPEN's movie loop runs after its stream. The
     * compiler drops it but keeps its frame: the fifth-argument slot of the
     * SetDef*Env calls and the address-taken mode byte (retail's 72 bytes). */
    mode = CdlModeSpeed;
    CdControlB(CdlSetmode, &mode, NULL);
    DecDCToutCallback(NULL);
    CdDataCallback(NULL);
    CdReadyCallback(NULL);
    CdControlB(CdlPause, NULL, NULL);
    SetDefDrawEnv(&display_buffers[0].draw, 0, 0, 320, KF_DISPLAY_HEIGHT);
    SetDefDrawEnv(&display_buffers[1].draw, 0, KF_DISPLAY_HEIGHT, 320, KF_DISPLAY_HEIGHT);
    SetDefDispEnv(&display_buffers[0].disp, 0, KF_DISPLAY_HEIGHT, 320, KF_DISPLAY_HEIGHT);
    SetDefDispEnv(&display_buffers[1].disp, 0, 0, 320, KF_DISPLAY_HEIGHT);
    display_current = &display_buffers[0];
}

#include "../lib/movie_stream.inc"
