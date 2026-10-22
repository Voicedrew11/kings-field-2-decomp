#include <kf/lib/address.h>
#include <kf/lib/overlay.h>
#include <kf/game/game.h>
#include <kf/game/actor.h>
#include <kf/game/audio.h>
#include <kf/game/callback.h>
#include <kf/game/card.h>
#include <kf/game/cd.h>
#include <kf/game/effect.h>
#include <kf/game/event_state.h>
#include <kf/game/graphics.h>
#include <kf/game/map_object.h>
#include <kf/game/memory.h>
#include <kf/game/menu.h>
#include <kf/game/player.h>
#include <kf/game/resources.h>
#include <psyq/audio.h>
#include <psyq/cd.h>
#include <psyq/kernel.h>
#include <psyq/pad.h>
#include <psyq/sdk.h>


/* Eight zero bytes open the game .sdata at the gp anchor, where OPEN's and
   END's main units keep their mailbox pointer and data-file name. Nothing in
   any GAME build reads them; their types are unresolved byte views. */
DATA(0x8006e560, 0x4, ".sdata")
static u32 main_unreferenced_sdata_word = 0;
DATA(0x8006e564, 0x4, ".sdata")
static u8 main_unreferenced_sdata_bytes[4] = {0};
DATA(0x8006e568, 0x4, ".sdata")
u32 DAT_8006e568 = 0x40;
DATA(0x8006e56c, 0x4, ".sdata")
u32 DAT_8006e56c = 0x20;
DATA(0x8006e570, 0x4, ".sdata")
u32 DAT_8006e570 = 0x80;
DATA(0x8006e574, 0x4, ".sdata")
u32 DAT_8006e574 = 0x10;
DATA(0x8006e578, 0x4, ".sdata")
u32 DAT_8006e578 = 0x100;
DATA(0x8006e57c, 0x4, ".sdata")
u32 DAT_8006e57c = 0x800;
DATA(0x8006e580, 0x4, ".sdata")
u32 DAT_8006e580 = 0x8;
DATA(0x8006e584, 0x4, ".sdata")
u32 DAT_8006e584 = 0x2;
DATA(0x8006e588, 0x4, ".sdata")
u32 DAT_8006e588 = 0x4;
DATA(0x8006e58c, 0x4, ".sdata")
u32 DAT_8006e58c = 0x1;
DATA(0x8006e590, 0x4, ".sdata")
u32 DAT_8006e590 = 0x1000;
DATA(0x8006e594, 0x4, ".sdata")
u32 DAT_8006e594 = 0x4000;
DATA(0x8006e598, 0x4, ".sdata")
u32 DAT_8006e598 = 0x2000;
DATA(0x8006e59c, 0x4, ".sdata")
u32 DAT_8006e59c = 0x8000;
DATA(0x8006e5a0, 0x4, ".sdata")
u32 DAT_8006e5a0 = 0x0;
DATA(0x8006e5a4, 0x4, ".sdata")
s32 DAT_8006e5a4 = 0x0;
DATA(0x8006e5a8, 0x4, ".sdata")
s32 DAT_8006e5a8 = 0x0;
DATA(0x8006e5ac, 0x4, ".sdata")
s32 DAT_8006e5ac = 0x0;
DATA(0x8006e5b0, 0x4, ".sdata")
s32 DAT_8006e5b0 = 0x158;

DATA(0x80199574, 0x4, ".bss")
KfGameMainState game_main_exit_flag;


enum { GAME_INITIAL_MASTER_VOLUME = 0x7f };

/*
 * GCC inserts the `__main` hook call for a function named main; the SDK
 * start routine tail-calls here. The heap runs from the end of .bss to the
 * stack reserved below the top of RAM.
 */
ADDRESS(0x80013634, 0x68)
void main(void)
{
    InitHeap((void *)BSS_END, OVERLAY_STACK_BOTTOM - (u32)BSS_END);
    CdInit();
    PadInit(0);
    InitCARD(1);
    ChangeClearPAD(0);
    ExitCriticalSection();
    game_main_loop();
}

ADDRESS(0x8001369c, 0x36c)
void game_main_loop(void)
{
    RECT clear_rect;
    VECTOR camera_position;
    SVECTOR camera_rotation;
    KfGameMainState exit_requested;

    repeat_store_word((u32 *)&resource_state, 0,
        sizeof resource_state / sizeof(u32));
    repeat_store_word((u32 *)&game_graphics_runtime, 0,
        sizeof game_graphics_runtime / sizeof(u32));
    repeat_store_word((u32 *)&player_state, 0,
        sizeof player_state / sizeof(u32));
    repeat_store_word((u32 *)&event_state, 0,
        sizeof event_state / sizeof(u32));
    repeat_store_word((u32 *)&bss_801c7540, 0,
        sizeof bss_801c7540 / sizeof(u32));
    repeat_store_word((u32 *)&map_object_state, 0,
        sizeof map_object_state / sizeof(u32));
    repeat_store_word((u32 *)&actor_state, 0,
        sizeof actor_state / sizeof(u32));
    repeat_store_word((u32 *)&effect_state, 0,
        sizeof effect_state / sizeof(u32));
    repeat_store_word((u32 *)&audio_state, 0,
        sizeof audio_state / sizeof(u32));

    DAT_800864dc = KF_TRUE;
    cd_initialize();
    display_initialize();
    audio_initialize_runtime();
    map_object_pool_reset();
    actor_pool_clear();
    effect_pool_reset();
    resource_initialize_game_assets();
    event_state_initialize();
    memory_card_initialize();
    reset_collision_rows_and_overlay();
    game_graphics_runtime.collision_rotation_dirty = KF_TRUE;
    refresh_collision_row_rotations();
    DAT_800864dc = KF_FALSE;
    setRECT(&clear_rect, 0, 0, 0x140, 0x1e0);
    ClearImage(&clear_rect, 0, 0, 0);
    SsSetMVol(GAME_INITIAL_MASTER_VOLUME, GAME_INITIAL_MASTER_VOLUME);
    if (menu_card_browser() != -1) {
        player_restore_equipment_effects();
    }
    resource_run_initial_transition();
    player_sync_position_to_map();

    floor_item_capture_image(0x140, 0x100, 0, 1, KF_FLOOR_ITEM_SCROLLING_IMAGE, 0x40, 0x40);
    floor_item_capture_image(0x198, 0x1c0, 0, 1, KF_FLOOR_ITEM_SCROLLING_IMAGE, 0x20, 0x20);
    floor_item_capture_image(0x1a0, 0x1c0, 0, 4, KF_FLOOR_ITEM_SCROLLING_IMAGE, 0x20, 0x20);
    floor_item_capture_image(0x150, 0x140, 0, 2, KF_FLOOR_ITEM_SCROLLING_IMAGE, 0x40, 0x40);
    floor_item_capture_image(0x150, 0x100, 0, 4, KF_FLOOR_ITEM_SCROLLING_IMAGE, 0x40, 0x40);

    /* The fixed arena base and exit word have unresolved original owners. */
    memory_arena_initialize_blocks(KF_GAME_RESOURCE_ARENA_BASE,
                                   KF_GAME_RESOURCE_ARENA_CAPACITY);
    render_frames_with_color_overlay(KF_COLOR_OVERLAY_SUBTRACT | KF_COLOR_OVERLAY_FRONT,
                                     0x1000, 0, -128);
    game_main_exit_flag = GAME_MAIN_RUNNING;
    exit_requested = GAME_MAIN_EXIT_REQUESTED;

    do {
        reset_collision_rows_and_overlay();
        map_object_update_actions();
        player_update_frame();
        actor_update_frame();
        effect_pool_sweep();
        player_state.force_actor_lifecycle_refresh = KF_FALSE;
        callback_invoke_slot_04_zero();
        resource_advance_transition();
        player_get_camera_pose(&camera_position, &camera_rotation);
        audio_update_listener(&camera_position, &camera_rotation);
        refresh_collision_row_rotations();
        cd_request_service_stream();
        cd_request_service_vab();
        render_game_frame(&camera_position, &camera_rotation);
    } while (game_main_exit_flag == GAME_MAIN_RUNNING);

    game_shutdown();
    /* PSX.EXE owns the fixed next-overlay mailbox. */
    if (game_main_exit_flag == exit_requested) {
        DAT_800102f0 = KF_OVERLAY_END;
    } else if (game_main_exit_flag == GAME_MAIN_EXIT_ALTERNATE) {
        DAT_800102f0 = KF_OVERLAY_OPEN;
        DAT_800102f8 = KF_ENUM_ENCODE(u8, exit_requested);
    }
}

ADDRESS(0x80013a08, 0x38)
void game_shutdown(void)
{
    audio_shutdown();
    cd_close_events();
    PadStop();
    ResetGraph(KF_GPU_RESET_KEEP_DISPLAY);
}
