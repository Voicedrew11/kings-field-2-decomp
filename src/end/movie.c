#include <kf/lib/address.h>
#include <kf/lib/null.h>
#include <kf/lib/display.h>
#include <kf/end/ending.h>
#include <kf/lib/audio.h>
#include <kf/lib/movie_stream.h>
#include <psyq/libc.h>

#include "../lib/movie_stream_state.inc"

enum {
    ENDING_MDEC_16BIT_MODE = 2
};

RODATA(0x80011000, 0x1f)

/* US END: plays ED0.S or ED1.S. ED1 then waits 150 counted periods of six
 * VSyncs each, advancing StrFrame once per period, before stopping the drive. */
ADDRESS(0x80011cc4, 0x1dc)
void ending_play_movie(s32 movie)
{
    CdlFILE file;
    /* Unused: retail's frame reserves an 8-byte slot after the file that no
     * statement touches; a RECT-sized dead local reproduces it. */
    RECT rect;
    CdlFILE *found;
    int i;

    if (movie == 0) {
        found = CdSearchFile(&file, "\\OP\\ED0.S;1");
    } else {
        found = CdSearchFile(&file, "\\OP\\ED1.S;1");
    }
    if (found == NULL) {
        printf("\n__ file not found");
        return;
    }
    movie_last_frame = movie != 0 ? 0xa6d : 0x6a2;
    strSetDefDecEnv();
    strInit(&file.pos);
    strNextVlc();
    display_current = &display_buffers[0];
    StrFrame = 0;
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
        StrFrame++;
    } while (StrFrame <= movie_last_frame);
    if (movie == 1) {
        for (i = 0; i < 150; i++) {
            VSync(0);
            VSync(0);
            VSync(0);
            VSync(0);
            VSync(0);
            VSync(0);
            StrFrame++;
        }
    }
    CdStop();
}

#include "../lib/movie_stream.inc"
