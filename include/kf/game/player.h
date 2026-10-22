#ifndef KF_GAME_PLAYER_H
#define KF_GAME_PLAYER_H

#include <kf/lib/bool.h>
#include <kf/lib/offsetof.h>
#include <kf/lib/types.h>
#include <kf/game/collision_flags.h>
#include <kf/game/item.h>
#include <kf/game/magic.h>
#include <kf/game/render_types.h>
#include <kf/game/pool.h>
#include <psyq/sdk.h>

struct KfMagicRecord;
struct KfEffectRecord;
struct KfActor;

/* Runtime progression limits (King's Field capped vitals at 9999 and
 * experience at 99999 and loaded forty growth rows). */
enum {
    KF_PLAYER_POWER_MAX = 999,
    KF_PLAYER_VITAL_MAX = 999,
    KF_PLAYER_EXPERIENCE_MAX = 999999,
    KF_PLAYER_LEVEL_MAX = 255,
    KF_PLAYER_LEVEL_GROWTH_COUNT = 100,
    KF_PLAYER_TRAINING_POINTS_PER_GAIN = 100
};

/* Player option bytes, their card copies and the options menu rows share
 * off/on (KF1 KfPlayerOption). */
KF_ENUM_BEGIN(KfPlayerOption, u8)
    KF_PLAYER_OPTION_OFF = 0,
    KF_PLAYER_OPTION_ON = 1
KF_ENUM_END(KfPlayerOption)

/* Vertical extent of the player in distance tests (King's Field's
 * KF_COLLISION_PLAYER_HEIGHT). */
enum {
    KF_PLAYER_HEIGHT = 1700,
    KF_PLAYER_COLLISION_RADIUS = 800,
    KF_PLAYER_CAMERA_EYE_OFFSET = 1600
};

enum {
    KF_PLAYER_CHARGE_FULL = 5000,
    KF_WEAPON_ATTACK_INACTIVE = -1
};

/* player_cap_status_components groups: FIRST caps curse and slow, SECOND
 * caps darkness (either also clears poison), THIRD caps slow and clears
 * paralysis. Member names stay WIP. */
KF_ENUM_BEGIN(KfPlayerStatusMask, u32)
    KF_PLAYER_STATUS_NONE = 0,
    KF_PLAYER_STATUS_FIRST = 1,
    KF_PLAYER_STATUS_SECOND = 2,
    KF_PLAYER_STATUS_THIRD = 4
KF_ENUM_END(KfPlayerStatusMask)
KF_ENUM_FLAGS(KfPlayerStatusMask, u32)

/* player_update_vertical_motion state (KF1 KfPlayerVerticalState). */
KF_ENUM_BEGIN(KfPlayerVerticalState, u8)
    KF_PLAYER_VERTICAL_GROUNDED = 0,
    KF_PLAYER_VERTICAL_FALLING = 0x10,
    KF_PLAYER_VERTICAL_STEP_UP = 0x20,
    KF_PLAYER_VERTICAL_DEEP_FALL = 0x40,
    KF_PLAYER_VERTICAL_LANDING = 0x50
KF_ENUM_END(KfPlayerVerticalState)

/* The byte at player +0xcd selects the camera, overlap, damage, and death
 * reaction paths in player_update_frame. */
KF_ENUM_BEGIN(KfPlayerReaction, u8)
    KF_PLAYER_REACTION_NORMAL = 0,
    KF_PLAYER_REACTION_MAP_OBJECT_FOLLOW = 1,
    KF_PLAYER_REACTION_MAP_OBJECT_APPROACH = 2,
    KF_PLAYER_REACTION_ROTATION = 3,
    KF_PLAYER_REACTION_OVERLAP_BOB = 4,
    KF_PLAYER_REACTION_POSITION_RECOVERY = 5,
    KF_PLAYER_REACTION_MOVING_DAMAGE = 0x10,
    KF_PLAYER_REACTION_DEATH = 0x11,
    KF_PLAYER_REACTION_ROTATION_DAMAGE = 0x12
KF_ENUM_END(KfPlayerReaction)

/* Buttons that stay pressed when the player's motion is cleared; the
 * direction and shoulder buttons are dropped. */
enum {
    KF_PLAYER_PAD_KEPT_ON_STOP = PADstart | PADRleft | PADRright | PADRup
};

/*
 * Record zero seeds a new player's values. Later records provide the next
 * absolute HP/MP values, stat steps, and cumulative experience threshold.
 * Past the last record the game extrapolates from the final two.
 */
typedef struct KfPlayerLevelGrowth {
    u16 maximum_hp;
    u16 maximum_mp;
    u16 physical_power_step;
    u16 magic_step;
    u32 experience_threshold;
} KfPlayerLevelGrowth;

typedef char kf_player_level_growth_size[sizeof(KfPlayerLevelGrowth) == 12 ? 1 : -1];

typedef struct KfPlayerVitals {
    u16 maximum_hp;
    u16 current_hp;
    u16 maximum_mp;
    u16 current_mp;
} KfPlayerVitals;

typedef struct KfEquipmentRecord {
    u8 unknown_00[2];
    u16 bonus_components[9];
    u16 hp_regen_interval;
    u16 hp_drain_interval;
    u8 unknown_18[8];
} KfEquipmentRecord;

typedef char kf_equipment_record_size[sizeof(KfEquipmentRecord) == 0x20 ? 1 : -1];
typedef char kf_equipment_record_bonuses_offset[
    offsetof(KfEquipmentRecord, bonus_components) == 2 ? 1 : -1];
typedef char kf_equipment_record_hp_regen_offset[
    offsetof(KfEquipmentRecord, hp_regen_interval) == 0x14 ? 1 : -1];
typedef char kf_equipment_record_hp_drain_offset[
    offsetof(KfEquipmentRecord, hp_drain_interval) == 0x16 ? 1 : -1];

/* The fourth equipment defense component resists poison application. */
enum { KF_PLAYER_COMBAT_POISON_RESISTANCE = 3 };

typedef struct KfWeaponRecordGame {
    u8 sound_id;
    u8 charge_rank;
    u8 unknown_02;
    KfEffectKind initial_effect_id;
    KfEffectKind release_effect_id;
    u8 magic_shots;
    u16 attack_components[8];
    u16 hp_regen_interval;
    u16 mp_regen_interval;
    u16 attack_angle;
    u16 attack_phase_step;
    u16 normal_attack_end_phase;
    u16 magic_window_start;
    u16 magic_window_end;
    u16 alternate_attack_phase_step;
    u16 alternate_attack_window_start;
    u16 alternate_attack_end_phase;
    u16 magic_phase_step;
    u16 normal_attack_sound_phase;
    u16 alternate_attack_sound_start_phase;
    u16 alternate_attack_sound_end_phase;
    u16 alternate_attack_sound_phase_step;
    u16 position_offset_x;
    u16 position_offset_y;
    u16 position_offset_z;
    s16 initial_vertex_index;
    u16 rotation_offset_x;
    u16 rotation_offset_y;
    u16 rotation_offset_z;
    s16 final_vertex_index;
} KfWeaponRecordGame;

typedef char kf_weapon_record_game_magic_window_offset[
    offsetof(KfWeaponRecordGame, magic_window_start) == 0x20 ? 1 : -1];
typedef char kf_weapon_record_game_initial_vertex_offset[
    offsetof(KfWeaponRecordGame, initial_vertex_index) == 0x3a ? 1 : -1];
typedef char kf_weapon_record_game_final_vertex_offset[
    offsetof(KfWeaponRecordGame, final_vertex_index) == 0x42 ? 1 : -1];

typedef struct KfWeaponAssetBuffer {
    u8 bytes[0xc000];
} KfWeaponAssetBuffer;

typedef char kf_weapon_asset_buffer_size[sizeof(KfWeaponAssetBuffer) == 0xc000 ? 1 : -1];

typedef struct KfPlayerViewRotation {
    s16 angles[3];
    u16 unknown_06;
} KfPlayerViewRotation;

typedef union KfPlayerViewRotationOffset {
    SVECTOR vector;
    u16 components[4];
} KfPlayerViewRotationOffset;
typedef char kf_player_view_rotation_offset_size[sizeof(KfPlayerViewRotationOffset) == 8 ? 1 : -1];

typedef struct KfQueuedMagicAction {
    KfEffectKind magic_id;
    u8 casts_remaining;
    u8 repeat_interval;
    u8 countdown;
} KfQueuedMagicAction;
typedef char kf_queued_magic_action_size[sizeof(KfQueuedMagicAction) == 4 ? 1 : -1];

typedef struct KfPlayerDamageReaction {
    SVECTOR rotation;
    SVECTOR motion;
} KfPlayerDamageReaction;

typedef struct KfPlayerViewReaction {
    u8 map_object_index;
    u8 approach_step;
    KfPlayerViewRotation rotation;
    u8 unknown_0a[6];
} KfPlayerViewReaction;

typedef struct KfPlayerPositionReaction {
    u8 recovery_step;
    VECTOR position;
} KfPlayerPositionReaction;

typedef char kf_player_position_reaction_size[
    sizeof(KfPlayerPositionReaction) == 0x14 ? 1 : -1];
typedef char kf_player_position_reaction_position_offset[
    offsetof(KfPlayerPositionReaction, position) == 4 ? 1 : -1];

/* PadRead(1) buttons for this frame, and the previous frame's copy taken
 * after the player update. */
typedef struct KfPlayerPadButtonHalves {
    u16 current;
    u16 previous;
} KfPlayerPadButtonHalves;

typedef union KfPlayerPadButtons {
    u32 word;
    u16 current;
    KfPlayerPadButtonHalves halves;
} KfPlayerPadButtons;

typedef char kf_player_pad_button_halves_size[
    sizeof(KfPlayerPadButtonHalves) == 4 ? 1 : -1];
typedef char kf_player_pad_buttons_previous_offset[
    offsetof(KfPlayerPadButtons, halves.previous) == 2 ? 1 : -1];
typedef char kf_player_pad_buttons_size[sizeof(KfPlayerPadButtons) == 4 ? 1 : -1];

/* Whole-word tests of BUTTON in both frames: newly pressed, or held. */
#define KF_PLAYER_PAD_BOTH_FRAMES(button) ((u32)(button) << 16 | (button))
#define KF_PLAYER_PAD_PRESSED(pad, button) \
    (((pad).word & KF_PLAYER_PAD_BOTH_FRAMES(button)) == (button))
#define KF_PLAYER_PAD_HELD(pad, button) \
    (((pad).word & KF_PLAYER_PAD_BOTH_FRAMES(button)) == KF_PLAYER_PAD_BOTH_FRAMES(button))

typedef union KfPlayerReactionOverlay {
    KfPlayerDamageReaction damage;
    KfPlayerViewReaction view;
    KfPlayerPositionReaction position;
    u16 angle_phase;
} KfPlayerReactionOverlay;

typedef char kf_player_reaction_overlay_size[
    sizeof(KfPlayerReactionOverlay) == 0x14 ? 1 : -1];
typedef char kf_player_reaction_view_rotation_offset[
    offsetof(KfPlayerReactionOverlay, view.rotation) == 2 ? 1 : -1];
typedef char kf_player_reaction_damage_motion_offset[
    offsetof(KfPlayerReactionOverlay, damage.motion) == 8 ? 1 : -1];
typedef char kf_player_reaction_position_offset[
    offsetof(KfPlayerReactionOverlay, position.position) == 4 ? 1 : -1];

typedef union KfPlayerMovementSpeed {
    u16 unsigned_value;
    s16 signed_value;
} KfPlayerMovementSpeed;

typedef char kf_player_movement_speed_size[
    sizeof(KfPlayerMovementSpeed) == 2 ? 1 : -1];

typedef char kf_weapon_record_game_size[sizeof(KfWeaponRecordGame) == 0x44 ? 1 : -1];
typedef char kf_weapon_record_game_hp_regen_offset[
    offsetof(KfWeaponRecordGame, hp_regen_interval) == 0x16 ? 1 : -1];
typedef char kf_weapon_record_game_mp_regen_offset[
    offsetof(KfWeaponRecordGame, mp_regen_interval) == 0x18 ? 1 : -1];
typedef char kf_weapon_record_game_normal_end_offset[
    offsetof(KfWeaponRecordGame, normal_attack_end_phase) == 0x1e ? 1 : -1];
typedef char kf_weapon_record_game_alternate_end_offset[
    offsetof(KfWeaponRecordGame, alternate_attack_end_phase) == 0x28 ? 1 : -1];
typedef char kf_weapon_record_game_attacks_offset[
    offsetof(KfWeaponRecordGame, attack_components) == 6 ? 1 : -1];
typedef char kf_weapon_record_game_alternate_phase_step_offset[
    offsetof(KfWeaponRecordGame, alternate_attack_phase_step) == 0x24 ? 1 : -1];
typedef char kf_weapon_record_game_alternate_window_offset[
    offsetof(KfWeaponRecordGame, alternate_attack_window_start) == 0x26 ? 1 : -1];
typedef char kf_weapon_record_game_normal_sound_offset[
    offsetof(KfWeaponRecordGame, normal_attack_sound_phase) == 0x2c ? 1 : -1];
typedef char kf_weapon_record_game_alternate_sound_step_offset[
    offsetof(KfWeaponRecordGame, alternate_attack_sound_phase_step) == 0x32 ? 1 : -1];

typedef struct KfPlayerMagicIdSequence {
    u8 effect_ids[12];
} KfPlayerMagicIdSequence;

typedef char kf_player_magic_id_sequence_size[
    sizeof(KfPlayerMagicIdSequence) == 0x0c ? 1 : -1];

/* One magic spawn offset; its SVECTOR pad slot carries the effect kind. */
typedef struct KfPlayerMagicSpawnFields {
    s16 x;
    s16 y;
    s16 z;
    KF_ENUM_STORAGE(KfEffectKind, s16) effect_kind;
} KfPlayerMagicSpawnFields;

typedef union KfPlayerMagicSpawnRecord {
    SVECTOR offset;
    KfPlayerMagicSpawnFields fields;
} KfPlayerMagicSpawnRecord;

typedef char kf_player_magic_spawn_record_size[
    sizeof(KfPlayerMagicSpawnRecord) == 8 ? 1 : -1];
typedef char kf_player_magic_spawn_effect_kind_offset[
    offsetof(KfPlayerMagicSpawnRecord, fields.effect_kind) == 6 ? 1 : -1];

typedef struct KfMapOccupancyLayer {
    u8 object_index;
    u8 elevation;
    u8 quarter_turns;
    u8 collision_shape_id;
    u8 lighting_index;
} KfMapOccupancyLayer;

typedef KfMapOccupancyLayer KfMapCellShape;
typedef char kf_map_cell_shape_size[sizeof(KfMapCellShape) == 5 ? 1 : -1];
typedef char kf_map_cell_orientation_offset[
    offsetof(KfMapCellShape, quarter_turns) == 2 ? 1 : -1];
typedef char kf_map_cell_lighting_offset[
    offsetof(KfMapCellShape, lighting_index) == 4 ? 1 : -1];

typedef struct KfMapOccupancyCell {
    KfMapOccupancyLayer layer[2];
} KfMapOccupancyCell;

typedef char kf_map_occupancy_cell_size[sizeof(KfMapOccupancyCell) == 10 ? 1 : -1];

enum { KF_MAP_WORLD_GRID_SIDE = 80 };

/* Heights produced by one shape query. The shape evaluator addresses them
 * through a single pointer to this block. */
typedef struct KfCollisionHeights {
    s32 height;
    s32 result;
    s32 height_limit;
    s32 lower_bound;
    s32 upper_bound;
} KfCollisionHeights;

typedef char kf_collision_heights_size[sizeof(KfCollisionHeights) == 0x14 ? 1 : -1];

/* Interior collision-query state within the startup-cleared BSS owner. */
typedef struct KfCollisionCache {
    KfMapOccupancyCell *cell;
    KfMapOccupancyLayer *shape;
    u8 unknown_08[2];
    u16 layer;
    KfCollisionHeights heights;
    KfCollisionHitFlags flags;
    s32 actor_index;
    s32 object_index;
    u8 unknown_2c[4];
    VECTOR position;
    u16 radius;
    u16 interaction_height;
} KfCollisionCache;

typedef char kf_collision_cache_size[sizeof(KfCollisionCache) == 0x44 ? 1 : -1];
typedef char kf_collision_cache_height_offset[
    offsetof(KfCollisionCache, heights) == 0x0c ? 1 : -1];
typedef char kf_collision_cache_position_offset[
    offsetof(KfCollisionCache, position) == 0x30 ? 1 : -1];
typedef char kf_collision_cache_radius_offset[
    offsetof(KfCollisionCache, radius) == 0x40 ? 1 : -1];

/* The resource transition loads 0xfa00 bytes of 80-by-80 map cells. Startup
 * clears the complete BSS region, whose later storage remains partly opaque. */
typedef struct KfBss801c7540 {
    KfMapOccupancyCell map_cells[KF_MAP_WORLD_GRID_SIDE][KF_MAP_WORLD_GRID_SIDE];
    u8 unknown_fa00[0x600];
    u8 shape_bank[0x1800];
    KfCollisionCache collision_cache;
} KfBss801c7540;

typedef char kf_map_cells_loaded_size[
    sizeof(((KfBss801c7540 *)NULL)->map_cells) == 0xfa00 ? 1 : -1];
typedef char kf_bss_801c7540_size[sizeof(KfBss801c7540) == 0x11844 ? 1 : -1];
typedef char kf_collision_shape_bank_offset[
    offsetof(KfBss801c7540, shape_bank) == 0x10000 ? 1 : -1];
typedef char kf_collision_shape_bank_size[
    sizeof(((KfBss801c7540 *)NULL)->shape_bank) == 0x1800 ? 1 : -1];
enum { KF_EQUIPMENT_RECORD_FIRST_ID = 21, KF_EQUIPMENT_RECORD_COUNT = 64 };

/* player_set_equipment_slot target; NONE only refreshes the record
 * pointers of the current equipment. */
KF_ENUM_BEGIN(KfEquipmentSlot, u8)
    KF_EQUIPMENT_SLOT_HEAD = 0,
    KF_EQUIPMENT_SLOT_BODY = 1,
    KF_EQUIPMENT_SLOT_LEG = 2,
    KF_EQUIPMENT_SLOT_SHIELD = 3,
    KF_EQUIPMENT_SLOT_ARM = 4,
    KF_EQUIPMENT_SLOT_ACCESSORY = 5,
    KF_EQUIPMENT_SLOT_EXTRA = 6,
    KF_EQUIPMENT_SLOT_NONE = 0xff
KF_ENUM_END(KfEquipmentSlot)

/* Encoded empty byte of the menu's equipped item/magic label rows (lane B
 * menu locals, which mix KfObjectId and KfEffectKind values). */
enum { KF_EQUIPMENT_NONE = 0xff };

/*
 * The player object: game_main_loop clears 0x160 bytes at its base and the
 * experience routine addresses members from a registered base. Unknown spans
 * remain opaque.
 */
typedef struct KfPlayerState {
    s32 experience;
    s32 next_level_experience;
    u8 level;
    u8 unknown_09;
    b8 force_actor_lifecycle_refresh;
    u8 weapon_charge_delay;
    u8 unknown_0c;
    u8 movement_speed_adjustment_decay_latch;
    s16 movement_speed_adjustment_q12;
    s16 damage_scale;
    KfPlayerVitals vitals;
    u16 attack_charge_current;
    u16 attack_charge_committed;
    u16 magic_charge;
    u16 physical_power_training;
    u16 magic_training;
    u16 base_physical_power;
    u16 base_magic;
    u16 physical_power;
    u16 magic;
    u32 gold;
    u16 attack_components[8];
    u16 unknown_40;
    u16 combat_components[9];
    s16 poison_timer;
    s16 curse_strength;
    u16 curse_phase_limit;
    s16 darkness_phase;
    u16 darkness_phase_limit;
    s16 slow_timer;
    s16 paralysis_timer;
    s16 defense_boost_timer;
    s16 attack_boost_timer;
    s16 magic_tint_phase;
    s16 magic_tint_phase_limit;
    s16 map_marker_visual_effect_timer;
    s16 full_mp_timer;
    s16 magic_boost_timer;
    u8 unknown_70[4];
    u32 equipment_effect_ticks;
    const u16 *magic_attack_mask_cursor;
    struct KfMagicRecord *selected_magic_record;
    KfWeaponRecordGame *equipped_weapon_record;
    struct KfAssetHeader *weapon_asset_buffer;
    struct KfPoolRecord *weapon_animation_cache;
    struct KfEffectRecord *weapon_effect;
    s16 weapon_attack_phase;
    s16 weapon_attack_window;
    s16 weapon_next_sound_phase;
    u8 weapon_magic_shots_remaining;
    KfEffectKind primary_magic_shortcut_id;
    KfEffectKind secondary_magic_shortcut_id;
    KF_ENUM_STORAGE(KfObjectId, u8) secondary_item_shortcut_id;
    KfAnimationClip weapon_attack_mode;
    KF_ENUM_STORAGE(KfObjectId, u8) equipped_weapon_id;
    u8 unknown_9c[2];
    u8 weapon_magic_shots_configured;
    b8 weapon_attack_fully_charged;
    b8 weapon_guard_active;
    KfEquipmentRecord *equipped_head_record;
    KfEquipmentRecord *equipped_body_record;
    KfEquipmentRecord *equipped_arm_record;
    KfEquipmentRecord *equipped_leg_record;
    KfEquipmentRecord *equipped_shield_record;
    KfEquipmentRecord *equipped_accessory_record;
    KfEquipmentRecord *equipped_extra_record;
    KF_ENUM_STORAGE(KfObjectId, u8) equipped_head_id;
    KF_ENUM_STORAGE(KfObjectId, u8) equipped_body_id;
    KF_ENUM_STORAGE(KfObjectId, u8) equipped_arm_id;
    KF_ENUM_STORAGE(KfObjectId, u8) equipped_leg_id;
    KF_ENUM_STORAGE(KfObjectId, u8) equipped_shield_id;
    KF_ENUM_STORAGE(KfObjectId, u8) equipped_accessory_id;
    KF_ENUM_STORAGE(KfObjectId, u8) equipped_extra_id;
    KfPlayerOption audio_effects_enabled;
    KfPlayerOption audio_music_enabled;
    KfPlayerOption hud_gauges_enabled;
    KfPlayerOption compass_enabled;
    KfPlayerOption item_preview_enabled;
    KfPlayerOption walking_bob_enabled;
    KfPlayerReaction death_state;
    u8 unknown_ce[2];
    KfPlayerVerticalState vertical_motion_state;
    KfQueuedMagicAction queued_magic_action;
    b8 fatal_fall_latch;
    VECTOR camera_position;
    SVECTOR frame_displacement;
    KfPlayerViewRotation camera_rotation;
    KfPlayerViewRotation camera_rotation_target;
    s16 reaction_rotation_offset[3];
    s16 death_transition_frame;
    KfPlayerViewRotationOffset view_rotation_offset;
    s16 vertical_motion_pitch_offset;
    s16 camera_yaw_roll_offsets[2];
    u8 unknown_116[2];
    SVECTOR magic_origin_offset;
    s32 collision_lower_clearance;
    s32 collision_upper_clearance;
    u16 map_layer_index;
    s16 strafe_velocity;
    s16 forward_velocity;
    KfPlayerMovementSpeed movement_speed;
    s16 yaw_step;
    s16 pitch_step;
    s16 camera_vertical_offset;
    s16 walking_bob_phase;
    s16 landing_vertical_offset;
    s16 vertical_velocity;
    s16 damage_red_overlay_scale;
    s16 damage_red_overlay_decay;
    KfPlayerPadButtons pad_buttons;
    s32 movement_step_limit;
    s32 turn_step_limit;
    KfPlayerReactionOverlay reaction;
} KfPlayerState;

typedef char kf_player_state_size[sizeof(KfPlayerState) == 0x160 ? 1 : -1];
typedef char kf_player_movement_speed_adjustment_decay_latch_offset[
    offsetof(KfPlayerState, movement_speed_adjustment_decay_latch) == 0x0d ? 1 : -1];
typedef char kf_player_lifecycle_refresh_offset[
    offsetof(KfPlayerState, force_actor_lifecycle_refresh) == 0x0a ? 1 : -1];
typedef char kf_player_primary_magic_shortcut_id_offset[
    offsetof(KfPlayerState, primary_magic_shortcut_id) == 0x97 ? 1 : -1];
typedef char kf_player_combat_components_offset[
    offsetof(KfPlayerState, combat_components) == 0x42 ? 1 : -1];
typedef char kf_player_magic_attack_mask_cursor_offset[
    offsetof(KfPlayerState, magic_attack_mask_cursor) == 0x78 ? 1 : -1];
typedef char kf_player_weapon_next_sound_phase_offset[
    offsetof(KfPlayerState, weapon_next_sound_phase) == 0x94 ? 1 : -1];
typedef char kf_player_selected_magic_record_offset[
    offsetof(KfPlayerState, selected_magic_record) == 0x7c ? 1 : -1];
typedef char kf_player_equipped_head_record_offset[
    offsetof(KfPlayerState, equipped_head_record) == 0xa4 ? 1 : -1];
typedef char kf_player_equipped_head_id_offset[
    offsetof(KfPlayerState, equipped_head_id) == 0xc0 ? 1 : -1];
typedef char kf_player_death_state_offset[
    offsetof(KfPlayerState, death_state) == 0xcd ? 1 : -1];
typedef char kf_player_queued_magic_action_offset[
    offsetof(KfPlayerState, queued_magic_action) == 0xd1 ? 1 : -1];
typedef char kf_player_fatal_fall_latch_offset[
    offsetof(KfPlayerState, fatal_fall_latch) == 0xd5 ? 1 : -1];
typedef char kf_player_camera_position_offset[
    offsetof(KfPlayerState, camera_position) == 0xd8 ? 1 : -1];
typedef char kf_player_frame_displacement_offset[
    offsetof(KfPlayerState, frame_displacement) == 0xe8 ? 1 : -1];
typedef char kf_player_vertical_motion_pitch_offset[
    offsetof(KfPlayerState, vertical_motion_pitch_offset) == 0x110 ? 1 : -1];
typedef char kf_player_camera_yaw_roll_offsets_offset[
    offsetof(KfPlayerState, camera_yaw_roll_offsets) == 0x112 ? 1 : -1];
typedef char kf_player_death_rotation_offset[
    offsetof(KfPlayerState, reaction) == 0x14c ? 1 : -1];
typedef char kf_player_movement_speed_offset[
    offsetof(KfPlayerState, movement_speed) == 0x12e ? 1 : -1];
typedef char kf_player_pad_buttons_offset[
    offsetof(KfPlayerState, pad_buttons) == 0x140 ? 1 : -1];
typedef char kf_player_turn_step_limit_offset[
    offsetof(KfPlayerState, turn_step_limit) == 0x148 ? 1 : -1];
typedef char kf_player_movement_step_limit_offset[
    offsetof(KfPlayerState, movement_step_limit) == 0x144 ? 1 : -1];

extern KfPlayerLevelGrowth player_level_growth_table[KF_PLAYER_LEVEL_GROWTH_COUNT];
extern KfPlayerState player_state;
extern KfPlayerMagicIdSequence player_magic_id_sequence;
/* US-only .sbss copy of the attack masks, refreshed from the pad masks. */
extern u16 player_magic_attack_masks[4];
extern KfBss801c7540 bss_801c7540;
extern KfWeaponRecordGame player_weapon_records[18];
extern KfEquipmentRecord player_equipment_records[KF_EQUIPMENT_RECORD_COUNT];

b32 player_move_horizontal(s32 heading, s32 distance);
void player_recalculate_combat_stats(void);
void player_increment_physical_power_training(void);
void player_increment_magic_training(void);
void player_add_experience(s16 amount);
void player_clear_motion(void);
s32 player_distance_to_point_in_cone(
    const VECTOR *point, s16 facing, s32 max_distance, s32 angle_tolerance);
s32 player_distance_to_point(
    s32 point_x, s32 point_y, s32 point_z, s32 max_distance, s32 point_height);
s32 player_distance_to_point_with_margin(
    s32 point_x, s32 point_y, s32 point_z, s32 max_distance, s32 point_height);
void player_update_collision_bounds(void);
void player_reload_map_resources(s32 first, s32 second, s32 third, s32 fourth,
                   s32 fifth, s32 optional_resource);
s32 player_charge_gain_for_rank(s32 value, s32 rank);
void player_add_equipment_bonuses(KF_ENUM_PARAM(KfObjectId, s32) item_id);
s32 player_calculate_damage_component(s32 base_power, s32 defense, s32 attack);
void player_apply_damage_reaction(const VECTOR *origin, s32 damage, s32 reaction_flags);
/* player_apply_damage's flags word (actor attacks, magic records): the low
 * nibble is a status kind plus one, which the status-guard accessory can
 * clear, and bits 6 and 7 select the strong and rotation-only reactions
 * of player_apply_damage_reaction. */
enum {
    KF_PLAYER_DAMAGE_STATUS_MASK = 0xf,
    KF_PLAYER_DAMAGE_GUARDED_STATUS = 0x7,
    KF_PLAYER_DAMAGE_REACTION_STRONG = 0x40,
    KF_PLAYER_DAMAGE_REACTION_ROTATION_ONLY = 0x80
};

void player_apply_damage(u16 damage0, u16 damage1, u16 damage2, u16 damage_flags,
                   u16 damage3, u16 damage4, u16 damage5, u16 damage6,
                   u16 damage7, u16 scale_q16, u16 multiplier_tenths,
                   const VECTOR *origin);
void player_apply_radial_damage(VECTOR *position, s32 start, s32 end, s32 mode,
                   u16 falloff, u16 damage0, u16 damage1, u16 damage2,
                   u16 damage3, u16 damage4, u16 damage5, u16 damage6,
                   u16 damage7, u16 damage8, s32 scale_and_flags, u16 record_id);
void player_adjust_hp_unclamped(s32 delta);
void player_cap_status_components(KfPlayerStatusMask mask);
void player_death_begin(const SVECTOR *rotation);
void player_adjust_hp(s32 delta);
void player_adjust_mp(s32 delta);
void player_set_primary_magic_shortcut_id(KfEffectKind value);
void player_set_secondary_magic_shortcut_id(KfEffectKind value);
void player_set_secondary_item_shortcut_id(KF_ENUM_PARAM(KfObjectId, u8) value);
void player_set_equipment_slot(KF_ENUM_PARAM(KfObjectId, u8) item_id, KfEquipmentSlot slot);
void player_equip_weapon(KF_ENUM_PARAM(KfObjectId, u8) weapon_id);
struct KfActor *player_probe_view_target_and_vectors(s32 scale, VECTOR *position,
                              SVECTOR *direction, s32 *distance);
void player_dispatch_magic_effect(KF_ENUM_PARAM(KfEffectKind, s32) effect_id, ...);
void player_sample_weapon_world_vertex(s32 vertex_index, VECTOR *output);
void player_update_weapon_attack(void);
void player_select_magic_action(KF_ENUM_PARAM(KfEffectKind, s32) magic_id);
void player_update_vertical_motion(void);
b32 player_move_reaction_with_collision(void);
void player_update_camera_rotation(void);
void player_update_horizontal_motion(void);
b32 item_id_is_71_to_80(KF_ENUM_PARAM(KfObjectId, s32) value);
void player_update_actions_and_charge(void);
void player_render_frame_and_release_pool(void);
void player_begin_view_reaction(u8 map_object_index);
void player_begin_rotation_reaction(const SVECTOR *rotation);
void player_begin_moving_damage_reaction(const SVECTOR *rotation, const SVECTOR *motion,
                   s16 duration);
void player_begin_rotation_only_damage_reaction(const SVECTOR *rotation, const SVECTOR *motion,
                   s16 duration);
void player_update_frame(void);
void render_frames_with_color_overlay(KF_ENUM_PARAM(KfColorOverlayControl, s32) mode, s32 phase, s32 last_phase,
    s32 step);
void player_begin_weapon_attack(KF_ENUM_PARAM(KfAnimationClip, s32) mode);
void player_reset_status(void);
void player_get_camera_pose(VECTOR *position, SVECTOR *angles);
void player_reset_view(void);
void player_restore_equipment_effects(void);
void player_sync_position_to_map(void);
b32 player_meets_weapon_magic_power_requirement(void);
void player_initialize_state(void);
void game_initialize_session(void);

#endif
