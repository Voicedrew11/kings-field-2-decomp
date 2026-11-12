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
    lower -= cur_avatar_status.camera_vertical_offset + cur_avatar_status.camera_position.vy
           + cur_avatar_status.landing_vertical_offset;
    cur_avatar_status.collision_lower_clearance = lower;
    if (lower < COLLISION_LOWER_DEATH_LIMIT) {
        player_death_begin(NULL);
    }

    upper = KF_COLLISION_CACHE_UPPER_BOUND + KF_PLAYER_CAMERA_EYE_OFFSET;
    upper -= cur_avatar_status.camera_vertical_offset + cur_avatar_status.camera_position.vy
           + cur_avatar_status.landing_vertical_offset;
    cur_avatar_status.collision_upper_clearance = upper;
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
        &cur_avatar_status.camera_position, point_x, point_y, point_z,
        max_distance + KF_PLAYER_COLLISION_RADIUS, KF_PLAYER_HEIGHT,
        point_height);
}

enum {
    PLAYER_COLLISION_CLEARANCE_INITIAL = 10000
};

ADDRESS(0x80023fcc, 0xec)
void player_reset_view(void)
{
    cur_avatar_status.camera_vertical_offset = 0;
    cur_avatar_status.walking_bob_phase = 0;
    cur_avatar_status.landing_vertical_offset = 0;
    cur_avatar_status.vertical_motion_state = KF_PLAYER_VERTICAL_GROUNDED;
    cur_avatar_status.vertical_velocity = 0;
    cur_avatar_status.death_state = KF_PLAYER_REACTION_NORMAL;
    cur_avatar_status.camera_yaw_roll_offsets[1] = 0;
    cur_avatar_status.camera_yaw_roll_offsets[0] = 0;
    cur_avatar_status.vertical_motion_pitch_offset = 0;
    cur_avatar_status.view_rotation_offset.components[2] = 0;
    cur_avatar_status.view_rotation_offset.components[1] = 0;
    cur_avatar_status.view_rotation_offset.components[0] = 0;
    cur_avatar_status.reaction_rotation_offset[2] = 0;
    cur_avatar_status.reaction_rotation_offset[1] = 0;
    cur_avatar_status.reaction_rotation_offset[0] = 0;
    cur_avatar_status.camera_rotation = cur_avatar_status.camera_rotation_target;
    cur_avatar_status.movement_speed_adjustment_q12 = 0;
    cur_avatar_status.movement_speed_adjustment_decay_latch = 0;
    cur_avatar_status.damage_scale = KF_FIXED12_ONE;
    cur_avatar_status.queued_magic_action.magic_id = KF_MAGIC_NONE;
    cur_avatar_status.collision_lower_clearance = PLAYER_COLLISION_CLEARANCE_INITIAL;
    cur_avatar_status.collision_upper_clearance = PLAYER_COLLISION_CLEARANCE_INITIAL;
}
