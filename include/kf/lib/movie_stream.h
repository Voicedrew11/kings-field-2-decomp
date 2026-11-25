#ifndef KF_MOVIE_STREAM_H
#define KF_MOVIE_STREAM_H
#include <kf/lib/bool.h>
#include <kf/lib/types.h>
#include <psyq/cd.h>
#include <psyq/press.h>

/* STR streaming playback helpers shared by OPEN.EXE and END.EXE; structure
 * and names follow Sony's streaming movie tutorial (TUTO0.C). */
typedef struct {
    u_long *vlcbuf[2];
    int vlcid;
    u_short *imgbuf;
    RECT rect[2];
    int rectid;
    RECT slice;
    KfBool isdone;
} DECENV;

/* StSetStream frame argument that accepts any frame of the stream. */
enum {
    STR_FRAME_ANY = 0x0fffffff
};

typedef char kf_decenv_size[sizeof(DECENV) == 0x30 ? 1 : -1];

void strSetDefDecEnv(void);
void strInit(CdlLOC *loc);
void strCallback(void);
int strNextVlc(void);
u_long *strNext(DECENV *env);
void strSync(DECENV *env);
void strKickCD(CdlLOC *loc);

#endif
