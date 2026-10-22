#include <kf/lib/address.h>
#include <kf/lib/types.h>
#include <kf/lib/math.h>
#include <psyq/libc.h>
#include <kf/game/render_model.h>

/* GAME .data opens with these two objects, ahead of every later unit's
   contribution; the renderer passes the matrix as its world transform. */
DATA(0x80064b30, 0x20, ".data")
MATRIX render_world_identity_matrix = {
    {{KF_FIXED12_ONE, 0, 0}, {0, KF_FIXED12_ONE, 0}, {0, 0, KF_FIXED12_ONE}},
    {0, 0, 0}
};

/* Unreferenced in every retail build; owner and element type unresolved. */
DATA(0x80064b50, 0x14, ".data")
static u8 matrix_rotation_unreferenced_bytes[20] = {
    0, 0, 0, 0, 0, 1, 1, 1, 1, 0, 2, 2, 2, 2, 0, 3, 3, 3, 3, 0
};

enum {
    RANDOM_TRIANGULAR_CENTER = 0x8000,
    LERP_HALFWORD_COUNT = 9
};

/*
 * Angles are 12-bit (0..0xfff). Within a half turn the step is applied
 * directly and clamped at the target; beyond it the angle wraps the other
 * way and the wrapped value is clamped only while it stays on the target's
 * side of the half-turn boundary.
 */
ADDRESS(0x8001481c, 0xc4)
s16 angle_approach(s16 current, s16 target, s32 step)
{
    s16 result;

    current &= KF_ANGLE_WRAP_MASK;
    target &= KF_ANGLE_WRAP_MASK;
    if (target == current) {
        return target;
    }
    if (current < target) {
        if (target - current <= KF_ANGLE_HALF_TURN) {
            result = current + step;
            if (target < result) {
                return target;
            }
        } else {
            result = (current - step) & KF_ANGLE_WRAP_MASK;
            if (result >= KF_ANGLE_HALF_TURN && result <= target) {
                return target;
            }
        }
    } else {
        if (current - target <= KF_ANGLE_HALF_TURN) {
            result = current - step;
            if (result < target) {
                return target;
            }
        } else {
            result = (current + step) & KF_ANGLE_WRAP_MASK;
            if (result <= KF_ANGLE_HALF_TURN && result >= target) {
                return target;
            }
        }
    }
    return result;
}

/* Moves CURRENT toward TARGET by |STEP| without passing it; a negative STEP
 * is applied with the opposite sign. */
ADDRESS(0x800148e0, 0x68)
s32 value_approach(s32 current, s32 target, s32 step)
{
    if (step > 0) {
        if (current < target) {
            current += step;
            if (target < current) {
                current = target;
            }
        } else if (target < current) {
            current -= step;
            if (current < target) {
                current = target;
            }
        }
    } else {
        if (target < current) {
            current += step;
            if (current < target) {
                current = target;
            }
        } else if (current < target) {
            current -= step;
            if (target < current) {
                current = target;
            }
        }
    }
    return current;
}

/* DIRECTION = (TO - FROM) scaled to length SCALE. */
ADDRESS(0x80014948, 0x13c)
void vector_direction_scaled(VECTOR *from, VECTOR *to, s32 scale, SVECTOR *direction)
{
    s32 dx = to->vx - from->vx;
    s32 dy = to->vy - from->vy;
    s32 dz = to->vz - from->vz;
    s32 length = fixed_vector3_length(dx, dy, dz);

    direction->vx = dx * scale / length;
    direction->vy = dy * scale / length;
    direction->vz = dz * scale / length;
}

/* Turns VELOCITY toward TARGET by ACCELERATION and damps it. When CURRENT is
 * within the angular and velocity tolerances, return the remaining angle. */
ADDRESS(0x80014a84, 0xb8)
s32 angle_velocity_step(s32 current, s32 target, s32 velocity, s32 acceleration, s32 damping)
{
    if (angle_within_tolerance(current, target, acceleration >> 1)
        && velocity <= acceleration && -acceleration <= velocity) {
        return current - target;
    }
    if (target != current) {
        if (angle_mod_delta_le_half_turn(target, current)) {
            velocity -= acceleration;
        } else {
            velocity += acceleration;
        }
    }
    if (velocity > 0) {
        velocity -= damping;
    } else if (velocity < 0) {
        velocity += damping;
    }
    return velocity;
}

ADDRESS(0x80014b3c, 0x4c)
void angle_to_forward_xz(s16 angle, struct KfVecXZi *direction)
{
    direction->x = -rsin(angle);
    direction->z = rcos(angle);
}

/* DESTINATION = SOURCE turned by TURNS quarter turns about Y. */
ADDRESS(0x80014b88, 0x224)
void matrix_rotate_quarter_turns(MATRIX *source, MATRIX *destination,
    KF_ENUM_PARAM(KfQuarterTurn, s32) turns)
{
    switch (turns) {
    case KF_QUARTER_TURN_0:
        destination->m[0][0] = source->m[0][0];
        destination->m[0][1] = source->m[0][1];
        destination->m[0][2] = source->m[0][2];
        destination->m[1][0] = source->m[1][0];
        destination->m[1][1] = source->m[1][1];
        destination->m[1][2] = source->m[1][2];
        destination->m[2][0] = source->m[2][0];
        destination->m[2][1] = source->m[2][1];
        destination->m[2][2] = source->m[2][2];
        break;
    case KF_QUARTER_TURN_1:
        destination->m[0][0] = -source->m[0][2];
        destination->m[0][1] = source->m[0][1];
        destination->m[0][2] = source->m[0][0];
        destination->m[1][0] = -source->m[1][2];
        destination->m[1][1] = source->m[1][1];
        destination->m[1][2] = source->m[1][0];
        destination->m[2][0] = -source->m[2][2];
        destination->m[2][1] = source->m[2][1];
        destination->m[2][2] = source->m[2][0];
        break;
    case KF_QUARTER_TURN_2:
        destination->m[0][0] = -source->m[0][0];
        destination->m[0][1] = source->m[0][1];
        destination->m[0][2] = -source->m[0][2];
        destination->m[1][0] = -source->m[1][0];
        destination->m[1][1] = source->m[1][1];
        destination->m[1][2] = -source->m[1][2];
        destination->m[2][0] = -source->m[2][0];
        destination->m[2][1] = source->m[2][1];
        destination->m[2][2] = -source->m[2][2];
        break;
    case KF_QUARTER_TURN_3:
        destination->m[0][0] = source->m[0][2];
        destination->m[0][1] = source->m[0][1];
        destination->m[0][2] = -source->m[0][0];
        destination->m[1][0] = source->m[1][2];
        destination->m[1][1] = source->m[1][1];
        destination->m[1][2] = -source->m[1][0];
        destination->m[2][0] = source->m[2][2];
        destination->m[2][1] = source->m[2][1];
        destination->m[2][2] = -source->m[2][0];
        break;
    }
}

ADDRESS(0x80014dac, 0xe4)
void svector_rotate_quarter_turns(SVECTOR *source, SVECTOR *destination,
    KF_ENUM_PARAM(KfQuarterTurn, s32) turns)
{
    switch (turns) {
    case KF_QUARTER_TURN_0:
        copyVector(destination, source);
        break;
    case KF_QUARTER_TURN_1:
        destination->vx = -source->vz;
        destination->vy = source->vy;
        destination->vz = source->vx;
        break;
    case KF_QUARTER_TURN_2:
        destination->vx = -source->vx;
        destination->vy = source->vy;
        destination->vz = -source->vz;
        break;
    case KF_QUARTER_TURN_3:
        destination->vx = source->vz;
        destination->vy = source->vy;
        destination->vz = -source->vx;
        break;
    }
}

ADDRESS(0x80014e90, 0x70)
void matrix_set_rotation_x(s16 angle, MATRIX *matrix)
{
    s32 sine = rsin(angle);
    s32 cosine = rcos(angle);

    matrix->m[0][0] = KF_FIXED12_ONE;
    matrix->m[0][1] = 0;
    matrix->m[0][2] = 0;
    matrix->m[1][0] = 0;
    matrix->m[1][1] = cosine;
    matrix->m[1][2] = -sine;
    matrix->m[2][0] = 0;
    matrix->m[2][1] = sine;
    matrix->m[2][2] = cosine;
}

ADDRESS(0x80014f00, 0x70)
void matrix_set_rotation_y(s16 angle, MATRIX *matrix)
{
    s32 sine = rsin(angle);
    s32 cosine = rcos(angle);

    matrix->m[0][0] = cosine;
    matrix->m[0][1] = 0;
    matrix->m[0][2] = -sine;
    matrix->m[1][0] = 0;
    matrix->m[1][1] = KF_FIXED12_ONE;
    matrix->m[1][2] = 0;
    matrix->m[2][0] = sine;
    matrix->m[2][1] = 0;
    matrix->m[2][2] = cosine;
}

ADDRESS(0x80014f70, 0x70)
void matrix_set_rotation_z(s16 angle, MATRIX *matrix)
{
    s32 sine = rsin(angle);
    s32 cosine = rcos(angle);

    matrix->m[0][0] = cosine;
    matrix->m[0][1] = -sine;
    matrix->m[0][2] = 0;
    matrix->m[1][0] = sine;
    matrix->m[1][1] = cosine;
    matrix->m[1][2] = 0;
    matrix->m[2][0] = 0;
    matrix->m[2][1] = 0;
    matrix->m[2][2] = KF_FIXED12_ONE;
}

/* MATRIX = Ry * Rx * Rz. */
ADDRESS(0x80014fe0, 0x68)
void matrix_set_rotation_yxz(const struct KfEulerAngles *angles, MATRIX *matrix)
{
    MATRIX temporary;

    matrix_set_rotation_z(angles->z, &temporary);
    matrix_set_rotation_x(angles->x, matrix);
    MulMatrix(matrix, &temporary);
    matrix_set_rotation_y(angles->y, &temporary);
    MulMatrix2(&temporary, matrix);
}

/* MATRIX = Rx * Rz * Ry. */
ADDRESS(0x80015048, 0x68)
void matrix_set_rotation_xzy(const struct KfEulerAngles *angles, MATRIX *matrix)
{
    MATRIX temporary;

    matrix_set_rotation_y(angles->y, &temporary);
    matrix_set_rotation_z(angles->z, matrix);
    MulMatrix(matrix, &temporary);
    matrix_set_rotation_x(angles->x, &temporary);
    MulMatrix2(&temporary, matrix);
}

ADDRESS(0x800150b0, 0xd0)
void pitch_yaw_to_forward_vector(const struct KfEulerAngles *angles, SVECTOR *direction)
{
    MATRIX pitch_matrix;
    MATRIX yaw_matrix;
    SVECTOR source;
    VECTOR result;

    setVector(&source, 0, 0, KF_FIXED12_ONE);
    matrix_set_rotation_x(-angles->x, &pitch_matrix);
    ApplyMatrix(&pitch_matrix, &source, &result);
    copyVector(&source, &result);
    matrix_set_rotation_y(angles->y, &yaw_matrix);
    ApplyMatrix(&yaw_matrix, &source, &result);
    copyVector(direction, &result);
}

ADDRESS(0x80015180, 0x44)
void vector_rotate_yxz(const struct KfEulerAngles *angles, SVECTOR *vector, VECTOR *result)
{
    MATRIX rotation;

    matrix_set_rotation_yxz(angles, &rotation);
    ApplyMatrix(&rotation, vector, result);
}

ADDRESS(0x800151c4, 0x40)
void vector2i_scale_shift11(s16 scale, struct KfVecXZi *vector)
{
    s32 x = scale * vector->x;
    s32 z = scale * vector->z;

    vector->x = x >> KF_FIXED11_BITS;
    vector->z = z >> KF_FIXED11_BITS;
}

ADDRESS(0x80015204, 0x5c)
void vector3s_scale_shift12(s16 scale, SVECTOR *vector)
{
    s32 x = vector->vx * scale;
    s32 y = vector->vy * scale;
    s32 z = vector->vz * scale;

    setVector(vector, x >> KF_FIXED12_BITS, y >> KF_FIXED12_BITS, z >> KF_FIXED12_BITS);
}

ADDRESS(0x80015260, 0x40)
void vector2i_scale_shift12(s16 scale, s32 *vector)
{
    s32 x = scale * vector[0];
    s32 y = scale * vector[1];

    vector[0] = x >> KF_FIXED12_BITS;
    vector[1] = y >> KF_FIXED12_BITS;
}

ADDRESS(0x800152a0, 0x5c)
void vector3s_scale_shift12_alt(s16 scale, s16 *vector)
{
    s32 x = vector[0] * scale;
    s32 y = vector[1] * scale;
    s32 z = vector[2] * scale;

    vector[0] = x >> KF_FIXED12_BITS;
    vector[1] = y >> KF_FIXED12_BITS;
    vector[2] = z >> KF_FIXED12_BITS;
}

ADDRESS(0x800152fc, 0x2c)
void vector3i_add_xz(VECTOR *destination, const struct KfVecXZi *delta)
{
    destination->vx += delta->x;
    destination->vz += delta->z;
}

ADDRESS(0x80015328, 0x3c)
KfBool angle_within_tolerance(int lhs, int rhs, s16 range)
{
    int delta = (lhs - rhs) & KF_ANGLE_WRAP_MASK;

    return delta <= range || KF_ANGLE_FULL_TURN - range <= delta;
}

ADDRESS(0x80015364, 0x10)
KfBool angle_mod_delta_le_half_turn(int lhs, int rhs)
{
    return ((lhs - rhs) & KF_ANGLE_WRAP_MASK) <= KF_ANGLE_HALF_TURN;
}

/* Signed turn from FROM to TO in -0x7ff..0x800. */
ADDRESS(0x80015374, 0x20)
s32 angle_shortest_delta(s32 from, s32 to)
{
    s32 difference = (to - from) & KF_ANGLE_WRAP_MASK;

    if (difference > KF_ANGLE_HALF_TURN) {
        return difference - KF_ANGLE_FULL_TURN;
    }
    return difference;
}

/* Psy-Q LIBGTE: catan(long) returns a 12-bit angle for a 12-bit fixed ratio.
 * The larger component is the divisor, keeping the ratio within one. */
ADDRESS(0x80015394, 0x150)
s32 vector_xz_to_angle(s32 x, s32 z)
{
    if (abs(x) <= abs(z)) {
        if (z > 0) {
            return -catan((x << KF_FIXED12_BITS) / z) & KF_ANGLE_WRAP_MASK;
        }
        if (z < 0) {
            return KF_ANGLE_HALF_TURN - catan((x << KF_FIXED12_BITS) / z);
        }
        return 0;
    }
    if (x >= 0) {
        return catan((z << KF_FIXED12_BITS) / x) + KF_ANGLE_THREE_QUARTER_TURN;
    }
    return catan((z << KF_FIXED12_BITS) / x) + KF_ANGLE_QUARTER_TURN;
}

ADDRESS(0x800154e4, 0x40)
s32 fixed_vector2_length(s32 x, s32 y)
{
    x >>= KF_LENGTH_SQUARE_DOWNSHIFT;
    y >>= KF_LENGTH_SQUARE_DOWNSHIFT;
    return SquareRoot0(x * x + y * y) << KF_LENGTH_SQUARE_DOWNSHIFT;
}

ADDRESS(0x80015524, 0x54)
s32 fixed_vector3_length(s32 x, s32 y, s32 z)
{
    x >>= KF_LENGTH_SQUARE_DOWNSHIFT;
    y >>= KF_LENGTH_SQUARE_DOWNSHIFT;
    z >>= KF_LENGTH_SQUARE_DOWNSHIFT;
    return SquareRoot0(x * x + y * y + z * z) << KF_LENGTH_SQUARE_DOWNSHIFT;
}

ADDRESS(0x80015578, 0x78)
void vector_displacement_to_pitch_yaw(s32 x, s32 y, s32 z, struct KfEulerAngles *angles)
{
    angles->y = vector_xz_to_angle(x, z);
    angles->x = -vector_xz_to_angle(y, fixed_vector2_length(x, z)) & KF_ANGLE_WRAP_MASK;
    angles->z = 0;
}

ADDRESS(0x800155f0, 0x30)
KfBool directed_intervals_overlap(s32 first, s32 first_width, s32 second, s32 second_width)
{
    if (second < first) {
        if (second < first - first_width) {
            return KF_FALSE;
        }
    } else {
        if (first < second - second_width) {
            return KF_FALSE;
        }
    }
    return KF_TRUE;
}

ADDRESS(0x80015620, 0xf4)
s32 vector_distance_to_point(
    const VECTOR *position, s32 point_x, s32 point_y, s32 point_z,
    s32 max_distance, s32 height, s32 point_height)
{
    s32 dx;
    s32 dz;
    s32 distance;

    dx = position->vx - point_x;
    if (dx < -max_distance || max_distance < dx) {
        return KF_DISTANCE_NONE;
    }
    dz = position->vz - point_z;
    if (dz < -max_distance || max_distance < dz) {
        return KF_DISTANCE_NONE;
    }
    if (point_y != KF_DISTANCE_IGNORE_HEIGHT) {
        if (position->vy < point_y) {
            if (position->vy < point_y - point_height) {
                return KF_DISTANCE_NONE;
            }
        } else if (point_y < position->vy - height) {
            goto reject;
        }
    }
    dx >>= KF_LENGTH_SQUARE_DOWNSHIFT;
    dz >>= KF_LENGTH_SQUARE_DOWNSHIFT;
    distance = SquareRoot0(dx * dx + dz * dz) << KF_LENGTH_SQUARE_DOWNSHIFT;
    if (max_distance < distance) {
        goto reject;
    }
    return distance;
reject:
    return KF_DISTANCE_NONE;
}

ADDRESS(0x80015714, 0x114)
s32 vector_distance_between_with_reach(const VECTOR *first, s32 reach, const VECTOR *second,
                  s32 offset, s32 height)
{
    s32 limit = reach + offset;
    s32 x = second->vx - first->vx;
    s32 z;
    s32 y;
    s32 distance;

    if (x < -limit || limit < x) {
        return KF_DISTANCE_OUTSIDE_REACH;
    }
    z = second->vz - first->vz;
    if (z < -limit || limit < z) {
        return KF_DISTANCE_OUTSIDE_REACH;
    }
    y = second->vy - first->vy - (height >> 1);
    if (y < -limit || limit < y) {
        return KF_DISTANCE_OUTSIDE_REACH;
    }
    x >>= KF_LENGTH_SQUARE_DOWNSHIFT;
    z >>= KF_LENGTH_SQUARE_DOWNSHIFT;
    y >>= KF_LENGTH_SQUARE_DOWNSHIFT;
    distance = SquareRoot0(x * x + z * z + y * y)
             << KF_LENGTH_SQUARE_DOWNSHIFT;
    if (limit >= distance) {
        if (distance > offset) {
            return distance - offset;
        }
        return 0;
    }
    return KF_DISTANCE_OUTSIDE_REACH;
}

ADDRESS(0x80015828, 0x4c)
s32 random_triangular_scaled(s32 amplitude)
{
    s32 first = rand();
    s32 second = rand();

    return ((first + second) * amplitude) >> 15;
}

ADDRESS(0x80015874, 0x54)
s32 random_centered_triangular_scaled(s32 amplitude)
{
    s32 first = rand();
    s32 second = rand();

    return ((first + second - RANDOM_TRIANGULAR_CENTER) * amplitude) >> 15;
}

ADDRESS(0x800158c8, 0x20)
s32 fixed_lerp_q12(s32 start, s32 end, s32 fraction)
{
    return (((end - start) * fraction) >> KF_FIXED12_BITS) + start;
}

ADDRESS(0x800158e8, 0x48)
s32 angle_lerp_shortest_q12(s32 start, s32 end, s32 fraction)
{
    s32 delta = ((s16)end - (s16)start) & KF_ANGLE_WRAP_MASK;

    if (delta > KF_ANGLE_HALF_TURN) {
        delta -= KF_ANGLE_FULL_TURN;
    }
    return (start + ((delta * fraction) >> KF_FIXED12_BITS)) & KF_ANGLE_WRAP_MASK;
}

ADDRESS(0x80015930, 0x64)
void fixed_lerp_nine_halfwords_q12(const s16 *start, const s16 *end, s16 *output, s16 fraction)
{
    const s16 *source = start;
    const s16 *target = end;
    s16 *destination = output;
    s32 index;

    for (index = LERP_HALFWORD_COUNT - 1; index != -1; index--) {
        s16 value = *source++;
        s16 next = *target++;
        s32 delta = next - value;

        *destination++ = value + ((delta * fraction) >> KF_FIXED12_BITS);
    }
}

ADDRESS(0x80015994, 0x2b0)
KF_ENUM_PARAM(KfTrajectoryResult, s32) trajectory_solve_time_angle(KF_ENUM_PARAM(KfTrajectoryMode, s32) mode, s32 horizontal_distance,
    s32 vertical_distance, s32 speed, s32 amplitude,
    s32 *travel_time, s32 *angle)
{
    s32 amplitude_squared = amplitude * amplitude;
    s32 speed_squared = speed * speed;
    s32 acceleration = amplitude_squared >> 6;
    s32 discriminant = acceleration * acceleration
        - (((speed_squared * horizontal_distance) >> 10)
           * (horizontal_distance >> 2))
        - (((speed * acceleration) >> 3) * (vertical_distance >> 2));
    s32 midpoint;
    s32 longer_time;
    s32 shorter_time;
    s32 chosen_time;
    s32 result_time;
    s32 horizontal_component;
    s32 vertical_component;

    if (discriminant < 0) {
        return KF_TRAJECTORY_UNREACHABLE;
    }
    discriminant = SquareRoot0(discriminant << 4) << 2;
    midpoint = (amplitude_squared >> 2) - ((vertical_distance * speed) >> 2);
    longer_time = ((midpoint + discriminant) << 1) / speed_squared;
    shorter_time = ((midpoint - discriminant) << 1) / speed_squared;
    if (longer_time <= 0 && shorter_time <= 0) {
        return KF_TRAJECTORY_UNREACHABLE;
    }
    if (mode == KF_TRAJECTORY_SHORTER_TIME) {
        chosen_time = shorter_time;
        if (longer_time > 0 && longer_time <= shorter_time) {
            chosen_time = longer_time;
        }
    } else {
        chosen_time = shorter_time;
        if (shorter_time <= longer_time) {
            chosen_time = longer_time;
        }
    }
    result_time = SquareRoot0(chosen_time << 14);
    horizontal_component = ((horizontal_distance << 14) / amplitude) / result_time;
    vertical_component = (((vertical_distance + ((speed * chosen_time) << 1)) << 12)
        / (amplitude >> 1)) / (result_time >> 1);
    *travel_time = result_time;
    *angle = vector_xz_to_angle(vertical_component, horizontal_component);
    return KF_TRAJECTORY_SOLVED;
}

ADDRESS(0x80015c44, 0x118)
KF_ENUM_PARAM(KfTrajectoryResult, s32) trajectory_solve_motion_between_points(
    KF_ENUM_PARAM(KfTrajectoryMode, s32) mode,
    s32 source_x, s32 source_y, s32 source_z, s32 target_x, s32 target_y, s32 target_z,
    s32 speed, s32 amplitude, s16 *result, s16 *motion_x, s16 *motion_z)
{
    s32 travel_time;
    s32 angle;
    KF_ENUM_PARAM(KfTrajectoryResult, s32) status;
    s32 distance = fixed_vector2_length(target_x - source_x,
                                       target_z - source_z);

    status = trajectory_solve_time_angle(mode, distance, source_y - target_y,
                           speed, amplitude, &travel_time, &angle);
    if (status == KF_TRAJECTORY_SOLVED) {
        *motion_x = (amplitude * rcos(angle)) >> KF_FIXED12_BITS;
        *motion_z = (amplitude * -rsin(angle)) >> KF_FIXED12_BITS;
        *result = travel_time;
    }
    return status;
}

ADDRESS(0x80015d5c, 0x70)
void vector_add_scaled_delta(const VECTOR *origin, const SVECTOR *delta, s32 scale,
    VECTOR *output)
{
    setVector(output, delta->vx * scale + origin->vx,
              delta->vy * scale + origin->vy,
              delta->vz * scale + origin->vz);
}
