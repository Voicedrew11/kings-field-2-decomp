#include <kf/lib/address.h>
#include <kf/lib/math.h>
#include <kf/game/actor.h>
#include <kf/game/collision_cache.h>
#include <kf/lib/quarter_turn.h>
#include <kf/game/graphics.h>
#include <kf/game/map_cell.h>
#include <kf/game/map_object.h>
#include <kf/game/render_mask.h>
#include <psyq/sdk.h>

/* Lower the query floor to FLOOR, reporting FLAG when the probe is below it. */
#define KF_SHAPE_LOWER_FLOOR(heights, floor, y, flags, flag) do { \
    if ((floor) < (heights)->result) {                         \
        (heights)->result = (floor);                           \
        if ((floor) < (y)) {                                   \
            (flags) |= (flag);                                 \
        }                                                      \
    }                                                          \
} while (0)

RODATA(0x8001134c, 0xc4)

ADDRESS(0x8002b7d0, 0xb60)
KF_ENUM_PARAM(KfCollisionHitFlags, s32) collision_evaluate_shape_records(s32 x, s32 y, s32 z,
                                                                   s32 radius, s32 height)
{
    KfCollisionHitFlags flags;
    b32 base_floor_hit;
    s32 base_ceiling_hit;
    b32 other_layer_visited;
    KfMapOccupancyLayer *layer;
    s16 *record;
    s32 records_left;
    s32 bottom;
    KfCollisionHeights *heights;
    s32 cell_x;
    s32 cell_z;
    s16 local_x;
    s16 local_z;
    u32 height_mode;
    KF_ENUM_PARAM(KfCollisionHitFlags, s32) wall_flags;
    s32 limit;
    s32 floor;
    s32 step;
    KfShapeWallRecord *wall;
    KfShapeSlopeRecord *slope;

    flags = KF_COLLISION_HIT_NONE;
    base_floor_hit = KF_FALSE;
    /* No record sets this; ceiling records are always evaluated. */
    base_ceiling_hit = 0;
    other_layer_visited = KF_FALSE;
    KF_COLLISION_CACHE_RESULT = 100000;
    KF_COLLISION_CACHE_HEIGHT_LIMIT = KF_COLLISION_CACHE_HEIGHT - 40000;
    KF_COLLISION_CACHE_LOWER_BOUND = 100000;
    KF_COLLISION_CACHE_UPPER_BOUND = 100000;
    layer = (KfMapOccupancyLayer *)
        ((u8 *)KF_COLLISION_CACHE_CELL + KF_COLLISION_CACHE_LAYER);
    KF_COLLISION_CACHE_SHAPE = layer;
    height_mode = height & KF_COLLISION_HEIGHT_MODE_MASK;
    height &= KF_COLLISION_HEIGHT_VALUE_MASK;

next_layer:
    record = (s16 *)(KF_COLLISION_SHAPE_BANK +
        ((KfCollisionShapeOffsetTable *)KF_COLLISION_SHAPE_BANK)
            ->offsets[layer->collision_shape_id]);
    radius = radius * *record++ >> 12;
    bottom = y - height;
    records_left = *record++;
    while (--records_left != -1) {
        heights = &KF_COLLISION_CACHE.heights;
        cell_x = x & KF_MAP_CELL_LOCAL_MASK;
        cell_z = z & KF_MAP_CELL_LOCAL_MASK;
        local_x = x & KF_MAP_CELL_LOCAL_MASK;
        local_z = z & KF_MAP_CELL_LOCAL_MASK;
        switch (KF_ENUM_DECODE(KfShapeRecordKind, *record++)) {
        case KF_SHAPE_RECORD_FLOOR: {
            KfShapeHeightRecord *level = (KfShapeHeightRecord *)record;

            record = (s16 *)(level + 1);
            if (!base_floor_hit) {
                if (level->height + heights->height < heights->result) {
                    heights->result = level->height + heights->height;
                }
                if (heights->result < y) {
                    flags |= KF_COLLISION_HIT_FLOOR;
                }
            }
            break;
        }
        case KF_SHAPE_RECORD_CEILING: {
            KfShapeCeilingRecord *ceiling = (KfShapeCeilingRecord *)record;
            s32 top;

            record = (s16 *)(ceiling + 1);
            if (!base_ceiling_hit) {
                top = ceiling->limit + heights->height;
                if (bottom < top) {
                    floor = ceiling->floor + heights->height;
                    if (floor < bottom) {
                        flags |= KF_COLLISION_HIT_HEIGHT_LIMIT;
                    } else {
                        if (floor < heights->result) {
                            heights->result = floor;
                        }
                        heights->height_limit = -100000;
                        break;
                    }
                }
                KF_COLLISION_CACHE_HEIGHT_LIMIT = top;
            }
            break;
        }
        case KF_SHAPE_RECORD_WALL:
            wall = (KfShapeWallRecord *)record;
            record = (s16 *)(wall + 1);
            wall_flags = KF_COLLISION_HIT_FLOOR | KF_COLLISION_HIT_AXIS;
            switch (KF_ENUM_DECODE(KfQuarterTurn, (layer->quarter_turns + wall->quarter_turns) & KF_MAP_CELL_QUARTER_TURN_MASK)) {
            case KF_QUARTER_TURN_0:
                if (wall->offset + radius < cell_x) {
                    break;
                }
            wall_hit:
                limit = wall->limit;
                limit += heights->height;
                floor = wall->floor + heights->height;
                if (bottom < limit) {
                    KF_SHAPE_LOWER_FLOOR(heights, floor, y, flags, wall_flags);
                } else {
                    heights->height_limit = limit;
                }
                break;
            case KF_QUARTER_TURN_1:
                if (KF_MAP_CELL_SIZE - wall->offset - radius <= cell_z) {
                    goto wall_hit;
                }
                break;
            case KF_QUARTER_TURN_2:
                if (KF_MAP_CELL_SIZE - wall->offset - radius <= cell_x) {
                    goto wall_hit;
                }
                break;
            case KF_QUARTER_TURN_3:
                if (cell_z <= wall->offset + radius) {
                    goto wall_hit;
                }
                break;
            }
            break;
        case KF_SHAPE_RECORD_OUTER_CORNER_WALL:
            wall = (KfShapeWallRecord *)record;
            record = (s16 *)(wall + 1);
            wall_flags = KF_COLLISION_HIT_FLOOR | KF_COLLISION_HIT_AXIS;
            switch (KF_ENUM_DECODE(KfQuarterTurn, (layer->quarter_turns + wall->quarter_turns) & KF_MAP_CELL_QUARTER_TURN_MASK)) {
            case KF_QUARTER_TURN_0:
                if (cell_x <= wall->offset + radius
                    || KF_MAP_CELL_SIZE - wall->offset - radius <= cell_z) {
                    goto wall_hit;
                }
                break;
            case KF_QUARTER_TURN_1:
                if (KF_MAP_CELL_SIZE - wall->offset - radius <= cell_z
                    || KF_MAP_CELL_SIZE - wall->offset - radius <= cell_x) {
                    goto wall_hit;
                }
                break;
            case KF_QUARTER_TURN_2:
                if (KF_MAP_CELL_SIZE - wall->offset - radius <= cell_x
                    || cell_z <= wall->offset + radius) {
                    goto wall_hit;
                }
                break;
            case KF_QUARTER_TURN_3:
                if (cell_z <= wall->offset + radius
                    || cell_x <= wall->offset + radius) {
                    goto wall_hit;
                }
                break;
            }
            break;
        case KF_SHAPE_RECORD_INNER_CORNER_WALL:
            wall = (KfShapeWallRecord *)record;
            record = (s16 *)(wall + 1);
            wall_flags = KF_COLLISION_HIT_FLOOR | KF_COLLISION_HIT_AXIS;
            switch (KF_ENUM_DECODE(KfQuarterTurn, (layer->quarter_turns + wall->quarter_turns) & KF_MAP_CELL_QUARTER_TURN_MASK)) {
            case KF_QUARTER_TURN_0:
                if (cell_x <= wall->offset + radius
                    && KF_MAP_CELL_SIZE - wall->offset - radius <= cell_z) {
                    goto wall_hit;
                }
                break;
            case KF_QUARTER_TURN_1:
                if (KF_MAP_CELL_SIZE - wall->offset - radius <= cell_z
                    && KF_MAP_CELL_SIZE - wall->offset - radius <= cell_x) {
                    goto wall_hit;
                }
                break;
            case KF_QUARTER_TURN_2:
                if (KF_MAP_CELL_SIZE - wall->offset - radius <= cell_x
                    && cell_z <= wall->offset + radius) {
                    goto wall_hit;
                }
                break;
            case KF_QUARTER_TURN_3:
                if (cell_z <= wall->offset + radius
                    && cell_x <= wall->offset + radius) {
                    goto wall_hit;
                }
                break;
            }
            break;
        case KF_SHAPE_RECORD_DIAGONAL_WALL: {
            s32 reach;

            wall = (KfShapeWallRecord *)record;
            record = (s16 *)(wall + 1);
            wall_flags = KF_COLLISION_HIT_FLOOR | KF_COLLISION_HIT_DIAGONAL;
            switch (KF_ENUM_DECODE(KfQuarterTurn, (layer->quarter_turns + wall->quarter_turns) & KF_MAP_CELL_QUARTER_TURN_MASK)) {
            case KF_QUARTER_TURN_0:
                reach = radius - KF_MAP_CELL_SIZE;
                if (local_x - local_z <= wall->offset + reach) {
                    goto wall_hit;
                }
                break;
            case KF_QUARTER_TURN_1:
                reach = radius - 2 * KF_MAP_CELL_SIZE;
                if (-local_x - local_z <= wall->offset + reach) {
                    goto wall_hit;
                }
                break;
            case KF_QUARTER_TURN_2:
                reach = radius - KF_MAP_CELL_SIZE;
                if (local_z - local_x <= wall->offset + reach) {
                    goto wall_hit;
                }
                break;
            case KF_QUARTER_TURN_3:
                if (local_x + local_z <= wall->offset + radius) {
                    goto wall_hit;
                }
                break;
            }
            break;
        }
        case KF_SHAPE_RECORD_STAIRS:
            slope = (KfShapeSlopeRecord *)record;
            record = (s16 *)(slope + 1);
            switch (KF_ENUM_DECODE(KfQuarterTurn, (layer->quarter_turns + slope->quarter_turns) & KF_MAP_CELL_QUARTER_TURN_MASK)) {
            case KF_QUARTER_TURN_0:
                if (local_x < slope->start - radius || slope->end + radius < local_x) {
                    break;
                }
                step = cell_z / slope->run;
            stair_floor:
                step = (step + 1) * slope->rise;
                floor = slope->floor + heights->height - step;
                KF_SHAPE_LOWER_FLOOR(heights, floor, y, flags, KF_COLLISION_HIT_FLOOR);
                break;
            case KF_QUARTER_TURN_1:
                if (KF_MAP_CELL_SIZE - slope->start + radius < local_z
                    || local_z < KF_MAP_CELL_SIZE - slope->end - radius) {
                    break;
                }
                step = cell_x / slope->run;
                goto stair_floor;
            case KF_QUARTER_TURN_2:
                if (KF_MAP_CELL_SIZE - slope->start + radius < local_x
                    || local_x < KF_MAP_CELL_SIZE - slope->end - radius) {
                    break;
                }
                step = (KF_MAP_CELL_SIZE - cell_z) / slope->run;
                goto stair_floor;
            case KF_QUARTER_TURN_3:
                if (local_z < slope->start - radius || slope->end + radius < local_z) {
                    break;
                }
                step = (KF_MAP_CELL_SIZE - cell_x) / slope->run;
                goto stair_floor;
            }
            break;
        case KF_SHAPE_RECORD_RAMP:
            slope = (KfShapeSlopeRecord *)record;
            record = (s16 *)(slope + 1);
            switch (KF_ENUM_DECODE(KfQuarterTurn, (layer->quarter_turns + slope->quarter_turns) & KF_MAP_CELL_QUARTER_TURN_MASK)) {
            case KF_QUARTER_TURN_0:
                step = KF_MAP_CELL_SIZE - local_x + local_z;
            ramp_floor:
                if (step < slope->start) {
                    step = slope->start;
                } else if (slope->end < step) {
                    step = slope->end;
                }
                step = (step - slope->start) / slope->run;
                step *= slope->rise;
                floor = slope->floor + heights->height - step;
                KF_SHAPE_LOWER_FLOOR(heights, floor, y, flags, KF_COLLISION_HIT_FLOOR);
                break;
            case KF_QUARTER_TURN_1:
                step = local_x + local_z;
                goto ramp_floor;
            case KF_QUARTER_TURN_2:
                step = KF_MAP_CELL_SIZE - local_z + local_x;
                goto ramp_floor;
            case KF_QUARTER_TURN_3:
                step = 2 * KF_MAP_CELL_SIZE - local_x - local_z;
                goto ramp_floor;
            }
            break;
        case KF_SHAPE_RECORD_LEDGE:
            /* Only an axis hit is tested, and its record is skipped only then. */
            if ((flags & KF_COLLISION_HIT_AXIS) != KF_COLLISION_HIT_NONE) {
                KfShapeLedgeRecord *ledge = (KfShapeLedgeRecord *)record;
                s16 distance;
                s16 x_distance;

                record = (s16 *)(ledge + 1);
                distance = cell_z;
                x_distance = cell_x;
                switch (KF_ENUM_DECODE(KfQuarterTurn, (layer->quarter_turns + ledge->quarter_turns) & KF_MAP_CELL_QUARTER_TURN_MASK)) {
                case KF_QUARTER_TURN_0:
                ledge_floor:
                    if (distance >= ledge->start + radius
                        && distance <= ledge->end - radius) {
                        KF_COLLISION_CACHE_RESULT =
                            ledge->floor + KF_COLLISION_CACHE_HEIGHT;
                        KF_COLLISION_CACHE_HEIGHT_LIMIT =
                            ledge->limit + KF_COLLISION_CACHE_HEIGHT;
                        if (bottom < KF_COLLISION_CACHE_HEIGHT_LIMIT) {
                            flags |= KF_COLLISION_HIT_HEIGHT_LIMIT;
                        }
                        if (y <= KF_COLLISION_CACHE_RESULT) {
                            flags &= ~(KF_COLLISION_HIT_FLOOR | KF_COLLISION_HIT_AXIS);
                        }
                    }
                    break;
                case KF_QUARTER_TURN_1:
                    distance = x_distance;
                    goto ledge_floor;
                case KF_QUARTER_TURN_2:
                    distance = KF_MAP_CELL_SIZE - cell_z;
                    goto ledge_floor;
                case KF_QUARTER_TURN_3:
                    distance = KF_MAP_CELL_SIZE - cell_x;
                    goto ledge_floor;
                }
            }
            break;
        case KF_SHAPE_RECORD_OTHER_LAYER:
            if (other_layer_visited) {
                return flags;
            }
            {
                u16 *layer_offset = &KF_COLLISION_CACHE_LAYER;

                *layer_offset = *layer_offset == 0 ? sizeof(KfMapOccupancyLayer) : 0;
                layer = (KfMapOccupancyLayer *)
                    ((u8 *)KF_COLLISION_CACHE_CELL + *layer_offset);
            }
            KF_COLLISION_CACHE_HEIGHT = -layer->elevation * (1 << KF_MAP_CELL_ELEVATION_SHIFT);
            other_layer_visited = KF_TRUE;
            goto next_layer;
        case KF_SHAPE_RECORD_BASE_FLOOR: {
            KfShapeHeightRecord *level = (KfShapeHeightRecord *)record;

            record = (s16 *)(level + 1);
            floor = level->height + KF_COLLISION_CACHE_HEIGHT;
            KF_COLLISION_CACHE_LOWER_BOUND = floor;
            if (height_mode & KF_COLLISION_HEIGHT_CHECK_FLOOR) {
                KF_COLLISION_CACHE_RESULT = floor;
                if (floor < y) {
                    flags |= KF_COLLISION_HIT_FLOOR;
                    base_floor_hit = KF_TRUE;
                }
            }
            if (height_mode & KF_COLLISION_HEIGHT_CHECK_LIMIT) {
                KF_COLLISION_CACHE_HEIGHT_LIMIT = floor;
                if (bottom <= floor) {
                    flags |= KF_COLLISION_HIT_HEIGHT_LIMIT;
                }
            }
            break;
        }
        case KF_SHAPE_RECORD_UPPER_BOUND: {
            KfShapeHeightRecord *level = (KfShapeHeightRecord *)record;

            record = (s16 *)(level + 1);
            KF_COLLISION_CACHE_UPPER_BOUND = level->height + KF_COLLISION_CACHE_HEIGHT;
            break;
        }
        }
    }
    return flags;
}

ADDRESS(0x8002c330, 0x78)
s32 collision_probe_floor_height(s32 x, s32 y, s32 z, s32 radius, s32 height)
{
    collision_sample_map_cell_layer(x, y - 1280, z);
    collision_evaluate_shape_records(x, y, z, radius, height);
    return KF_COLLISION_CACHE_RESULT;
}

ADDRESS(0x8002c3a8, 0xc0)
s32 collision_sample_map_layer_height(KfMapLayerMask layer, s32 x, s32 z, s32 radius, s32 height)
{
    KfMapOccupancyCell *cell = &bss_801c7540.map_cells
        [z >> KF_MAP_CELL_POSITION_SHIFT][x >> KF_MAP_CELL_POSITION_SHIFT];
    s32 elevation;

    if (layer == KF_MAP_LAYER_SECOND) {
        KF_COLLISION_CACHE_LAYER = sizeof(KfMapOccupancyLayer);
        elevation = -(s32)cell->layer[1].elevation;
    } else {
        KF_COLLISION_CACHE_LAYER = 0;
        elevation = -(s32)cell->layer[0].elevation;
    }
    KF_COLLISION_CACHE_HEIGHT = elevation * (1 << KF_MAP_CELL_ELEVATION_SHIFT);
    KF_COLLISION_CACHE_CELL = cell;
    collision_evaluate_shape_records(x, KF_COLLISION_CACHE_HEIGHT, z, radius, height);
    return KF_COLLISION_CACHE_RESULT;
}

ADDRESS(0x8002c468, 0xbc)
void map_cell_add_layer_occupancy(s32 x, s32 z, s32 radius, s32 amount)
{
    s32 expanded = radius + (1 << KF_MAP_CELL_POSITION_SHIFT);
    s32 first_x = (x - expanded) >> KF_MAP_CELL_POSITION_SHIFT;
    s32 width = ((x + expanded) >> KF_MAP_CELL_POSITION_SHIFT) - first_x;
    s32 first_z = (z - expanded) >> KF_MAP_CELL_POSITION_SHIFT;
    s32 height = ((z + expanded) >> KF_MAP_CELL_POSITION_SHIFT) - first_z;
    KfMapOccupancyCell *row = &bss_801c7540.map_cells[first_z][first_x];
    u32 value = (u32)amount << 2;
    KfMapOccupancyCell *cell;
    s32 col;
    s32 remaining;
    s32 row_index = first_z;

    do {
        cell = row;
        row += KF_MAP_WORLD_GRID_SIDE;
        if ((u32)row_index < KF_MAP_WORLD_GRID_SIDE) {
            col = first_x;
            remaining = width;
            do {
                if ((u32)col < KF_MAP_WORLD_GRID_SIDE) {
                    cell->layer[0].quarter_turns =
                        value + cell->layer[0].quarter_turns;
                }
                col++;
                cell++;
                remaining--;
            } while (remaining != -1);
        }
        row_index++;
        height--;
    } while (height != -1);
}

ADDRESS(0x8002c524, 0x7c)
KF_ENUM_PARAM(KfCollisionHitFlags, s32) collision_query_shapes_with_layer_sample(s32 x, s32 y, s32 z,
                                                                           s32 radius, s32 height)
{
    collision_sample_map_cell_layer(x, y - (((u32)height << 4) >> 5), z);
    return collision_evaluate_shape_records(x, y, z, radius, height);
}

ADDRESS(0x8002c5a0, 0x160)
void collision_cache_load_hit_bounds(void)
{
    if ((KF_COLLISION_CACHE_FLAGS & KF_COLLISION_HIT_PLAYER) != KF_COLLISION_HIT_NONE) {
        KF_COLLISION_CACHE_POSITION = player_state.camera_position;
        KF_COLLISION_CACHE_RADIUS = 800;
        KF_COLLISION_CACHE_INTERACTION_HEIGHT = KF_PLAYER_HEIGHT;
    } else if (KF_COLLISION_CACHE_ACTOR_INDEX != KF_ACTOR_INDEX_NONE) {
        KfActor *actor = &actor_state.actors[KF_COLLISION_CACHE_ACTOR_INDEX];
        KF_COLLISION_CACHE_POSITION = actor->position;
        KF_COLLISION_CACHE_RADIUS = actor->collision_radius;
        KF_COLLISION_CACHE_INTERACTION_HEIGHT = actor->collision_height;
    } else {
        KfMapObject *object;
        KfMapObjectTemplate *object_template;

        if (KF_COLLISION_CACHE_OBJECT_INDEX == -1) {
            return;
        }
        object = &map_object_state.objects[KF_COLLISION_CACHE_OBJECT_INDEX];
        object_template = &map_object_state.templates[KF_ENUM_ENCODE(u16, object->object_id)];
        KF_COLLISION_CACHE_POSITION = object->position;
        KF_COLLISION_CACHE_RADIUS = object_template->collision_radius;
        KF_COLLISION_CACHE_INTERACTION_HEIGHT = object_template->interaction_height;
    }
}

ADDRESS(0x8002c700, 0x244)
KF_ENUM_PARAM(KfCollisionHitFlags, s32) collision_query_world(s32 x, s32 y, s32 z, s32 radius,
                                                              s32 height,
                                                              KF_ENUM_PARAM(KfCollisionQuery, u8) mode)
{
    KF_ENUM_PARAM(KfCollisionHitFlags, s32) result = KF_COLLISION_HIT_NONE;

    if ((mode & KF_COLLISION_QUERY_SHAPES) != KF_COLLISION_QUERY_NONE) {
        result = collision_query_shapes_with_layer_sample(x, y, z, radius, height);
        if ((mode & KF_COLLISION_QUERY_LAYER_FLAG_40) != KF_COLLISION_QUERY_NONE &&
            (KF_COLLISION_CACHE_SHAPE->lighting_index & KF_MAP_CELL_LAYER_COLLISION_FLAG_40)) {
            KF_COLLISION_CACHE_RESULT = -100000;
            result |= KF_COLLISION_HIT_AXIS;
        }
    } else {
        KF_COLLISION_CACHE_CELL = &bss_801c7540.map_cells
            [z >> KF_MAP_CELL_POSITION_SHIFT][x >> KF_MAP_CELL_POSITION_SHIFT];
    }

    height &= KF_COLLISION_HEIGHT_VALUE_MASK;
    if (KF_COLLISION_CACHE_CELL->layer[0].quarter_turns & 0xfc) {
        if ((mode & KF_COLLISION_QUERY_ACTORS) != KF_COLLISION_QUERY_NONE) {
            KF_COLLISION_CACHE_ACTOR_INDEX = actor_find_overlap_excluding_target_type3(x, y, z, radius, height);
            if (KF_COLLISION_CACHE_ACTOR_INDEX != KF_ACTOR_INDEX_NONE) {
                result |= KF_COLLISION_HIT_ACTOR;
            }
        } else {
            if ((mode & KF_COLLISION_QUERY_ACTORS_INCLUDE_TYPE3) != KF_COLLISION_QUERY_NONE) {
                KF_COLLISION_CACHE_ACTOR_INDEX = actor_find_overlap(x, y, z, radius, height);
                if (KF_COLLISION_CACHE_ACTOR_INDEX != KF_ACTOR_INDEX_NONE) {
                    result |= KF_COLLISION_HIT_ACTOR;
                }
            }
            KF_COLLISION_CACHE_ACTOR_INDEX = KF_ACTOR_INDEX_NONE;
        }

        if ((mode & KF_COLLISION_QUERY_MAP_OBJECTS) != KF_COLLISION_QUERY_NONE) {
            KF_COLLISION_CACHE_OBJECT_INDEX = map_object_find_collision_at_point(x, y, z, radius, height);
            if (KF_COLLISION_CACHE_OBJECT_INDEX != -1) {
                result |= KF_COLLISION_HIT_MAP_OBJECT;
            }
        } else {
            KF_COLLISION_CACHE_OBJECT_INDEX = -1;
        }

        if ((mode & KF_COLLISION_QUERY_PLAYER) != KF_COLLISION_QUERY_NONE &&
            player_distance_to_point_with_margin(x, y, z, radius, height) != KF_DISTANCE_NONE) {
            result |= KF_COLLISION_HIT_PLAYER;
        }
    } else {
        KF_COLLISION_CACHE_ACTOR_INDEX = KF_ACTOR_INDEX_NONE;
        KF_COLLISION_CACHE_OBJECT_INDEX = -1;
    }

    KF_COLLISION_CACHE_FLAGS = result;
    return result;
}

ADDRESS(0x8002c944, 0x130)
void reset_collision_rows_and_overlay(void)
{
    KfCollisionDefaultRow *defaults = collision_default_rows;
    KfCollisionRow *row = game_graphics_runtime.collision_rows;
    s32 index = KF_COLLISION_ROW_COUNT - 1;

    do {
        row->motion = defaults->motion;
        row->rotations[0] = defaults->rotation;
        row->filter.kinds = defaults->filter.kinds;
        row->filter.angle = defaults->filter.angle;
        defaults++;
        row++;
        index--;
    } while (index != -1);

    game_graphics_runtime.collision_rotation_dirty = KF_FALSE;
    game_graphics_runtime.color_overlay_sample_count = 0;
    game_graphics_runtime.color_overlay_blue_sum = 0;
    game_graphics_runtime.color_overlay_green_sum = 0;
    game_graphics_runtime.color_overlay_red_sum = 0;
    game_graphics_runtime.color_overlay_control = KF_COLOR_OVERLAY_OFF;
}

ADDRESS(0x8002ca74, 0x80)
void refresh_collision_row_rotations(void)
{
    KfCollisionRow *row;
    s32 index;

    if (game_graphics_runtime.collision_rotation_dirty) {
        row = game_graphics_runtime.collision_rows;
        index = KF_COLLISION_ROW_COUNT - 1;
        do {
            matrix_rotate_quarter_turns((MATRIX *)&row->rotations[0],
                                        (MATRIX *)&row->rotations[1], KF_QUARTER_TURN_1);
            matrix_rotate_quarter_turns((MATRIX *)&row->rotations[0],
                                        (MATRIX *)&row->rotations[2], KF_QUARTER_TURN_2);
            matrix_rotate_quarter_turns((MATRIX *)&row->rotations[0],
                                        (MATRIX *)&row->rotations[3], KF_QUARTER_TURN_3);
            index--;
            row++;
        } while (index != -1);
    }
}

ADDRESS(0x8002caf4, 0xe0)
void interpolate_collision_row_fields(KF_ENUM_PARAM(KfCollisionRowFields, s32) flags,
    const KfCollisionFilterPayload *payload,
                   KfCollisionRow *row, s32 amount)
{
    if ((flags & KF_COLLISION_ROW_MOTION) != KF_COLLISION_ROW_NONE) {
        fixed_lerp_nine_halfwords_q12(row->motion.values, payload->motion.values,
                      row->motion.values, amount);
    }
    if ((flags & KF_COLLISION_ROW_ROTATION) != KF_COLLISION_ROW_NONE) {
        fixed_lerp_nine_halfwords_q12(row->rotations[0].m[0], payload->rotation.m[0],
                      row->rotations[0].m[0], amount);
    }
    if ((flags & KF_COLLISION_ROW_FILTER_TYPES) != KF_COLLISION_ROW_NONE) {
        row->filter.kinds.types[0] = fixed_lerp_q12(
            row->filter.kinds.types[0], payload->filter.kinds.types[0], amount);
        row->filter.kinds.types[1] = fixed_lerp_q12(
            row->filter.kinds.types[1], payload->filter.kinds.types[1], amount);
        row->filter.kinds.types[2] = fixed_lerp_q12(
            row->filter.kinds.types[2], payload->filter.kinds.types[2], amount);
    }
    if ((flags & KF_COLLISION_ROW_FILTER_ANGLE) != KF_COLLISION_ROW_NONE) {
        row->filter.angle = fixed_lerp_q12(row->filter.angle, payload->filter.angle, amount);
    }
}

ADDRESS(0x8002cbd4, 0x9c)
void interpolate_collision_rows(KF_ENUM_PARAM(KfCollisionRowFields, s32) flags,
    const KfCollisionFilterPayload *payload, s32 amount)
{
    KfCollisionRow *row = game_graphics_runtime.collision_rows;
    s32 index;

    for (index = 0; index < 62; index++, row++) {
        if (index != 38) {
            interpolate_collision_row_fields(flags, payload, row, amount);
        }
    }
    if ((flags & KF_COLLISION_ROW_ROTATION) != KF_COLLISION_ROW_NONE) {
        game_graphics_runtime.collision_rotation_dirty = KF_TRUE;
    }
}

ADDRESS(0x8002cc70, 0x74)
void interpolate_collision_filter_rows(u8 type0, u8 type1, u8 type2, s32 angle, u16 amount)
{
    KfCollisionFilterPayload payload;
    KfCollisionRowFields flags = KF_COLLISION_ROW_NONE;

    if (type0 != 0xff || type1 != type0 || type2 != type1) {
        payload.filter.kinds.types[0] = type0;
        payload.filter.kinds.types[1] = type1;
        payload.filter.kinds.types[2] = type2;
        flags |= KF_COLLISION_ROW_FILTER_TYPES;
    }
    if (angle != -1) {
        payload.filter.angle = angle;
        flags |= KF_COLLISION_ROW_FILTER_ANGLE;
    }
    interpolate_collision_rows(flags, &payload, (s16)amount);
}

ADDRESS(0x8002cce4, 0x28)
void clear_map_cell_layer_masks(void)
{
    u32 *mask = (u32 *)game_graphics_runtime.render_grid.map_cell_layer_masks;
    s32 index = 143;

    do {
        *mask = 0;
        index--;
        mask++;
    } while (index != -1);
}

ADDRESS(0x8002cd0c, 0x19c)
void rasterize_map_cell_layer_mask_line(const KfCollisionMaskPoint *start,
                   const KfCollisionMaskPoint *end, KfMapLayerMask value)
{
    /* The rasterizer reads 16-bit origins and steps 16-bit grid coordinates. */
    u16 x = ((u32)start->x >> 12) + (u16)game_graphics_runtime.render_state.cell_origin_x;
    u16 z = ((u32)start->z >> 12) + (u16)game_graphics_runtime.render_state.cell_origin_z;
    s16 dx = (((u32)end->x >> 12) + (u16)game_graphics_runtime.render_state.cell_origin_x) - x;
    s16 dz = (((u32)end->z >> 12) + (u16)game_graphics_runtime.render_state.cell_origin_z) - z;
    s16 step_x;
    s16 step_z;
    s16 count;
    s16 error;

    if (dx < 0) {
        dx = -dx;
        step_x = -1;
    } else {
        step_x = 1;
    }
    step_z = 1;
    if (dz < 0) {
        dz = -dz;
        step_z = -1;
    }

    if (dx >= dz) {
        error = dx >> 1;
        count = dx;
        do {
            if (x < KF_MAP_CELL_GRID_SIDE && z < KF_MAP_CELL_GRID_SIDE) {
                game_graphics_runtime.render_grid.map_cell_layer_masks[z][x] = value;
            }
            error -= dz;
            if (error <= 0) {
                z += step_z;
                error += dx;
            }
            x += step_x;
            count--;
        } while (count >= 0);
    } else {
        error = dz >> 1;
        count = dz;
        do {
            if (x < KF_MAP_CELL_GRID_SIDE && z < KF_MAP_CELL_GRID_SIDE) {
                game_graphics_runtime.render_grid.map_cell_layer_masks[z][x] = value;
            }
            error -= dx;
            if (error <= 0) {
                x += step_x;
                error += dz;
            }
            z += step_z;
            count--;
        } while (count >= 0);
    }
}

ADDRESS(0x8002cea8, 0x64)
s32 find_map_cell_layer_mask_run_boundary(const KfMapLayerMask *row, s32 index, s32 step,
                   KfMapLayerMask value)
{
    KF_ENUM_STORAGE(KfMaskRunScan, s32) state = KF_MASK_RUN_SEEK;

    for (;;) {
        KfMapLayerMask current;
        if ((u32)index >= KF_MAP_CELL_GRID_SIDE) {
            return index;
        }
        current = row[index];
        switch (state) {
        case KF_MASK_RUN_SEEK:
            if (current == value) {
                state = KF_MASK_RUN_INSIDE;
            }
            break;
        case KF_MASK_RUN_INSIDE:
            if (current != value) {
                return index;
            }
            break;
        }
        index += step;
    }
}

ADDRESS(0x8002cf0c, 0xbc)
void fill_map_cell_layer_mask_interior(KfMapLayerMask value)
{
    KfMapLayerMask *row = &game_graphics_runtime.render_grid.map_cell_layer_masks[0][0];
    s32 row_index = KF_MAP_CELL_GRID_SIDE - 1;

    do {
        s32 first = find_map_cell_layer_mask_run_boundary(row, 0, 1, value);
        s32 last = find_map_cell_layer_mask_run_boundary(row, KF_MAP_CELL_GRID_SIDE - 1, -1, value);

        if (last >= first) {
            KfMapLayerMask *cell = row + first;
            s32 count = last - first;
            do {
                *cell = value;
                count--;
                cell++;
            } while (count != -1);
        }
        row_index--;
        row += KF_MAP_CELL_GRID_SIDE;
    } while (row_index != -1);
}

ADDRESS(0x8002cfc8, 0x194)
void update_current_map_cell_layer_mask(s32 cursor_offset)
{
    KfMapOccupancyCell *cell;
    KfMapOccupancyLayer *first_layer;
    KfMapOccupancyLayer *second_layer;
    KfMapLayerMask *mask;
    KfMapLayerMask value;

    if ((u32)render_mask_scan_state.window_x >= KF_MAP_CELL_GRID_SIDE ||
        (u32)render_mask_scan_state.window_z >= KF_MAP_CELL_GRID_SIDE) {
        return;
    }
    mask = render_mask_scan_state.mask_cursor;
    if (*mask == KF_MAP_LAYER_NONE) {
        return;
    }
    if ((u32)render_mask_scan_state.map_x < KF_MAP_WORLD_GRID_SIDE &&
        (u32)render_mask_scan_state.map_z < KF_MAP_WORLD_GRID_SIDE) {
        value = mask[cursor_offset];
        cell = &bss_801c7540.map_cells[render_mask_scan_state.map_z]
                                            [render_mask_scan_state.map_x];
        first_layer = (KfMapOccupancyLayer *)((u8 *)cell +
                          render_mask_scan_state.first_layer_byte_offset);
        if ((value & render_mask_scan_state.first_layer_mask) != KF_MAP_LAYER_NONE) {
            goto check_first_layer;
        }
clear_first_layer:
        *render_mask_scan_state.mask_cursor &=
            ~render_mask_scan_state.first_layer_mask;
        goto check_second_layer;
check_first_layer:
        if (first_layer->object_index == 0xff) {
            goto clear_first_layer;
        }
        if (!(first_layer->lighting_index & KF_MAP_CELL_LAYER_REVEALS_OTHER_LAYER)) {
            goto check_second_layer;
        }
set_second_layer:
        *render_mask_scan_state.mask_cursor |=
            render_mask_scan_state.second_layer_mask;
        return;
check_second_layer:
        second_layer = (KfMapOccupancyLayer *)((u8 *)cell +
                           render_mask_scan_state.second_layer_byte_offset);
        if (second_layer->object_index != 0xff &&
            (value & render_mask_scan_state.second_layer_mask) != KF_MAP_LAYER_NONE) {
            goto set_second_layer;
        }
        return;
    }
    *render_mask_scan_state.mask_cursor = KF_MAP_LAYER_NONE;
}

ADDRESS(0x8002d15c, 0x24c)
void sweep_map_cell_layer_mask_line(s32 first_offset, s32 second_offset, s32 map_step,
                   s8 window_step, s32 mask_stride, s32 count)
{
    s32 window_x = render_mask_scan_state.window_x;
    s32 window_z = render_mask_scan_state.window_z;
    s32 map_x = render_mask_scan_state.map_x;
    s32 map_z = render_mask_scan_state.map_z;
    KfMapLayerMask *cursor = render_mask_scan_state.mask_cursor;

    count--;
    if (count != -1) {
        do {
            if ((u32)window_x < KF_MAP_CELL_GRID_SIDE && (u32)window_z < KF_MAP_CELL_GRID_SIDE &&
                *cursor != KF_MAP_LAYER_NONE) {
                if ((u32)map_x < KF_MAP_WORLD_GRID_SIDE && (u32)map_z < KF_MAP_WORLD_GRID_SIDE) {
                    KfMapLayerMask first = cursor[first_offset];
                    KfMapLayerMask second = cursor[second_offset];
                    KfMapOccupancyCell *cell;
                    KfMapOccupancyLayer *first_layer;

                    if ((first & render_mask_scan_state.first_layer_mask) != KF_MAP_LAYER_NONE) {
                        goto check_first_layer;
                    }
                    if ((second & render_mask_scan_state.first_layer_mask) != KF_MAP_LAYER_NONE) {
                        goto check_first_layer;
                    }
                    cell = &bss_801c7540.map_cells[map_z][map_x];
    clear_first_layer:
                    *cursor &= ~render_mask_scan_state.first_layer_mask;
                    goto check_second_layer;
    check_first_layer:
                    cell = &bss_801c7540.map_cells[map_z][map_x];
                    first_layer = (KfMapOccupancyLayer *)((u8 *)cell +
                                  render_mask_scan_state.first_layer_byte_offset);
                    if (first_layer->object_index == 0xff) {
                        goto clear_first_layer;
                    }
                    if (!(first_layer->lighting_index & KF_MAP_CELL_LAYER_REVEALS_OTHER_LAYER)) {
                        goto check_second_layer;
                    }
    set_second_layer:
                    *cursor |= render_mask_scan_state.second_layer_mask;
                    goto advance_iteration;
    check_second_layer:
                    if (((KfMapOccupancyLayer *)((u8 *)cell +
                         render_mask_scan_state.second_layer_byte_offset))->object_index != 0xff) {
                        if ((first & render_mask_scan_state.second_layer_mask) != KF_MAP_LAYER_NONE ||
                            (second & render_mask_scan_state.second_layer_mask) != KF_MAP_LAYER_NONE) {
                            goto set_second_layer;
                        }
                    }
                } else {
                    *cursor = KF_MAP_LAYER_NONE;
                }
            }
    advance_iteration:
            cursor += mask_stride;
            window_x += map_step;
            window_z += window_step;
            map_x += map_step;
            map_z += window_step;
            count--;
        } while (count != -1);
    }

    render_mask_scan_state.window_x = window_x;
    render_mask_scan_state.window_z = window_z;
    render_mask_scan_state.map_x = map_x;
    render_mask_scan_state.map_z = map_z;
    render_mask_scan_state.mask_cursor = cursor;
}



/* Moves the mask scan one cell, updating both map and window coordinates. */
#define MASK_SCAN_STEP(dx, dz) ( \
    render_mask_scan_state.map_x += (dx), \
    render_mask_scan_state.map_z += (dz), \
    render_mask_scan_state.window_x += (dx), \
    render_mask_scan_state.window_z += (dz), \
    render_mask_scan_state.mask_cursor += (dz) * KF_MAP_CELL_GRID_SIDE + (dx))

ADDRESS(0x8002d3a8, 0x7bc)
void build_camera_map_cell_layer_masks(void)
{
    s16 shape[7];
    KfCollisionMaskPoint corners[4];
    KfMapMaskShapePair *pair;
    s16 *shape_cursor;
    s32 pitch_weight;
    s32 sine;
    s32 cosine;
    s32 center_x;
    s32 center_z;
    s32 x;
    s32 z;
    s32 shape_index;
    s32 index;
    u8 *first_lighting;
    KfMapLayerMask *mask;
    s32 lighting_offset;

    pitch_weight = KF_FIXED12_ONE - rcos(game_graphics_runtime.render_state.view_rotation.vx);
    pair = map_mask_pitch_shape_pairs;
    shape_cursor = shape;
    shape_index = 6;
    do {
        *shape_cursor = (((pair->far - pair->near) * pitch_weight) >> 12) + pair->near;
        pair++;
        shape_cursor++;
    } while (--shape_index != -1);

    sine = rsin(game_graphics_runtime.render_state.view_rotation.vy);
    cosine = rcos(game_graphics_runtime.render_state.view_rotation.vy);
    clear_map_cell_layer_masks();

    render_mask_scan_state.map_x = game_graphics_runtime.render_state.view_cell_x;
    render_mask_scan_state.map_z = game_graphics_runtime.render_state.view_cell_z;
    x = (u8)(((sine * shape[0]) >> 20) + 12);
    game_graphics_runtime.render_state.cell_origin_x = x - render_mask_scan_state.map_x;
    game_graphics_runtime.render_grid.map_scan_start_x =
        -game_graphics_runtime.render_state.cell_origin_x;
    render_mask_scan_state.window_x = x;
    center_x = (s32)((u32)game_graphics_runtime.render_state.view_position.vx
                     << 1);
    z = (u8)((((-cosine) * shape[0]) >> 20) + 12);
    game_graphics_runtime.render_state.cell_origin_z = z - render_mask_scan_state.map_z;
    game_graphics_runtime.render_grid.map_scan_start_z =
        -game_graphics_runtime.render_state.cell_origin_z;
    render_mask_scan_state.window_z = z;
    mask = &game_graphics_runtime.render_grid.map_cell_layer_masks[z][x];
    render_mask_scan_state.mask_cursor = mask;

    center_z = (s32)((u32)game_graphics_runtime.render_state.view_position.vz
                     << 1);
    collision_sample_map_cell_layer(game_graphics_runtime.render_state.view_position.vx, game_graphics_runtime.render_state.view_position.vy,
                  game_graphics_runtime.render_state.view_position.vz);
    render_mask_scan_state.first_layer_byte_offset = KF_COLLISION_CACHE_LAYER;
    render_mask_scan_state.second_layer_byte_offset =
        sizeof(KfMapOccupancyLayer) - render_mask_scan_state.first_layer_byte_offset;
    if (render_mask_scan_state.first_layer_byte_offset == 0) {
        render_mask_scan_state.first_layer_mask = KF_MAP_LAYER_FIRST;
        render_mask_scan_state.second_layer_mask = KF_MAP_LAYER_SECOND;
    } else {
        render_mask_scan_state.first_layer_mask = KF_MAP_LAYER_SECOND;
        render_mask_scan_state.second_layer_mask = KF_MAP_LAYER_FIRST;
    }
    corners[0].x = ((shape[1] * cosine - shape[3] * sine) >> 8) + center_x;
    corners[0].z = ((shape[1] * sine + shape[3] * cosine) >> 8) + center_z;
    corners[1].x = ((shape[2] * cosine - shape[3] * sine) >> 8) + center_x;
    corners[1].z = ((shape[2] * sine + shape[3] * cosine) >> 8) + center_z;
    corners[2].x = ((shape[4] * cosine - shape[6] * sine) >> 8) + center_x;
    corners[2].z = ((shape[4] * sine + shape[6] * cosine) >> 8) + center_z;
    corners[3].x = ((shape[5] * cosine - shape[6] * sine) >> 8) + center_x;
    corners[3].z = ((shape[5] * sine + shape[6] * cosine) >> 8) + center_z;

    rasterize_map_cell_layer_mask_line(&corners[0], &corners[1],
                  render_mask_scan_state.first_layer_mask | KF_MAP_LAYER_IN_VIEW);
    rasterize_map_cell_layer_mask_line(&corners[1], &corners[3],
                  render_mask_scan_state.first_layer_mask | KF_MAP_LAYER_IN_VIEW);
    rasterize_map_cell_layer_mask_line(&corners[3], &corners[2],
                  render_mask_scan_state.first_layer_mask | KF_MAP_LAYER_IN_VIEW);
    rasterize_map_cell_layer_mask_line(&corners[2], &corners[0],
                  render_mask_scan_state.first_layer_mask | KF_MAP_LAYER_IN_VIEW);
    fill_map_cell_layer_mask_interior(render_mask_scan_state.first_layer_mask | KF_MAP_LAYER_IN_VIEW);

    lighting_offset = render_mask_scan_state.map_z * sizeof(bss_801c7540.map_cells[0]) +
        render_mask_scan_state.map_x * sizeof(bss_801c7540.map_cells[0][0]) +
        render_mask_scan_state.first_layer_byte_offset;
    /* The scan stores a byte offset (0 or 5) into the two-layer cell. */
    first_lighting = &((KfMapOccupancyLayer *)
        ((u8 *)bss_801c7540.map_cells + lighting_offset))->lighting_index;
    if (*first_lighting & KF_MAP_CELL_LAYER_REVEALS_OTHER_LAYER) {
        *render_mask_scan_state.mask_cursor = KF_MAP_LAYER_BOTH;
    } else {
        *render_mask_scan_state.mask_cursor = render_mask_scan_state.first_layer_mask;
    }

    for (index = 0; index < 14; index++) {
        render_mask_scan_state.map_z++;
        render_mask_scan_state.window_z++;
        render_mask_scan_state.mask_cursor += KF_MAP_CELL_GRID_SIDE;
        update_current_map_cell_layer_mask(-24);
        MASK_SCAN_STEP(-1, 0);
        sweep_map_cell_layer_mask_line(-24, -23, -1, 0, -1, index);
        update_current_map_cell_layer_mask(-23);
        MASK_SCAN_STEP(0, -1);
        sweep_map_cell_layer_mask_line(1, -23, 0, -1, -24, index);
        update_current_map_cell_layer_mask(1);
        MASK_SCAN_STEP(0, -1);
        sweep_map_cell_layer_mask_line(1, 25, 0, -1, -24, index);
        update_current_map_cell_layer_mask(25);
        MASK_SCAN_STEP(1, 0);
        sweep_map_cell_layer_mask_line(24, 25, 1, 0, 1, index);
        update_current_map_cell_layer_mask(24);
        MASK_SCAN_STEP(1, 0);
        sweep_map_cell_layer_mask_line(24, 23, 1, 0, 1, index);
        update_current_map_cell_layer_mask(23);
        MASK_SCAN_STEP(0, 1);
        sweep_map_cell_layer_mask_line(-1, 23, 0, 1, 24, index);
        update_current_map_cell_layer_mask(-1);
        MASK_SCAN_STEP(0, 1);
        sweep_map_cell_layer_mask_line(-1, -25, 0, 1, 24, index);
        update_current_map_cell_layer_mask(-25);
        MASK_SCAN_STEP(-1, 0);
        sweep_map_cell_layer_mask_line(-24, -25, -1, 0, -1, index);
    }

    mask[-25] |= KF_MAP_LAYER_NEAR_CLIPPED;
    mask[-24] |= KF_MAP_LAYER_NEAR_CLIPPED | KF_MAP_LAYER_NEAR_PREPARE;
    mask[-23] |= KF_MAP_LAYER_NEAR_CLIPPED;
    mask[-1] |= KF_MAP_LAYER_NEAR_CLIPPED | KF_MAP_LAYER_NEAR_PREPARE;
    mask[0] |= KF_MAP_LAYER_NEAR_CLIPPED | KF_MAP_LAYER_NEAR_PREPARE;
    mask[1] |= KF_MAP_LAYER_NEAR_CLIPPED | KF_MAP_LAYER_NEAR_PREPARE;
    mask[23] |= KF_MAP_LAYER_NEAR_CLIPPED;
    mask[24] |= KF_MAP_LAYER_NEAR_CLIPPED | KF_MAP_LAYER_NEAR_PREPARE;
    mask[25] |= KF_MAP_LAYER_NEAR_CLIPPED;
}

DATA(0x801b69b4, 0x20, ".bss")
KfRenderMaskScanState render_mask_scan_state;
