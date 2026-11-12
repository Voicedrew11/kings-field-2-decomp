#ifndef KF_GAME_ACTOR_H
#define KF_GAME_ACTOR_H

#include <kf/lib/bool.h>
#include <kf/lib/math.h>
#include <kf/lib/offsetof.h>
#include <kf/lib/types.h>
#include <kf/game/render_types.h>
#include <kf/game/collision_flags.h>
#include <kf/game/item.h>
#include <kf/game/magic.h>
#include <kf/game/pool.h>
#include <psyq/sdk.h>

enum {
    KF_ACTOR_ANIMATION_PHASE_PERIOD = 0x1000,
    KF_ACTOR_ANIMATION_PHASE_MAX = KF_ACTOR_ANIMATION_PHASE_PERIOD - 1,
    KF_ACTOR_CAPACITY = 200,
    KF_ACTOR_DYNAMIC_START = 190,
    KF_ACTOR_DYNAMIC_COUNT = KF_ACTOR_CAPACITY - KF_ACTOR_DYNAMIC_START,
    KF_ACTOR_PITCH_TRACK_TARGET = -1,
    /* actor_turn_and_move_toward_point: no reference heading on input; on
     * return, the actor reached the point after passing that heading. */
    KF_ACTOR_HEADING_NONE = -1,
    /* Actor-overlap finders return no actor index. */
    KF_ACTOR_INDEX_NONE = -1,
    KF_ACTOR_PLACEMENT_KEEP_INITIAL_YAW = 1,
    /* An unused target-group row ends the loaded group list. */
    KF_TARGET_GROUP_DEFINITION_END = 0xff
};

/*
 * Actor flag word, seeded from KfTargetGroup.initial_actor_flags. The low
 * two bits are the packed position mode read by actor_resolve_group_position;
 * bits 14-15 extend the collision height argument (shifted into bits 30-31).
 * NO_STEP_UP makes the horizontal mover treat shape hits as walls,
 * IGNORE_MAGIC_REACTION skips the magic-hit retarget and
 * IGNORE_HAZARD_DAMAGE ignores hazard-class damage; PLAYER_OVERLAP_BOB starts
 * the player's overlap bob when the player stands inside the actor.
 * DIE_WITH_LINKED sends a linked companion into its zero-health target type
 * when the actor it is linked to leaves the active lifecycle.
 */
KF_ENUM_BEGIN(KfActorFlags, u32)
    KF_ACTOR_FLAGS_NONE = 0,
    KF_ACTOR_POSITION_MODE_MASK = 0x3,
    KF_ACTOR_POSITION_DIRECT = 0,
    KF_ACTOR_POSITION_GROUP_OFFSET = 1,
    KF_ACTOR_POSITION_ROTATED_GROUP_OFFSET = 2,
    KF_ACTOR_FLAG_STATIC_COLLISION_ONLY = 0x4,
    KF_ACTOR_FLAG_PLAYER_OVERLAP_BOB = 0x8,
    KF_ACTOR_FLAG_LINKED = 0x10,
    KF_ACTOR_FLAG_RENDER_WITH_IDENTITY_MATRIX = 0x20,
    KF_ACTOR_FLAG_BLENDED_MODEL = 0x80,
    KF_ACTOR_FLAG_BLOCK_PLAYER_TARGETING = 0x100,
    KF_ACTOR_FLAG_DIE_WITH_LINKED = 0x200,
    KF_ACTOR_FLAG_USE_MAP_LAYER_FLOOR = 0x400,
    KF_ACTOR_FLAG_EFFECT_ANIMATION_HOLD = 0x800,
    KF_ACTOR_FLAG_RENDER_INCLUDE_LAYER_0X20 = 0x2000,
    KF_ACTOR_FLAG_NO_STEP_UP = 0x4000,
    KF_ACTOR_FLAG_COLLISION_HEIGHT_MASK = 0xc000,
    KF_ACTOR_FLAG_MAP_OBJECT_ATTACHED = 0x10000,
    KF_ACTOR_FLAG_CONE_TARGET_PRIORITY = 0x20000,
    KF_ACTOR_FLAG_IGNORE_MAGIC_REACTION = 0x40000,
    KF_ACTOR_FLAG_RENDER_RADIUS_VISIBILITY = 0x80000,
    KF_ACTOR_FLAG_IGNORE_HAZARD_DAMAGE = 0x100000
KF_ENUM_END(KfActorFlags)
KF_ENUM_FLAGS(KfActorFlags, u32)

/* Progress of the current target's action. actor_set_target starts at
 * ENTRY; the behaviour case does its entry work and moves to
 * RETARGET_BLOCKED, then RETARGET_ALLOWED lets actor_select_best_target pick
 * another candidate. UNSELECTED follows a cleared target. */
KF_ENUM_BEGIN(KfActorTargetActionState, u8)
    KF_ACTOR_TARGET_ACTION_ENTRY = 0,
    KF_ACTOR_TARGET_ACTION_RETARGET_BLOCKED = 0xf0,
    KF_ACTOR_TARGET_ACTION_RETARGET_ALLOWED = 0xf1,
    KF_ACTOR_TARGET_ACTION_UNSELECTED = 0xff
KF_ENUM_END(KfActorTargetActionState)

/*
 * actor_move_horizontal_with_collision options. SLIDE retries a blocked step
 * along one axis or a diagonal wall (SLIDE_KEEP_SPEED keeps the full step
 * length); PROBE_LEDGE reports a drop ahead as KF_COLLISION_HIT_LEDGE and
 * AVOID_LEDGE also refuses that step; NO_STEP_UP treats every shape hit as a
 * wall; STORE_MOTION writes the final step back to the caller's motion.
 */
KF_ENUM_BEGIN(KfActorMoveFlags, s32)
    KF_ACTOR_MOVE_NONE = 0,
    KF_ACTOR_MOVE_SLIDE_KEEP_SPEED = 0x01,
    KF_ACTOR_MOVE_SLIDE = 0x02,
    KF_ACTOR_MOVE_AVOID_LEDGE = 0x04,
    KF_ACTOR_MOVE_STORE_MOTION = 0x08,
    KF_ACTOR_MOVE_NO_STEP_UP = 0x10,
    KF_ACTOR_MOVE_PROBE_LEDGE = 0x20
KF_ENUM_END(KfActorMoveFlags)
KF_ENUM_FLAGS(KfActorMoveFlags, s32)

/* actor_move_along_euler_angles: which part of the 3D step was blocked. */
KF_ENUM_BEGIN(KfActorEulerMoveResult, s32)
    KF_ACTOR_EULER_MOVE_CLEAR = 0,
    KF_ACTOR_EULER_BLOCKED_HORIZONTAL = 1,
    KF_ACTOR_EULER_BLOCKED_VERTICAL = 2
KF_ENUM_END(KfActorEulerMoveResult)
KF_ENUM_FLAGS(KfActorEulerMoveResult, s32)

/* Activation state, independent of the slot policy (KF1 KfActorLifecycle). */
KF_ENUM_BEGIN(KfActorLifecycle, u8)
    KF_ACTOR_LIFECYCLE_DORMANT = 0,
    KF_ACTOR_LIFECYCLE_ACTIVE = 1,
    KF_ACTOR_LIFECYCLE_WAIT_FOR_RANGE_EXIT = 2,
    KF_ACTOR_LIFECYCLE_DISABLED = 3
KF_ENUM_END(KfActorLifecycle)

/* actor_update_vertical_motion state (KF1 KfActorVerticalState): NONE runs
 * the floor check only; VELOCITY integrates rise/fall velocity; FALLING
 * applies gravity to the floor; BALLISTIC follows the launch arc; SUSPENDED
 * skips the update. */
KF_ENUM_BEGIN(KfActorVerticalState, u8)
    KF_ACTOR_VERTICAL_MOTION_NONE = 0,
    KF_ACTOR_VERTICAL_MOTION_VELOCITY = 0x10,
    KF_ACTOR_VERTICAL_MOTION_FALLING = 0x20,
    KF_ACTOR_VERTICAL_MOTION_BALLISTIC = 0x30,
    KF_ACTOR_VERTICAL_MOTION_SUSPENDED = 0x60
KF_ENUM_END(KfActorVerticalState)

/*
 * Slot policy byte of a placed or spawned actor (KF1 KfActorSlotState). The
 * loader copies it from KfActorLoadRecord; 0xff frees the slot and ends the
 * placement stream. Slot 0 is the placement default and keeps a WIP name.
 */
KF_ENUM_BEGIN(KfActorSlotState, u8)
    KF_ACTOR_SLOT_0 = 0,
    KF_ACTOR_SLOT_PERSISTENT = 1,
    KF_ACTOR_SLOT_RESPAWNING = 2,
    KF_ACTOR_SLOT_HOMEBOUND = 3,
    KF_ACTOR_SLOT_LINKED_COMPANION = 4,
    KF_ACTOR_SLOT_EFFECT_SPAWNED = 5,
    KF_ACTOR_SLOT_FREE = 0xff
KF_ENUM_END(KfActorSlotState)

/*
 * Candidate behaviour type. actor_set_target copies a candidate's type into
 * the actor's current target type, actor_find_target_of_type matches it, and
 * actor_update_behavior / actor_score_target_candidate dispatch on it. Types
 * from 128 are scored by the active map's callback; 0xff is no target (and a
 * disabled candidate row). Other members keep decimal WIP names.
 */
KF_ENUM_BEGIN(KfActorTargetType, u8)
    KF_ACTOR_TARGET_0 = 0,
    KF_ACTOR_TARGET_1 = 1,
    KF_ACTOR_TARGET_2 = 2,
    KF_ACTOR_TARGET_3 = 3,
    KF_ACTOR_TARGET_4 = 4,
    KF_ACTOR_TARGET_5 = 5,
    KF_ACTOR_TARGET_6 = 6,
    KF_ACTOR_TARGET_7 = 7,
    KF_ACTOR_TARGET_8 = 8,
    KF_ACTOR_TARGET_9 = 9,
    KF_ACTOR_TARGET_10 = 10,
    KF_ACTOR_TARGET_11 = 11,
    KF_ACTOR_TARGET_12 = 12,
    KF_ACTOR_TARGET_13 = 13,
    KF_ACTOR_TARGET_14 = 14,
    KF_ACTOR_TARGET_15 = 15,
    KF_ACTOR_TARGET_16 = 16,
    KF_ACTOR_TARGET_17 = 17,
    KF_ACTOR_TARGET_18 = 18,
    KF_ACTOR_TARGET_19 = 19,
    KF_ACTOR_TARGET_20 = 20,
    KF_ACTOR_TARGET_21 = 21,
    KF_ACTOR_TARGET_22 = 22,
    KF_ACTOR_TARGET_23 = 23,
    KF_ACTOR_TARGET_24 = 24,
    KF_ACTOR_TARGET_25 = 25,
    KF_ACTOR_TARGET_26 = 26,
    KF_ACTOR_TARGET_27 = 27,
    KF_ACTOR_TARGET_28 = 28,
    KF_ACTOR_TARGET_ASCENDING_SPIN = 29,
    KF_ACTOR_TARGET_COLLISION_MOVE = 30,
    KF_ACTOR_TARGET_EVENT_STREAM = 0x70,
    KF_ACTOR_TARGET_MAP_CALLBACK_FIRST = 0x80,
    KF_ACTOR_TARGET_130 = 130,
    KF_ACTOR_TARGET_132 = 132,
    KF_ACTOR_TARGET_240 = 240,
    KF_ACTOR_TARGET_NONE = 0xff
KF_ENUM_END(KfActorTargetType)

typedef struct KfTargetCandidateWord0cBytes {
    u8 low;
    u8 high;
} KfTargetCandidateWord0cBytes;
typedef char kf_target_candidate_word0c_bytes_size[
    sizeof(KfTargetCandidateWord0cBytes) == 2 ? 1 : -1];

typedef struct KfTargetCandidateSelectionScales {
    u8 initial_score_scale;
    u8 continuing_score_scale;
} KfTargetCandidateSelectionScales;
typedef char kf_target_candidate_selection_scales_size[
    sizeof(KfTargetCandidateSelectionScales) == 2 ? 1 : -1];

typedef struct KfTargetCandidateDamageReaction {
    u8 reaction_chance;
    u8 unused;
} KfTargetCandidateDamageReaction;
typedef char kf_target_candidate_damage_reaction_size[
    sizeof(KfTargetCandidateDamageReaction) == 2 ? 1 : -1];

typedef union KfTargetCandidateWord02 {
    KfTargetCandidateSelectionScales target_selection;
    KfTargetCandidateDamageReaction damage_reaction;
} KfTargetCandidateWord02;
typedef char kf_target_candidate_word02_size[
    sizeof(KfTargetCandidateWord02) == 2 ? 1 : -1];

/* A dying actor's candidate drops this object with chance high/256. */
typedef struct KfTargetCandidateDeathDrop {
    KF_ENUM_STORAGE(KfObjectId, u8) object_id;
    u8 chance;
} KfTargetCandidateDeathDrop;
typedef char kf_target_candidate_death_drop_size[
    sizeof(KfTargetCandidateDeathDrop) == 2 ? 1 : -1];

/* Type-25 scripts dispatch this effect kind through actor_dispatch_group_effect. */
typedef struct KfTargetCandidateScriptEffect {
    KfEffectKind kind;
    u8 unknown_01;
} KfTargetCandidateScriptEffect;
typedef char kf_target_candidate_script_effect_size[
    sizeof(KfTargetCandidateScriptEffect) == 2 ? 1 : -1];

typedef union KfTargetCandidateWord0c {
    u16 value;
    KfTargetCandidateWord0cBytes bytes;
    KfTargetCandidateDeathDrop death_drop;
    KfTargetCandidateScriptEffect script_effect;
} KfTargetCandidateWord0c;
typedef char kf_target_candidate_word0c_size[
    sizeof(KfTargetCandidateWord0c) == 2 ? 1 : -1];
typedef char kf_target_candidate_word0c_high_offset[
    offsetof(KfTargetCandidateWord0c, bytes.high) == 1 ? 1 : -1];

typedef struct KfTargetCandidateWord0eBytes {
    u8 low;
    u8 high;
} KfTargetCandidateWord0eBytes;
typedef char kf_target_candidate_word0e_bytes_size[
    sizeof(KfTargetCandidateWord0eBytes) == 2 ? 1 : -1];

typedef union KfTargetCandidateWord0e {
    u16 value;
    KfTargetCandidateWord0eBytes bytes;
} KfTargetCandidateWord0e;
typedef char kf_target_candidate_word0e_size[
    sizeof(KfTargetCandidateWord0e) == 2 ? 1 : -1];

typedef struct KfTargetCandidateWord10Bytes {
    u8 fallback_offset;
    KfAnimationClip completion_animation_id;
} KfTargetCandidateWord10Bytes;
typedef char kf_target_candidate_word10_bytes_size[
    sizeof(KfTargetCandidateWord10Bytes) == 2 ? 1 : -1];

typedef struct KfTargetCandidateWord10Attack {
    u8 fallback_offset;
    u8 damage_flags;
} KfTargetCandidateWord10Attack;
typedef char kf_target_candidate_word10_attack_size[
    sizeof(KfTargetCandidateWord10Attack) == 2 ? 1 : -1];

typedef union KfTargetCandidateWord10 {
    u16 value;
    KfTargetCandidateWord10Bytes bytes;
    KfTargetCandidateWord10Attack attack;
} KfTargetCandidateWord10;
typedef char kf_target_candidate_word10_size[
    sizeof(KfTargetCandidateWord10) == 2 ? 1 : -1];

typedef struct KfTargetCandidateWord12Bytes {
    u8 post_stream_menu_action;
    u8 marker_state;
} KfTargetCandidateWord12Bytes;
typedef char kf_target_candidate_word12_bytes_size[
    sizeof(KfTargetCandidateWord12Bytes) == 2 ? 1 : -1];

typedef struct KfTargetCandidateWord12Flight {
    u8 vertical_velocity_step;
    u8 orientation_change_threshold;
} KfTargetCandidateWord12Flight;
typedef char kf_target_candidate_word12_flight_size[
    sizeof(KfTargetCandidateWord12Flight) == 2 ? 1 : -1];

typedef union KfTargetCandidateWord12 {
    u16 value;
    KfTargetCandidateWord12Bytes bytes;
    KfTargetCandidateWord12Flight flight;
} KfTargetCandidateWord12;
typedef char kf_target_candidate_word12_size[
    sizeof(KfTargetCandidateWord12) == 2 ? 1 : -1];

typedef union KfTargetCandidateWord14 {
    u16 value;
    u8 bytes[2];
} KfTargetCandidateWord14;
typedef char kf_target_candidate_word14_size[
    sizeof(KfTargetCandidateWord14) == 2 ? 1 : -1];

typedef struct KfTargetCandidateWord16Bytes {
    u8 low;
    u8 high;
} KfTargetCandidateWord16Bytes;
typedef char kf_target_candidate_word16_bytes_size[
    sizeof(KfTargetCandidateWord16Bytes) == 2 ? 1 : -1];

typedef union KfTargetCandidateWord16 {
    u16 value;
    KfTargetCandidateWord16Bytes bytes;
} KfTargetCandidateWord16;
typedef char kf_target_candidate_word16_size[
    sizeof(KfTargetCandidateWord16) == 2 ? 1 : -1];

typedef struct KfTargetCandidateWord18Bytes {
    u8 low;
    u8 high;
} KfTargetCandidateWord18Bytes;
typedef char kf_target_candidate_word18_bytes_size[
    sizeof(KfTargetCandidateWord18Bytes) == 2 ? 1 : -1];

typedef union KfTargetCandidateWord18 {
    u16 value;
    KfTargetCandidateWord18Bytes bytes;
} KfTargetCandidateWord18;
typedef char kf_target_candidate_word18_size[
    sizeof(KfTargetCandidateWord18) == 2 ? 1 : -1];

typedef union KfTargetCandidateWord1a {
    u16 value;
    u8 bytes[2];
} KfTargetCandidateWord1a;
typedef char kf_target_candidate_word1a_size[
    sizeof(KfTargetCandidateWord1a) == 2 ? 1 : -1];

typedef union KfTargetCandidateWord1c {
    u16 value;
    u8 bytes[2];
} KfTargetCandidateWord1c;
typedef char kf_target_candidate_word1c_size[
    sizeof(KfTargetCandidateWord1c) == 2 ? 1 : -1];

typedef union KfTargetCandidateWord1e {
    u16 value;
    u8 bytes[2];
} KfTargetCandidateWord1e;
typedef char kf_target_candidate_word1e_size[
    sizeof(KfTargetCandidateWord1e) == 2 ? 1 : -1];

typedef union KfTargetCandidateWord26 {
    u16 unsigned_value;
    s16 signed_value;
} KfTargetCandidateWord26;
typedef char kf_target_candidate_word26_size[
    sizeof(KfTargetCandidateWord26) == 2 ? 1 : -1];

typedef union KfTargetCandidateWord24 {
    u16 unsigned_value;
    s16 signed_value;
} KfTargetCandidateWord24;
typedef char kf_target_candidate_word24_size[
    sizeof(KfTargetCandidateWord24) == 2 ? 1 : -1];

typedef union KfTargetCandidateWord20 {
    u16 damage_component0;
    u16 animation_phase_start;
} KfTargetCandidateWord20;
typedef char kf_target_candidate_word20_size[
    sizeof(KfTargetCandidateWord20) == 2 ? 1 : -1];

typedef union KfTargetCandidateWord22 {
    u16 damage_component1;
    u16 animation_phase_stop;
} KfTargetCandidateWord22;
typedef char kf_target_candidate_word22_size[
    sizeof(KfTargetCandidateWord22) == 2 ? 1 : -1];

/* Low 14 bits: sound interval; high 2 bits: trigger mode. */
typedef struct KfTargetSoundTriggerFields {
    u16 interval : 14;
    u16 mode : 2;
} KfTargetSoundTriggerFields;
typedef char kf_target_sound_trigger_fields_size[
    sizeof(KfTargetSoundTriggerFields) == 2 ? 1 : -1];

typedef union KfTargetSoundTrigger {
    u16 value;
    KfTargetSoundTriggerFields fields;
} KfTargetSoundTrigger;
typedef char kf_target_sound_trigger_size[
    sizeof(KfTargetSoundTrigger) == 2 ? 1 : -1];

/* This is the observed prefix; complete extent and stride remain under study. */
typedef struct KfTargetCandidate {
    KfActorTargetType type;
    KfAnimationClip animation_id;
    KfTargetCandidateWord02 word_02;
    u8 sound_code;
    u8 unknown_05[2];
    u8 start_vertical_motion_on_entry;
    u16 animation_step;
    KfTargetSoundTrigger sound_trigger;
    KfTargetCandidateWord0c word_0c;
    KfTargetCandidateWord0e word_0e;
    KfTargetCandidateWord10 word_10;
    KfTargetCandidateWord12 word_12;
    KfTargetCandidateWord14 word_14;
    KfTargetCandidateWord16 word_16;
    KfTargetCandidateWord18 word_18;
    KfTargetCandidateWord1a word_1a;
    KfTargetCandidateWord1c word_1c;
    KfTargetCandidateWord1e word_1e;
    KfTargetCandidateWord20 word_20;
    KfTargetCandidateWord22 word_22;
    KfTargetCandidateWord24 word_24;
    KfTargetCandidateWord26 word_26;
    u16 repeated_attack_phase_step;
    u16 secondary_hit_phase;
} KfTargetCandidate;
typedef char kf_target_candidate_view_size[sizeof(KfTargetCandidate) == 0x2c ? 1 : -1];
typedef char kf_target_candidate_animation_id_offset[offsetof(KfTargetCandidate, animation_id) == 1 ? 1 : -1];
typedef char kf_target_candidate_word_02_offset[offsetof(KfTargetCandidate, word_02) == 2 ? 1 : -1];
typedef char kf_target_candidate_sound_code_offset[offsetof(KfTargetCandidate, sound_code) == 4 ? 1 : -1];
typedef char kf_target_candidate_start_vertical_motion_offset[
    offsetof(KfTargetCandidate, start_vertical_motion_on_entry) == 7 ? 1 : -1];
typedef char kf_target_candidate_word_10_offset[offsetof(KfTargetCandidate, word_10) == 0x10 ? 1 : -1];
typedef char kf_target_candidate_fallback_offset[offsetof(KfTargetCandidateWord10Bytes, fallback_offset) == 0 ? 1 : -1];
typedef char kf_target_candidate_animation_step_offset[offsetof(KfTargetCandidate, animation_step) == 0x08 ? 1 : -1];
typedef char kf_target_candidate_sound_trigger_offset[offsetof(KfTargetCandidate, sound_trigger) == 0x0a ? 1 : -1];
typedef char kf_target_candidate_word_0c_offset[offsetof(KfTargetCandidate, word_0c) == 0x0c ? 1 : -1];
typedef char kf_target_candidate_word_0e_offset[offsetof(KfTargetCandidate, word_0e) == 0x0e ? 1 : -1];
typedef char kf_target_candidate_word_0e_low_offset[offsetof(KfTargetCandidate, word_0e.bytes.low) == 0x0e ? 1 : -1];
typedef char kf_target_candidate_completion_animation_offset[offsetof(KfTargetCandidateWord10Bytes, completion_animation_id) == 1 ? 1 : -1];
typedef char kf_target_candidate_completion_animation_total_offset[offsetof(KfTargetCandidate, word_10.bytes.completion_animation_id) == 0x11 ? 1 : -1];
typedef char kf_target_candidate_attack_damage_flags_offset[
    offsetof(KfTargetCandidate, word_10.attack.damage_flags) == 0x11 ? 1 : -1];
typedef char kf_target_candidate_word_12_offset[offsetof(KfTargetCandidate, word_12) == 0x12 ? 1 : -1];
typedef char kf_target_candidate_flight_vertical_step_offset[
    offsetof(KfTargetCandidate, word_12.flight.vertical_velocity_step) == 0x12 ? 1 : -1];
typedef char kf_target_candidate_flight_orientation_threshold_offset[
    offsetof(KfTargetCandidate, word_12.flight.orientation_change_threshold) == 0x13 ? 1 : -1];
typedef char kf_target_candidate_marker_state_offset[offsetof(KfTargetCandidateWord12Bytes, marker_state) == 1 ? 1 : -1];
typedef char kf_target_candidate_marker_state_total_offset[offsetof(KfTargetCandidate, word_12.bytes.marker_state) == 0x13 ? 1 : -1];
typedef char kf_target_candidate_word_14_offset[offsetof(KfTargetCandidate, word_14) == 0x14 ? 1 : -1];
typedef char kf_target_candidate_stream_bytes_offset[offsetof(KfTargetCandidate, word_14.bytes) == 0x14 ? 1 : -1];
typedef char kf_target_candidate_word_16_offset[offsetof(KfTargetCandidate, word_16) == 0x16 ? 1 : -1];
typedef char kf_target_candidate_word_16_high_offset[offsetof(KfTargetCandidate, word_16.bytes.high) == 0x17 ? 1 : -1];
typedef char kf_target_candidate_word_18_offset[offsetof(KfTargetCandidate, word_18) == 0x18 ? 1 : -1];
typedef char kf_target_candidate_word_18_high_offset[offsetof(KfTargetCandidate, word_18.bytes.high) == 0x19 ? 1 : -1];
typedef char kf_target_candidate_word_1a_offset[offsetof(KfTargetCandidate, word_1a) == 0x1a ? 1 : -1];
typedef char kf_target_candidate_word_1c_offset[offsetof(KfTargetCandidate, word_1c) == 0x1c ? 1 : -1];
typedef char kf_target_candidate_word_1e_offset[offsetof(KfTargetCandidate, word_1e) == 0x1e ? 1 : -1];
typedef char kf_target_candidate_word_20_offset[offsetof(KfTargetCandidate, word_20) == 0x20 ? 1 : -1];
typedef char kf_target_candidate_word_22_offset[offsetof(KfTargetCandidate, word_22) == 0x22 ? 1 : -1];
typedef char kf_target_candidate_word_24_offset[offsetof(KfTargetCandidate, word_24) == 0x24 ? 1 : -1];
typedef char kf_target_candidate_word_26_offset[offsetof(KfTargetCandidate, word_26) == 0x26 ? 1 : -1];
typedef char kf_target_candidate_repeated_attack_phase_step_offset[
    offsetof(KfTargetCandidate, repeated_attack_phase_step) == 0x28 ? 1 : -1];
typedef char kf_target_candidate_secondary_hit_phase_offset[
    offsetof(KfTargetCandidate, secondary_hit_phase) == 0x2a ? 1 : -1];

/* Type 25 reads a variable halfword stream after this proved prefix. Its
 * complete allocation and record stride are not established. */
typedef struct KfTargetCandidateAction25 {
    KfActorTargetType type;
    u8 unknown_01[3];
    u8 unknown_04;
    u8 unknown_05[2];
    u16 unknown_08;
    u16 unknown_0a;
    KfTargetCandidateWord0c word_0c;
    KfTargetCandidateWord0e word_0e;
    KfTargetCandidateWord10 word_10;
    KfTargetCandidateWord12 word_12;
    KfTargetCandidateWord14 word_14;
    KfTargetCandidateWord16 word_16;
    KfTargetCandidateWord18 word_18;
    u16 stream[1]; /* first word of a variable-length archive payload */
} KfTargetCandidateAction25;
typedef char kf_target_candidate_action25_prefix_size[
    sizeof(KfTargetCandidateAction25) == 0x1c ? 1 : -1];
typedef char kf_target_candidate_action25_word_08_offset[
    offsetof(KfTargetCandidateAction25, unknown_08) == 0x08 ? 1 : -1];
typedef char kf_target_candidate_action25_stream_offset[
    offsetof(KfTargetCandidateAction25, stream) == 0x1a ? 1 : -1];

/* Group slots hold byte offsets until actor_fixup_group_targets runs. */
typedef union KfTargetReference {
    s32 relative_offset;
    KfTargetCandidate *pointer;
} KfTargetReference;
typedef char kf_target_reference_size[sizeof(KfTargetReference) == 4 ? 1 : -1];

/* Slot-state-3 actors can use the initial-health word as a local X fallback. */
typedef union KfTargetGroupWord1a {
    u16 initial_health;
    u16 slot3_home_x_fallback;
} KfTargetGroupWord1a;
typedef char kf_target_group_word1a_size[sizeof(KfTargetGroupWord1a) == 2 ? 1 : -1];

typedef struct KfTargetGroup {
    u8 definition_id;
    u8 unknown_01;
    u8 knockback_divisor;
    u8 movement_step;
    u8 turn_acceleration;
    u8 vertical_acceleration;
    u8 contact_damage_component1;
    u8 vab_resource_indices[2];
    u8 render_depth;
    u8 activation_range_cells;
    u8 deactivation_range_cells;
    s16 position_offset_x;
    s16 position_offset_y;
    s16 position_offset_z;
    u16 collision_radius;
    u16 collision_height;
    u16 player_facing_tolerance;
    u16 actor_facing_tolerance;
    KfTargetGroupWord1a word_1a;
    u16 default_vertical_anchor_offset;
    u16 experience_reward;
    u16 magic_component_divisors[8];
    u16 scattered_effect_id_center;
    u16 initial_model_scale_q12;
    KfActorFlags initial_actor_flags;
    KfTargetReference targets[16];
} KfTargetGroup;
typedef char kf_target_group_size[sizeof(KfTargetGroup) == 0x78 ? 1 : -1];
typedef char kf_target_group_definition_id_offset[
    offsetof(KfTargetGroup, definition_id) == 0x00 ? 1 : -1];
typedef char kf_target_group_render_depth_offset[
    offsetof(KfTargetGroup, render_depth) == 0x09 ? 1 : -1];
typedef char kf_target_group_vab_resource_indices_offset[
    offsetof(KfTargetGroup, vab_resource_indices) == 0x07 ? 1 : -1];
typedef char kf_target_group_activation_range_offset[
    offsetof(KfTargetGroup, activation_range_cells) == 0x0a ? 1 : -1];
typedef char kf_target_group_deactivation_range_offset[
    offsetof(KfTargetGroup, deactivation_range_cells) == 0x0b ? 1 : -1];
typedef char kf_target_group_collision_radius_offset[
    offsetof(KfTargetGroup, collision_radius) == 0x12 ? 1 : -1];
typedef char kf_target_group_collision_height_offset[
    offsetof(KfTargetGroup, collision_height) == 0x14 ? 1 : -1];
typedef char kf_target_group_player_facing_tolerance_offset[
    offsetof(KfTargetGroup, player_facing_tolerance) == 0x16 ? 1 : -1];
typedef char kf_target_group_actor_facing_tolerance_offset[
    offsetof(KfTargetGroup, actor_facing_tolerance) == 0x18 ? 1 : -1];
typedef char kf_target_group_word_1a_offset[
    offsetof(KfTargetGroup, word_1a) == 0x1a ? 1 : -1];
typedef char kf_target_group_experience_reward_offset[
    offsetof(KfTargetGroup, experience_reward) == 0x1e ? 1 : -1];
typedef char kf_target_group_vertical_acceleration_offset[
    offsetof(KfTargetGroup, vertical_acceleration) == 0x05 ? 1 : -1];
typedef char kf_target_group_movement_step_offset[
    offsetof(KfTargetGroup, movement_step) == 0x03 ? 1 : -1];
typedef char kf_target_group_turn_acceleration_offset[
    offsetof(KfTargetGroup, turn_acceleration) == 0x04 ? 1 : -1];
typedef char kf_target_group_knockback_divisor_offset[
    offsetof(KfTargetGroup, knockback_divisor) == 0x02 ? 1 : -1];
typedef char kf_target_group_contact_damage_component1_offset[
    offsetof(KfTargetGroup, contact_damage_component1) == 0x06 ? 1 : -1];
typedef char kf_target_group_offset_x[
    offsetof(KfTargetGroup, position_offset_x) == 0x0c ? 1 : -1];
typedef char kf_target_group_offset_y[
    offsetof(KfTargetGroup, position_offset_y) == 0x0e ? 1 : -1];
typedef char kf_target_group_offset_z[
    offsetof(KfTargetGroup, position_offset_z) == 0x10 ? 1 : -1];
typedef char kf_target_group_curve_offset[
    offsetof(KfTargetGroup, magic_component_divisors) == 0x20 ? 1 : -1];
typedef char kf_target_group_scattered_effect_id_center_offset[
    offsetof(KfTargetGroup, scattered_effect_id_center) == 0x30 ? 1 : -1];
typedef char kf_target_group_initial_model_scale_offset[
    offsetof(KfTargetGroup, initial_model_scale_q12) == 0x32 ? 1 : -1];
typedef char kf_target_group_initial_actor_flags_offset[
    offsetof(KfTargetGroup, initial_actor_flags) == 0x34 ? 1 : -1];

typedef struct KfActorModelScaleYBytes {
    u8 low;
    s8 high;
} KfActorModelScaleYBytes;
typedef char kf_actor_model_scale_y_bytes_size[
    sizeof(KfActorModelScaleYBytes) == 2 ? 1 : -1];

typedef union KfActorModelScaleY {
    s16 value;
    KfActorModelScaleYBytes bytes;
} KfActorModelScaleY;
typedef char kf_actor_model_scale_y_size[sizeof(KfActorModelScaleY) == 2 ? 1 : -1];

typedef struct KfActorState70Bytes {
    u8 low;
    u8 high;
} KfActorState70Bytes;
typedef char kf_actor_state_70_bytes_size[
    sizeof(KfActorState70Bytes) == 2 ? 1 : -1];

typedef union KfActorState70 {
    s16 signed_state;
    KfActorState70Bytes bytes;
} KfActorState70;
typedef char kf_actor_state_70_size[sizeof(KfActorState70) == 2 ? 1 : -1];

typedef struct KfActorOrientation {
    struct KfEulerAngles rotation;
    u8 unknown_46[2];
} KfActorOrientation;
typedef char kf_actor_orientation_size[sizeof(KfActorOrientation) == 8 ? 1 : -1];

typedef struct KfActorTail72Motion {
    struct KfEulerAngles angles;
    s16 baseline;
} KfActorTail72Motion;
typedef char kf_actor_tail_72_motion_size[
    sizeof(KfActorTail72Motion) == 8 ? 1 : -1];

typedef struct KfActorTail72Script {
    u16 word_index;
    u16 effect_cycle_index;
} KfActorTail72Script;
typedef char kf_actor_tail_72_script_size[
    sizeof(KfActorTail72Script) == 4 ? 1 : -1];

/* Action-specific tail storage overlaps a vector, signed state, and script words. */
typedef union KfActorTail72 {
    s16 signed_state;
    u16 unsigned_state;
    struct KfEulerAngles angles;
    SVECTOR direction;
    KfActorTail72Motion motion;
    KfActorTail72Script script;
} KfActorTail72;
typedef char kf_actor_tail_72_size[sizeof(KfActorTail72) == 8 ? 1 : -1];
typedef char kf_actor_tail_72_baseline_offset[
    offsetof(KfActorTail72, motion.baseline) == 6 ? 1 : -1];

typedef struct KfActorBallisticPhaseView {
    s16 motion_x;
    s16 phase;
    s16 motion_z;
    s16 pad;
} KfActorBallisticPhaseView;
typedef char kf_actor_ballistic_phase_view_size[
    sizeof(KfActorBallisticPhaseView) == 8 ? 1 : -1];

typedef union KfActorMotion {
    SVECTOR vector;
    KfActorBallisticPhaseView ballistic;
} KfActorMotion;
typedef char kf_actor_motion_size[sizeof(KfActorMotion) == 8 ? 1 : -1];
typedef char kf_actor_ballistic_phase_offset[
    offsetof(KfActorMotion, ballistic.phase) == 2 ? 1 : -1];

typedef union KfActorWord20 {
    u16 value;
    u16 home_yaw;
    u16 linked_map_object_slot;
} KfActorWord20;
typedef char kf_actor_word20_size[sizeof(KfActorWord20) == 2 ? 1 : -1];

typedef union KfActorWord22 {
    s16 value;
    s16 home_local_z;
    s16 linked_actor_slot;
    s16 linked_map_object_slot;
} KfActorWord22;
typedef char kf_actor_word22_size[sizeof(KfActorWord22) == 2 ? 1 : -1];

typedef union KfActorWord24 {
    s16 value;
    s16 home_local_x;
    s16 linked_animation_vertex_index;
} KfActorWord24;
typedef char kf_actor_word24_size[sizeof(KfActorWord24) == 2 ? 1 : -1];

/* The 0x7c stride and these fields are fixed by the actor pool and phase
 * helpers. Other fields remain open. */
typedef struct KfActor {
    KfActorSlotState slot_state;
    u8 definition_id;
    u8 group_index;
    KfMapLayerMask current_map_layer;
    u8 unknown_04;
    u8 placement_flags;
    KfMapLayerMask home_map_layer;
    u8 home_cell_z;
    u8 home_cell_x;
    KfActorLifecycle lifecycle;
    u8 spawn_chance;
    KF_ENUM_STORAGE(KfObjectId, u8) death_drop_object_id;
    KfAnimationClip animation_id;
    KfActorVerticalState vertical_motion_state;
    KfActorTargetType target_type;
    KfActorTargetActionState target_action_state;
    KfActorTargetType previous_target_type;
    u8 unknown_11;
    u8 unknown_12;
    KfRenderQueueMode render_mode;
    KfLightingIndex lighting_override;
    u8 render_depth;
    s16 lighting_blend;
    u16 animation_phase;
    u16 health;
    u16 collision_radius;
    u16 collision_height;
    KfActorWord20 word_20;
    KfActorWord22 word_22;
    KfActorWord24 word_24;
    s16 vertical_anchor_offset;
    KfActorFlags flags;
    VECTOR position;
    s32 ballistic_origin_y;
    struct KfEulerAngles rotation;
    u8 unknown_46[2];
    s16 model_scale_x;
    KfActorModelScaleY model_scale_y;
    s16 model_scale_z;
    u8 unknown_4e[2];
    KfActorMotion motion;
    s16 turn_rate;
    struct KfPoolRecord *animation_cache;
    KfTargetCandidate *target;
    s16 movement_yaw;
    s16 animation_step;
    s16 ballistic_horizontal_speed;
    s16 ballistic_launch_speed_y;
    s16 ballistic_acceleration;
    u8 unknown_6e[2];
    KfActorState70 state_70;
    KfActorTail72 tail_72;
} KfActor;

typedef char kf_actor_size[sizeof(KfActor) == 0x7c ? 1 : -1];
typedef char kf_actor_definition_id_offset[
    offsetof(KfActor, definition_id) == 0x01 ? 1 : -1];
typedef char kf_actor_current_map_layer_offset[
    offsetof(KfActor, current_map_layer) == 0x03 ? 1 : -1];
typedef char kf_actor_home_map_layer_offset[
    offsetof(KfActor, home_map_layer) == 0x06 ? 1 : -1];
typedef char kf_actor_home_cell_z_offset[
    offsetof(KfActor, home_cell_z) == 0x07 ? 1 : -1];
typedef char kf_actor_home_cell_x_offset[
    offsetof(KfActor, home_cell_x) == 0x08 ? 1 : -1];
typedef char kf_actor_animation_id_offset[
    offsetof(KfActor, animation_id) == 0x0c ? 1 : -1];
typedef char kf_actor_vertical_motion_state_offset[
    offsetof(KfActor, vertical_motion_state) == 0x0d ? 1 : -1];
typedef char kf_actor_target_action_state_offset[
    offsetof(KfActor, target_action_state) == 0x0f ? 1 : -1];
typedef char kf_actor_render_mode_offset[
    offsetof(KfActor, render_mode) == 0x13 ? 1 : -1];
typedef char kf_actor_lighting_override_offset[
    offsetof(KfActor, lighting_override) == 0x14 ? 1 : -1];
typedef char kf_actor_lighting_blend_offset[
    offsetof(KfActor, lighting_blend) == 0x16 ? 1 : -1];
typedef char kf_actor_spawn_chance_offset[offsetof(KfActor, spawn_chance) == 0x0a ? 1 : -1];
typedef char kf_actor_death_drop_object_offset[
    offsetof(KfActor, death_drop_object_id) == 0x0b ? 1 : -1];
typedef char kf_actor_render_depth_offset[
    offsetof(KfActor, render_depth) == 0x15 ? 1 : -1];
typedef char kf_actor_health_offset[
    offsetof(KfActor, health) == 0x1a ? 1 : -1];
typedef char kf_actor_collision_radius_offset[
    offsetof(KfActor, collision_radius) == 0x1c ? 1 : -1];
typedef char kf_actor_collision_height_offset[
    offsetof(KfActor, collision_height) == 0x1e ? 1 : -1];
typedef char kf_actor_phase_offset[offsetof(KfActor, animation_phase) == 0x18 ? 1 : -1];
typedef char kf_actor_flags_offset[offsetof(KfActor, flags) == 0x28 ? 1 : -1];
typedef char kf_actor_position_offset[offsetof(KfActor, position) == 0x2c ? 1 : -1];
typedef char kf_actor_ballistic_origin_y_offset[
    offsetof(KfActor, ballistic_origin_y) == 0x3c ? 1 : -1];
typedef char kf_actor_home_yaw_offset[offsetof(KfActor, word_20.home_yaw) == 0x20 ? 1 : -1];
typedef char kf_actor_home_offset_z[offsetof(KfActor, word_22.home_local_z) == 0x22 ? 1 : -1];
typedef char kf_actor_home_offset_x[offsetof(KfActor, word_24.home_local_x) == 0x24 ? 1 : -1];
typedef char kf_actor_vertical_anchor_offset[
    offsetof(KfActor, vertical_anchor_offset) == 0x26 ? 1 : -1];
typedef char kf_actor_model_scale_x_offset[
    offsetof(KfActor, model_scale_x) == 0x48 ? 1 : -1];
typedef char kf_actor_model_scale_y_offset[
    offsetof(KfActor, model_scale_y) == 0x4a ? 1 : -1];
typedef char kf_actor_model_scale_z_offset[
    offsetof(KfActor, model_scale_z) == 0x4c ? 1 : -1];
typedef char kf_actor_motion_offset[offsetof(KfActor, motion) == 0x50 ? 1 : -1];
typedef char kf_actor_motion_x_offset[offsetof(KfActor, motion.vector.vx) == 0x50 ? 1 : -1];
typedef char kf_actor_motion_z_offset[offsetof(KfActor, motion.vector.vz) == 0x54 ? 1 : -1];
typedef char kf_actor_turn_rate_offset[offsetof(KfActor, turn_rate) == 0x58 ? 1 : -1];
typedef char kf_actor_cache_offset[offsetof(KfActor, animation_cache) == 0x5c ? 1 : -1];
typedef char kf_actor_target_offset[offsetof(KfActor, target) == 0x60 ? 1 : -1];
typedef char kf_actor_movement_yaw_offset[offsetof(KfActor, movement_yaw) == 0x64 ? 1 : -1];
typedef char kf_actor_step_offset[offsetof(KfActor, animation_step) == 0x66 ? 1 : -1];
typedef char kf_actor_ballistic_horizontal_speed_offset[
    offsetof(KfActor, ballistic_horizontal_speed) == 0x68 ? 1 : -1];
typedef char kf_actor_ballistic_launch_speed_y_offset[
    offsetof(KfActor, ballistic_launch_speed_y) == 0x6a ? 1 : -1];
typedef char kf_actor_ballistic_acceleration_offset[
    offsetof(KfActor, ballistic_acceleration) == 0x6c ? 1 : -1];
typedef char kf_actor_state_70_offset[offsetof(KfActor, state_70) == 0x70 ? 1 : -1];
typedef char kf_actor_state_71_offset[offsetof(KfActor, state_70.bytes.high) == 0x71 ? 1 : -1];
typedef char kf_actor_tail_72_offset[offsetof(KfActor, tail_72) == 0x72 ? 1 : -1];
typedef char kf_actor_tail_72_signed_offset[offsetof(KfActor, tail_72.signed_state) == 0x72 ? 1 : -1];
typedef char kf_actor_tail_72_unsigned_offset[offsetof(KfActor, tail_72.unsigned_state) == 0x72 ? 1 : -1];
typedef char kf_actor_tail_72_angles_offset[offsetof(KfActor, tail_72.angles) == 0x72 ? 1 : -1];
typedef char kf_actor_tail_72_script_index_offset[offsetof(KfActor, tail_72.script.word_index) == 0x72 ? 1 : -1];
typedef char kf_actor_tail_72_script_effect_cycle_index_offset[offsetof(KfActor, tail_72.script.effect_cycle_index) == 0x74 ? 1 : -1];
typedef char kf_actor_motion_baseline_offset[offsetof(KfActor, tail_72.motion.baseline) == 0x78 ? 1 : -1];

/* The startup clear bounds this runtime; the two trailer writes and actor
 * array are fixed by actor_pool_clear. */
typedef struct KfActorStateGame {
    KfActor actors[KF_ACTOR_CAPACITY];
    KfTargetGroup target_groups[40];
    /* 0x16820 loads the groups and this opaque tail as one 0x32c0-byte span;
     * 0x3f7ec fixes group target offsets after the copy. */
    u8 target_candidate_blob[0x2000];
    KF_ENUM_STORAGE(KfActorFlags, u8) actor_overlap_exclusion_flags;
    KfCollisionQuery actor_collision_query_flags;
    KfTargetGroup *active_group;
    KfActor *current;
    KfTargetGroup *other_group;
    KfActor *other_actor;
    s32 current_actor_slot_index;
    u32 current_group_index;
    u32 active_actor_count;
    u32 actor_update_frame_count;
    KfActor *player_view_target;
} KfActorStateGame;

typedef char kf_actor_state_size[sizeof(KfActorStateGame) == 0x93cc ? 1 : -1];
typedef char kf_actor_state_groups_offset[offsetof(KfActorStateGame, target_groups) == 0x60e0 ? 1 : -1];
typedef char kf_actor_state_target_candidate_blob_offset[
    offsetof(KfActorStateGame, target_candidate_blob) == 0x73a0 ? 1 : -1];
typedef char kf_actor_state_active_group_offset[offsetof(KfActorStateGame, active_group) == 0x93a8 ? 1 : -1];
typedef char kf_actor_state_current_offset[offsetof(KfActorStateGame, current) == 0x93ac ? 1 : -1];
typedef char kf_actor_state_overlap_exclusion_flags_offset[
    offsetof(KfActorStateGame, actor_overlap_exclusion_flags) == 0x93a0 ? 1 : -1];
typedef char kf_actor_state_collision_query_flags_offset[
    offsetof(KfActorStateGame, actor_collision_query_flags) == 0x93a4 ? 1 : -1];
typedef char kf_actor_state_current_actor_slot_index_offset[
    offsetof(KfActorStateGame, current_actor_slot_index) == 0x93b8 ? 1 : -1];
typedef char kf_actor_state_active_actor_count_offset[
    offsetof(KfActorStateGame, active_actor_count) == 0x93c0 ? 1 : -1];
typedef char kf_actor_state_update_frame_count_offset[
    offsetof(KfActorStateGame, actor_update_frame_count) == 0x93c4 ? 1 : -1];

extern KfActorStateGame actor_status;

/* One 16-byte actor placement record from a map archive; the loader copies
 * KF_ACTOR_CAPACITY consecutive records into the actor pool. */
typedef struct KfActorLoadRecord {
    KfActorSlotState slot_state;
    u8 group_index;
    u8 placement_flags;
    u8 cell_z;
    u8 cell_x;
    u8 spawn_chance;
    KF_ENUM_STORAGE(KfObjectId, u8) death_drop_object_id;
    KfMapLayerMask home_map_layer;
    u16 initial_actor_word_20;
    u16 initial_actor_word_22;
    u16 initial_actor_word_24;
    u16 vertical_anchor_offset;
} KfActorLoadRecord;

typedef char kf_actor_load_record_size[sizeof(KfActorLoadRecord) == 16 ? 1 : -1];
typedef char kf_actor_load_record_word_20_offset[
    offsetof(KfActorLoadRecord, initial_actor_word_20) == 8 ? 1 : -1];

KfActor *actor_pool_find_free(void);
void actor_set_home_position(KfActor *actor);
void actor_disable_type3_transition_actors(void);
void actor_set_lifecycle_and_home_position(KfActor *actor);
void actor_pool_clear(void);
void actor_set_target(KfActor *actor, KfTargetCandidate *target);
void actor_copy_group_defaults(KfActor *actor);
void actor_initialize_from_group(KfActor *actor);
void actor_prepare_and_initialize(KfActor *actor);
void actor_bind_current(KfActor *actor);
void actor_fixup_group_targets(void);
void actor_load_records(const KfActorLoadRecord *records);
void actor_update_lifecycle_for_player_range(void);
void actor_update_behavior(void);
KfTargetCandidate *actor_find_target_of_type(const KfTargetGroup *group, KfActorTargetType type);
u8 event_target_stream_find_marker(const KfTargetCandidate *candidate, u8 marker);
u8 *event_target_stream_resolve_cursor(KfActor *actor);
void actor_animation_seek_phase(KfActor *actor, KfAnimationClip state, u16 phase,
                   s32 target_phase, s32 phase_step);
void actor_select_target_type_in_own_group(KfActor *actor, KfActorTargetType type);
void actor_select_best_target(s32 player_distance);
s32 actor_score_target_candidate(KfTargetCandidate *target, s32 player_distance);
void actor_select_target_for_player_distance(void);
KfActor *actor_find_best_in_cone(const VECTOR *position, s16 yaw, s16 pitch,
                       s32 max_distance, s32 yaw_limit, s32 pitch_limit,
                       s32 *distance, s32 variation);
s32 actor_find_overlap_excluding_target_type3(s32 x, s32 y, s32 z,
                                             s32 radius, s32 height);
s32 actor_find_overlap(s32 x, s32 y, s32 z, s32 radius, s32 height);
void actor_apply_magic_to_actor(s32 actor_index, u16 power, u16 magic_06,
                   u16 magic_08, u16 magic_0a, u16 magic_0c,
                   u16 magic_0e, u16 magic_10, u16 magic_12,
                   u16 magic_14, u16 amount, KF_ENUM_PARAM(KfActorDamageFlags, s32) effect_flags,
                   const VECTOR *position);
void actor_apply_area_magic(VECTOR *position, s32 minimum_distance, s32 reach,
                   s32 mode, u16 falloff, u16 power, u16 magic_06,
                   u16 magic_08, u16 magic_0a, u16 magic_0c, u16 magic_0e,
                   u16 magic_10, u16 magic_12, u16 magic_14,
                   s32 amount_and_flags, KF_ENUM_PARAM(KfActorDamageFlags, u16) effect_flags);
VECTOR *actor_resolve_group_position(KfActor *actor, VECTOR *output);
s32 actor_sample_rotated_animation_vertex(KfActor *actor, s32 vertex_index,
                                          VECTOR *output);
s32 actor_compute_target_direction(KfActor *actor, const VECTOR *origin, s32 step,
                  const VECTOR *target, SVECTOR *direction,
                  s32 pitch_override, u16 yaw_limit, s32 iterations);
void actor_reset_target_and_reselect(void);
void actor_set_animation(KfAnimationClip animation_id);
void actor_set_animation_if_changed(KfAnimationClip animation_id);
void actor_advance_animation_wrapped(KfActor *actor, s16 delta);
void actor_advance_animation_clamped(KfActor *actor, s16 delta);
KfBool32 actor_animation_crossed_phase(const KfActor *actor, u16 phase);
KF_ENUM_PARAM(KfCollisionHitFlags, s32) actor_move_horizontal_with_collision(SVECTOR *motion,
    KF_ENUM_PARAM(KfActorMoveFlags, s32) flags);
void actor_play_target_sound(KfActor *actor);
KF_ENUM_PARAM(KfCollisionHitFlags, s32) actor_damp_horizontal_motion(s32 decay,
    KF_ENUM_PARAM(KfActorMoveFlags, s32) move_flags);
KF_ENUM_PARAM(KfCollisionHitFlags, s32) actor_move_with_collision(SVECTOR *motion);
KF_ENUM_PARAM(KfCollisionHitFlags, s32) actor_move_along_heading(s16 angle, s32 speed, s32 step,
    KF_ENUM_PARAM(KfActorMoveFlags, s32) move_flags);
s32 actor_start_ballistic_motion(KF_ENUM_PARAM(KfTrajectoryMode, s32) mode, s32 target_x,
    s32 target_y, s32 target_z, s32 trajectory_parameter, s32 trajectory_speed);
void actor_suspend_vertical_motion(void);
b32 actor_try_damage_player_in_cone(s32 minimum_distance, s32 maximum_distance,
                  s32 y_offset, s32 angle_tolerance, u16 damage0,
                  u16 damage1, u16 damage2, u16 damage_flags);
KF_ENUM_PARAM(KfCollisionHitFlags, s32) actor_turn_and_move_along_heading(s16 angle, s32 speed,
    s32 range, s32 step, s32 mode, KF_ENUM_PARAM(KfActorMoveFlags, s32) move_flags);
KF_ENUM_PARAM(KfActorEulerMoveResult, s32) actor_turn_and_move_along_euler_angles(
    const struct KfEulerAngles *angles, s32 speed, s32 range, s32 step, s32 mode,
    KF_ENUM_PARAM(KfActorMoveFlags, s32) move_flags);
s32 actor_turn_and_move_toward_point(s32 world_x, s32 world_z, s32 speed, s32 range,
    s16 reference_angle, s32 step, s32 mode, KF_ENUM_PARAM(KfActorMoveFlags, s32) move_flags);
void actor_turn_toward_angle(KfActor *actor, s32 target_angle, s32 max_speed,
                   s32 acceleration);
void actor_update_motion_animation(KF_ENUM_PARAM(KfAnimationClip, s32) first,
    KF_ENUM_PARAM(KfAnimationClip, s32) reverse, KF_ENUM_PARAM(KfAnimationClip, s32) forward,
    KF_ENUM_PARAM(KfAnimationClip, s32) fast, KF_ENUM_PARAM(KfAnimationClip, s32) slow,
    s32 phase_step);
void actor_dispatch_group_effect(KF_ENUM_PARAM(KfEffectKind, s32) kind, s32 damage_multiplier_tenths,
                                 s32 position_mode, ...);
void actor_update_vertical_motion(void);
void actor_update_frame(void);

#endif
