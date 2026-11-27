#ifndef KF_GAME_MAP_CELL_H
#define KF_GAME_MAP_CELL_H

#include <kf/game/player.h>
#include <kf/game/render_types.h>

enum {
    /* Low bits of KfMapOccupancyLayer.quarter_turns: the cell's KfQuarterTurn. */
    KF_MAP_CELL_QUARTER_TURN_MASK = 3,
    /* Low bits of KfMapOccupancyLayer.lighting_index: the lighting row. */
    KF_MAP_CELL_LIGHTING_MASK = 0x3f,
    KF_MAP_CELL_POSITION_SHIFT = 11,
    KF_MAP_CELL_SIZE = 1 << KF_MAP_CELL_POSITION_SHIFT,
    KF_MAP_CELL_LOCAL_MASK = KF_MAP_CELL_SIZE - 1,
    KF_MAP_CELL_ELEVATION_SHIFT = 7,
    KF_MAP_CELL_NO_OBJECT_INDEX = 0xff,
    /* An object index of 0xf0 or more draws nothing; 0xfe is a cleared marker. */
    KF_MAP_CELL_MARKER_CLEARED = 0xfe
};

/* High bits of KfMapOccupancyLayer.lighting_index. With both shape-query bits
 * set, layer flag 0x40 forces an axis hit and a cache result of -100000.
 * Flag 0x80 reveals the other layer to the camera mask when this layer's map
 * object is present. Pattern copies move each bit separately. */
enum {
    KF_MAP_CELL_LAYER_FORCES_AXIS_HIT = 0x40,
    KF_MAP_CELL_LAYER_REVEALS_OTHER_LAYER = 0x80
};

void render_map_cell_object(const KfMapCellShape *shape, SVECTOR *position,
                            KfMapLayerMask flags);
s32 collision_sample_map_layer_height(KfMapLayerMask layer, s32 x, s32 z, s32 radius, s32 height);
void map_cell_add_layer_occupancy(s32 x, s32 z, s32 radius, s32 amount);

#endif
