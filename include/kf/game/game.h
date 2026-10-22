#ifndef KF_GAME_GAME_H
#define KF_GAME_GAME_H

#include <kf/lib/enum.h>
#include <kf/lib/types.h>

/* game_main_loop runs frames while the word reads RUNNING. */
KF_ENUM_BEGIN(KfGameMainState, u32)
    GAME_MAIN_RUNNING = 0,
    GAME_MAIN_EXIT_REQUESTED = 1,
    /* Written by player_handle_interaction_and_menu on QUIT. */
    GAME_MAIN_EXIT_ALTERNATE = 9
KF_ENUM_END(KfGameMainState)

extern KfGameMainState game_main_exit_flag;

void game_main_loop(void);
void game_shutdown(void);

/* US-only: words 0..13 are pad-button masks read in place of the PAD*
   constants; func_8001883c reads the last four as frame and state words.
   Meanings unresolved. */
extern u32 DAT_8006e568;
extern u32 DAT_8006e56c;
extern u32 DAT_8006e570;
extern u32 DAT_8006e574;
extern u32 DAT_8006e578;
extern u32 DAT_8006e57c;
extern u32 DAT_8006e580;
extern u32 DAT_8006e584;
extern u32 DAT_8006e588;
extern u32 DAT_8006e58c;
extern u32 DAT_8006e590;
extern u32 DAT_8006e594;
extern u32 DAT_8006e598;
extern u32 DAT_8006e59c;
extern u32 DAT_8006e5a0;
extern s32 DAT_8006e5a4;
extern s32 DAT_8006e5a8;
extern s32 DAT_8006e5ac;
extern s32 DAT_8006e5b0;

#endif
