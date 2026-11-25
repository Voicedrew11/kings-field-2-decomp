#ifndef KF_GAME_GRAPHICS_H
#define KF_GAME_GRAPHICS_H

#include <kf/lib/bool.h>
#include <kf/lib/offsetof.h>
#include <kf/lib/types.h>
#include <kf/lib/enum.h>
#include <kf/lib/gpu.h>
#include <kf/game/asset.h>
#include <kf/game/pool.h>
#include <kf/game/tmd.h>
#include <kf/game/map_placed.h>
#include <kf/game/render_types.h>
#include <kf/game/notify_types.h>
#include <psyq/sdk.h>

/* GAME.EXE double-buffered display and the graphics runtime region cleared
 * by game_main_loop (0x5f3c words). Members are named where a
 * reconstructed function establishes them; the rest stay opaque. */

enum {
    KF_DISPLAY_BUFFER_COUNT = 2,
    KF_GAME_ORDERING_TABLE_LENGTH = 0x2000,
    KF_GAME_PRIMITIVE_BUFFER_BYTES = 0x19000,
    KF_GAME_TMD_SLOT_COUNT = 4,
    /* Extent up to current_tmd_vertices; entry 0x181 is read directly. The
     * original capacity is unproven. */
    KF_ASSET_REGISTRY_EXTENT = 0x240,
    /* Extent from the projection loops' base; the original capacity is unproven. */
    KF_PROJECTED_VERTEX_EXTENT = 1000,
    KF_ANIMATION_VERTEX_SCRATCH_EXTENT = 1000,
    KF_NOTIFICATION_CAPACITY = 8,
    KF_CLIP_EDGE_COUNT = 16,
    KF_FLOOR_ITEM_CAPACITY = 8,
    KF_MAP_CELL_GRID_SIDE = 24,
    KF_COLLISION_ROW_COUNT = 80
};

/* Retail toggles the index with an equality, so UNINITIALIZED selects FIRST
 * on the first frame. */
KF_ENUM_BEGIN(KfDisplayBufferIndex, u8)
    KF_DISPLAY_BUFFER_FIRST = 0,
    KF_DISPLAY_BUFFER_SECOND = 1,
    KF_DISPLAY_BUFFER_UNINITIALIZED = 0xff
KF_ENUM_END(KfDisplayBufferIndex)

#if KF_MODERN_TYPES
constexpr KfDisplayBufferIndex display_next_buffer(KfDisplayBufferIndex current)
{
    return static_cast<KfDisplayBufferIndex>(current == KF_DISPLAY_BUFFER_FIRST);
}
#else
#define display_next_buffer(current) ((current) == KF_DISPLAY_BUFFER_FIRST)
#endif

typedef struct KfPrimitiveBuffer {
    u8 *start;
    u8 *end;
    u8 *cursor;
} KfPrimitiveBuffer;

typedef struct KfOrderingTable {
    u32 entries[KF_GAME_ORDERING_TABLE_LENGTH];
} KfOrderingTable;

typedef struct KfDisplayState {
    KfDisplayBufferIndex buffer_index;
    u8 *asset_load_buffer;
    KfPrimitiveBuffer primitive_buffers[KF_DISPLAY_BUFFER_COUNT];
    KfPrimitiveBuffer *primitive_buffer;
    KfOrderingTable ordering_tables[KF_DISPLAY_BUFFER_COUNT];
    u32 *ordering_table;
} KfDisplayState;

typedef struct KfTmdState {
    KfTmdHeader *slots[KF_GAME_TMD_SLOT_COUNT];
    KfTmdHeader *current_asset;
} KfTmdState;

/* Projected or view-space vertex: screen x/y, a depth and the raw z. */
typedef struct KfScreenVertex {
    s16 x;
    s16 y;
    s16 sz;
    s16 depth_cue;
} KfScreenVertex;
typedef char kf_screen_vertex_size[sizeof(KfScreenVertex) == 8 ? 1 : -1];
typedef char kf_screen_vertex_depth_cue_offset[
    offsetof(KfScreenVertex, depth_cue) == 6 ? 1 : -1];

typedef struct KfNotificationControl {
    u8 queue_tail;
    u8 queue_head;
    KfNotificationPhase effect_phase;
    u8 hold_frames;
} KfNotificationControl;

typedef struct KfFloorItem {
    u8 kind;
    u8 update_interval;
    u8 frames_until_update;
    u8 row_step;
    u16 row_offset;
    RECT rect;
    u_long *pixels;
    u8 unknown_14[4];
} KfFloorItem;

typedef struct KfRenderState {
    MATRIX view_matrix;
    MATRIX pitch_matrix;
    MATRIX unknown_matrix_40;
    VECTOR view_position;
    SVECTOR view_rotation;
    s32 view_cell_x;
    s32 view_cell_z;
    s32 cell_origin_x;
    s32 cell_origin_z;
} KfRenderState;

typedef struct KfRenderGridState {
    s32 map_scan_start_x;
    s32 map_scan_start_z;
    s32 fog_near_distance;
    KfMapLayerMask map_cell_layer_masks[KF_MAP_CELL_GRID_SIDE][KF_MAP_CELL_GRID_SIDE];
} KfRenderGridState;

/* Each transform occupies 20 bytes: the rotation helper uses its first
 * nine halfwords, while the translation part of SDK MATRIX is not present. */
typedef struct KfCollisionRotation {
    s16 m[3][3];
    s16 pad;
} KfCollisionRotation;

typedef struct KfCollisionMotion {
    s16 values[9];
} KfCollisionMotion;

typedef struct KfCollisionKinds {
    u8 types[3];
    u8 pad;
} KfCollisionKinds;

typedef struct KfCollisionTail {
    KfCollisionKinds kinds;
    s16 angle;
} KfCollisionTail;

typedef struct KfCollisionDefaultTail {
    KfCollisionKinds kinds;
    u16 angle;
} KfCollisionDefaultTail;

typedef struct KfCollisionRow {
    KfCollisionRotation rotations[4];
    KfCollisionMotion motion;
    KfCollisionTail filter;
} KfCollisionRow;

typedef struct KfCollisionFilterPayload {
    KfCollisionRotation rotation;
    KfCollisionMotion motion;
    KfCollisionTail filter;
} KfCollisionFilterPayload;

typedef struct KfCollisionDefaultRow {
    KfCollisionRotation rotation;
    KfCollisionMotion motion;
    KfCollisionDefaultTail filter;
} KfCollisionDefaultRow;

/* Row fields interpolate_collision_rows blends toward the payload: the
 * rotation matrix (which also marks the rotations dirty), the motion
 * halfwords, the three filter types and the filter angle. */
KF_ENUM_BEGIN(KfCollisionRowFields, s32)
    KF_COLLISION_ROW_NONE = 0,
    KF_COLLISION_ROW_ROTATION = 1,
    KF_COLLISION_ROW_MOTION = 2,
    KF_COLLISION_ROW_FILTER_TYPES = 4,
    KF_COLLISION_ROW_FILTER_ANGLE = 8
KF_ENUM_END(KfCollisionRowFields)
KF_ENUM_FLAGS(KfCollisionRowFields, s32)

void interpolate_collision_rows(KF_ENUM_PARAM(KfCollisionRowFields, s32) flags,
    const KfCollisionFilterPayload *payload, s32 value);
void reset_collision_rows_and_overlay(void);
void build_camera_map_cell_layer_masks(void);
void floor_item_update_textures(void);
void render_map_cell_window(void);
void render_active_model_rows(void);
void render_color_overlay(void);
void render_accumulated_color_overlay(void);
void color_overlay_transition(s32 step, s32 first, s32 second, s32 third,
                              s32 target_first, s32 target_second,
                              s32 target_third);
void render_player_weapon(void);
void accumulate_color_overlay(s32 first, s32 second, s32 third, s32 scale);
void render_scene_and_update_resources(void);
void render_textured_quad(s32 x, s32 y, s32 right, s32 bottom,
                   u8 texture_u, u8 texture_v, u8 texture_width,
                   u8 texture_height, u8 semitrans, u16 tpage,
                   u16 clut, u8 red, u8 green, u8 blue, s32 depth);
void render_sliding_panel_primary(void);
void render_sliding_panel_secondary(void);
void render_game_frame(const VECTOR *position, const SVECTOR *rotation);

typedef struct KfGraphicsRuntimeGame {
    KfDisplayState display_state;
    DRAWENV display_draw_environments[KF_DISPLAY_BUFFER_COUNT];
    DISPENV display_disp_environments[KF_DISPLAY_BUFFER_COUNT];
    KfTmdState tmd_state;
    KfAssetHeader *asset_registry_entries[KF_ASSET_REGISTRY_EXTENT];
    SVECTOR *current_tmd_vertices;
    KfPoolRecord pool_records[KF_ANIMATION_CACHE_CAPACITY];
    KfScreenVertex tmd_projected_vertices[KF_PROJECTED_VERTEX_EXTENT];
    u8 unknown_12a50[4];
    SVECTOR animation_vertex_scratch[KF_ANIMATION_VERTEX_SCRATCH_EXTENT];
    /* First three are consumed by GAME; capacity beyond those is provisional. */
    EVECTOR *clip_result_vertices[KF_CLIP_EDGE_COUNT];
    u8 unknown_149d4[0x10];
    EVECTOR clip_edges[KF_CLIP_EDGE_COUNT];
    KfNotificationId notification_message_ids[KF_NOTIFICATION_CAPACITY];
    u16 notification_payloads[KF_NOTIFICATION_CAPACITY];
    KfNotificationControl notification_control;
    u8 notification_brightness;
    KfColorOverlayControl color_overlay_control;
    u8 color_overlay_rgb[3];
    u8 color_overlay_sample_count;
    u16 color_overlay_red_sum;
    u16 color_overlay_green_sum;
    u16 color_overlay_blue_sum;
    u32 frame_counter_a;
    u32 frame_counter_b;
    KfFloorItem floor_items[KF_FLOOR_ITEM_CAPACITY];
    KfRenderState render_state;
    KfRenderGridState render_grid;
    b32 collision_rotation_dirty;
    KfCollisionRow collision_rows[KF_COLLISION_ROW_COUNT];
    s32 map_placed_frame_counter;
    KfMapPlacedEntry map_placed_entries[KF_MAP_PLACED_ENTRY_COUNT];
} KfGraphicsRuntimeGame;

typedef char kf_display_state_size[sizeof(KfDisplayState) == 0x10028 ? 1 : -1];
typedef char kf_display_asset_load_buffer_offset[
    offsetof(KfDisplayState, asset_load_buffer) == 4 ? 1 : -1];
typedef char kf_render_state_size[sizeof(KfRenderState) == 0x88 ? 1 : -1];
typedef char kf_render_grid_size[sizeof(KfRenderGridState) == 0x24c ? 1 : -1];
typedef char kf_collision_row_size[sizeof(KfCollisionRow) == 104 ? 1 : -1];
typedef char kf_floor_item_size[sizeof(KfFloorItem) == 24 ? 1 : -1];
typedef char kf_floor_item_update_interval_offset[
    offsetof(KfFloorItem, update_interval) == 1 ? 1 : -1];
typedef char kf_floor_item_frames_until_update_offset[
    offsetof(KfFloorItem, frames_until_update) == 2 ? 1 : -1];
typedef char kf_floor_item_row_step_offset[
    offsetof(KfFloorItem, row_step) == 3 ? 1 : -1];
typedef char kf_floor_item_row_offset_offset[
    offsetof(KfFloorItem, row_offset) == 4 ? 1 : -1];
typedef char kf_floor_item_rect_offset[
    offsetof(KfFloorItem, rect) == 6 ? 1 : -1];
typedef char kf_floor_item_pixels_offset[
    offsetof(KfFloorItem, pixels) == 16 ? 1 : -1];
typedef char kf_collision_row_types_offset[
    offsetof(KfCollisionRow, filter.kinds.types) == 98 ? 1 : -1];
typedef char kf_collision_row_angle_offset[
    offsetof(KfCollisionRow, filter.angle) == 102 ? 1 : -1];
typedef char kf_collision_filter_payload_size[
    sizeof(KfCollisionFilterPayload) == 44 ? 1 : -1];
typedef char kf_collision_filter_payload_motion_offset[
    offsetof(KfCollisionFilterPayload, motion) == 20 ? 1 : -1];
typedef char kf_collision_filter_payload_filter_offset[
    offsetof(KfCollisionFilterPayload, filter) == 38 ? 1 : -1];
typedef char kf_collision_default_row_size[
    sizeof(KfCollisionDefaultRow) == 44 ? 1 : -1];
typedef char kf_collision_rows_offset[
    offsetof(KfGraphicsRuntimeGame, collision_rows) == 0x1506c ? 1 : -1];
typedef char kf_collision_control_offset[
    offsetof(KfGraphicsRuntimeGame, color_overlay_red_sum) == 0x14cc6 ? 1 : -1];
typedef char kf_graphics_runtime_size[sizeof(KfGraphicsRuntimeGame) == 0x17cf0 ? 1 : -1];
typedef char kf_animation_vertex_scratch_offset[
    offsetof(KfGraphicsRuntimeGame, animation_vertex_scratch) == 0x12a54 ? 1 : -1];
typedef char kf_clip_result_vertices_offset[
    offsetof(KfGraphicsRuntimeGame, clip_result_vertices) == 0x14994 ? 1 : -1];
/* The SDK clip result is a complete EVECTOR, including its trailing window fields. */
typedef char kf_clip_evector_size[sizeof(EVECTOR) == 0x2c ? 1 : -1];
typedef char kf_clip_evector_depth_offset[
    offsetof(EVECTOR, sxyz.vz) == 16 ? 1 : -1];
typedef char kf_clip_evector_perspective_offset[
    offsetof(EVECTOR, sxyz.pad) == 20 ? 1 : -1];
typedef char kf_clip_evector_xy_offset[
    offsetof(EVECTOR, sxy) == 24 ? 1 : -1];
typedef char kf_clip_evector_color_offset[
    offsetof(EVECTOR, rgb) == 28 ? 1 : -1];
typedef char kf_clip_evector_uv_offset[
    offsetof(EVECTOR, txuv) == 32 ? 1 : -1];

extern KfGraphicsRuntimeGame scene_display_sys;
extern KfCollisionDefaultRow collision_default_rows[KF_COLLISION_ROW_COUNT];
/* Primitive memory the display reset splits into the two primitive buffers. */
extern u8 ot_packet_memory[KF_DISPLAY_BUFFER_COUNT * KF_GAME_PRIMITIVE_BUFFER_BYTES];
/* Cleared with the per-frame counters; no other reference is known yet. */
extern s32 frame_cleared_state;
extern RECT menu_transition_rect;
s32 menu_fade_transition(s32 level, s32 step);

enum {
    KF_FLOOR_ITEM_NONE = 0xff,
    KF_FLOOR_ITEM_SCROLLING_IMAGE = 1
};

void fog_set_near(s32 distance);
void render_set_color_overlay(KfColorOverlayControl control, u8 red, u8 green, u8 blue);
void menu_show_transition_image(u16 archive_slot, u16 archive_entry);
void display_initialize(void);
void display_reset(void);
void refresh_collision_row_rotations(void);
void floor_item_capture_image(s32 x, s32 y, u8 update_interval, u8 row_step,
                   s32 kind, ...);
void display_begin_frame(void);
void display_present_frame(void);
void display_set_view_transform(const VECTOR *position, const SVECTOR *rotation);
void primitive_buffer_begin_poly_ft4(void);
void primitive_buffer_commit_poly_ft4(s32 depth);

#endif
