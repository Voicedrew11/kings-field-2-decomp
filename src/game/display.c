#include <kf/lib/address.h>
#include <kf/game/map_placed.h>
#include <psyq/pad.h>
#include <kf/lib/null.h>
#include <kf/game/animation.h>
#include <psyq/libc.h>
#include <psyq/sdk.h>
#include <kf/game/graphics.h>
#include <kf/game/player.h>
#include <kf/game/pool.h>
#include <kf/game/tmd.h>
#include <kf/game/tmd_packets.h>
#include <kf/lib/math.h>
#include <kf/game/callback.h>
#include <kf/game/map_cell.h>
#include <kf/game/memory.h>
#include <kf/game/asset.h>
#include <kf/game/render_model.h>
#include <kf/game/actor.h>
#include <kf/game/audio.h>
#include <kf/game/cd.h>
#include <kf/game/effect.h>
#include <kf/game/map_object.h>
#include <kf/game/render_mask.h>
#include <kf/game/resources.h>
#include <stdarg.h>
#include <kf/game/menu.h>
#include <kf/game/notification_quad.h>
#include <kf/game/notify.h>


enum {
    KF_MAP_CELL_PREPARED_LIMIT = 16
};

enum {
    KF_DISPLAY_WIDTH = 320,
    KF_DISPLAY_HEIGHT = 240,
    KF_PROJECTION_DISTANCE = 200,
    KF_CLIP_DEPTH = 100,
    KF_CLIP_FAR = 0x10000,
    KF_INITIAL_FOG_NEAR_DISTANCE = 0x59d8,
    /* SetFogNear takes a 15-bit distance at half scale. */
    KF_FOG_DISTANCE_MASK = 0x7fff
};

#define GRAPHICS game_graphics_runtime
#define DISPLAY game_graphics_runtime.display_state


DATA(0x800676f4, 0x7e, ".data")
KfNotificationQuad notification_quads[7] = {
    {KF_SPRITE_HIDDEN, 0, 0, 127, 14, 96, 203, 127, 14, 0x7f24, 0x1b},
    {KF_SPRITE_HIDDEN, 0, 0, 127, 14, 110, 203, 127, 14, 0x7f24, 0x1b},
    {KF_SPRITE_HIDDEN, 240, 0, 7, 14, 90, 203, 7, 13, 0x7f64, 0x1d},
    {KF_SPRITE_HIDDEN, 240, 0, 7, 14, 80, 203, 7, 13, 0x7f64, 0x1d},
    {KF_SPRITE_HIDDEN, 240, 0, 7, 14, 70, 203, 7, 13, 0x7f64, 0x1d},
    {KF_SPRITE_HIDDEN, 240, 0, 7, 14, 60, 203, 7, 13, 0x7f64, 0x1d},
    {KF_SPRITE_END, 0, 0, 0, 0, 0, 0, 0, 0, 0}
};

DATA(0x80067774, 0x21c, ".data")
KfRenderModelRow render_model_rows[KF_RENDER_MODEL_ROW_COUNT] = {
    {KF_SPRITE_VISIBLE, 0, KF_LIGHTING_HUD_COMPASS,  0, 0, { 85,  85, 85, 0}, {290, 32, 50, 0}, {0}, NULL},
    {KF_SPRITE_VISIBLE, 0, KF_LIGHTING_HUD,  1, 0, {256, 256,256, 0}, { 28, 25, 32, 0}, {0}, NULL},
    {KF_SPRITE_VISIBLE, 0, KF_LIGHTING_HUD,  2, 0, {256, 256,256, 0}, { 28, 42, 32, 0}, {0}, NULL},
    {KF_SPRITE_VISIBLE, 0, KF_LIGHTING_HUD,  3, 0, {256, 256,256, 0}, { 52, 25, 32, 0}, {0}, NULL},
    {KF_SPRITE_VISIBLE, 0, KF_LIGHTING_HUD,  3, 0, {256, 256,256, 0}, { 64, 25, 32, 0}, {0}, NULL},
    {KF_SPRITE_VISIBLE, 0, KF_LIGHTING_HUD,  3, 0, {256, 256,256, 0}, { 76, 25, 32, 0}, {0}, NULL},
    {KF_SPRITE_VISIBLE, 0, KF_LIGHTING_HUD,  3, 0, {256, 256,256, 0}, { 52, 42, 32, 0}, {0}, NULL},
    {KF_SPRITE_VISIBLE, 0, KF_LIGHTING_HUD,  3, 0, {256, 256,256, 0}, { 64, 42, 32, 0}, {0}, NULL},
    {KF_SPRITE_VISIBLE, 0, KF_LIGHTING_HUD,  3, 0, {256, 256,256, 0}, { 76, 42, 32, 0}, {0}, NULL},
    {KF_SPRITE_VISIBLE, 0, KF_LIGHTING_HUD, 13, 0, { 64,   8,  2, 0}, { 16, 35, 24, 0}, {0}, NULL},
    {KF_SPRITE_VISIBLE, 0, KF_LIGHTING_HUD, 14, 0, { 64,   8,  2, 0}, { 16, 52, 24, 0}, {0}, NULL},
    {KF_SPRITE_VISIBLE, 0, KF_LIGHTING_HUD, 15, 0, {204,   8,  2, 0}, { 16, 35, 32, 0}, {0}, NULL},
    {KF_SPRITE_VISIBLE, 0, KF_LIGHTING_HUD, 15, 0, {204,   8,  2, 0}, { 16, 52, 32, 0}, {0}, NULL},
    {KF_SPRITE_VISIBLE, 0, KF_LIGHTING_PRESET_48, 16, 0, {178, 200,  2, 0}, {  5, 12, 40, 0}, {0}, NULL},
    {KF_SPRITE_END}
};

/* KF1's renderer setup passes the same two CLUT rows to GetClut; nothing in
   any King's Field II build reads them. */
DATA(0x80067990, 0x10, ".data")
static RECT render_palette_rects[2] = {{0, 500, 16, 1}, {0, 499, 16, 1}};

/* Startup may replace this load-image table from the archive. The retail
   .data run places it after the renderer tables above, not with its
   collision consumers, which precede this unit in the link. */
DATA(0x800679a0, 0xdc0, ".data")
KfCollisionDefaultRow collision_default_rows[KF_COLLISION_ROW_COUNT] = {
    {0},
    {0},
    {0},
    {0},
    {0},
    {0},
    {0},
    {0},
    {0},
    {0},
    {0},
    {0},
    {0},
    {0},
    {0},
    {0},
    {0},
    {0},
    {0},
    {0},
    {0},
    {0},
    {0},
    {0},
    {0},
    {0},
    {0},
    {0},
    {0},
    {0},
    {0},
    {0},
    {0},
    {0},
    {0},
    {0},
    {0},
    {0},
    {0},
    {0},
    {0},
    {0},
    {0},
    {0},
    {0},
    {0},
    {0},
    {0},
    {0},
    {0},
    {0},
    {0},
    {0},
    {0},
    {0},
    {0},
    {0},
    {0},
    {0},
    {0},
    {0},
    {0},
    {0},
    {0},
    {{{{0, 0, -4096}, {0, 0, -3800}, {0, 0, -3800}}, 0}, {{900, 900, 900, 900, 900, 900, 900, 900, 900}}, {{{60, 60, 60}, 0}, 32000}},
    {{{{0, 0, -4096}, {0, 0, -3800}, {0, 0, -3800}}, 0}, {{500, 500, 500, 500, 500, 500, 500, 500, 500}}, {{{60, 60, 60}, 0}, 32000}},
    {{{{0, 0, 0}, {0, 0, 0}, {0, 0, 0}}, 0}, {{0, 0, 0, 0, 0, 0, 0, 0, 0}}, {{{60, 60, 60}, 0}, 0}},
    {{{{0, 0, -4096}, {0, 0, -3800}, {0, 0, -3800}}, 0}, {{900, 900, 900, 900, 900, 900, 900, 900, 900}}, {{{90, 90, 90}, 0}, 32000}},
    {{{{3800, -2800, 1024}, {-3000, -3600, -3400}, {-1300, 2700, 1024}}, 0}, {{1024, 1024, 1024, 1024, 1024, 1024, 1024, 1024, 1024}}, {{{90, 90, 90}, 0}, 32000}},
    {{{{0, 3800, 0}, {-500, 3600, 0}, {0, 3700, 500}}, 0}, {{1200, 1200, 1200, 1200, 1200, 1200, 1200, 1200, 1200}}, {{{10, 10, 10}, 0}, 32000}},
    {{{{0, 0, -4096}, {0, 0, -3800}, {0, 0, -3800}}, 0}, {{800, 800, 800, 600, 600, 600, 500, 500, 500}}, {{{30, 25, 20}, 0}, 64268}},
    {{{{3000, 3700, -1500}, {-2000, 3700, -3000}, {-1300, -1000, 1024}}, 0}, {{4095, 4095, 4095, 4095, 4095, 4095, 4095, 4095, 4095}}, {{{100, 100, 100}, 0}, 65535}},
    {{{{0, 0, -4096}, {0, 0, -3800}, {0, 0, -3800}}, 0}, {{3095, 3095, 3095, 3095, 3095, 3095, 3095, 3095, 3095}}, {{{60, 60, 60}, 0}, 32000}},
    {{{{3800, -2800, 1024}, {-3000, -3600, -3400}, {-1300, 2700, 1024}}, 0}, {{3072, 3072, 3072, 3072, 3072, 3072, 3072, 3072, 3072}}, {{{100, 100, 100}, 0}, 32000}},
    {{{{0, 3800, 0}, {0, -3600, 0}, {0, 3700, 0}}, 0}, {{3000, 3000, 3000, 3000, 3000, 3000, 3000, 3000, 3000}}, {{{0, 0, 0}, 0}, 32000}},
    {0},
    {0},
    {0},
    {0},
    {0},
};

DATA(0x80068760, 0x1c, ".data")
KfMapMaskShapePair map_mask_pitch_shape_pairs[7] = {
    {0x0500, 0x0000},
    {(s16)0xf720, (s16)0xfb20},
    {0x0920, 0x0520},
    {0x0a80, 0x0480},
    {(s16)0xff20, (s16)0xfb20},
    {0x0120, 0x0520},
    {(s16)0xff80, (s16)0xfb80},
};

DATA(0x8006e604, 0x4, ".sdata")
CVECTOR map_textured_primitive_color = {128, 128, 128, 0};

DATA(0x8006e608, 0x4, ".sdata")
s32 render_model_yaw_smoothing_accumulator = 0;

/* Unreferenced in every retail build; owner and type unresolved. */
DATA(0x8006e60c, 0x4, ".sdata")
static s32 display_unreferenced_sdata_word = 0;

DATA(0x8006e610, 0x8, ".sdata")
RECT menu_transition_rect = {KF_DISPLAY_WIDTH, 0, KF_DISPLAY_WIDTH, KF_DISPLAY_HEIGHT};

DATA(0x800fc99c, 0x32000, ".bss")
u8 display_primitive_memory[KF_DISPLAY_BUFFER_COUNT * KF_GAME_PRIMITIVE_BUFFER_BYTES];

DATA(0x8017e084, 0x17cf0, ".bss")
KfGraphicsRuntimeGame game_graphics_runtime;

DATA(0x801da554, 0x4, ".bss")
s32 display_frame_cleared_word;

enum { KF_TMD_DEPTH_SHIFT = 2 };

RODATA(0x80011410, 0x74)

ADDRESS(0x8002dddc, 0x30)
void fog_set_near(s32 distance)
{
    GRAPHICS.render_grid.fog_near_distance = distance;
    SetFogNear((distance & KF_FOG_DISTANCE_MASK) >> 1, KF_PROJECTION_DISTANCE);
}

ADDRESS(0x8002de0c, 0x174)
void display_initialize(void)
{
    ResetGraph(KF_GPU_RESET_KEEP_DISPLAY);
    SetDispMask(1);
    InitGeom();
    SetGeomOffset(KF_DISPLAY_WIDTH / 2, KF_DISPLAY_HEIGHT / 2);
    SetGeomScreen(KF_PROJECTION_DISTANCE);
    InitClip(GRAPHICS.clip_edges, KF_DISPLAY_WIDTH, KF_DISPLAY_HEIGHT, KF_CLIP_DEPTH, 0, KF_CLIP_FAR);
    SetDefDrawEnv(&GRAPHICS.display_draw_environments[0], 0, 0, KF_DISPLAY_WIDTH, KF_DISPLAY_HEIGHT);
    SetDefDispEnv(&GRAPHICS.display_disp_environments[0], 0, KF_DISPLAY_HEIGHT, KF_DISPLAY_WIDTH, KF_DISPLAY_HEIGHT);
    SetDefDrawEnv(&GRAPHICS.display_draw_environments[1], 0, KF_DISPLAY_HEIGHT, KF_DISPLAY_WIDTH, KF_DISPLAY_HEIGHT);
    SetDefDispEnv(&GRAPHICS.display_disp_environments[1], 0, 0, KF_DISPLAY_WIDTH, KF_DISPLAY_HEIGHT);
    GRAPHICS.display_draw_environments[0].dtd = GRAPHICS.display_draw_environments[1].dtd = 1;
    GRAPHICS.display_draw_environments[0].isbg = 1;
    GRAPHICS.display_draw_environments[1].isbg = 1;
    setRGB0(&GRAPHICS.display_draw_environments[0], 0, 0, 0);
    setRGB0(&GRAPHICS.display_draw_environments[1], 0, 0, 0);
    PutDispEnv(&GRAPHICS.display_disp_environments[0]);
    SetGeomScreen(KF_PROJECTION_DISTANCE);
    SetBackColor(0, 0, 0);
    SetFarColor(0, 0, 0);
    fog_set_near(KF_INITIAL_FOG_NEAR_DISTANCE);
    display_reset();
}

ADDRESS(0x8002df80, 0xe4)
void display_reset(void)
{
    u8 *memory;
    KfNotificationId *message_id;
    u8 messages_left;
    KfFloorItem *item;
    s32 items_left;

    DISPLAY.buffer_index = KF_DISPLAY_BUFFER_UNINITIALIZED;
    memory = display_primitive_memory;
    DISPLAY.primitive_buffers[0].start = DISPLAY.asset_load_buffer = memory;
    memory += KF_GAME_PRIMITIVE_BUFFER_BYTES;
    DISPLAY.primitive_buffers[1].start = DISPLAY.primitive_buffers[0].end = memory;
    memory += KF_GAME_PRIMITIVE_BUFFER_BYTES;
    DISPLAY.primitive_buffers[1].end = memory;
    GRAPHICS.notification_control.effect_phase = KF_NOTIFICATION_IDLE;
    GRAPHICS.notification_control.queue_tail = 0;
    GRAPHICS.notification_control.queue_head = 0;
    message_id = GRAPHICS.notification_message_ids;
    messages_left = KF_NOTIFICATION_CAPACITY - 1;
    do {
        *message_id++ = KF_NOTIFICATION_NONE;
    } while (messages_left-- != 0);
    item = GRAPHICS.floor_items;
    for (items_left = KF_FLOOR_ITEM_CAPACITY - 1; items_left != -1; items_left--) {
        item->kind = KF_FLOOR_ITEM_NONE;
        item++;
    }
    GRAPHICS.color_overlay_control = KF_COLOR_OVERLAY_OFF;
    GRAPHICS.map_placed_frame_counter = 0;
    pool_reset();
}

ADDRESS(0x8002e064, 0x98)
void display_begin_frame(void)
{
    DISPLAY.buffer_index = display_next_buffer(DISPLAY.buffer_index);
    DISPLAY.primitive_buffer = &DISPLAY.primitive_buffers[KF_ENUM_ENCODE(u8, DISPLAY.buffer_index)];
    DISPLAY.ordering_table = DISPLAY.ordering_tables[KF_ENUM_ENCODE(u8, DISPLAY.buffer_index)].entries;
    ClearOTagR(DISPLAY.ordering_table, KF_GAME_ORDERING_TABLE_LENGTH);
    DISPLAY.primitive_buffer->cursor = DISPLAY.primitive_buffer->start;
    display_frame_cleared_word = 0;
    GRAPHICS.frame_counter_b = 0;
    GRAPHICS.frame_counter_a = 0;
}

ADDRESS(0x8002e0fc, 0x94)
void display_present_frame(void)
{
    DrawSync(0);
    VSync(0);
    PutDrawEnv(&GRAPHICS.display_draw_environments[KF_ENUM_ENCODE(u8, DISPLAY.buffer_index)]);
    PutDispEnv(&GRAPHICS.display_disp_environments[KF_ENUM_ENCODE(u8, DISPLAY.buffer_index)]);
    DrawOTag(DISPLAY.ordering_table + (KF_GAME_ORDERING_TABLE_LENGTH - 1));
}

ADDRESS(0x8002e190, 0x2c)
void tmd_select(u16 slot)
{
    GRAPHICS.tmd_state.current_asset = GRAPHICS.tmd_state.slots[slot];
}

ADDRESS(0x8002e1bc, 0x24)
KfTmdObject *tmd_get_object(u16 index)
{
    return TMD_OBJECTS(GRAPHICS.tmd_state.current_asset) + index;
}

ADDRESS(0x8002e1e0, 0x10)
void tmd_set_current_vertices(SVECTOR *vertices)
{
    GRAPHICS.current_tmd_vertices = vertices;
}

ADDRESS(0x8002e1f0, 0x3c)
void tmd_select_object_vertices(u16 index)
{
    GRAPHICS.current_tmd_vertices =
        TMD_OBJECT_VERTICES(GRAPHICS.tmd_state.current_asset, tmd_get_object(index));
}

ADDRESS(0x8002e22c, 0xe8)
void display_set_view_transform(const VECTOR *position, const SVECTOR *rotation)
{
    struct KfEulerAngles angles;

    if (position != NULL) {
        GRAPHICS.render_state.view_position = *position;
        GRAPHICS.render_state.view_cell_x =
            GRAPHICS.render_state.view_position.vx >> KF_FIXED11_BITS;
        GRAPHICS.render_state.view_cell_z =
            GRAPHICS.render_state.view_position.vz >> KF_FIXED11_BITS;
    }
    if (rotation != NULL) {
        GRAPHICS.render_state.view_rotation = *rotation;
    }

    angles.x = GRAPHICS.render_state.view_rotation.vx;
    angles.y = -GRAPHICS.render_state.view_rotation.vy;
    angles.z = GRAPHICS.render_state.view_rotation.vz;
    matrix_set_rotation_xzy(&angles, &GRAPHICS.render_state.view_matrix);
    matrix_set_rotation_x(angles.x, &GRAPHICS.render_state.pitch_matrix);
}

ADDRESS(0x8002e314, 0x2d4)
void tmd_prepare_primitive_indices(KfTmdHeader *tmd)
{
    KfTmdObject *object;
    u8 *packet;
    KfTmdPrimitive *primitive;
    u32 objects_left;
    u32 primitives_left;
    KfTmdPacketHeader header;

    objects_left = tmd->object_count;
    object = TMD_OBJECTS(tmd);
    while (--objects_left != (u32)-1) {
        primitives_left = object->primitive_count;
        packet = TMD_SECTION(tmd, object->primitive_offset);
        while (--primitives_left != (u32)-1) {
            primitive = (KfTmdPrimitive *)TMD_PACKET_BODY(packet);
            header.word = *(u32 *)packet;
            packet = (u8 *)primitive + header.bytes.input_length * KF_TMD_WORD_BYTES;
            switch (tmd_packet_kind(header.word)) {
            case KF_TMD_MODE_F3: {
                primitive->f3.vertex0 <<= KF_TMD_VECTOR_OFFSET_SHIFT;
                primitive->f3.vertex1 <<= KF_TMD_VECTOR_OFFSET_SHIFT;
                primitive->f3.vertex2 <<= KF_TMD_VECTOR_OFFSET_SHIFT;
                primitive->f3.normal <<= KF_TMD_VECTOR_OFFSET_SHIFT;
                break;
            }
            case KF_TMD_MODE_G3: {
                primitive->g3.vertex0 <<= KF_TMD_VECTOR_OFFSET_SHIFT;
                primitive->g3.vertex1 <<= KF_TMD_VECTOR_OFFSET_SHIFT;
                primitive->g3.vertex2 <<= KF_TMD_VECTOR_OFFSET_SHIFT;
                primitive->g3.normal0 <<= KF_TMD_VECTOR_OFFSET_SHIFT;
                primitive->g3.normal1 <<= KF_TMD_VECTOR_OFFSET_SHIFT;
                primitive->g3.normal2 <<= KF_TMD_VECTOR_OFFSET_SHIFT;
                break;
            }
            case KF_TMD_MODE_FT3: {
                primitive->ft3.vertex0 <<= KF_TMD_VECTOR_OFFSET_SHIFT;
                primitive->ft3.vertex1 <<= KF_TMD_VECTOR_OFFSET_SHIFT;
                primitive->ft3.vertex2 <<= KF_TMD_VECTOR_OFFSET_SHIFT;
                primitive->ft3.normal <<= KF_TMD_VECTOR_OFFSET_SHIFT;
                break;
            }
            case KF_TMD_MODE_GT3: {
                primitive->gt3.vertex0 <<= KF_TMD_VECTOR_OFFSET_SHIFT;
                primitive->gt3.vertex1 <<= KF_TMD_VECTOR_OFFSET_SHIFT;
                primitive->gt3.vertex2 <<= KF_TMD_VECTOR_OFFSET_SHIFT;
                primitive->gt3.normal0 <<= KF_TMD_VECTOR_OFFSET_SHIFT;
                primitive->gt3.normal1 <<= KF_TMD_VECTOR_OFFSET_SHIFT;
                primitive->gt3.normal2 <<= KF_TMD_VECTOR_OFFSET_SHIFT;
                break;
            }
            case KF_TMD_MODE_F4: {
                primitive->f4.vertex0 <<= KF_TMD_VECTOR_OFFSET_SHIFT;
                primitive->f4.vertex1 <<= KF_TMD_VECTOR_OFFSET_SHIFT;
                primitive->f4.vertex2 <<= KF_TMD_VECTOR_OFFSET_SHIFT;
                primitive->f4.vertex3 <<= KF_TMD_VECTOR_OFFSET_SHIFT;
                primitive->f4.normal <<= KF_TMD_VECTOR_OFFSET_SHIFT;
                break;
            }
            case KF_TMD_MODE_G4: {
                primitive->g4.vertex0 <<= KF_TMD_VECTOR_OFFSET_SHIFT;
                primitive->g4.vertex1 <<= KF_TMD_VECTOR_OFFSET_SHIFT;
                primitive->g4.vertex2 <<= KF_TMD_VECTOR_OFFSET_SHIFT;
                primitive->g4.vertex3 <<= KF_TMD_VECTOR_OFFSET_SHIFT;
                primitive->g4.normal0 <<= KF_TMD_VECTOR_OFFSET_SHIFT;
                primitive->g4.normal1 <<= KF_TMD_VECTOR_OFFSET_SHIFT;
                primitive->g4.normal2 <<= KF_TMD_VECTOR_OFFSET_SHIFT;
                primitive->g4.normal3 <<= KF_TMD_VECTOR_OFFSET_SHIFT;
                break;
            }
            case KF_TMD_MODE_FT4: {
                primitive->ft4.vertex0 <<= KF_TMD_VECTOR_OFFSET_SHIFT;
                primitive->ft4.vertex1 <<= KF_TMD_VECTOR_OFFSET_SHIFT;
                primitive->ft4.vertex2 <<= KF_TMD_VECTOR_OFFSET_SHIFT;
                primitive->ft4.vertex3 <<= KF_TMD_VECTOR_OFFSET_SHIFT;
                primitive->ft4.normal <<= KF_TMD_VECTOR_OFFSET_SHIFT;
                break;
            }
            case KF_TMD_MODE_GT4: {
                primitive->gt4.vertex0 <<= KF_TMD_VECTOR_OFFSET_SHIFT;
                primitive->gt4.vertex1 <<= KF_TMD_VECTOR_OFFSET_SHIFT;
                primitive->gt4.vertex2 <<= KF_TMD_VECTOR_OFFSET_SHIFT;
                primitive->gt4.vertex3 <<= KF_TMD_VECTOR_OFFSET_SHIFT;
                primitive->gt4.normal0 <<= KF_TMD_VECTOR_OFFSET_SHIFT;
                primitive->gt4.normal1 <<= KF_TMD_VECTOR_OFFSET_SHIFT;
                primitive->gt4.normal2 <<= KF_TMD_VECTOR_OFFSET_SHIFT;
                primitive->gt4.normal3 <<= KF_TMD_VECTOR_OFFSET_SHIFT;
                break;
            }
            }
        }
        object++;
    }
}

ADDRESS(0x8002e5e8, 0x40)
void tmd_register(u16 slot, KfTmdHeader *tmd)
{
    game_graphics_runtime.tmd_state.current_asset = game_graphics_runtime.tmd_state.slots[slot] = tmd;
    tmd_prepare_primitive_indices(tmd);
}

ADDRESS(0x8002e628, 0x20)
void tmd_set_slot(u16 slot, KfTmdHeader *tmd)
{
    game_graphics_runtime.tmd_state.slots[slot] = tmd;
}

/* Left empty in retail. */
ADDRESS(0x8002e648, 0x8)
void tmd_release_slot(void)
{
}

ADDRESS(0x8002e650, 0x17c)
void tmd_project_vertices_with_fog(s32 count)
{
    KfScreenVertex *projected;
    SVECTOR *vertex;
    long perspective;
    long gte_flags;
    s32 remaining;

    projected = game_graphics_runtime.tmd_projected_vertices;
    vertex = game_graphics_runtime.current_tmd_vertices;
    remaining = count;
    if (game_graphics_runtime.render_grid.fog_near_distance >= 32000) {
        for (remaining--; remaining != -1; remaining--) {
            projected->sz = RotTransPers(vertex, (long *)projected,
                                         &perspective, &gte_flags);
            projected->depth_cue = 0;
            projected++;
            vertex++;
        }
    } else if (game_graphics_runtime.render_grid.fog_near_distance & 0x8000) {
        for (remaining--; remaining != -1; remaining--) {
            projected->sz = RotTransPers(vertex, (long *)projected,
                                         &perspective, &gte_flags);
            perspective -= 800;
            if (perspective < 0) {
                perspective = 0;
            }
            projected->depth_cue = (u16)perspective << 1;
            projected++;
            vertex++;
        }
    } else {
        for (remaining--; remaining != -1; remaining--) {
            projected->sz = RotTransPers(vertex, (long *)projected,
                                         &perspective, &gte_flags);
            if (perspective >= 2800) {
                perspective += (perspective - 2800) * 2;
            }
            projected->depth_cue = perspective;
            projected++;
            vertex++;
        }
    }
}

ADDRESS(0x8002e7cc, 0x144)
void tmd_project_vertices_mark_clipped(s32 count)
{
    KfScreenVertex *projected;
    SVECTOR *vertex;
    long perspective;
    long gte_flags;
    s32 remaining;

    projected = game_graphics_runtime.tmd_projected_vertices;
    vertex = game_graphics_runtime.current_tmd_vertices;
    remaining = count;
    if (game_graphics_runtime.render_grid.fog_near_distance >= 32000) {
        for (remaining--; remaining != -1; remaining--) {
            projected->sz = RotTransPers(vertex, (long *)projected,
                                         &perspective, &gte_flags);
            if (gte_flags != 0x1000) {
                projected->sz = -1;
            }
            projected->depth_cue = 0;
            projected++;
            vertex++;
        }
    } else {
        for (remaining--; remaining != -1; remaining--) {
            projected->sz = RotTransPers(vertex, (long *)projected,
                                         &perspective, &gte_flags);
            if (gte_flags != 0x1000) {
                projected->sz = -1;
            }
            if (perspective >= 2800) {
                perspective += (perspective - 2800) * 2;
            }
            projected->depth_cue = perspective;
            projected++;
            vertex++;
        }
    }
}

ADDRESS(0x8002e910, 0xa8)
void tmd_transform_vertices(s32 count)
{
    KfScreenVertex *projected;
    SVECTOR *vertex;
    VECTOR transformed;
    long gte_flags;
    s32 remaining;

    projected = game_graphics_runtime.tmd_projected_vertices;
    vertex = game_graphics_runtime.current_tmd_vertices;
    for (remaining = count - 1; remaining != -1; remaining--) {
        RotTrans(vertex, &transformed, &gte_flags);
        projected->x = transformed.vx;
        projected->y = transformed.vy;
        projected->depth_cue = transformed.vz;
        projected->sz = transformed.vz >> KF_TMD_DEPTH_SHIFT;
        projected++;
        vertex++;
    }
}

ADDRESS(0x8002e9b8, 0xa8)
void tmd_transform_vertices_depth(s32 count, s16 depth)
{
    KfScreenVertex *projected;
    SVECTOR *vertex;
    VECTOR transformed;
    long gte_flags;
    s32 remaining;

    projected = game_graphics_runtime.tmd_projected_vertices;
    vertex = game_graphics_runtime.current_tmd_vertices;
    for (remaining = count - 1; remaining != -1; remaining--) {
        RotTrans(vertex, &transformed, &gte_flags);
        projected->x = transformed.vx;
        projected->y = transformed.vy;
        projected->depth_cue = transformed.vz;
        projected->sz = depth;
        projected++;
        vertex++;
    }
}

ADDRESS(0x8002ea60, 0x8c)
void tmd_project_vertices(s32 count)
{
    KfScreenVertex *projected;
    SVECTOR *vertex;
    long perspective;
    long gte_flags;
    s32 remaining;

    projected = game_graphics_runtime.tmd_projected_vertices;
    vertex = game_graphics_runtime.current_tmd_vertices;
    for (remaining = count - 1; remaining != -1; remaining--) {
        projected->sz = RotTransPers(vertex, (long *)projected, &perspective, &gte_flags);
        projected->depth_cue = 0;
        projected++;
        vertex++;
    }
}

/* Prepared vertex and normal indices are byte offsets into their arrays;
 * NormalClip and the GPU packets take each vertex's packed screen XY word. */
#define TMD_VERTEX(base, offset) ((KfScreenVertex *)((u8 *)(base) + (offset)))
#define TMD_VECTOR(base, offset) ((SVECTOR *)((u8 *)(base) + (offset)))
#define TMD_XY(vertex) (*(long *)(vertex))

ADDRESS(0x8002eaec, 0x728)
void render_enqueue_blended_tmd(u16 object_index, s32 depth_bias,
                   KF_ENUM_PARAM(KfRenderQueueMode, s32) render_mode)
{
    KfTmdObject *object;
    u8 *normals;
    u8 *packet;
    u32 remaining;
    u16 blend_bits = KF_ENUM_ENCODE(u16, render_mode) << 5;

    object = tmd_get_object(object_index);
    packet = TMD_SECTION(game_graphics_runtime.tmd_state.current_asset, object->primitive_offset);
    normals = TMD_SECTION(game_graphics_runtime.tmd_state.current_asset, object->normal_offset);
    remaining = object->primitive_count;
    while (remaining-- != 0) {
        u8 *vertices = (u8 *)game_graphics_runtime.tmd_projected_vertices;
        KfTmdPacketHeader header;
        u8 mode;
        KfTmdPrimitive *face;
        KfScreenVertex *va;
        KfScreenVertex *vb;
        KfScreenVertex *vc;
        KfScreenVertex *vd;
        s32 depth;
        s32 average;

        header.word = *(u32 *)packet;
        mode = header.word >> 24;
        packet += KF_TMD_PACKET_HEADER_BYTES;
        face = (KfTmdPrimitive *)packet;
        switch (mode & KF_TMD_MODE_MASK) {
        case KF_TMD_MODE_FT3: {
            POLY_FT3 *prim;

            va = TMD_VERTEX(vertices, face->ft3.vertex0);
            vb = TMD_VERTEX(vertices, face->ft3.vertex1);
            vc = TMD_VERTEX(vertices, face->ft3.vertex2);
            if (NormalClip(TMD_XY(va), TMD_XY(vb), TMD_XY(vc)) <= 0) {
                break;
            }
            prim = (POLY_FT3 *)game_graphics_runtime.display_state.primitive_buffer->cursor;
            game_graphics_runtime.display_state.primitive_buffer->cursor += sizeof(POLY_FT3);
            if (game_graphics_runtime.display_state.primitive_buffer->cursor >
                game_graphics_runtime.display_state.primitive_buffer->end) {
                return;
                }
            prim->clut = face->ft3.clut;
            prim->tpage = (face->ft3.tpage & KF_GPU_TPAGE_WITHOUT_ABR) | blend_bits;
            *(long *)&prim->x0 = TMD_XY(va);
            *(long *)&prim->x1 = TMD_XY(vb);
            *(long *)&prim->x2 = TMD_XY(vc);
            *(u16 *)&prim->u0 = face->ft3.uv0;
            *(u16 *)&prim->u1 = face->ft3.uv1;
            *(u16 *)&prim->u2 = face->ft3.uv2;
            NormalColorDpq(TMD_VECTOR(normals, face->ft3.normal),
                           &map_textured_primitive_color,
                           (va->depth_cue + vb->depth_cue + vc->depth_cue) / 3,
                           (CVECTOR *)&prim->r0);
            setlen(prim, 7);
            prim->code = KF_TMD_MODE_FT3 | KF_TMD_MODE_SEMI_TRANSPARENT;
            depth = (va->sz + vb->sz + vc->sz) / 3;
            if (depth <= 0) {
                break;
            }
            depth += depth_bias;
            if ((u32)depth < KF_MAP_OT_DEPTH_LIMIT) {
                AddPrim((void *)&game_graphics_runtime.display_state.ordering_table[depth], (void *)prim);
            }
            break;
        }
        case KF_TMD_MODE_GT3: {
            KfGpuGT3 *prim;

            va = TMD_VERTEX(vertices, face->gt3.vertex0);
            vb = TMD_VERTEX(vertices, face->gt3.vertex1);
            vc = TMD_VERTEX(vertices, face->gt3.vertex2);
            if (NormalClip(TMD_XY(va), TMD_XY(vb), TMD_XY(vc)) <= 0) {
                break;
            }
            prim = (KfGpuGT3 *)game_graphics_runtime.display_state.primitive_buffer->cursor;
            game_graphics_runtime.display_state.primitive_buffer->cursor += sizeof(POLY_GT3);
            if (game_graphics_runtime.display_state.primitive_buffer->cursor >
                game_graphics_runtime.display_state.primitive_buffer->end) {
                return;
                }
            prim->packed.clut = face->gt3.clut;
            prim->packed.tpage = (face->gt3.tpage & KF_GPU_TPAGE_WITHOUT_ABR) | blend_bits;
            prim->packed.xy0 = TMD_XY(va);
            prim->packed.xy1 = TMD_XY(vb);
            prim->packed.xy2 = TMD_XY(vc);
            prim->packed.uv0 = face->gt3.uv0;
            prim->packed.uv1 = face->gt3.uv1;
            prim->packed.uv2 = face->gt3.uv2;
            NormalColorDpq3(TMD_VECTOR(normals, face->gt3.normal0),
                            TMD_VECTOR(normals, face->gt3.normal1),
                            TMD_VECTOR(normals, face->gt3.normal2),
                            &map_textured_primitive_color, va->depth_cue,
                            &prim->packed.color0, &prim->packed.color1,
                            &prim->packed.color2);
            setlen(&prim->sdk, 9);
            prim->sdk.code = KF_TMD_MODE_GT3 | KF_TMD_MODE_SEMI_TRANSPARENT;
            depth = (va->sz + vb->sz + vc->sz) / 3;
            if (depth <= 0) {
                break;
            }
            depth += depth_bias;
            if ((u32)depth < KF_MAP_OT_DEPTH_LIMIT) {
                AddPrim((void *)&game_graphics_runtime.display_state.ordering_table[depth], (void *)&prim->sdk);
            }
            break;
        }
        case KF_TMD_MODE_GT4: {
            KfGpuGT4 *prim;

            va = TMD_VERTEX(vertices, face->gt4.vertex0);
            vb = TMD_VERTEX(vertices, face->gt4.vertex1);
            vc = TMD_VERTEX(vertices, face->gt4.vertex2);
            if (NormalClip(TMD_XY(va), TMD_XY(vb), TMD_XY(vc)) <= 0) {
                break;
            }
            vd = TMD_VERTEX(vertices, face->gt4.vertex3);
            prim = (KfGpuGT4 *)game_graphics_runtime.display_state.primitive_buffer->cursor;
            game_graphics_runtime.display_state.primitive_buffer->cursor += sizeof(POLY_GT4);
            if (game_graphics_runtime.display_state.primitive_buffer->cursor >
                game_graphics_runtime.display_state.primitive_buffer->end) {
                return;
                }
            prim->packed.clut = face->gt4.clut;
            prim->packed.tpage = (face->gt4.tpage & KF_GPU_TPAGE_WITHOUT_ABR) | blend_bits;
            prim->packed.xy0 = TMD_XY(va);
            prim->packed.xy1 = TMD_XY(vb);
            prim->packed.xy2 = TMD_XY(vc);
            prim->packed.xy3 = TMD_XY(vd);
            prim->packed.uv0 = face->gt4.uv0;
            prim->packed.uv1 = face->gt4.uv1;
            prim->packed.uv2 = face->gt4.uv2;
            prim->packed.uv3 = face->gt4.uv3;
            NormalColorDpq3(TMD_VECTOR(normals, face->gt4.normal0),
                            TMD_VECTOR(normals, face->gt4.normal1),
                            TMD_VECTOR(normals, face->gt4.normal2),
                            &map_textured_primitive_color, va->depth_cue,
                            &prim->packed.color0, &prim->packed.color1,
                            &prim->packed.color2);
            NormalColorDpq(TMD_VECTOR(normals, face->gt4.normal3),
                           &map_textured_primitive_color, va->depth_cue,
                           &prim->packed.color3);
            setlen(&prim->sdk, 12);
            prim->sdk.code = KF_TMD_MODE_GT4 | KF_TMD_MODE_SEMI_TRANSPARENT;
            average = (va->sz + vb->sz + vc->sz + vd->sz) >> 2;
            if (average <= 0) {
                break;
            }
            depth = average + depth_bias;
            if ((u32)depth < KF_MAP_OT_DEPTH_LIMIT) {
                AddPrim((void *)&game_graphics_runtime.display_state.ordering_table[depth], (void *)&prim->sdk);
            }
            break;
        }
        case KF_TMD_MODE_FT4: {
            POLY_FT4 *prim;

            va = TMD_VERTEX(vertices, face->ft4.vertex0);
            vb = TMD_VERTEX(vertices, face->ft4.vertex1);
            vc = TMD_VERTEX(vertices, face->ft4.vertex2);
            if (NormalClip(TMD_XY(va), TMD_XY(vb), TMD_XY(vc)) <= 0) {
                break;
            }
            vd = TMD_VERTEX(vertices, face->ft4.vertex3);
            prim = (POLY_FT4 *)game_graphics_runtime.display_state.primitive_buffer->cursor;
            game_graphics_runtime.display_state.primitive_buffer->cursor += sizeof(POLY_FT4);
            if (game_graphics_runtime.display_state.primitive_buffer->cursor >
                game_graphics_runtime.display_state.primitive_buffer->end) {
                return;
                }
            prim->clut = face->ft4.clut;
            prim->tpage = (face->ft4.tpage & KF_GPU_TPAGE_WITHOUT_ABR) | blend_bits;
            *(long *)&prim->x0 = TMD_XY(va);
            *(long *)&prim->x1 = TMD_XY(vb);
            *(long *)&prim->x2 = TMD_XY(vc);
            *(long *)&prim->x3 = TMD_XY(vd);
            *(u16 *)&prim->u0 = face->ft4.uv0;
            *(u16 *)&prim->u1 = face->ft4.uv1;
            *(u16 *)&prim->u2 = face->ft4.uv2;
            *(u16 *)&prim->u3 = face->ft4.uv3;
            NormalColorDpq(TMD_VECTOR(normals, face->ft4.normal),
                           &map_textured_primitive_color,
                           (va->depth_cue + vb->depth_cue + vc->depth_cue + vd->depth_cue) >> 2,
                           (CVECTOR *)&prim->r0);
            setlen(prim, 9);
            prim->code = KF_TMD_MODE_FT4 | KF_TMD_MODE_SEMI_TRANSPARENT;
            average = (va->sz + vb->sz + vc->sz + vd->sz) >> 2;
            if (average <= 0) {
                break;
            }
            depth = average + depth_bias;
            if ((u32)depth < KF_MAP_OT_DEPTH_LIMIT) {
                AddPrim((void *)&game_graphics_runtime.display_state.ordering_table[depth], (void *)prim);
            }
            break;
        }
        }
        packet += TMD_PACKET_BODY_BYTES(header.word);
    }
}

ADDRESS(0x8002f214, 0x704)
void render_enqueue_textured_tmd(u16 object_index, s32 depth_bias)
{
    KfTmdObject *object;
    u8 *normals;
    u8 *packet;
    KfTmdPacketHeader header;
    u32 remaining;

    object = tmd_get_object(object_index);
    packet = TMD_SECTION(game_graphics_runtime.tmd_state.current_asset, object->primitive_offset);
    normals = TMD_SECTION(game_graphics_runtime.tmd_state.current_asset, object->normal_offset);
    remaining = object->primitive_count;
    while (remaining-- != 0) {
        u8 *vertices = (u8 *)game_graphics_runtime.tmd_projected_vertices;
        s32 mode;
        KfTmdPrimitive *face;
        KfScreenVertex *va;
        KfScreenVertex *vb;
        KfScreenVertex *vc;
        KfScreenVertex *vd;
        s32 depth;
        s32 average;

        header.word = *(u32 *)packet;
        mode = header.word >> 24;
        packet += KF_TMD_PACKET_HEADER_BYTES;
        face = (KfTmdPrimitive *)packet;
        switch (mode & KF_TMD_MODE_MASK) {
        case KF_TMD_MODE_FT3: {
            POLY_FT3 *prim;

            va = TMD_VERTEX(vertices, face->ft3.vertex0);
            vb = TMD_VERTEX(vertices, face->ft3.vertex1);
            vc = TMD_VERTEX(vertices, face->ft3.vertex2);
            if (NormalClip(TMD_XY(va), TMD_XY(vb), TMD_XY(vc)) <= 0) {
                break;
            }
            prim = (POLY_FT3 *)game_graphics_runtime.display_state.primitive_buffer->cursor;
            game_graphics_runtime.display_state.primitive_buffer->cursor += sizeof(POLY_FT3);
            if (game_graphics_runtime.display_state.primitive_buffer->cursor >
                game_graphics_runtime.display_state.primitive_buffer->end) {
                return;
                }
            prim->clut = face->ft3.clut;
            prim->tpage = face->ft3.tpage;
            *(long *)&prim->x0 = TMD_XY(va);
            *(long *)&prim->x1 = TMD_XY(vb);
            *(long *)&prim->x2 = TMD_XY(vc);
            *(u16 *)&prim->u0 = face->ft3.uv0;
            *(u16 *)&prim->u1 = face->ft3.uv1;
            *(u16 *)&prim->u2 = face->ft3.uv2;
            NormalColorDpq(TMD_VECTOR(normals, face->ft3.normal),
                           &map_textured_primitive_color,
                           (va->depth_cue + vb->depth_cue + vc->depth_cue) / 3,
                           (CVECTOR *)&prim->r0);
            setlen(prim, 7);
            prim->code = mode;
            depth = (va->sz + vb->sz + vc->sz) / 3;
            if (depth <= 0) {
                break;
            }
            depth += depth_bias;
            if ((u32)depth < KF_MAP_OT_DEPTH_LIMIT) {
                AddPrim((void *)&game_graphics_runtime.display_state.ordering_table[depth], (void *)prim);
            }
            break;
        }
        case KF_TMD_MODE_GT3: {
            KfGpuGT3 *prim;

            va = TMD_VERTEX(vertices, face->gt3.vertex0);
            vb = TMD_VERTEX(vertices, face->gt3.vertex1);
            vc = TMD_VERTEX(vertices, face->gt3.vertex2);
            if (NormalClip(TMD_XY(va), TMD_XY(vb), TMD_XY(vc)) <= 0) {
                break;
            }
            prim = (KfGpuGT3 *)game_graphics_runtime.display_state.primitive_buffer->cursor;
            game_graphics_runtime.display_state.primitive_buffer->cursor += sizeof(POLY_GT3);
            if (game_graphics_runtime.display_state.primitive_buffer->cursor >
                game_graphics_runtime.display_state.primitive_buffer->end) {
                return;
                }
            prim->packed.clut = face->gt3.clut;
            prim->packed.tpage = face->gt3.tpage;
            prim->packed.xy0 = TMD_XY(va);
            prim->packed.xy1 = TMD_XY(vb);
            prim->packed.xy2 = TMD_XY(vc);
            prim->packed.uv0 = face->gt3.uv0;
            prim->packed.uv1 = face->gt3.uv1;
            prim->packed.uv2 = face->gt3.uv2;
            NormalColorDpq3(TMD_VECTOR(normals, face->gt3.normal0),
                            TMD_VECTOR(normals, face->gt3.normal1),
                            TMD_VECTOR(normals, face->gt3.normal2),
                            &map_textured_primitive_color, va->depth_cue,
                            &prim->packed.color0, &prim->packed.color1,
                            &prim->packed.color2);
            setlen(&prim->sdk, 9);
            prim->sdk.code = mode;
            depth = (va->sz + vb->sz + vc->sz) / 3;
            if (depth <= 0) {
                break;
            }
            depth += depth_bias;
            if ((u32)depth < KF_MAP_OT_DEPTH_LIMIT) {
                AddPrim((void *)&game_graphics_runtime.display_state.ordering_table[depth], (void *)&prim->sdk);
            }
            break;
        }
        case KF_TMD_MODE_GT4: {
            KfGpuGT4 *prim;

            va = TMD_VERTEX(vertices, face->gt4.vertex0);
            vb = TMD_VERTEX(vertices, face->gt4.vertex1);
            vc = TMD_VERTEX(vertices, face->gt4.vertex2);
            if (NormalClip(TMD_XY(va), TMD_XY(vb), TMD_XY(vc)) <= 0) {
                break;
            }
            vd = TMD_VERTEX(vertices, face->gt4.vertex3);
            prim = (KfGpuGT4 *)game_graphics_runtime.display_state.primitive_buffer->cursor;
            game_graphics_runtime.display_state.primitive_buffer->cursor += sizeof(POLY_GT4);
            if (game_graphics_runtime.display_state.primitive_buffer->cursor >
                game_graphics_runtime.display_state.primitive_buffer->end) {
                return;
                }
            prim->packed.clut = face->gt4.clut;
            prim->packed.tpage = face->gt4.tpage;
            prim->packed.xy0 = TMD_XY(va);
            prim->packed.xy1 = TMD_XY(vb);
            prim->packed.xy2 = TMD_XY(vc);
            prim->packed.xy3 = TMD_XY(vd);
            prim->packed.uv0 = face->gt4.uv0;
            prim->packed.uv1 = face->gt4.uv1;
            prim->packed.uv2 = face->gt4.uv2;
            prim->packed.uv3 = face->gt4.uv3;
            NormalColorDpq3(TMD_VECTOR(normals, face->gt4.normal0),
                            TMD_VECTOR(normals, face->gt4.normal1),
                            TMD_VECTOR(normals, face->gt4.normal2),
                            &map_textured_primitive_color, va->depth_cue,
                            &prim->packed.color0, &prim->packed.color1,
                            &prim->packed.color2);
            NormalColorDpq(TMD_VECTOR(normals, face->gt4.normal3),
                           &map_textured_primitive_color, va->depth_cue,
                           &prim->packed.color3);
            setlen(&prim->sdk, 12);
            prim->sdk.code = mode;
            average = (va->sz + vb->sz + vc->sz + vd->sz) >> 2;
            if (average <= 0) {
                break;
            }
            depth = average + depth_bias;
            if ((u32)depth < KF_MAP_OT_DEPTH_LIMIT) {
                AddPrim((void *)&game_graphics_runtime.display_state.ordering_table[depth], (void *)&prim->sdk);
            }
            break;
        }
        case KF_TMD_MODE_FT4: {
            POLY_FT4 *prim;

            va = TMD_VERTEX(vertices, face->ft4.vertex0);
            vb = TMD_VERTEX(vertices, face->ft4.vertex1);
            vc = TMD_VERTEX(vertices, face->ft4.vertex2);
            if (NormalClip(TMD_XY(va), TMD_XY(vb), TMD_XY(vc)) <= 0) {
                break;
            }
            vd = TMD_VERTEX(vertices, face->ft4.vertex3);
            prim = (POLY_FT4 *)game_graphics_runtime.display_state.primitive_buffer->cursor;
            game_graphics_runtime.display_state.primitive_buffer->cursor += sizeof(POLY_FT4);
            if (game_graphics_runtime.display_state.primitive_buffer->cursor >
                game_graphics_runtime.display_state.primitive_buffer->end) {
                return;
                }
            prim->clut = face->ft4.clut;
            prim->tpage = face->ft4.tpage;
            *(long *)&prim->x0 = TMD_XY(va);
            *(long *)&prim->x1 = TMD_XY(vb);
            *(long *)&prim->x2 = TMD_XY(vc);
            *(long *)&prim->x3 = TMD_XY(vd);
            *(u16 *)&prim->u0 = face->ft4.uv0;
            *(u16 *)&prim->u1 = face->ft4.uv1;
            *(u16 *)&prim->u2 = face->ft4.uv2;
            *(u16 *)&prim->u3 = face->ft4.uv3;
            NormalColorDpq(TMD_VECTOR(normals, face->ft4.normal),
                           &map_textured_primitive_color,
                           (va->depth_cue + vb->depth_cue + vc->depth_cue + vd->depth_cue) >> 2,
                           (CVECTOR *)&prim->r0);
            setlen(prim, 9);
            prim->code = mode;
            average = (va->sz + vb->sz + vc->sz + vd->sz) >> 2;
            if (average <= 0) {
                break;
            }
            depth = average + depth_bias;
            if ((u32)depth < KF_MAP_OT_DEPTH_LIMIT) {
                AddPrim((void *)&game_graphics_runtime.display_state.ordering_table[depth], (void *)prim);
            }
            break;
        }
        }
        packet += TMD_PACKET_BODY_BYTES(header.word);
    }
}

ADDRESS(0x8002f918, 0x5b4)
void render_enqueue_tmd_fixed_depth(u16 object_index, s32 blend_mode, s32 fixed_depth)
{
    KfTmdObject *object;
    u8 *normals;
    u8 *packet;
    u32 remaining;

    object = tmd_get_object(object_index);
    blend_mode <<= 5;
    packet = TMD_SECTION(game_graphics_runtime.tmd_state.current_asset, object->primitive_offset);
    normals = TMD_SECTION(game_graphics_runtime.tmd_state.current_asset, object->normal_offset);
    remaining = object->primitive_count;
    while (remaining-- != 0) {
        u8 *vertices = (u8 *)game_graphics_runtime.tmd_projected_vertices;
        KfTmdPacketHeader header;
        u8 mode;
        KfTmdPrimitive *face;
        KfScreenVertex *va;
        KfScreenVertex *vb;
        KfScreenVertex *vc;
        KfScreenVertex *vd;
        s32 depth_index;

        header.word = *(u32 *)packet;
        mode = header.word >> 24;
        packet += KF_TMD_PACKET_HEADER_BYTES;
        face = (KfTmdPrimitive *)packet;
        switch (mode & KF_TMD_MODE_MASK) {
        case KF_TMD_MODE_GT3: {
            KfGpuGT3 *prim;

            va = TMD_VERTEX(vertices, face->gt3.vertex0);
            vb = TMD_VERTEX(vertices, face->gt3.vertex1);
            vc = TMD_VERTEX(vertices, face->gt3.vertex2);
            if (NormalClip(TMD_XY(va), TMD_XY(vb), TMD_XY(vc)) <= 0) {
                break;
            }
            prim = (KfGpuGT3 *)game_graphics_runtime.display_state.primitive_buffer->cursor;
            game_graphics_runtime.display_state.primitive_buffer->cursor += sizeof(POLY_GT3);
            if (game_graphics_runtime.display_state.primitive_buffer->cursor >
                game_graphics_runtime.display_state.primitive_buffer->end) {
                return;
                }
            prim->packed.clut = face->gt3.clut;
            prim->packed.tpage = (face->gt3.tpage & KF_GPU_TPAGE_WITHOUT_ABR) | blend_mode;
            prim->packed.xy0 = TMD_XY(va);
            prim->packed.xy1 = TMD_XY(vb);
            prim->packed.xy2 = TMD_XY(vc);
            prim->packed.uv0 = face->gt3.uv0;
            prim->packed.uv1 = face->gt3.uv1;
            prim->packed.uv2 = face->gt3.uv2;
            NormalColorCol3(TMD_VECTOR(normals, face->gt3.normal0),
                            TMD_VECTOR(normals, face->gt3.normal1),
                            TMD_VECTOR(normals, face->gt3.normal2),
                            &map_textured_primitive_color,
                            &prim->packed.color0, &prim->packed.color1,
                            &prim->packed.color2);
            setlen(&prim->sdk, 9);
            prim->sdk.code = (mode & KF_TMD_MODE_SEMI_TRANSPARENT) | KF_TMD_MODE_GT3;
            depth_index = (s16)fixed_depth;
            if (depth_index > 0 && (u32)depth_index < KF_MAP_OT_DEPTH_LIMIT) {
                AddPrim((void *)&game_graphics_runtime.display_state.ordering_table[depth_index],
                        (void *)&prim->sdk);
            }
            break;
        }
        case KF_TMD_MODE_GT4: {
            KfGpuGT4 *prim;

            va = TMD_VERTEX(vertices, face->gt4.vertex0);
            vb = TMD_VERTEX(vertices, face->gt4.vertex1);
            vc = TMD_VERTEX(vertices, face->gt4.vertex2);
            if (NormalClip(TMD_XY(va), TMD_XY(vb), TMD_XY(vc)) <= 0) {
                break;
            }
            vd = TMD_VERTEX(vertices, face->gt4.vertex3);
            prim = (KfGpuGT4 *)game_graphics_runtime.display_state.primitive_buffer->cursor;
            game_graphics_runtime.display_state.primitive_buffer->cursor += sizeof(POLY_GT4);
            if (game_graphics_runtime.display_state.primitive_buffer->cursor >
                game_graphics_runtime.display_state.primitive_buffer->end) {
                return;
                }
            prim->packed.clut = face->gt4.clut;
            prim->packed.tpage = (face->gt4.tpage & KF_GPU_TPAGE_WITHOUT_ABR) | blend_mode;
            prim->packed.xy0 = TMD_XY(va);
            prim->packed.xy1 = TMD_XY(vb);
            prim->packed.xy2 = TMD_XY(vc);
            prim->packed.xy3 = TMD_XY(vd);
            prim->packed.uv0 = face->gt4.uv0;
            prim->packed.uv1 = face->gt4.uv1;
            prim->packed.uv2 = face->gt4.uv2;
            prim->packed.uv3 = face->gt4.uv3;
            NormalColorCol3(TMD_VECTOR(normals, face->gt4.normal0),
                            TMD_VECTOR(normals, face->gt4.normal1),
                            TMD_VECTOR(normals, face->gt4.normal2),
                            &map_textured_primitive_color,
                            &prim->packed.color0, &prim->packed.color1,
                            &prim->packed.color2);
            NormalColorCol(TMD_VECTOR(normals, face->gt4.normal3),
                           &map_textured_primitive_color,
                           &prim->packed.color3);
            setlen(&prim->sdk, 12);
            prim->sdk.code = (mode & KF_TMD_MODE_SEMI_TRANSPARENT) | KF_TMD_MODE_GT4;
            depth_index = (s16)fixed_depth;
            if (depth_index > 0 && (u32)depth_index < KF_MAP_OT_DEPTH_LIMIT) {
                AddPrim((void *)&game_graphics_runtime.display_state.ordering_table[depth_index],
                        (void *)&prim->sdk);
            }
            break;
        }
        case KF_TMD_MODE_G3: {
            POLY_G3 *prim;

            va = TMD_VERTEX(vertices, face->g3.vertex0);
            vb = TMD_VERTEX(vertices, face->g3.vertex1);
            vc = TMD_VERTEX(vertices, face->g3.vertex2);
            if (NormalClip(TMD_XY(va), TMD_XY(vb), TMD_XY(vc)) <= 0) {
                break;
            }
            prim = (POLY_G3 *)game_graphics_runtime.display_state.primitive_buffer->cursor;
            game_graphics_runtime.display_state.primitive_buffer->cursor += sizeof(POLY_G3);
            if (game_graphics_runtime.display_state.primitive_buffer->cursor >
                game_graphics_runtime.display_state.primitive_buffer->end) {
                return;
                }
            *(long *)&prim->x0 = TMD_XY(va);
            *(long *)&prim->x1 = TMD_XY(vb);
            *(long *)&prim->x2 = TMD_XY(vc);
            NormalColorCol3(TMD_VECTOR(normals, face->g3.normal0),
                            TMD_VECTOR(normals, face->g3.normal1),
                            TMD_VECTOR(normals, face->g3.normal2),
                            &face->g3.color,
                            (CVECTOR *)&prim->r0, (CVECTOR *)&prim->r1,
                            (CVECTOR *)&prim->r2);
            setlen(prim, 6);
            prim->code = KF_TMD_MODE_G3;
            depth_index = (s16)fixed_depth;
            if (depth_index > 0 && (u32)depth_index < KF_MAP_OT_DEPTH_LIMIT) {
                AddPrim((void *)&game_graphics_runtime.display_state.ordering_table[depth_index],
                        (void *)prim);
            }
            break;
        }
        case KF_TMD_MODE_G4: {
            POLY_G4 *prim;

            va = TMD_VERTEX(vertices, face->g4.vertex0);
            vb = TMD_VERTEX(vertices, face->g4.vertex1);
            vc = TMD_VERTEX(vertices, face->g4.vertex2);
            if (NormalClip(TMD_XY(va), TMD_XY(vb), TMD_XY(vc)) <= 0) {
                break;
            }
            vd = TMD_VERTEX(vertices, face->g4.vertex3);
            prim = (POLY_G4 *)game_graphics_runtime.display_state.primitive_buffer->cursor;
            game_graphics_runtime.display_state.primitive_buffer->cursor += sizeof(POLY_G4);
            if (game_graphics_runtime.display_state.primitive_buffer->cursor >
                game_graphics_runtime.display_state.primitive_buffer->end) {
                return;
                }
            *(long *)&prim->x0 = TMD_XY(va);
            *(long *)&prim->x1 = TMD_XY(vb);
            *(long *)&prim->x2 = TMD_XY(vc);
            *(long *)&prim->x3 = TMD_XY(vd);
            NormalColorCol3(TMD_VECTOR(normals, face->g4.normal0),
                            TMD_VECTOR(normals, face->g4.normal1),
                            TMD_VECTOR(normals, face->g4.normal2),
                            &face->g4.color,
                            (CVECTOR *)&prim->r0, (CVECTOR *)&prim->r1,
                            (CVECTOR *)&prim->r2);
            NormalColorCol(TMD_VECTOR(normals, face->g4.normal3),
                           &face->g4.color,
                           (CVECTOR *)&prim->r3);
            setlen(prim, 8);
            prim->code = KF_TMD_MODE_G4;
            depth_index = (s16)fixed_depth;
            if (depth_index > 0 && (u32)depth_index < KF_MAP_OT_DEPTH_LIMIT) {
                AddPrim((void *)&game_graphics_runtime.display_state.ordering_table[depth_index],
                        (void *)prim);
            }
            break;
        }
        }
        packet += TMD_PACKET_BODY_BYTES(header.word);
    }
}

ADDRESS(0x8002fecc, 0x41c)
void render_enqueue_map(u16 object_index)
{
    KfTmdObject *object;
    KfTmdPacketHeader header;
    u8 *normals;
    u8 *packet;
    u32 remaining;
    CVECTOR shade;
    KfScreenVertex *va;
    KfScreenVertex *vb;
    KfScreenVertex *vc;
    KfScreenVertex *vd;

    object = tmd_get_object(object_index);
    normals = TMD_SECTION(game_graphics_runtime.tmd_state.current_asset, object->normal_offset);
    tmd_project_vertices_with_fog(object->vertex_count);
    packet = TMD_SECTION(game_graphics_runtime.tmd_state.current_asset, object->primitive_offset);
    remaining = object->primitive_count;
    while (remaining-- != 0) {
        u8 *vertices = (u8 *)game_graphics_runtime.tmd_projected_vertices;

        header.word = *(u32 *)packet;
        packet += KF_TMD_PACKET_HEADER_BYTES;
        switch (header.bytes.mode & KF_TMD_MODE_MASK) {
        case KF_TMD_MODE_FT4: {
            KfTmdFt4 *face = (KfTmdFt4 *)packet;
            KfGpuGT4 *prim;
            s32 depth;

            va = TMD_VERTEX(vertices, face->vertex0);
            vb = TMD_VERTEX(vertices, face->vertex1);
            vc = TMD_VERTEX(vertices, face->vertex2);
            if (NormalClip(TMD_XY(va), TMD_XY(vb), TMD_XY(vc)) <= 0) {
                break;
            }
            prim = (KfGpuGT4 *)game_graphics_runtime.display_state.primitive_buffer->cursor;
            vd = TMD_VERTEX(vertices, face->vertex3);
            game_graphics_runtime.display_state.primitive_buffer->cursor += sizeof(POLY_GT4);
            if (game_graphics_runtime.display_state.primitive_buffer->cursor >
                game_graphics_runtime.display_state.primitive_buffer->end) {
                return;
            }
            prim->packed.clut = face->clut;
            prim->packed.tpage = face->tpage;
            prim->packed.xy0 = TMD_XY(va);
            prim->packed.xy1 = TMD_XY(vb);
            prim->packed.xy2 = TMD_XY(vc);
            prim->packed.xy3 = TMD_XY(vd);
            prim->packed.uv0 = face->uv0;
            prim->packed.uv1 = face->uv1;
            prim->packed.uv2 = face->uv2;
            prim->packed.uv3 = face->uv3;
            NormalColorCol(TMD_VECTOR(normals, face->normal),
                           &map_textured_primitive_color, &shade);
            DpqColor(&shade, va->depth_cue, &prim->packed.color0);
            DpqColor(&shade, vb->depth_cue, &prim->packed.color1);
            DpqColor(&shade, vc->depth_cue, &prim->packed.color2);
            DpqColor(&shade, vd->depth_cue, &prim->packed.color3);
            setlen(&prim->sdk, 0x0c);
            prim->sdk.code = header.bytes.mode | KF_TMD_MODE_GT4;
            depth = ((va->sz + vb->sz + vc->sz + vd->sz) >> 2) +
                KF_MAP_OT_DEPTH_BIAS;
            if ((u32)depth < KF_MAP_OT_DEPTH_LIMIT) {
                AddPrim((void *)&game_graphics_runtime.display_state.ordering_table[depth],
                        (void *)&prim->sdk);
            }
            break;
        }
        case KF_TMD_MODE_FT3: {
            KfTmdFt3 *face = (KfTmdFt3 *)packet;
            KfGpuGT3 *prim;
            s32 depth;

            va = TMD_VERTEX(vertices, face->vertex0);
            vb = TMD_VERTEX(vertices, face->vertex1);
            vc = TMD_VERTEX(vertices, face->vertex2);
            if (NormalClip(TMD_XY(va), TMD_XY(vb), TMD_XY(vc)) <= 0) {
                break;
            }
            prim = (KfGpuGT3 *)game_graphics_runtime.display_state.primitive_buffer->cursor;
            game_graphics_runtime.display_state.primitive_buffer->cursor += sizeof(POLY_GT3);
            if (game_graphics_runtime.display_state.primitive_buffer->cursor >
                game_graphics_runtime.display_state.primitive_buffer->end) {
                return;
            }
            prim->packed.clut = face->clut;
            prim->packed.tpage = face->tpage;
            prim->packed.xy0 = TMD_XY(va);
            prim->packed.xy1 = TMD_XY(vb);
            prim->packed.xy2 = TMD_XY(vc);
            prim->packed.uv0 = face->uv0;
            prim->packed.uv1 = face->uv1;
            prim->packed.uv2 = face->uv2;
            NormalColorCol(TMD_VECTOR(normals, face->normal),
                           &map_textured_primitive_color, &shade);
            DpqColor(&shade, va->depth_cue, &prim->packed.color0);
            DpqColor(&shade, vb->depth_cue, &prim->packed.color1);
            DpqColor(&shade, vc->depth_cue, &prim->packed.color2);
            setlen(&prim->sdk, 0x09);
            prim->sdk.code = header.bytes.mode | KF_TMD_MODE_GT3;
            depth = (va->sz + vb->sz + vc->sz) / 3 + KF_MAP_OT_DEPTH_BIAS;
            if ((u32)depth < KF_MAP_OT_DEPTH_LIMIT) {
                AddPrim((void *)&game_graphics_runtime.display_state.ordering_table[depth],
                        (void *)&prim->sdk);
            }
            break;
        }
        }
        packet += header.bytes.input_length * KF_TMD_WORD_BYTES;
    }
}

/* The clipping SDK returns EVECTOR pointers; NormalClip takes packed screen XY. */
#define CLIPPED_MAP_VERTEX(index) \
    (game_graphics_runtime.clip_result_vertices[index])
#define CLIPPED_MAP_XY(vertex) (*(long *)&(vertex)->sxy)

ADDRESS(0x800302e8, 0x258)
void render_enqueue_clipped_tmd_polygon(s32 vertex_count, SVECTOR *normal, u16 clut, u16 tpage,
                   u32 mode, s32 depth_bias)
{
    EVECTOR *first;
    EVECTOR *second;
    EVECTOR *third;
    CVECTOR shade;
    CVECTOR first_color;
    u32 packet_code;
    EVECTOR **next;

    if (NormalClip(CLIPPED_MAP_XY(CLIPPED_MAP_VERTEX(0)),
                   CLIPPED_MAP_XY(CLIPPED_MAP_VERTEX(1)),
                   CLIPPED_MAP_XY(CLIPPED_MAP_VERTEX(2))) <= 0) {
        return;
    }
    NormalColorCol(normal, &map_textured_primitive_color, &shade);
    next = game_graphics_runtime.clip_result_vertices;
    first = *next++;
    packet_code = mode | KF_TMD_MODE_GT3;
    DpqColor(&shade, first->sxyz.pad >> 1, &first_color);
    second = *next++;
    DpqColor(&shade, second->sxyz.pad >> 1, &second->rgb);

    vertex_count -= 2;
    while (vertex_count-- > 0) {
        KfGpuGT3 *packet;
        s32 depth;
        s32 summed_depth;

        third = *next++;
        DpqColor(&shade, third->sxyz.pad >> 1, &third->rgb);
        packet = (KfGpuGT3 *)game_graphics_runtime.display_state.primitive_buffer->cursor;
        game_graphics_runtime.display_state.primitive_buffer->cursor += sizeof(POLY_GT3);
        if (game_graphics_runtime.display_state.primitive_buffer->cursor >
            game_graphics_runtime.display_state.primitive_buffer->end) {
            return;
        }
        packet->packed.clut = clut;
        packet->packed.tpage = tpage;
        packet->packed.xy0 = CLIPPED_MAP_XY(first);
        packet->packed.xy1 = CLIPPED_MAP_XY(second);
        packet->packed.xy2 = CLIPPED_MAP_XY(third);
        *(u16 *)&packet->sdk.u0 = first->txuv;
        *(u16 *)&packet->sdk.u1 = second->txuv;
        *(u16 *)&packet->sdk.u2 = third->txuv;
        *(u32 *)&packet->packed.color0 = *(u32 *)&first_color;
        *(u32 *)&packet->packed.color1 = *(u32 *)&second->rgb;
        *(u32 *)&packet->packed.color2 = *(u32 *)&third->rgb;
        setlen(&packet->sdk, 9);
        packet->sdk.code = packet_code;
        summed_depth = first->sxyz.vz + second->sxyz.vz + third->sxyz.vz;
        depth = summed_depth / 12 + depth_bias;
        if (depth < 16) {
            depth = 16;
        }
        AddPrim((void *)&game_graphics_runtime.display_state.ordering_table[depth & 0x1fff],
                (void *)&packet->sdk);

        second = third;
    }
}

#define MAP_OUTSIDE_Y(delta) ((u32)(delta) + 511u >= 1023u)
#define MAP_OUTSIDE_X(delta) ((u32)(delta) + 1023u >= 2047u)

/* Faces are read through the packet cursor itself; retail keeps no copy. */
#define FT4_FACE ((KfTmdFt4 *)packet)
#define FT3_FACE ((KfTmdFt3 *)packet)

ADDRESS(0x80030540, 0x754)
void render_enqueue_tmd_with_clipping(u16 object_index, s32 depth_bias,
                   KfTmdPreparedAsset *prepared_asset)
{
    KfTmdPacketHeader header;
    KfTmdObject *object;
    u8 *normals;
    u8 *packet;
    u8 *vertices;
    SVECTOR *original_vertices;
    u32 remaining;
    /* Unreferenced: the clipper writes clip_result_vertices instead, but this
     * array still occupies the retail frame below shade. */
    EVECTOR *clip_vertices[10];
    CVECTOR shade;
    KfScreenVertex *va;
    KfScreenVertex *vb;
    KfScreenVertex *vc;
    KfScreenVertex *vd;
    s32 dy0, dy1, dy2, dy3, dy4;
    s32 dx0, dx1, dx2, dx3, dx4;

    if (prepared_asset != NULL) {
        object = &prepared_asset->object;
        normals = TMD_SECTION(prepared_asset, object->normal_offset);
        game_graphics_runtime.current_tmd_vertices =
            (SVECTOR *)TMD_SECTION(prepared_asset, object->vertex_offset);
    } else {
        object = tmd_get_object(object_index);
        normals = TMD_SECTION(game_graphics_runtime.tmd_state.current_asset, object->normal_offset);
    }
    original_vertices = game_graphics_runtime.current_tmd_vertices;
    tmd_project_vertices_mark_clipped(object->vertex_count);
    if (prepared_asset != NULL) {
        packet = TMD_SECTION(prepared_asset, object->primitive_offset);
    } else {
        packet = TMD_SECTION(game_graphics_runtime.tmd_state.current_asset,
                             object->primitive_offset);
    }
    remaining = object->primitive_count;
    if (remaining-- != 0) {
        do {
            vertices = (u8 *)game_graphics_runtime.tmd_projected_vertices;
            header.word = *(u32 *)packet;
            packet += KF_TMD_PACKET_HEADER_BYTES;
            switch (header.bytes.mode & KF_TMD_MODE_MASK) {
            case KF_TMD_MODE_FT4: {
                s32 clipped_count;
                s32 depth;
                KfGpuGT4 *prim;

                va = TMD_VERTEX(vertices, FT4_FACE->vertex0);
                vb = TMD_VERTEX(vertices, FT4_FACE->vertex1);
                vc = TMD_VERTEX(vertices, FT4_FACE->vertex2);
                vd = TMD_VERTEX(vertices, FT4_FACE->vertex3);
                dy0 = va->y - vb->y;
                dy1 = vb->y - vd->y;
                dy2 = vd->y - vc->y;
                dy3 = vc->y - va->y;
                dy4 = vb->y - vc->y;
                dx0 = va->x - vb->x;
                dx1 = vb->x - vd->x;
                dx2 = vd->x - vc->x;
                dx3 = vc->x - va->x;
                dx4 = vb->x - vc->x;
                if (!((s16)(va->sz | vb->sz | vc->sz | vd->sz) == -1 ||
                    MAP_OUTSIDE_Y(dy0) || MAP_OUTSIDE_Y(dy1) ||
                    MAP_OUTSIDE_Y(dy2) || MAP_OUTSIDE_Y(dy3) ||
                    MAP_OUTSIDE_Y(dy4) || MAP_OUTSIDE_X(dx0) ||
                    MAP_OUTSIDE_X(dx1) || MAP_OUTSIDE_X(dx2) ||
                    MAP_OUTSIDE_X(dx3) || MAP_OUTSIDE_X(dx4))) {
                    if (NormalClip(TMD_XY(va), TMD_XY(vb), TMD_XY(vc)) <= 0) {
                        break;
                    }
                    prim = (KfGpuGT4 *)game_graphics_runtime.display_state.primitive_buffer->cursor;
                    game_graphics_runtime.display_state.primitive_buffer->cursor += sizeof(POLY_GT4);
                    if (game_graphics_runtime.display_state.primitive_buffer->cursor >
                        game_graphics_runtime.display_state.primitive_buffer->end) {
                        return;
                    }
                    prim->packed.clut = FT4_FACE->clut;
                    prim->packed.tpage = FT4_FACE->tpage;
                    prim->packed.xy0 = TMD_XY(va);
                    prim->packed.xy1 = TMD_XY(vb);
                    prim->packed.xy2 = TMD_XY(vc);
                    prim->packed.xy3 = TMD_XY(vd);
                    prim->packed.uv0 = FT4_FACE->uv0;
                    prim->packed.uv1 = FT4_FACE->uv1;
                    prim->packed.uv2 = FT4_FACE->uv2;
                    prim->packed.uv3 = FT4_FACE->uv3;
                    NormalColorCol(TMD_VECTOR(normals, FT4_FACE->normal),
                                   &map_textured_primitive_color, &shade);
                    DpqColor(&shade, va->depth_cue, &prim->packed.color0);
                    DpqColor(&shade, vb->depth_cue, &prim->packed.color1);
                    DpqColor(&shade, vc->depth_cue, &prim->packed.color2);
                    DpqColor(&shade, vd->depth_cue, &prim->packed.color3);
                    setlen(&prim->sdk, 12);
                    prim->sdk.code = (header.bytes.mode & KF_TMD_MODE_SEMI_TRANSPARENT) | KF_TMD_MODE_GT4;
                    depth = ((va->sz + vb->sz + vc->sz + vd->sz) >> 2) + depth_bias;
                    if (depth < 16) {
                        depth = 16;
                    }
                    AddPrim((void *)&game_graphics_runtime.display_state.ordering_table[depth & 0x1fff],
                            (void *)&prim->sdk);
                } else {
                    clipped_count = Clip4FTP(TMD_VECTOR(original_vertices, FT4_FACE->vertex0),
                                             TMD_VECTOR(original_vertices, FT4_FACE->vertex1),
                                             TMD_VECTOR(original_vertices, FT4_FACE->vertex2),
                                             TMD_VECTOR(original_vertices, FT4_FACE->vertex3),
                                             (short *)&FT4_FACE->uv0,
                                             (short *)&FT4_FACE->uv1,
                                             (short *)&FT4_FACE->uv2,
                                             (short *)&FT4_FACE->uv3,
                                             game_graphics_runtime.clip_result_vertices);
                    if (clipped_count >= 3) {
                        render_enqueue_clipped_tmd_polygon(clipped_count,
                                       TMD_VECTOR(normals, FT4_FACE->normal),
                                       FT4_FACE->clut, FT4_FACE->tpage,
                                       header.bytes.mode & KF_TMD_MODE_SEMI_TRANSPARENT, depth_bias);
                    }
                }
                break;
            }
            case KF_TMD_MODE_FT3: {
                s32 clipped_count;
                s32 depth;
                KfGpuGT3 *prim;

                va = TMD_VERTEX(vertices, FT3_FACE->vertex0);
                vb = TMD_VERTEX(vertices, FT3_FACE->vertex1);
                vc = TMD_VERTEX(vertices, FT3_FACE->vertex2);
                dy0 = va->y - vb->y;
                dy1 = vb->y - vc->y;
                dy2 = vc->y - va->y;
                dx0 = va->x - vb->x;
                dx1 = vb->x - vc->x;
                dx2 = vc->x - va->x;
                if (!((s16)(va->sz | vb->sz | vc->sz) == -1 ||
                    MAP_OUTSIDE_Y(dy0) || MAP_OUTSIDE_Y(dy1) ||
                    MAP_OUTSIDE_Y(dy2) || MAP_OUTSIDE_X(dx0) ||
                    MAP_OUTSIDE_X(dx1) || MAP_OUTSIDE_X(dx2))) {
                    if (NormalClip(TMD_XY(va), TMD_XY(vb), TMD_XY(vc)) <= 0) {
                        break;
                    }
                    prim = (KfGpuGT3 *)game_graphics_runtime.display_state.primitive_buffer->cursor;
                    game_graphics_runtime.display_state.primitive_buffer->cursor += sizeof(POLY_GT3);
                    if (game_graphics_runtime.display_state.primitive_buffer->cursor >
                        game_graphics_runtime.display_state.primitive_buffer->end) {
                        return;
                    }
                    prim->packed.clut = FT3_FACE->clut;
                    prim->packed.tpage = FT3_FACE->tpage;
                    prim->packed.xy0 = TMD_XY(va);
                    prim->packed.xy1 = TMD_XY(vb);
                    prim->packed.xy2 = TMD_XY(vc);
                    prim->packed.uv0 = FT3_FACE->uv0;
                    prim->packed.uv1 = FT3_FACE->uv1;
                    prim->packed.uv2 = FT3_FACE->uv2;
                    NormalColorCol(TMD_VECTOR(normals, FT3_FACE->normal),
                                   &map_textured_primitive_color, &shade);
                    DpqColor(&shade, va->depth_cue, &prim->packed.color0);
                    DpqColor(&shade, vb->depth_cue, &prim->packed.color1);
                    DpqColor(&shade, vc->depth_cue, &prim->packed.color2);
                    setlen(&prim->sdk, 9);
                    prim->sdk.code = (header.bytes.mode & KF_TMD_MODE_SEMI_TRANSPARENT) | KF_TMD_MODE_GT3;
                    depth = (va->sz + vb->sz + vc->sz) / 3 + depth_bias;
                    if (depth < 16) {
                        depth = 16;
                    }
                    AddPrim((void *)&game_graphics_runtime.display_state.ordering_table[depth & 0x1fff],
                            (void *)&prim->sdk);
                } else {
                    clipped_count = Clip3FTP(TMD_VECTOR(original_vertices, FT3_FACE->vertex0),
                                             TMD_VECTOR(original_vertices, FT3_FACE->vertex1),
                                             TMD_VECTOR(original_vertices, FT3_FACE->vertex2),
                                             (short *)&FT3_FACE->uv0,
                                             (short *)&FT3_FACE->uv1,
                                             (short *)&FT3_FACE->uv2,
                                             game_graphics_runtime.clip_result_vertices);
                    if (clipped_count >= 3) {
                        render_enqueue_clipped_tmd_polygon(clipped_count,
                                       TMD_VECTOR(normals, FT3_FACE->normal),
                                       FT3_FACE->clut, FT3_FACE->tpage,
                                       header.bytes.mode & KF_TMD_MODE_SEMI_TRANSPARENT, depth_bias);
                    }
                }
                break;
            }
            }
            packet += header.bytes.input_length * KF_TMD_WORD_BYTES;
        } while (remaining-- != 0);
    }
}

#undef FT4_FACE
#undef FT3_FACE

/* The packet word is read bytewise and as signed halves through its address. */
#define WORD_BYTE(n) (((u8 *)&word)[n])
#define WORD_HALF(n) (((s16 *)&word)[n])

#define SUBDIVIDE_CORNER(corner, half) do { \
    SVECTOR *vertex = (SVECTOR *)(base + source->vertex_offset + WORD_HALF(half)); \
    *(u32 *)&(corner).vx = *(u32 *)&vertex->vx; \
    *(u32 *)&(corner).vz = *(u32 *)&vertex->vz; \
} while (0)
/* The last corner forms its vertex address in its own contour and copies the
 * vertex after it; the earlier corners copy inside theirs. */
#define SUBDIVIDE_LAST_CORNER(corner, half) { \
    SVECTOR *vertex; \
    do { \
        vertex = (SVECTOR *)(base + source->vertex_offset + WORD_HALF(half)); \
    } while (0); \
    *(u32 *)&(corner).vx = *(u32 *)&vertex->vx; \
    *(u32 *)&(corner).vz = *(u32 *)&vertex->vz; \
}
#define SUBDIVIDE_MIDPOINT(lhs, rhs) { \
    midpoint_end->vx = ((lhs).vx + (rhs).vx) >> 1; \
    midpoint_end->vy = ((lhs).vy + (rhs).vy) >> 1; \
    midpoint_end->vz = ((lhs).vz + (rhs).vz) >> 1; \
    midpoint_end++; \
}
#define SUBDIVIDE_UV(dst, lhs, rhs) { \
    (dst).parts.uv.v = ((lhs).parts.uv.v + (rhs).parts.uv.v) >> 1; \
    (dst).parts.uv.u = ((lhs).parts.uv.u + (rhs).parts.uv.u) >> 1; \
}
#define SUBDIVIDE_INDEX(n) ((count + (n)) << 3)
#define SUBDIVIDE_WRITE_UV(offset, value) { \
    out[offset] = (value).parts.uv.u; \
    out[(offset) + 1] = (value).parts.uv.v; \
}
#define SUBDIVIDE_WRITE_INDEX(offset, value) { \
    WORD_HALF(((offset) >> 1) & 1) = (value); \
    out[offset] = WORD_BYTE((offset) & 3); \
    out[(offset) + 1] = WORD_BYTE(((offset) & 3) + 1); \
}
#define SUBDIVIDE_WRITE_INDEX_WORD(offset, lo, hi) { \
    WORD_HALF(0) = (lo); \
    WORD_HALF(1) = (hi); \
    out[offset] = WORD_BYTE(0); \
    out[(offset) + 1] = WORD_BYTE(1); \
    out[(offset) + 2] = WORD_BYTE(2); \
    out[(offset) + 3] = WORD_BYTE(3); \
}
#define SUBDIVIDE_NEXT_PACKET() do { \
    word = *(u32 *)packet; \
    packet += (WORD_BYTE(1) + 1) * KF_TMD_WORD_BYTES; \
} while (0)

ADDRESS(0x80030c94, 0xcbc)
void tmd_prepare_subdivided_object(KfTmdHeader *asset, s32 object_index, u8 *out)
{
    SVECTOR corners[4];
    KfTmdFt4TextureWords tex;
    SVECTOR midpoints[128];
    u32 word;
    u8 *base;
    u32 midpoint_count;
    KfTmdObject *source;
    KfTmdObject *target;
    u32 output_bytes;
    u32 remaining;
    KfTmdUvWord uv0, uv1, uv2, uv3, uv4;
    u8 *packet;
    SVECTOR *midpoint_end;
    u32 count;
    u32 source_vertex_count;

    midpoint_count = 0;
    output_bytes = 0;
    midpoint_end = midpoints;
    out += KF_TMD_HEADER_BYTES;
    target = (KfTmdObject *)out;
    base = (u8 *)asset + KF_TMD_HEADER_BYTES;
    source = (KfTmdObject *)base + object_index;
    out += sizeof(KfTmdObject);
    target->primitive_offset = sizeof(KfTmdObject);
    target->primitive_count = source->primitive_count;
    packet = base + source->primitive_offset;
    remaining = source->primitive_count;
    while (--remaining != (u32)-1) {
        /* Retail stores the header word twice before testing its mode. */
        word = *(u32 *)packet;
        word = *(u32 *)packet;
        if (WORD_BYTE(3) == 0x2c || WORD_BYTE(3) == 0x2e) {
            resource_copy_words((u32 *)out, (u32 *)packet, 8);
            word = *(u32 *)(packet + 20);
            SUBDIVIDE_CORNER(corners[0], 1);
            word = *(u32 *)(packet + 24);
            SUBDIVIDE_CORNER(corners[1], 0);
            SUBDIVIDE_CORNER(corners[2], 1);
            word = *(u32 *)(packet + 28);
            SUBDIVIDE_LAST_CORNER(corners[3], 0);
            SUBDIVIDE_MIDPOINT(corners[0], corners[1]);
            SUBDIVIDE_MIDPOINT(corners[0], corners[2]);
            SUBDIVIDE_MIDPOINT(corners[0], corners[3]);
            SUBDIVIDE_MIDPOINT(corners[2], corners[3]);
            SUBDIVIDE_MIDPOINT(corners[1], corners[3]);
            resource_copy_words((u32 *)&tex, (u32 *)(packet + 4), 4);
            SUBDIVIDE_UV(uv0, tex.uv0, tex.uv1);
            SUBDIVIDE_UV(uv1, tex.uv0, tex.uv2);
            SUBDIVIDE_UV(uv2, tex.uv0, tex.uv3);
            SUBDIVIDE_UV(uv3, tex.uv2, tex.uv3);
            SUBDIVIDE_UV(uv4, tex.uv1, tex.uv3);
            count = midpoint_count + source->vertex_count;
            SUBDIVIDE_WRITE_UV(8, uv0);
            SUBDIVIDE_WRITE_UV(12, uv1);
            SUBDIVIDE_WRITE_UV(16, uv2);
            WORD_HALF(0) = SUBDIVIDE_INDEX(0);
            WORD_HALF(1) = SUBDIVIDE_INDEX(1);
            *(u32 *)(out + 24) = word;
            WORD_HALF(0) = SUBDIVIDE_INDEX(2);
            *(u32 *)(out + 28) = word;
            out += 32;
            resource_copy_words((u32 *)out, (u32 *)packet, 8);
            SUBDIVIDE_WRITE_UV(4, uv0);
            SUBDIVIDE_WRITE_UV(12, uv2);
            SUBDIVIDE_WRITE_UV(16, uv4);
            SUBDIVIDE_WRITE_INDEX(22, SUBDIVIDE_INDEX(0));
            SUBDIVIDE_WRITE_INDEX(26, SUBDIVIDE_INDEX(2));
            SUBDIVIDE_WRITE_INDEX(28, SUBDIVIDE_INDEX(4));
            out += 32;
            resource_copy_words((u32 *)out, (u32 *)packet, 8);
            SUBDIVIDE_WRITE_UV(4, uv1);
            SUBDIVIDE_WRITE_UV(8, uv2);
            SUBDIVIDE_WRITE_UV(16, uv3);
            SUBDIVIDE_WRITE_INDEX(22, SUBDIVIDE_INDEX(1));
            SUBDIVIDE_WRITE_INDEX(24, SUBDIVIDE_INDEX(2));
            SUBDIVIDE_WRITE_INDEX(28, SUBDIVIDE_INDEX(3));
            out += 32;
            resource_copy_words((u32 *)out, (u32 *)packet, 8);
            SUBDIVIDE_WRITE_UV(4, uv2);
            SUBDIVIDE_WRITE_UV(8, uv4);
            SUBDIVIDE_WRITE_UV(12, uv3);
            SUBDIVIDE_WRITE_INDEX(22, SUBDIVIDE_INDEX(2));
            SUBDIVIDE_WRITE_INDEX_WORD(24, SUBDIVIDE_INDEX(4), SUBDIVIDE_INDEX(3));
            out += 32;
            output_bytes += 128;
            midpoint_count += 5;
            target->primitive_count += 3;
        } else if (WORD_BYTE(3) == 0x24 || WORD_BYTE(3) == 0x26) {
            resource_copy_words((u32 *)out, (u32 *)packet, 6);
            word = *(u32 *)(packet + 16);
            SUBDIVIDE_CORNER(corners[0], 1);
            word = *(u32 *)(packet + 20);
            SUBDIVIDE_CORNER(corners[1], 0);
            SUBDIVIDE_LAST_CORNER(corners[2], 1);
            SUBDIVIDE_MIDPOINT(corners[0], corners[1]);
            SUBDIVIDE_MIDPOINT(corners[0], corners[2]);
            SUBDIVIDE_MIDPOINT(corners[1], corners[2]);
            resource_copy_words((u32 *)&tex, (u32 *)(packet + 4), 3);
            SUBDIVIDE_UV(uv0, tex.uv0, tex.uv1);
            SUBDIVIDE_UV(uv1, tex.uv0, tex.uv2);
            SUBDIVIDE_UV(uv2, tex.uv1, tex.uv2);
            count = midpoint_count + source->vertex_count;
            SUBDIVIDE_WRITE_UV(8, uv0);
            SUBDIVIDE_WRITE_UV(12, uv1);
            SUBDIVIDE_WRITE_INDEX_WORD(20, SUBDIVIDE_INDEX(0), SUBDIVIDE_INDEX(1));
            out += 24;
            resource_copy_words((u32 *)out, (u32 *)packet, 6);
            SUBDIVIDE_WRITE_UV(4, uv0);
            SUBDIVIDE_WRITE_UV(12, uv2);
            SUBDIVIDE_WRITE_INDEX(18, SUBDIVIDE_INDEX(0));
            SUBDIVIDE_WRITE_INDEX(22, SUBDIVIDE_INDEX(2));
            out += 24;
            resource_copy_words((u32 *)out, (u32 *)packet, 6);
            SUBDIVIDE_WRITE_UV(4, uv1);
            SUBDIVIDE_WRITE_UV(8, uv2);
            SUBDIVIDE_WRITE_INDEX(18, SUBDIVIDE_INDEX(1));
            SUBDIVIDE_WRITE_INDEX(20, SUBDIVIDE_INDEX(2));
            out += 24;
            resource_copy_words((u32 *)out, (u32 *)packet, 6);
            SUBDIVIDE_WRITE_UV(4, uv0);
            SUBDIVIDE_WRITE_UV(8, uv2);
            SUBDIVIDE_WRITE_UV(12, uv1);
            SUBDIVIDE_WRITE_INDEX(18, SUBDIVIDE_INDEX(0));
            SUBDIVIDE_WRITE_INDEX_WORD(20, SUBDIVIDE_INDEX(2), SUBDIVIDE_INDEX(1));
            out += 24;
            output_bytes += 96;
            midpoint_count += 3;
            target->primitive_count += 3;
        } else {
            count = WORD_BYTE(1) + 1;
            resource_copy_words((u32 *)out, (u32 *)packet, count);
            count <<= 2;
            out += count;
            output_bytes += count;
        }
        SUBDIVIDE_NEXT_PACKET();
    }
    target->vertex_offset = output_bytes + target->primitive_offset;
    target->vertex_count = midpoint_count + source->vertex_count;
    source_vertex_count = source->vertex_count;
    resource_copy_words((u32 *)out, (u32 *)(base + source->vertex_offset),
                        source_vertex_count * 2);
    output_bytes = source_vertex_count * sizeof(SVECTOR);
    out += output_bytes;
    resource_copy_words((u32 *)out, (u32 *)midpoints, midpoint_count * 2);
    output_bytes += midpoint_count * sizeof(SVECTOR);
    target->normal_offset = output_bytes + target->vertex_offset;
    target->normal_count = source->normal_count;
    resource_copy_words((u32 *)(out + midpoint_count * sizeof(SVECTOR)),
                        (u32 *)(base + source->normal_offset), source->normal_count * 2);
}
#undef WORD_BYTE
#undef WORD_HALF
#undef SUBDIVIDE_CORNER
#undef SUBDIVIDE_LAST_CORNER
#undef SUBDIVIDE_MIDPOINT
#undef SUBDIVIDE_UV
#undef SUBDIVIDE_INDEX
#undef SUBDIVIDE_WRITE_UV
#undef SUBDIVIDE_WRITE_INDEX
#undef SUBDIVIDE_WRITE_INDEX_WORD
#undef SUBDIVIDE_NEXT_PACKET

ADDRESS(0x80031950, 0x1cc)
void render_map_cell_object(const KfMapCellShape *shape, SVECTOR *position,
                            KfMapLayerMask flags)
{
    MATRIX cell_matrix;
    long gte_flags;
    KfCollisionRow *lighting;
    u16 object_index;
    KF_ENUM_STORAGE(KfQuarterTurn, s32) orientation;

    orientation = KF_ENUM_DECODE(KfQuarterTurn,
                                 shape->quarter_turns & KF_MAP_CELL_QUARTER_TURN_MASK);
    SetRotMatrix(&game_graphics_runtime.render_state.view_matrix);
    SetTransMatrix(&game_graphics_runtime.render_state.view_matrix);
    RotTrans(position, (VECTOR *)&cell_matrix.t, &gte_flags);
    matrix_rotate_quarter_turns(&game_graphics_runtime.render_state.view_matrix,
                                &cell_matrix, orientation);
    SetRotMatrix(&cell_matrix);
    SetTransMatrix(&cell_matrix);

    lighting = &game_graphics_runtime.collision_rows[
        shape->lighting_index & KF_MAP_CELL_LIGHTING_MASK];
    SetLightMatrix((MATRIX *)&lighting->rotations[KF_ENUM_ENCODE(s32, orientation)]);
    SetColorMatrix((MATRIX *)&lighting->motion);
    fog_set_near(lighting->filter.angle);
    SetBackColor(lighting->filter.kinds.types[0],
                 lighting->filter.kinds.types[1],
                 lighting->filter.kinds.types[2]);

    object_index = shape->object_index;
    if (resource_state.transition_active == KF_TRUE && resource_state.tmd_object_limit_active &&
        object_index >= game_graphics_runtime.tmd_state.current_asset->flags) {
        return;
    }
    tmd_select_object_vertices(object_index);
    if ((flags & KF_MAP_LAYER_NEAR_CLIPPED) != KF_MAP_LAYER_NONE) {
        if ((flags & KF_MAP_LAYER_NEAR_PREPARE) != KF_MAP_LAYER_NONE) {
            if (tmd_get_object(object_index)->primitive_count < KF_MAP_CELL_PREPARED_LIMIT) {
                KfTmdPreparedAsset prepared_asset;

                tmd_prepare_subdivided_object(game_graphics_runtime.tmd_state.current_asset,
                              object_index, (u8 *)&prepared_asset);
                render_enqueue_tmd_with_clipping(object_index, 240, &prepared_asset);
                return;
            }
        }
        render_enqueue_tmd_with_clipping(object_index, 240, NULL);
    } else {
        render_enqueue_map(object_index);
    }
}

enum {
    KF_MAP_GRID_EMPTY_OBJECT = 240,
    KF_MAP_GRID_CELL_LENGTH = 1 << KF_MAP_CELL_POSITION_SHIFT,
    KF_MAP_GRID_CELL_MIDPOINT = 1024,
    KF_MAP_GRID_ELEVATION_LENGTH = 1 << KF_MAP_CELL_ELEVATION_SHIFT
};

ADDRESS(0x80031b1c, 0x178)
void render_map_cell_layers(s32 x, s32 z, KfMapLayerMask flags)
{
    KfMapOccupancyCell *cell = &bss_801c7540.map_cells[z][x];
    s32 object_index = cell->layer[0].object_index;
    SVECTOR position;

    if ((flags & KF_MAP_LAYER_FIRST) != KF_MAP_LAYER_NONE &&
        object_index < KF_MAP_GRID_EMPTY_OBJECT) {
        position.vx = x * KF_MAP_GRID_CELL_LENGTH -
                      game_graphics_runtime.render_state.view_position.vx +
                      KF_MAP_GRID_CELL_MIDPOINT;
        position.vy = -cell->layer[0].elevation * KF_MAP_GRID_ELEVATION_LENGTH -
                      game_graphics_runtime.render_state.view_position.vy;
        position.vz = z * KF_MAP_GRID_CELL_LENGTH -
                      game_graphics_runtime.render_state.view_position.vz +
                      KF_MAP_GRID_CELL_MIDPOINT;
        render_map_cell_object(&cell->layer[0], &position, flags);

        object_index = cell->layer[1].object_index;
        if ((flags & KF_MAP_LAYER_SECOND) != KF_MAP_LAYER_NONE &&
            object_index < KF_MAP_GRID_EMPTY_OBJECT) {
            position.vy = -cell->layer[1].elevation * KF_MAP_GRID_ELEVATION_LENGTH -
                          game_graphics_runtime.render_state.view_position.vy;
            render_map_cell_object(&cell->layer[1], &position, flags);
        }
    } else {
        object_index = cell->layer[1].object_index;
        if ((flags & KF_MAP_LAYER_SECOND) != KF_MAP_LAYER_NONE &&
            object_index < KF_MAP_GRID_EMPTY_OBJECT) {
            position.vx = x * KF_MAP_GRID_CELL_LENGTH -
                          game_graphics_runtime.render_state.view_position.vx +
                          KF_MAP_GRID_CELL_MIDPOINT;
            position.vy = -cell->layer[1].elevation * KF_MAP_GRID_ELEVATION_LENGTH -
                          game_graphics_runtime.render_state.view_position.vy;
            position.vz = z * KF_MAP_GRID_CELL_LENGTH -
                          game_graphics_runtime.render_state.view_position.vz +
                          KF_MAP_GRID_CELL_MIDPOINT;
            render_map_cell_object(&cell->layer[1], &position, flags);
        }
    }
}

ADDRESS(0x80031c94, 0xc8)
void render_map_cell_window(void)
{
    s32 row;
    s32 remaining_rows;
    KfMapLayerMask *mask;
    s32 start_x;
    KfRenderGridState *render = &game_graphics_runtime.render_grid;

    tmd_select(KF_TMD_SLOT_MAP);
    mask = &render->map_cell_layer_masks[0][0];
    remaining_rows = KF_MAP_CELL_GRID_SIDE;
    start_x = render->map_scan_start_x;
    row = render->map_scan_start_z;
    while (remaining_rows != 0) {
        if ((u32)row < KF_MAP_WORLD_GRID_SIDE) {
            s32 x = start_x;
            s32 remaining_columns = KF_MAP_CELL_GRID_SIDE;
            do {
                s32 column = x & 0xff;
                x++;
                if ((u32)column < KF_MAP_WORLD_GRID_SIDE && *mask != KF_MAP_LAYER_NONE) {
                    render_map_cell_layers(column, row, *mask);
                }
                mask++;
                remaining_columns--;
            } while (remaining_columns != 0);
        } else {
            mask += KF_MAP_CELL_GRID_SIDE;
        }
        remaining_rows--;
        row++;
    }
}

ADDRESS(0x80031d5c, 0x18c)
void render_active_model_rows(void)
{
    KfRenderModelRow *entry;
    KfCollisionRow *lighting;
    KfTmdObject *object;
    MATRIX model;
    VECTOR scale;
    MATRIX light_matrix;

    entry = render_model_rows;
    if (entry->state == KF_SPRITE_END) {
        return;
    }
    do {
        if (entry->state == KF_SPRITE_VISIBLE) {
            lighting = &game_graphics_runtime.collision_rows[KF_ENUM_ENCODE(u8, entry->lighting_index)];
            model.t[0] = entry->translation.vx;
            model.t[1] = entry->translation.vy;
            model.t[2] = entry->translation.vz;
            RotMatrix(&entry->rotation, &model);
            fog_set_near(lighting->filter.angle);
            SetBackColor(lighting->filter.kinds.types[0],
                         lighting->filter.kinds.types[1],
                         lighting->filter.kinds.types[2]);
            SetColorMatrix((MATRIX *)&lighting->motion);
            MulMatrix0((MATRIX *)&lighting->rotations[0], &model, &light_matrix);
            SetLightMatrix(&light_matrix);
            copyVector(&scale, &entry->scale);
            ScaleMatrix(&model, &scale);
            SetRotMatrix(&model);
            SetTransMatrix(&model);
            asset_registry_select(entry->asset_id);
            object = tmd_get_object(0);
            if (animation_prepare_asset_vertices(&entry->animation_state, entry->asset_id,
                              KF_ENUM_DECODE(KfAnimationClip, entry->animation_clip), entry->animation_phase,
                              object->vertex_count)) {
                tmd_select_object_vertices(0);
            }
            tmd_transform_vertices(object->vertex_count);
            render_enqueue_textured_tmd(0, 0);
        }
        entry++;
    } while (entry->state != KF_SPRITE_END);
}

ADDRESS(0x80031ee8, 0x144)
void render_textured_quad(s32 x, s32 y, s32 right, s32 bottom,
                   u8 texture_u, u8 texture_v, u8 texture_width,
                   u8 texture_height, u8 semitrans, u16 tpage,
                   u16 clut, u8 red, u8 green, u8 blue, s32 depth)
{
    POLY_FT4 *quad = (POLY_FT4 *)game_graphics_runtime.display_state.primitive_buffer->cursor;

    game_graphics_runtime.display_state.primitive_buffer->cursor += sizeof(POLY_FT4);
    if (game_graphics_runtime.display_state.primitive_buffer->cursor >
        game_graphics_runtime.display_state.primitive_buffer->end) {
        return;
    }

    setPolyFT4(quad);
    if (semitrans != KF_TEXTURED_QUAD_OPAQUE) {
        setSemiTrans(quad, semitrans);
    }
    setRGB0(quad, red, green, blue);
    quad->tpage = tpage;
    quad->clut = clut;
    quad->u0 = texture_u;
    quad->v0 = texture_v;
    quad->u1 = texture_u + texture_width;
    quad->v1 = texture_v;
    quad->u2 = texture_u;
    quad->v2 = texture_v + texture_height;
    quad->u3 = texture_u + texture_width;
    quad->v3 = texture_v + texture_height;
    quad->x0 = quad->x2 = x;
    quad->x1 = quad->x3 = right;
    quad->y0 = quad->y1 = y;
    quad->y2 = quad->y3 = bottom;

    if (depth > 0 && (u32)depth < KF_GAME_ORDERING_TABLE_LENGTH) {
        AddPrim((void *)&game_graphics_runtime.display_state.ordering_table[depth], (void *)quad);
    }
}

ADDRESS(0x8003202c, 0x90)
void render_sliding_panel_primary(void)
{
    s32 y = player_state.collision_lower_clearance;

    if (y < 240) {
        y += 120;
        if (y < 0) {
            y = 0;
        }
        render_textured_quad(0, y, 320, 240, 128, 192, 15, 15, KF_TEXTURED_QUAD_SEMI_TRANSPARENT, 55,
                             0x7bdc, 60, 60, 60, 64);
    }
}

ADDRESS(0x800320bc, 0x90)
void render_sliding_panel_secondary(void)
{
    s32 y = player_state.collision_upper_clearance;

    if (y < 240) {
        y += 120;
        if (y < 0) {
            y = 0;
        }
        render_textured_quad(0, y, 320, 240, 144, 192, 15, 15, KF_TEXTURED_QUAD_SEMI_TRANSPARENT, 55,
                             0x7bdc, 60, 60, 60, 64);
    }
}

ADDRESS(0x8003214c, 0xc0)
void render_color_overlay(void)
{
    if (game_graphics_runtime.color_overlay_control != KF_COLOR_OVERLAY_OFF) {
        render_textured_quad(0, 0, 0x140, 0xf0,
                             0x80, 0xd0, 0xf, 0xf, KF_TEXTURED_QUAD_SEMI_TRANSPARENT,
                             KF_GPU_TPAGE_ABR(game_graphics_runtime.color_overlay_control & KF_COLOR_OVERLAY_BLEND_MASK)
                                 | 0x17,
                             0x7bdc,
                             game_graphics_runtime.color_overlay_rgb[0],
                             game_graphics_runtime.color_overlay_rgb[1],
                             game_graphics_runtime.color_overlay_rgb[2],
                             (game_graphics_runtime.color_overlay_control & KF_COLOR_OVERLAY_FRONT) != KF_COLOR_OVERLAY_NO_FLAGS
                                 ? 1 : 0x40);
    }
}

ADDRESS(0x8003220c, 0x28)
void render_set_color_overlay(KfColorOverlayControl control, u8 red, u8 green, u8 blue)
{
    game_graphics_runtime.color_overlay_control = control;
    game_graphics_runtime.color_overlay_rgb[0] = red;
    game_graphics_runtime.color_overlay_rgb[1] = green;
    game_graphics_runtime.color_overlay_rgb[2] = blue;
}

ADDRESS(0x80032234, 0x138)
void render_accumulated_color_overlay(void)
{
    if (game_graphics_runtime.color_overlay_sample_count != 0) {
        render_textured_quad(0, 0, 0x140, 0xf0,
                      0x80, 0xd0, 0xf, 0xf, KF_TEXTURED_QUAD_SEMI_TRANSPARENT, 0x37, 0x7bdc,
                      *(s16 *)&game_graphics_runtime.color_overlay_red_sum /
                          game_graphics_runtime.color_overlay_sample_count,
                      *(s16 *)&game_graphics_runtime.color_overlay_green_sum /
                          game_graphics_runtime.color_overlay_sample_count,
                      *(s16 *)&game_graphics_runtime.color_overlay_blue_sum /
                          game_graphics_runtime.color_overlay_sample_count,
                      0x40);
    }
}

ADDRESS(0x8003236c, 0x94)
void accumulate_color_overlay(s32 first, s32 second, s32 third, s32 scale)
{
    game_graphics_runtime.color_overlay_sample_count++;
    game_graphics_runtime.color_overlay_red_sum += (first * scale) >> KF_FIXED12_BITS;
    game_graphics_runtime.color_overlay_green_sum += (second * scale) >> KF_FIXED12_BITS;
    game_graphics_runtime.color_overlay_blue_sum += (third * scale) >> KF_FIXED12_BITS;
}

ADDRESS(0x80032400, 0x188)
void render_player_weapon(void)
{
    s32 cell_x;
    s32 cell_z;
    u8 *layer;
    KfCollisionRow *row;
    KfWeaponRecordGame *weapon;
    KfTmdObject *object;
    MATRIX model;

    if (player_state.weapon_attack_phase == -1) {
        return;
    }

    cell_z = player_state.camera_position.vz >> KF_MAP_CELL_POSITION_SHIFT;
    cell_x = player_state.camera_position.vx >> KF_MAP_CELL_POSITION_SHIFT;
    layer = &bss_801c7540.map_cells[0][0].layer[0].lighting_index;
    row = &game_graphics_runtime.collision_rows[
        layer[cell_x * sizeof(KfMapOccupancyCell) +
              cell_z * sizeof(bss_801c7540.map_cells[0]) +
              player_state.map_layer_index] & KF_MAP_CELL_LIGHTING_MASK];
    SetColorMatrix((MATRIX *)&row->motion);
    SetLightMatrix((MATRIX *)&row->rotations[0]);
    fog_set_near(row->filter.angle);
    SetBackColor(row->filter.kinds.types[0], row->filter.kinds.types[1],
                 row->filter.kinds.types[2]);

    weapon = player_state.equipped_weapon_record;
    model.t[0] = (s16)weapon->position_offset_x;
    model.t[1] = (s16)weapon->position_offset_y;
    model.t[2] = (s16)weapon->position_offset_z;
    RotMatrix((SVECTOR *)&weapon->rotation_offset_x, &model);
    SetRotMatrix(&model);
    SetTransMatrix(&model);
    asset_registry_select(0x20);
    object = tmd_get_object(0);
    if (animation_prepare_asset_vertices(&player_state.weapon_animation_cache, 0x20,
                      player_state.weapon_attack_mode,
                      player_state.weapon_attack_phase,
                      object->vertex_count) != 0) {
        tmd_project_vertices_with_fog(object->vertex_count);
        render_enqueue_textured_tmd(0, 100);
    }
}

ADDRESS(0x80032588, 0x53c)
void render_world_model(KfMapLayerMask map_layer, u16 asset_index, const VECTOR *position,
                   const struct KfEulerAngles *rotation, const SVECTOR *scale,
                   KfPoolRecord **cache, MATRIX *world_matrix, KF_ENUM_PARAM(KfAnimationClip, u16) clip,
                   u16 phase, KfLightingIndex lighting_override, s16 lighting_blend,
                   KfRenderQueueMode render_mode, s32 depth)
{
    /* Retail reads vy after a null world_matrix path without initializing it. */
    SVECTOR relative;
    VECTOR scale_vector;
    MATRIX model;
    MATRIX light_matrix;
    MATRIX color_matrix;
    long gte_flags;
    KfCollisionRow *lighting;
    KfCollisionRow *override;
    KfTmdObject *object;
    u16 object_index;
    s32 red;
    s32 green;
    s32 blue;

    SetRotMatrix(&game_graphics_runtime.render_state.view_matrix);
    SetTransMatrix(&game_graphics_runtime.render_state.view_matrix);
    if (world_matrix != NULL) {
        KfMapOccupancyCell *cell;
        KfMapOccupancyCell *row;
        KfMapOccupancyLayer *lighting_layer;

        relative.vx = position->vx - game_graphics_runtime.render_state.view_position.vx;
        relative.vy = position->vy - game_graphics_runtime.render_state.view_position.vy;
        relative.vz = position->vz - game_graphics_runtime.render_state.view_position.vz;
        RotTrans(&relative, (VECTOR *)&model.t, &gte_flags);
        row = bss_801c7540.map_cells[position->vz >> 11];
        cell = &row[position->vx >> 11];
        if (map_layer != KF_MAP_LAYER_FIRST) {
            lighting_layer = &cell->layer[1];
        } else {
            lighting_layer = &cell->layer[0];
        }
        lighting = &game_graphics_runtime.collision_rows[
            lighting_layer->lighting_index & KF_MAP_CELL_LIGHTING_MASK];
    } else {
        model.t[0] = position->vx;
        model.t[1] = position->vy;
        model.t[2] = position->vz;
        {
            u16 layer_offset = player_state.map_layer_index;
            u8 lighting_index = *(
                &bss_801c7540.map_cells[
                    game_graphics_runtime.render_state.view_position.vz >> KF_MAP_CELL_POSITION_SHIFT][
                    game_graphics_runtime.render_state.view_position.vx >> KF_MAP_CELL_POSITION_SHIFT]
                    .layer[0].lighting_index + layer_offset);
            lighting = &game_graphics_runtime.collision_rows[lighting_index & KF_MAP_CELL_LIGHTING_MASK];
        }
    }

    if (render_mode == KF_RENDER_QUEUE_TEXTURED_UNBIASED) {
        render_mode = KF_RENDER_QUEUE_TEXTURED;
    } else if (relative.vy <= 0) {
        depth += 240;
    }

    matrix_set_rotation_yxz(rotation, &model);
    if (scale != NULL) {
        scale_vector.vx = scale->vx;
        scale_vector.vy = scale->vy;
        scale_vector.vz = scale->vz;
        ScaleMatrix(&model, &scale_vector);
    }

    if (lighting_override != KF_LIGHTING_NONE) {
        override = &game_graphics_runtime.collision_rows[KF_ENUM_ENCODE(u8, lighting_override)];
        if (override->motion.values[0] != -1) {
            fixed_lerp_nine_halfwords_q12(lighting->motion.values, override->motion.values,
                          color_matrix.m[0], lighting_blend);
            SetColorMatrix(&color_matrix);
        } else {
            SetColorMatrix((MATRIX *)&lighting->motion);
        }
        if (override->rotations[0].m[0][0] != -1) {
            MulMatrix0((MATRIX *)&override->rotations[0], &model, &light_matrix);
        } else {
            MulMatrix0((MATRIX *)&lighting->rotations[0], &model, &light_matrix);
        }
        if (override->filter.angle != -1) {
            fog_set_near(fixed_lerp_q12(lighting->filter.angle,
                                       override->filter.angle,
                                       lighting_blend));
        } else {
            fog_set_near(lighting->filter.angle);
        }
        if (override->filter.kinds.types[0] != 0xff) {
            red = fixed_lerp_q12(lighting->filter.kinds.types[0],
                                override->filter.kinds.types[0], lighting_blend);
            green = fixed_lerp_q12(lighting->filter.kinds.types[1],
                                  override->filter.kinds.types[1], lighting_blend);
            blue = fixed_lerp_q12(lighting->filter.kinds.types[2],
                                 override->filter.kinds.types[2], lighting_blend);
            SetBackColor(red, green, blue);
        } else {
            SetBackColor(lighting->filter.kinds.types[0],
                         lighting->filter.kinds.types[1],
                         lighting->filter.kinds.types[2]);
        }
    } else {
        SetColorMatrix((MATRIX *)&lighting->motion);
        fog_set_near(lighting->filter.angle);
        SetBackColor(lighting->filter.kinds.types[0],
                     lighting->filter.kinds.types[1],
                     lighting->filter.kinds.types[2]);
        MulMatrix0((MATRIX *)&lighting->rotations[0], &model, &light_matrix);
    }
    SetLightMatrix(&light_matrix);
    if (world_matrix != NULL) {
        MulMatrix2(world_matrix, &model);
    }
    SetRotMatrix(&model);
    SetTransMatrix(&model);

    asset_registry_select(asset_index);
    if (clip < KF_ANIMATION_CLIP_STATIC_OBJECT_FIRST) {
        object_index = 0;
        object = tmd_get_object(0);
        if (animation_prepare_asset_vertices(cache, asset_index, clip, phase,
                          object->vertex_count) == 0) {
            tmd_select_object_vertices(0);
        }
    } else {
        object_index = KF_ENUM_ENCODE(u16, clip) & KF_ASSET_OBJECT_INDEX_MASK;
        tmd_select_object_vertices(object_index);
        object = tmd_get_object(object_index);
    }
    if (world_matrix != NULL) {
        tmd_project_vertices_with_fog(object->vertex_count);
    } else {
        tmd_transform_vertices_depth(object->vertex_count, depth);
    }
    if (render_mode == KF_RENDER_QUEUE_TEXTURED) {
        render_enqueue_textured_tmd(object_index, depth);
    } else if (render_mode == KF_RENDER_QUEUE_CLIPPED) {
        render_enqueue_tmd_with_clipping(object_index, depth, NULL);
    } else {
        render_enqueue_blended_tmd(object_index, depth, render_mode);
    }
}

ADDRESS(0x80032ac4, 0x214)
void render_animated_object(u16 asset_index, const struct KfEulerAngles *rotation,
                   KfPoolRecord **cache, KF_ENUM_PARAM(KfAnimationClip, u16) clip, u16 phase,
                   s32 blend_mode, s32 lighting_flags, s16 depth)
{
    MATRIX model;
    KfCollisionRotation reversed_light;
    KfCollisionRow *lighting;
    KfTmdObject *object;

    model.t[2] = 0;
    model.t[1] = 0;
    model.t[0] = 0;
    matrix_set_rotation_yxz(rotation, &model);
    MulMatrix2(&game_graphics_runtime.render_state.view_matrix, &model);
    SetRotMatrix(&model);
    SetTransMatrix(&model);

    lighting = &game_graphics_runtime.collision_rows[lighting_flags & KF_LIGHTING_FLAGS_INDEX_MASK];
    SetColorMatrix((MATRIX *)&lighting->motion);
    SetBackColor(lighting->filter.kinds.types[0],
                 lighting->filter.kinds.types[1],
                 lighting->filter.kinds.types[2]);
    if (lighting_flags & KF_LIGHTING_FLAG_REVERSED_LIGHT) {
        reversed_light.m[0][0] = -lighting->rotations[0].m[0][0];
        reversed_light.m[0][1] = -lighting->rotations[0].m[0][1];
        reversed_light.m[0][2] = -lighting->rotations[0].m[0][2];
        reversed_light.m[1][0] = -lighting->rotations[0].m[1][0];
        reversed_light.m[1][1] = -lighting->rotations[0].m[1][1];
        reversed_light.m[1][2] = -lighting->rotations[0].m[1][2];
        reversed_light.m[2][0] = -lighting->rotations[0].m[2][0];
        reversed_light.m[2][1] = -lighting->rotations[0].m[2][1];
        reversed_light.m[2][2] = -lighting->rotations[0].m[2][2];
        SetLightMatrix((MATRIX *)&reversed_light);
    } else {
        SetLightMatrix((MATRIX *)&lighting->rotations[0]);
    }

    asset_registry_select(asset_index);
    object = tmd_get_object(0);
    if (animation_prepare_asset_vertices(cache, asset_index, clip, phase,
                      object->vertex_count) == 0) {
        tmd_select_object_vertices(0);
        object = tmd_get_object(0);
        tmd_project_vertices(object->vertex_count);
        render_enqueue_tmd_fixed_depth(0, blend_mode, depth);
    } else {
        tmd_project_vertices(object->vertex_count);
        render_enqueue_tmd_fixed_depth(0, blend_mode, depth);
    }
}

ADDRESS(0x80032cd8, 0x68)
KfAssetHeader *resource_registry_get(u16 index)
{
    KfAssetHeader *asset = game_graphics_runtime.asset_registry_entries[index];

    if (index < 104) {
        return asset;
    }
    /* Encoded range test: RECLAIMABLE or OWNED. */
    if (asset != NULL && (u8)(KF_ENUM_ENCODE(u8, memory_block_kind((u8 *)asset)) - 1) < 2) {
        return asset;
    }
    return NULL;
}

ADDRESS(0x80032d40, 0x38)
void resource_tmd_read_complete(u8 *data)
{
    KfAssetHeader *asset = (KfAssetHeader *)data;

    tmd_prepare_primitive_indices(ASSET_TMD(asset));
    memory_block_set_kind(data, KF_MEMORY_BLOCK_OWNED);
}

ADDRESS(0x80032d78, 0x70)
KF_ENUM_PARAM(KfMapLayerMask, u32) map_cell_layer_mask(const VECTOR *position)
{
    s32 z = (position->vz >> KF_MAP_CELL_POSITION_SHIFT) + game_graphics_runtime.render_state.cell_origin_z;
    s32 x;

    if ((u32)z >= KF_MAP_CELL_GRID_SIDE) {
        return KF_MAP_LAYER_NONE;
    }
    x = (position->vx >> KF_MAP_CELL_POSITION_SHIFT) + game_graphics_runtime.render_state.cell_origin_x;
    if ((u32)x >= KF_MAP_CELL_GRID_SIDE) {
        return KF_MAP_LAYER_NONE;
    }
    return game_graphics_runtime.render_grid.map_cell_layer_masks[z][x];
}

ADDRESS(0x80032de8, 0xc4)
KF_ENUM_PARAM(KfMapLayerMask, u32) map_cell_layer_mask_radius(
    const VECTOR *position, s32 radius)
{
    KfMapLayerMask mask = KF_MAP_LAYER_NONE;
    s32 z = (position->vz >> KF_MAP_CELL_POSITION_SHIFT) + game_graphics_runtime.render_state.cell_origin_z - radius;
    s32 x0 = (position->vx >> KF_MAP_CELL_POSITION_SHIFT) + game_graphics_runtime.render_state.cell_origin_x - radius;
    s32 x;
    s32 rows;
    s32 columns;

    radius *= 2;
    rows = radius;
    do {
        if (z >= 0 && (u32)z < KF_MAP_CELL_GRID_SIDE) {
            x = x0;
            columns = radius;
            do {
                if (x >= 0 && (u32)x < KF_MAP_CELL_GRID_SIDE) {
                    mask |= game_graphics_runtime.render_grid.map_cell_layer_masks[z][x];
                }
                x++;
                columns--;
            } while (columns != -1);
        }
        z++;
        rows--;
    } while (rows != -1);
    return mask;
}

ADDRESS(0x80032eac, 0x64)
b32 map_cell_visible(const VECTOR *position, s32 radius_x, s32 radius_z)
{
    s32 z = position->vz >> KF_MAP_CELL_POSITION_SHIFT;
    s32 x;

    if (z - radius_z <= game_graphics_runtime.render_state.view_cell_z &&
        game_graphics_runtime.render_state.view_cell_z <= z + radius_z) {
        x = position->vx >> KF_MAP_CELL_POSITION_SHIFT;
        if (x - radius_x <= game_graphics_runtime.render_state.view_cell_x &&
            game_graphics_runtime.render_state.view_cell_x <= x + radius_x) {
            return KF_TRUE;
        }
    }
    return KF_FALSE;
}

ADDRESS(0x80032f10, 0x9c)
void resource_tmd_queue_read(s32 archive_slot, s32 entry, s32 registry_index)
{
    u32 size = cd_archive_entry_size(archive_slot, entry);
    u8 *block;

    block = memory_arena_allocate_block(KF_GAME_RESOURCE_ARENA_BASE, size,
        (u8 **)&game_graphics_runtime.asset_registry_entries[registry_index]);
    if (block != NULL) {
        memory_block_set_kind(block, KF_MEMORY_BLOCK_PENDING);
        memory_block_set_tag(block, registry_index);
        /* Kind 0x10 completion passes the destination, not the request. */
        cd_archive_queue_read(archive_slot, entry, (u_long *)block,
            (KfCdRequestCallback)resource_tmd_read_complete);
    }
}

ADDRESS(0x80032fac, 0xf0)
void resource_vab_update_range(s32 archive_slot, s32 entry, s32 vab_slot,
    s32 count, b8 *flags)
{
    s32 end = count + entry;

    while (entry < end) {
        KfAudioVabSlot *slot = &audio_state.vab_slots[vab_slot];
        KfAudioVabStreamSlot *state = slot->stream_slot;

        if (*flags++) {
            if (state == NULL) {
                audio_queue_vab_stream(archive_slot, entry, vab_slot);
            } else if (state->state == KF_AUDIO_VAB_STREAM_RECLAIMABLE) {
                state->state = KF_AUDIO_VAB_STREAM_IN_USE;
            }
        } else if (state != NULL && state->state == KF_AUDIO_VAB_STREAM_IN_USE) {
            state->state = KF_AUDIO_VAB_STREAM_RECLAIMABLE;
        }
        entry++;
        vab_slot++;
    }
}

ADDRESS(0x8003309c, 0x118)
void resource_tmd_update_range(s32 archive_slot, s32 entry, s32 registry_index,
    s32 count, b8 *flags)
{
    s32 end = count + entry;

    while (entry < end) {
        u8 *block;

        if (*flags++) {
            block = (u8 *)game_graphics_runtime.asset_registry_entries[registry_index];
            if (block == NULL) {
                resource_tmd_queue_read(archive_slot, entry, registry_index);
            } else if (memory_block_kind(block) == KF_MEMORY_BLOCK_RECLAIMABLE) {
                memory_block_set_kind(block, KF_MEMORY_BLOCK_OWNED);
            }
        } else {
            block = (u8 *)game_graphics_runtime.asset_registry_entries[registry_index];
            if (block != NULL && memory_block_kind(block) != KF_MEMORY_BLOCK_PENDING) {
                memory_block_set_kind(block, KF_MEMORY_BLOCK_RECLAIMABLE);
            }
        }
        entry++;
        registry_index++;
    }
}
/* The two flag ranges are consumed as byte arrays by the resource updaters. */
ADDRESS(0x800331b4, 0xb70)
void render_scene_and_update_resources(void)
{
    struct KfEulerAngles rotation;
    /* Unreferenced 8-byte slot between rotation and actor_position. */
    SVECTOR unused;
    VECTOR actor_position;
    b8 tmd_flags[320];
    /* The second stack region spans 320 bytes; the VAB updater reads 64. */
    b8 vab_flags[320];
    KfActor *actor;
    KfMapObject *object;
    KfEffectRecord *effect;
    KfMapPlacedEntry *placed;
    s32 frame;
    s16 remaining;

    repeat_store_word((u32 *)tmd_flags, 0, 32);
    repeat_store_word((u32 *)vab_flags, 0, 16);
    for (actor = actor_state.actors, remaining = KF_ACTOR_CAPACITY - 1; remaining != -1;
         actor++, remaining--) {
        KF_ENUM_STORAGE(KfMapLayerMask, u32) layer;
        KfTargetGroup *group;
        const VECTOR *position;

        if (actor->lifecycle != KF_ACTOR_LIFECYCLE_ACTIVE) {
            continue;
        }
        if ((actor->flags & KF_ACTOR_FLAG_RENDER_INCLUDE_LAYER_0X20) != KF_ACTOR_FLAGS_NONE) {
            layer = actor->current_map_layer | KF_MAP_LAYER_IN_VIEW;
        } else {
            layer = actor->current_map_layer;
        }
        if ((actor->flags & KF_ACTOR_FLAG_RENDER_RADIUS_VISIBILITY) != KF_ACTOR_FLAGS_NONE) {
            goto actor_radius_check;
        }
        if ((map_cell_layer_mask(&actor->position) & layer) == KF_MAP_LAYER_NONE) {
            continue;
        }
actor_visible:
        if (resource_registry_get(actor->definition_id + 0x80) != NULL) {
            position = actor_resolve_group_position(actor, &actor_position);
            if ((actor->flags & KF_ACTOR_FLAG_RENDER_WITH_IDENTITY_MATRIX) != KF_ACTOR_FLAGS_NONE) {
                rotation.z = 0;
                rotation.y = 0;
                rotation.x = 0;
                render_world_model(actor->current_map_layer, actor->definition_id + 0x80,
                               &actor->position, &rotation, (SVECTOR *)&actor->model_scale_x,
                               &actor->animation_cache, &render_world_identity_matrix,
                               actor->animation_id, actor->animation_phase,
                               actor->lighting_override, actor->lighting_blend,
                               actor->render_mode, (s8)actor->render_depth);
            } else {
                rotation.x = actor->rotation.x;
                rotation.y = actor->rotation.y + KF_ANGLE_HALF_TURN;
                rotation.z = actor->rotation.z;
                render_world_model(actor->current_map_layer, actor->definition_id + 0x80,
                               position, &rotation, (SVECTOR *)&actor->model_scale_x,
                               &actor->animation_cache,
                               &game_graphics_runtime.render_state.view_matrix,
                               actor->animation_id, actor->animation_phase,
                               actor->lighting_override, actor->lighting_blend,
                               actor->render_mode, (s8)actor->render_depth);
            }
        }
        group = &actor_state.target_groups[actor->group_index];
        vab_flags[group->vab_resource_indices[0]] = KF_TRUE;
        vab_flags[group->vab_resource_indices[1]] = KF_TRUE;
        tmd_flags[actor->definition_id] = KF_TRUE;
        continue;
actor_radius_check:
        if ((map_cell_layer_mask_radius(&actor->position, 3) &
             actor->current_map_layer) != KF_MAP_LAYER_NONE) {
                 goto actor_visible;
             }
    }
    resource_tmd_update_range(KF_RESOURCE_ARCHIVE_MO, 0, 0x80, 0x80, tmd_flags);
    resource_vab_update_range(KF_RESOURCE_ARCHIVE_VAB, 0x20, 2, 0x40, vab_flags);

    frame = cd_state.frame_count;
    repeat_store_word((u32 *)tmd_flags, 0, 80);
    repeat_store_word((u32 *)vab_flags, 0, 16);
    for (object = map_object_state.objects, remaining = KF_MAP_OBJECT_CAPACITY - 1; remaining != -1;
         object++, remaining--) {
        KF_ENUM_STORAGE(KfMapLayerMask, u32) visibility;

        if (object->object_id == KF_OBJECT_NONE) {
            continue;
        }
        object->collision_flags &= ~KF_MAP_OBJECT_FLAG_RENDERED;
        switch (object->action) {
        case KF_MAP_OBJECT_OP_ANIMATED_MODEL:
            if (map_cell_visible(&object->position,
                                 object->tail.animated.radius_x,
                                 object->tail.animated.radius_z) &&
                (object->layer_mask & render_mask_scan_state.first_layer_mask) != KF_MAP_LAYER_NONE) {
                if (resource_registry_get(KF_ENUM_ENCODE(u16, object->object_id) + 0x100) != NULL) {
                    render_animated_object(KF_ENUM_ENCODE(u16, object->object_id) + 0x100,
                                   (const struct KfEulerAngles *)&object->rotation,
                                   &object->tail.animated.animation_cache,
                                   object->asset_clip_selector, object->phase_q12,
                                   object->tail.animated.blend_mode,
                                   object->tail.animated.lighting_flags,
                                   0x1fff - object->tail.animated.depth_code);
                    object->collision_flags |= KF_MAP_OBJECT_FLAG_RENDERED;
                }
                tmd_flags[KF_ENUM_ENCODE(u16, object->object_id)] = KF_TRUE;
            }
            break;
        case KF_MAP_OBJECT_OP_AMBIENT_SOUND: {
            s32 sound;
            s32 distance;
            s32 nearest;
            s32 volume;

            if (player_camera_within_map_region(object->position.vx >> KF_MAP_CELL_POSITION_SHIFT,
                              object->position.vz >> KF_MAP_CELL_POSITION_SHIFT,
                              object->tail.ambient_sound.region_width,
                              object->tail.ambient_sound.region_depth, 0x8000) != 0) {
                sound = object->tail.ambient_sound.sound_id;
                if (audio_state.voices.params[sound].vab_slot_index >= 0x42
                    && audio_state.voices.params[sound].vab_slot_index < 0x82) {
                    /* This update starts at VAB slot 0x42. */
                    vab_flags[audio_state.voices.params[sound].vab_slot_index - 0x42] = KF_TRUE;
                }
                if ((s32)(object->extra_40.next_sound_frame - frame) < 0) {
                    object->extra_40.next_sound_frame = frame +
                        object->tail.ambient_sound.repeat_delay_units * 6;
                    /* One local carries each half extent, the audible radius and
                     * finally the volume. */
                    volume = object->tail.ambient_sound.region_width * 0x400;
                    nearest = player_state.camera_position.vx - (volume + object->position.vx);
                    if (nearest < 0) {
                        nearest = -nearest;
                    }
                    nearest = volume - nearest;
                    volume = object->tail.ambient_sound.region_depth * 0x400;
                    distance = player_state.camera_position.vz - (volume + object->position.vz);
                    if (distance < 0) {
                        distance = -distance;
                    }
                    distance = volume - distance;
                    if (distance < nearest) {
                        nearest = distance;
                    }
                    volume = object->tail.ambient_sound.audible_radius_code << 11;
                    if (nearest >= volume) {
                        volume = object->tail.ambient_sound.maximum_volume;
                    } else {
                        if (volume == 0) {
                            continue;
                        }
                        volume = object->tail.ambient_sound.maximum_volume * nearest / volume;
                    }
                    if (object->tail.ambient_sound.vertical_attenuation_flags & 1) {
                        distance = player_state.camera_position.vy - object->position.vy;
                        if (distance < 0) {
                            distance = -distance;
                        }
                        distance = object->tail.ambient_sound.maximum_volume * distance >> 13;
                        volume -= distance;
                    }
                    if (volume > 19) {
                        audio_play_sound(object->tail.ambient_sound.sound_id, volume);
                    }
                }
            } else {
                object->extra_40.next_sound_frame = frame +
                    object->tail.ambient_sound.repeat_delay_units * 6;
            }
            break;
        }
        default: {
            KfRenderQueueMode render_mode;
            KfMapObjectTemplate *object_template;
            SVECTOR *scale;
            if ((object->collision_flags & KF_MAP_OBJECT_FLAG_RADIUS_VISIBLE) != KF_MAP_OBJECT_FLAGS_NONE) {
                goto map_radius_check;
            }
            visibility = map_cell_layer_mask(&object->position);
            if ((visibility & object->layer_mask) == KF_MAP_LAYER_NONE) {
                continue;
            }
            object_template = &map_object_state.templates[KF_ENUM_ENCODE(u16, object->object_id)];
map_ordinary_visible:
            tmd_flags[KF_ENUM_ENCODE(u16, object->object_id)] = KF_TRUE;
            vab_flags[object_template->vab_resource_index] = KF_TRUE;
            scale = &object->scale;
            if (resource_registry_get(KF_ENUM_ENCODE(u16, object->object_id) + 0x100) != NULL) {
                rotation.x = object->rotation.vx;
                rotation.y = object->rotation.vy + KF_ANGLE_HALF_TURN;
                rotation.z = object->rotation.vz;
                render_mode = object->render_queue_mode;
                if ((object->collision_flags & KF_MAP_OBJECT_FLAG_NEAR_CLIP) != KF_MAP_OBJECT_FLAGS_NONE) {
                    render_mode = (visibility & KF_MAP_LAYER_NEAR_CLIPPED) != KF_MAP_LAYER_NONE
                        ? KF_RENDER_QUEUE_CLIPPED : KF_RENDER_QUEUE_TEXTURED;
                }
                render_world_model(object->layer_mask, KF_ENUM_ENCODE(u16, object->object_id) + 0x100,
                               &object->position, &rotation, scale,
                               (KfPoolRecord **)&object->tail,
                               &game_graphics_runtime.render_state.view_matrix,
                               object->asset_clip_selector, object->phase_q12,
                               object->lighting_override_index, object->lighting_blend_q12,
                               render_mode,
                               object->render_depth_offset);
                object->collision_flags |= KF_MAP_OBJECT_FLAG_RENDERED;
            }
            continue;
map_radius_check:
            object_template = &map_object_state.templates[KF_ENUM_ENCODE(u16, object->object_id)];
            visibility = map_cell_layer_mask_radius(&object->position,
                object_template->params.marker.marker_action_05);
            if ((visibility & object->layer_mask) != KF_MAP_LAYER_NONE) {
                goto map_ordinary_visible;
            }
            break;
        }
        }
    }
    resource_tmd_update_range(KF_RESOURCE_ARCHIVE_MO, 0x80, 0x100, 0x140, tmd_flags);
    resource_vab_update_range(KF_RESOURCE_ARCHIVE_VAB, 0x60, 0x42, 0x40, vab_flags);

    for (effect = effect_state.records, remaining = KF_EFFECT_CAPACITY - 1; remaining != -1;
         effect++, remaining--) {
        if (effect->type == KF_EFFECT_SLOT_FREE ||
            (effect->render_flags & KF_EFFECT_RENDER_VISIBILITY_MASK) == KF_EFFECT_RENDER_HIDDEN) {
            continue;
        }
        if ((effect->render_flags & KF_EFFECT_RENDER_VISIBILITY_MASK) != KF_EFFECT_RENDER_ALWAYS_VISIBLE &&
            (map_cell_layer_mask(&effect->position) & effect->map_layer_mask) == KF_MAP_LAYER_NONE) {
            continue;
        }
        switch (effect->render_flags & KF_EFFECT_RENDER_TRANSFORM_MASK) {
        case KF_EFFECT_RENDER_WORLD_TRANSFORM:
            rotation.x = effect->rotation.vx;
            rotation.y = effect->rotation.vy + KF_ANGLE_HALF_TURN;
            rotation.z = effect->rotation.vz;
            render_world_model(effect->map_layer_mask, KF_ENUM_ENCODE(u8, effect->render_id) + 0x28,
                           &effect->position, &rotation, (SVECTOR *)&effect->scale_x,
                           &effect->cache_tail.animation_cache,
                           &game_graphics_runtime.render_state.view_matrix,
                           effect->animation_clip, effect->animation_phase_q12,
                           effect->lighting_override_index, effect->lighting_blend_q12,
                           effect->render_queue_mode, -60);
            break;
        case KF_EFFECT_RENDER_IDENTITY_TRANSFORM:
            render_world_model(effect->map_layer_mask, KF_ENUM_ENCODE(u8, effect->render_id) + 0x28,
                           &effect->position, (const struct KfEulerAngles *)&effect->rotation,
                           (SVECTOR *)&effect->scale_x, &effect->cache_tail.animation_cache,
                           &render_world_identity_matrix,
                           effect->animation_clip, effect->animation_phase_q12,
                           effect->lighting_override_index, effect->lighting_blend_q12,
                           effect->render_queue_mode, -60);
            break;
        case KF_EFFECT_RENDER_PITCH_TRANSFORM:
            render_world_model(effect->map_layer_mask, KF_ENUM_ENCODE(u8, effect->render_id) + 0x28,
                           &effect->position, (const struct KfEulerAngles *)&effect->rotation,
                           (SVECTOR *)&effect->scale_x, &effect->cache_tail.animation_cache,
                           &game_graphics_runtime.render_state.pitch_matrix,
                           effect->animation_clip, effect->animation_phase_q12,
                           effect->lighting_override_index, effect->lighting_blend_q12,
                           effect->render_queue_mode, -60);
            break;
        case KF_EFFECT_RENDER_SCREEN_SPACE:
            render_world_model(effect->map_layer_mask, KF_ENUM_ENCODE(u8, effect->render_id) + 0x28,
                           &effect->position,
                           (const struct KfEulerAngles *)&effect->rotation,
                           (SVECTOR *)&effect->scale_x,
                           &effect->cache_tail.animation_cache, NULL,
                           effect->animation_clip, effect->animation_phase_q12,
                           effect->lighting_override_index, effect->lighting_blend_q12,
                           effect->render_queue_mode, 0x14);
            break;
        default:
            break;
        }
    }

    placed = game_graphics_runtime.map_placed_entries;
    rotation.z = 0;
    rotation.y = 0;
    rotation.x = 0;
    remaining = KF_MAP_PLACED_ENTRY_COUNT - 1;
    while (remaining != -1) {
        KF_ENUM_STORAGE(KfMapLayerMask, u32) visibility;
        if (placed->model_index != KF_MAP_PLACED_NONE) {
            visibility = map_cell_layer_mask(&placed->position);
            if ((visibility & placed->layer_mask) != KF_MAP_LAYER_NONE) {
                render_world_model(placed->layer_mask,
                               placed->model_index + KF_MAP_PLACED_ASSET_BASE,
                               &placed->position, &rotation, NULL, NULL,
                               &game_graphics_runtime.render_state.pitch_matrix,
                               KF_ENUM_DECODE(KF_ENUM_PROMOTED(KfAnimationClip),
                                              placed->frame_index + KF_MAP_PLACED_CLIP_BASE),
                               0, KF_LIGHTING_MAP_PLACED,
                               0x1000, KF_RENDER_QUEUE_BLEND_ADD, 0);
            }
            if (placed->frame_period != KF_MAP_PLACED_ANIMATION_DISABLED &&
                game_graphics_runtime.map_placed_frame_counter % placed->frame_period == 0) {
                placed->frame_index++;
                if (placed->frame_index >= placed->frame_count) {
                    placed->frame_index = 0;
                }
            }
        }
        placed++;
        remaining--;
    }
    game_graphics_runtime.map_placed_frame_counter++;
}

ADDRESS(0x80033d24, 0x154)
void notification_draw_quad(const KfNotificationQuad *source, u16 tpage_flags, const u8 *color)
{
    POLY_FT4 *quad = (POLY_FT4 *)game_graphics_runtime.display_state.primitive_buffer->cursor;

    game_graphics_runtime.display_state.primitive_buffer->cursor += sizeof(POLY_FT4);
    if (game_graphics_runtime.display_state.primitive_buffer->cursor >
        game_graphics_runtime.display_state.primitive_buffer->end) {
        return;
    }

    setPolyFT4(quad);
    setSemiTrans(quad, 1);
    quad->x0 = quad->x2 = source->x;
    quad->x1 = quad->x3 = source->x + source->width;
    quad->y0 = quad->y1 = source->y;
    quad->y2 = quad->y3 = source->y + source->height;
    quad->clut = source->clut;
    quad->tpage = source->tpage | tpage_flags;
    quad->u0 = quad->u2 = source->texture_u;
    quad->u1 = quad->u3 = source->texture_u + source->texture_width;
    quad->v0 = quad->v1 = source->texture_v;
    quad->v2 = quad->v3 = source->texture_v + source->texture_height;
    setRGB0(quad, color[0], color[1], color[2]);
    AddPrim((void *)(game_graphics_runtime.display_state.ordering_table + 1), (void *)quad);
}

ADDRESS(0x80033e78, 0x90)
void notification_draw(void)
{
    KfNotificationQuad *quad = notification_quads;
    u8 color[3];

    color[0] = color[1] = color[2] = game_graphics_runtime.notification_brightness;
    if (quad->kind == KF_SPRITE_END) {
        return;
    }
    do {
        if (quad->kind != KF_SPRITE_HIDDEN) {
            notification_draw_quad(quad, KF_GPU_TPAGE_ABR(KF_RENDER_QUEUE_BLEND_ADD), color);
            notification_draw_quad(quad, KF_GPU_TPAGE_ABR(KF_RENDER_QUEUE_BLEND_SUBTRACT), color);
        }
        quad++;
    } while (quad->kind != KF_SPRITE_END);
}

ADDRESS(0x80033f08, 0xa4)
void notify_enqueue(KF_ENUM_PARAM(KfNotificationId, s32) message_id, ...)
{
    u8 *head;

    if (message_id > KF_NOTIFICATION_MAX_QUEUED_ID) {
        return;
    }
    head = &game_graphics_runtime.notification_control.queue_head;
    if (game_graphics_runtime.notification_message_ids[*head] == KF_NOTIFICATION_NONE) {
        game_graphics_runtime.notification_message_ids[*head] = message_id;
        if (message_id == KF_NOTIFICATION_PAYLOAD_ID) {
            va_list arguments;
            va_start(arguments, message_id);
            game_graphics_runtime.notification_payloads[*head] = va_arg(arguments, s32);
            va_end(arguments);
        }
        *head = (*head + 1) & (KF_NOTIFICATION_CAPACITY - 1);
    }
}

ADDRESS(0x80033fac, 0x10)
void notification_digit_set_v(KfNotificationDigitSprite *sprite, s32 digit)
{
    sprite->texture_v = digit * 15;
}

static inline void notification_dequeue_group(void)
{
    KfNotificationControl *control;
    KF_ENUM_STORAGE(KfNotificationId, s32) id;

    control = &game_graphics_runtime.notification_control;
    id = game_graphics_runtime.notification_message_ids[
        game_graphics_runtime.notification_control.queue_tail];
    do {
        game_graphics_runtime.notification_message_ids[control->queue_tail] =
            KF_NOTIFICATION_NONE;
        control->queue_tail = (control->queue_tail + 1) & (KF_NOTIFICATION_CAPACITY - 1);
    } while (id == game_graphics_runtime.notification_message_ids[control->queue_tail]
             && id != KF_NOTIFICATION_PAYLOAD_ID);
    control->effect_phase = KF_NOTIFICATION_IDLE;
}

ADDRESS(0x80033fbc, 0x300)
void notification_update(void)
{
    switch (game_graphics_runtime.notification_control.effect_phase) {
    case KF_NOTIFICATION_IDLE: {
        u8 tail = game_graphics_runtime.notification_control.queue_tail;
        KfNotificationId id = game_graphics_runtime.notification_message_ids[tail];
        if (id == KF_NOTIFICATION_NONE) {
            break;
        }
        game_graphics_runtime.notification_control.effect_phase = KF_NOTIFICATION_FADE_IN;
        game_graphics_runtime.notification_brightness = 0;
        game_graphics_runtime.notification_control.hold_frames = 15;
        if (id == KF_NOTIFICATION_PAYLOAD_ID) {
            s16 digits[12];

            notification_quads[KF_NOTIFICATION_TEXT_SPRITE].kind = KF_SPRITE_HIDDEN;
            notification_quads[KF_NOTIFICATION_GOLD_SPRITE].kind = KF_SPRITE_VISIBLE;
            notification_quads[KF_NOTIFICATION_GOLD_SPRITE].texture_u = 128;
            notification_quads[KF_NOTIFICATION_GOLD_SPRITE].texture_v = 42;
            menu_format_number(game_graphics_runtime.notification_payloads[tail],
                               4, KF_FORMAT_PAD_SPACES, KF_MENU_FORMAT_STYLE_PLAIN, digits);
            notification_quads[KF_NOTIFICATION_ONES_SPRITE].kind = KF_SPRITE_VISIBLE;
            notification_digit_set_v(&notification_quads[KF_NOTIFICATION_ONES_SPRITE], (u16)digits[3]);
            notification_quads[KF_NOTIFICATION_TENS_SPRITE].kind = KF_SPRITE_VISIBLE;
            notification_digit_set_v(&notification_quads[KF_NOTIFICATION_TENS_SPRITE], (u16)digits[2]);
            notification_quads[KF_NOTIFICATION_HUNDREDS_SPRITE].kind = KF_SPRITE_VISIBLE;
            notification_digit_set_v(&notification_quads[KF_NOTIFICATION_HUNDREDS_SPRITE], (u16)digits[1]);
            notification_quads[KF_NOTIFICATION_THOUSANDS_SPRITE].kind = KF_SPRITE_VISIBLE;
            notification_digit_set_v(&notification_quads[KF_NOTIFICATION_THOUSANDS_SPRITE], (u16)digits[0]);
        } else {
            notification_quads[KF_NOTIFICATION_TEXT_SPRITE].kind = KF_SPRITE_VISIBLE;
            notification_quads[KF_NOTIFICATION_THOUSANDS_SPRITE].kind = KF_SPRITE_HIDDEN;
            notification_quads[KF_NOTIFICATION_HUNDREDS_SPRITE].kind = KF_SPRITE_HIDDEN;
            notification_quads[KF_NOTIFICATION_TENS_SPRITE].kind = KF_SPRITE_HIDDEN;
            notification_quads[KF_NOTIFICATION_ONES_SPRITE].kind = KF_SPRITE_HIDDEN;
            notification_quads[KF_NOTIFICATION_GOLD_SPRITE].kind = KF_SPRITE_HIDDEN;
            notification_quads[KF_NOTIFICATION_TEXT_SPRITE].texture_u = (KF_ENUM_VALUE(id) / 18) << 7;
            notification_quads[KF_NOTIFICATION_TEXT_SPRITE].texture_v = (KF_ENUM_VALUE(id) % 18) * 14;
        }
        break;
    }
    case KF_NOTIFICATION_FADE_IN:
        game_graphics_runtime.notification_brightness += 20;
        if (game_graphics_runtime.notification_brightness >= 100) {
            game_graphics_runtime.notification_control.effect_phase = KF_NOTIFICATION_HOLD;
        }
        break;
    case KF_NOTIFICATION_HOLD:
    {
        u8 frames = game_graphics_runtime.notification_control.hold_frames - 1;
        game_graphics_runtime.notification_control.hold_frames = frames;
        if (frames == 0) {
            game_graphics_runtime.notification_control.effect_phase = KF_NOTIFICATION_FADE_OUT;
        }
        break;
    }
    case KF_NOTIFICATION_FADE_OUT:
        game_graphics_runtime.notification_brightness -= 20;
        if (game_graphics_runtime.notification_brightness == 0) {
            notification_quads[KF_NOTIFICATION_THOUSANDS_SPRITE].kind = KF_SPRITE_HIDDEN;
            notification_quads[KF_NOTIFICATION_HUNDREDS_SPRITE].kind = KF_SPRITE_HIDDEN;
            notification_quads[KF_NOTIFICATION_TENS_SPRITE].kind = KF_SPRITE_HIDDEN;
            notification_quads[KF_NOTIFICATION_ONES_SPRITE].kind = KF_SPRITE_HIDDEN;
            notification_quads[KF_NOTIFICATION_GOLD_SPRITE].kind = KF_SPRITE_HIDDEN;
            notification_quads[KF_NOTIFICATION_TEXT_SPRITE].kind = KF_SPRITE_HIDDEN;
            notification_dequeue_group();
        }
        break;
    }
}

ADDRESS(0x800342bc, 0x1c)
void display_toggle_buffer_index(void)
{
    game_graphics_runtime.display_state.buffer_index =
        display_next_buffer(game_graphics_runtime.display_state.buffer_index);
}

ADDRESS(0x800342d8, 0x3f4)
void render_game_frame(const VECTOR *position, const SVECTOR *rotation)
{
    s32 remainder;
    s32 hp_hundreds;
    s32 hp_tens;
    s32 hp_ones;
    s32 mp_hundreds;
    s32 mp_tens;
    s32 mp_ones;
    s32 attack_width;
    s32 magic_width;
    s32 yaw_delta;
    KfSpriteState row_state;

    display_set_view_transform(position, rotation);
    floor_item_update_textures();
    notification_update();
    build_camera_map_cell_layer_masks();
    display_begin_frame();
    pool_mark_allocated();
    render_player_weapon();

    /* The option bytes encode HIDDEN/VISIBLE directly. */
    render_model_rows[0].state = KF_ENUM_DECODE(KfSpriteState,
        KF_ENUM_VALUE(player_state.compass_enabled));
    row_state = KF_ENUM_DECODE(KfSpriteState, KF_ENUM_VALUE(player_state.hud_gauges_enabled));
    render_model_rows[13].state = row_state;
    render_model_rows[12].state = row_state;
    render_model_rows[11].state = row_state;
    render_model_rows[10].state = row_state;
    render_model_rows[9].state = row_state;
    render_model_rows[8].state = row_state;
    render_model_rows[7].state = row_state;
    render_model_rows[6].state = row_state;
    render_model_rows[5].state = row_state;
    render_model_rows[4].state = row_state;
    render_model_rows[3].state = row_state;
    render_model_rows[2].state = row_state;
    render_model_rows[1].state = row_state;

    yaw_delta = render_model_yaw_smoothing_accumulator +
                angle_shortest_delta(render_model_rows[0].rotation.vy,
                                     game_graphics_runtime.render_state.view_rotation.vy);
    render_model_yaw_smoothing_accumulator = yaw_delta;
    if (yaw_delta > 0) {
        render_model_yaw_smoothing_accumulator = yaw_delta - ((yaw_delta + 7) >> 3);
    } else if (yaw_delta < 0) {
        render_model_yaw_smoothing_accumulator = yaw_delta - ((yaw_delta - 7) >> 3);
    }

    remainder = player_state.vitals.current_hp % 1000;
    hp_hundreds = remainder / 100;
    hp_tens = (remainder % 100) / 10;
    hp_ones = remainder % 10;
    remainder = player_state.vitals.current_mp % 1000;
    mp_hundreds = remainder / 100;
    mp_tens = (remainder % 100) / 10;
    mp_ones = remainder % 10;
    attack_width = player_state.attack_charge_current * 204 / 5000;
    magic_width = player_state.magic_charge * 204 / 5000;

    render_model_rows[0].rotation.vy +=
        render_model_yaw_smoothing_accumulator >> 6;
    render_model_rows[0].rotation.vx = game_graphics_runtime.render_state.view_rotation.vx;
    render_model_rows[3].asset_id = hp_hundreds + 3;
    render_model_rows[4].asset_id = hp_tens + 3;
    render_model_rows[5].asset_id = hp_ones + 3;
    render_model_rows[6].asset_id = mp_hundreds + 3;
    render_model_rows[7].asset_id = mp_tens + 3;
    render_model_rows[8].asset_id = mp_ones + 3;
    render_model_rows[9].scale.vx = attack_width;
    render_model_rows[10].scale.vx = magic_width;

    render_active_model_rows();
    notification_draw();
    render_map_cell_window();
    render_scene_and_update_resources();
    render_sliding_panel_primary();
    render_sliding_panel_secondary();
    render_color_overlay();
    render_accumulated_color_overlay();
    display_present_frame();
    cd_wait_two_vsyncs();
    pool_release_stale();
}

enum {
    KF_MENU_MODEL_BACK_COLOR = 60,
    KF_MENU_MODEL_GEOM_SCREEN = 200,
    KF_MENU_MODEL_FOG_NEAR = 0x59d8
};

ADDRESS(0x800346cc, 0x68)
void menu_render_item_model(void)
{
    SetBackColor(KF_MENU_MODEL_BACK_COLOR, KF_MENU_MODEL_BACK_COLOR, KF_MENU_MODEL_BACK_COLOR);
    SetGeomScreen(KF_MENU_MODEL_GEOM_SCREEN);
    tmd_select(KF_TMD_SLOT_MENU_ITEM);
    tmd_select_object_vertices(0);
    fog_set_near(KF_MENU_MODEL_FOG_NEAR);
    tmd_project_vertices_with_fog(tmd_get_object(0)->vertex_count);
    render_enqueue_textured_tmd(0, 0);
}

ADDRESS(0x80034734, 0xb8)
void asset_registry_load_tmd_archive(u16 first_asset_id, u8 *archive)
{
    u16 count = *(u16 *)archive;

    archive += KF_ASSET_ARCHIVE_HEADER_BYTES;
    while (count-- != 0) {
        KfAssetHeader *asset = (KfAssetHeader *)archive;
        u32 size = asset->byte_size;

        if (size >= KF_ASSET_MIN_REGISTERED_BYTES) {
            game_graphics_runtime.asset_registry_entries[first_asset_id] = asset;
            asset_registry_select(first_asset_id);
            tmd_prepare_primitive_indices(game_graphics_runtime.tmd_state.current_asset);
        }
        archive += size;
        first_asset_id++;
    }
}

ADDRESS(0x800347ec, 0x48)
void asset_registry_set(u16 index, KfAssetHeader *asset)
{
    game_graphics_runtime.asset_registry_entries[index] = asset;
    asset_registry_select(index);
    tmd_prepare_primitive_indices(game_graphics_runtime.tmd_state.current_asset);
}

ADDRESS(0x80034834, 0x38)
void asset_registry_select(u16 index)
{
    KfAssetHeader *asset = game_graphics_runtime.asset_registry_entries[index];

    game_graphics_runtime.tmd_state.current_asset = ASSET_TMD(asset);
}

enum { KF_ANIMATION_BLEND_ONE = 0x1000, KF_ANIMATION_BLEND_SHIFT = 12 };

ADDRESS(0x8003486c, 0xc8)
KfAnimKeyframe *animation_select_keyframe(KfAssetHeader *asset,
    KF_ENUM_PARAM(KfAnimationClip, s32) clip_index, s32 phase, s32 *keyframe_index,
    u32 *blend_fraction)
{
    u32 *clip_table = ASSET_CLIP_TABLE(asset);
    KfAnimClip *clip = ASSET_CLIP(asset, clip_table[KF_ENUM_ENCODE(s32, clip_index)]);
    u32 *offsets = clip->keyframe_offsets;
    s32 remaining = clip->keyframe_count;
    s32 index = 0;
    s32 phase_end = 0;
    s32 phase_start = 0;
    u32 fraction;
    KfAnimKeyframe *keyframe;

    for (--remaining; remaining != -1; --remaining) {
        keyframe = ASSET_KEYFRAME(asset, *offsets++);

        phase_end += keyframe->duration;
        if (phase < phase_end) {
            fraction = ((u32)(phase - phase_start) << KF_ANIMATION_BLEND_SHIFT)
                / keyframe->duration;
            if (keyframe->reverse != 0) {
                fraction = KF_ANIMATION_BLEND_ONE - fraction;
            }
            goto selected;
        }
        phase_start = phase_end;
        index++;
    }
    index--;
    fraction = KF_ANIMATION_BLEND_ONE;

selected:
    *keyframe_index = index;
    *blend_fraction = fraction;
    return keyframe;
}

enum { KF_ANIMATION_SPARSE_SKIP = -32768 };

ADDRESS(0x80034934, 0xc4)
void animation_expand_sparse_vertices(SVECTOR *vertices, const SVECTOR *base,
                                      const s16 *encoded)
{
    s32 remaining = *encoded++;

    for (--remaining; remaining != -1; --remaining) {
        u16 value = *encoded++;

        if ((s16)value == KF_ANIMATION_SPARSE_SKIP) {
            s32 copy_count = *encoded++;

            for (--copy_count; copy_count != -1; --copy_count) {
                copyVector(vertices, base);
                vertices++;
                base++;
            }
        } else {
            vertices->vx = value;
            vertices->vy = *encoded++;
            vertices->vz = *encoded++;
            vertices++;
            base++;
        }
    }
}

ADDRESS(0x800349f8, 0x7c)
void animation_decode_sparse_vertices(SVECTOR *vertices, const s16 *encoded)
{
    s32 remaining = *encoded++;

    for (--remaining; remaining != -1; --remaining) {
        u16 value = *encoded++;

        if ((s16)value == KF_ANIMATION_SPARSE_SKIP) {
            vertices += *encoded++;
        } else {
            vertices->vx = value;
            vertices->vy = *encoded++;
            vertices->vz = *encoded++;
            vertices++;
        }
    }
}

ADDRESS(0x80034a74, 0x2b8)
void animation_apply_sparse_morph(SVECTOR *vertices, const s16 *encoded, s32 blend_fraction)
{
    MATRIX deltas;
    VECTOR scale;
    SVECTOR *group_start;
    SVECTOR *cursor;
    s32 pending;
    s32 remaining;
    s16 *delta_write;

    scale.vz = blend_fraction;
    scale.vy = blend_fraction;
    scale.vx = blend_fraction;
    remaining = *encoded;
    cursor = vertices;
    encoded++;
    pending = 0;
    group_start = cursor;
    delta_write = &deltas.m[0][0];

    for (--remaining; remaining != -1; --remaining) {
        s16 value = *encoded++;

        if (value == KF_ANIMATION_SPARSE_SKIP) {
            cursor += *encoded++;
            if (pending != 0) {
                s16 *scaled;

                ScaleMatrix(&deltas, &scale);
                scaled = &deltas.m[0][0];
                for (--pending; pending != -1; --pending) {
                    group_start->vx += *scaled++;
                    group_start->vy += *scaled++;
                    group_start->vz += *scaled++;
                    group_start++;
                }
                pending = 0;
                delta_write = &deltas.m[0][0];
            }
            group_start = cursor;
        } else {
            *delta_write++ = value - cursor->vx;
            *delta_write++ = *encoded++ - cursor->vy;
            *delta_write++ = *encoded++ - cursor->vz;
            cursor++;

            if (pending == 2) {
                ScaleMatrix(&deltas, &scale);
                group_start[0].vx += deltas.m[0][0];
                group_start[0].vy += deltas.m[0][1];
                group_start[0].vz += deltas.m[0][2];
                group_start[1].vx += deltas.m[1][0];
                group_start[1].vy += deltas.m[1][1];
                group_start[1].vz += deltas.m[1][2];
                group_start[2].vx += deltas.m[2][0];
                pending = 0;
                group_start[2].vy += deltas.m[2][1];
                group_start[2].vz += deltas.m[2][2];
                delta_write = &deltas.m[0][0];
                group_start = cursor;
            } else {
                ++pending;
            }
        }
    }

    if (pending != 0) {
        s16 *scaled;

        ScaleMatrix(&deltas, &scale);
        scaled = &deltas.m[0][0];
        for (--pending; pending != -1; --pending) {
            group_start->vx += *scaled++;
            group_start->vy += *scaled++;
            group_start->vz += *scaled++;
            group_start++;
        }
    }
}

ADDRESS(0x80034d2c, 0x7c)
const s16 *animation_find_sparse_vertex(const s16 *encoded, s32 vertex_index)
{
    s32 remaining = *encoded;
    s32 current = 0;

    encoded++;
    for (--remaining; remaining != -1; --remaining) {
        s16 value = *encoded;

        if (value == KF_ANIMATION_SPARSE_SKIP) {
            encoded++;
            current += *encoded++;
            if (vertex_index < current) {
                return NULL;
            }
        } else {
            if (current == vertex_index) {
                return encoded;
            }
            current++;
            encoded += 3;
        }
    }
    return NULL;
}


ADDRESS(0x80034da8, 0x2d4)
s32 animation_prepare_asset_vertices(KfPoolRecord **owner_slot, s32 asset_index,
                  KF_ENUM_PARAM(KfAnimationClip, s32) clip,
                  s32 phase, s32 vertex_count)
{
    KfAssetHeader *asset = game_graphics_runtime.asset_registry_entries[asset_index];
    KfPoolRecord *record = *owner_slot;
    KfAnimKeyframe *keyframe;
    u32 *morph_offsets;
    u16 *morph_indices;
    u16 remaining;
    s32 keyframe_index;
    u32 blend_fraction;

    if (asset->animation_present == 0) {
        if (record != NULL) {
            pool_record_release(record);
        }
        asset_registry_select(asset_index);
        tmd_select_object_vertices(0);
        return 1;
    }

    if (record == NULL) {
        record = pool_allocate();
        if (record == NULL) {
            return 0;
        }
allocate_vertices:
        record->asset_index = asset_index;
        record->owner_slot = owner_slot;
        for (;;) {
            record->cached_vertices = (SVECTOR *)memory_malloc_checked(vertex_count * sizeof(SVECTOR));
            if (record->cached_vertices != NULL) {
                break;
            }
            pool_release_all();
        }
        *owner_slot = record;
    } else if (record->asset_index != asset_index) {
        pool_record_release(record);
        record->clip_index = KF_ANIMATION_CLIP_NONE;
        goto allocate_vertices;
    }

    keyframe = animation_select_keyframe(asset, clip, phase, &keyframe_index,
                                          &blend_fraction);
    if (record->clip_index != clip || record->keyframe_index != keyframe_index) {
        tmd_select_object_vertices(0);
        morph_offsets = ASSET_MORPH_OFFSETS(asset);
        remaining = keyframe->morph_count;
        if (remaining != 0) {
            morph_indices = (u16 *)(keyframe + 1);
            animation_expand_sparse_vertices(
                record->cached_vertices, game_graphics_runtime.current_tmd_vertices,
                ASSET_MORPH(asset, morph_offsets[*morph_indices++]));
            for (--remaining; remaining != 0; --remaining) {
                animation_decode_sparse_vertices(
                    record->cached_vertices,
                    ASSET_MORPH(asset, morph_offsets[*morph_indices++]));
            }
        } else {
            const u32 *source = (const u32 *)game_graphics_runtime.current_tmd_vertices;
            u32 *destination = (u32 *)record->cached_vertices;
            u16 copy_count = vertex_count;

            do {
                *destination++ = *source++;
                *destination++ = *source++;
            } while (--copy_count != 0);
        }
        record->rest_morph_offset = morph_offsets[keyframe->rest_index];
    }
    record->clip_index = clip;
    record->keyframe_index = keyframe_index;

    {
        u16 copy_count = vertex_count;
        const u32 *source = (const u32 *)record->cached_vertices;
        u32 *destination = (u32 *)game_graphics_runtime.animation_vertex_scratch;

        do {
            *destination++ = *source++;
            *destination++ = *source++;
        } while (--copy_count != 0);
    }
    animation_apply_sparse_morph(game_graphics_runtime.animation_vertex_scratch,
                   ASSET_MORPH(asset, record->rest_morph_offset),
                   blend_fraction);
    tmd_set_current_vertices(game_graphics_runtime.animation_vertex_scratch);
    record->state = KF_ANIMATION_CACHE_LIVE;
    return (s32)record;
}

ADDRESS(0x8003507c, 0x2a0)
b32 animation_sample_vertex(s32 asset_index, KF_ENUM_PARAM(KfAnimationClip, s32) clip, s32 phase,
                            s32 vertex_index,
                  SVECTOR *output)
{
    KfAssetHeader *asset = resource_registry_get(asset_index);
    KfTmdHeader *tmd;
    SVECTOR *vertices;
    KfAnimKeyframe *keyframe;
    SVECTOR vertex;
    u32 *morph_offsets;
    u16 *morph_indices;
    const s16 *encoded;
    s32 remaining;
    s32 keyframe_index;
    u32 blend_fraction;

    if (asset == NULL) {
        output->vz = 0;
        output->vy = 0;
        output->vx = 0;
        return KF_TRUE;
    }

    tmd = ASSET_TMD(asset);
    if (clip >= KF_ANIMATION_CLIP_STATIC_OBJECT_FIRST) {
copy_object_vertex:
        vertices = TMD_OBJECT_VERTICES(tmd, &TMD_OBJECTS(tmd)[KF_ENUM_ENCODE(s32,
            clip) & KF_ASSET_OBJECT_INDEX_MASK]);
        *output = vertices[vertex_index];
        goto finished;
    }
    if (asset->animation_present == 0) {
        clip = KF_ANIMATION_CLIP_FIRST;
        goto copy_object_vertex;
    }

    vertices = TMD_OBJECT_VERTICES(tmd, &TMD_OBJECTS(tmd)[0]);
    keyframe = animation_select_keyframe(asset, clip, phase, &keyframe_index,
                                          &blend_fraction);
    vertex = vertices[vertex_index];
    morph_offsets = ASSET_MORPH_OFFSETS(asset);
    remaining = keyframe->morph_count;
    morph_indices = (u16 *)(keyframe + 1);
    for (--remaining; remaining != -1; --remaining) {
        encoded = animation_find_sparse_vertex(
            ASSET_MORPH(asset, morph_offsets[*morph_indices++]),
            vertex_index);
        if (encoded != NULL) {
            vertex.vx = *encoded++;
            vertex.vy = *encoded++;
            vertex.vz = *encoded;
        }
    }
    encoded = animation_find_sparse_vertex(
        ASSET_MORPH(asset, morph_offsets[keyframe->rest_index]),
        vertex_index);
    if (encoded != NULL) {
        vertex.vx = (((*encoded++ - vertex.vx) * (s32)blend_fraction) >> KF_FIXED12_BITS) + vertex.vx;
        vertex.vy = (((*encoded - vertex.vy) * (s32)blend_fraction) >> KF_FIXED12_BITS) + vertex.vy;
        vertex.vz = (((encoded[1] - vertex.vz) * (s32)blend_fraction) >> KF_FIXED12_BITS) + vertex.vz;
    }
    *output = vertex;
finished:
    return KF_FALSE;
}

ADDRESS(0x8003531c, 0x60)
u32 asset_vertex_count(s32 asset_index, KF_ENUM_PARAM(KfAnimationClip, s32) clip)
{
    KfAssetHeader *asset = game_graphics_runtime.asset_registry_entries[asset_index];
    KfTmdHeader *tmd;

    if (asset == NULL) {
        return 0;
    }
    tmd = ASSET_TMD(asset);
    if (clip >= KF_ANIMATION_CLIP_STATIC_OBJECT_FIRST) {
        return TMD_OBJECTS(tmd)[KF_ENUM_ENCODE(s32, clip) & KF_ASSET_OBJECT_INDEX_MASK].vertex_count;
    }
    return TMD_OBJECTS(tmd)[0].vertex_count;
}

/*
 * The twelve-entry pool that caches per-instance vertex allocations across
 * frames. Records advance free -> live and are marked stale each frame so the
 * render pass can revalidate them before pool_release_stale frees the rest.
 */
ADDRESS(0x8003537c, 0x30)
void pool_reset(void)
{
    KfPoolRecord *record = game_graphics_runtime.pool_records;
    u16 records_left = KF_ANIMATION_CACHE_CAPACITY;

    do {
        record->state = KF_ANIMATION_CACHE_FREE;
        record->cached_vertices = NULL;
        record++;
    } while (--records_left != 0);
}

ADDRESS(0x800353ac, 0x3c)
void pool_mark_allocated(void)
{
    KfPoolRecord *record = game_graphics_runtime.pool_records;
    u16 records_left = KF_ANIMATION_CACHE_CAPACITY;

    do {
        if (record->state != KF_ANIMATION_CACHE_FREE) {
            record->state = KF_ANIMATION_CACHE_STALE;
        }
        record++;
    } while (--records_left != 0);
}

ADDRESS(0x800353e8, 0x48)
void pool_record_release(KfPoolRecord *record)
{
    record->state = KF_ANIMATION_CACHE_FREE;
    *record->owner_slot = NULL;
    if (record->cached_vertices != NULL) {
        free((void *)record->cached_vertices);
        record->cached_vertices = NULL;
    }
}

ADDRESS(0x80035430, 0x6c)
void pool_release_all(void)
{
    KfPoolRecord *record = game_graphics_runtime.pool_records;
    s16 records_left;

    for (records_left = KF_ANIMATION_CACHE_CAPACITY - 1; records_left != -1; records_left--) {
        if (record->state != KF_ANIMATION_CACHE_FREE) {
            pool_record_release(record);
        }
        record++;
    }
}

ADDRESS(0x8003549c, 0x6c)
void pool_release_stale(void)
{
    KfPoolRecord *record = game_graphics_runtime.pool_records;
    u16 records_left = KF_ANIMATION_CACHE_CAPACITY;

    do {
        if (record->state == KF_ANIMATION_CACHE_STALE) {
            pool_record_release(record);
        }
        record++;
    } while (--records_left != 0);
}

ADDRESS(0x80035508, 0x48)
KfPoolRecord *pool_allocate(void)
{
    KfPoolRecord *record = game_graphics_runtime.pool_records;
    u16 records_left = KF_ANIMATION_CACHE_CAPACITY;

    do {
        if (record->state == KF_ANIMATION_CACHE_FREE) {
            record->clip_index = KF_ANIMATION_CLIP_NONE;
            return record;
        }
        record++;
    } while (--records_left != 0);
    return NULL;
}

enum { KF_MAP_PLACED_RANDOM_SHIFT = 15 };

ADDRESS(0x80035550, 0x134)
void map_placed_expand_sources(const KfMapPlacedSource *sources)
{
    KfMapPlacedEntry *entry = game_graphics_runtime.map_placed_entries;
    s32 remaining;

    for (remaining = KF_MAP_PLACED_ENTRY_COUNT - 1; remaining != -1; --remaining) {
        if (sources->model_index != KF_MAP_PLACED_NONE) {
            entry->model_index = sources->model_index;
            entry->layer_mask = sources->layer_mask;
            entry->frame_count = sources->frame_count;
            entry->frame_period = sources->frame_period;
            entry->position.vx = (sources->region_x << KF_MAP_CELL_POSITION_SHIFT) + sources->local_x;
            entry->position.vz = (sources->region_z << KF_MAP_CELL_POSITION_SHIFT) + sources->local_z;
            entry->position.vy = collision_sample_map_layer_height(entry->layer_mask, entry->position.vx, entry->position.vz, 0, 0)
                + sources->height_offset;
            entry->frame_index = (rand() * entry->frame_count) >> KF_MAP_PLACED_RANDOM_SHIFT;
        } else {
            entry->model_index = KF_MAP_PLACED_NONE;
        }
        entry++;
        sources++;
    }
}

ADDRESS(0x80035684, 0x70)
void tim_upload_images(u8 *tim_data)
{
    TIM_IMAGE image;

    OpenTIM((u_long *)tim_data);
    while (ReadTIM(&image) != NULL) {
        if (image.caddr != NULL) {
            LoadImage(image.crect, image.caddr);
        }
        if (image.paddr != NULL) {
            LoadImage(image.prect, image.paddr);
        }
    }
}

#define MENU_FADE_NEXT_QUAD() { \
    quad = (POLY_FT4 *)game_graphics_runtime.display_state.primitive_buffer->cursor; \
    game_graphics_runtime.display_state.primitive_buffer->cursor += sizeof(POLY_FT4); \
    if (game_graphics_runtime.display_state.primitive_buffer->cursor > \
        game_graphics_runtime.display_state.primitive_buffer->end) \
        goto present; \
    SetPolyFT4(quad); \
}

enum {
    KF_MENU_FADE_WAIT_FOR_RELEASE = -1,
    KF_MENU_FADE_WAIT_FOR_PRESS = -2,
    KF_MENU_FADE_FIRST_VRAM_SLICE_WIDTH = 192
};

ADDRESS(0x800356f4, 0x454)
s32 menu_fade_transition(s32 level, s32 step)
{
    POLY_FT4 *quad;
    s32 state = KF_MENU_FADE_WAIT_FOR_RELEASE;
    s32 shade;
    u32 buttons;

    for (;;) {
        display_begin_frame();
        shade = 0x80 - (level >> 1);

        MENU_FADE_NEXT_QUAD();
        setXYWH(quad, 0, 0, KF_MENU_FADE_FIRST_VRAM_SLICE_WIDTH, KF_DISPLAY_HEIGHT);
        setUVWH(quad, 0, 0, KF_MENU_FADE_FIRST_VRAM_SLICE_WIDTH, KF_DISPLAY_HEIGHT);
        quad->clut = 0;
        setTPage(quad, 2, 0, 320, 0);
        setRGB0(quad, shade, shade, shade);
        AddPrim((void *)(game_graphics_runtime.display_state.ordering_table + 2), (void *)quad);

        MENU_FADE_NEXT_QUAD();
        setXYWH(quad, KF_MENU_FADE_FIRST_VRAM_SLICE_WIDTH, 0,
            KF_DISPLAY_WIDTH - KF_MENU_FADE_FIRST_VRAM_SLICE_WIDTH, KF_DISPLAY_HEIGHT);
        setUVWH(quad, 0, 0,
            KF_DISPLAY_WIDTH - KF_MENU_FADE_FIRST_VRAM_SLICE_WIDTH, KF_DISPLAY_HEIGHT);
        quad->clut = 0;
        setTPage(quad, 2, 0, 512, 0);
        setRGB0(quad, shade, shade, shade);
        AddPrim((void *)(game_graphics_runtime.display_state.ordering_table + 2), (void *)quad);

        MENU_FADE_NEXT_QUAD();
        SetSemiTrans((void *)quad, 1);
        setXYWH(quad, 32, 112, 256, 128);
        setUVWH(quad, 0, 0, 255, 128);
        setClut(quad, 576, 511);
        setTPage(quad, 0, 1, 960, 256);
        setRGB0(quad, level, level, level);
        AddPrim((void *)game_graphics_runtime.display_state.ordering_table, (void *)quad);

        MENU_FADE_NEXT_QUAD();
        SetSemiTrans((void *)quad, 1);
        setXYWH(quad, 32, 112, 256, 128);
        setUVWH(quad, 0, 0, 255, 128);
        setClut(quad, 576, 511);
        setTPage(quad, 0, 2, 960, 256);
        setRGB0(quad, level, level, level);
        AddPrim((void *)(game_graphics_runtime.display_state.ordering_table + 1), (void *)quad);

present:
        DrawSync(0);
        display_present_frame();
        level += step;
        if (level <= 0 || level >= 120) {
            break;
        }
        buttons = PadRead(1);
        if (state == KF_MENU_FADE_WAIT_FOR_RELEASE) {
            if (buttons == 0) {
                state = KF_MENU_FADE_WAIT_FOR_PRESS;
            }
        } else if (buttons != 0) {
            DrawSync(0);
            return level;
        }
    }
    DrawSync(0);
    return state;
}

#undef MENU_FADE_NEXT_QUAD

enum {
    KF_MENU_TRANSITION_FADE_STEP = 12,
    KF_MENU_TRANSITION_REVERSE_START_LEVEL = 80,
    KF_MENU_TRANSITION_IMAGE_BYTES = KF_DISPLAY_WIDTH * KF_DISPLAY_HEIGHT * sizeof(u16),
    KF_MENU_TRANSITION_PRIMITIVE_BUFFER_BYTES =
        (KF_DISPLAY_BUFFER_COUNT * KF_GAME_PRIMITIVE_BUFFER_BYTES -
         KF_MENU_TRANSITION_IMAGE_BYTES) / KF_DISPLAY_BUFFER_COUNT
};

ADDRESS(0x80035b48, 0x180)
void menu_show_transition_image(u16 archive_slot, u16 archive_entry)
{
    s32 frame;
    u32 buttons;
    u8 *scratch;

    DrawSync(0);
    cd_archive_read(archive_slot, archive_entry,
        (u_long *)game_graphics_runtime.display_state.asset_load_buffer);
    tim_upload_images(game_graphics_runtime.display_state.asset_load_buffer);
    DrawSync(0);

    scratch = game_graphics_runtime.display_state.primitive_buffers[0].start;
    scratch += KF_MENU_TRANSITION_PRIMITIVE_BUFFER_BYTES;
    game_graphics_runtime.display_state.primitive_buffers[0].end = scratch;
    game_graphics_runtime.display_state.primitive_buffers[1].start = scratch;
    scratch += KF_MENU_TRANSITION_PRIMITIVE_BUFFER_BYTES;
    game_graphics_runtime.display_state.primitive_buffers[1].end = scratch;
    StoreImage(&menu_transition_rect, (u_long *)scratch);
    DrawSync(0);
    MoveImage(&game_graphics_runtime.display_draw_environments[
            KF_ENUM_ENCODE(u8, game_graphics_runtime.display_state.buffer_index)].clip,
        menu_transition_rect.x, menu_transition_rect.y);
    DrawSync(0);

    frame = menu_fade_transition(0, KF_MENU_TRANSITION_FADE_STEP);
    if (frame < 0) {
        for (;;) {
            buttons = PadRead(1);
            if (frame == KF_MENU_FADE_WAIT_FOR_RELEASE) {
                if (buttons != 0) {
                    continue;
                }
                frame = KF_MENU_FADE_WAIT_FOR_PRESS;
                continue;
            }
            if (buttons == 0) {
                continue;
            }
            frame = KF_MENU_TRANSITION_REVERSE_START_LEVEL;
            break;
        }
    }
    menu_fade_transition(frame, -KF_MENU_TRANSITION_FADE_STEP);
    LoadImage(&menu_transition_rect,
        (u_long *)game_graphics_runtime.display_state.primitive_buffers[1].end);
    game_graphics_runtime.display_state.primitive_buffers[0].end =
        game_graphics_runtime.display_state.primitive_buffers[0].start
        + KF_GAME_PRIMITIVE_BUFFER_BYTES;
    game_graphics_runtime.display_state.primitive_buffers[1].start =
        game_graphics_runtime.display_state.primitive_buffers[0].end;
    game_graphics_runtime.display_state.primitive_buffers[1].end =
        game_graphics_runtime.display_state.primitive_buffers[1].start
        + KF_GAME_PRIMITIVE_BUFFER_BYTES;
}
