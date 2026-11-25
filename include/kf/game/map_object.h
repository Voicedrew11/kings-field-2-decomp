#ifndef KF_GAME_MAP_OBJECT_H
#define KF_GAME_MAP_OBJECT_H

#include <kf/lib/bool.h>
#include <kf/lib/offsetof.h>
#include <kf/lib/types.h>
#include <kf/lib/enum.h>
#include <kf/lib/math.h>
#include <kf/game/audio.h>
#include <kf/game/item.h>
#include <kf/game/pool.h>
#include <kf/game/render_types.h>
#include <kf/game/notify_types.h>

/* Template byte 0 selects an object's operation. The loader copies most
 * selectors unchanged into the runtime action byte (the loader's byte-indexed
 * operation jump table, sb +4), which map_object_update_actions dispatches; the interaction
 * and world-state switches read the template selector directly. Definition-
 * only and runtime-only operations therefore share one namespace. Names
 * follow the update/interaction code (and KF1's numbering where it agrees:
 * lift door 2, hinged container 8, item container 9, screen image 13, save
 * point 14, item pickup 64); members without behavioural evidence keep
 * their decimal encoding as a WIP name.
 *
 * Sockets show an item used on them: items 114..116 placed in a recall
 * socket are the destinations of items 111..113, and item 103 placed in a
 * door socket sets that side's open bit of the linked door's marker.
 * Operations 95 and 160..165 have no GAME update case; the map callback
 * table (FDAT 3r + 2) initializes (slot 8), updates (slot 9) and arms them
 * (slots 0 and 2), so REGION0/REGION1 name the overlay that implements an
 * otherwise identical item socket. No JP/US/EU template uses 0, 11 or 80,
 * the only operation-33 template (object 239) is never placed, dropped or
 * spawned, and no code or map callback stores any of the four in the action
 * byte; only their GAME dispatch hooks remain. */
KF_ENUM_BEGIN(KfMapObjectOperation, u8)
    KF_MAP_OBJECT_OP_0 = 0,
    KF_MAP_OBJECT_OP_LIFT_DOOR = 2,
    KF_MAP_OBJECT_OP_SIGNAL_DOOR = 3,
    KF_MAP_OBJECT_OP_HINGE = 4,
    KF_MAP_OBJECT_OP_ANIMATED_CONTAINER = 5,
    KF_MAP_OBJECT_OP_HINGED_CONTAINER = 8,
    KF_MAP_OBJECT_OP_ITEM_CONTAINER = 9,
    KF_MAP_OBJECT_OP_11 = 11,
    KF_MAP_OBJECT_OP_SCREEN_IMAGE = 13,
    KF_MAP_OBJECT_OP_SAVE_POINT = 14,
    KF_MAP_OBJECT_OP_RECALL_SOCKET = 15,
    KF_MAP_OBJECT_OP_BOB = 16,
    KF_MAP_OBJECT_OP_DOOR_SOCKET = 17,
    KF_MAP_OBJECT_OP_RESTORE_POINT = 18,
    KF_MAP_OBJECT_OP_GROW_ITEM = 19,
    KF_MAP_OBJECT_OP_HIDDEN_SCREEN_IMAGE = 20,
    KF_MAP_OBJECT_OP_HIDDEN_ITEM_CONTAINER = 21,
    KF_MAP_OBJECT_OP_SLIDING_CONTAINER = 22,
    KF_MAP_OBJECT_OP_AMBIENT_SOUND = 31,
    KF_MAP_OBJECT_OP_PLAYER_REACTION = 32,
    KF_MAP_OBJECT_OP_33 = 33,
    KF_MAP_OBJECT_OP_WARP = 34,
    KF_MAP_OBJECT_OP_RAISED_ADDITIVE = 48,
    KF_MAP_OBJECT_OP_ITEM_PICKUP = 64,
    KF_MAP_OBJECT_OP_80 = 80,
    KF_MAP_OBJECT_OP_ANIMATED_HAZARD = 81,
    KF_MAP_OBJECT_OP_SWITCH = 83,
    KF_MAP_OBJECT_OP_PATTERN_GATE = 84,
    KF_MAP_OBJECT_OP_CELL_COPY_TOGGLE = 88,
    KF_MAP_OBJECT_OP_LAYER_FADE = 89,
    KF_MAP_OBJECT_OP_SIGNAL_CELL_COPY = 95,
    KF_MAP_OBJECT_OP_FALL_AND_TIP = 96,
    KF_MAP_OBJECT_OP_FALL_AND_SPIN = 97,
    KF_MAP_OBJECT_OP_BOUNCE = 98,
    KF_MAP_OBJECT_OP_OFFSET_MOTION = 112,
    KF_MAP_OBJECT_OP_REGION0_ITEM_SOCKET = 160,
    KF_MAP_OBJECT_OP_REGION1_ITEM_SOCKET = 161,
    KF_MAP_OBJECT_OP_SIGNAL_EVENT_BIT = 162,
    KF_MAP_OBJECT_OP_EVENT_BIT_ACTIVATED = 163,
    KF_MAP_OBJECT_OP_ITEM_DIAL = 164,
    KF_MAP_OBJECT_OP_EFFECT_EMITTER = 165,
    KF_MAP_OBJECT_OP_RESOURCE_TRIGGER = 224,
    KF_MAP_OBJECT_OP_REGION_TRIGGER = 225,
    KF_MAP_OBJECT_OP_SCENE_INSPECT = 226,
    KF_MAP_OBJECT_OP_ANIMATED_MODEL = 240,
    KF_MAP_OBJECT_OP_NONE = 255
KF_ENUM_END(KfMapObjectOperation)

/* action_timer of a KF_MAP_OBJECT_OP_SWITCH object; activation writes INIT. */
enum {
    KF_MAP_OBJECT_SWITCH_IDLE = 0,
    KF_MAP_OBJECT_SWITCH_START = 1,
    KF_MAP_OBJECT_SWITCH_FORWARD = 2,
    KF_MAP_OBJECT_SWITCH_REVERSE = 3,
    KF_MAP_OBJECT_SWITCH_RETURN = 4,
    KF_MAP_OBJECT_SWITCH_INIT = 9,
    KF_MAP_OBJECT_SWITCH_DONE = 99
};

enum {
    KF_MAP_OBJECT_ACTION_TIMER_INIT = 0,
    KF_MAP_OBJECT_INDEX_NONE = 0xffff,
    KF_MAP_OBJECT_SPAWN_SEQUENCE_MODULUS = 0x10000,
    KF_MAP_OBJECT_TEMPLATE_CAPACITY = 320,
    KF_MAP_OBJECT_PLACED_COUNT = 0x15e,
    KF_MAP_OBJECT_SCATTER_POOL_FIRST = 0x15e,
    KF_MAP_OBJECT_DEFINITION_DROP_FIRST = 0x168,
    KF_MAP_OBJECT_PLACEMENT_DROP_FIRST = 0x172,
    KF_MAP_OBJECT_EFFECT_POOL_SIZE = 10,
    /* Scene events spawn into the last sixteen slots. */
    KF_MAP_OBJECT_EVENT_POOL_FIRST = 0x17c,
    KF_MAP_OBJECT_EVENT_POOL_SIZE = 0x10,
    /* Largest q12 phase: a phase that reaches one full unit saturates here. */
    KF_MAP_OBJECT_PHASE_FULL = KF_FIXED12_ONE - 1,
    KF_MAP_REGION_HEIGHT_ANY = 0x8000,
    KF_MAP_OBJECT_CAPACITY = 0x18c
};

/* Map object sentinels and clearance sizes shared by the update and
 * initialization code. */
enum {
    KF_MAP_OBJECT_ROTATION_CODE_NONE = 0xff,
    KF_MAP_OBJECT_LINK_NONE = 0xff,
    KF_MAP_OBJECT_HAZARD_NO_CELL_MARKER = 0xff,
    KF_MAP_OBJECT_SIGNAL_NONE = 0xff,
    KF_MAP_OBJECT_EXTRA_FILL_BYTE = 0xff,
    KF_MAP_OBJECT_RESOURCE_LOCAL_X_UNSET = 0xff,
    KF_MAP_OBJECT_RESOURCE_OFFSET_UNSET = 0x7f,
    /* Keeps the fraction of a q12 phase, wrapping it to one unit. */
    KF_MAP_OBJECT_PHASE_WRAP_MASK = KF_FIXED12_ONE - 1,
    KF_MAP_OBJECT_SIGNAL_DOOR_CLEARANCE_RADIUS = 0x1130,
    KF_MAP_OBJECT_DOOR_CLEARANCE_HEIGHT = 0xc80,
    /* One height step, in world units, for a layer fade or a raised sequence unit. */
    KF_MAP_OBJECT_RAISE_STEP = 0x100,
    KF_MAP_LAYER_FADE_SHAPE_TRANSITION = 0x74,
    KF_MAP_LAYER_FADE_SHAPE_HIDDEN = 0x75,
    /* Render depth offset of a hinge's far side. */
    KF_MAP_OBJECT_HINGE_DEPTH_FAR = 0xf0
};

/* Template collision byte, copied into the object: NEAR_CLIP picks the
 * clipping enqueue near the camera, RADIUS_VISIBLE tests a cell radius
 * instead of the object's own cell, ANY_ANGLE accepts interactions from
 * any bearing and UNBIASED_DEPTH selects KF_RENDER_QUEUE_TEXTURED_UNBIASED.
 * The renderer sets RENDERED for objects drawn this frame. */
KF_ENUM_BEGIN(KfMapObjectFlags, u8)
    KF_MAP_OBJECT_FLAGS_NONE = 0,
    KF_MAP_OBJECT_FLAG_NEAR_CLIP = 0x01,
    KF_MAP_OBJECT_FLAG_RADIUS_VISIBLE = 0x02,
    KF_MAP_OBJECT_INTERACTION_ANY_ANGLE = 0x04,
    KF_MAP_OBJECT_FLAG_UNBIASED_DEPTH = 0x20,
    KF_MAP_OBJECT_FLAG_RENDERED = 0x80
KF_ENUM_END(KfMapObjectFlags)
KF_ENUM_FLAGS(KfMapObjectFlags, u8)

/* extra_40.bytes[0] one-shot latch of camera-region and trigger actions. */
enum {
    KF_MAP_OBJECT_LATCH_CLEAR = 0,
    KF_MAP_OBJECT_LATCH_SET = 1
};

/* Action 84 pattern_flags: VARIANT picks the pattern of the pair, ONCE keeps
 * the pattern applied; the pattern is applied ON (variant 1) and removed OFF
 * (variant 0) of map_object_cell_patterns rows. */
enum {
    KF_MAP_OBJECT_PATTERN_VARIANT = 1,
    KF_MAP_OBJECT_PATTERN_ONCE = 2,
    KF_MAP_OBJECT_PATTERN_OFF = 0,
    KF_MAP_OBJECT_PATTERN_ON = 1
};

/* Region-action operation byte: low nibble selects the operation, REPEAT
 * keeps the latch open. */
enum {
    KF_MAP_OBJECT_REGION_CALLBACK = 0,
    KF_MAP_OBJECT_REGION_SIGNAL = 1,
    KF_MAP_OBJECT_REGION_SET_CONTROL = 2,
    KF_MAP_OBJECT_REGION_OPERATION_MASK = 0x0f,
    KF_MAP_OBJECT_REGION_REPEAT = 0x80
};

/* map_object_spawn_effect pool: an actor's own placement drop or its target
 * group's definition drop. */
KF_ENUM_BEGIN(KfMapObjectDropSource, u8)
    KF_MAP_OBJECT_DROP_FROM_PLACEMENT = 0,
    KF_MAP_OBJECT_DROP_FROM_DEFINITION = 1
KF_ENUM_END(KfMapObjectDropSource)

/* Action 83: ONCE runs forward and stops, ONCE_AND_RETURN runs forward and
 * falls back; the toggle pair alternates, CLOSED running forward to OPEN and
 * OPEN running back to CLOSED. Only the toggle states are saved. */
KF_ENUM_BEGIN(KfMapObjectTransitionMode, u8)
    KF_MAP_OBJECT_TRANSITION_ONCE = 0,
    KF_MAP_OBJECT_TRANSITION_ONCE_AND_RETURN = 1,
    KF_MAP_OBJECT_TRANSITION_TOGGLE_CLOSED = 2,
    KF_MAP_OBJECT_TRANSITION_TOGGLE_OPEN = 3
KF_ENUM_END(KfMapObjectTransitionMode)

/* Action 88 copies its alternate cells, then REVERT fades back and restores
 * the source cells while HOLD fades in and keeps them; a marker signal
 * toggles the mode. */
KF_ENUM_BEGIN(KfMapObjectCellCopyMode, u8)
    KF_MAP_OBJECT_CELL_COPY_REVERT = 0,
    KF_MAP_OBJECT_CELL_COPY_HOLD = 1
KF_ENUM_END(KfMapObjectCellCopyMode)

/* tail.fields.unknown_38 interaction flags: set_property arms all of them,
 * event masks clear or set individual bits, pickups require ARMED. */
enum {
    KF_MAP_OBJECT_EVENT_DISARMED = 0,
    KF_MAP_OBJECT_EVENT_ARMED = 0xff
};

/* Action 81 collision probe: a marker signal toggles RUNNING; region width
 * NEVER disables the camera trigger and ALWAYS skips the region test. */
enum {
    KF_MAP_OBJECT_PROBE_STOPPED = 0,
    KF_MAP_OBJECT_PROBE_RUNNING = 0xff,
    KF_MAP_OBJECT_REGION_NEVER = 0xfe,
    KF_MAP_OBJECT_REGION_ALWAYS = 0xff
};

/* Template byte 1, the pickup category of a dropped or placed object: a drop
 * tips over (0x10/0x13/0x16), spins (0x17) or bounces (the rest); GOLD is a
 * gold pile whose pickup adds its amount. Clearing a 0x10 object's layer
 * stands it up. Other members keep their encoding as a WIP name. */
KF_ENUM_BEGIN(KfMapObjectKind, u8)
    KF_MAP_OBJECT_KIND_10 = 0x10,
    KF_MAP_OBJECT_KIND_11 = 0x11,
    KF_MAP_OBJECT_KIND_12 = 0x12,
    KF_MAP_OBJECT_KIND_13 = 0x13,
    KF_MAP_OBJECT_KIND_14 = 0x14,
    KF_MAP_OBJECT_KIND_15 = 0x15,
    KF_MAP_OBJECT_KIND_16 = 0x16,
    KF_MAP_OBJECT_KIND_17 = 0x17,
    KF_MAP_OBJECT_KIND_18 = 0x18,
    KF_MAP_OBJECT_KIND_19 = 0x19,
    KF_MAP_OBJECT_KIND_GOLD = 0x20
KF_ENUM_END(KfMapObjectKind)

/* Marker bytes hold the key-item or signal id an object waits for; a
 * consumed marker becomes CLEARED and a fired event TRIGGERED. Signal ids
 * 150..198 come in pairs that differ in bit 0. Doors read markers from
 * OPEN_SIDES_FIRST up as side bits: FRONT opens facing the door's yaw and
 * BACK from behind, so a cleared marker opens from both sides. */
enum {
    KF_MAP_OBJECT_MARKER_TRIGGERED = 0xfe,
    KF_MAP_OBJECT_MARKER_CLEARED = 0xff,
    KF_MAP_OBJECT_MARKER_PAIR_MASK = 0xfe,
    KF_MAP_OBJECT_MARKER_OPEN_SIDES_FIRST = 0xfc,
    KF_MAP_OBJECT_MARKER_OPEN_FRONT = 1,
    KF_MAP_OBJECT_MARKER_OPEN_BACK = 2
};

/* map_object_check_and_consume_marker: the object takes no marker, the
 * marker matched and was consumed, it was already cleared or triggered, a
 * different marker is needed, or the object is a recall or door socket. */
KF_ENUM_BEGIN(KfMapObjectMarkerCheck, s32)
    KF_MAP_OBJECT_MARKER_NOT_APPLICABLE = 0,
    KF_MAP_OBJECT_MARKER_CONSUMED = 1,
    KF_MAP_OBJECT_MARKER_ALREADY_CLEARED = 2,
    KF_MAP_OBJECT_MARKER_MISMATCH = 3,
    KF_MAP_OBJECT_MARKER_REFUSED = 4
KF_ENUM_END(KfMapObjectMarkerCheck)

/* map_object_set_property selector; SET_LAYER_MASK and SET_RENDER_DEPTH
 * read one variadic value. */
KF_ENUM_BEGIN(KfMapObjectProperty, s32)
    KF_MAP_OBJECT_PROPERTY_CLEAR_LAYER_AND_STATE = 0,
    KF_MAP_OBJECT_PROPERTY_SET_LAYER_MASK = 1,
    KF_MAP_OBJECT_PROPERTY_ARM_EVENT = 2,
    KF_MAP_OBJECT_PROPERTY_SET_RENDER_DEPTH = 3
KF_ENUM_END(KfMapObjectProperty)

/* map_object_set_cell_marker: PLACE hides the object and writes its marker
 * into the map cell (unless the map-marker effect is running); CLEAR shows
 * the object and clears the cell. map_object_refresh_cell_markers passes the
 * same value to map_object_set_property, where 0 hides the linked object and
 * 1 restores its layer mask. */
KF_ENUM_BEGIN(KfMapCellMarkerMode, s32)
    KF_MAP_CELL_MARKER_PLACE = 0,
    KF_MAP_CELL_MARKER_CLEAR = 1
KF_ENUM_END(KfMapCellMarkerMode)

/* The last twelve template bytes are interpreted by the object's action:
 * marker and map-cell actions, the scene pose path, the collision probe and
 * the pattern-pair action each read their own layout. */
typedef struct KfMapObjectTemplateMarkerParams {
    u8 marker_action_05;
    u8 cell_width;
    u8 cell_height;
    u8 sound_id;
    u8 unknown_04[7];
    u8 marker_action_51;
} KfMapObjectTemplateMarkerParams;

typedef struct KfMapObjectTemplatePoseParams {
    s16 height_offset;
    s16 depth_offset;
    u16 unknown_04;
    u8 unknown_06[6];
} KfMapObjectTemplatePoseParams;

typedef struct KfMapObjectTemplateCollisionParams {
    u16 vertex_index;
    u16 reach;
    u16 height;
    u8 impact_magic_values[4];
    u8 sound_id;
    u8 marker_action_51;
} KfMapObjectTemplateCollisionParams;

typedef struct KfMapObjectTemplatePatternParams {
    u8 unknown_00[2];
    u8 pattern_pair_index;
    u8 unknown_03[9];
} KfMapObjectTemplatePatternParams;

typedef union KfMapObjectTemplateParams {
    KfMapObjectTemplateMarkerParams marker;
    KfMapObjectTemplatePoseParams pose;
    KfMapObjectTemplateCollisionParams collision;
    KfMapObjectTemplatePatternParams pattern;
} KfMapObjectTemplateParams;

typedef struct KfMapObjectTemplate {
    KfMapObjectOperation collision_kind;
    KfMapObjectKind kind;
    u8 vab_resource_index;
    KfMapObjectFlags collision_flags;
    u16 collision_radius;
    u16 interaction_radius;
    u16 interaction_height;
    u16 initial_render_depth_offset;
    KfMapObjectTemplateParams params;
} KfMapObjectTemplate;

typedef char kf_map_object_template_params_size[
    sizeof(KfMapObjectTemplateParams) == 12 ? 1 : -1];
typedef char kf_map_object_template_size[sizeof(KfMapObjectTemplate) == 24 ? 1 : -1];
typedef char kf_map_object_template_vab_resource_index_offset[
    offsetof(KfMapObjectTemplate, vab_resource_index) == 2 ? 1 : -1];
typedef char kf_map_object_template_collision_flags_offset[
    offsetof(KfMapObjectTemplate, collision_flags) == 3 ? 1 : -1];
typedef char kf_map_object_template_radius_offset[
    offsetof(KfMapObjectTemplate, collision_radius) == 4 ? 1 : -1];
typedef char kf_map_object_template_interaction_radius_offset[
    offsetof(KfMapObjectTemplate, interaction_radius) == 6 ? 1 : -1];
typedef char kf_map_object_template_interaction_height_offset[
    offsetof(KfMapObjectTemplate, interaction_height) == 8 ? 1 : -1];
typedef char kf_map_object_template_initial_render_depth_offset_offset[
    offsetof(KfMapObjectTemplate, initial_render_depth_offset) == 0x0a ? 1 : -1];
typedef char kf_map_object_template_params_offset[
    offsetof(KfMapObjectTemplate, params) == 0x0c ? 1 : -1];
typedef char kf_map_object_template_sound_id_offset[
    offsetof(KfMapObjectTemplate, params.marker.sound_id) == 0x0f ? 1 : -1];
typedef char kf_map_object_template_marker_action_51_offset[
    offsetof(KfMapObjectTemplate, params.marker.marker_action_51) == 0x17 ? 1 : -1];
typedef char kf_map_object_template_pose_tail_offset[
    offsetof(KfMapObjectTemplate, params.pose.unknown_04) == 0x10 ? 1 : -1];
typedef char kf_map_object_template_collision_magic_values_offset[
    offsetof(KfMapObjectTemplate, params.collision.impact_magic_values) == 0x12 ? 1 : -1];
typedef char kf_map_object_template_collision_sound_offset[
    offsetof(KfMapObjectTemplate, params.collision.sound_id) == 0x16 ? 1 : -1];
typedef char kf_map_object_template_pattern_pair_offset[
    offsetof(KfMapObjectTemplate, params.pattern.pattern_pair_index) == 0x0e ? 1 : -1];

/* The placement's final two words copy together into the object tail. */
typedef struct KfMapObjectTailCopyWords {
    u32 first;
    u32 second;
} KfMapObjectTailCopyWords;
typedef char kf_map_object_tail_copy_words_size[
    sizeof(KfMapObjectTailCopyWords) == 8 ? 1 : -1];

/* Map resource placements consumed in 24-byte rows by map_object_initialize_from_placements. */
typedef struct KfMapObjectPlacement {
    KfMapLayerMask layer_mask;
    u8 region_z;
    u8 region_x;
    KF_ENUM_STORAGE(KfObjectId, u16) object_id;
    s16 rotation_y;
    s16 local_z;
    s16 local_x;
    s16 height;
    KfMapObjectTailCopyWords tail_words;
} KfMapObjectPlacement;

typedef char kf_map_object_placement_size[
    sizeof(KfMapObjectPlacement) == 24 ? 1 : -1];
typedef char kf_map_object_placement_height_offset[
    offsetof(KfMapObjectPlacement, height) == 12 ? 1 : -1];
typedef char kf_map_object_placement_tail_words_offset[
    offsetof(KfMapObjectPlacement, tail_words) == 16 ? 1 : -1];

typedef struct KfMapObjectTailHalfwordBytes {
    u8 low;
    u8 high;
} KfMapObjectTailHalfwordBytes;

typedef union KfMapObjectTailHalfword {
    u16 value;
    s16 signed_value;
    KfMapObjectTailHalfwordBytes bytes;
} KfMapObjectTailHalfword;

typedef struct KfMapObjectTailFields {
    u32 unknown_34;
    u8 unknown_38;
    u8 unknown_39;
    KfMapObjectTailHalfword unknown_3a;
    u16 spawn_sequence;
    KfMapObjectTailHalfword unknown_3e;
} KfMapObjectTailFields;

/* Actions 0x60 and 0x62 reuse this halfword for vertical and angular motion. */
typedef struct KfMapObjectTailMotionView {
    u32 unknown_34;
    u8 unknown_38;
    u8 unknown_39;
    KfMapObjectTailHalfword unknown_3a;
    u16 unknown_3c;
    KfMapObjectTailHalfword motion_velocity;
} KfMapObjectTailMotionView;
typedef char kf_map_object_tail_motion_velocity_offset[
    offsetof(KfMapObjectTailMotionView, motion_velocity) == 10 ? 1 : -1];

typedef struct KfMapObjectTailNotificationView {
    u32 unknown_34;
    u8 unknown_38;
    u8 unknown_39;
    KfMapObjectTailHalfword unknown_3a;
    u16 unknown_3c;
    KfNotificationId linked_notification;
    KfNotificationId default_notification;
} KfMapObjectTailNotificationView;
typedef char kf_map_object_tail_notification_size[
    sizeof(KfMapObjectTailNotificationView) == 12 ? 1 : -1];
typedef char kf_map_object_tail_linked_notification_offset[
    offsetof(KfMapObjectTailNotificationView, linked_notification) == 10 ? 1 : -1];
typedef char kf_map_object_tail_default_notification_offset[
    offsetof(KfMapObjectTailNotificationView, default_notification) == 11 ? 1 : -1];

typedef struct KfMapObjectTailTransitionView {
    u32 unknown_34;
    u8 region_x;
    u8 region_z;
    u8 region_width;
    u8 region_depth;
    u8 destination_cell_x;
    u8 destination_cell_z;
    u8 destination_layer_code;
    u8 destination_yaw_code;
} KfMapObjectTailTransitionView;
typedef char kf_map_object_tail_transition_size[
    sizeof(KfMapObjectTailTransitionView) == 12 ? 1 : -1];

typedef struct KfMapObjectTailResourceTriggerView {
    u32 unknown_34;
    u8 region_width;
    u8 region_depth;
    u8 resource_selectors[5];
    u8 unknown_3f;
} KfMapObjectTailResourceTriggerView;
typedef char kf_map_object_tail_resource_trigger_size[
    sizeof(KfMapObjectTailResourceTriggerView) == 12 ? 1 : -1];

/* Action 225 uses the same camera-region bytes as action 224, followed by an
 * operation byte, its operand, and the value written by operation 2. */
typedef struct KfMapObjectTailRegionActionView {
    u32 unknown_34;
    u8 region_width;
    u8 region_depth;
    u8 operation_flags;
    u8 operand;
    u8 assigned_value;
    u8 unknown_3d[3];
} KfMapObjectTailRegionActionView;
typedef char kf_map_object_tail_region_action_size[
    sizeof(KfMapObjectTailRegionActionView) == 12 ? 1 : -1];
typedef char kf_map_object_tail_region_action_flags_offset[
    offsetof(KfMapObjectTailRegionActionView, operation_flags) == 6 ? 1 : -1];
typedef char kf_map_object_tail_region_action_operand_offset[
    offsetof(KfMapObjectTailRegionActionView, operand) == 7 ? 1 : -1];
typedef char kf_map_object_tail_region_action_value_offset[
    offsetof(KfMapObjectTailRegionActionView, assigned_value) == 8 ? 1 : -1];

/* Scene command 0x55 checks this camera region and opens the selected image. */
typedef struct KfMapObjectTailSceneInspectView {
    u32 unknown_34;
    u8 region_width;
    u8 region_depth;
    u16 transition_image_id;
    u8 unknown_3c[4];
} KfMapObjectTailSceneInspectView;
typedef char kf_map_object_tail_scene_inspect_size[
    sizeof(KfMapObjectTailSceneInspectView) == 12 ? 1 : -1];
typedef char kf_map_object_tail_scene_inspect_image_offset[
    offsetof(KfMapObjectTailSceneInspectView, transition_image_id) == 6 ? 1 : -1];

/* Kind 0x20 gives the player this amount when its interaction completes. */
typedef struct KfMapObjectTailGoldRewardView {
    u32 unknown_34;
    u8 unknown_38[2];
    u16 gold_amount;
    u8 unknown_3c[4];
} KfMapObjectTailGoldRewardView;
typedef char kf_map_object_tail_gold_reward_size[
    sizeof(KfMapObjectTailGoldRewardView) == 12 ? 1 : -1];
typedef char kf_map_object_tail_gold_reward_amount_offset[
    offsetof(KfMapObjectTailGoldRewardView, gold_amount) == 6 ? 1 : -1];

typedef struct KfMapObjectTailAnimatedView {
    KfPoolRecord *animation_cache;
    u8 radius_x;
    u8 radius_z;
    u8 depth_code;
    u8 lighting_flags;
    u8 blend_mode;
    u8 unknown_3d;
    u16 unknown_3e;
} KfMapObjectTailAnimatedView;
typedef char kf_map_object_tail_animated_size[
    sizeof(KfMapObjectTailAnimatedView) == 12 ? 1 : -1];

/* The ambient-sound action schedules playback for a rectangular map region. */
typedef struct KfMapObjectTailAmbientSoundView {
    u32 unknown_34;
    u8 region_width;
    u8 region_depth;
    u8 sound_id;
    u8 maximum_volume;
    u8 audible_radius_code;
    u8 vertical_attenuation_flags;
    u16 repeat_delay_units;
} KfMapObjectTailAmbientSoundView;
typedef char kf_map_object_tail_ambient_sound_size[
    sizeof(KfMapObjectTailAmbientSoundView) == 12 ? 1 : -1];
typedef char kf_map_object_tail_ambient_sound_id_offset[
    offsetof(KfMapObjectTailAmbientSoundView, sound_id) == 6 ? 1 : -1];
typedef char kf_map_object_tail_ambient_repeat_delay_offset[
    offsetof(KfMapObjectTailAmbientSoundView, repeat_delay_units) == 10 ? 1 : -1];

/* Actions 3 and 4 copy a rotated region between map-cell coordinates. */
typedef struct KfMapObjectTailCellCopyView {
    u32 unknown_34;
    u8 unknown_38;
    u8 destination_x;
    u8 destination_z;
    u8 source_x;
    u8 source_z;
    u8 linked_object_index;
    u16 unknown_3e;
} KfMapObjectTailCellCopyView;
typedef char kf_map_object_tail_cell_copy_size[
    sizeof(KfMapObjectTailCellCopyView) == 12 ? 1 : -1];
typedef char kf_map_object_tail_cell_copy_destination_offset[
    offsetof(KfMapObjectTailCellCopyView, destination_x) == 5 ? 1 : -1];
typedef char kf_map_object_tail_cell_copy_source_offset[
    offsetof(KfMapObjectTailCellCopyView, source_x) == 7 ? 1 : -1];
typedef char kf_map_object_tail_cell_copy_link_offset[
    offsetof(KfMapObjectTailCellCopyView, linked_object_index) == 9 ? 1 : -1];

/* Action 88 stores its copy coordinates and dimensions at different offsets. */
typedef struct KfMapObjectTailAction88CellCopyView {
    u32 unknown_34;
    KfMapObjectCellCopyMode transition_mode;
    u8 marker_id;
    u8 destination_x;
    u8 destination_z;
    u8 source_x;
    u8 source_z;
    u8 width;
    u8 height;
} KfMapObjectTailAction88CellCopyView;
typedef char kf_map_object_tail_action88_cell_copy_size[
    sizeof(KfMapObjectTailAction88CellCopyView) == 12 ? 1 : -1];
typedef char kf_map_object_tail_action88_mode_offset[
    offsetof(KfMapObjectTailAction88CellCopyView, transition_mode) == 4 ? 1 : -1];
typedef char kf_map_object_tail_action88_marker_offset[
    offsetof(KfMapObjectTailAction88CellCopyView, marker_id) == 5 ? 1 : -1];
typedef char kf_map_object_tail_action88_source_offset[
    offsetof(KfMapObjectTailAction88CellCopyView, source_x) == 8 ? 1 : -1];
typedef char kf_map_object_tail_action88_width_offset[
    offsetof(KfMapObjectTailAction88CellCopyView, width) == 10 ? 1 : -1];

/* Action 0x59 waits for a marker, then restores its layer for a timed fade. */
typedef struct KfMapObjectTailAction89LayerFadeView {
    u32 unknown_34;
    u8 marker_id;
    u8 unknown_39;
    u16 delay_frames;
    u8 unknown_3c[4];
} KfMapObjectTailAction89LayerFadeView;
typedef char kf_map_object_tail_action89_layer_fade_size[
    sizeof(KfMapObjectTailAction89LayerFadeView) == 12 ? 1 : -1];
typedef char kf_map_object_tail_action89_marker_offset[
    offsetof(KfMapObjectTailAction89LayerFadeView, marker_id) == 4 ? 1 : -1];
typedef char kf_map_object_tail_action89_delay_offset[
    offsetof(KfMapObjectTailAction89LayerFadeView, delay_frames) == 6 ? 1 : -1];

/* Action 84 checks a camera region and alternates two pattern rows. */
typedef struct KfMapObjectTailAction84PatternView {
    u32 unknown_34;
    u8 marker_id;
    u8 center_x;
    u8 center_z;
    u8 region_width;
    u8 region_depth;
    u8 depth_code;
    u8 pattern_flags;
    u8 unknown_3f;
} KfMapObjectTailAction84PatternView;
typedef char kf_map_object_tail_action84_pattern_size[
    sizeof(KfMapObjectTailAction84PatternView) == 12 ? 1 : -1];
typedef char kf_map_object_tail_action84_marker_offset[
    offsetof(KfMapObjectTailAction84PatternView, marker_id) == 4 ? 1 : -1];
typedef char kf_map_object_tail_action84_region_width_offset[
    offsetof(KfMapObjectTailAction84PatternView, region_width) == 7 ? 1 : -1];
typedef char kf_map_object_tail_action84_pattern_flags_offset[
    offsetof(KfMapObjectTailAction84PatternView, pattern_flags) == 10 ? 1 : -1];

/* Action 81 uses a camera gate and dispatches a magic impact on collision. */
typedef struct KfMapObjectTailCollisionProbeView {
    u32 unknown_34;
    u8 marker_trigger_state;
    u8 damage_multiplier_tenths;
    u8 phase_step_code;
    u8 camera_region_width;
    u8 camera_region_depth;
    u8 unknown_3d;
    u16 unknown_3e;
} KfMapObjectTailCollisionProbeView;
typedef char kf_map_object_tail_collision_probe_size[
    sizeof(KfMapObjectTailCollisionProbeView) == 12 ? 1 : -1];
typedef char kf_map_object_tail_collision_probe_trigger_offset[
    offsetof(KfMapObjectTailCollisionProbeView, marker_trigger_state) == 4 ? 1 : -1];
typedef char kf_map_object_tail_collision_damage_offset[
    offsetof(KfMapObjectTailCollisionProbeView, damage_multiplier_tenths) == 5 ? 1 : -1];
typedef char kf_map_object_tail_collision_region_offset[
    offsetof(KfMapObjectTailCollisionProbeView, camera_region_width) == 7 ? 1 : -1];

/* Actions 5, 8, and 22 direct property changes to a linked map object. */
typedef struct KfMapObjectTailLinkedPropertyView {
    u32 unknown_34;
    u8 unknown_38;
    u8 unknown_39;
    u16 linked_object_index;
    u32 unknown_3c;
} KfMapObjectTailLinkedPropertyView;
typedef char kf_map_object_tail_linked_property_size[
    sizeof(KfMapObjectTailLinkedPropertyView) == 12 ? 1 : -1];
typedef char kf_map_object_tail_linked_property_index_offset[
    offsetof(KfMapObjectTailLinkedPropertyView, linked_object_index) == 6 ? 1 : -1];

/* Action 83 emits its marker after each opening or closing phase. */
typedef struct KfMapObjectTailAction83View {
    u32 unknown_34;
    KfMapObjectTransitionMode transition_mode;
    u8 completion_marker;
    u8 unknown_3a[6];
} KfMapObjectTailAction83View;
typedef char kf_map_object_tail_action83_size[
    sizeof(KfMapObjectTailAction83View) == 12 ? 1 : -1];
typedef char kf_map_object_tail_action83_mode_offset[
    offsetof(KfMapObjectTailAction83View, transition_mode) == 4 ? 1 : -1];
typedef char kf_map_object_tail_action83_marker_offset[
    offsetof(KfMapObjectTailAction83View, completion_marker) == 5 ? 1 : -1];

/* Action 19 grows and animates a linked map object in 1/32-scale steps. */
typedef struct KfMapObjectTailScaleLinkView {
    u32 unknown_34;
    u8 scale_step_code;
    u8 unknown_39;
    u16 linked_object_index;
    u32 unknown_3c;
} KfMapObjectTailScaleLinkView;
typedef char kf_map_object_tail_scale_link_size[
    sizeof(KfMapObjectTailScaleLinkView) == 12 ? 1 : -1];
typedef char kf_map_object_tail_scale_step_offset[
    offsetof(KfMapObjectTailScaleLinkView, scale_step_code) == 4 ? 1 : -1];
typedef char kf_map_object_tail_scale_link_index_offset[
    offsetof(KfMapObjectTailScaleLinkView, linked_object_index) == 6 ? 1 : -1];

/* Event archive commands read the two state bytes at +0x38 as one halfword. */
typedef struct KfMapObjectTailPair38View {
    u32 unknown_34;
    u16 value_38;
    u8 unknown_3a[6];
} KfMapObjectTailPair38View;

/* Marker-driven actions compare this byte with an incoming signal, then
 * consume or arm the action. Other actions give the byte different meanings. */
typedef struct KfMapObjectTailMarkerView {
    u32 unknown_34;
    u8 marker_id;
    u8 unknown_39[7];
} KfMapObjectTailMarkerView;
typedef char kf_map_object_tail_marker_size[
    sizeof(KfMapObjectTailMarkerView) == 12 ? 1 : -1];
typedef char kf_map_object_tail_marker_id_offset[
    offsetof(KfMapObjectTailMarkerView, marker_id) == 4 ? 1 : -1];

/* Action 0x51 uses the high byte of the effect spawn-sequence slot as its
 * incoming marker identifier. */
typedef struct KfMapObjectTailAction51MarkerView {
    u32 unknown_34;
    u8 unknown_38;
    u8 unknown_39;
    KfMapObjectTailHalfword unknown_3a;
    u8 unknown_3c;
    u8 marker_id;
    KfMapObjectTailHalfword unknown_3e;
} KfMapObjectTailAction51MarkerView;
typedef char kf_map_object_tail_action51_marker_size[
    sizeof(KfMapObjectTailAction51MarkerView) == 12 ? 1 : -1];
typedef char kf_map_object_tail_action51_marker_offset[
    offsetof(KfMapObjectTailAction51MarkerView, marker_id) == 9 ? 1 : -1];

/* Two placement kinds seed three object rotation axes from byte codes;
 * 0xff leaves an axis at its default value. The final code overlaps the
 * spawn-sequence halfword used by other object kinds. */
typedef struct KfMapObjectTailInitialRotationView {
    u32 unknown_34;
    u8 unknown_38[2];
    u8 rotation_x_code;
    u8 rotation_y_code;
    u8 rotation_z_code;
    u8 unknown_3d[3];
} KfMapObjectTailInitialRotationView;
typedef char kf_map_object_tail_initial_rotation_size[
    sizeof(KfMapObjectTailInitialRotationView) == 12 ? 1 : -1];
typedef char kf_map_object_tail_initial_rotation_x_offset[
    offsetof(KfMapObjectTailInitialRotationView, rotation_x_code) == 6 ? 1 : -1];
typedef char kf_map_object_tail_initial_rotation_z_offset[
    offsetof(KfMapObjectTailInitialRotationView, rotation_z_code) == 8 ? 1 : -1];

typedef struct KfMapObjectTailEventEffectView {
    u32 unknown_34;
    KF_ENUM_STORAGE(KfObjectId, u8) pending_event_command;
    u8 effect_object_index;
    u8 linked_object_flag_mask;
    u8 linked_object_index;
    u8 unknown_3c[4];
} KfMapObjectTailEventEffectView;
typedef char kf_map_object_tail_event_effect_size[
    sizeof(KfMapObjectTailEventEffectView) == 12 ? 1 : -1];
typedef char kf_map_object_tail_event_command_offset[
    offsetof(KfMapObjectTailEventEffectView, pending_event_command) == 4 ? 1 : -1];
typedef char kf_map_object_tail_event_effect_index_offset[
    offsetof(KfMapObjectTailEventEffectView, effect_object_index) == 5 ? 1 : -1];
typedef char kf_map_object_tail_event_linked_mask_offset[
    offsetof(KfMapObjectTailEventEffectView, linked_object_flag_mask) == 6 ? 1 : -1];
typedef char kf_map_object_tail_event_linked_index_offset[
    offsetof(KfMapObjectTailEventEffectView, linked_object_index) == 7 ? 1 : -1];

typedef char kf_map_object_tail_pair38_size[
    sizeof(KfMapObjectTailPair38View) == 12 ? 1 : -1];
typedef char kf_map_object_tail_pair38_offset[
    offsetof(KfMapObjectTailPair38View, value_38) == 4 ? 1 : -1];

typedef struct KfMapObjectTailSpawnByteFields {
    u32 unknown_34;
    u8 unknown_38;
    u8 unknown_39;
    KfMapObjectTailHalfword unknown_3a;
    KfMapObjectTailHalfwordBytes spawn_sequence;
    KfMapObjectTailHalfword unknown_3e;
} KfMapObjectTailSpawnByteFields;
typedef char kf_map_object_tail_spawn_byte_fields_size[
    sizeof(KfMapObjectTailSpawnByteFields) == 12 ? 1 : -1];
typedef char kf_map_object_tail_spawn_bytes_offset[
    offsetof(KfMapObjectTailSpawnByteFields, spawn_sequence) == 8 ? 1 : -1];

/* The scattered effect pool stores the selected effect ID beside its spawn
 * sequence; the preceding state bytes have action-specific uses elsewhere. */
typedef struct KfMapObjectTailScatteredEffectView {
    u32 unknown_34;
    u8 unknown_38[2];
    u16 effect_id;
    u16 spawn_sequence;
    u16 unknown_3e;
} KfMapObjectTailScatteredEffectView;
typedef char kf_map_object_tail_scattered_effect_size[
    sizeof(KfMapObjectTailScatteredEffectView) == 12 ? 1 : -1];
typedef char kf_map_object_tail_scattered_effect_id_offset[
    offsetof(KfMapObjectTailScatteredEffectView, effect_id) == 6 ? 1 : -1];

typedef struct KfMapObjectTailPlacement {
    u32 unknown_34;
    KfMapObjectTailCopyWords copy_words;
} KfMapObjectTailPlacement;
typedef char kf_map_object_tail_placement_size[
    sizeof(KfMapObjectTailPlacement) == 12 ? 1 : -1];

typedef union KfMapObjectTail {
    KfMapObjectTailFields fields;
    KfMapObjectTailMotionView motion;
    KfMapObjectTailNotificationView notification;
    KfMapObjectTailTransitionView transition;
    KfMapObjectTailResourceTriggerView resource_trigger;
    KfMapObjectTailRegionActionView region_action;
    KfMapObjectTailSceneInspectView scene_inspect;
    KfMapObjectTailGoldRewardView gold_reward;
    KfMapObjectTailAnimatedView animated;
    KfMapObjectTailAmbientSoundView ambient_sound;
    KfMapObjectTailCellCopyView cell_copy;
    KfMapObjectTailAction88CellCopyView action_88_cell_copy;
    KfMapObjectTailAction89LayerFadeView action_89_layer_fade;
    KfMapObjectTailAction84PatternView action_84_pattern;
    KfMapObjectTailCollisionProbeView collision_probe;
    KfMapObjectTailLinkedPropertyView linked_property;
    KfMapObjectTailAction83View action_83;
    KfMapObjectTailScaleLinkView scale_link;
    KfMapObjectTailPair38View pair_38;
    KfMapObjectTailMarkerView marker;
    KfMapObjectTailAction51MarkerView action_51_marker;
    KfMapObjectTailInitialRotationView initial_rotation;
    KfMapObjectTailEventEffectView event_effect;
    KfMapObjectTailSpawnByteFields spawn_bytes;
    KfMapObjectTailScatteredEffectView scattered_effect;
    u32 reset_words[3];
    KfMapObjectTailPlacement placement;
} KfMapObjectTail;
typedef char kf_map_object_tail_size[sizeof(KfMapObjectTail) == 12 ? 1 : -1];

/* KfMapObjectRecord40.reaction_mode: ROTATE applies the rotation kick in
 * player_apply_map_object_reaction; any other value recovers the player's
 * position instead. ARMED and TRIGGERED are the view-reaction steps that
 * event_world_dispatch_interaction advances (ARMED -> TRIGGERED). */
enum {
    KF_MAP_OBJECT_REACTION_ROTATE = 0,
    KF_MAP_OBJECT_REACTION_ARMED = 1,
    KF_MAP_OBJECT_REACTION_TRIGGERED = 5
};

/* The +0x40 word points to this heap record while the object runs the player
 * reaction action; other actions keep byte state there. Only this prefix is
 * observed, and the record's complete extent is unresolved. */
typedef struct KfMapObjectRecord40 {
    u8 unknown_00;
    u8 reaction_mode;
    u8 unknown_02[0x0a];
    SVECTOR reaction_rotation_vector;
    u8 unknown_14[0x24];
    s16 reaction_rotation_scale_q15;
} KfMapObjectRecord40;
typedef char kf_map_object_record40_reaction_mode_offset[
    offsetof(KfMapObjectRecord40, reaction_mode) == 0x01 ? 1 : -1];
typedef char kf_map_object_record40_rotation_vector_offset[
    offsetof(KfMapObjectRecord40, reaction_rotation_vector) == 0x0c ? 1 : -1];
typedef char kf_map_object_record40_rotation_scale_offset[
    offsetof(KfMapObjectRecord40, reaction_rotation_scale_q15) == 0x38 ? 1 : -1];

/* Hinged-door progress ticks (KF1 KfMapObjectProgress): the door swings
 * open for 32 ticks and copies its open cells at tick 24, jumps to the
 * hold at 280, starts closing at 300 once unblocked and stops at 332. The
 * placement value IDLE is past the close and parks the door. */
enum {
    KF_MAP_OBJECT_HINGE_PASSABLE = 24,
    KF_MAP_OBJECT_HINGE_OPEN_LAST = 31,
    KF_MAP_OBJECT_HINGE_OPEN_END = 32,
    KF_MAP_OBJECT_HINGE_HOLD_FIRST = 280,
    KF_MAP_OBJECT_HINGE_CLOSE_FIRST = 300,
    KF_MAP_OBJECT_HINGE_CLOSE_END = 332,
    KF_MAP_OBJECT_HINGE_IDLE = 999
};

typedef struct KfMapObjectHingeMotion {
    u16 progress_ticks;
    u16 base_yaw;
} KfMapObjectHingeMotion;
typedef char kf_map_object_hinge_motion_size[
    sizeof(KfMapObjectHingeMotion) == 4 ? 1 : -1];

typedef struct KfMapObjectOffsetMotionState {
    u8 elapsed_frames;
} KfMapObjectOffsetMotionState;

typedef struct KfMapObjectResourceOffsets {
    s8 offset_x;
    s8 offset_z;
    s8 offset_y;
} KfMapObjectResourceOffsets;

typedef struct KfMapObjectLayerFadeState {
    u16 delay_frames_left;
    KfMapLayerMask original_layer_mask;
} KfMapObjectLayerFadeState;
typedef char kf_map_object_layer_fade_state_size[
    sizeof(KfMapObjectLayerFadeState) == 4 ? 1 : -1];

/* Placement kinds 9, 0x15, 0x54, and 0xe2 save the layer before changing
 * visibility; action 0x54 later passes it to map-cell pattern updates. */
typedef struct KfMapObjectSavedLayerState {
    KfMapLayerMask layer_mask;
} KfMapObjectSavedLayerState;

typedef union KfMapObjectExtra40 {
    KfMapObjectRecord40 *record;
    u32 raw;
    s32 bob_base_y;
    u32 next_sound_frame;
    u8 bytes[4];
    u16 object_index;
    s16 angular_velocity_x;
    u16 movement_frames_left;
    KfMapObjectHingeMotion hinge;
    KfMapObjectOffsetMotionState offset_motion;
    KfMapObjectResourceOffsets resource_offsets;
    KfMapObjectLayerFadeState layer_fade;
    KfMapObjectSavedLayerState saved_layer;
} KfMapObjectExtra40;

typedef char kf_map_object_extra40_size[sizeof(KfMapObjectExtra40) == 4 ? 1 : -1];

/* The map-object pool is traversed in 0x44-byte records. */
typedef struct KfMapObject {
    KfMapLayerMask layer_mask;
    KfAnimationClip asset_clip_selector;
    KfRenderQueueMode render_queue_mode;
    KfMapObjectFlags collision_flags;
    KfMapObjectOperation action;
    KfLightingIndex lighting_override_index;
    KF_ENUM_STORAGE(KfObjectId, u16) object_id;
    u16 action_timer;
    u16 phase_q12;
    u16 collision_height;
    s16 render_depth_offset;
    u16 lighting_blend_q12;
    VECTOR position;
    SVECTOR rotation;
    SVECTOR scale;
    KfMapObjectTail tail;
    KfMapObjectExtra40 extra_40;
} KfMapObject;

typedef char kf_map_object_size[sizeof(KfMapObject) == 0x44 ? 1 : -1];
typedef char kf_map_object_asset_clip_selector_offset[
    offsetof(KfMapObject, asset_clip_selector) == 1 ? 1 : -1];
typedef char kf_map_object_action_offset[offsetof(KfMapObject, action) == 4 ? 1 : -1];
typedef char kf_map_object_render_queue_mode_offset[
    offsetof(KfMapObject, render_queue_mode) == 2 ? 1 : -1];
typedef char kf_map_object_lighting_override_index_offset[
    offsetof(KfMapObject, lighting_override_index) == 5 ? 1 : -1];
typedef char kf_map_object_action_timer_offset[offsetof(KfMapObject, action_timer) == 8 ? 1 : -1];
typedef char kf_map_object_phase_q12_offset[
    offsetof(KfMapObject, phase_q12) == 0x0a ? 1 : -1];
typedef char kf_map_object_collision_height_offset[
    offsetof(KfMapObject, collision_height) == 0x0c ? 1 : -1];
typedef char kf_map_object_lighting_blend_q12_offset[
    offsetof(KfMapObject, lighting_blend_q12) == 0x10 ? 1 : -1];
typedef char kf_map_object_position_offset[offsetof(KfMapObject, position) == 0x14 ? 1 : -1];
typedef char kf_map_object_rotation_offset[offsetof(KfMapObject, rotation) == 0x24 ? 1 : -1];
typedef char kf_map_object_scale_offset[offsetof(KfMapObject, scale) == 0x2c ? 1 : -1];
typedef char kf_map_object_tail_offset[offsetof(KfMapObject, tail) == 0x34 ? 1 : -1];
typedef char kf_map_object_spawn_sequence_offset[
    offsetof(KfMapObject, tail.fields.spawn_sequence) == 0x3c ? 1 : -1];
typedef char kf_map_object_record40_offset[
    offsetof(KfMapObject, extra_40) == 0x40 ? 1 : -1];
typedef char kf_map_object_offset_motion_elapsed_offset[
    offsetof(KfMapObject, extra_40.offset_motion.elapsed_frames) == 0x40 ? 1 : -1];
typedef char kf_map_object_saved_layer_mask_offset[
    offsetof(KfMapObject, extra_40.saved_layer.layer_mask) == 0x40 ? 1 : -1];
typedef char kf_map_object_resource_offset_x_offset[
    offsetof(KfMapObject, extra_40.resource_offsets.offset_x) == 0x40 ? 1 : -1];
typedef char kf_map_object_resource_offset_z_offset[
    offsetof(KfMapObject, extra_40.resource_offsets.offset_z) == 0x41 ? 1 : -1];
typedef char kf_map_object_resource_offset_y_offset[
    offsetof(KfMapObject, extra_40.resource_offsets.offset_y) == 0x42 ? 1 : -1];
typedef char kf_map_object_layer_fade_delay_offset[
    offsetof(KfMapObject, extra_40.layer_fade.delay_frames_left) == 0x40 ? 1 : -1];
typedef char kf_map_object_layer_fade_mask_offset[
    offsetof(KfMapObject, extra_40.layer_fade.original_layer_mask) == 0x42 ? 1 : -1];

/* game_main_loop clears the whole region containing the 0x18c-object pool. */
typedef struct KfMapObjectStateGame {
    KfMapObjectTemplate templates[KF_MAP_OBJECT_TEMPLATE_CAPACITY];
    KfMapObject objects[KF_MAP_OBJECT_CAPACITY];
    u8 unknown_8730[4];
    KfMapObjectTemplate *current_template;
    KfMapObject *current_collision_object;
    u8 unknown_873c[2];
    u16 spawn_sequence_pool_15e;
    u16 definition_drop_sequence;
    u16 placement_drop_sequence;
} KfMapObjectStateGame;

typedef char kf_map_object_state_size[sizeof(KfMapObjectStateGame) == 0x8744 ? 1 : -1];
typedef char kf_map_object_state_objects_offset[offsetof(KfMapObjectStateGame, objects) == 0x1e00 ? 1 : -1];
typedef char kf_map_object_state_current_template_offset[
    offsetof(KfMapObjectStateGame, current_template) == 0x8734 ? 1 : -1];
typedef char kf_map_object_state_current_collision_offset[
    offsetof(KfMapObjectStateGame, current_collision_object) == 0x8738 ? 1 : -1];
typedef char kf_map_object_state_counter_873e_offset[
    offsetof(KfMapObjectStateGame, spawn_sequence_pool_15e) == 0x873e ? 1 : -1];
typedef char kf_map_object_state_counter_8742_offset[
    offsetof(KfMapObjectStateGame, placement_drop_sequence) == 0x8742 ? 1 : -1];

extern KfMapObjectStateGame map_objects_slots;

void map_object_start_action_if_idle(KfMapObject *object, KfMapObjectOperation action);
void map_object_initialize_from_placements(const KfMapObjectPlacement *placements);
s32 map_object_find_collision_at_point(s32 x, s32 y, s32 z, s32 radius, s32 height);
KfMapObject *map_object_effect_pool_acquire(s32 first_index, s32 count, s32 sequence);
KfAudioPlaybackResult map_object_play_spatial_sound(KfMapObject *object, s32 sound);
void map_object_reset(KfMapObject *object);
void map_object_pool_reset(void);
void map_object_set_property(s32 index, KfMapObjectProperty property, ...);
void map_object_set_cell_marker(KfMapObject *object, KfMapCellMarkerMode mode, u8 marker);
void map_object_apply_marker_signal(u8 identifier);
KfMapObjectMarkerCheck map_object_check_and_consume_marker(KfMapObject *object, s32 marker);
b32 player_camera_within_map_region(s32 x, s32 z, s32 width, s32 depth, s32 height);
b32 map_object_step_offset_motion(KfMapObject *source, KfMapObject *target,
                  SVECTOR *start_offset, SVECTOR *end_offset,
                  b32 brighten, s32 duration);
void map_object_sample_world_vertex(KfMapObject *object, s32 vertex_index, VECTOR *result);
void map_object_spawn_scattered_effect(u16 effect_id, const VECTOR *origin,
                                       s32 height_offset);
void map_object_spawn_effect(KfMapObjectDropSource source, KF_ENUM_PARAM(KfObjectId, u8) object_id,
                             const VECTOR *position, s32 height_offset);
void map_object_update_actions(void);
void map_object_refresh_cell_markers(KfMapCellMarkerMode mode);
s32 map_object_find_interaction_target(s32 first_index, const VECTOR *position, s32 radius,
    s32 point_height, s32 angle, s32 tolerance);

#endif
