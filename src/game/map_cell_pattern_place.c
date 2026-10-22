#include <kf/lib/address.h>
#include <kf/lib/math.h>
#include <kf/lib/quarter_turn.h>
#include <kf/game/map_cell.h>
#include <kf/game/map_cell_pattern.h>
#include <kf/game/player.h>
#include <psyq/sdk.h>

enum {
    KF_PATTERN_END = 0xff,
    KF_PATTERN_SKIP_BYTE = 0xfe,
    KF_PATTERN_SKIP_OBJECT = 0xff,
    KF_PATTERN_LAYER_FLAG_MASK = 0x7f,
    KF_PATTERN_ORIENTATION_OTHER_BITS_MASK = 0xfc,
    KF_PATTERN_LIGHTING_FLAGS_MASK = 0xc0
};

ADDRESS(0x80035cc8, 0x204)
void map_cell_apply_rotated_pattern(KfMapLayerMask mode, s32 world_x, s32 world_z, s32 angle,
                   const KfMapCellPattern *patterns, s32 variant_index,
                   s32 layer_flag)
{
    s32 cosine;
    s32 sine;
    s32 first_layer_offset;
    s32 second_layer_offset;

    world_x >>= KF_MAP_CELL_POSITION_SHIFT;
    world_z >>= KF_MAP_CELL_POSITION_SHIFT;
    cosine = rcos(angle);
    sine = rsin(angle);
    first_layer_offset = (mode == KF_MAP_LAYER_FIRST)
                                 ? 0 : sizeof(KfMapOccupancyLayer);
    second_layer_offset = first_layer_offset == 0
                                  ? sizeof(KfMapOccupancyLayer) : 0;

    while (patterns->variant[0].first_collision_shape_id != KF_PATTERN_END) {
        s32 local_x = patterns->offset_x;
        s32 local_z = patterns->offset_z;
        s32 cell_x = ((local_x * cosine - local_z * sine) >> KF_FIXED12_BITS) +
                     world_x;
        s32 cell_z = ((local_z * cosine + local_x * sine) >> KF_FIXED12_BITS) +
                     world_z;
        KfMapOccupancyCell *cell = &bss_801c7540.map_cells[cell_z][cell_x];
        const KfMapCellPatternVariant *variant = &patterns->variant[variant_index];
        KfMapOccupancyLayer *first_layer =
            (KfMapOccupancyLayer *)((u8 *)cell + first_layer_offset);
        KfMapOccupancyLayer *second_layer =
            (KfMapOccupancyLayer *)((u8 *)cell + second_layer_offset);

        if (variant->first_collision_shape_id != KF_PATTERN_SKIP_BYTE) {
            first_layer->collision_shape_id = variant->first_collision_shape_id;
        }
        if (variant->second_collision_shape_id != KF_PATTERN_SKIP_BYTE) {
            second_layer->collision_shape_id = variant->second_collision_shape_id;
        }
        if (variant->first_object_index != KF_PATTERN_SKIP_OBJECT) {
            first_layer->object_index = variant->first_object_index;
        }
        if (variant->second_object_index != KF_PATTERN_SKIP_OBJECT) {
            second_layer->object_index = variant->second_object_index;
        }
        if (layer_flag != KF_PATTERN_LIGHTING_UNCHANGED) {
            first_layer->lighting_index =
                (first_layer->lighting_index & KF_PATTERN_LAYER_FLAG_MASK) |
                layer_flag;
            second_layer->lighting_index =
                (second_layer->lighting_index & KF_PATTERN_LAYER_FLAG_MASK) |
                layer_flag;
        }
        patterns++;
    }
}


ADDRESS(0x80035ecc, 0x370)
void map_cell_copy_rotated_fields(KfMapLayerMask layer_select, s32 source_x, s32 source_z,
                   s32 destination_x, s32 destination_z,
                   s32 width, s32 height, s32 rotation, u32 field_mask)
{
    KfMapOccupancyCell *source_row;
    KfMapOccupancyCell *destination_row;
    s32 inner_step;
    s32 row_step;
    s32 rows_remaining;

    if (width == KF_MAP_CELL_COPY_DISABLED_WIDTH) {
        return;
    }
    /* The rotation argument is reduced to quarter turns in place. */
    rotation = -(rotation >> 10) & KF_MAP_CELL_QUARTER_TURN_MASK;
    switch (KF_ENUM_DECODE(KfQuarterTurn, rotation)) {
    case KF_QUARTER_TURN_0:
        inner_step = 1;
        row_step = KF_MAP_WORLD_GRID_SIDE;
        break;
    case KF_QUARTER_TURN_1:
        inner_step = -KF_MAP_WORLD_GRID_SIDE;
        row_step = 1;
        destination_z += width - 1;
        break;
    case KF_QUARTER_TURN_2:
        inner_step = -1;
        row_step = -KF_MAP_WORLD_GRID_SIDE;
        destination_x += width - 1;
        destination_z += height - 1;
        break;
    case KF_QUARTER_TURN_3:
        inner_step = KF_MAP_WORLD_GRID_SIDE;
        row_step = -1;
        destination_x += height - 1;
        break;
    }
    source_row = &bss_801c7540.map_cells[source_z][source_x];
    destination_row = &bss_801c7540.map_cells[destination_z][destination_x];
    for (rows_remaining = height; --rows_remaining != -1;) {
        KfMapOccupancyCell *source = source_row;
        KfMapOccupancyCell *destination;
        s32 columns_remaining;
        source_row += KF_MAP_WORLD_GRID_SIDE;
        destination = destination_row;
        destination_row += row_step;
        for (columns_remaining = width; --columns_remaining != -1;) {
            if ((layer_select & KF_MAP_LAYER_FIRST) != KF_MAP_LAYER_NONE) {
                if (field_mask & KF_MAP_CELL_COPY_OBJECT_INDEX) {
                    destination->layer[0].object_index = source->layer[0].object_index;
                }
                if (field_mask & KF_MAP_CELL_COPY_ELEVATION) {
                    destination->layer[0].elevation = source->layer[0].elevation;
                }
                if (field_mask & KF_MAP_CELL_COPY_ROTATED_ORIENTATION) {
                    destination->layer[0].quarter_turns =
                        (destination->layer[0].quarter_turns &
                         KF_PATTERN_ORIENTATION_OTHER_BITS_MASK) |
                        ((source->layer[0].quarter_turns + rotation) &
                         KF_MAP_CELL_QUARTER_TURN_MASK);
                }
                if (field_mask & KF_MAP_CELL_COPY_COLLISION_SHAPE) {
                    destination->layer[0].collision_shape_id = source->layer[0].collision_shape_id;
                }
                if (field_mask & KF_MAP_CELL_COPY_LIGHTING_INDEX) {
                    destination->layer[0].lighting_index =
                        (source->layer[0].lighting_index & KF_MAP_CELL_LIGHTING_MASK) |
                        (destination->layer[0].lighting_index & KF_PATTERN_LIGHTING_FLAGS_MASK);
                }
                if (field_mask & KF_MAP_CELL_COPY_LIGHTING_BIT_40) {
                    destination->layer[0].lighting_index =
                        (source->layer[0].lighting_index & KF_MAP_CELL_LAYER_COLLISION_FLAG_40) |
                        (destination->layer[0].lighting_index
                         & (u8)~KF_MAP_CELL_LAYER_COLLISION_FLAG_40);
                }
                if (field_mask & KF_MAP_CELL_COPY_LIGHTING_BIT_80) {
                    destination->layer[0].lighting_index =
                        (source->layer[0].lighting_index & KF_MAP_CELL_LAYER_REVEALS_OTHER_LAYER) |
                        (destination->layer[0].lighting_index
                         & (u8)~KF_MAP_CELL_LAYER_REVEALS_OTHER_LAYER);
                }
            }
            if ((layer_select & KF_MAP_LAYER_SECOND) != KF_MAP_LAYER_NONE) {
                if (field_mask & KF_MAP_CELL_COPY_OBJECT_INDEX) {
                    destination->layer[1].object_index = source->layer[1].object_index;
                }
                if (field_mask & KF_MAP_CELL_COPY_ELEVATION) {
                    destination->layer[1].elevation = source->layer[1].elevation;
                }
                if (field_mask & KF_MAP_CELL_COPY_ROTATED_ORIENTATION) {
                    destination->layer[1].quarter_turns =
                        (destination->layer[1].quarter_turns &
                         KF_PATTERN_ORIENTATION_OTHER_BITS_MASK) |
                        ((source->layer[1].quarter_turns + rotation) &
                         KF_MAP_CELL_QUARTER_TURN_MASK);
                }
                if (field_mask & KF_MAP_CELL_COPY_COLLISION_SHAPE) {
                    destination->layer[1].collision_shape_id = source->layer[1].collision_shape_id;
                }
                if (field_mask & KF_MAP_CELL_COPY_LIGHTING_INDEX) {
                    destination->layer[1].lighting_index =
                        (source->layer[1].lighting_index & KF_MAP_CELL_LIGHTING_MASK) |
                        (destination->layer[1].lighting_index & KF_PATTERN_LIGHTING_FLAGS_MASK);
                }
                if (field_mask & KF_MAP_CELL_COPY_LIGHTING_BIT_40) {
                    destination->layer[1].lighting_index =
                        (source->layer[1].lighting_index & KF_MAP_CELL_LAYER_COLLISION_FLAG_40) |
                        (destination->layer[1].lighting_index
                         & (u8)~KF_MAP_CELL_LAYER_COLLISION_FLAG_40);
                }
                if (field_mask & KF_MAP_CELL_COPY_LIGHTING_BIT_80) {
                    destination->layer[1].lighting_index =
                        (source->layer[1].lighting_index & KF_MAP_CELL_LAYER_REVEALS_OTHER_LAYER) |
                        (destination->layer[1].lighting_index
                         & (u8)~KF_MAP_CELL_LAYER_REVEALS_OTHER_LAYER);
                }
            }
            source++;
            destination += inner_step;
        }
    }
}
