/*
 * The tutorial's earlier revision as linked here: one decode buffer, 16-bit
 * output and a global DECENV. OPEN.EXE and END.EXE link the same bodies; the
 * player sets movie_last_frame for each movie. Included after the movie player's functions, in the
 * translation unit whose state movie_stream_state.inc defines.
 */

/* STR sector header as read here: the Psy-Q 3.0 StHEADER stops after
 * `height`, but the code also compares the next two words (named headm and
 * headv by later SDK headers) with the first two words of the frame data. */
typedef struct {
    StHEADER header;
    u_long headm;
    u_long headv;
} StrSectorHeader;

ADDRESS_AT("OPEN", 0x80014170, 0xbc)
ADDRESS_AT("END", 0x80011ea0, 0xbc)
void strSetDefDecEnv(void)
{
    dec.vlcbuf[0] = vlcbuf0;
    dec.vlcbuf[1] = vlcbuf1;
    dec.vlcid = 0;
    dec.imgbuf = imgbuf;
    dec.rectid = 0;
    dec.isdone = KF_FALSE;
    setRECT(&dec.rect[0], 0, 0, SCR_WIDTH, SCR_HEIGHT);
    setRECT(&dec.rect[1], 0, SCR_HEIGHT, SCR_WIDTH, SCR_HEIGHT);
    setRECT(&dec.slice, 0, 0, SLICE_WIDTH_PIXELS, SCR_HEIGHT);
}

ADDRESS_AT("OPEN", 0x8001422c, 0x74)
ADDRESS_AT("END", 0x80011f5c, 0x74)
void strInit(CdlLOC *loc)
{
    DecDCTReset(0);
    Rewind_Switch = KF_FALSE;
    DecDCToutCallback(strCallback);
    StSetRing(Ring_Buff, RING_SIZE);
    StSetStream(0, 1, 0x0fffffff, NULL, NULL);
    strKickCD(loc);
}

/* DecDCTout completion callback: store the decoded slice, then start the
 * next slice or finish the frame and flip the destination rectangle. */
ADDRESS_AT("OPEN", 0x800142a0, 0x10c)
ADDRESS_AT("END", 0x80011fd0, 0x10c)
void strCallback(void)
{
    LoadImage(&dec.slice, (u_long *)dec.imgbuf);
    dec.slice.x += dec.slice.w;
    if (dec.slice.x < dec.rect[dec.rectid].x + dec.rect[dec.rectid].w) {
        DecDCTout((u_long *)dec.imgbuf, dec.slice.w * dec.slice.h / 2);
    } else {
        dec.isdone = KF_TRUE;
        dec.rectid = dec.rectid ? 0 : 1;
        dec.slice.x = dec.rect[dec.rectid].x;
        dec.slice.y = dec.rect[dec.rectid].y;
    }
}

ADDRESS_AT("OPEN", 0x800143ac, 0x94)
ADDRESS_AT("END", 0x800120dc, 0x94)
int strNextVlc(void)
{
    int cnt = WAIT_TIME;
    u_long *next;

    while ((next = strNext(&dec)) == NULL) {
        if (--cnt == 0) {
            return -1;
        }
    }
    dec.vlcid = dec.vlcid ? 0 : 1;
    DecDCTvlc(next, dec.vlcbuf[dec.vlcid]);
    StFreeRing(next);
    return 0;
}

/* The environment argument is unused; the caller passes the global. */
ADDRESS_AT("OPEN", 0x80014440, 0xb4)
ADDRESS_AT("END", 0x80012170, 0xac)
u_long *strNext(DECENV *env)
{
    u_long *addr;
    StrSectorHeader *sector;
    int cnt = WAIT_TIME;

    while (StGetNext(&addr, (u_long **)&sector)) {
        if (--cnt == 0) {
            return NULL;
        }
    }
    if (addr[0] != sector->headm || addr[1] != sector->headv) {
        StFreeRing(addr);
        return NULL;
    }
#ifndef KF_END
    /* END's player counts StrFrame itself. */
    StrFrame = sector->header.frameCount;
#endif
    if (sector->header.frameCount > movie_last_frame) {
        Rewind_Switch = KF_TRUE;
    }
    return addr;
}

ADDRESS_AT("OPEN", 0x800144f4, 0x74)
ADDRESS_AT("END", 0x8001221c, 0x74)
void strSync(DECENV *env)
{
    u_long cnt = WAIT_TIME;

    while (!env->isdone) {
        if (--cnt == 0) {
            env->isdone = KF_TRUE;
            env->rectid = env->rectid ? 0 : 1;
            env->slice.x = env->rect[env->rectid].x;
            env->slice.y = env->rect[env->rectid].y;
        }
    }
    env->isdone = KF_FALSE;
}

ADDRESS_AT("OPEN", 0x80014568, 0x48)
ADDRESS_AT("END", 0x80012290, 0x48)
void strKickCD(CdlLOC *loc)
{
    while (CdSeekL(loc) == 0) {
    }
    while (CdRead2(CdlModeStream | CdlModeSpeed | CdlModeRT) == 0) {
    }
}
