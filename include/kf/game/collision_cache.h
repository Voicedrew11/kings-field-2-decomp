#ifndef KF_GAME_COLLISION_CACHE_H
#define KF_GAME_COLLISION_CACHE_H

#include <kf/lib/bool.h>
#include <kf/game/player.h>

s32 collision_sample_map_cell_layer(s32 x, s32 y, s32 z);
KF_ENUM_PARAM(KfCollisionHitFlags, s32) collision_evaluate_shape_records(
    s32 x, s32 y, s32 z, s32 radius, s32 height);
s32 collision_probe_floor_height(s32 x, s32 y, s32 z, s32 radius, s32 height);
void collision_cache_load_hit_bounds(void);
KF_ENUM_PARAM(KfCollisionHitFlags, s32) collision_query_shapes_with_layer_sample(
    s32 x, s32 y, s32 z, s32 radius, s32 height);
b32 collision_probe_forward_shape_0x20(const VECTOR *position, const struct KfEulerAngles *angles);
KF_ENUM_PARAM(KfCollisionHitFlags, s32) collision_query_world(
    s32 x, s32 y, s32 z, s32 radius, s32 height, KF_ENUM_PARAM(KfCollisionQuery, u8) mode);
void interpolate_collision_filter_rows(u8 type0, u8 type1, u8 type2, s32 angle, u16 amount);

/* The high bit of the shape-query height argument enables floor records
 * (record kind 0x18); the low bits still carry the collision height. */
#define KF_COLLISION_HEIGHT_CHECK_FLOOR ((s32)0x80000000u)
#define KF_COLLISION_HEIGHT_CHECK_LIMIT ((s32)0x40000000u)
/* collision_evaluate_shape_records splits its height argument into these. */
#define KF_COLLISION_HEIGHT_MODE_MASK ((s32)0xf0000000u)
#define KF_COLLISION_HEIGHT_VALUE_MASK 0x0fffffff

/* Phase-one resource loading copies 0x600 words into this bank: a table of
 * shape offsets followed by the shapes. */
#define KF_COLLISION_SHAPE_BANK pl_ext_7540.shape_bank
#define KF_COLLISION_SHAPE_BANK_BYTES 0x1800

typedef struct KfCollisionShapeOffsetTable {
    u16 offsets[256];
} KfCollisionShapeOffsetTable;

typedef char kf_collision_shape_offset_table_size[
    sizeof(KfCollisionShapeOffsetTable) == 0x200 ? 1 : -1];

/* A collision shape is a radius scale, a record count and that many records,
 * each a halfword opcode followed by the operands below. Wall, slope and
 * ledge records are rotated by the cell's and their own quarter turns.
 * FLOOR/CEILING bound the column; BASE_FLOOR and UPPER_BOUND feed the cached
 * bounds and the height-mode checks. Walls test one plane, either of two
 * (OUTER_CORNER), both (INNER_CORNER) or a diagonal; STAIRS step by run and
 * RAMP slopes along the diagonal; LEDGE follows an axis hit; OTHER_LAYER
 * evaluates the cell's second layer once. */
KF_ENUM_BEGIN(KfShapeRecordKind, s16)
    KF_SHAPE_RECORD_FLOOR = 0x10,
    KF_SHAPE_RECORD_CEILING = 0x11,
    KF_SHAPE_RECORD_BASE_FLOOR = 0x18,
    KF_SHAPE_RECORD_UPPER_BOUND = 0x19,
    KF_SHAPE_RECORD_WALL = 0x20,
    KF_SHAPE_RECORD_OUTER_CORNER_WALL = 0x21,
    KF_SHAPE_RECORD_INNER_CORNER_WALL = 0x22,
    KF_SHAPE_RECORD_DIAGONAL_WALL = 0x23,
    KF_SHAPE_RECORD_STAIRS = 0x30,
    KF_SHAPE_RECORD_LEDGE = 0x31,
    KF_SHAPE_RECORD_RAMP = 0x32,
    KF_SHAPE_RECORD_OTHER_LAYER = 0x40
KF_ENUM_END(KfShapeRecordKind)

typedef struct KfShapeHeightRecord {
    s16 height;
} KfShapeHeightRecord;

typedef struct KfShapeCeilingRecord {
    s16 limit;
    s16 floor;
} KfShapeCeilingRecord;

typedef struct KfShapeWallRecord {
    s16 offset;
    s16 limit;
    s16 floor;
    u16 quarter_turns;
} KfShapeWallRecord;

typedef struct KfShapeSlopeRecord {
    s16 floor;
    s16 start;
    s16 end;
    u16 quarter_turns;
    s16 rise;
    s16 run;
} KfShapeSlopeRecord;

typedef struct KfShapeLedgeRecord {
    s16 start;
    s16 end;
    s16 floor;
    s16 limit;
    u16 quarter_turns;
} KfShapeLedgeRecord;

typedef char kf_shape_wall_record_size[sizeof(KfShapeWallRecord) == 8 ? 1 : -1];
typedef char kf_shape_slope_record_size[sizeof(KfShapeSlopeRecord) == 12 ? 1 : -1];
typedef char kf_shape_ledge_record_size[sizeof(KfShapeLedgeRecord) == 10 ? 1 : -1];

/* A cast view of pl_ext_7540.collision_cache. The field spelling compiles
 * to different register allocation in collision_evaluate_shape_records and
 * collision_sample_map_layer_height. */
#define KF_COLLISION_CACHE \
    (*(KfCollisionCache *)((u8 *)&pl_ext_7540 + 0x11800))
#define KF_COLLISION_CACHE_CELL KF_COLLISION_CACHE.cell
#define KF_COLLISION_CACHE_SHAPE KF_COLLISION_CACHE.shape
#define KF_COLLISION_CACHE_LAYER KF_COLLISION_CACHE.layer
#define KF_COLLISION_CACHE_HEIGHT KF_COLLISION_CACHE.heights.height
#define KF_COLLISION_CACHE_RESULT KF_COLLISION_CACHE.heights.result
#define KF_COLLISION_CACHE_HEIGHT_LIMIT KF_COLLISION_CACHE.heights.height_limit
#define KF_COLLISION_CACHE_LOWER_BOUND KF_COLLISION_CACHE.heights.lower_bound
#define KF_COLLISION_CACHE_UPPER_BOUND KF_COLLISION_CACHE.heights.upper_bound
#define KF_COLLISION_CACHE_FLAGS KF_COLLISION_CACHE.flags
#define KF_COLLISION_CACHE_ACTOR_INDEX KF_COLLISION_CACHE.actor_index
#define KF_COLLISION_CACHE_OBJECT_INDEX KF_COLLISION_CACHE.object_index
#define KF_COLLISION_CACHE_POSITION KF_COLLISION_CACHE.position
#define KF_COLLISION_CACHE_RADIUS KF_COLLISION_CACHE.radius
#define KF_COLLISION_CACHE_INTERACTION_HEIGHT KF_COLLISION_CACHE.interaction_height

#endif
