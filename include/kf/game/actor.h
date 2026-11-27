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
 * placement stream. Slot 0, the placement default, wakes a dormant actor
 * only when its spawn_chance roll succeeds.
 */
KF_ENUM_BEGIN(KfActorSlotState, u8)
    KF_ACTOR_SLOT_CHANCE_SPAWN = 0,
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
 * disabled candidate row). The three MAP_DEFINED types are updated by the
 * map's active_table[17] callback and nothing in the executables tells
 * them apart, so they are named by their order.
 */
KF_ENUM_BEGIN(KfActorTargetType, u8)
    KF_ACTOR_TARGET_IDLE = 0,
    KF_ACTOR_TARGET_WANDER = 1,
    KF_ACTOR_TARGET_DAMAGE_REACTION = 2,
    KF_ACTOR_TARGET_DEATH = 3,
    KF_ACTOR_TARGET_ATTACK = 4,
    KF_ACTOR_TARGET_KEEP_RANGE = 5,
    KF_ACTOR_TARGET_MAP_DEFINED_FIRST = 6,
    KF_ACTOR_TARGET_MAP_DEFINED_SECOND = 7,
    KF_ACTOR_TARGET_MAP_DEFINED_THIRD = 8,
    KF_ACTOR_TARGET_JUMP_ATTACK = 9,
    KF_ACTOR_TARGET_DRIFT = 10,
    KF_ACTOR_TARGET_LUNGE = 11,
    KF_ACTOR_TARGET_FLYING_WANDER = 12,
    KF_ACTOR_TARGET_FLYING_KEEP_RANGE = 13,
    KF_ACTOR_TARGET_PARTNER_TURN = 14,
    KF_ACTOR_TARGET_MOVE_FORWARD = 15,
    KF_ACTOR_TARGET_FLYING_WANDER_ANIMATED = 16,
    KF_ACTOR_TARGET_FLYING_KEEP_RANGE_ANIMATED = 17,
    KF_ACTOR_TARGET_FLYING_PURSUIT_ATTACK = 18,
    KF_ACTOR_TARGET_WEAPON_WAIT = 19,
    KF_ACTOR_TARGET_WEAPON_RETREAT = 20,
    KF_ACTOR_TARGET_AMBUSH = 21,
    KF_ACTOR_TARGET_MAGIC_REACTION = 22,
    KF_ACTOR_TARGET_REPEATED_ATTACK = 23,
    KF_ACTOR_TARGET_PURSUIT_ATTACK = 24,
    KF_ACTOR_TARGET_EFFECT_SCRIPT = 25,
    KF_ACTOR_TARGET_RISE_FROM_OBJECT = 26,
    KF_ACTOR_TARGET_RETURN_HOME = 27,
    KF_ACTOR_TARGET_SPINNING_CONTACT = 28,
    KF_ACTOR_TARGET_ASCENDING_SPIN = 29,
    KF_ACTOR_TARGET_COLLISION_MOVE = 30,
    KF_ACTOR_TARGET_EVENT_STREAM = 0x70,
    KF_ACTOR_TARGET_MAP_CALLBACK_FIRST = 0x80,
    KF_ACTOR_TARGET_PLAYER_VIEW_REACTION = 130,
    KF_ACTOR_TARGET_MAP_ATTACK = 132,
    KF_ACTOR_TARGET_SUSPEND_VERTICAL_MOTION = 240,
    KF_ACTOR_TARGET_NONE = 0xff
KF_ENUM_END(KfActorTargetType)

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

/* Damage reactions reuse the selection scales as their trigger chance. */
typedef union KfTargetCandidateWeights {
    KfTargetCandidateSelectionScales target_selection;
    KfTargetCandidateDamageReaction damage_reaction;
} KfTargetCandidateWeights;
typedef char kf_target_candidate_weights_size[
    sizeof(KfTargetCandidateWeights) == 2 ? 1 : -1];

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

/*
 * Per-type parameters at candidate offset 0x0c. Each view is named after the
 * behaviour that reads it in actor_update_behavior, actor_score_target_candidate
 * or the event stream interpreter; a type reads only its own view.
 */

/* Damage reaction: taken when a hit deals at least this much damage. */
typedef struct KfTargetDamageReactionParams {
    u16 damage_threshold;
} KfTargetDamageReactionParams;

/* Death: a dying actor drops object_id with chance/256. */
typedef struct KfTargetDeathDropParams {
    KF_ENUM_STORAGE(KfObjectId, u8) object_id;
    u8 chance;
} KfTargetDeathDropParams;

/* Wander and forward moves: turn toward a heading, then move along it. */
typedef struct KfTargetHeadingMoveParams {
    u16 move_speed;
    u16 turn_speed;
    u8 heading_change_chance;
} KfTargetHeadingMoveParams;

/* Clip arguments of actor_update_motion_animation after its first clip. */
typedef struct KfTargetMotionClips {
    KfAnimationClip reverse;
    KfAnimationClip forward;
    KfAnimationClip fast;
    KfAnimationClip slow;
} KfTargetMotionClips;
typedef char kf_target_motion_clips_size[sizeof(KfTargetMotionClips) == 4 ? 1 : -1];

/* Flying wander: hover within hover_range of the home height. */
typedef struct KfTargetFlyingWanderParams {
    u16 move_speed;
    u16 turn_speed;
    u16 hover_range;
    u8 vertical_velocity_step;
    u8 orientation_change_threshold;
    KfTargetMotionClips motion_clips;
} KfTargetFlyingWanderParams;

/*
 * Keep range: approach beyond approach_distance, retreat inside
 * retreat_distance. Scoring takes a new target within engage_distance and
 * keeps the current one within hold_distance.
 */
typedef struct KfTargetKeepRangeParams {
    u16 move_speed;
    u16 turn_speed;
    u16 engage_distance;
    u16 hold_distance;
    u16 retreat_distance;
    u16 approach_distance;
    u16 ledge_drop_chance;
} KfTargetKeepRangeParams;

/* Flying keep range: the keep-range fields, then motion clips. */
typedef struct KfTargetFlyingKeepRangeParams {
    u16 move_speed;
    u16 turn_speed;
    u16 engage_distance;
    u16 hold_distance;
    u16 retreat_distance;
    u16 approach_distance;
    KfTargetMotionClips motion_clips;
} KfTargetFlyingKeepRangeParams;
typedef char kf_target_flying_keep_range_shares_prefix[
    offsetof(KfTargetFlyingKeepRangeParams, approach_distance) ==
    offsetof(KfTargetKeepRangeParams, approach_distance) ? 1 : -1];

/* Jump attack: back off, wind up, then a ballistic jump at the player. */
typedef struct KfTargetJumpAttackParams {
    u16 engage_distance;
    u16 back_off_distance;
    u16 trajectory_speed;
    u16 back_off_speed;
    KfAnimationClip back_off_animation_id;
    KfAnimationClip landing_animation_id;
    u8 jump_animation_step;
    u8 trajectory_parameter;
} KfTargetJumpAttackParams;

/* Drift: random acceleration on each axis, clamped to max_speed. */
typedef struct KfTargetDriftParams {
    u16 max_speed;
    u16 turn_speed;
    u16 acceleration;
} KfTargetDriftParams;

/* The cone hit passed to actor_try_damage_player_in_cone. */
typedef struct KfTargetConeHit {
    u8 maximum_distance;
    u8 y_offset;
    u8 angle_tolerance;
    u8 damage_flags;
    u16 damage[3];
} KfTargetConeHit;
typedef char kf_target_cone_hit_size[sizeof(KfTargetConeHit) == 10 ? 1 : -1];

/* Pursuing attacks move toward the player; step changes by animation phase. */
typedef struct KfTargetAttackPursuit {
    u16 move_speed;
    u16 turn_speed;
    u16 phase_start;
    u16 phase_stop;
    s16 step_after_start;
    s16 step_after_stop;
} KfTargetAttackPursuit;

/* Repeated hits: the first hit repeats every hit_phase_step up to last_hit_phase. */
typedef struct KfTargetRepeatedHits {
    KfTargetConeHit secondary_hit;
    u16 last_hit_phase;
    u16 hit_phase_step;
    u16 secondary_hit_phase;
} KfTargetRepeatedHits;

typedef union KfTargetAttackFollowUp {
    KfTargetAttackPursuit pursuit;
    KfTargetRepeatedHits repeated;
} KfTargetAttackFollowUp;
typedef char kf_target_attack_follow_up_size[sizeof(KfTargetAttackFollowUp) == 0x10 ? 1 : -1];

/*
 * Cone attack: hit when the animation crosses hit_phase. motion_step is the
 * motion decay of standing attacks and the move step of pursuing ones.
 */
typedef struct KfTargetAttackParams {
    u8 unused;
    u8 motion_step;
    KfTargetConeHit hit;
    u16 hit_phase;
    u16 engage_distance;
    KfTargetAttackFollowUp follow_up;
} KfTargetAttackParams;

/* Lunge: a straight charge that damages the player on contact. */
typedef struct KfTargetLungeParams {
    u8 unused;
    u8 damage_flags;
    u16 damage[3];
    u16 lunge_speed;
    u16 turn_speed;
    u16 lunge_acceleration;
    u16 engage_distance;
} KfTargetLungeParams;

/* Partner turn: follow the other actor's turn direction. */
typedef struct KfTargetPartnerTurnParams {
    KfAnimationClip negative_turn_animation_id;
    KfAnimationClip positive_turn_animation_id;
} KfTargetPartnerTurnParams;

/* Weapon reactions are scored while the player's weapon attack is active. */
typedef struct KfTargetWeaponWaitParams {
    u8 facing_angle;
    u8 unused;
    u16 engage_distance;
    u16 lead_in_phase;
} KfTargetWeaponWaitParams;

typedef struct KfTargetWeaponRetreatParams {
    u8 facing_angle;
    u8 unused;
    u16 engage_distance;
    u16 retreat_speed;
    u16 pass_count;
} KfTargetWeaponRetreatParams;
typedef char kf_target_weapon_retreat_shares_prefix[
    offsetof(KfTargetWeaponRetreatParams, engage_distance) ==
    offsetof(KfTargetWeaponWaitParams, engage_distance) ? 1 : -1];

/* Ambush: wait hidden at a reduced size until the player comes close. */
typedef struct KfTargetAmbushParams {
    u16 trigger_distance;
    u16 emerge_speed;
    u16 hold_frames;
    u16 move_phase;
    u16 hidden_radius;
    u16 hidden_height;
    KfAnimationClip hold_animation_id;
    u8 hold_animation_step;
} KfTargetAmbushParams;

/*
 * Effect script: each cue phase runs halfword opcodes from stream. The stream
 * runs past the candidate prefix; its complete extent is not established.
 */
typedef struct KfTargetEffectScriptParams {
    KfEffectKind effect_kind;
    u8 facing_angle;
    u16 first_cue_phase;
    u16 last_cue_phase;
    u16 cue_phase_step;
    u16 minimum_distance;
    u16 maximum_distance;
    u16 damage_multiplier_tenths;
    u16 stream[1];
} KfTargetEffectScriptParams;

/* Return home: walk back, face the home yaw, then resize. */
typedef struct KfTargetReturnHomeParams {
    u16 return_distance;
    u16 move_speed;
    u16 turn_speed;
    u16 rest_radius;
    u16 rest_height;
    KfAnimationClip walk_animation_id;
    u8 walk_animation_step;
} KfTargetReturnHomeParams;

/* Contact damage: the player_apply_damage arguments in table order. */
typedef struct KfTargetContactDamageParams {
    u16 damage_flags;
    u16 damage[8];
} KfTargetContactDamageParams;

/*
 * Event stream: byte opcodes from stream. cursor_offset is the resume
 * position in stream; marker offsets count from marker_state.
 */
typedef struct KfTargetEventStreamParams {
    u16 talk_image_base;
    u16 completion_phase_step;
    u8 cursor_offset;
    KfAnimationClip completion_animation_id;
    u8 post_stream_menu_action;
    u8 marker_state;
    u8 stream[1];
} KfTargetEventStreamParams;

typedef union KfTargetParams {
    KfTargetDamageReactionParams damage_reaction;
    KfTargetDeathDropParams death_drop;
    KfTargetHeadingMoveParams heading_move;
    KfTargetFlyingWanderParams flying_wander;
    KfTargetKeepRangeParams keep_range;
    KfTargetFlyingKeepRangeParams flying_keep_range;
    KfTargetJumpAttackParams jump_attack;
    KfTargetDriftParams drift;
    KfTargetAttackParams attack;
    KfTargetLungeParams lunge;
    KfTargetPartnerTurnParams partner_turn;
    KfTargetWeaponWaitParams weapon_wait;
    KfTargetWeaponRetreatParams weapon_retreat;
    KfTargetAmbushParams ambush;
    KfTargetEffectScriptParams effect_script;
    KfTargetReturnHomeParams return_home;
    KfTargetContactDamageParams contact_damage;
    KfTargetEventStreamParams event_stream;
} KfTargetParams;
typedef char kf_target_params_size[sizeof(KfTargetParams) == 0x20 ? 1 : -1];

/* This is the observed prefix; complete extent and stride remain under study. */
typedef struct KfTargetCandidate {
    KfActorTargetType type;
    KfAnimationClip animation_id;
    KfTargetCandidateWeights weights;
    u8 sound_code;
    u8 unused_05[2];
    u8 start_vertical_motion_on_entry;
    u16 animation_step;
    KfTargetSoundTrigger sound_trigger;
    KfTargetParams params;
} KfTargetCandidate;
typedef char kf_target_candidate_view_size[sizeof(KfTargetCandidate) == 0x2c ? 1 : -1];
typedef char kf_target_candidate_animation_id_offset[offsetof(KfTargetCandidate, animation_id) == 1 ? 1 : -1];
typedef char kf_target_candidate_weights_offset[offsetof(KfTargetCandidate, weights) == 2 ? 1 : -1];
typedef char kf_target_candidate_sound_code_offset[offsetof(KfTargetCandidate, sound_code) == 4 ? 1 : -1];
typedef char kf_target_candidate_start_vertical_motion_offset[
    offsetof(KfTargetCandidate, start_vertical_motion_on_entry) == 7 ? 1 : -1];
typedef char kf_target_candidate_animation_step_offset[offsetof(KfTargetCandidate, animation_step) == 0x08 ? 1 : -1];
typedef char kf_target_candidate_sound_trigger_offset[offsetof(KfTargetCandidate, sound_trigger) == 0x0a ? 1 : -1];
typedef char kf_target_candidate_params_offset[offsetof(KfTargetCandidate, params) == 0x0c ? 1 : -1];
typedef char kf_target_candidate_attack_hit_offset[
    offsetof(KfTargetCandidate, params.attack.hit) == 0x0e ? 1 : -1];
typedef char kf_target_candidate_attack_damage_flags_offset[
    offsetof(KfTargetCandidate, params.attack.hit.damage_flags) == 0x11 ? 1 : -1];
typedef char kf_target_candidate_attack_hit_phase_offset[
    offsetof(KfTargetCandidate, params.attack.hit_phase) == 0x18 ? 1 : -1];
typedef char kf_target_candidate_attack_engage_offset[
    offsetof(KfTargetCandidate, params.attack.engage_distance) == 0x1a ? 1 : -1];
typedef char kf_target_candidate_secondary_hit_offset[
    offsetof(KfTargetCandidate, params.attack.follow_up.repeated.secondary_hit) == 0x1c ? 1 : -1];
typedef char kf_target_candidate_secondary_hit_phase_offset[
    offsetof(KfTargetCandidate, params.attack.follow_up.repeated.secondary_hit_phase) == 0x2a ? 1 : -1];
typedef char kf_target_candidate_step_after_stop_offset[
    offsetof(KfTargetCandidate, params.attack.follow_up.pursuit.step_after_stop) == 0x26 ? 1 : -1];
typedef char kf_target_candidate_lunge_engage_offset[
    offsetof(KfTargetCandidate, params.lunge.engage_distance) == 0x1a ? 1 : -1];
typedef char kf_target_candidate_keep_range_ledge_offset[
    offsetof(KfTargetCandidate, params.keep_range.ledge_drop_chance) == 0x18 ? 1 : -1];
typedef char kf_target_candidate_flying_keep_range_clips_offset[
    offsetof(KfTargetCandidate, params.flying_keep_range.motion_clips) == 0x18 ? 1 : -1];
typedef char kf_target_candidate_flying_wander_clips_offset[
    offsetof(KfTargetCandidate, params.flying_wander.motion_clips) == 0x14 ? 1 : -1];
typedef char kf_target_candidate_jump_step_offset[
    offsetof(KfTargetCandidate, params.jump_attack.jump_animation_step) == 0x16 ? 1 : -1];
typedef char kf_target_candidate_ambush_clip_offset[
    offsetof(KfTargetCandidate, params.ambush.hold_animation_id) == 0x18 ? 1 : -1];
typedef char kf_target_candidate_script_stream_offset[
    offsetof(KfTargetCandidate, params.effect_script.stream) == 0x1a ? 1 : -1];
typedef char kf_target_candidate_return_clip_offset[
    offsetof(KfTargetCandidate, params.return_home.walk_animation_id) == 0x16 ? 1 : -1];
typedef char kf_target_candidate_contact_damage_last_offset[
    offsetof(KfTargetCandidate, params.contact_damage.damage[7]) == 0x1c ? 1 : -1];
typedef char kf_target_candidate_event_cursor_offset[
    offsetof(KfTargetCandidate, params.event_stream.cursor_offset) == 0x10 ? 1 : -1];
typedef char kf_target_candidate_event_marker_offset[
    offsetof(KfTargetCandidate, params.event_stream.marker_state) == 0x13 ? 1 : -1];
typedef char kf_target_candidate_event_stream_offset[
    offsetof(KfTargetCandidate, params.event_stream.stream) == 0x14 ? 1 : -1];

/* Group slots hold byte offsets until actor_fixup_group_targets runs. */
typedef union KfTargetReference {
    s32 relative_offset;
    KfTargetCandidate *pointer;
} KfTargetReference;
typedef char kf_target_reference_size[sizeof(KfTargetReference) == 4 ? 1 : -1];

/* Linked homebound actors with no placement vertex read this word as their
 * default attachment vertex instead of the initial health. */
typedef union KfTargetGroupHealthOrVertex {
    u16 initial_health;
    u16 linked_vertex_fallback;
} KfTargetGroupHealthOrVertex;
typedef char kf_target_group_health_or_vertex_size[
    sizeof(KfTargetGroupHealthOrVertex) == 2 ? 1 : -1];

typedef struct KfTargetGroup {
    u8 definition_id;
    u8 unused_01;
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
    KfTargetGroupHealthOrVertex health_or_vertex;
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
typedef char kf_target_group_health_or_vertex_offset[
    offsetof(KfTargetGroup, health_or_vertex) == 0x1a ? 1 : -1];
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

/* Keep-range behaviour: retreating moves away from the player instead of
 * toward it; ledge_drop lets the actor step off a ledge it probed. */
typedef struct KfActorKeepRangeState {
    u8 retreating;
    u8 ledge_drop;
} KfActorKeepRangeState;
typedef char kf_actor_state_70_bytes_size[
    sizeof(KfActorKeepRangeState) == 2 ? 1 : -1];

typedef union KfActorState70 {
    s16 signed_state;
    KfActorKeepRangeState keep_range;
} KfActorState70;
typedef char kf_actor_state_70_size[sizeof(KfActorState70) == 2 ? 1 : -1];

typedef struct KfActorOrientation {
    struct KfEulerAngles rotation;
    u8 unused_46[2];
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

/* Bytes 0x20-0x25, copied from the actor's placement row. Most actors read
 * them as their home yaw and cell-local position (actor_prepare_and_initialize
 * reads that view for every actor); linked actors also as the actor they
 * follow and its attachment vertex; object-linked actors as map-object slots
 * (the object a rising actor waits on, or the one an attached group rides). */
typedef struct KfActorHomeAnchor {
    u16 yaw;
    s16 local_z;
    s16 local_x;
} KfActorHomeAnchor;

typedef struct KfActorLinkedActorAnchor {
    u16 home_yaw;
    s16 actor_slot;
    s16 vertex_index;
} KfActorLinkedActorAnchor;

typedef struct KfActorObjectAnchor {
    u16 rise_object_slot;
    s16 attached_object_slot;
    s16 home_local_x;
} KfActorObjectAnchor;

typedef union KfActorAnchor {
    KfActorHomeAnchor home;
    KfActorLinkedActorAnchor linked_actor;
    KfActorObjectAnchor object_link;
} KfActorAnchor;
typedef char kf_actor_anchor_size[sizeof(KfActorAnchor) == 6 ? 1 : -1];

/* The 0x7c stride and these fields are fixed by the actor pool and phase
 * helpers. Other fields remain open. */
typedef struct KfActor {
    KfActorSlotState slot_state;
    u8 definition_id;
    u8 group_index;
    KfMapLayerMask current_map_layer;
    u8 zeroed_on_spawn;
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
    u8 zeroed_on_group_init;
    u8 unused_12;
    KfRenderQueueMode render_mode;
    KfLightingIndex lighting_override;
    u8 render_depth;
    s16 lighting_blend;
    u16 animation_phase;
    u16 health;
    u16 collision_radius;
    u16 collision_height;
    KfActorAnchor anchor;
    s16 vertical_anchor_offset;
    KfActorFlags flags;
    VECTOR position;
    s32 ballistic_origin_y;
    struct KfEulerAngles rotation;
    u8 unused_46[2];
    s16 model_scale_x;
    KfActorModelScaleY model_scale_y;
    s16 model_scale_z;
    u8 unused_4e[2];
    KfActorMotion motion;
    s16 turn_rate;
    struct KfPoolRecord *animation_cache;
    KfTargetCandidate *target;
    s16 movement_yaw;
    s16 animation_step;
    s16 ballistic_horizontal_speed;
    s16 ballistic_launch_speed_y;
    s16 ballistic_acceleration;
    u8 unused_6e[2];
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
typedef char kf_actor_home_yaw_offset[offsetof(KfActor, anchor.home.yaw) == 0x20 ? 1 : -1];
typedef char kf_actor_home_offset_z[offsetof(KfActor, anchor.home.local_z) == 0x22 ? 1 : -1];
typedef char kf_actor_home_offset_x[offsetof(KfActor, anchor.home.local_x) == 0x24 ? 1 : -1];
typedef char kf_actor_linked_actor_slot_offset[
    offsetof(KfActor, anchor.linked_actor.actor_slot) == 0x22 ? 1 : -1];
typedef char kf_actor_linked_vertex_offset[
    offsetof(KfActor, anchor.linked_actor.vertex_index) == 0x24 ? 1 : -1];
typedef char kf_actor_attached_object_offset[
    offsetof(KfActor, anchor.object_link.attached_object_slot) == 0x22 ? 1 : -1];
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
typedef char kf_actor_state_71_offset[offsetof(KfActor, state_70.keep_range.ledge_drop) == 0x71 ? 1 : -1];
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
