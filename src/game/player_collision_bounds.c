#include <kf/lib/address.h>
#include <kf/lib/null.h>
#include <kf/lib/math.h>
#include <kf/game/collision_cache.h>
#include <kf/game/player.h>

enum {
    COLLISION_LOWER_DEATH_LIMIT = -1000
};

ADDRESS(0x80023ecc, 0xac)
void player_update_collision_bounds(void)
{
    s32 lower;
    s32 upper;

    lower = KF_COLLISION_CACHE_LOWER_BOUND + KF_PLAYER_CAMERA_EYE_OFFSET;
    lower -= player_state.camera_vertical_offset + player_state.camera_position.vy
           + player_state.landing_vertical_offset;
    player_state.collision_lower_clearance = lower;
    if (lower < COLLISION_LOWER_DEATH_LIMIT) {
        player_death_begin(NULL);
    }

    upper = KF_COLLISION_CACHE_UPPER_BOUND + KF_PLAYER_CAMERA_EYE_OFFSET;
    upper -= player_state.camera_vertical_offset + player_state.camera_position.vy
           + player_state.landing_vertical_offset;
    player_state.collision_upper_clearance = upper;
    if (upper <= 0) {
        player_death_begin(NULL);
    }
}

ADDRESS(0x80023f78, 0x54)
s32 player_distance_to_point_with_margin(
    s32 point_x, s32 point_y, s32 point_z, s32 max_distance,
    s32 point_height)
{
    return vector_distance_to_point(
        &player_state.camera_position, point_x, point_y, point_z,
        max_distance + KF_PLAYER_COLLISION_RADIUS, KF_PLAYER_HEIGHT,
        point_height);
}

enum {
    PLAYER_COLLISION_CLEARANCE_INITIAL = 10000
};

ADDRESS(0x80023fcc, 0xec)
void player_reset_view(void)
{
    player_state.camera_vertical_offset = 0;
    player_state.walking_bob_phase = 0;
    player_state.landing_vertical_offset = 0;
    player_state.vertical_motion_state = KF_PLAYER_VERTICAL_GROUNDED;
    player_state.vertical_velocity = 0;
    player_state.death_state = KF_PLAYER_REACTION_NORMAL;
    player_state.camera_yaw_roll_offsets[1] = 0;
    player_state.camera_yaw_roll_offsets[0] = 0;
    player_state.vertical_motion_pitch_offset = 0;
    player_state.view_rotation_offset.components[2] = 0;
    player_state.view_rotation_offset.components[1] = 0;
    player_state.view_rotation_offset.components[0] = 0;
    player_state.reaction_rotation_offset[2] = 0;
    player_state.reaction_rotation_offset[1] = 0;
    player_state.reaction_rotation_offset[0] = 0;
    player_state.camera_rotation = player_state.camera_rotation_target;
    player_state.movement_speed_adjustment_q12 = 0;
    player_state.movement_speed_adjustment_decay_latch = 0;
    player_state.damage_scale = KF_FIXED12_ONE;
    player_state.queued_magic_action.magic_id = KF_MAGIC_NONE;
    player_state.collision_lower_clearance = PLAYER_COLLISION_CLEARANCE_INITIAL;
    player_state.collision_upper_clearance = PLAYER_COLLISION_CLEARANCE_INITIAL;
}
