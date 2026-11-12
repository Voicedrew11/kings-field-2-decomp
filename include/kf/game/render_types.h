#ifndef KF_GAME_RENDER_TYPES_H
#define KF_GAME_RENDER_TYPES_H

#include <kf/lib/types.h>
#include <kf/lib/enum.h>

/* Selectors shared by actor, effect, map-object and HUD model records and the
 * render_world_model queue. */

/* Map layers of a two-layer occupancy cell: FIRST is layer[0], SECOND
 * layer[1]. Object, actor, effect and placed records carry the layers they
 * occupy (NONE hides them); the camera's 24x24 visibility grid uses the same
 * bits for visible layers and adds the view-region and near-camera flags that
 * render_map_cell_object reads. */
KF_ENUM_BEGIN(KfMapLayerMask, u8)
    KF_MAP_LAYER_NONE = 0,
    KF_MAP_LAYER_FIRST = 1,
    KF_MAP_LAYER_SECOND = 2,
    KF_MAP_LAYER_BOTH = KF_MAP_LAYER_FIRST | KF_MAP_LAYER_SECOND,
    /* Rasterized from the camera's pitch-dependent view polygon. */
    KF_MAP_LAYER_IN_VIEW = 0x20,
    /* The camera cell and its four edge neighbours: subdivide small objects. */
    KF_MAP_LAYER_NEAR_PREPARE = 0x40,
    /* The 3x3 cells around the camera: enqueue with clipping. */
    KF_MAP_LAYER_NEAR_CLIPPED = 0x80
KF_ENUM_END(KfMapLayerMask)
KF_ENUM_FLAGS(KfMapLayerMask, u8)

/* Visibility of HUD model and notification rows; tables that end in a
 * sentinel row mark it END (KF1 KfSpriteState). */
KF_ENUM_BEGIN(KfSpriteState, u8)
    KF_SPRITE_HIDDEN = 0,
    KF_SPRITE_VISIBLE = 1,
    KF_SPRITE_END = 0xff
KF_ENUM_END(KfSpriteState)

/* Full-screen colour overlay: the low bits are the tpage semi-transparency
 * (ABR) of the quad, FRONT draws it at OT depth 1 instead of 0x40, OFF
 * disables it. Fades add (1) or subtract in front (0x82). */
KF_ENUM_BEGIN(KfColorOverlayControl, u8)
    KF_COLOR_OVERLAY_NO_FLAGS = 0,
    KF_COLOR_OVERLAY_ADD = 1,
    KF_COLOR_OVERLAY_SUBTRACT = 2,
    KF_COLOR_OVERLAY_BLEND_MASK = 3,
    KF_COLOR_OVERLAY_FRONT = 0x80,
    KF_COLOR_OVERLAY_OFF = 0xff
KF_ENUM_END(KfColorOverlayControl)
KF_ENUM_FLAGS(KfColorOverlayControl, u8)

/* The tpage semi-transparency field (ABR) of a blend mode. */
#define KF_GPU_TPAGE_ABR(mode) (KF_ENUM_VALUE(mode) << 5)

/* render_textured_quad's semitrans argument; OPAQUE skips SetSemiTrans. */
enum {
    KF_TEXTURED_QUAD_SEMI_TRANSPARENT = 1,
    KF_TEXTURED_QUAD_OPAQUE = 0xff
};

/* render_animated_object's lighting byte: a KfLightingIndex row in the low
 * seven bits and a flag that negates the row's light matrix. */
enum {
    KF_LIGHTING_FLAGS_INDEX_MASK = 0x7f,
    KF_LIGHTING_FLAG_REVERSED_LIGHT = 0x80
};

/* render_world_model's queue selector. 0xff and 0xfe pick the textured and
 * clipping enqueues; 0x80 is textured without the below-view depth bias.
 * Other values reach render_enqueue_blended_tmd, which shifts them into the
 * GPU tpage semi-transparency field (ABR, bits 5-6). */
KF_ENUM_BEGIN(KfRenderQueueMode, u8)
    KF_RENDER_QUEUE_BLEND_AVERAGE = 0,
    KF_RENDER_QUEUE_BLEND_ADD = 1,
    KF_RENDER_QUEUE_BLEND_SUBTRACT = 2,
    KF_RENDER_QUEUE_BLEND_ADD_QUARTER = 3,
    KF_RENDER_QUEUE_TEXTURED_UNBIASED = 0x80,
    KF_RENDER_QUEUE_CLIPPED = 0xfe,
    KF_RENDER_QUEUE_TEXTURED = 0xff
KF_ENUM_END(KfRenderQueueMode)

/* Row of scene_display_sys.collision_rows (colour matrix, light matrix,
 * fog and back colour). Map-cell layers select rows 0..0x3f through their low
 * six bits; the fixed rows from 0x40 light HUD models, effects, actors and
 * map-placed models. NONE disables an object's override blend. Rows without a
 * distinctive consumer keep their encoding as a WIP name. */
KF_ENUM_BEGIN(KfLightingIndex, u8)
    KF_LIGHTING_HUD_COMPASS = 0x40,
    KF_LIGHTING_HUD = 0x41,
    KF_LIGHTING_PRESET_42 = 0x42,
    KF_LIGHTING_SCALED_EFFECT = 0x43,
    KF_LIGHTING_EFFECT = 0x44,
    KF_LIGHTING_MAP_PLACED = 0x46,
    KF_LIGHTING_ACTOR_DEFAULT = 0x47,
    KF_LIGHTING_PRESET_48 = 0x48,
    KF_LIGHTING_PRESET_49 = 0x49,
    KF_LIGHTING_NONE = 0xff
KF_ENUM_END(KfLightingIndex)

#endif
