#ifndef KF_GAME_RENDER_MASK_H
#define KF_GAME_RENDER_MASK_H

#include <kf/lib/offsetof.h>
#include <kf/lib/types.h>
#include <kf/game/render_types.h>

/* The observed span is written by 0x8002c670 while it scans a 24-by-24
 * window over the two-layer map occupancy grid. */
typedef struct KfRenderMaskScanState {
    s32 first_layer_byte_offset;
    s32 second_layer_byte_offset;
    KfMapLayerMask first_layer_mask;
    KfMapLayerMask second_layer_mask;
    s32 map_x;
    s32 map_z;
    s32 window_x;
    s32 window_z;
    KfMapLayerMask *mask_cursor;
} KfRenderMaskScanState;

typedef char kf_render_mask_scan_state_size[
    sizeof(KfRenderMaskScanState) == 0x20 ? 1 : -1];
typedef char kf_render_mask_scan_map_x_offset[
    offsetof(KfRenderMaskScanState, map_x) == 0x0c ? 1 : -1];
typedef char kf_render_mask_scan_cursor_offset[
    offsetof(KfRenderMaskScanState, mask_cursor) == 0x1c ? 1 : -1];

/* find_map_cell_layer_mask_run_boundary scans for the first cell holding
 * the value, then for the first cell past that run. */
KF_ENUM_BEGIN(KfMaskRunScan, s32)
    KF_MASK_RUN_SEEK = 0,
    KF_MASK_RUN_INSIDE = 1
KF_ENUM_END(KfMaskRunScan)

typedef struct KfCollisionMaskPoint {
    s32 x;
    s32 z;
} KfCollisionMaskPoint;

typedef struct KfMapMaskShapePair {
    s16 near;
    s16 far;
} KfMapMaskShapePair;

typedef char kf_map_mask_shape_pair_size[sizeof(KfMapMaskShapePair) == 4 ? 1 : -1];

extern KfRenderMaskScanState render_masks_scan_state;
extern KfMapMaskShapePair map_mask_pitch_shape_pairs[7];

void clear_map_cell_layer_masks(void);
void rasterize_map_cell_layer_mask_line(const KfCollisionMaskPoint *start,
                   const KfCollisionMaskPoint *end, KfMapLayerMask value);
void fill_map_cell_layer_mask_interior(KfMapLayerMask value);
void update_current_map_cell_layer_mask(s32 cursor_offset);
void sweep_map_cell_layer_mask_line(s32 first_offset, s32 second_offset, s32 map_step,
                   s8 window_step, s32 mask_stride, s32 count);

#endif
