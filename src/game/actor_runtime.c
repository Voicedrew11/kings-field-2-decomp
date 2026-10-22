#include <kf/lib/address.h>
#include <kf/lib/null.h>
#include <kf/game/actor.h>
#include <kf/game/callback.h>
#include <kf/game/collision_cache.h>
#include <kf/game/map_cell.h>
#include <kf/game/player.h>
#include <psyq/libc.h>
#include <kf/lib/math.h>
#include <kf/lib/types.h>
#include <kf/game/animation.h>
#include <kf/game/audio.h>
#include <kf/game/effect.h>
#include <kf/game/map_object.h>



DATA(0x8016c544, 0x93cc, ".bss")
KfActorStateGame actor_state;

ADDRESS(0x80039a00, 0x3c)
KfActor *actor_pool_find_free(void)
{
    KfActor *actor = &actor_state.actors[KF_ACTOR_DYNAMIC_START];
    KfActor *found;
    s32 count = KF_ACTOR_DYNAMIC_COUNT - 1;

    do {
        if (actor->slot_state == KF_ACTOR_SLOT_FREE) {
            found = actor;
            goto done;
        }
        actor++;
    } while (--count != -1);
    found = NULL;
done:
    return found;
}

enum {
    ACTOR_HOME_CELL_SHIFT = 11,
    ACTOR_LIGHTING_BLEND_HALF = KF_FIXED12_ONE / 2
};

ADDRESS(0x80039a3c, 0xc0)
void actor_set_home_position(KfActor *actor)
{
    actor->position.vx = (actor->home_cell_x << ACTOR_HOME_CELL_SHIFT)
                       + actor->word_24.home_local_x;
    actor->position.vz = (actor->home_cell_z << ACTOR_HOME_CELL_SHIFT)
                       + actor->word_22.home_local_z;
    actor->position.vy = collision_sample_map_layer_height(actor->home_map_layer,
                                        actor->position.vx,
                                        actor->position.vz,
                                        actor->collision_radius,
                                        actor->collision_height);
    if (actor->position.vy >= 0) {
        actor->position.vy = 0;
    }
    if ((actor->flags & KF_ACTOR_FLAG_USE_MAP_LAYER_FLOOR) != KF_ACTOR_FLAGS_NONE) {
        actor->position.vy = KF_COLLISION_CACHE_HEIGHT;
    }
    if ((actor->flags & KF_ACTOR_FLAG_LINKED) == KF_ACTOR_FLAGS_NONE) {
        actor->position.vy += actor->vertical_anchor_offset;
    }
}

ADDRESS(0x80039afc, 0x74)
void actor_copy_group_defaults(KfActor *actor)
{
    const KfTargetGroup *group = &actor_state.target_groups[actor->group_index];
    u16 value;

    actor->definition_id = group->definition_id;
    actor->health = group->word_1a.initial_health;
    actor->collision_radius = group->collision_radius;
    actor->collision_height = group->collision_height;
    actor->flags = group->initial_actor_flags;
    value = group->initial_model_scale_q12;
    actor->model_scale_z = value;
    actor->model_scale_y.value = value;
    actor->model_scale_x = value;
}

ADDRESS(0x80039b70, 0xc4)
void actor_initialize_from_group(KfActor *actor)
{
    actor_copy_group_defaults(actor);
    actor->lifecycle = KF_ACTOR_LIFECYCLE_ACTIVE;
    actor->animation_id = KF_ANIMATION_CLIP_FIRST;
    actor->animation_phase = 0;
    actor->unknown_11 = 0;
    actor->vertical_motion_state = KF_ACTOR_VERTICAL_MOTION_NONE;
    actor->target_type = KF_ACTOR_TARGET_0;
    actor->target_action_state = KF_ACTOR_TARGET_ACTION_UNSELECTED;
    actor->target = NULL;
    if ((actor->placement_flags & KF_ACTOR_PLACEMENT_KEEP_INITIAL_YAW) == 0) {
        actor->rotation.y = rand() >> KF_RANDOM_ANGLE_SHIFT;
    }
    actor->motion.vector.vz = 0;
    actor->motion.vector.vy = 0;
    actor->motion.vector.vx = 0;
    actor->turn_rate = 0;
    actor->lighting_override = KF_LIGHTING_ACTOR_DEFAULT;
    actor->lighting_blend = ACTOR_LIGHTING_BLEND_HALF;
    if ((actor->flags & KF_ACTOR_FLAG_BLENDED_MODEL) != KF_ACTOR_FLAGS_NONE) {
        actor->render_mode = KF_RENDER_QUEUE_BLEND_ADD;
    } else {
        actor->render_mode = KF_RENDER_QUEUE_TEXTURED;
    }
    map_cell_add_layer_occupancy(actor->position.vx, actor->position.vz, actor->collision_radius, 1);
}

ADDRESS(0x80039c34, 0x24)
void actor_set_lifecycle_and_home_position(KfActor *actor)
{
    actor->lifecycle = KF_ACTOR_LIFECYCLE_DISABLED;
    actor_set_home_position(actor);
}

ADDRESS(0x80039c58, 0xd0)
void actor_disable_type3_transition_actors(void)
{
    KfActor *actor = actor_state.actors;
    s32 remaining = KF_ACTOR_CAPACITY - 1;

    do {
        if (actor->slot_state == KF_ACTOR_SLOT_PERSISTENT &&
            actor->lifecycle == KF_ACTOR_LIFECYCLE_ACTIVE &&
            actor->target_type == KF_ACTOR_TARGET_3 &&
            actor->target_action_state == KF_ACTOR_TARGET_ACTION_RETARGET_BLOCKED &&
            (actor->state_70.signed_state != 0 ||
             actor->animation_phase > KF_ACTOR_ANIMATION_PHASE_PERIOD / 2)) {
            ((void (*)(KfActor *))resource_state.active_table[19])(actor);
            actor_set_lifecycle_and_home_position(actor);
        }
        actor++;
    } while (--remaining != -1);
}

ADDRESS(0x80039d28, 0x58)
void actor_prepare_and_initialize(KfActor *actor)
{
    actor->rotation.z = 0;
    actor->rotation.x = 0;
    actor->current_map_layer = actor->home_map_layer;
    actor->rotation.y = actor->word_20.home_yaw;
    actor_set_home_position(actor);
    actor_initialize_from_group(actor);
    if (actor->slot_state == KF_ACTOR_SLOT_HOMEBOUND) {
        actor->current_map_layer = KF_MAP_LAYER_NONE;
    }
}

ADDRESS(0x80039d80, 0x38)
void actor_prepare_and_initialize_by_index(u16 actor_index)
{
    actor_prepare_and_initialize(&actor_state.actors[actor_index]);
}

ADDRESS(0x80039db8, 0x50)
void actor_pool_clear(void)
{
    KfActor *actor;
    u16 index;

    actor_state.actor_update_frame_count = 0;
    actor = actor_state.actors;
    for (index = 0; index < KF_ACTOR_CAPACITY; index++, actor++) {
        actor->slot_state = KF_ACTOR_SLOT_FREE;
        actor->lifecycle = KF_ACTOR_LIFECYCLE_DORMANT;
        actor->animation_cache = NULL;
    }
    actor_state.actor_overlap_exclusion_flags = KF_ACTOR_FLAGS_NONE;
}


typedef s32 (*KfCandidateScoreCallback)(KfTargetCandidate *target,
                                         s32 player_distance);

RODATA(0x80011cd8, 0x7c4)

ADDRESS(0x80039e08, 0x38)
void actor_set_target(KfActor *actor, KfTargetCandidate *target)
{
    actor->previous_target_type = actor->target_type;
    if (target != NULL) {
        KfActorTargetType target_type;

        actor->target = target;
        target_type = target->type;
        actor->target_action_state = KF_ACTOR_TARGET_ACTION_ENTRY;
        actor->target_type = target_type;
    } else {
        actor->target = NULL;
        actor->target_type = KF_ACTOR_TARGET_NONE;
        actor->target_action_state = KF_ACTOR_TARGET_ACTION_UNSELECTED;
    }
}

ADDRESS(0x80039e40, 0x4c0)
s32 actor_score_target_candidate(KfTargetCandidate *target, s32 player_distance)
{
    KfActor *actor = actor_state.current;
    /* Retail leaves several rejection paths returning this unassigned. */
    s32 score;
    s32 angle;

    if (target->type == KF_ACTOR_TARGET_NONE) {
        return 0;
    }

    switch (target->type) {
    case KF_ACTOR_TARGET_5:
    case KF_ACTOR_TARGET_13:
        if (target == actor->target) {
            score = 0;
            if (target->word_12.value >= player_distance) {
                score = random_triangular_scaled(target->word_02.target_selection.continuing_score_scale);
            }
            break;
        }
        score = 0;
        if (target->word_10.value >= player_distance) {
            score = random_triangular_scaled(target->word_02.target_selection.initial_score_scale);
        }
        switch (actor->target_type) {
        case KF_ACTOR_TARGET_4:
        case KF_ACTOR_TARGET_18:
        case KF_ACTOR_TARGET_23:
        case KF_ACTOR_TARGET_24:
        case KF_ACTOR_TARGET_132:
            score *= 2;
            break;
        default:
            break;
        }
        break;

    case KF_ACTOR_TARGET_9:
        if (target->word_0c.value < player_distance) {
            score = 0;
            break;
        }
        if (player_state.camera_position.vy - actor->position.vy > -1024 &&
            player_state.camera_position.vy - actor->position.vy < 1024 &&
            rand() >= 4096) {
            break;
        }
        angle = vector_xz_to_angle(player_state.camera_position.vx - actor->position.vx,
                                   player_state.camera_position.vz - actor->position.vz);
        if (angle_within_tolerance(actor->rotation.y, angle, 0x140)) {
            goto score_target;
        }
        break;

    case KF_ACTOR_TARGET_4:
    case KF_ACTOR_TARGET_18:
    case KF_ACTOR_TARGET_23:
    case KF_ACTOR_TARGET_24:
    case KF_ACTOR_TARGET_132:
        if ((actor->flags & KF_ACTOR_FLAG_BLOCK_PLAYER_TARGETING) != KF_ACTOR_FLAGS_NONE ||
            target->word_1a.value < player_distance) {
            score = 0;
            break;
        }
        if (!directed_intervals_overlap(actor->position.vy, actor->collision_height,
                            player_state.camera_position.vy + 200, 0x834)) {
            score = 0;
            break;
        }
        angle = vector_xz_to_angle(player_state.camera_position.vx - actor->position.vx,
                                   player_state.camera_position.vz - actor->position.vz);
        if (!angle_within_tolerance(actor->rotation.y, angle,
                                    target->word_10.bytes.fallback_offset << 4)) {
            score = 0;
            break;
        }
        if (target == actor->target) {
            score = random_triangular_scaled(target->word_02.target_selection.continuing_score_scale);
            break;
        }
        score = target->word_02.target_selection.initial_score_scale;
        {
            s32 near_distance = target->word_0e.bytes.low << 6;
            s32 middle_distance = (near_distance + target->word_1a.value) >> 1;
            if (player_distance >= middle_distance) {
                score -= score / 3;
            } else if (player_distance >= near_distance) {
                score -= score / 6;
            }
        }
        score = random_triangular_scaled(score);
        break;

    case KF_ACTOR_TARGET_25:
        score = 0;
        if ((actor->flags & KF_ACTOR_FLAG_BLOCK_PLAYER_TARGETING) != KF_ACTOR_FLAGS_NONE) {
            break;
        }
        if (target->word_16.value < player_distance ||
            target->word_14.value > player_distance) {
            break;
        }
        angle = vector_xz_to_angle(player_state.camera_position.vx - actor->position.vx,
                                   player_state.camera_position.vz - actor->position.vz);
        if (angle_within_tolerance(actor->rotation.y, angle,
                                   target->word_0c.bytes.high << 4)) {
            goto score_target;
        }
        goto zero_score;

    case KF_ACTOR_TARGET_11:
        if (target->word_1a.value < player_distance) {
            score = 0;
            break;
        }
        angle = vector_xz_to_angle(player_state.camera_position.vx - actor->position.vx,
                                   player_state.camera_position.vz - actor->position.vz);
        if (angle_within_tolerance(actor->rotation.y, angle, 0x140)) {
            goto score_target;
        }
        goto zero_score;

    case KF_ACTOR_TARGET_19:
    case KF_ACTOR_TARGET_20:
        score = 0;
        if (target->word_0e.value < player_distance) {
            break;
        }
        if (player_state.weapon_attack_phase == -1) {
            break;
        }
        angle = vector_xz_to_angle(player_state.camera_position.vx - actor->position.vx,
                                   player_state.camera_position.vz - actor->position.vz);
        if (angle_within_tolerance(actor->rotation.y, angle,
                                   target->word_0c.bytes.low << 5)) {
            goto score_target;
        }
        goto zero_score;

    case KF_ACTOR_TARGET_EVENT_STREAM:
        score = -1;
        break;
    case KF_ACTOR_TARGET_27:
        if (player_distance >= target->word_0c.value) {
            goto score_target;
        }
        /* Fall through to the zero-score cases. */
    case KF_ACTOR_TARGET_2:
    case KF_ACTOR_TARGET_3:
    case KF_ACTOR_TARGET_22:
zero_score:
        score = 0;
        break;
    default:
        if (target->type < KF_ACTOR_TARGET_MAP_CALLBACK_FIRST ||
            ((KfCandidateScoreCallback)resource_state.active_table[16])(
                target, player_distance)) {
score_target:
            if (target == actor->target) {
                score = random_triangular_scaled(
                    target->word_02.target_selection.continuing_score_scale);
            } else {
                score = random_triangular_scaled(
                    target->word_02.target_selection.initial_score_scale);
            }
        }
        break;
    }
    return score;
}

ADDRESS(0x8003a300, 0xfc)
void actor_select_best_target(s32 player_distance)
{
    KfActor *actor = actor_state.current;
    KfTargetGroup *group = actor_state.active_group;
    KfTargetCandidate *target;
    KfTargetCandidate *best_target;
    const KfTargetReference *slot;
    s32 best_score;
    s32 score;
    s32 remaining;

    if (actor->target_action_state == KF_ACTOR_TARGET_ACTION_RETARGET_BLOCKED ||
        actor->target_action_state == KF_ACTOR_TARGET_ACTION_ENTRY) {
        return;
    }

    slot = group->targets;
    best_score = -2;
    best_target = NULL;
    remaining = 15;
    do {
        target = (slot++)->pointer;
        if (target == NULL) {
            break;
        }
        score = actor_score_target_candidate(target, player_distance);
        if (score > best_score) {
            best_score = score;
            best_target = target;
        }
    } while (--remaining != -1);

    if (best_target != NULL &&
        (best_target != actor->target ||
         actor->target_action_state == KF_ACTOR_TARGET_ACTION_UNSELECTED)) {
        actor_set_target(actor, best_target);
    }
}

ADDRESS(0x8003a3fc, 0x4c)
void actor_select_target_for_player_distance(void)
{
    KfActor *actor = actor_state.current;
    s32 distance = fixed_vector2_length(
        actor->position.vx - player_state.camera_position.vx,
        actor->position.vz - player_state.camera_position.vz);

    actor_select_best_target(distance);
}

ADDRESS(0x8003a448, 0x48)
KfTargetCandidate *actor_find_target_of_type(const KfTargetGroup *group, KfActorTargetType type)
{
    KfTargetCandidate *target;
    const KfTargetReference *slot = group->targets;
    s32 remaining = 15;

    do {
        target = (slot++)->pointer;
        if (target == NULL) {
            break;
        }
        if (target->type == type) {
            return target;
        }
    } while (--remaining != -1);
    return NULL;
}

ADDRESS(0x8003a490, 0x50)
void actor_select_target_type_in_own_group(KfActor *actor, KfActorTargetType type)
{
    KfTargetGroup *group = &actor_state.target_groups[actor->group_index];
    KfTargetCandidate *target = actor_find_target_of_type(group, type);

    actor_set_target(actor, target);
}

ADDRESS(0x8003a4e0, 0x30)
void actor_reset_target_and_reselect(void)
{
    KfActor *actor = actor_state.current;

    actor->target = NULL;
    actor->target_action_state = KF_ACTOR_TARGET_ACTION_UNSELECTED;
    actor_select_target_for_player_distance();
}

ADDRESS(0x8003a510, 0x2c)
void actor_set_animation(KfAnimationClip animation_id)
{
    if (animation_id != KF_ANIMATION_CLIP_NONE) {
        KfActor *actor = actor_state.current;

        actor->animation_id = animation_id;
        actor->animation_phase = 0;
    }
}

ADDRESS(0x8003a53c, 0x38)
void actor_set_animation_if_changed(KfAnimationClip animation_id)
{
    KfActor *actor = actor_state.current;

    if (animation_id != KF_ANIMATION_CLIP_NONE &&
        actor->animation_id != animation_id) {
        actor->animation_id = animation_id;
        actor->animation_phase = 0;
    }
}

/* Puts an actor back to sleep at its home position. */
#define ACTOR_RETURN_HOME(actor) do { \
    (actor)->lifecycle = KF_ACTOR_LIFECYCLE_DORMANT; \
    actor_set_home_position(actor); \
} while (0)

ADDRESS(0x8003a574, 0x31c)
void actor_update_lifecycle_for_player_range(void)
{
    KfActor *actor = actor_state.current;
    KfTargetGroup *group = actor_state.active_group;
    KfActorSlotState slot_state = actor->slot_state;
    s32 distance;

    switch (actor->lifecycle) {
    case KF_ACTOR_LIFECYCLE_DORMANT:
        distance = vector_distance_to_point(
            &actor->position, player_state.camera_position.vx,
            KF_DISTANCE_IGNORE_HEIGHT, player_state.camera_position.vz,
            (group->activation_range_cells + 1) << KF_FIXED11_BITS, 0, 0);
        if (distance == KF_DISTANCE_NONE) {
            return;
        }

        if (slot_state == KF_ACTOR_SLOT_HOMEBOUND || slot_state == KF_ACTOR_SLOT_LINKED_COMPANION) {
            if (actor_state.other_actor->lifecycle != KF_ACTOR_LIFECYCLE_ACTIVE) {
                return;
            }
            actor_prepare_and_initialize(actor_state.current);
            actor_select_best_target(distance);
            return;
        }

        if (slot_state == KF_ACTOR_SLOT_RESPAWNING) {
            u8 chance = actor->spawn_chance;
            if (chance != 0xff && chance < (rand() >> 4)) {
                return;
            }
        } else {
            if (distance < (group->activation_range_cells << KF_FIXED11_BITS) &&
                player_state.force_actor_lifecycle_refresh == 0) {
                goto set_dormant;
            }
            if (slot_state != KF_ACTOR_SLOT_PERSISTENT) {
                if (slot_state != KF_ACTOR_SLOT_0) {
                    goto set_dormant;
                }
                if (actor->spawn_chance == 0 ||
                    actor->spawn_chance < (rand() >> 7)) {
                    goto set_dormant;
                }
            }
        }

        if (actor_find_overlap_excluding_target_type3(actor->position.vx, actor->position.vy,
                          actor->position.vz, group->collision_radius,
                          group->collision_height) != KF_ACTOR_INDEX_NONE) {
            if (slot_state != KF_ACTOR_SLOT_RESPAWNING) {
                actor->lifecycle = KF_ACTOR_LIFECYCLE_WAIT_FOR_RANGE_EXIT;
            }
            return;
        }

        actor_prepare_and_initialize(actor_state.current);
        {
            KfTargetCandidate *target = actor_find_target_of_type(group, KF_ACTOR_TARGET_21);
            if (target == NULL) {
                target = actor_find_target_of_type(group, KF_ACTOR_TARGET_26);
            }
            if (target != NULL) {
                actor_set_target(actor, target);
                return;
            }
        }
        actor_select_best_target(distance);
        return;

    set_dormant:
        if (slot_state != KF_ACTOR_SLOT_RESPAWNING) {
            actor->lifecycle = KF_ACTOR_LIFECYCLE_WAIT_FOR_RANGE_EXIT;
        }
        return;

    case KF_ACTOR_LIFECYCLE_ACTIVE:
        distance = vector_distance_to_point(
            &actor->position, player_state.camera_position.vx,
            KF_DISTANCE_IGNORE_HEIGHT, player_state.camera_position.vz,
            group->deactivation_range_cells << KF_FIXED11_BITS, 0, 0);
        if (distance != KF_DISTANCE_NONE) {
            return;
        }
        map_cell_add_layer_occupancy(actor->position.vx, actor->position.vz,
                       actor->collision_radius, -1);
        ACTOR_RETURN_HOME(actor);
        return;

    case KF_ACTOR_LIFECYCLE_WAIT_FOR_RANGE_EXIT:
        if (slot_state == KF_ACTOR_SLOT_HOMEBOUND || slot_state == KF_ACTOR_SLOT_LINKED_COMPANION) {
            if (actor_state.other_actor->lifecycle == KF_ACTOR_LIFECYCLE_ACTIVE) {
                return;
            }
        } else {
            distance = vector_distance_to_point(
                &actor->position, player_state.camera_position.vx,
                KF_DISTANCE_IGNORE_HEIGHT, player_state.camera_position.vz,
                group->deactivation_range_cells << KF_FIXED11_BITS, 0, 0);
            if (distance != KF_DISTANCE_NONE) {
                return;
            }
        }
        ACTOR_RETURN_HOME(actor);
        return;
    default:
        break;
    }
}

ADDRESS(0x8003a890, 0xbc)
void actor_retarget_or_disable_group_members(s32 group_index)
{
    KfActor *actor = actor_state.actors;
    s16 remaining = KF_ACTOR_CAPACITY - 1;

    do {
        if (actor->slot_state != KF_ACTOR_SLOT_FREE &&
            actor->group_index == (group_index & 0xffff)) {
            if (actor->lifecycle == KF_ACTOR_LIFECYCLE_ACTIVE) {
                actor_select_target_type_in_own_group(actor, KF_ACTOR_TARGET_3);
            } else {
                actor_set_lifecycle_and_home_position(actor);
            }
        }
        actor++;
    } while (--remaining != -1);
}

ADDRESS(0x8003a94c, 0x80)
s32 actor_magic_component_curve(s32 base, s32 amount, s32 divisor)
{
    amount <<= 4;
    divisor <<= 4;
    base <<= 4;
    if (amount == 0) {
        return 0;
    }
    amount += base;
    base = amount - divisor;
    if (base < 0) {
        base = 0;
    }
    if (divisor == 0) {
        divisor = 16;
    }
    return base + (amount * amount) / (divisor << 1);
}

typedef void (*KfMagicRecipientCallback)(KfActor *actor, s32 amount,
    u16 magic_06, u16 magic_08, u16 magic_0a, u16 magic_0c,
    u16 magic_0e, u16 magic_10, u16 magic_12, u16 magic_14);

ADDRESS(0x8003a9cc, 0x684)
void actor_apply_magic_to_actor(s32 actor_index, u16 power, u16 magic_06,
    u16 magic_08, u16 magic_0a, u16 magic_0c, u16 magic_0e,
    u16 magic_10, u16 magic_12, u16 magic_14, u16 amount,
    KF_ENUM_PARAM(KfActorDamageFlags, s32) effect_flags, const VECTOR *position)
{
    KfActor *actor = &actor_state.actors[(u16)actor_index];
    KfTargetGroup *group;
    KfTargetCandidate *candidate;
    KfActor *linked;
    s32 damage;
    s32 remaining;
    KF_ENUM_PROMOTED(KfActorDamageFlags) mode;
    KF_ENUM_PROMOTED(KfActorDamageFlags) kind;
    s32 remaining_slots;
    KfTargetReference *target_slot;
    s32 motion_divisor;

    if (actor->slot_state == KF_ACTOR_SLOT_HOMEBOUND) {
        actor = &actor_state.actors[actor->word_22.linked_actor_slot];
    }
    group = &actor_state.target_groups[actor->group_index];
    if (actor->target_type == KF_ACTOR_TARGET_3 && actor->animation_phase >= 1548) {
        return;
    }
    if (actor->target_type == KF_ACTOR_TARGET_21) {
        if (actor->state_70.signed_state != 0) {
            return;
        }
        actor->state_70.signed_state = 1;
        return;
    }
    if (actor->target_type == KF_ACTOR_TARGET_26) {
        return;
    }

    kind = effect_flags & KF_ACTOR_DAMAGE_SOURCE_MASK;
    mode = effect_flags & KF_ACTOR_DAMAGE_MODE_MASK;
    if (kind == KF_ACTOR_DAMAGE_FROM_HAZARD && (actor->flags & KF_ACTOR_FLAG_IGNORE_HAZARD_DAMAGE) != KF_ACTOR_FLAGS_NONE) {
        return;
    }

    damage = actor_magic_component_curve(power, magic_06, group->magic_component_divisors[0]);
    damage += actor_magic_component_curve(power, magic_08, group->magic_component_divisors[1]);
    damage += actor_magic_component_curve(power, magic_0a, group->magic_component_divisors[2]);
    damage += actor_magic_component_curve(power, magic_0c, group->magic_component_divisors[3]);
    damage += actor_magic_component_curve(power, magic_0e, group->magic_component_divisors[4]);
    damage += actor_magic_component_curve(power, magic_10, group->magic_component_divisors[5]);
    damage += actor_magic_component_curve(power, magic_12, group->magic_component_divisors[6]);
    damage += actor_magic_component_curve(power, magic_14, group->magic_component_divisors[7]);
    if (damage > 0x68db7) {
        damage = 0x68db7;
    }
    damage = ((damage * amount) / 5000 + 128) >> 4;
    /* Slot 18's target is unresolved; its O32 arguments are observed. */
    ((KfMagicRecipientCallback)resource_state.active_table[18])(
        actor, damage, magic_06, magic_08, magic_0a, magic_0c,
        magic_0e, magic_10, magic_12, magic_14);
    if (damage == 0) {
        return;
    }

    if (actor->health != 0 && kind == KF_ACTOR_DAMAGE_FROM_PLAYER) {
        if (mode == KF_ACTOR_DAMAGE_MAGIC) {
            player_increment_magic_training();
        } else if (mode == KF_ACTOR_DAMAGE_PHYSICAL && amount >= 2500) {
            player_increment_physical_power_training();
        }
    }
    if (mode == KF_ACTOR_DAMAGE_PHYSICAL) {
        if (actor->target_type == KF_ACTOR_TARGET_19 && actor->state_70.signed_state == 0x10) {
            goto update_motion;
        }
    } else if (mode == KF_ACTOR_DAMAGE_MAGIC) {
        if ((actor->flags & KF_ACTOR_FLAG_IGNORE_MAGIC_REACTION) != KF_ACTOR_FLAGS_NONE) {
            goto update_motion;
        }
        candidate = actor_find_target_of_type(group, KF_ACTOR_TARGET_22);
        if (candidate != NULL) {
            s32 angle = vector_xz_to_angle(
                actor->position.vx - position->vx,
                actor->position.vz - position->vz);
            if (angle_within_tolerance(actor->rotation.y, angle + KF_ANGLE_HALF_TURN,
                                       0x200)) {
                actor_set_target(actor, candidate);
                goto update_motion;
            }
        }
    }

    remaining = actor->health - damage;
    if (remaining <= 0) {
        if (actor->health != 0 && kind == KF_ACTOR_DAMAGE_FROM_PLAYER) {
            player_add_experience(group->experience_reward);
        }
        actor_select_target_type_in_own_group(actor, KF_ACTOR_TARGET_3);
        remaining = 0;
    } else {
        target_slot = group->targets;
        remaining_slots = 15;
        do {
            KfTargetCandidate *reaction = (target_slot++)->pointer;
            if (reaction == NULL) {
                break;
            }
            if (reaction->type == KF_ACTOR_TARGET_2 && reaction->word_0c.value <= damage) {
                s32 chance = reaction->word_02.damage_reaction.reaction_chance;
                if (chance == 0xff || (rand() >> 7) < chance) {
                    actor_set_target(actor, reaction);
                    break;
                }
            }
        } while (--remaining_slots != -1);
    }
    actor->health = remaining;

update_motion:
    if ((actor->flags & KF_ACTOR_FLAG_LINKED) != KF_ACTOR_FLAGS_NONE) {
        KfActorStateGame *state = &actor_state;
        KfTargetGroup *groups = state->target_groups;
        linked = &state->actors[actor->word_22.linked_actor_slot];
        motion_divisor = groups[linked->group_index].knockback_divisor;
    } else {
        motion_divisor = group->knockback_divisor;
    }
    if (position != NULL && motion_divisor < 0xf0) {
        struct KfEulerAngles angles;
        vector_displacement_to_pitch_yaw(actor->position.vx - position->vx,
                      actor->position.vy - (actor->collision_height >> 1) - position->vy,
                      actor->position.vz - position->vz, &angles);
        pitch_yaw_to_forward_vector(&angles, &actor->motion.vector);
        damage = SquareRoot0(SquareRoot0(damage << 11));
        damage = (((damage << 10) / motion_divisor) << 5) / motion_divisor;
        if (damage > 512) {
            damage = 512;
        }
        vector3s_scale_shift12(damage, &actor->motion.vector);
        actor->motion.vector.vx >>= 3;
        actor->motion.vector.vz >>= 3;
        actor->motion.vector.vy >>= 6;
        if ((actor->flags & KF_ACTOR_FLAG_LINKED) != KF_ACTOR_FLAGS_NONE) {
            linked->motion.vector.vx = actor->motion.vector.vx;
            linked->motion.vector.vy = actor->motion.vector.vy;
            linked->motion.vector.vz = actor->motion.vector.vz;
            if (linked->target_type != KF_ACTOR_TARGET_3) {
                actor_select_target_type_in_own_group(linked, KF_ACTOR_TARGET_2);
            }
        }
    }
    actor->vertical_motion_state = KF_ACTOR_VERTICAL_MOTION_VELOCITY;
}

enum {
    KF_AREA_MAGIC_OMIT_DAMAGE_ORIGIN = 0x8000,
    KF_AREA_MAGIC_AMOUNT_MASK = 0x7fff
};

/* Strips the flag bits from a packed area-magic amount. The do-while
   contour weights the packed word's use, so it outranks `falloff`. */
#define AREA_MAGIC_AMOUNT(amount, amount_and_flags) do { \
    (amount) = (u32)(amount_and_flags) & KF_AREA_MAGIC_AMOUNT_MASK; \
} while (0)

ADDRESS(0x8003b050, 0x2fc)
void actor_apply_area_magic(VECTOR *position, s32 minimum_distance, s32 reach,
                   s32 mode, u16 falloff, u16 power, u16 magic_06,
                   u16 magic_08, u16 magic_0a, u16 magic_0c, u16 magic_0e,
                   u16 magic_10, u16 magic_12, u16 magic_14,
                   s32 amount_and_flags, KF_ENUM_PARAM(KfActorDamageFlags, u16) effect_flags)
{
    KfActor *actor;
    s16 index;
    const VECTOR *damage_position = position;
    u32 amount;

    if ((amount_and_flags & KF_AREA_MAGIC_OMIT_DAMAGE_ORIGIN) != 0) {
        damage_position = NULL;
    }
    AREA_MAGIC_AMOUNT(amount, amount_and_flags);
    actor = actor_state.actors;
    for (index = 0; index < KF_ACTOR_CAPACITY; index++, actor++) {
        s32 distance;
        u32 scaled_amount;

        if (actor->lifecycle != KF_ACTOR_LIFECYCLE_ACTIVE || actor == actor_state.current) {
            continue;
        }
        if (mode == KF_RADIAL_MODE_REACH) {
            distance = vector_distance_between_with_reach(position, reach, &actor->position,
                                     actor->collision_radius, actor->collision_height);
        } else if (mode == KF_RADIAL_MODE_REACH_ABOVE) {
            if (position->vy < actor->position.vy - actor->collision_height) {
                distance = KF_DISTANCE_OUTSIDE_REACH;
            } else {
                distance = vector_distance_between_with_reach(position, reach, &actor->position,
                                         actor->collision_radius, actor->collision_height);
            }
        } else {
            distance = vector_distance_to_point(
                &actor->position, position->vx, position->vy, position->vz,
                reach, actor->collision_height, mode);
            if (distance == KF_DISTANCE_NONE) {
                distance = KF_DISTANCE_OUTSIDE_REACH;
            }
        }
        if (distance < minimum_distance) {
            continue;
        }

        if (falloff != KF_FIXED12_ONE) {
            u32 ratio = (u16)((distance << KF_FIXED12_BITS) / reach);
            u16 weight = KF_FIXED12_ONE -
                         ((ratio * (KF_FIXED12_ONE - falloff)) >> KF_FIXED12_BITS);
            scaled_amount = (amount * weight) >> KF_FIXED12_BITS;
        } else {
            scaled_amount = amount;
        }
        actor_apply_magic_to_actor(index, power, magic_06, magic_08, magic_0a,
                      magic_0c, magic_0e, magic_10, magic_12, magic_14,
                      (u16)scaled_amount, effect_flags,
                      damage_position);
    }
}

ADDRESS(0x8003b34c, 0x164)
b32 actor_try_damage_player_in_cone(s32 minimum_distance, s32 maximum_distance, s32 y_offset,
                  s32 angle_tolerance, u16 damage0, u16 damage1,
                  u16 damage2, u16 damage_flags)
{
    s32 offset = y_offset << 5;
    KfActor *actor = actor_state.current;
    const VECTOR *camera_position;
    VECTOR origin;
    s32 distance;
    s32 angle;

    minimum_distance <<= 6;
    maximum_distance <<= 6;
    angle_tolerance <<= 4;
    origin.vx = actor->position.vx;
    origin.vy = actor->position.vy - offset;
    origin.vz = actor->position.vz;
    distance = vector_distance_to_point(&origin,
                                        player_state.camera_position.vx,
                                        player_state.camera_position.vy,
                                        player_state.camera_position.vz,
                                        maximum_distance, 0, 1700);
    if (distance == -1 || distance < minimum_distance) {
        return KF_FALSE;
    }

    camera_position = &player_state.camera_position;
    angle = vector_xz_to_angle(camera_position->vx - origin.vx,
                               camera_position->vz - origin.vz);
    if (!angle_within_tolerance(actor->rotation.y, angle, angle_tolerance)) {
        return KF_FALSE;
    }

    player_apply_damage(damage0, damage1, damage2, damage_flags,
                  0, 0, 0, 0, 0, 0x1000, 10, &origin);
    return KF_TRUE;
}

ADDRESS(0x8003b4b0, 0x27c)
KfActor *actor_find_best_in_cone(const VECTOR *position, s16 yaw, s16 pitch,
                       s32 max_distance, s32 yaw_limit, s32 pitch_limit,
                       s32 *distance, s32 variation)
{
    KfActor *best = NULL;
    s32 best_score = 30000;
    s32 best_distance = -1;
    KfActor *actor = actor_state.actors;
    u16 remaining = KF_ACTOR_CAPACITY - 1;
    struct KfEulerAngles direction;
    s32 actor_distance;
    s32 score;
    s32 reach;

    do {
        if (actor->lifecycle != KF_ACTOR_LIFECYCLE_ACTIVE || actor->target_type == KF_ACTOR_TARGET_3 ||
            actor == actor_state.current) {
            continue;
        }

        reach = max_distance;
        if ((actor->flags & KF_ACTOR_FLAG_CONE_TARGET_PRIORITY) != KF_ACTOR_FLAGS_NONE) {
            reach <<= 1;
        }
        actor_distance = vector_distance_to_point(
            &actor->position, position->vx, KF_DISTANCE_IGNORE_HEIGHT,
            position->vz, reach, 0, 0);
        if (actor_distance == KF_DISTANCE_NONE) {
            continue;
        }

        vector_displacement_to_pitch_yaw(actor->position.vx - position->vx,
                      actor->position.vy - position->vy,
                      actor->position.vz - position->vz, &direction);
        direction.y = (direction.y - yaw) & KF_ANGLE_WRAP_MASK;
        if (direction.y >= KF_ANGLE_HALF_TURN) {
            direction.y = KF_ANGLE_FULL_TURN - direction.y;
        }
        direction.x = (direction.x - pitch) & KF_ANGLE_WRAP_MASK;
        if (direction.x >= KF_ANGLE_HALF_TURN) {
            direction.x = KF_ANGLE_FULL_TURN - direction.x;
        }

        if ((actor->flags & KF_ACTOR_FLAG_CONE_TARGET_PRIORITY) != KF_ACTOR_FLAGS_NONE && variation >= 0) {
            actor_distance >>= 2;
            direction.x -= 512;
            direction.y -= 512;
        } else if (direction.y > yaw_limit || direction.x > pitch_limit) {
            continue;
        }

        score = direction.y + direction.x + (actor_distance >> 7);
        if (variation > 0) {
            score += (rand() * variation) >> 15;
        }
        if (score < best_score) {
            best_score = score;
            best = actor;
            best_distance = actor_distance;
        }
    } while (actor++, remaining-- != 0);

    *distance = best_distance;
    return best;
}

ADDRESS(0x8003b72c, 0x168)
s32 actor_find_overlap_excluding_target_type3(s32 x, s32 y, s32 z,
                                             s32 radius, s32 height)
{
    KfActor *actor = actor_state.actors;
    s32 index;

    for (index = 0; index < KF_ACTOR_CAPACITY; index++, actor++) {
        VECTOR alternate;

        if (actor->lifecycle != KF_ACTOR_LIFECYCLE_ACTIVE || actor->target_type == KF_ACTOR_TARGET_3
            || (actor_state.actor_overlap_exclusion_flags & actor->flags) != KF_ACTOR_FLAGS_NONE
            || actor == actor_state.current) {
            continue;
        }
        if ((actor->flags & KF_ACTOR_FLAG_LINKED) != KF_ACTOR_FLAGS_NONE) {
            if (actor->word_22.linked_actor_slot == actor_state.current_actor_slot_index) {
                continue;
            }
            alternate.vx = actor->position.vx;
            alternate.vz = actor->position.vz;
            alternate.vy = actor->position.vy + actor->vertical_anchor_offset;
            if (vector_distance_to_point(&alternate, x, y, z,
                actor->collision_radius + radius, actor->collision_height, height) != KF_DISTANCE_NONE) {
                return index;
            }
        } else {
            if (vector_distance_to_point(&actor->position, x, y, z,
                actor->collision_radius + radius, actor->collision_height, height) != KF_DISTANCE_NONE) {
                return index;
            }
        }
    }
    return KF_ACTOR_INDEX_NONE;
}

ADDRESS(0x8003b894, 0x158)
s32 actor_find_overlap(s32 x, s32 y, s32 z, s32 radius, s32 height)
{
    KfActor *actor = actor_state.actors;
    s32 index;

    for (index = 0; index < KF_ACTOR_CAPACITY; index++, actor++) {
        VECTOR alternate;

        if (actor->lifecycle != KF_ACTOR_LIFECYCLE_ACTIVE
            || (actor_state.actor_overlap_exclusion_flags & actor->flags) != KF_ACTOR_FLAGS_NONE
            || actor == actor_state.current) {
            continue;
        }
        if ((actor->flags & KF_ACTOR_FLAG_LINKED) != KF_ACTOR_FLAGS_NONE) {
            if (actor->word_22.linked_actor_slot == actor_state.current_actor_slot_index) {
                continue;
            }
            alternate.vx = actor->position.vx;
            alternate.vz = actor->position.vz;
            alternate.vy = actor->position.vy + actor->vertical_anchor_offset;
            if (vector_distance_to_point(&alternate, x, y, z,
                actor->collision_radius + radius, actor->collision_height, height) != KF_DISTANCE_NONE) {
                return index;
            }
        } else {
            if (vector_distance_to_point(&actor->position, x, y, z,
                actor->collision_radius + radius, actor->collision_height, height) != KF_DISTANCE_NONE) {
                return index;
            }
        }
    }
    return KF_ACTOR_INDEX_NONE;
}

ADDRESS(0x8003b9ec, 0xdc)
void actor_bind_current(KfActor *actor)
{
    KfTargetGroup *group;
    KfActor *other;

    if (actor != NULL) {
        actor_state.current = actor;
        group = &actor_state.target_groups[actor->group_index];
        actor_state.active_group = group;
        actor_state.current_group_index = actor->group_index;
        if ((group->initial_actor_flags & KF_ACTOR_FLAG_LINKED) != KF_ACTOR_FLAGS_NONE) {
            other = &actor_state.actors[actor->word_22.linked_actor_slot];
            actor_state.other_actor = other;
            actor_state.other_group = &actor_state.target_groups[other->group_index];
        }
    } else {
        actor_state.current = NULL;
        actor_state.active_group = NULL;
        actor_state.current_group_index = 0;
        actor_state.other_actor = NULL;
        actor_state.other_group = NULL;
    }
}

ADDRESS(0x8003bac8, 0x34)
void actor_advance_animation_wrapped(KfActor *actor, s16 delta)
{
    if (delta < 0) {
        actor->animation_step = -delta;
    } else {
        actor->animation_step = delta;
    }
    actor->animation_phase = (actor->animation_phase + delta) & KF_ACTOR_ANIMATION_PHASE_MAX;
}

ADDRESS(0x8003bafc, 0x5c)
void actor_advance_animation_clamped(KfActor *actor, s16 delta)
{
    s16 phase;

    if (delta < 0) {
        actor->animation_step = -delta;
    } else {
        actor->animation_step = delta;
    }
    phase = actor->animation_phase + delta;
    actor->animation_phase = phase;
    if (phase >= KF_ACTOR_ANIMATION_PHASE_PERIOD) {
        actor->animation_phase = KF_ACTOR_ANIMATION_PHASE_MAX;
    } else if (phase < 0) {
        actor->animation_phase = 0;
    }
}

ADDRESS(0x8003bb58, 0x30)
KfBool32 actor_animation_crossed_phase(const KfActor *actor, u16 phase)
{
    return phase < actor->animation_phase
        && phase >= actor->animation_phase - actor->animation_step;
}

ADDRESS(0x8003bb88, 0x4ec)
KF_ENUM_PARAM(KfCollisionHitFlags, s32) actor_move_horizontal_with_collision(SVECTOR *motion,
    KF_ENUM_PARAM(KfActorMoveFlags, s32) flags)
{
    KfActor *actor = actor_state.current;
    s32 motion_x;
    s32 motion_z;
    s32 original_x;
    s32 original_z;
    VECTOR proposed;
    KF_ENUM_PARAM(KfCollisionHitFlags, s32) collision;
    b32 axis_attempted;
    b32 diagonal_attempted;
    s32 retry_count;
    KF_ENUM_PARAM(KfCollisionHitFlags, s32) result = KF_COLLISION_HIT_NONE;

    if (motion->vx == 0 && motion->vz == 0) {
        return KF_COLLISION_HIT_NONE;
    }
    retry_count = 0;
    diagonal_attempted = KF_FALSE;
    axis_attempted = KF_FALSE;
    original_x = motion_x = motion->vx;
    original_z = motion_z = motion->vz;

retry_move:
    proposed.vx = actor->position.vx + motion_x;
    proposed.vz = actor->position.vz + motion_z;
    collision = collision_query_world(proposed.vx, actor->position.vy, proposed.vz,
        actor->collision_radius,
        actor->collision_height | (KF_ENUM_ENCODE(u32, actor->flags & KF_ACTOR_FLAG_COLLISION_HEIGHT_MASK) << 16),
        actor_state.actor_collision_query_flags);
    if (collision == KF_COLLISION_HIT_NONE) {
    check_floor:
        if (actor->vertical_motion_state == KF_ACTOR_VERTICAL_MOTION_NONE
            && (flags & (KF_ACTOR_MOVE_AVOID_LEDGE | KF_ACTOR_MOVE_PROBE_LEDGE)) != KF_ACTOR_MOVE_NONE) {
            s32 floor_height = KF_COLLISION_CACHE_RESULT;
            s32 probe_x;
            s32 probe_z;

            if (motion_x == 0) {
                probe_x = 0;
            } else if (motion_x > 0) {
                probe_x = actor->collision_radius * 2;
            } else {
                probe_x = -(s32)actor->collision_radius * 2;
            }
            if (motion_z == 0) {
                probe_z = 0;
            } else if (motion_z > 0) {
                probe_z = actor->collision_radius * 2;
            } else {
                probe_z = -(s32)actor->collision_radius * 2;
            }
            collision_query_shapes_with_layer_sample(actor->position.vx, actor->position.vy,
                actor->position.vz + probe_z, actor->collision_radius,
                actor->collision_height | (KF_ENUM_ENCODE(u32, actor->flags & KF_ACTOR_FLAG_COLLISION_HEIGHT_MASK) << 16));
            if (floor_height < KF_COLLISION_CACHE_RESULT) {
                floor_height = KF_COLLISION_CACHE_RESULT;
            }
            collision_query_shapes_with_layer_sample(actor->position.vx + probe_x, actor->position.vy,
                actor->position.vz, actor->collision_radius,
                actor->collision_height | (KF_ENUM_ENCODE(u32, actor->flags & KF_ACTOR_FLAG_COLLISION_HEIGHT_MASK) << 16));
            if (floor_height < KF_COLLISION_CACHE_RESULT) {
                floor_height = KF_COLLISION_CACHE_RESULT;
            }
            if (actor->position.vy < floor_height - 1100) {
                result |= KF_COLLISION_HIT_LEDGE;
                if ((flags & KF_ACTOR_MOVE_AVOID_LEDGE) != KF_ACTOR_MOVE_NONE) {
                    goto try_axis;
                }
            }
        }
        actor->position.vx = proposed.vx;
        actor->position.vz = proposed.vz;
        goto finish;
    }

    result |= collision;
    if ((flags & (KF_ACTOR_MOVE_SLIDE_KEEP_SPEED | KF_ACTOR_MOVE_SLIDE)) == KF_ACTOR_MOVE_NONE) {
        goto finish;
    }
    if ((collision & (KF_COLLISION_HIT_ACTOR | KF_COLLISION_HIT_MAP_OBJECT | KF_COLLISION_HIT_PLAYER)) != KF_COLLISION_HIT_NONE) {
        s32 obstacle_angle;
        s32 movement_angle;
        s32 length;

        retry_count++;
        if (retry_count == 2) {
            goto finish;
        }
        collision_cache_load_hit_bounds();
        obstacle_angle = vector_xz_to_angle(
            KF_COLLISION_CACHE_POSITION.vx - actor->position.vx,
            KF_COLLISION_CACHE_POSITION.vz - actor->position.vz);
        movement_angle = vector_xz_to_angle(motion_x, motion_z);
        if (angle_mod_delta_le_half_turn(movement_angle, obstacle_angle)) {
            obstacle_angle = (obstacle_angle + KF_ANGLE_QUARTER_TURN) & KF_ANGLE_WRAP_MASK;
        } else {
            obstacle_angle = (obstacle_angle - KF_ANGLE_QUARTER_TURN) & KF_ANGLE_WRAP_MASK;
        }
        length = SquareRoot0(motion_x * motion_x + motion_z * motion_z);
        motion_x = -(rsin(obstacle_angle) * length) >> 13;
        motion_z = (rcos(obstacle_angle) * length) >> 13;
        goto retry_move;
    }
    if ((flags & KF_ACTOR_MOVE_NO_STEP_UP) == KF_ACTOR_MOVE_NONE) {
        if ((actor->flags & KF_ACTOR_FLAG_NO_STEP_UP) != KF_ACTOR_FLAGS_NONE) {
            goto try_axis;
        }
        if ((collision & ~(KF_COLLISION_HIT_AXIS | KF_COLLISION_HIT_FLOOR)) == KF_COLLISION_HIT_NONE) {
            if (actor->position.vy <= KF_COLLISION_CACHE_RESULT + 800) {
                goto check_floor;
            }
            goto try_axis;
        }
    }
    if ((collision & KF_COLLISION_HIT_AXIS) == KF_COLLISION_HIT_NONE) {
        goto check_diagonal;
    }

try_axis:
    if (axis_attempted) {
        motion_z = 0;
        motion_x = 0;
        goto finish;
    }
    if (motion_x != 0) {
        motion_x = 0;
        if ((flags & KF_ACTOR_MOVE_SLIDE_KEEP_SPEED) != KF_ACTOR_MOVE_NONE) {
            s32 length = SquareRoot0(original_x * original_x
                                    + original_z * original_z);
            motion_z = length;
            if (original_z <= 0) {
                motion_z = -motion_z;
            }
        } else {
            motion_z = original_z;
        }
        goto retry_move;
    }
    if (motion_z != 0) {
        motion_z = 0;
        if ((flags & KF_ACTOR_MOVE_SLIDE_KEEP_SPEED) != KF_ACTOR_MOVE_NONE) {
            s32 length = SquareRoot0(original_x * original_x
                                    + original_z * original_z);
            motion_x = length;
            if (original_x <= 0) {
                motion_x = -motion_x;
            }
        } else {
            motion_z = original_z;
        }
        axis_attempted = KF_TRUE;
        goto retry_move;
    }
check_diagonal:
    if ((collision & KF_COLLISION_HIT_DIAGONAL) == KF_COLLISION_HIT_NONE) {
        goto finish;
    }
    if (diagonal_attempted) {
        goto try_axis;
    }
    switch (KF_ENUM_DECODE(KfQuarterTurn,
                           KF_COLLISION_CACHE_SHAPE->quarter_turns & KF_MAP_CELL_QUARTER_TURN_MASK)) {
    case KF_QUARTER_TURN_0:
    case KF_QUARTER_TURN_2:
        motion_x = (original_x + original_z) >> 1;
        motion_z = motion_x;
        break;
    default:
        motion_x = (original_x - original_z) >> 1;
        motion_z = -motion_x;
        break;
    }
    diagonal_attempted = KF_TRUE;
    goto retry_move;

finish:
    if ((flags & KF_ACTOR_MOVE_STORE_MOTION) != KF_ACTOR_MOVE_NONE) {
        motion->vx = motion_x;
        motion->vz = motion_z;
    }
    return result;
}

ADDRESS(0x8003c074, 0x1e4)
KF_ENUM_PARAM(KfCollisionHitFlags, s32) actor_move_with_collision(SVECTOR *motion)
{
    KfActor *actor = actor_state.current;
    VECTOR proposed;
    KF_ENUM_PARAM(KfCollisionHitFlags, s32) result;

    proposed.vx = actor->position.vx + motion->vx;
    proposed.vy = actor->position.vy + motion->vy;
    proposed.vz = actor->position.vz + motion->vz;
    result = collision_query_world(proposed.vx, proposed.vy, proposed.vz,
        actor->collision_radius,
        actor->collision_height | (KF_ENUM_ENCODE(u32, actor->flags & KF_ACTOR_FLAG_COLLISION_HEIGHT_MASK) << 16),
        actor_state.actor_collision_query_flags);
    if (result == KF_COLLISION_HIT_NONE) {
        copyVector(&actor->position, &proposed);
    } else if (collision_query_world(proposed.vx, actor->position.vy,
                             actor->position.vz, actor->collision_radius,
                             actor->collision_height | (KF_ENUM_ENCODE(u32, actor->flags & KF_ACTOR_FLAG_COLLISION_HEIGHT_MASK) << 16),
                             actor_state.actor_collision_query_flags) != KF_COLLISION_HIT_NONE) {
        motion->vx = -motion->vx;
    } else if (collision_query_world(actor->position.vx, proposed.vy,
                             actor->position.vz, actor->collision_radius,
                             actor->collision_height | (KF_ENUM_ENCODE(u32, actor->flags & KF_ACTOR_FLAG_COLLISION_HEIGHT_MASK) << 16),
                             actor_state.actor_collision_query_flags) != KF_COLLISION_HIT_NONE) {
        motion->vy = -motion->vy;
    } else if (collision_query_world(actor->position.vx, actor->position.vy,
                             proposed.vz, actor->collision_radius,
                             actor->collision_height | (KF_ENUM_ENCODE(u32, actor->flags & KF_ACTOR_FLAG_COLLISION_HEIGHT_MASK) << 16),
                             actor_state.actor_collision_query_flags) != KF_COLLISION_HIT_NONE) {
        motion->vz = -motion->vz;
    }
    return result;
}

ADDRESS(0x8003c258, 0x9c)
s32 actor_start_ballistic_motion(KF_ENUM_PARAM(KfTrajectoryMode, s32) mode, s32 target_x,
    s32 target_y, s32 target_z, s32 trajectory_parameter, s32 trajectory_speed)
{
    KfActor *actor = actor_state.current;
    s16 result;

    if (trajectory_solve_motion_between_points(mode, actor->position.vx, actor->position.vy,
        actor->position.vz, target_x, target_y, target_z,
        trajectory_parameter, trajectory_speed, &result,
        &actor->ballistic_horizontal_speed, &actor->ballistic_launch_speed_y) != KF_TRAJECTORY_SOLVED) {
        return KF_ENUM_ENCODE(s32, KF_TRAJECTORY_UNREACHABLE);
    }
    actor->vertical_motion_state = KF_ACTOR_VERTICAL_MOTION_BALLISTIC;
    actor->motion.ballistic.phase = 1;
    actor->ballistic_acceleration = trajectory_parameter;
    actor->ballistic_origin_y = actor->position.vy;
    return result;
}

ADDRESS(0x8003c2f4, 0x14)
void actor_suspend_vertical_motion(void)
{
    actor_state.current->vertical_motion_state = KF_ACTOR_VERTICAL_MOTION_SUSPENDED;
}

ADDRESS(0x8003c308, 0x3d4)
void actor_update_vertical_motion(void)
{
    KfActor *actor = actor_state.current;
    KfTargetGroup *group = actor_state.active_group;
    KF_ENUM_PROMOTED(KfActorVerticalState) vertical_state;
    const u16 *collision_layer = &KF_COLLISION_CACHE.layer;

    collision_probe_floor_height(actor->position.vx, actor->position.vy, actor->position.vz,
                   actor->collision_radius,
                   actor->collision_height | (KF_ENUM_ENCODE(u32, actor->flags & KF_ACTOR_FLAG_COLLISION_HEIGHT_MASK) << 16));
    actor->current_map_layer = *collision_layer == 0 ? KF_MAP_LAYER_FIRST : KF_MAP_LAYER_SECOND;
    if ((actor->flags & KF_ACTOR_FLAG_USE_MAP_LAYER_FLOOR) != KF_ACTOR_FLAGS_NONE) {
        KF_COLLISION_CACHE_RESULT = KF_COLLISION_CACHE_HEIGHT;
    }

    vertical_state = actor->vertical_motion_state;
    switch (vertical_state) {
    case KF_ACTOR_VERTICAL_MOTION_NONE: {
        s32 next_y;
        next_y = KF_COLLISION_CACHE_RESULT - actor->position.vy;
        if (next_y < 0) {
            actor->vertical_motion_state = KF_ACTOR_VERTICAL_MOTION_FALLING;
            actor->motion.vector.vy = -100;
        } else if (next_y > 0) {
            actor->vertical_motion_state = KF_ACTOR_VERTICAL_MOTION_VELOCITY;
            actor->motion.vector.vy = 0;
        }
        return;
    }

    case KF_ACTOR_VERTICAL_MOTION_VELOCITY: {
        s32 next_y;
        KF_ENUM_PARAM(KfCollisionHitFlags, s32) collision;
        next_y = actor->position.vy + actor->motion.vector.vy;
        collision = collision_query_world(actor->position.vx, next_y,
                                  actor->position.vz, actor->collision_radius,
                                  actor->collision_height |
                                      (KF_ENUM_ENCODE(u32, actor->flags & KF_ACTOR_FLAG_COLLISION_HEIGHT_MASK) << 16),
                                  actor_state.actor_collision_query_flags);
        if (collision == KF_COLLISION_HIT_NONE) {
        advance_rise:
            actor->position.vy = next_y;
            actor->motion.vector.vy += group->vertical_acceleration;
            return;
        }
        if (collision == KF_COLLISION_HIT_PLAYER && actor->motion.vector.vy > 40) {
            player_apply_damage(0, group->contact_damage_component1, 0, 0, 0, 0, 0, 0, 0,
                          0x1000, 10, &actor->position);
        }
        if ((collision & KF_COLLISION_HIT_FLOOR) != KF_COLLISION_HIT_NONE) {
            if ((actor->flags & KF_ACTOR_FLAG_USE_MAP_LAYER_FLOOR) != KF_ACTOR_FLAGS_NONE) {
                s32 floor_y = KF_COLLISION_CACHE_HEIGHT;
                if (actor->position.vy < floor_y) {
                    goto advance_rise;
                }
                actor->position.vy = floor_y;
            } else {
                actor->position.vy = KF_COLLISION_CACHE_RESULT;
            }
            actor->motion.vector.vy = 0;
            goto reset_vertical_motion_state;
        }
        if ((actor->flags & KF_ACTOR_FLAG_USE_MAP_LAYER_FLOOR) != KF_ACTOR_FLAGS_NONE) {
            goto advance_rise;
        }
        goto reset_vertical_motion_state;
    }

    case KF_ACTOR_VERTICAL_MOTION_FALLING:
        actor->position.vy += actor->motion.vector.vy;
        actor->motion.vector.vy += 5;
        if (KF_COLLISION_CACHE_RESULT < actor->position.vy &&
            actor->motion.vector.vy < 0) {
            return;
        }
        actor->position.vy = KF_COLLISION_CACHE_RESULT;
reset_vertical_motion_state:
        actor->vertical_motion_state = KF_ACTOR_VERTICAL_MOTION_NONE;
        return;

    case KF_ACTOR_VERTICAL_MOTION_BALLISTIC: {
        s32 phase;
        s32 launch_speed;
        s32 next_y;
        KF_ENUM_PARAM(KfCollisionHitFlags, s32) collision;
        launch_speed = actor->ballistic_launch_speed_y;
        phase = actor->motion.ballistic.phase;
        next_y = actor->ballistic_origin_y - launch_speed * phase +
                 ((actor->ballistic_acceleration * phase * phase) >> 1);
        collision = collision_query_world(actor->position.vx, next_y,
                                  actor->position.vz, actor->collision_radius,
                                  actor->collision_height |
                                      (KF_ENUM_ENCODE(u32, actor->flags & KF_ACTOR_FLAG_COLLISION_HEIGHT_MASK) << 16),
                                  actor_state.actor_collision_query_flags);
        if (collision == KF_COLLISION_HIT_NONE) {
            actor->position.vy = next_y;
            actor->motion.ballistic.phase++;
            actor->current_map_layer = *collision_layer == 0 ? KF_MAP_LAYER_FIRST : KF_MAP_LAYER_SECOND;
            return;
        }
        if (collision == KF_COLLISION_HIT_PLAYER) {
            player_apply_damage(0, group->contact_damage_component1, 0, 0, 0, 0, 0, 0, 0,
                          0x1000, 10, &actor->position);
        }
        actor->vertical_motion_state = KF_ACTOR_VERTICAL_MOTION_VELOCITY;
        actor->motion.ballistic.phase = 0;
        return;
    }
    case KF_ACTOR_VERTICAL_MOTION_SUSPENDED:
        break;
    }
}

ADDRESS(0x8003c6dc, 0x140)
KF_ENUM_PARAM(KfCollisionHitFlags, s32) actor_damp_horizontal_motion(s32 decay,
    KF_ENUM_PARAM(KfActorMoveFlags, s32) move_flags)
{
    KfActor *actor = actor_state.current;
    s32 length;

    if (actor->vertical_motion_state == KF_ACTOR_VERTICAL_MOTION_NONE) {
        length = SquareRoot0(actor->motion.vector.vx * actor->motion.vector.vx
                           + actor->motion.vector.vz * actor->motion.vector.vz);
        if (length == 0) {
            return KF_COLLISION_HIT_NONE;
        }
        actor->motion.vector.vx = value_approach(actor->motion.vector.vx, 0,
            (actor->motion.vector.vx * decay * 2) / length);
        actor->motion.vector.vz = value_approach(actor->motion.vector.vz, 0,
            (actor->motion.vector.vz * decay * 2) / length);
    }
    return actor_move_horizontal_with_collision(&actor->motion.vector, move_flags);
}

ADDRESS(0x8003c81c, 0xbc)
KF_ENUM_PARAM(KfCollisionHitFlags, s32) actor_move_along_heading(s16 angle, s32 speed, s32 step,
    KF_ENUM_PARAM(KfActorMoveFlags, s32) move_flags)
{
    KfActor *actor = actor_state.current;
    struct KfVecXZi direction;
    struct KfVecXZi step_direction;

    angle_to_forward_xz(angle, &direction);
    step_direction = direction;
    vector2i_scale_shift11(speed, &direction);
    vector2i_scale_shift11(step, &step_direction);
    actor->motion.vector.vx = value_approach(actor->motion.vector.vx,
                                        direction.x, step_direction.x);
    actor->motion.vector.vz = value_approach(actor->motion.vector.vz,
                                        direction.z, step_direction.z);
    return actor_move_horizontal_with_collision(&actor->motion.vector, move_flags);
}

ADDRESS(0x8003c8d8, 0x130)
void actor_turn_toward_angle(KfActor *actor, s32 target_angle, s32 max_speed,
    s32 acceleration)
{
    if (angle_shortest_delta(target_angle, actor->rotation.y) != 0) {
        s32 old_angle;

        if (angle_mod_delta_le_half_turn(target_angle, actor->rotation.y)) {
            actor->turn_rate += acceleration;
            if (max_speed < actor->turn_rate) {
                actor->turn_rate = max_speed;
            }
        } else {
            actor->turn_rate -= acceleration;
            if (actor->turn_rate < -max_speed) {
                actor->turn_rate = -max_speed;
            }
        }

        old_angle = actor->rotation.y;
        actor->rotation.y += actor->turn_rate;
        if (actor->turn_rate > 0) {
            if (angle_mod_delta_le_half_turn(target_angle, old_angle) &&
                !angle_mod_delta_le_half_turn(target_angle, actor->rotation.y)) {
                actor->rotation.y = target_angle;
            }
        } else {
            if (angle_mod_delta_le_half_turn(old_angle, target_angle) &&
                !angle_mod_delta_le_half_turn(actor->rotation.y, target_angle)) {
                actor->rotation.y = target_angle;
            }
        }
    } else {
        actor->turn_rate = 0;
    }
}

ADDRESS(0x8003ca08, 0x70)
KF_ENUM_PARAM(KfCollisionHitFlags, s32) actor_turn_and_move_along_heading(s16 angle, s32 speed,
    s32 range, s32 step, s32 mode, KF_ENUM_PARAM(KfActorMoveFlags, s32) move_flags)
{
    KfActor *actor = actor_state.current;

    actor_turn_toward_angle(actor, angle, range, mode);
    return actor_move_along_heading(actor->rotation.y, speed, step, move_flags);
}

ADDRESS(0x8003ca78, 0xf8)
s32 actor_turn_and_move_toward_point(s32 world_x, s32 world_z, s32 speed, s32 range,
    s16 reference_angle, s32 step, s32 mode, KF_ENUM_PARAM(KfActorMoveFlags, s32) move_flags)
{
    KfActor *actor = actor_state.current;
    s32 dx = world_x - actor->position.vx;
    s32 dz = world_z - actor->position.vz;
    s32 angle = vector_xz_to_angle(dx, dz);
    s32 distance = abs(dx) + abs(dz);

    if (reference_angle != KF_ACTOR_HEADING_NONE && distance <= 600
        && !angle_within_tolerance(angle, reference_angle, 0x320)) {
        return KF_ACTOR_HEADING_NONE;
    }
    actor_turn_and_move_along_heading(angle, speed, range, step, mode, move_flags);
    return angle & KF_ANGLE_WRAP_MASK;
}

ADDRESS(0x8003cb70, 0x13c)
KF_ENUM_PARAM(KfActorEulerMoveResult, s32) actor_move_along_euler_angles(
    const struct KfEulerAngles *angles, s32 speed, s32 step,
    KF_ENUM_PARAM(KfActorMoveFlags, s32) move_flags)
{
    KfActor *actor = actor_state.current;
    SVECTOR direction;
    SVECTOR step_direction;
    KF_ENUM_PARAM(KfActorEulerMoveResult, s32) moved;
    s32 proposed_y;
    s32 radius;
    s32 height_and_flags;

    pitch_yaw_to_forward_vector(angles, &direction);
    step_direction = direction;
    vector3s_scale_shift12(speed, &direction);
    vector3s_scale_shift12(step, &step_direction);
    actor->motion.vector.vx = value_approach(actor->motion.vector.vx,
                                        direction.vx, step_direction.vx);
    actor->motion.vector.vy = value_approach(actor->motion.vector.vy,
                                        direction.vy, step_direction.vy);
    actor->motion.vector.vz = value_approach(actor->motion.vector.vz,
                                        direction.vz, step_direction.vz);
    moved = KF_ENUM_DECODE(KF_ENUM_PARAM(KfActorEulerMoveResult, s32),
                           actor_move_horizontal_with_collision(&actor->motion.vector, move_flags) !=
                               KF_COLLISION_HIT_NONE);
    proposed_y = actor->position.vy + actor->motion.vector.vy;
    radius = actor->collision_radius;
    height_and_flags = actor->collision_height | (KF_ENUM_ENCODE(u32, actor->flags & KF_ACTOR_FLAG_COLLISION_HEIGHT_MASK) << 16);
    if (collision_query_world(actor->position.vx, proposed_y, actor->position.vz,
                      radius, height_and_flags,
                      actor_state.actor_collision_query_flags) == KF_COLLISION_HIT_NONE) {
        actor->position.vy = proposed_y;
    } else {
        moved |= KF_ACTOR_EULER_BLOCKED_VERTICAL;
    }
    return moved;
}

ADDRESS(0x8003ccac, 0x8c)
KF_ENUM_PARAM(KfActorEulerMoveResult, s32) actor_turn_and_move_along_euler_angles(
    const struct KfEulerAngles *angles, s32 speed, s32 range, s32 step, s32 mode,
    KF_ENUM_PARAM(KfActorMoveFlags, s32) move_flags)
{
    KfActor *actor = actor_state.current;

    actor_turn_toward_angle(actor, angles->y, range, mode);
    actor->rotation.x = angle_approach(actor->rotation.x, angles->x, 8);
    return actor_move_along_euler_angles(&actor->rotation, speed, step, move_flags);
}

ADDRESS(0x8003cd38, 0x10c)
s32 actor_sample_rotated_animation_vertex(KfActor *actor, s32 vertex_index, VECTOR *output)
{
    struct KfEulerAngles rotation;
    SVECTOR offset;

    if (animation_sample_vertex(actor->definition_id + 128, actor->animation_id,
                      actor->animation_phase, vertex_index, &offset)) {
        offset.vx = 0;
        offset.vy = -(s32)actor->collision_height >> 1;
        offset.vz = -(s32)actor->collision_radius;
    } else {
        offset.vx = (offset.vx * actor->model_scale_x) >> KF_FIXED12_BITS;
        offset.vy = (offset.vy * actor->model_scale_y.value) >> KF_FIXED12_BITS;
        offset.vz = (offset.vz * actor->model_scale_z) >> KF_FIXED12_BITS;
    }
    rotation.x = actor->rotation.x;
    rotation.y = actor->rotation.y + KF_ANGLE_HALF_TURN;
    rotation.z = actor->rotation.z;
    vector_rotate_yxz(&rotation, &offset, output);
    return 1;
}

ADDRESS(0x8003ce44, 0x114)
VECTOR *actor_resolve_group_position(KfActor *actor, VECTOR *output)
{
    KfTargetGroup *group;
    switch (actor->flags & KF_ACTOR_POSITION_MODE_MASK) {
    case KF_ACTOR_POSITION_DIRECT:
        return &actor->position;
    case KF_ACTOR_POSITION_GROUP_OFFSET:
        group = &actor_state.target_groups[actor->group_index];
        setVector(output, actor->position.vx + group->position_offset_x,
                  actor->position.vy + group->position_offset_y,
                  actor->position.vz + group->position_offset_z);
        return output;
    case KF_ACTOR_POSITION_ROTATED_GROUP_OFFSET:
        group = &actor_state.target_groups[actor->group_index];
        vector_rotate_yxz(&actor->rotation,
                          (SVECTOR *)&group->position_offset_x, output);
        addVector(output, &actor->position);
        return output;
    default:
        break;
    }
    /* Retail leaves the return register unspecified for mode 3. */
}

ADDRESS(0x8003cf58, 0x1c0)
void actor_update_motion_animation(KF_ENUM_PARAM(KfAnimationClip, s32) first,
    KF_ENUM_PARAM(KfAnimationClip, s32) reverse, KF_ENUM_PARAM(KfAnimationClip, s32) forward,
    KF_ENUM_PARAM(KfAnimationClip, s32) fast, KF_ENUM_PARAM(KfAnimationClip, s32) slow,
    s32 phase_step)
{
    KfActor *actor = actor_state.current;
    KF_ENUM_PROMOTED(KfAnimationClip) selected = first;
    s32 magnitude = 0;
    s16 motion = actor->turn_rate;

    if (motion > 0) {
        selected = forward;
        magnitude = motion;
    } else if (motion < 0) {
        selected = reverse;
        magnitude = -motion;
    }
    if (actor->motion.vector.vy >= 11) {
        if (magnitude < actor->motion.vector.vy) {
            selected = fast;
        }
    } else if (actor->motion.vector.vy < 10) {
        if (magnitude < -actor->motion.vector.vy) {
            selected = slow;
        }
    }

    if (actor->animation_id == first) {
        actor_advance_animation_wrapped(actor, phase_step);
        if (actor_animation_crossed_phase(actor, 0)) {
            actor_set_animation_if_changed(selected);
        }
    } else if (actor->animation_id == reverse) {
        if (actor->animation_phase < KF_ACTOR_ANIMATION_PHASE_PERIOD / 2) {
            actor_advance_animation_clamped(actor, phase_step);
        } else if (selected != actor->animation_id) {
            actor_advance_animation_clamped(actor, phase_step);
            if (actor->animation_phase >= KF_ACTOR_ANIMATION_PHASE_MAX) {
                actor_set_animation(selected);
            }
        }
    } else if (actor->animation_id == forward) {
        if (actor->animation_phase < KF_ACTOR_ANIMATION_PHASE_PERIOD / 2) {
            actor_advance_animation_clamped(actor, phase_step);
        } else if (selected != actor->animation_id) {
            actor_advance_animation_clamped(actor, phase_step);
            if (actor->animation_phase >= KF_ACTOR_ANIMATION_PHASE_MAX) {
                actor_set_animation(selected);
            }
        }
    } else if (actor->animation_id == fast) {
        if (actor->animation_phase < KF_ACTOR_ANIMATION_PHASE_PERIOD / 2) {
            actor_advance_animation_clamped(actor, phase_step);
        } else if (selected != actor->animation_id) {
            actor_advance_animation_clamped(actor, phase_step);
            if (actor->animation_phase >= KF_ACTOR_ANIMATION_PHASE_MAX) {
                actor_set_animation(selected);
            }
        }
    } else if (actor->animation_id == slow) {
        if (actor->animation_phase < KF_ACTOR_ANIMATION_PHASE_PERIOD / 2) {
            actor_advance_animation_clamped(actor, phase_step);
        } else if (selected != actor->animation_id) {
            actor_advance_animation_clamped(actor, phase_step);
            if (actor->animation_phase >= KF_ACTOR_ANIMATION_PHASE_MAX) {
                actor_set_animation(selected);
            }
        }
    }
}

ADDRESS(0x8003d118, 0x234)
s32 actor_compute_target_direction(KfActor *actor, const VECTOR *origin, s32 step,
                  const VECTOR *target, SVECTOR *direction,
                  s32 pitch_override, u16 yaw_limit, s32 iterations)
{
    VECTOR position = *origin;
    struct KfEulerAngles angles;
    s32 pitch = pitch_override;
    s32 yaw_scaled;
    s32 yaw_delta;
    s32 pitch_error;
    s32 distance;
    s32 target_y;

    for (;;) {
        target_y = target->vy + 1600;
        vector_displacement_to_pitch_yaw(position.vx - target->vx,
                      position.vy - target_y,
                      position.vz - target->vz, &angles);
        yaw_delta = (angles.y - actor->rotation.y) & KF_ANGLE_WRAP_MASK;
        yaw_scaled = yaw_delta << KF_FIXED12_BITS;
        if (yaw_delta >= KF_ANGLE_HALF_TURN) {
            yaw_delta = KF_ANGLE_FULL_TURN - yaw_delta;
            yaw_scaled = yaw_delta << KF_FIXED12_BITS;
        }
        yaw_delta = yaw_scaled / (s16)yaw_limit;
        if (yaw_delta > KF_FIXED12_ONE) {
            yaw_delta = KF_FIXED12_ONE;
        }
        angles.y = angle_lerp_shortest_q12(angles.y, actor->rotation.y, yaw_delta);

        if ((s16)pitch == KF_ACTOR_PITCH_TRACK_TARGET) {
            pitch_error = (angles.x - actor->rotation.x) & KF_ANGLE_WRAP_MASK;
            if (pitch_error >= KF_ANGLE_HALF_TURN) {
                pitch_error = KF_ANGLE_FULL_TURN - pitch_error;
            }
            angles.x = angle_lerp_shortest_q12(angles.x, actor->rotation.x, pitch_error);
        } else {
            angles.x = pitch;
        }

        pitch_yaw_to_forward_vector(&angles, direction);
        vector3s_scale_shift12(step, direction);
        if (--iterations == 0) {
            break;
        }

        distance = fixed_vector3_length(target->vx - position.vx,
                                        target->vy - position.vy,
                                        target->vz - position.vz);
        vector_add_scaled_delta(origin, &player_state.frame_displacement,
                      distance / step, &position);
    }
    return angles.y;
}




enum {
    ACTOR_EFFECT_POSITION_ROTATED_OFFSET = -1,
    ACTOR_EFFECT_POSITION_TWO_VERTICES = -2
};

/* Type-25 effect-script words: control opcodes from 0x8000, otherwise the
 * animation vertex to emit the script effect from. */
enum {
    ACTOR_SCRIPT_REWIND = 0x8000,
    ACTOR_SCRIPT_REPEAT = 0x8001,
    ACTOR_SCRIPT_ROTATED_OFFSET_EFFECT = 0x8002,
    ACTOR_SCRIPT_SKIP = 0x8003,
    ACTOR_SCRIPT_TWO_VERTEX_EFFECT = 0x8004
};

/* The script gives either three signed coordinates, two vertex indices and a
 * blend fraction, or one vertex index. The final pointer is used by the
 * coordinate form when an effect kind consumes an extra script halfword. */
ADDRESS(0x8003d34c, 0xa70)
void actor_dispatch_group_effect(KF_ENUM_PARAM(KfEffectKind, s32) kind, s32 damage_multiplier_tenths,
                                 s32 position_mode, ...)
{
    /* Retail walks O32 argument home slots from the last named word. */
    const s32 *arguments = &position_mode;
    KfActor *current = actor_state.current;
    const VECTOR *player = &player_state.camera_position;
    s32 first;
    s32 second;
    s32 third;
    VECTOR position;
    VECTOR offset;
    VECTOR target;
    VECTOR predicted;
    SVECTOR direction;
    union {
        SVECTOR motion;
        struct KfEulerAngles angles;
    } orientation;
    SVECTOR rotated;
    VECTOR trajectory_target;
    KfEffectRecord *effect;
    KfActor *spawned;
    s32 travel_time;
    s32 trajectory_angle;
    s32 distance;
    s32 count;
    s32 group_index;

    if (position_mode == ACTOR_EFFECT_POSITION_ROTATED_OFFSET) {
        /* Each coordinate occupies an O32 word slot but is read as u16. */
        arguments += 3;
        rotated.vx = *(const u16 *)(arguments - 2);
        rotated.vy = *(const u16 *)(arguments - 1);
        rotated.vz = *(const u16 *)arguments;
        vector_rotate_yxz(&current->rotation, &rotated, &offset);
    } else if (position_mode == ACTOR_EFFECT_POSITION_TWO_VERTICES) {
        first = arguments[1];
        actor_sample_rotated_animation_vertex(current, first, &target);
        second = arguments[2];
        actor_sample_rotated_animation_vertex(current, second, &offset);
        arguments += 3;
        third = *arguments;
        predicted.vx = fixed_lerp_q12(player->vx,
            ((offset.vx - target.vx) << 8) + current->position.vx, third);
        predicted.vy = fixed_lerp_q12(player->vy,
            ((offset.vy - target.vy) << 8) + current->position.vy, third);
        predicted.vz = fixed_lerp_q12(player->vz,
            ((offset.vz - target.vz) << 8) + current->position.vz, third);
        player = &predicted;
    } else {
        actor_sample_rotated_animation_vertex(current, position_mode, &offset);
    }

    position.vx = current->position.vx + offset.vx;
    position.vy = current->position.vy + offset.vy;
    position.vz = current->position.vz + offset.vz;

    switch (kind) {
    case KF_EFFECT_KIND_123:
        kind = KF_EFFECT_KIND_32;
        if (arguments[2] != 0) {
            goto target_effect;
        }
        /* fall through */
    case KF_EFFECT_KIND_7:
    case KF_EFFECT_KIND_32:
        audio_play_spatial_default_range(0x23, &position, 0x6e, 0);
    target_effect:
        actor_compute_target_direction(current, player, 500, &position, &direction,
            KF_ACTOR_PITCH_TRACK_TARGET, 0x400, 1);
        effect = effect_construct_record(damage_multiplier_tenths,
                                         KF_EFFECT_SOURCE_HAZARD | KF_EFFECT_TARGET_ACTORS_AND_PLAYER, kind,
                                         &position, &direction);
        if (effect != NULL) {
            effect->cooldown = 3;
        }
        break;
    case KF_EFFECT_KIND_121:
        actor_compute_target_direction(current, player, 600, &position, &direction,
            KF_ACTOR_PITCH_TRACK_TARGET, 0x400, 1);
        distance = fixed_vector3_length(position.vx - player->vx,
                                        position.vy - player->vy,
                                        position.vz - player->vz);
        distance = (distance - 2000) / 600;
        if (distance < 0) {
            distance = 0;
        }
        effect_construct_record(damage_multiplier_tenths,
                                KF_EFFECT_SOURCE_HAZARD | KF_EFFECT_TARGET_ACTORS_AND_PLAYER, kind, &position, &direction,
                      distance);
        break;
    case KF_EFFECT_KIND_4:
        /* Five-argument effect calls leave the optional words that the steering
         * call stored in the outgoing argument area. */
        actor_compute_target_direction(current, player, 800, &position, &direction,
            KF_ACTOR_PITCH_TRACK_TARGET, 0x400, 1);
        effect_construct_record(damage_multiplier_tenths,
                                KF_EFFECT_SOURCE_HAZARD | KF_EFFECT_TARGET_ACTORS_AND_PLAYER, kind,
                                &position, &direction);
        break;
    case KF_EFFECT_KIND_40: {
        s32 raised_y = position.vy + 1600;
        vector_displacement_to_pitch_yaw(player->vx - position.vx,
                      player->vy - raised_y,
                      player->vz - position.vz,
                      &orientation.angles);
        pitch_yaw_to_forward_vector(&orientation.angles, &direction);
        vector3s_scale_shift12(1000, &direction);
        position.vx += direction.vx;
        position.vy += direction.vy;
        position.vz += direction.vz;
        effect_construct_record(damage_multiplier_tenths,
                                KF_EFFECT_SOURCE_HAZARD | KF_EFFECT_TARGET_ACTORS_AND_PLAYER, kind, &position, &direction,
                      &orientation.angles);
        break;
    }
    case KF_EFFECT_KIND_9:
    case KF_EFFECT_KIND_33:
        actor_compute_target_direction(current, player, 400, &position, &direction,
            KF_ACTOR_PITCH_TRACK_TARGET, 0x400, 1);
        effect_construct_record(damage_multiplier_tenths,
                                KF_EFFECT_SOURCE_HAZARD | KF_EFFECT_TARGET_ACTORS_AND_PLAYER, kind, &position, &direction,
                      KF_EFFECT_TARGET_ACTOR_PLAYER);
        break;
    case KF_EFFECT_KIND_24:
        actor_compute_target_direction(current, player, 250, &position, &direction,
            KF_ACTOR_PITCH_TRACK_TARGET, 0x400, 1);
        effect_construct_record(damage_multiplier_tenths,
                                KF_EFFECT_SOURCE_HAZARD | KF_EFFECT_TARGET_ACTORS_AND_PLAYER, kind,
                                &position, &direction);
        break;
    case KF_EFFECT_KIND_2:
        effect_construct_record(damage_multiplier_tenths,
                                KF_EFFECT_SOURCE_HAZARD | KF_EFFECT_TARGET_ACTORS_AND_PLAYER, kind, &position, &direction,
                      0x1000, 0x100, 0x1000);
        break;
    case KF_EFFECT_KIND_22:
        actor_compute_target_direction(current, player, 400, &position, &direction,
            KF_ACTOR_PITCH_TRACK_TARGET, 0x400, 1);
        effect_construct_record(damage_multiplier_tenths,
                                KF_EFFECT_SOURCE_HAZARD | KF_EFFECT_TARGET_ACTORS_AND_PLAYER, kind,
                                &position, &direction);
        break;
    case KF_EFFECT_KIND_23:
        effect_construct_record(damage_multiplier_tenths,
                                KF_EFFECT_SOURCE_HAZARD | KF_EFFECT_TARGET_ACTORS_AND_PLAYER, kind, &position, NULL,
                      actor_state.current_actor_slot_index, position_mode,
                      ((const u16 *)arguments[1])[2]);
        break;
    case KF_EFFECT_KIND_108:
        pitch_yaw_to_forward_vector(&current->rotation, &direction);
        vector3s_scale_shift12(550, &direction);
        effect = effect_construct_record(damage_multiplier_tenths,
                                         KF_EFFECT_SOURCE_HAZARD | KF_EFFECT_TARGET_ACTORS_AND_PLAYER,
                                         KF_EFFECT_KIND_7, &position, &direction);
        if (effect != NULL) {
            effect->cooldown = 5;
        }
        break;
    case KF_EFFECT_KIND_1:
    case KF_EFFECT_KIND_28:
        actor_compute_target_direction(current, player, 500, &position, &direction,
            KF_ACTOR_PITCH_TRACK_TARGET, 0x400, 1);
        effect_construct_record(damage_multiplier_tenths,
                                KF_EFFECT_SOURCE_HAZARD | KF_EFFECT_TARGET_ACTORS_AND_PLAYER, kind,
                                &position, &direction);
        break;
    case KF_EFFECT_KIND_26:
    case KF_EFFECT_KIND_27:
        actor_compute_target_direction(current, player, 300, &position, &direction,
            KF_ACTOR_PITCH_TRACK_TARGET, 0x400, 1);
        effect_construct_record(damage_multiplier_tenths,
                                KF_EFFECT_SOURCE_HAZARD | KF_EFFECT_TARGET_ACTORS_AND_PLAYER, kind,
                                &position, &direction);
        break;
    case KF_EFFECT_KIND_12:
        /* Retail aims at the two-vertex prediction even for other position modes. */
        vector_displacement_to_pitch_yaw(predicted.vx - position.vx,
                      predicted.vy - position.vy,
                      predicted.vz - position.vz,
                      &orientation.angles);
        pitch_yaw_to_forward_vector(&orientation.angles, &direction);
        vector3s_scale_shift12(20, &direction);
        effect_construct_record(damage_multiplier_tenths,
                                KF_EFFECT_SOURCE_HAZARD | KF_EFFECT_TARGET_ACTORS_AND_PLAYER, kind, &position, &direction,
                      &orientation.angles, 500, 0x3c, 0x80, 0x50, 0x8c);
        break;
    case KF_EFFECT_KIND_120:
        position.vx = (rand() >> 2) + player_state.camera_position.vx - 4096;
        position.vz = (rand() >> 2) + player_state.camera_position.vz - 4096;
        position.vy = player_state.camera_position.vy - 5000;
        effect_construct_record(damage_multiplier_tenths,
                                KF_EFFECT_SOURCE_HAZARD | KF_EFFECT_TARGET_ACTORS_AND_PLAYER, kind,
                                &position, NULL);
        effect_construct_record(damage_multiplier_tenths,
                                KF_EFFECT_SOURCE_HAZARD | KF_EFFECT_TARGET_ACTORS_AND_PLAYER, kind,
                                &position, NULL);
        break;
    case KF_EFFECT_KIND_110:
        group_index = ((const u16 *)arguments[1])[2];
        spawned = actor_pool_find_free();
        if (spawned != NULL) {
            KfTargetGroup *group;

            actor_compute_target_direction(current, player, 400,
                          &position, &direction, KF_ACTOR_PITCH_TRACK_TARGET, 0x400, 1);
            group = &actor_state.target_groups[group_index];
            spawned->slot_state = KF_ACTOR_SLOT_EFFECT_SPAWNED;
            spawned->group_index = group_index;
            spawned->unknown_04 = 0;
            spawned->placement_flags = 0;
            spawned->current_map_layer = current->home_map_layer;
            spawned->lifecycle = KF_ACTOR_LIFECYCLE_ACTIVE;
            spawned->flags = group->initial_actor_flags;
            spawned->render_depth = group->render_depth;
            spawned->position.vx = position.vx;
            spawned->position.vy = position.vy + 4096;
            spawned->position.vz = position.vz;
            actor_initialize_from_group(spawned);
            spawned->motion.vector = direction;
            *(KfActorOrientation *)&spawned->rotation =
                *(const KfActorOrientation *)&current->rotation;
            actor_select_target_type_in_own_group(spawned, KF_ACTOR_TARGET_ASCENDING_SPIN);
        }
        break;
    case KF_EFFECT_KIND_112:
        group_index = ((const u16 *)arguments[1])[2];
        spawned = actor_pool_find_free();
        if (spawned != NULL) {
            KfTargetGroup *group;

            actor_compute_target_direction(current, player, 250,
                          &position, &direction, KF_ACTOR_PITCH_TRACK_TARGET, 0x400, 1);
            group = &actor_state.target_groups[group_index];
            spawned->slot_state = KF_ACTOR_SLOT_EFFECT_SPAWNED;
            spawned->group_index = group_index;
            spawned->unknown_04 = 0;
            spawned->placement_flags = 0;
            spawned->current_map_layer = current->home_map_layer;
            spawned->lifecycle = KF_ACTOR_LIFECYCLE_ACTIVE;
            spawned->flags = group->initial_actor_flags;
            spawned->render_depth = group->render_depth;
            spawned->position.vx = position.vx;
            spawned->position.vy = position.vy + (group->collision_height >> 1);
            spawned->position.vz = position.vz;
            actor_initialize_from_group(spawned);
            spawned->motion.vector = direction;
            *(KfActorOrientation *)&spawned->rotation =
                *(const KfActorOrientation *)&current->rotation;
            actor_select_target_type_in_own_group(spawned, KF_ACTOR_TARGET_COLLISION_MOVE);
        }
        break;
    case KF_EFFECT_KIND_29:
    case KF_EFFECT_KIND_31:
        trajectory_target = *player;
        count = 6;
        do {
            distance = fixed_vector2_length(trajectory_target.vx - position.vx,
                                            trajectory_target.vz - position.vz);
            if (trajectory_solve_time_angle(KF_TRAJECTORY_SHORTER_TIME, distance,
                    position.vy - (trajectory_target.vy - 1400), 10, 800,
                    &travel_time, &trajectory_angle) != KF_TRAJECTORY_SOLVED) {
                trajectory_angle = 0x100;
            }
            count--;
            if (count != 0) {
                vector_add_scaled_delta(player, &player_state.frame_displacement,
                              travel_time >> 6, &trajectory_target);
            }
        } while (count != 0);
        orientation.motion.vy = actor_compute_target_direction(current, &trajectory_target,
                                              800, &position, &direction,
                                              trajectory_angle, 0xc00, 1);
        orientation.motion.vx = trajectory_angle;
        orientation.motion.vz = 0;
        effect = effect_construct_record(damage_multiplier_tenths,
                                         KF_EFFECT_SOURCE_HAZARD | KF_EFFECT_TARGET_ACTORS_AND_PLAYER, kind, &position, &direction,
                              &orientation.motion);
        if (effect != NULL) {
            effect->cache_tail.payload.ballistic.origin_y = position.vy;
            effect->updates_remaining = 0x32;
            effect->phase = 0;
        }
        break;
    default:
        break;
    }
}




enum {
    KF_TARGET_SOUND_BASE_ID = 96,
    KF_TARGET_SOUND_ALTERNATE_RANGE = 0x80,
    KF_TARGET_SOUND_INDEX_MASK = 0x7f,
    KF_TARGET_SOUND_TRIGGER_MODE_MASK = 0xc000,
    KF_TARGET_SOUND_TRIGGER_ANIMATION_PHASE = 0,
    KF_TARGET_SOUND_TRIGGER_STAGGERED = 0x4000,
    KF_TARGET_SOUND_TRIGGER_RANDOM = 0x8000
};

ADDRESS(0x8003ddbc, 0x64)
s32 actor_sound_note_offset(KfActor *actor)
{
    s32 offset = 16 - actor->model_scale_y.bytes.high;

    if (offset > 12) {
        offset = 12;
    } else if (offset < -12) {
        offset = -12;
    }
    return offset + (((rand() * 5) >> 15) - 2);
}

ADDRESS(0x8003de20, 0x9c)
void actor_play_target_sound(KfActor *actor)
{
    KfTargetCandidate *target = actor->target;

    if (target->sound_code & KF_TARGET_SOUND_ALTERNATE_RANGE) {
        audio_play_spatial_range((target->sound_code & KF_TARGET_SOUND_INDEX_MASK) + KF_TARGET_SOUND_BASE_ID,
            &actor->position, 0x7f, 0x6000, 0x7800,
            actor_sound_note_offset(actor));
    } else {
        audio_play_spatial_default_range(target->sound_code + KF_TARGET_SOUND_BASE_ID,
            &actor->position, 0x6e, actor_sound_note_offset(actor));
    }
}

ADDRESS(0x8003debc, 0x248c)
void actor_update_behavior(void)
{
    KfActor *actor = actor_state.current;
    KfTargetGroup *group = actor_state.active_group;
    KfTargetCandidate *target = actor->target;
    s32 interval;

    if ((actor->flags & KF_ACTOR_FLAG_STATIC_COLLISION_ONLY) != KF_ACTOR_FLAGS_NONE) {
        actor_state.actor_collision_query_flags =
            KF_COLLISION_QUERY_SHAPES | KF_COLLISION_QUERY_LAYER_FLAG_40;
    } else {
        actor_state.actor_collision_query_flags =
            KF_COLLISION_QUERY_SHAPES | KF_COLLISION_QUERY_LAYER_FLAG_40 |
            KF_COLLISION_QUERY_ACTORS | KF_COLLISION_QUERY_PLAYER;
    }
    map_cell_add_layer_occupancy(actor->position.vx, actor->position.vz,
                   actor->collision_radius, -1);

    if (target->sound_code != KF_AUDIO_SOUND_NONE) {
        interval = target->sound_trigger.fields.interval;
        switch (target->sound_trigger.value & KF_TARGET_SOUND_TRIGGER_MODE_MASK) {
        case KF_TARGET_SOUND_TRIGGER_ANIMATION_PHASE:
            if (actor_animation_crossed_phase(actor, interval)) {
                actor_play_target_sound(actor);
            }
            break;
        case KF_TARGET_SOUND_TRIGGER_RANDOM:
            if ((rand() >> 3) < interval) {
                actor_play_target_sound(actor);
            }
            break;
        case KF_TARGET_SOUND_TRIGGER_STAGGERED: {
            s32 phase = (interval * actor_state.current_actor_slot_index / 3) % interval;

            if ((s32)actor_state.actor_update_frame_count % interval == phase) {
                actor_play_target_sound(actor);
            }
            break;
        }
        }
    }

    switch (actor->target_type) {
    case KF_ACTOR_TARGET_2:
        if (actor->target_action_state == KF_ACTOR_TARGET_ACTION_ENTRY) {
            actor->target_action_state = KF_ACTOR_TARGET_ACTION_RETARGET_BLOCKED;
            actor_set_animation(target->animation_id);
        }
        if (actor->animation_phase < 0x800 ||
            (actor->flags & KF_ACTOR_FLAG_EFFECT_ANIMATION_HOLD) == KF_ACTOR_FLAGS_NONE) {
            actor_advance_animation_clamped(actor, target->animation_step);
        }
        if (actor->animation_phase > 0xffe) {
            actor_reset_target_and_reselect();
        }
        actor_damp_horizontal_motion(group->movement_step * 2,
                                     KF_ACTOR_MOVE_STORE_MOTION | KF_ACTOR_MOVE_SLIDE);
        break;
    case KF_ACTOR_TARGET_3: {
        s32 old_state = actor->state_70.signed_state;

        if (actor->target_action_state == KF_ACTOR_TARGET_ACTION_ENTRY) {
            actor->target_action_state = KF_ACTOR_TARGET_ACTION_RETARGET_BLOCKED;
            actor_set_animation(target->animation_id);
            actor->state_70.signed_state = 0;
            actor->flags &= ~KF_ACTOR_FLAG_MAP_OBJECT_ATTACHED;
        }
        if (actor->state_70.signed_state == 0) {
            if (actor->animation_phase >= 0x400 &&
                (actor->flags & KF_ACTOR_FLAG_EFFECT_ANIMATION_HOLD) != KF_ACTOR_FLAGS_NONE) {
                break;
            }
            actor_advance_animation_clamped(actor, target->animation_step);
            if (actor_animation_crossed_phase(actor, 0x800)) {
                u16 effect_id = group->scattered_effect_id_center +
                    random_centered_triangular_scaled(group->scattered_effect_id_center);

                if (effect_id != 0) {
                    map_object_spawn_scattered_effect(effect_id, &actor->position,
                                   -(actor->collision_height >> 1));
                }
                if (actor->slot_state == KF_ACTOR_SLOT_0
                    || actor->slot_state == KF_ACTOR_SLOT_LINKED_COMPANION) {
                    if (target->word_0c.death_drop.object_id != KF_OBJECT_NONE &&
                        (rand() >> 7) < target->word_0c.death_drop.chance) {
                        map_object_spawn_effect(
                            KF_MAP_OBJECT_DROP_FROM_DEFINITION,
                                target->word_0c.death_drop.object_id, &actor->position,
                            -(actor->collision_height >> 1));
                    }
                } else if (actor->slot_state == KF_ACTOR_SLOT_PERSISTENT &&
                           actor->death_drop_object_id != KF_OBJECT_NONE) {
                    map_object_spawn_effect(
                        KF_MAP_OBJECT_DROP_FROM_PLACEMENT,
                            actor->death_drop_object_id, &actor->position,
                        -(actor->collision_height >> 1));
                }
            }
            if (actor->animation_phase >= KF_ACTOR_ANIMATION_PHASE_MAX ||
                target->animation_step == 0) {
                actor->state_70.signed_state = 15;
            }
        } else {
            s32 current_state = actor->state_70.signed_state;

            if (current_state == 99) {
                break;
            }
            actor->state_70.signed_state++;
            if (old_state < 20) {
                actor_damp_horizontal_motion(group->movement_step * 2,
                                             KF_ACTOR_MOVE_STORE_MOTION | KF_ACTOR_MOVE_SLIDE);
                break;
            }
            if (old_state == 20) {
                actor->lighting_override = KF_LIGHTING_PRESET_42;
                actor->lighting_blend = 0x400;
                actor->render_mode = KF_RENDER_QUEUE_BLEND_ADD;
            } else if (old_state < 38) {
                if (actor->lighting_blend < 0x1000) {
                    actor->lighting_blend += 192;
                } else {
                    actor->lighting_blend = 0x1000;
                }
            } else {
                ((void (*)(KfActor *))resource_state.active_table[19])(actor);
                if (actor->slot_state == KF_ACTOR_SLOT_PERSISTENT) {
                    actor_set_lifecycle_and_home_position(actor);
                } else if (actor->slot_state == KF_ACTOR_SLOT_RESPAWNING) {
                    actor->lifecycle = KF_ACTOR_LIFECYCLE_DORMANT;
                    actor_set_home_position(actor);
                } else if (actor->slot_state == KF_ACTOR_SLOT_EFFECT_SPAWNED) {
                    actor->slot_state = KF_ACTOR_SLOT_FREE;
                    actor->lifecycle = KF_ACTOR_LIFECYCLE_DORMANT;
                } else {
                    actor->lifecycle = KF_ACTOR_LIFECYCLE_WAIT_FOR_RANGE_EXIT;
                    actor_set_home_position(actor);
                }
            }
        }
        actor_damp_horizontal_motion(group->movement_step * 2,
                                     KF_ACTOR_MOVE_STORE_MOTION | KF_ACTOR_MOVE_SLIDE);
        break;
    }
    case KF_ACTOR_TARGET_0:
        if (actor->target_action_state == KF_ACTOR_TARGET_ACTION_ENTRY) {
            actor->target_action_state = KF_ACTOR_TARGET_ACTION_RETARGET_BLOCKED;
            actor_set_animation_if_changed(target->animation_id);
        }
        actor_advance_animation_wrapped(actor, target->animation_step);
        if (angle_within_tolerance(actor->animation_phase, 0,
                                   target->animation_step - 1)) {
            actor->target_action_state = KF_ACTOR_TARGET_ACTION_RETARGET_ALLOWED;
        }
        actor_damp_horizontal_motion(group->movement_step, KF_ACTOR_MOVE_STORE_MOTION | KF_ACTOR_MOVE_SLIDE);
        break;
    case KF_ACTOR_TARGET_1:
        if (actor->target_action_state == KF_ACTOR_TARGET_ACTION_ENTRY) {
            actor->target_action_state = KF_ACTOR_TARGET_ACTION_RETARGET_ALLOWED;
            actor_set_animation_if_changed(target->animation_id);
            actor->movement_yaw = rand() >> KF_RANDOM_ANGLE_SHIFT;
        } else if (actor_turn_and_move_along_heading(actor->movement_yaw,
                                  target->word_0c.value, target->word_0e.value,
                                  group->movement_step,
                                  group->turn_acceleration,
                                  KF_ACTOR_MOVE_AVOID_LEDGE | KF_ACTOR_MOVE_SLIDE_KEEP_SPEED) !=
                       KF_COLLISION_HIT_NONE ||
                   (rand() >> 5) < target->word_10.bytes.fallback_offset) {
            actor->movement_yaw = rand() >> KF_RANDOM_ANGLE_SHIFT;
        }
        actor_advance_animation_wrapped(actor, target->animation_step);
        break;
    case KF_ACTOR_TARGET_12:
    case KF_ACTOR_TARGET_16: {
        KF_ENUM_PARAM(KfActorEulerMoveResult, s32) motion_flags;

        if (actor->target_action_state == KF_ACTOR_TARGET_ACTION_ENTRY) {
            actor->target_action_state = KF_ACTOR_TARGET_ACTION_RETARGET_ALLOWED;
            actor->tail_72.angles.z = 0;
            actor->tail_72.angles.y = 0;
            actor->tail_72.angles.x = 0;
            actor->tail_72.motion.baseline = actor->vertical_anchor_offset +
                collision_sample_map_layer_height(actor->home_map_layer,
                    (actor->home_cell_x << KF_MAP_CELL_POSITION_SHIFT) + actor->word_24.home_local_x,
                    (actor->home_cell_z << KF_MAP_CELL_POSITION_SHIFT) + actor->word_22.home_local_z,
                    actor->collision_radius, actor->collision_height);
            actor_set_animation_if_changed(target->animation_id);
            actor_suspend_vertical_motion();
            motion_flags = KF_ACTOR_EULER_BLOCKED_HORIZONTAL | KF_ACTOR_EULER_BLOCKED_VERTICAL;
        } else {
            motion_flags = actor_turn_and_move_along_euler_angles(
                &actor->tail_72.angles, target->word_0c.value,
                target->word_0e.value,
                group->movement_step,
                group->turn_acceleration, KF_ACTOR_MOVE_NO_STEP_UP | KF_ACTOR_MOVE_SLIDE_KEEP_SPEED);
            if ((rand() >> 5) < target->word_12.flight.orientation_change_threshold) {
                motion_flags |= KF_ACTOR_EULER_BLOCKED_HORIZONTAL;
            }
            if ((rand() >> 5) < target->word_12.flight.orientation_change_threshold) {
                motion_flags |= KF_ACTOR_EULER_BLOCKED_VERTICAL;
            }
            if (actor->tail_72.motion.baseline + target->word_10.value <
                actor->position.vy) {
                actor->motion.vector.vy -= target->word_12.flight.vertical_velocity_step;
            } else if (actor->position.vy <
                       actor->tail_72.motion.baseline - target->word_10.value) {
                actor->motion.vector.vy += target->word_12.flight.vertical_velocity_step;
            }
        }
        if ((motion_flags & KF_ACTOR_EULER_BLOCKED_HORIZONTAL) != KF_ACTOR_EULER_MOVE_CLEAR) {
            actor->tail_72.angles.y = rand() >> KF_RANDOM_ANGLE_SHIFT;
        }
        if ((motion_flags & KF_ACTOR_EULER_BLOCKED_VERTICAL) != KF_ACTOR_EULER_MOVE_CLEAR) {
            actor->tail_72.angles.x = (rand() >> 5) - 512;
        }
        if (actor->target_type == KF_ACTOR_TARGET_16) {
            actor_update_motion_animation(target->animation_id,
                           KF_ENUM_DECODE(KfAnimationClip, target->word_14.bytes[0]),
                           KF_ENUM_DECODE(KfAnimationClip, target->word_14.bytes[1]),
                           KF_ENUM_DECODE(KfAnimationClip, target->word_16.bytes.low),
                           KF_ENUM_DECODE(KfAnimationClip,
                                          target->word_16.bytes.high), target->animation_step);
        } else {
            actor_advance_animation_wrapped(actor, target->animation_step);
        }
        break;
    }
    case KF_ACTOR_TARGET_5: {
        s32 distance;
        s32 angle;
        KF_ENUM_PARAM(KfActorMoveFlags, s32) mode;
        KF_ENUM_PARAM(KfCollisionHitFlags, s32) motion_flags;

        if (actor->target_action_state == KF_ACTOR_TARGET_ACTION_ENTRY) {
            actor->target_action_state = KF_ACTOR_TARGET_ACTION_RETARGET_ALLOWED;
            actor_set_animation_if_changed(target->animation_id);
            switch (actor->previous_target_type) {
            case KF_ACTOR_TARGET_4:
            case KF_ACTOR_TARGET_18:
            case KF_ACTOR_TARGET_23:
            case KF_ACTOR_TARGET_24:
                actor->state_70.bytes.low = 1;
                break;
            default:
                actor->state_70.bytes.low = 0;
                break;
            }
            actor->state_70.bytes.high = 0;
        }
        if (rand() < 6000) {
            distance = fixed_vector2_length(
                player_state.camera_position.vx - actor->position.vx,
                player_state.camera_position.vz - actor->position.vz);
            if (distance >= target->word_16.value) {
                actor->state_70.bytes.low = 0;
            } else if (distance <= target->word_14.value) {
                actor->state_70.bytes.low = 1;
            }
        }
        angle = vector_xz_to_angle(
            player_state.camera_position.vx - actor->position.vx,
            player_state.camera_position.vz - actor->position.vz);
        actor->movement_yaw = angle;
        mode = actor->state_70.bytes.high == 0 ? KF_ACTOR_MOVE_AVOID_LEDGE | KF_ACTOR_MOVE_SLIDE_KEEP_SPEED
                                               : KF_ACTOR_MOVE_PROBE_LEDGE | KF_ACTOR_MOVE_SLIDE_KEEP_SPEED;
        if (actor->state_70.bytes.low == 0) {
            motion_flags = actor_turn_and_move_along_heading((s16)angle,
                target->word_0c.value, target->word_0e.value,
                group->movement_step,
                group->turn_acceleration, mode);
        } else {
            motion_flags = actor_move_along_heading((s16)angle + KF_ANGLE_HALF_TURN,
                target->word_0c.value,
                group->movement_step, mode);
        }
        if ((motion_flags & KF_COLLISION_HIT_LEDGE) != KF_COLLISION_HIT_NONE) {
            if (actor->state_70.bytes.high != 0) {
                goto case5_advance;
            }
            if (rand() < target->word_18.value) {
                actor->state_70.bytes.high = 1;
                goto case5_advance;
            }
        } else {
            actor->state_70.bytes.high = 0;
        }
        if (motion_flags != KF_COLLISION_HIT_NONE) {
            actor->state_70.bytes.low = actor->state_70.bytes.low == 0;
        }
    case5_advance:
        actor_advance_animation_wrapped(actor, target->animation_step);
        break;
    }
    case KF_ACTOR_TARGET_13:
    case KF_ACTOR_TARGET_17: {
        s32 delta_x;
        s32 delta_y;
        s32 delta_z;
        s32 distance;
        SVECTOR opposite;

        if (actor->target_action_state == KF_ACTOR_TARGET_ACTION_ENTRY) {
            actor->target_action_state = KF_ACTOR_TARGET_ACTION_RETARGET_BLOCKED;
            actor_set_animation_if_changed(target->animation_id);
            actor->state_70.signed_state = 0;
        }
        delta_x = player_state.camera_position.vx - actor->position.vx;
        delta_y = player_state.camera_position.vy - actor->position.vy - 1600;
        delta_z = player_state.camera_position.vz - actor->position.vz;
        distance = fixed_vector3_length(delta_x, delta_y, delta_z);
        vector_displacement_to_pitch_yaw(delta_x, delta_y, delta_z, &actor->tail_72.angles);
        if (actor->tail_72.angles.x >= 3585) {
            actor->tail_72.angles.x = 3584;
        } else if (actor->tail_72.angles.x > 512) {
            actor->tail_72.angles.x = 512;
        }
        if (distance >= target->word_16.value) {
            actor->state_70.signed_state = 0;
            actor->target_action_state = KF_ACTOR_TARGET_ACTION_RETARGET_ALLOWED;
        } else if (distance <= target->word_14.value) {
            actor->state_70.signed_state = 1;
            actor->target_action_state = KF_ACTOR_TARGET_ACTION_RETARGET_BLOCKED;
        }
        if (actor->state_70.signed_state == 0) {
            actor_turn_and_move_along_euler_angles(&actor->tail_72.angles, target->word_0c.value,
                          target->word_0e.value,
                          group->movement_step,
                          group->turn_acceleration,
                          KF_ACTOR_MOVE_NO_STEP_UP | KF_ACTOR_MOVE_SLIDE_KEEP_SPEED);
        } else {
            opposite.vx = -512;
            opposite.vy = actor->tail_72.angles.y + KF_ANGLE_HALF_TURN;
            opposite.vz = 0;
            actor_turn_and_move_along_euler_angles((struct KfEulerAngles *)&opposite, target->word_0c.value,
                          target->word_0e.value,
                          group->movement_step,
                          group->turn_acceleration,
                          KF_ACTOR_MOVE_NO_STEP_UP | KF_ACTOR_MOVE_SLIDE_KEEP_SPEED);
        }
        if (actor->target_type == KF_ACTOR_TARGET_17) {
            actor_update_motion_animation(target->animation_id,
                           KF_ENUM_DECODE(KfAnimationClip, target->word_18.bytes.low),
                           KF_ENUM_DECODE(KfAnimationClip, target->word_18.bytes.high),
                           KF_ENUM_DECODE(KfAnimationClip, target->word_1a.bytes[0]),
                           KF_ENUM_DECODE(KfAnimationClip, target->word_1a.bytes[1]), target->animation_step);
        } else {
            actor_advance_animation_wrapped(actor, target->animation_step);
        }
        break;
    }
    case KF_ACTOR_TARGET_9:
        if (actor->target_action_state == KF_ACTOR_TARGET_ACTION_ENTRY) {
            actor->target_action_state = KF_ACTOR_TARGET_ACTION_RETARGET_BLOCKED;
            actor->state_70.signed_state = 0;
            actor_set_animation(KF_ENUM_DECODE(KfAnimationClip, target->word_14.bytes[0]));
        }
        switch (actor->state_70.signed_state) {
        case 0:
            if (actor_move_along_heading(actor->rotation.y + KF_ANGLE_HALF_TURN,
                              target->word_12.value,
                              group->movement_step,
                              KF_ACTOR_MOVE_AVOID_LEDGE | KF_ACTOR_MOVE_SLIDE_KEEP_SPEED) ==
                KF_COLLISION_HIT_NONE) {
                actor_advance_animation_wrapped(actor, target->animation_step);
                if (fixed_vector2_length(
                        player_state.camera_position.vx - actor->position.vx,
                        player_state.camera_position.vz - actor->position.vz)
                    < target->word_0e.value) {
                    break;
                }
            }
            actor->state_70.signed_state = 1;
            actor_set_animation(target->animation_id);
            break;
        case 1:
            actor_advance_animation_clamped(actor, target->word_16.bytes.low);
            if (actor->animation_phase >= KF_ACTOR_ANIMATION_PHASE_MAX) {
                if ((s16)actor_start_ballistic_motion(
                        KF_TRAJECTORY_LONGER_TIME, player_state.camera_position.vx,
                        player_state.camera_position.vy - 500,
                        player_state.camera_position.vz,
                        target->word_16.bytes.high,
                        target->word_10.value) <= 0) {
                    actor->state_70.signed_state = 3;
                    actor_set_animation(KF_ENUM_DECODE(KfAnimationClip, target->word_14.bytes[1]));
                    break;
                }
                actor->state_70.signed_state = 2;
            }
            actor_damp_horizontal_motion(group->movement_step * 2,
                                         KF_ACTOR_MOVE_STORE_MOTION | KF_ACTOR_MOVE_SLIDE);
            break;
        case 2:
            if (actor->vertical_motion_state == KF_ACTOR_VERTICAL_MOTION_NONE) {
                actor->state_70.signed_state = 3;
                actor_set_animation(KF_ENUM_DECODE(KfAnimationClip, target->word_14.bytes[1]));
            } else {
                actor_turn_and_move_along_heading(actor->rotation.y,
                              actor->ballistic_horizontal_speed, 0,
                              actor->ballistic_horizontal_speed,
                              group->turn_acceleration, KF_ACTOR_MOVE_STORE_MOTION | KF_ACTOR_MOVE_SLIDE);
            }
            break;
        case 3:
            actor_advance_animation_clamped(actor, target->word_16.bytes.low);
            if (actor->animation_phase >= KF_ACTOR_ANIMATION_PHASE_MAX) {
                actor_reset_target_and_reselect();
            }
            actor_damp_horizontal_motion(actor->ballistic_horizontal_speed >> 4,
                                         KF_ACTOR_MOVE_STORE_MOTION | KF_ACTOR_MOVE_SLIDE);
            break;
        }
        break;
    case KF_ACTOR_TARGET_10: {
        s32 random_value;

        if (actor->target_action_state == KF_ACTOR_TARGET_ACTION_ENTRY) {
            actor->target_action_state = KF_ACTOR_TARGET_ACTION_RETARGET_ALLOWED;
            actor_set_animation_if_changed(target->animation_id);
            actor_suspend_vertical_motion();
        }
        random_value = rand();
        if (random_value < 2048) {
            actor->motion.vector.vx += target->word_10.value;
            if (actor->motion.vector.vx > target->word_0c.value) {
                actor->motion.vector.vx = target->word_0c.value;
            }
        } else if (random_value < 4096) {
            actor->motion.vector.vx -= target->word_10.value;
            if (actor->motion.vector.vx < -(s32)target->word_0c.value) {
                actor->motion.vector.vx = -target->word_0c.value;
            }
        }
        random_value = rand();
        if (random_value < 2048) {
            actor->motion.vector.vz += target->word_10.value;
            if (actor->motion.vector.vz > target->word_0c.value) {
                actor->motion.vector.vz = target->word_0c.value;
            }
        } else if (random_value < 4096) {
            actor->motion.vector.vz -= target->word_10.value;
            if (actor->motion.vector.vz < -(s32)target->word_0c.value) {
                actor->motion.vector.vz = -target->word_0c.value;
            }
        }
        random_value = rand();
        if (random_value < 2048) {
            actor->motion.vector.vy += target->word_10.value;
            if (actor->motion.vector.vy > target->word_0c.value) {
                actor->motion.vector.vy = target->word_0c.value;
            }
        } else if (random_value < 4096) {
            actor->motion.vector.vy -= target->word_10.value;
            if (actor->motion.vector.vy < -(s32)target->word_0c.value) {
                actor->motion.vector.vy = -target->word_0c.value;
            }
        }
        actor_move_with_collision(&actor->motion.vector);
        actor_advance_animation_wrapped(actor, target->animation_step);
        actor_turn_toward_angle(actor, vector_xz_to_angle(actor->motion.vector.vx,
                                                actor->motion.vector.vz),
                        target->word_0e.value,
                        group->turn_acceleration);
        break;
    }
    case KF_ACTOR_TARGET_4:
        if (actor->target_action_state == KF_ACTOR_TARGET_ACTION_ENTRY) {
            actor->target_action_state = KF_ACTOR_TARGET_ACTION_RETARGET_BLOCKED;
            actor_set_animation(target->animation_id);
        }
        actor_advance_animation_clamped(actor, target->animation_step);
        if (actor_animation_crossed_phase(actor, target->word_18.value)) {
            actor_try_damage_player_in_cone(0, target->word_0e.bytes.low,
                           target->word_0e.bytes.high,
                           target->word_10.bytes.fallback_offset,
                           target->word_12.value, target->word_14.value,
                           target->word_16.value,
                           target->word_10.attack.damage_flags);
        }
        if (actor->animation_phase >= KF_ACTOR_ANIMATION_PHASE_MAX) {
            actor_reset_target_and_reselect();
        }
        actor_damp_horizontal_motion(target->word_0c.bytes.high,
                                     KF_ACTOR_MOVE_STORE_MOTION | KF_ACTOR_MOVE_SLIDE);
        break;
    case KF_ACTOR_TARGET_23:
        if (actor->target_action_state == KF_ACTOR_TARGET_ACTION_ENTRY) {
            actor->target_action_state = KF_ACTOR_TARGET_ACTION_RETARGET_BLOCKED;
            actor->state_70.signed_state = target->word_18.value;
            actor_set_animation(target->animation_id);
        }
        actor_advance_animation_clamped(actor, target->animation_step);
        if (actor_animation_crossed_phase(actor, actor->state_70.signed_state)) {
            actor->state_70.signed_state += target->repeated_attack_phase_step;
            if (target->word_26.unsigned_value < actor->state_70.signed_state) {
                actor->state_70.signed_state = 0;
            }
            actor_try_damage_player_in_cone(0, target->word_0e.bytes.low,
                           target->word_0e.bytes.high,
                           target->word_10.bytes.fallback_offset,
                           target->word_12.value, target->word_14.value,
                           target->word_16.value,
                           target->word_10.attack.damage_flags | KF_PLAYER_DAMAGE_REACTION_ROTATION_ONLY);
        }
        if (target->secondary_hit_phase != 0 &&
            actor_animation_crossed_phase(actor, target->secondary_hit_phase)) {
            actor_try_damage_player_in_cone(0, target->word_1c.bytes[0],
                           target->word_1c.bytes[1],
                           target->word_1e.bytes[0], target->word_20.damage_component0,
                           target->word_22.damage_component1, target->word_24.unsigned_value,
                           target->word_1e.bytes[1]);
        }
        if (actor->animation_phase >= KF_ACTOR_ANIMATION_PHASE_MAX) {
            actor_reset_target_and_reselect();
        }
        actor_damp_horizontal_motion(target->word_0c.bytes.high,
                                     KF_ACTOR_MOVE_STORE_MOTION | KF_ACTOR_MOVE_SLIDE);
        break;
    case KF_ACTOR_TARGET_24: {
        s32 step;
        s32 speed;
        s32 angle;
        if (actor->target_action_state == KF_ACTOR_TARGET_ACTION_ENTRY) {
            actor->target_action_state = KF_ACTOR_TARGET_ACTION_RETARGET_BLOCKED;
            actor_set_animation(target->animation_id);
        }
        actor_advance_animation_clamped(actor, target->animation_step);
        if (actor->animation_phase >= target->word_22.animation_phase_stop) {
            speed = 0;
            step = target->word_26.signed_value;
        } else if (actor->animation_phase >= target->word_20.animation_phase_start) {
            speed = target->word_1c.value;
            step = target->word_24.signed_value;
        } else {
            speed = target->word_1c.value;
            step = target->word_0c.bytes.high;
        }
        angle = vector_xz_to_angle(
            player_state.camera_position.vx - actor->position.vx,
            player_state.camera_position.vz - actor->position.vz);
        actor->movement_yaw = angle;
        actor_turn_and_move_along_heading(actor->movement_yaw, speed,
                      target->word_1e.value, step,
                      group->turn_acceleration, KF_ACTOR_MOVE_AVOID_LEDGE);
        if (actor_animation_crossed_phase(actor, target->word_18.value)) {
            actor_try_damage_player_in_cone(0, target->word_0e.bytes.low,
                           target->word_0e.bytes.high,
                           target->word_10.bytes.fallback_offset,
                           target->word_12.value, target->word_14.value,
                           target->word_16.value,
                           target->word_10.attack.damage_flags);
        }
        if (actor->animation_phase >= KF_ACTOR_ANIMATION_PHASE_MAX) {
            actor_reset_target_and_reselect();
        }
        break;
    }
    case KF_ACTOR_TARGET_11: {
        if (actor->target_action_state == KF_ACTOR_TARGET_ACTION_ENTRY) {
            actor->target_action_state = KF_ACTOR_TARGET_ACTION_RETARGET_BLOCKED;
            actor_set_animation_if_changed(target->animation_id);
            actor->state_70.signed_state = 0;
            actor_suspend_vertical_motion();
        }
        switch (actor->state_70.signed_state) {
        case 0:
            actor_advance_animation_clamped(actor, target->animation_step);
            if (actor->animation_phase >= KF_ACTOR_ANIMATION_PHASE_MAX) {
                SVECTOR toward_player;

                vector_displacement_to_pitch_yaw(
                    player_state.camera_position.vx - actor->position.vx,
                    player_state.camera_position.vy - actor->position.vy,
                    player_state.camera_position.vz - actor->position.vz,
                    (struct KfEulerAngles *)&toward_player);
                pitch_yaw_to_forward_vector((struct KfEulerAngles *)&toward_player,
                                            &actor->tail_72.direction);
                actor->state_70.signed_state = 1;
            }
            break;
        case 1: {
            SVECTOR forward;
            SVECTOR outer;
            KF_ENUM_PARAM(KfCollisionHitFlags, s32) collision;

            outer = actor->tail_72.direction;
            forward = outer;
            vector3s_scale_shift12(target->word_14.value, &forward);
            vector3s_scale_shift12(target->word_18.value, &outer);
            actor->motion.vector.vx = value_approach(actor->motion.vector.vx,
                                                forward.vx, outer.vx);
            actor->motion.vector.vy = value_approach(actor->motion.vector.vy,
                                                forward.vy, outer.vy);
            actor->motion.vector.vz = value_approach(actor->motion.vector.vz,
                                                forward.vz, outer.vz);
            collision = actor_move_with_collision(&actor->motion.vector);
            if (collision != KF_COLLISION_HIT_NONE) {
                if ((collision & KF_COLLISION_HIT_PLAYER) != KF_COLLISION_HIT_NONE) {
                    player_apply_damage(target->word_0e.value,
                                   target->word_10.value,
                                   target->word_12.value,
                                   target->word_0c.bytes.high,
                                   0, 0, 0, 0, 0, 0x1000, 10,
                                   &actor->position);
                }
                actor->state_70.signed_state = 2;
            }
            actor->rotation.x = (actor->rotation.x + 256) & KF_ANGLE_WRAP_MASK;
            goto case11_turn;
        }
        case 2: {
            u32 pitch_phase = ((u16)actor->rotation.x + 256) & KF_ANGLE_WRAP_MASK;
            actor->rotation.x = pitch_phase;
            if (pitch_phase < 256) {
                actor->rotation.x = 0;
                actor->state_70.signed_state = 3;
            }
            break;
        }
        case 3:
            actor_advance_animation_clamped(actor, -target->animation_step);
            if (actor->animation_phase == 0) {
                actor_reset_target_and_reselect();
            }
            break;
        default:
            goto case11_turn;
        }
        actor_move_with_collision(&actor->motion.vector);
    case11_turn:
        actor_turn_toward_angle(actor, vector_xz_to_angle(actor->motion.vector.vx,
                                                actor->motion.vector.vz),
                        target->word_16.value,
                        group->turn_acceleration);
        break;
    }
    case KF_ACTOR_TARGET_18: {
        s32 delta_x;
        s32 delta_y;
        s32 delta_z;

        if (actor->target_action_state == KF_ACTOR_TARGET_ACTION_ENTRY) {
            actor->target_action_state = KF_ACTOR_TARGET_ACTION_RETARGET_BLOCKED;
            actor_set_animation_if_changed(target->animation_id);
        }
        delta_x = player_state.camera_position.vx - actor->position.vx;
        delta_y = player_state.camera_position.vy - actor->position.vy - 1600;
        delta_z = player_state.camera_position.vz - actor->position.vz;
        fixed_vector3_length(delta_x, delta_y, delta_z);
        vector_displacement_to_pitch_yaw(delta_x, delta_y, delta_z, &actor->tail_72.angles);
        actor_turn_and_move_along_euler_angles(&actor->tail_72.angles, target->word_1c.value,
                      target->word_1e.value,
                      target->word_0c.bytes.high,
                      group->turn_acceleration, KF_ACTOR_MOVE_AVOID_LEDGE | KF_ACTOR_MOVE_SLIDE);
        actor_advance_animation_clamped(actor, target->animation_step);
        if (actor_animation_crossed_phase(actor, target->word_18.value)) {
            actor_try_damage_player_in_cone(0, target->word_0e.bytes.low,
                           target->word_0e.bytes.high,
                           target->word_10.bytes.fallback_offset,
                           target->word_12.value, target->word_14.value,
                           target->word_16.value,
                           target->word_10.attack.damage_flags);
        }
        if (actor->animation_phase >= KF_ACTOR_ANIMATION_PHASE_MAX) {
            actor_reset_target_and_reselect();
        }
        break;
    }
    case KF_ACTOR_TARGET_14: {
        KfActor *other = actor_state.other_actor;
        if (actor->target_action_state == KF_ACTOR_TARGET_ACTION_ENTRY) {
            actor->target_action_state = KF_ACTOR_TARGET_ACTION_RETARGET_ALLOWED;
            actor->state_70.signed_state = 0;
        }
        switch (actor->state_70.signed_state) {
        case 0:
            if (other->turn_rate > 0) {
                actor_set_animation(KF_ENUM_DECODE(KfAnimationClip, target->word_0c.bytes.high));
                actor->state_70.signed_state = 1;
            } else if (other->turn_rate < 0) {
                actor_set_animation(KF_ENUM_DECODE(KfAnimationClip, target->word_0c.bytes.low));
                actor->state_70.signed_state = 2;
            }
            break;
        case 1:
            if (actor->animation_phase < 2048) {
                actor->animation_phase += 128;
            } else if (other->turn_rate <= 0) {
                actor->state_70.signed_state = 3;
            }
            break;
        case 2:
            if (actor->animation_phase < 2048) {
                actor->animation_phase += 128;
            } else if (other->turn_rate >= 0) {
                actor->state_70.signed_state = 3;
            }
            break;
        case 3:
            actor->animation_phase -= 128;
            if (actor->animation_phase == 0) {
                actor->state_70.signed_state = 0;
            }
            break;
        }
        break;
    }
    case KF_ACTOR_TARGET_15:
        if (actor->target_action_state == KF_ACTOR_TARGET_ACTION_ENTRY) {
            actor->target_action_state = KF_ACTOR_TARGET_ACTION_RETARGET_BLOCKED;
            actor_set_animation_if_changed(target->animation_id);
        }
        actor_turn_and_move_along_heading(actor->movement_yaw,
                      target->word_0c.value, target->word_0e.value,
                      group->movement_step,
                      group->turn_acceleration, KF_ACTOR_MOVE_AVOID_LEDGE | KF_ACTOR_MOVE_SLIDE_KEEP_SPEED);
        actor_advance_animation_clamped(actor, target->animation_step);
        if (actor->animation_phase >= KF_ACTOR_ANIMATION_PHASE_MAX) {
            actor_reset_target_and_reselect();
        }
        break;
    case KF_ACTOR_TARGET_19:
        if (actor->target_action_state == KF_ACTOR_TARGET_ACTION_ENTRY) {
            actor->target_action_state = KF_ACTOR_TARGET_ACTION_RETARGET_BLOCKED;
            actor->state_70.signed_state = 0;
            actor_set_animation_if_changed(target->animation_id);
        }
        switch (actor->state_70.signed_state) {
        case 0:
            actor_advance_animation_clamped(actor, target->animation_step);
            if (actor->animation_phase >= target->word_10.value) {
                actor->state_70.signed_state = 16;
            }
            break;
        case 16:
            if (player_state.weapon_attack_phase == -1) {
                actor->state_70.signed_state = 32;
            }
            break;
        case 32:
        case19_clamped:
            actor_advance_animation_clamped(actor, target->animation_step);
            if (actor->animation_phase >= KF_ACTOR_ANIMATION_PHASE_MAX) {
                actor_reset_target_and_reselect();
            }
            break;
        }
        actor_damp_horizontal_motion(group->movement_step, KF_ACTOR_MOVE_STORE_MOTION | KF_ACTOR_MOVE_SLIDE);
        break;
    case KF_ACTOR_TARGET_20:
        if (actor->target_action_state == KF_ACTOR_TARGET_ACTION_ENTRY) {
            actor->target_action_state = KF_ACTOR_TARGET_ACTION_RETARGET_BLOCKED;
            actor->state_70.signed_state = target->word_12.value;
            actor_set_animation_if_changed(target->animation_id);
        }
        if (actor_move_along_heading(actor->rotation.y + KF_ANGLE_HALF_TURN,
                          target->word_10.value, 1000,
                          KF_ACTOR_MOVE_AVOID_LEDGE | KF_ACTOR_MOVE_SLIDE_KEEP_SPEED) !=
            KF_COLLISION_HIT_NONE) {
            actor->state_70.signed_state = 1;
        }
        actor_advance_animation_wrapped(actor, target->animation_step);
        if (actor->animation_phase >= KF_ACTOR_ANIMATION_PHASE_MAX - target->animation_step) {
            actor->state_70.signed_state--;
            if (actor->state_70.signed_state == 0) {
                actor->animation_phase = KF_ACTOR_ANIMATION_PHASE_MAX;
                actor_reset_target_and_reselect();
            }
        }
        break;
    case KF_ACTOR_TARGET_22:
        if (actor->target_action_state == KF_ACTOR_TARGET_ACTION_ENTRY) {
            actor->target_action_state = KF_ACTOR_TARGET_ACTION_RETARGET_BLOCKED;
            actor_set_animation(target->animation_id);
        }
        goto case19_clamped;
    case KF_ACTOR_TARGET_21:
        if (actor->target_action_state == KF_ACTOR_TARGET_ACTION_ENTRY) {
            actor->target_action_state = KF_ACTOR_TARGET_ACTION_RETARGET_BLOCKED;
            actor->state_70.signed_state = 0;
            actor_set_animation(target->animation_id);
            actor->collision_radius = target->word_14.value;
            actor->collision_height = target->word_16.value;
        }
        switch (actor->state_70.signed_state) {
        case 0:
            if (fixed_vector2_length(player_state.camera_position.vx - actor->position.vx,
                                     player_state.camera_position.vz - actor->position.vz)
                < target->word_0c.value) {
                actor->state_70.signed_state = 1;
            }
            break;
        case 1:
            actor_advance_animation_clamped(actor, target->animation_step);
            actor->collision_radius = fixed_lerp_q12(target->word_14.value,
                group->collision_radius, actor->animation_phase);
            actor->collision_height = fixed_lerp_q12(target->word_16.value,
                group->collision_height, actor->animation_phase);
            if (actor->animation_phase >= KF_ACTOR_ANIMATION_PHASE_MAX) {
                actor->collision_radius = group->collision_radius;
                actor->collision_height = group->collision_height;
                actor->state_70.signed_state = 2;
                actor->tail_72.signed_state = target->word_10.value;
                actor_set_animation(KF_ENUM_DECODE(KfAnimationClip, target->word_18.bytes.low));
            }
            if (actor->animation_phase >= target->word_12.value) {
                KfCollisionQuery saved_flags = actor_state.actor_collision_query_flags;
                actor_state.actor_collision_query_flags &= ~KF_COLLISION_QUERY_MAP_OBJECTS;
                actor_turn_and_move_along_heading(actor->movement_yaw,
                               target->word_0e.value, 0, 0xff, 0,
                               KF_ACTOR_MOVE_AVOID_LEDGE | KF_ACTOR_MOVE_SLIDE_KEEP_SPEED);
                actor_state.actor_collision_query_flags = saved_flags;
            }
            break;
        case 2: {
            KfCollisionQuery saved_flags = actor_state.actor_collision_query_flags;
            s32 next_timer;
            actor_state.actor_collision_query_flags &= ~KF_COLLISION_QUERY_MAP_OBJECTS;
            actor_turn_and_move_along_heading(actor->movement_yaw,
                           target->word_0e.value, 0, 0xff, 0,
                           KF_ACTOR_MOVE_AVOID_LEDGE | KF_ACTOR_MOVE_SLIDE_KEEP_SPEED);
            next_timer = actor->tail_72.unsigned_state - 1;
            actor->tail_72.unsigned_state = next_timer;
            if ((s16)next_timer == -1) {
                actor_reset_target_and_reselect();
            }
            actor_state.actor_collision_query_flags = saved_flags;
            actor_advance_animation_wrapped(actor, target->word_18.bytes.high);
            break;
        }
        }
        break;
    case KF_ACTOR_TARGET_26:
        if (actor->target_action_state == KF_ACTOR_TARGET_ACTION_ENTRY) {
            actor->target_action_state = KF_ACTOR_TARGET_ACTION_RETARGET_BLOCKED;
            actor->state_70.signed_state = 0;
            actor_set_animation(target->animation_id);
            actor->model_scale_z = 0;
            actor->model_scale_y.value = 0;
            actor->model_scale_x = 0;
        }
        switch (actor->state_70.signed_state) {
        case 0: {
            KfMapObject *object = &map_object_state.objects[actor->word_20.linked_map_object_slot];
            u16 group_scale;

            if (object->action_timer < 2 || object->rotation.vx == 0 ||
                object->rotation.vx >= 3072) {
                break;
            }
            actor->state_70.signed_state = 1;
            actor->tail_72.signed_state = 8;
            actor->collision_radius = group->collision_radius;
            actor->collision_height = group->collision_height;
            actor->position.vy += 2048;
            group_scale = group->initial_model_scale_q12;
            actor->model_scale_z = group_scale;
            actor->model_scale_y.value = group_scale;
            actor->model_scale_x = group_scale;
            actor->rotation.y = object->rotation.vy;
        }
            /* fall through */
        case 1:
            actor->vertical_motion_state = KF_ACTOR_VERTICAL_MOTION_NONE;
            goto case29_shared_motion;
        }
        break;
    case KF_ACTOR_TARGET_25: {
        const u16 *cursor;
        s32 repeat;

        if (actor->target_action_state == KF_ACTOR_TARGET_ACTION_ENTRY) {
            actor->target_action_state = KF_ACTOR_TARGET_ACTION_RETARGET_BLOCKED;
            actor_set_animation(target->animation_id);
            actor->state_70.signed_state = target->word_0e.value;
            actor->tail_72.script.word_index = 0;
            actor->tail_72.script.effect_cycle_index = 0;
            if (target->start_vertical_motion_on_entry == 1) {
                actor->vertical_motion_state = KF_ACTOR_VERTICAL_MOTION_VELOCITY;
            }
        }
        actor_advance_animation_clamped(actor, target->animation_step);
        if (actor_animation_crossed_phase(actor,
                                          actor->state_70.signed_state)) {
            actor->state_70.signed_state += target->word_12.value;
            repeat = 1;
            if (target->word_10.value < actor->state_70.signed_state) {
                actor->state_70.signed_state = 0;
            }
            cursor = ((const KfTargetCandidateAction25 *)target)->stream +
                (s16)actor->tail_72.script.word_index;
            for (;;) {
                s32 opcode = *cursor++;
                u16 index = actor->tail_72.script.word_index;
                actor->tail_72.script.word_index = index + 1;

                if (opcode == ACTOR_SCRIPT_REWIND) {
                    cursor = ((const KfTargetCandidateAction25 *)target)->stream;
                    actor->tail_72.script.word_index = 0;
                    continue;
                } else if (opcode == ACTOR_SCRIPT_REPEAT) {
                    repeat = *cursor++;
                    actor->tail_72.script.word_index = index + 2;
                    continue;
                } else if (opcode == ACTOR_SCRIPT_SKIP) {
                    u16 skip = *cursor;
                    cursor += skip + 1;
                    continue;
                } else if (opcode == ACTOR_SCRIPT_ROTATED_OFFSET_EFFECT) {
                    s16 x;
                    s16 y;
                    s16 z;

                    x = *cursor++;
                    actor->tail_72.script.word_index = index + 2;
                    y = *cursor++;
                    actor->tail_72.script.word_index = index + 3;
                    z = *cursor++;
                    actor->tail_72.script.word_index = index + 4;
                    actor_dispatch_group_effect(target->word_0c.script_effect.kind,
                                   target->word_18.value, ACTOR_EFFECT_POSITION_ROTATED_OFFSET,
                                   x, y, z, cursor);
                } else if (opcode == ACTOR_SCRIPT_TWO_VERTEX_EFFECT) {
                    u16 first = *cursor++;
                    u16 second = *cursor++;
                    u16 third = *cursor++;
                    actor->tail_72.script.word_index = index + 4;
                    actor_dispatch_group_effect(target->word_0c.script_effect.kind,
                                   target->word_18.value, ACTOR_EFFECT_POSITION_TWO_VERTICES,
                                   first, second, third);
                } else {
                    actor_dispatch_group_effect(target->word_0c.script_effect.kind,
                                   target->word_18.value, opcode,
                                   cursor + repeat - 1,
                                   (s16)actor->tail_72.script.effect_cycle_index);
                }
                if (--repeat == 0) {
                    break;
                }
            }
            actor->tail_72.script.effect_cycle_index++;
        }
        if (actor->animation_phase >= KF_ACTOR_ANIMATION_PHASE_MAX) {
            actor_reset_target_and_reselect();
        }
        actor_damp_horizontal_motion(group->movement_step, KF_ACTOR_MOVE_STORE_MOTION | KF_ACTOR_MOVE_SLIDE);
        break;
    }
    case KF_ACTOR_TARGET_27:
        if (actor->target_action_state == KF_ACTOR_TARGET_ACTION_ENTRY) {
            actor->target_action_state = KF_ACTOR_TARGET_ACTION_RETARGET_ALLOWED;
            actor->state_70.signed_state = 0;
            actor->tail_72.signed_state = KF_ACTOR_HEADING_NONE;
            actor_set_animation(KF_ENUM_DECODE(KfAnimationClip, target->word_16.bytes.low));
        }
        switch (actor->state_70.signed_state) {
        case 0:
            if (target->word_0e.value == 0) {
                actor->state_70.signed_state = 1;
            } else {
                actor->tail_72.signed_state = actor_turn_and_move_toward_point(
                    (actor->home_cell_x << KF_MAP_CELL_POSITION_SHIFT) + actor->word_24.home_local_x,
                    (actor->home_cell_z << KF_MAP_CELL_POSITION_SHIFT) + actor->word_22.home_local_z,
                    target->word_0e.value, target->word_10.value,
                    actor->tail_72.signed_state,
                    group->movement_step,
                    group->turn_acceleration, KF_ACTOR_MOVE_AVOID_LEDGE | KF_ACTOR_MOVE_SLIDE_KEEP_SPEED);
                if (actor->tail_72.signed_state == KF_ACTOR_HEADING_NONE) {
                    actor->state_70.signed_state = 1;
                }
            }
            actor_advance_animation_wrapped(actor, target->word_16.bytes.high);
            break;
        case 1:
            actor_turn_toward_angle(actor, actor->word_20.home_yaw, target->word_10.value,
                          group->turn_acceleration);
            if (actor->rotation.y == actor->word_20.home_yaw) {
                actor_set_animation(target->animation_id);
                actor->state_70.signed_state = 2;
            }
            break;
        case 2: {
            KfTargetCandidate *next_target;
            actor_advance_animation_clamped(actor, target->animation_step);
            actor->collision_radius = fixed_lerp_q12(
                group->collision_radius,
                target->word_12.value, actor->animation_phase);
            actor->collision_height = fixed_lerp_q12(
                group->collision_height,
                target->word_14.value, actor->animation_phase);
            if (actor->animation_phase >= KF_ACTOR_ANIMATION_PHASE_MAX) {
                actor->collision_radius = target->word_12.value;
                actor->collision_height = target->word_14.value;
                next_target = actor_find_target_of_type(group, KF_ACTOR_TARGET_21);
                if (next_target != NULL) {
                    actor_set_target(actor, next_target);
                } else {
                    actor_reset_target_and_reselect();
                }
            }
            break;
        }
        }
        break;
    case KF_ACTOR_TARGET_ASCENDING_SPIN:
        if (actor->target_action_state == KF_ACTOR_TARGET_ACTION_ENTRY) {
            actor->target_action_state = KF_ACTOR_TARGET_ACTION_RETARGET_BLOCKED;
            actor_set_animation(target->animation_id);
            actor_suspend_vertical_motion();
            actor->tail_72.signed_state = 16;
        }
        actor->rotation.x += 128;
        actor->rotation.y += 64;
    case29_shared_motion: {
        s32 next_y = actor->position.vy - 256;
        s32 next_timer = actor->tail_72.unsigned_state - 1;

        actor->position.vy = next_y;
        actor->tail_72.unsigned_state = next_timer;
        if ((s16)next_timer == 0) {
            actor_reset_target_and_reselect();
        }
        break;
    }
    case KF_ACTOR_TARGET_28: {
        KF_ENUM_PARAM(KfCollisionHitFlags, s32) collision;

        if (actor->target_action_state == KF_ACTOR_TARGET_ACTION_ENTRY) {
            actor->target_action_state = KF_ACTOR_TARGET_ACTION_RETARGET_BLOCKED;
            actor_set_animation(target->animation_id);
            actor_suspend_vertical_motion();
            audio_play_spatial_default_range(0x1b, &actor->position, 120, 0);
        }
        actor->rotation.x += 128;
        actor->rotation.y += 64;
        collision = actor_move_with_collision(&actor->motion.vector);
        if (collision != KF_COLLISION_HIT_NONE) {
            if ((collision & KF_COLLISION_HIT_PLAYER) != KF_COLLISION_HIT_NONE) {
                player_apply_damage(target->word_0e.value, target->word_10.value,
                               target->word_12.value, target->word_0c.value,
                               target->word_14.value, target->word_16.value,
                               target->word_18.value, target->word_1a.value,
                               target->word_1c.value, 0x1000, 10, &actor->position);
            }
            actor_select_target_type_in_own_group(actor, KF_ACTOR_TARGET_3);
            actor->motion.vector.vz = 0;
            actor->motion.vector.vy = 0;
            actor->motion.vector.vx = 0;
            actor->vertical_motion_state = KF_ACTOR_VERTICAL_MOTION_VELOCITY;
        }
        break;
    }
    case KF_ACTOR_TARGET_COLLISION_MOVE: {
        VECTOR next;
        KF_ENUM_PARAM(KfCollisionHitFlags, s32) collision;

        if (actor->target_action_state == KF_ACTOR_TARGET_ACTION_ENTRY) {
            actor->target_action_state = KF_ACTOR_TARGET_ACTION_RETARGET_BLOCKED;
            actor_set_animation(target->animation_id);
            actor_suspend_vertical_motion();
        }
        next.vx = actor->position.vx + actor->motion.vector.vx;
        next.vy = actor->position.vy + actor->motion.vector.vy;
        next.vz = actor->position.vz + actor->motion.vector.vz;
        collision = collision_query_world(
            next.vx, next.vy, next.vz, actor->collision_radius,
            actor->collision_height | (KF_ENUM_ENCODE(u32, actor->flags & KF_ACTOR_FLAG_COLLISION_HEIGHT_MASK) << 16),
            actor_state.actor_collision_query_flags);
        if (collision == KF_COLLISION_HIT_NONE) {
        case30_position:
            actor->position.vx = next.vx;
            actor->position.vy = next.vy;
            actor->position.vz = next.vz;
            actor_advance_animation_clamped(actor, target->animation_step);
            if (actor->animation_phase >= KF_ACTOR_ANIMATION_PHASE_MAX) {
                actor_select_target_type_in_own_group(actor, KF_ACTOR_TARGET_3);
                actor->vertical_motion_state = KF_ACTOR_VERTICAL_MOTION_VELOCITY;
            }
            break;
        } else if ((collision & KF_COLLISION_HIT_PLAYER) != KF_COLLISION_HIT_NONE) {
            player_apply_damage(target->word_0e.value, target->word_10.value,
                           target->word_12.value, target->word_0c.value,
                           target->word_14.value, target->word_16.value,
                           target->word_18.value, target->word_1a.value,
                           target->word_1c.value, 0x1000, 10, &actor->position);
            goto case30_position;
        } else {
            actor->motion.vector.vz = 0;
            actor->motion.vector.vy = 0;
            actor->motion.vector.vx = 0;
        }
        actor_advance_animation_clamped(actor, target->animation_step);
        if (actor->animation_phase >= KF_ACTOR_ANIMATION_PHASE_MAX) {
            actor_select_target_type_in_own_group(actor, KF_ACTOR_TARGET_3);
            actor->vertical_motion_state = KF_ACTOR_VERTICAL_MOTION_VELOCITY;
        }
        break;
    }
    case KF_ACTOR_TARGET_240:
        actor_suspend_vertical_motion();
        break;
    case KF_ACTOR_TARGET_6:
    case KF_ACTOR_TARGET_7:
    case KF_ACTOR_TARGET_8:
    default:
        resource_state.active_table[17]();
        break;
    }

    if ((actor->flags & KF_ACTOR_FLAG_LINKED) != KF_ACTOR_FLAGS_NONE) {
        KfActor *other = actor_state.other_actor;
        KfActorSlotState slot_state = actor->slot_state;

        if (slot_state == KF_ACTOR_SLOT_HOMEBOUND) {
            actor->current_map_layer = KF_MAP_LAYER_NONE;
            if (other->lifecycle != KF_ACTOR_LIFECYCLE_ACTIVE) {
                actor->lifecycle = KF_ACTOR_LIFECYCLE_DORMANT;
                goto behavior_done;
            }
            if (other->target_type == KF_ACTOR_TARGET_3) {
                actor->target_type = KF_ACTOR_TARGET_3;
                actor->target_action_state = KF_ACTOR_TARGET_ACTION_RETARGET_BLOCKED;
                actor->state_70.signed_state = 99;
                goto behavior_done;
            }
        } else {
            KF_ENUM_PARAM(KfCollisionHitFlags, s32) collision;

            actor->current_map_layer = other->current_map_layer;
            if (other->lifecycle != KF_ACTOR_LIFECYCLE_ACTIVE) {
                actor->flags = (actor->flags & ~KF_ACTOR_FLAG_LINKED) |
                    KF_ACTOR_FLAG_BLOCK_PLAYER_TARGETING;
                collision = collision_query_world(
                    actor->position.vx, actor->position.vy,
                    actor->position.vz, actor->collision_radius,
                    actor->collision_height, KF_COLLISION_QUERY_SHAPES | KF_COLLISION_QUERY_PLAYER);
                if ((collision & KF_COLLISION_HIT_PLAYER) != KF_COLLISION_HIT_NONE) {
                    actor->position.vx += 800 + actor->collision_radius;
                }
                if ((collision & KF_COLLISION_HIT_SHAPE_MASK) != KF_COLLISION_HIT_NONE) {
                    actor->flags |= KF_ACTOR_FLAG_USE_MAP_LAYER_FLOOR;
                }
                if ((actor->flags & KF_ACTOR_FLAG_DIE_WITH_LINKED) != KF_ACTOR_FLAGS_NONE) {
                    actor_select_target_type_in_own_group(actor, KF_ACTOR_TARGET_3);
                }
                actor->motion.vector.vx = other->motion.vector.vx;
                actor->motion.vector.vy = other->motion.vector.vy;
                actor->motion.vector.vz = other->motion.vector.vz;
                goto behavior_done;
            }
        }

        {
            struct KfEulerAngles rotation;
            VECTOR group_offset;
            VECTOR vertex_offset;

            /* Copy the two trailing orientation bytes with the three angles. */
            *(KfActorOrientation *)&actor->rotation =
                *(const KfActorOrientation *)&other->rotation;
            rotation.x = actor->rotation.x;
            rotation.y = actor->rotation.y;
            rotation.z = actor->rotation.z;
            vector_rotate_yxz(&rotation, (SVECTOR *)&group->position_offset_x,
                              &group_offset);
            actor_sample_rotated_animation_vertex(other, actor->word_24.linked_animation_vertex_index, &vertex_offset);
            actor->position.vx = other->position.vx + vertex_offset.vx -
                                 group_offset.vx;
            actor->position.vy = other->position.vy + vertex_offset.vy -
                                 group_offset.vy;
            actor->position.vz = other->position.vz + vertex_offset.vz -
                                 group_offset.vz;
        }
    } else if ((actor->flags & KF_ACTOR_FLAG_MAP_OBJECT_ATTACHED) == KF_ACTOR_FLAGS_NONE) {
        actor_update_vertical_motion();
    }

behavior_done:
    if (actor->lifecycle == KF_ACTOR_LIFECYCLE_ACTIVE) {
        map_cell_add_layer_occupancy(actor->position.vx, actor->position.vz,
                       actor->collision_radius, 1);
    }
}

ADDRESS(0x80040348, 0x1dc)
void actor_update_frame(void)
{
    KfActor *actor;

    actor_state.active_actor_count = 0;
    actor = actor_state.actors;
    actor_state.current_actor_slot_index = 0;
    do {
        if (actor->slot_state != KF_ACTOR_SLOT_FREE) {
            actor_bind_current(actor);
            if (((actor_state.actor_update_frame_count & 3) ==
                 (actor_state.current_actor_slot_index & 3)) ||
                player_state.force_actor_lifecycle_refresh != 0 ||
                player_state.death_state == KF_PLAYER_REACTION_MAP_OBJECT_FOLLOW) {
                actor_update_lifecycle_for_player_range();
            }

            if (actor->lifecycle == KF_ACTOR_LIFECYCLE_ACTIVE) {
                if ((actor_state.actor_update_frame_count & 3) ==
                    (actor_state.active_actor_count & 3)) {
                    actor_select_target_for_player_distance();
                }
                actor_update_behavior();
                actor_state.active_actor_count++;
            }

            if ((actor_state.active_group->initial_actor_flags &
                 KF_ACTOR_FLAG_MAP_OBJECT_ATTACHED) != KF_ACTOR_FLAGS_NONE) {
                KfMapObject *object =
                    &map_object_state.objects[actor->word_22.linked_map_object_slot];
                s32 object_z;

                actor->position.vx = object->position.vx;
                actor->position.vy = object->position.vy - 500;
                object_z = object->position.vz;
                actor->position.vz = object_z;
                if (actor->lifecycle == KF_ACTOR_LIFECYCLE_ACTIVE) {
                    actor->rotation.y = vector_xz_to_angle(
                        player_state.camera_position.vx - actor->position.vx,
                        player_state.camera_position.vz - object_z);
                }
            }
        }
        actor++;
        actor_state.current_actor_slot_index++;
    } while (actor_state.current_actor_slot_index < KF_ACTOR_CAPACITY);

    actor_state.actor_update_frame_count++;
    actor_bind_current(NULL);
}

ADDRESS(0x80040524, 0x74)
void actor_fixup_group_targets(void)
{
    KfTargetGroup *group = actor_state.target_groups;
    s32 group_index;
    s32 slot_index;
    KfTargetReference *slot;

    group_index = 0;
    while (group_index < KF_COUNTOF(actor_state.target_groups)) {
        if (group->definition_id == KF_TARGET_GROUP_DEFINITION_END) {
            break;
        }
        slot = group->targets;
        for (slot_index = 0; slot_index < KF_COUNTOF(group->targets); slot_index++, slot++) {
            if (slot->relative_offset == -1) {
                slot->pointer = NULL;
            } else {
                slot->pointer = (KfTargetCandidate *)
                    &actor_state.target_candidate_blob[slot->relative_offset];
            }
        }
        group_index++;
        group++;
    }
}

ADDRESS(0x80040598, 0x1cc)
void actor_load_records(const KfActorLoadRecord *records)
{
    KfActor *actor = actor_state.actors;
    u16 remaining = KF_ACTOR_CAPACITY - 1;

    do {
        actor->slot_state = records->slot_state;
        if (actor->slot_state != KF_ACTOR_SLOT_FREE) {
            const KfTargetGroup *group;

            actor->group_index = records->group_index;
            actor->unknown_04 = 0;
            actor->placement_flags = records->placement_flags;
            actor->home_map_layer = records->home_map_layer;
            actor->home_cell_z = records->cell_z;
            actor->home_cell_x = records->cell_x;
            actor->spawn_chance = records->spawn_chance;
            actor->death_drop_object_id = records->death_drop_object_id;
            actor->word_20.value = records->initial_actor_word_20;
            actor->word_22.value = records->initial_actor_word_22;
            actor->word_24.value = records->initial_actor_word_24;
            actor->vertical_anchor_offset = records->vertical_anchor_offset;
            actor->lifecycle = KF_ACTOR_LIFECYCLE_DORMANT;
            actor->target_type = KF_ACTOR_TARGET_0;
            actor->target_action_state = KF_ACTOR_TARGET_ACTION_UNSELECTED;
            actor->target = NULL;

            group = &actor_state.target_groups[actor->group_index];
            actor_copy_group_defaults(actor);
            actor_set_home_position(actor);
            actor->render_depth = group->render_depth;
            if ((actor->flags & KF_ACTOR_FLAG_LINKED) != KF_ACTOR_FLAGS_NONE) {
                if (actor->slot_state == KF_ACTOR_SLOT_HOMEBOUND) {
                    if (actor->word_24.value == -1) {
                        actor->word_24.value = group->word_1a.slot3_home_x_fallback;
                    }
                    if (actor->vertical_anchor_offset == -1) {
                        actor->vertical_anchor_offset = group->default_vertical_anchor_offset;
                    }
                } else {
                    actor->slot_state = KF_ACTOR_SLOT_LINKED_COMPANION;
                    actor->vertical_anchor_offset = 0;
                }
            }
        } else {
            actor->lifecycle = KF_ACTOR_LIFECYCLE_DORMANT;
        }
        records++;
        actor++;
    } while (remaining-- != 0);
}
