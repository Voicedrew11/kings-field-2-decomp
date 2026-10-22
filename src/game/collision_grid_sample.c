#include <kf/lib/address.h>
#include <kf/game/collision_cache.h>
#include <kf/game/map_cell.h>

enum { COLLISION_DEFAULT_SHAPE_ID = 0x3f };

DATA(0x800676e8, 0xa, ".data")
KfMapOccupancyCell collision_default_cell = {
    {{KF_MAP_CELL_NO_OBJECT_INDEX, 0, 0, COLLISION_DEFAULT_SHAPE_ID, 0},
     {KF_MAP_CELL_NO_OBJECT_INDEX, 0, 0, COLLISION_DEFAULT_SHAPE_ID, 0}}
};

ADDRESS(0x8002b6b4, 0x11c)
s32 collision_sample_map_cell_layer(s32 x, s32 y, s32 z)
{
    KfMapOccupancyCell *cell;
    s32 elevation;

    if ((u32)x < KF_MAP_WORLD_GRID_SIDE << KF_MAP_CELL_POSITION_SHIFT &&
        (u32)z < KF_MAP_WORLD_GRID_SIDE << KF_MAP_CELL_POSITION_SHIFT) {
        u16 height;

        cell = &bss_801c7540.map_cells[
            z >> KF_MAP_CELL_POSITION_SHIFT][x >> KF_MAP_CELL_POSITION_SHIFT];
        height = (u16)-(y >> KF_MAP_CELL_ELEVATION_SHIFT);
        if (cell->layer[0].elevation > cell->layer[1].elevation) {
            if (height >= cell->layer[0].elevation || cell->layer[1].elevation == 0) {
first_layer:
                KF_COLLISION_CACHE_LAYER = 0;
                elevation = -(s32)cell->layer[0].elevation;
            } else {
second_layer:
                KF_COLLISION_CACHE_LAYER = sizeof(KfMapOccupancyLayer);
                elevation = -(s32)cell->layer[1].elevation;
            }
            KF_COLLISION_CACHE_HEIGHT = elevation * (1 << KF_MAP_CELL_ELEVATION_SHIFT);
        } else if (height < cell->layer[1].elevation && cell->layer[0].elevation != 0) {
            goto first_layer;
        } else {
            goto second_layer;
        }
    } else {
        cell = &collision_default_cell;
        KF_COLLISION_CACHE_LAYER = 0;
        KF_COLLISION_CACHE_HEIGHT = 0;
    }

    KF_COLLISION_CACHE_CELL = cell;
    return KF_COLLISION_CACHE_HEIGHT;
}
