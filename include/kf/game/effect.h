#ifndef KF_GAME_EFFECT_H
#define KF_GAME_EFFECT_H

#include <kf/lib/bool.h>
#include <kf/lib/offsetof.h>
#include <kf/lib/types.h>
#include <kf/game/audio.h>
#include <kf/game/render_types.h>
#include <kf/game/collision_flags.h>
#include <kf/game/magic.h>
#include <kf/game/pool.h>
#include <psyq/sdk.h>

struct KfPoolRecord;

enum {
    KF_EFFECT_CAPACITY = 128,
    KF_MAGIC_RECORD_COUNT = 64
};

/*
 * Effect record type (KF1 KfEffectType). The low three bits choose what the
 * collision probe tests (player, actors, both, or shapes only); bits 4-5 are
 * the source class that actor damage keeps (KfActorDamageFlags): 0x10 casts
 * with the player's magic power, 0x20 marks actor and map hazards. 0xff
 * frees the pool slot.
 */
KF_ENUM_BEGIN(KfEffectType, u8)
    KF_EFFECT_TYPE_NONE = 0,
    KF_EFFECT_TARGET_PLAYER = 1,
    KF_EFFECT_TARGET_ACTORS = 2,
    KF_EFFECT_TARGET_ACTORS_AND_PLAYER = 3,
    KF_EFFECT_TARGET_SHAPES_ONLY = 4,
    KF_EFFECT_TARGET_MASK = 7,
    KF_EFFECT_USE_PLAYER_MAGIC = 0x10,
    KF_EFFECT_SOURCE_HAZARD = 0x20,
    KF_EFFECT_SOURCE_MASK = 0x30,
    KF_EFFECT_SLOT_FREE = 0xff
KF_ENUM_END(KfEffectType)
KF_ENUM_FLAGS(KfEffectType, u8)

/* Effect model row: the renderer and vertex sampler draw asset
 * render_id + 0x28. base_render_id keeps the constructor's row for kinds that
 * switch models. A row is named after the only effect kind that draws it;
 * a row that several kinds share is named after what they have in common. */
KF_ENUM_BEGIN(KfEffectRenderId, u8)
    KF_EFFECT_MODEL_HOLDING_TRAIL = 0,
    KF_EFFECT_MODEL_PROJECTILE = 8,
    KF_EFFECT_MODEL_EXPANDING_RING = 9,
    KF_EFFECT_MODEL_FLAT_SHOT = 10,
    KF_EFFECT_MODEL_RADIAL_BURST = 11,
    KF_EFFECT_MODEL_LOWER_BOUND_RIPPLE = 12,
    KF_EFFECT_MODEL_IMPACT_ORB = 13,
    KF_EFFECT_MODEL_PARTICLE = 14,
    KF_EFFECT_MODEL_DROP_THEN_SHRINK = 15,
    KF_EFFECT_MODEL_COLUMN = 16,
    KF_EFFECT_MODEL_STEPPED_BURST = 17,
    KF_EFFECT_MODEL_IMPACT_BURST = 19,
    KF_EFFECT_MODEL_RING_WAVE_SPREAD = 20,
    KF_EFFECT_MODEL_ACTOR_TURRET = 21,
    KF_EFFECT_MODEL_HOLDING_TRAIL_HOLD = 22,
    KF_EFFECT_MODEL_HOLDING_TRAIL_HEAD = 23,
    KF_EFFECT_MODEL_HOLDING_TRAIL_TAIL = 24,
    KF_EFFECT_MODEL_DEFENSE_BOOST = 25,
    KF_EFFECT_MODEL_ATTACK_BOOST = 26,
    KF_EFFECT_MODEL_ARROW = 28,
    KF_EFFECT_MODEL_BOLT = 29,
    KF_EFFECT_MODEL_GRAVITY_SHOT = 30,
    KF_EFFECT_MODEL_SPINNING_PROJECTILE = 31,
    KF_EFFECT_MODEL_BOUNCE_FADE = 32,
    KF_EFFECT_MODEL_PLAYER_HOMING = 33,
    KF_EFFECT_MODEL_AIMED_MISSILE = 34,
    KF_EFFECT_MODEL_HOLDING_TRAIL_SEGMENT = 35,
    KF_EFFECT_MODEL_RADIAL_HAZARD = 36,
    KF_EFFECT_MODEL_RADIAL_PULSE = 37,
    KF_EFFECT_MODEL_DROP_FROM_ABOVE = 38,
    KF_EFFECT_MODEL_SPINNING_LEVEL = 40,
    KF_EFFECT_MODEL_SPINNING_LEVEL_ALT = 41,
    KF_EFFECT_MODEL_ARROW_VARIANT = 42,
    KF_EFFECT_MODEL_BOLT_VARIANT = 43,
    KF_EFFECT_MODEL_SPRAY = 44,
    KF_EFFECT_MODEL_GROUND_GROWTH_SCATTER = 45,
    KF_EFFECT_MODEL_DROP_THEN_SHRINK_ALT = 46,
    KF_EFFECT_MODEL_PULSE_COLUMN_ALT = 47,
    KF_EFFECT_MODEL_STEPPED_BURST_ALT = 48,
    KF_EFFECT_MODEL_ACTOR_SEEK_GROWTH = 49,
    KF_EFFECT_MODEL_FALLING_DROP_BURST = 50
KF_ENUM_END(KfEffectRenderId)

/* render_flags: the low two bits hide the effect, draw it inside visible map
 * layers, or always draw it; bits 2-3 choose the world, identity, pitch or
 * screen-space transform in render_scene_and_update_resources. */
KF_ENUM_BEGIN(KfEffectRenderFlags, u8)
    KF_EFFECT_RENDER_HIDDEN = 0,
    KF_EFFECT_RENDER_VISIBLE = 1,
    KF_EFFECT_RENDER_ALWAYS_VISIBLE = 2,
    KF_EFFECT_RENDER_VISIBILITY_MASK = 3,
    KF_EFFECT_RENDER_WORLD_TRANSFORM = 0,
    KF_EFFECT_RENDER_IDENTITY_TRANSFORM = 4,
    KF_EFFECT_RENDER_PITCH_TRANSFORM = 8,
    KF_EFFECT_RENDER_SCREEN_SPACE = 0x0c,
    KF_EFFECT_RENDER_TRANSFORM_MASK = 0x0c
KF_ENUM_END(KfEffectRenderFlags)
KF_ENUM_FLAGS(KfEffectRenderFlags, u8)

/*
 * Stage byte at payload +0 for the kinds that run a small state machine
 * there. Each kind uses its own members of this one byte domain: kinds 1 and
 * 28 travel, fall after a first hit and fade; kinds 26 and 27 travel and
 * shrink; kind 46 tracks its linked effect, then collapses; kind 50 charges
 * at the weapon, flies and bursts.
 */
KF_ENUM_BEGIN(KfEffectStage, u8)
    KF_EFFECT_STAGE_TRAVEL = 0,
    KF_EFFECT_STAGE_BOUNCED = 1,
    KF_EFFECT_STAGE_FADE = 2,
    KF_EFFECT_STAGE_SHRINK = 1,
    KF_EFFECT_STAGE_TRACK = 0,
    KF_EFFECT_STAGE_COLLAPSE = 1,
    KF_EFFECT_STAGE_CHARGE = 0,
    KF_EFFECT_STAGE_FLIGHT = 1,
    KF_EFFECT_STAGE_BURST = 2
KF_ENUM_END(KfEffectStage)

/* Result of the effect motion helpers: the step was clear, the moved
 * position collided, or effect_target_motion was already within its minimum
 * distance of the target. */
KF_ENUM_BEGIN(KfEffectMotionResult, s32)
    KF_EFFECT_MOTION_ARRIVED = -2,
    KF_EFFECT_MOTION_BLOCKED = -1,
    KF_EFFECT_MOTION_CLEAR = 0
KF_ENUM_END(KfEffectMotionResult)

typedef union KfEffectScaleThreshold {
    s16 interpolation_start_y;
    s16 next_probe_phase;
} KfEffectScaleThreshold;

typedef char kf_effect_scale_threshold_size[sizeof(KfEffectScaleThreshold) == 2 ? 1 : -1];

/* Kind 6 copies a position and rotation into each 24-byte trail row. */
typedef struct KfEffectTrailRow {
    VECTOR position;
    SVECTOR rotation;
} KfEffectTrailRow;

typedef char kf_effect_trail_row_size[sizeof(KfEffectTrailRow) == 24 ? 1 : -1];
typedef char kf_effect_trail_rotation_offset[offsetof(KfEffectTrailRow, rotation) == 16 ? 1 : -1];

/* Kinds 6 and 107 share this view of kind 6's payload at record +0x40. */
typedef struct KfEffectTrailState {
    KfEffectTrailRow *rows;
    u8 frame_index;
    u8 phase_counter;
    u8 actor_index;
} KfEffectTrailState;

typedef char kf_effect_trail_state_size[sizeof(KfEffectTrailState) == 8 ? 1 : -1];
typedef char kf_effect_trail_state_frame_offset[offsetof(KfEffectTrailState, frame_index) == 4 ? 1 : -1];
typedef char kf_effect_trail_state_actor_offset[offsetof(KfEffectTrailState, actor_index) == 6 ? 1 : -1];

typedef struct KfEffectKind102Payload {
    s16 amplitude;
} KfEffectKind102Payload;

typedef char kf_effect_kind102_payload_size[sizeof(KfEffectKind102Payload) == 2 ? 1 : -1];

/* Ballistic kinds keep the launch Y and elapsed update count in the payload. */
typedef struct KfEffectBallisticState {
    s16 origin_y;
    s16 age;
} KfEffectBallisticState;

typedef char kf_effect_ballistic_state_size[sizeof(KfEffectBallisticState) == 4 ? 1 : -1];
typedef char kf_effect_ballistic_age_offset[offsetof(KfEffectBallisticState, age) == 2 ? 1 : -1];

/* Kind 23 tracks an actor vertex until its animation reaches a phase threshold. */
typedef struct KfEffectKind23Attachment {
    s16 actor_index;
    s16 vertex_index;
    u16 release_animation_phase;
} KfEffectKind23Attachment;

typedef char kf_effect_kind23_attachment_size[sizeof(KfEffectKind23Attachment) == 6 ? 1 : -1];
typedef char kf_effect_kind23_vertex_offset[offsetof(KfEffectKind23Attachment, vertex_index) == 2 ? 1 : -1];
typedef char kf_effect_kind23_phase_offset[offsetof(KfEffectKind23Attachment, release_animation_phase) == 4 ? 1 : -1];

/* Kind 46 tracks a linked effect before fading its captured Y scale. */
typedef struct KfEffectKind46State {
    KF_ENUM_STORAGE(KfEffectStage, s8) phase;
    s8 linked_effect_index;
    u16 age_q12;
} KfEffectKind46State;

typedef char kf_effect_kind46_state_size[sizeof(KfEffectKind46State) == 4 ? 1 : -1];
typedef char kf_effect_kind46_age_offset[offsetof(KfEffectKind46State, age_q12) == 2 ? 1 : -1];

/* Kind 5 fans out into kind-105 children attached to one actor. */
typedef struct KfEffectKind5Fanout {
    u8 initial_child_count;
    u8 actor_index;
    s16 children_remaining;
} KfEffectKind5Fanout;

typedef char kf_effect_kind5_fanout_size[sizeof(KfEffectKind5Fanout) == 4 ? 1 : -1];
typedef char kf_effect_kind5_remaining_offset[offsetof(KfEffectKind5Fanout, children_remaining) == 2 ? 1 : -1];

/* Kind 111 follows an actor, with 0xff selecting the effect's own position. */
typedef struct KfEffectKind111Target {
    s16 actor_index;
} KfEffectKind111Target;

typedef char kf_effect_kind111_target_size[sizeof(KfEffectKind111Target) == 2 ? 1 : -1];

/* Kinds 103 and 121 decrement this count between fanout spawns. */
typedef struct KfEffectKind103Counter {
    u16 remaining;
} KfEffectKind103Counter;

typedef char kf_effect_kind103_counter_size[sizeof(KfEffectKind103Counter) == 2 ? 1 : -1];

/* Kind 2 grows a radial effect until it reaches the requested scale. */
typedef struct KfEffectKind2Scale {
    s16 max_scale;
    s16 scale_step;
    s16 radial_damage_parameter;
} KfEffectKind2Scale;

typedef char kf_effect_kind2_scale_size[sizeof(KfEffectKind2Scale) == 6 ? 1 : -1];
typedef char kf_effect_kind2_step_offset[offsetof(KfEffectKind2Scale, scale_step) == 2 ? 1 : -1];
typedef char kf_effect_kind2_damage_offset[offsetof(KfEffectKind2Scale, radial_damage_parameter) == 4 ? 1 : -1];

/* Several collision effects apply the current magic once per contact. */
typedef struct KfEffectCollisionLatch {
    u8 impact_handled;
} KfEffectCollisionLatch;

typedef char kf_effect_collision_latch_size[sizeof(KfEffectCollisionLatch) == 1 ? 1 : -1];

/* Kind 42 counts down before its scale step. */
typedef struct KfEffectKind42Countdown {
    s8 ticks_remaining;
} KfEffectKind42Countdown;

typedef char kf_effect_kind42_countdown_size[sizeof(KfEffectKind42Countdown) == 1 ? 1 : -1];

/* Kinds 1 and 28 advance through initial, collision, and fade stages. */
typedef struct KfEffectKind1Stage {
    KfEffectStage collision_stage;
} KfEffectKind1Stage;

typedef char kf_effect_kind1_stage_size[sizeof(KfEffectKind1Stage) == 1 ? 1 : -1];

/* Kinds 26 and 27 travel until blocked, then shrink away. */
typedef struct KfEffectKind26Stage {
    KF_ENUM_STORAGE(KfEffectStage, s8) stage;
} KfEffectKind26Stage;

typedef char kf_effect_kind26_stage_size[sizeof(KfEffectKind26Stage) == 1 ? 1 : -1];

/* Kind 50 advances from growth to collision response and cleanup. */
typedef struct KfEffectKind50Stage {
    KF_ENUM_STORAGE(KfEffectStage, s8) stage;
} KfEffectKind50Stage;

typedef char kf_effect_kind50_stage_size[sizeof(KfEffectKind50Stage) == 1 ? 1 : -1];

/* Kind 106 waits for all fifteen spawned kind-8 children to finish. */
typedef struct KfEffectKind106Children {
    u8 children_remaining;
} KfEffectKind106Children;

typedef char kf_effect_kind106_children_size[sizeof(KfEffectKind106Children) == 1 ? 1 : -1];

/* Kinds 11, 45, and 54 pass this signed step to effect_scale_step. */
typedef struct KfEffectScaleStepArgument {
    s16 scale_step;
} KfEffectScaleStepArgument;

typedef char kf_effect_scale_step_argument_size[sizeof(KfEffectScaleStepArgument) == 2 ? 1 : -1];

/* Kind 107 follows one kind-6 record at an offset of three frames per row. */
typedef struct KfEffectTrailChildLink {
    u8 parent_index;
    u8 lag_index;
} KfEffectTrailChildLink;

typedef char kf_effect_trail_child_link_size[sizeof(KfEffectTrailChildLink) == 2 ? 1 : -1];
typedef char kf_effect_trail_child_lag_offset[offsetof(KfEffectTrailChildLink, lag_index) == 1 ? 1 : -1];

/* Kind 8 follows a parent effect and applies a per-tick vertical step. */
typedef struct KfEffectKind8State {
    u8 parent_index;
    u16 vertical_step;
} KfEffectKind8State;

typedef char kf_effect_kind8_state_size[sizeof(KfEffectKind8State) == 4 ? 1 : -1];
typedef char kf_effect_kind8_vertical_step_offset[offsetof(KfEffectKind8State, vertical_step) == 2 ? 1 : -1];

/* Actor-index payload bytes of kinds 5, 9, 105 and 111 (and the player
 * dispatcher's optional target word): 0xff names no actor, which kind 111
 * treats as its own position; kind 9 also accepts 0xfe for the player. */
enum {
    KF_EFFECT_TARGET_ACTOR_PLAYER = 0xfe,
    KF_EFFECT_TARGET_ACTOR_NONE = 0xff
};

typedef struct KfEffectKind9Target {
    u8 actor_index;
} KfEffectKind9Target;

typedef char kf_effect_kind9_target_size[sizeof(KfEffectKind9Target) == 1 ? 1 : -1];

/* Kind 109 follows another record in the effect pool. */
typedef struct KfEffectKind109Target {
    u8 effect_index;
} KfEffectKind109Target;

typedef char kf_effect_kind109_target_size[sizeof(KfEffectKind109Target) == 1 ? 1 : -1];

/* Kind 114 keeps its launch Y after the preceding four payload bytes. */
typedef struct KfEffectKind114State {
    u8 unused_40[4];
    s32 origin_y;
} KfEffectKind114State;

typedef char kf_effect_kind114_state_size[sizeof(KfEffectKind114State) == 8 ? 1 : -1];
typedef char kf_effect_kind114_origin_y_offset[offsetof(KfEffectKind114State, origin_y) == 4 ? 1 : -1];

typedef struct KfEffectKind101Motion {
    u16 scale_step;
    u16 vertical_step;
} KfEffectKind101Motion;

typedef char kf_effect_kind101_motion_size[sizeof(KfEffectKind101Motion) == 4 ? 1 : -1];
typedef char kf_effect_kind101_vertical_step_offset[offsetof(KfEffectKind101Motion, vertical_step) == 2 ? 1 : -1];

typedef struct KfEffectKind10Targeting {
    u8 actor_index;
    u8 emissions_remaining;
} KfEffectKind10Targeting;

typedef char kf_effect_kind10_targeting_size[sizeof(KfEffectKind10Targeting) == 2 ? 1 : -1];

/* Kind 105 follows an animation vertex and reports to its parent effect. */
typedef struct KfEffectKind105Attachment {
    u8 parent_index;
    u8 actor_index;
    s16 vertex_index;
} KfEffectKind105Attachment;

typedef char kf_effect_kind105_attachment_size[sizeof(KfEffectKind105Attachment) == 4 ? 1 : -1];
typedef char kf_effect_kind105_vertex_offset[offsetof(KfEffectKind105Attachment, vertex_index) == 2 ? 1 : -1];

typedef struct KfEffectKind12Aim {
    s16 max_length;
    s16 scale;
    s16 turn_step;
    s16 close_scale;
} KfEffectKind12Aim;

typedef char kf_effect_kind12_aim_size[sizeof(KfEffectKind12Aim) == 8 ? 1 : -1];
typedef char kf_effect_kind12_close_scale_offset[offsetof(KfEffectKind12Aim, close_scale) == 6 ? 1 : -1];

typedef union KfEffectKindPayload {
    u8 raw[8];
    KfEffectTrailState trail;
    KfEffectKind102Payload kind102;
    KfEffectBallisticState ballistic;
    KfEffectKind23Attachment kind23;
    KfEffectKind46State kind46;
    KfEffectKind5Fanout kind5;
    KfEffectKind111Target kind111;
    KfEffectKind103Counter kind103;
    KfEffectKind2Scale kind2;
    KfEffectCollisionLatch collision_latch;
    KfEffectKind42Countdown kind42;
    KfEffectKind1Stage kind1;
    KfEffectKind26Stage kind26;
    KfEffectKind50Stage kind50;
    KfEffectKind106Children kind106;
    KfEffectScaleStepArgument scale_step_argument;
    KfEffectTrailChildLink trail_child;
    KfEffectKind8State kind8;
    KfEffectKind9Target kind9;
    KfEffectKind109Target kind109;
    KfEffectKind114State kind114;
    KfEffectKind101Motion kind101;
    KfEffectKind10Targeting kind10;
    KfEffectKind105Attachment kind105;
    KfEffectKind12Aim kind12;
} KfEffectKindPayload;

typedef char kf_effect_kind_payload_size[sizeof(KfEffectKindPayload) == 8 ? 1 : -1];

/* Renderer-owned cache slot followed by the effect kind's variant payload. */
typedef struct KfEffectCacheTail {
    struct KfPoolRecord *animation_cache;
    KfEffectKindPayload payload;
} KfEffectCacheTail;

typedef char kf_effect_cache_tail_size[sizeof(KfEffectCacheTail) == 12 ? 1 : -1];
typedef char kf_effect_cache_payload_offset[offsetof(KfEffectCacheTail, payload) == 4 ? 1 : -1];

/* The pool scan and reset visit 128 records at a 72-byte stride. */
typedef struct KfEffectRecord {
    KfEffectType type;
    KfEffectKind kind;
    KfEffectRenderId base_render_id;
    KfEffectRenderId render_id;
    KfAnimationClip animation_clip;
    u8 zeroed_on_construct;
    u8 damage_multiplier_tenths;
    u8 phase;
    KfEffectRenderFlags render_flags;
    KfRenderQueueMode render_queue_mode;
    KfMapLayerMask map_layer_mask;
    u8 cooldown;
    KfLightingIndex lighting_override_index;
    b8 midpoint_collision_enabled;
    s16 updates_remaining;
    s16 lighting_blend_q12;
    u16 animation_phase_q12;
    VECTOR position;
    SVECTOR rotation;
    s16 scale_x;
    s16 scale_y;
    s16 scale_z;
    KfEffectScaleThreshold scale_threshold;
    SVECTOR direction;
    KfEffectCacheTail cache_tail;
} KfEffectRecord;

typedef char kf_effect_record_size[sizeof(KfEffectRecord) == 72 ? 1 : -1];
typedef char kf_effect_damage_multiplier_offset[offsetof(KfEffectRecord, damage_multiplier_tenths) == 0x06 ? 1 : -1];
typedef char kf_effect_render_flags_offset[offsetof(KfEffectRecord, render_flags) == 0x08 ? 1 : -1];
typedef char kf_effect_map_layer_mask_offset[offsetof(KfEffectRecord, map_layer_mask) == 0x0a ? 1 : -1];
typedef char kf_effect_render_queue_mode_offset[offsetof(KfEffectRecord, render_queue_mode) == 0x09 ? 1 : -1];
typedef char kf_effect_lighting_override_offset[offsetof(KfEffectRecord, lighting_override_index) == 0x0c ? 1 : -1];
typedef char kf_effect_midpoint_collision_offset[offsetof(KfEffectRecord, midpoint_collision_enabled) == 0x0d ? 1 : -1];
typedef char kf_effect_lighting_blend_offset[offsetof(KfEffectRecord, lighting_blend_q12) == 0x10 ? 1 : -1];
typedef char kf_effect_animation_phase_offset[offsetof(KfEffectRecord, animation_phase_q12) == 0x12 ? 1 : -1];
typedef char kf_effect_position_offset[offsetof(KfEffectRecord, position) == 0x14 ? 1 : -1];
typedef char kf_effect_scale_offset[offsetof(KfEffectRecord, scale_x) == 0x2c ? 1 : -1];
typedef char kf_effect_scale_threshold_offset[offsetof(KfEffectRecord, scale_threshold) == 0x32 ? 1 : -1];
typedef char kf_effect_direction_offset[offsetof(KfEffectRecord, direction) == 0x34 ? 1 : -1];
typedef char kf_effect_cache_tail_offset[offsetof(KfEffectRecord, cache_tail) == 0x3c ? 1 : -1];

typedef char kf_effect_trail_payload_offset[offsetof(KfEffectRecord, cache_tail.payload.trail) == 0x40 ? 1 : -1];

/* The effect sweep indexes this 26-byte row family by the record kind. */
typedef struct KfMagicRecord {
    u8 menu_available;
    u8 charge_rate;
    u8 unused_02[2];
    u8 player_status_flags;
    u16 damage_components[8];
    u16 mp_cost;
    u8 unused_18[2];
} KfMagicRecord;

typedef char kf_magic_record_size[sizeof(KfMagicRecord) == 26 ? 1 : -1];
typedef char kf_magic_record_charge_rate_offset[offsetof(KfMagicRecord, charge_rate) == 1 ? 1 : -1];
typedef char kf_magic_record_damage_components_offset[offsetof(KfMagicRecord, damage_components) == 0x06 ? 1 : -1];
typedef char kf_magic_record_status_flags_offset[offsetof(KfMagicRecord, player_status_flags) == 0x04 ? 1 : -1];
typedef char kf_magic_record_mp_cost_offset[offsetof(KfMagicRecord, mp_cost) == 0x16 ? 1 : -1];

/* game_main_loop clears this complete region at startup. */
typedef struct KfEffectState {
    KfMagicRecord magic_records[KF_MAGIC_RECORD_COUNT];
    KfEffectRecord records[KF_EFFECT_CAPACITY];
    KfMagicRecord *current_magic;
    KfEffectRecord *current_record;
    s32 current_index;
} KfEffectState;

typedef char kf_effect_state_size[sizeof(KfEffectState) == 0x2a8c ? 1 : -1];
typedef char kf_effect_records_offset[offsetof(KfEffectState, records) == 0x680 ? 1 : -1];
typedef char kf_effect_current_magic_offset[offsetof(KfEffectState, current_magic) == 0x2a80 ? 1 : -1];
typedef char kf_effect_current_index_offset[offsetof(KfEffectState, current_index) == 0x2a88 ? 1 : -1];

extern KfEffectState effect_status;

int effect_magic_power(KfEffectRecord *effect);
void effect_dispatch_magic_impact(KF_ENUM_PARAM(KfCollisionHitFlags, s32) kind,
                                  KF_ENUM_PARAM(KfActorDamageFlags, s32) source_flags,
                   s32 radius, u16 power,
                   u8 damage_multiplier_tenths, u16 magic_06, u16 magic_08, u16 magic_0a,
                   u16 magic_04, u16 magic_0c, u16 magic_0e, u16 magic_10,
                   u16 magic_12, u16 magic_14, const VECTOR *position);
KF_ENUM_PARAM(KfCollisionHitFlags, s32) effect_probe_collision_by_type(const VECTOR *position, s32 radius,
    s32 height_flags);
void effect_apply_current_magic(KF_ENUM_PARAM(KfCollisionHitFlags, s32) kind, s32 radius,
                                const VECTOR *position);
void effect_apply_current_magic_backstep(KF_ENUM_PARAM(KfCollisionHitFlags, s32) kind);
void effect_apply_radial_magic_damage(VECTOR *position, s32 start, s32 end,
                                      s32 mode, s32 falloff, s32 scale_and_flags);
KF_ENUM_PARAM(KfEffectMotionResult, s32) effect_move_probe(s32 scale, s32 max_length, s32 probe_radius,
                      s32 probe_height_flags, SVECTOR *motion);
KF_ENUM_PARAM(KfEffectMotionResult, s32) effect_aim_and_move(s32 max_length, s32 scale, s32 turn_step,
                        s32 probe_radius, s32 probe_height_flags, s32 proximity,
                        s32 close_scale, s32 target_filter);
KF_ENUM_PARAM(KfEffectMotionResult, s32) effect_target_motion(const VECTOR *target, s32 max_length, s32 scale,
                         s32 settle_distance, s32 min_distance,
                         s32 probe_radius, s32 probe_height_flags);
void effect_scale_step(s32 multiplier, s32 limit, s32 increment,
                       s32 falloff, s32 mode);
void effect_spawn_zero_direction(KfEffectRecord *record, s32 mode);
b32 effect_spawn_at_lower_bound(const VECTOR *position, s32 scale, s32 amplitude,
                                s32 vertical_window);
void effect_spawn_motion(KfEffectRecord *record, s32 position_mode,
                   s32 motion_mode, s32 scale, s32 scale_step, s32 updates_remaining,
                   s32 render_id, s32 vertical_step, ...);
b32 effect_scatter_lower_bound(const VECTOR *origin, s32 count, s32 spread,
                  s32 scale_x, s32 scale_z, s32 variation);
KF_ENUM_PARAM(KfCollisionHitFlags, s32) effect_collision_step(s32 radius, s32 angle, s32 step);
void effect_collision_backtrack(void);
void effect_spawn_radial_ring(s32 count, s32 radius, s32 vertical_angle, s32 vertical_step);
KfAudioPlaybackResult effect_play_spatial_sound(KfEffectRecord *effect, s32 sound);
KfEffectRecord *effect_pool_find_free(void);
KfEffectRecord *effect_construct_record(u8 damage_multiplier_tenths, KfEffectType type, KfEffectKind kind,
                              const VECTOR *position,
                              const SVECTOR *direction, ...);
void effect_sample_rotated_vertex(KfEffectRecord *record, s32 mode, VECTOR *output,
                   const SVECTOR *scale);
void effect_sample_world_vertex(KfEffectRecord *record, s32 mode, VECTOR *position,
                   const SVECTOR *scale);
void effect_pool_initialize_scaled(KfEffectRecord *record, KfEffectRenderId render_id, u16 scale);
void effect_pool_initialize_fixed(KfEffectRecord *record, KfEffectRenderId render_id);
void effect_pool_reset(void);
void effect_rotate_scale_offset_y(const SVECTOR *offset, VECTOR *output, s16 angle, s32 scale);
void magic_load_records(const KfMagicRecord *records);
void effect_pool_sweep(void);
void effect_update_dispatch(void);

#endif
