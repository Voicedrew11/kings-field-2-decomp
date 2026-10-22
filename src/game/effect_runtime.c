#include <kf/lib/address.h>
#include <kf/game/audio.h>
#include <kf/game/effect.h>
#include <kf/game/collision_cache.h>
#include <kf/lib/null.h>
#include <kf/lib/math.h>
#include <kf/game/animation.h>
#include <kf/game/actor.h>
#include <kf/game/player.h>
#include <psyq/libc.h>
#include <kf/game/asset.h>


DATA(0x8006e638, 0x4, ".sdata")
u32 effect_trail_next_slot = 0;

DATA(0x8006e63c, 0x8, ".sdata")
static SVECTOR effect_zero_direction = {0, 0, 0, 0};

DATA(0x8009b4ec, 0x4, ".bss")
s32 effect_kind102_sound_cooldown_frame;

DATA(0x8019c5ec, 0x2a8c, ".bss")
KfEffectState effect_state;

DATA(0x801c7fac, 0x8, ".bss")
SVECTOR effect_collision_motion_step;

DATA(0x801da56c, 0x900, ".bss")
KfEffectTrailRow effect_trail_rows[4][24];

RODATA(0x8001249c, 0x3f4)

enum {
    EFFECT_SPATIAL_VOLUME = 110,
    EFFECT_SPATIAL_MAX_DISTANCE = 0x6d60,
    EFFECT_SPATIAL_ATTENUATION_DISTANCE = 0x7148,
    EFFECT_COLLISION_HEIGHT_MASK = 0xfff,
    EFFECT_ANIMATION_PHASE_MASK = KF_FIXED12_ONE - 1
};

ADDRESS(0x80040764, 0x3c)
KfAudioPlaybackResult effect_play_spatial_sound(
    KfEffectRecord *effect, s32 sound)
{
    return audio_play_spatial_range(sound, &effect->position,
        EFFECT_SPATIAL_VOLUME, EFFECT_SPATIAL_MAX_DISTANCE,
        EFFECT_SPATIAL_ATTENUATION_DISTANCE, 0);
}

ADDRESS(0x800407a0, 0x12c)
KF_ENUM_PARAM(KfCollisionHitFlags, s32) effect_probe_collision_by_type(const VECTOR *position, s32 radius,
    s32 height_flags)
{
    KfEffectRecord *record = effect_state.current_record;
    s32 y;
    if (record->cooldown == 0) {
        y = position->vy + ((height_flags & EFFECT_COLLISION_HEIGHT_MASK) >> 1);
        switch (record->type & KF_EFFECT_TARGET_MASK) {
        case KF_EFFECT_TARGET_ACTORS:
            return collision_query_world(position->vx, y, position->vz, radius,
                height_flags, KF_COLLISION_QUERY_SHAPES | KF_COLLISION_QUERY_ACTORS |
                                  KF_COLLISION_QUERY_MAP_OBJECTS);
        case KF_EFFECT_TARGET_PLAYER:
            return collision_query_world(position->vx, y, position->vz, radius,
                height_flags, KF_COLLISION_QUERY_SHAPES | KF_COLLISION_QUERY_MAP_OBJECTS |
                                  KF_COLLISION_QUERY_PLAYER);
        case KF_EFFECT_TARGET_ACTORS_AND_PLAYER:
            return collision_query_world(position->vx, y, position->vz, radius,
                height_flags, KF_COLLISION_QUERY_SHAPES | KF_COLLISION_QUERY_ACTORS |
                                  KF_COLLISION_QUERY_MAP_OBJECTS | KF_COLLISION_QUERY_PLAYER);
        case KF_EFFECT_TARGET_SHAPES_ONLY:
            return collision_query_world(position->vx, y, position->vz, radius,
                height_flags, KF_COLLISION_QUERY_SHAPES);
        default:
            break;
        }
    } else {
        record->cooldown--;
        return KF_COLLISION_HIT_NONE;
    }
}

enum { EFFECT_FIXED_MAGIC_POWER = 5 };

ADDRESS(0x800408cc, 0x218)
void effect_dispatch_magic_impact(KF_ENUM_PARAM(KfCollisionHitFlags, s32) kind,
                                  KF_ENUM_PARAM(KfActorDamageFlags, s32) source_flags,
                   s32 radius, u16 power,
                   u8 damage_multiplier_tenths, u16 magic_06, u16 magic_08, u16 magic_0a,
                   u16 magic_04, u16 magic_0c, u16 magic_0e, u16 magic_10,
                   u16 magic_12, u16 magic_14, const VECTOR *position)
{
    KfCollisionHitFlags options = kind & KF_COLLISION_IMPACT_OPTION_MASK;
    kind &= ~KF_COLLISION_IMPACT_OPTION_MASK;

    if (kind == KF_COLLISION_HIT_PLAYER) {
        player_apply_damage(magic_06, magic_08, magic_0a, magic_04,
                      magic_0c, magic_0e, magic_10, magic_12,
                      magic_14, radius, damage_multiplier_tenths, position);
    } else if (kind == KF_COLLISION_HIT_ACTOR) {
        s32 actor_index = bss_801c7540.collision_cache.actor_index;
        KfActor *actor = &actor_state.actors[actor_index];
        KfTargetGroup *group = &actor_state.target_groups[actor->group_index];

        if (position != NULL) {
            s32 angle = vector_xz_to_angle(
                actor->position.vx - position->vx,
                actor->position.vz - position->vz);
            if (!angle_within_tolerance(actor->rotation.y, angle + KF_ANGLE_HALF_TURN,
                                        group->actor_facing_tolerance)) {
                return;
            }
        }

        source_flags &= KF_ACTOR_DAMAGE_SOURCE_MASK;
        if ((options & KF_COLLISION_IMPACT_COUNTS_AS_PHYSICAL) != KF_COLLISION_HIT_NONE) {
            source_flags |= KF_ACTOR_DAMAGE_PHYSICAL;
        } else {
            source_flags |= KF_ACTOR_DAMAGE_MAGIC;
        }
        actor_apply_magic_to_actor(bss_801c7540.collision_cache.actor_index, power, magic_06,
                      magic_08, magic_0a, magic_0c, magic_0e, magic_10,
                      magic_12, magic_14, radius, source_flags, position);
        if ((options & KF_COLLISION_IMPACT_HOLD_ACTOR_ANIMATION) != KF_COLLISION_HIT_NONE) {
            actor->flags |= KF_ACTOR_FLAG_EFFECT_ANIMATION_HOLD;
        }
    }
}

ADDRESS(0x80040ae4, 0x24)
int effect_magic_power(KfEffectRecord *effect)
{
    if ((effect->type & KF_EFFECT_USE_PLAYER_MAGIC) != KF_EFFECT_TYPE_NONE) {
        return player_state.magic;
    }
    return EFFECT_FIXED_MAGIC_POWER;
}


ADDRESS(0x80040b08, 0xe0)
void effect_apply_current_magic(KF_ENUM_PARAM(KfCollisionHitFlags, s32) kind, s32 radius,
                                const VECTOR *position)
{
    KfEffectRecord *record = effect_state.current_record;
    const KfMagicRecord *magic = effect_state.current_magic;
    u16 power = effect_magic_power(record);

    /* Actor damage keeps only the effect type's source-class bits. */
    effect_dispatch_magic_impact(kind, KF_ENUM_DECODE(KF_ENUM_PROMOTED(KfActorDamageFlags),
                                                      KF_ENUM_ENCODE(u8, record->type)),
                  radius, power, record->damage_multiplier_tenths,
                  magic->damage_components[0], magic->damage_components[1], magic->damage_components[2],
                  magic->player_status_flags, magic->damage_components[3], magic->damage_components[4],
                  magic->damage_components[5], magic->damage_components[6], magic->damage_components[7],
                  position);
}

ADDRESS(0x80040be8, 0x68)
void effect_apply_current_magic_backstep(KF_ENUM_PARAM(KfCollisionHitFlags, s32) kind)
{
    const KfEffectRecord *record = effect_state.current_record;
    VECTOR position;

    position.vx = record->position.vx - (record->direction.vx << 3);
    position.vy = record->position.vy - (record->direction.vy << 3);
    position.vz = record->position.vz - (record->direction.vz << 3);
    effect_apply_current_magic(kind, 5000, &position);
}


ADDRESS(0x80040c50, 0x1a8)
void effect_apply_radial_magic_damage(VECTOR *position, s32 start, s32 end,
                                      s32 mode, s32 falloff, s32 scale_and_flags)
{
    KfEffectRecord *record = effect_state.current_record;
    const KfMagicRecord *magic = effect_state.current_magic;

    if ((record->type & KF_EFFECT_TARGET_PLAYER) != KF_EFFECT_TYPE_NONE) {
        player_apply_radial_damage(position, start, end, mode, falloff,
                      magic->damage_components[0], magic->damage_components[1], magic->damage_components[2],
                      magic->player_status_flags, magic->damage_components[3], magic->damage_components[4],
                      magic->damage_components[5], magic->damage_components[6], magic->damage_components[7],
                      scale_and_flags, record->damage_multiplier_tenths);
    }
    if ((record->type & KF_EFFECT_TARGET_ACTORS) != KF_EFFECT_TYPE_NONE) {
        u16 power = effect_magic_power(record);

        actor_apply_area_magic(position, start, end, mode, falloff,
                      power, magic->damage_components[0],
                      magic->damage_components[1], magic->damage_components[2], magic->damage_components[3],
                      magic->damage_components[4], magic->damage_components[5], magic->damage_components[6],
                      magic->damage_components[7], scale_and_flags, KF_ENUM_DECODE(KF_ENUM_PROMOTED(KfActorDamageFlags),
                          KF_ENUM_ENCODE(u8, record->type & KF_EFFECT_SOURCE_MASK)) | KF_ACTOR_DAMAGE_MAGIC);
    }
}


ADDRESS(0x80040df8, 0xf4)
void effect_sample_rotated_vertex(KfEffectRecord *record, s32 mode, VECTOR *output, const SVECTOR *scale)
{
    struct KfEulerAngles angles;
    SVECTOR offset;

    animation_sample_vertex(KF_ENUM_ENCODE(u8, record->render_id) + 40, record->animation_clip,
                  record->animation_phase_q12, mode, &offset);
    offset.vx = offset.vx * scale->vx >> KF_FIXED12_BITS;
    offset.vy = offset.vy * scale->vy >> KF_FIXED12_BITS;
    offset.vz = offset.vz * scale->vz >> KF_FIXED12_BITS;

    angles.x = record->rotation.vx;
    angles.y = record->rotation.vy + KF_ANGLE_HALF_TURN;
    angles.z = record->rotation.vz;
    vector_rotate_yxz(&angles, &offset, output);
}

ADDRESS(0x80040eec, 0x6c)
void effect_sample_world_vertex(KfEffectRecord *record, s32 mode, VECTOR *position, const SVECTOR *scale)
{
    effect_sample_rotated_vertex(record, mode, position, scale);
    addVector(position, &record->position);
}


ADDRESS(0x80040f58, 0x44)
KfEffectRecord *effect_pool_find_free(void)
{
    KfEffectRecord *record = effect_state.records;
    u16 i = KF_EFFECT_CAPACITY;

    do {
        if (record->type == KF_EFFECT_SLOT_FREE) {
            return record;
        }
        record++;
    } while (--i != 0);
    return NULL;
}

ADDRESS(0x80040f9c, 0x40)
void effect_pool_initialize_scaled(KfEffectRecord *record, KfEffectRenderId render_id, u16 scale)
{
    record->render_flags = KF_EFFECT_RENDER_VISIBLE | KF_EFFECT_RENDER_IDENTITY_TRANSFORM;
    record->animation_clip = KF_ANIMATION_CLIP_STATIC_OBJECT_FIRST;
    record->base_render_id = render_id;
    record->render_id = render_id;
    record->render_queue_mode = KF_RENDER_QUEUE_BLEND_ADD;
    record->lighting_override_index = KF_LIGHTING_SCALED_EFFECT;
    record->lighting_blend_q12 = KF_FIXED12_ONE;
    record->scale_z = scale;
    record->scale_y = scale;
    record->scale_x = scale;
}

ADDRESS(0x80040fdc, 0x64)
void effect_pool_initialize_fixed(KfEffectRecord *record, KfEffectRenderId render_id)
{
    record->render_flags = KF_EFFECT_RENDER_ALWAYS_VISIBLE | KF_EFFECT_RENDER_SCREEN_SPACE;
    record->animation_clip = KF_ANIMATION_CLIP_STATIC_OBJECT_FIRST;
    record->base_render_id = render_id;
    record->render_id = render_id;
    record->render_queue_mode = KF_RENDER_QUEUE_BLEND_ADD;
    record->lighting_override_index = KF_LIGHTING_EFFECT;
    record->lighting_blend_q12 = KF_FIXED12_ONE;
    record->scale_y = 0x100;
    record->scale_z = 0x100;
    record->scale_x = 0x100;
    record->position.vx = 160;
    record->position.vy = 120;
    record->rotation.vz = 0;
    record->rotation.vy = 0;
    record->rotation.vx = 0;
    record->position.vz = 0;
}

ADDRESS(0x80041040, 0x13e4)
KfEffectRecord *effect_construct_record(u8 damage_multiplier_tenths, KfEffectType type, KfEffectKind kind,
                              const VECTOR *position,
                              const SVECTOR *direction, ...)
{
    KfEffectRecord *record;
    s32 length_squared;
    u16 third_parameter;
    /* O32 stacks the fifth argument; optional words follow its home slot. */
    s32 *va = (s32 *)&direction;

    record = effect_pool_find_free();

    if (record == NULL) {
        goto finish;
    }
    record->type = type;
    record->kind = kind;
    if (position != NULL) {
        record->position = *position;
    }
    record->map_layer_mask = KF_MAP_LAYER_BOTH;
    if (direction != NULL) {
        record->direction = *direction;
    } else {
        record->direction.vz = 0;
        record->direction.vy = 0;
        record->direction.vx = 0;
    }
    record->phase = 0;
    record->damage_multiplier_tenths = damage_multiplier_tenths;
    record->scale_z = KF_FIXED12_ONE;
    record->scale_y = KF_FIXED12_ONE;
    record->scale_x = KF_FIXED12_ONE;
    record->rotation.vz = 0;
    record->rotation.vy = 0;
    record->rotation.vx = 0;
    record->animation_phase_q12 = 0;
    record->unknown_05 = 0;
    record->render_flags = KF_EFFECT_RENDER_VISIBLE;
    if ((record->type & KF_EFFECT_USE_PLAYER_MAGIC) != KF_EFFECT_TYPE_NONE &&
        player_state.death_state == KF_PLAYER_REACTION_MAP_OBJECT_FOLLOW) {
        record->cooldown = 8;
    } else {
        record->cooldown = 1;
    }
    record->lighting_override_index = KF_LIGHTING_NONE;
    record->render_queue_mode = KF_RENDER_QUEUE_TEXTURED;
    record->updates_remaining = -1;
    length_squared = (s32)record->direction.vx * record->direction.vx +
        (s32)record->direction.vy * record->direction.vy +
        (s32)record->direction.vz * record->direction.vz;
    record->lighting_blend_q12 = 0;
    if (length_squared >= 810001) {
        record->midpoint_collision_enabled = KF_TRUE;
    } else {
        record->midpoint_collision_enabled = KF_FALSE;
    }

    switch (record->kind) {
    case KF_EFFECT_KIND_7:
    case KF_EFFECT_KIND_49:
        effect_pool_initialize_scaled(record, KF_EFFECT_MODEL_45, 0x1800);
        record->updates_remaining = 50;
        record->midpoint_collision_enabled = KF_TRUE;
        effect_play_spatial_sound(record, 0x23);
        break;
    case KF_EFFECT_KIND_32:
        effect_pool_initialize_scaled(record, KF_EFFECT_MODEL_33, 0x1800);
        record->updates_remaining = 50;
        break;
    case KF_EFFECT_KIND_4:
        record->render_flags = KF_EFFECT_RENDER_VISIBLE;
        record->animation_clip = KF_ANIMATION_CLIP_STATIC_OBJECT_FIRST;
        record->render_queue_mode = KF_RENDER_QUEUE_BLEND_ADD;
        record->base_render_id = KF_EFFECT_MODEL_31;
        record->render_id = KF_EFFECT_MODEL_31;
        record->lighting_override_index = KF_LIGHTING_EFFECT;
        record->lighting_blend_q12 = KF_FIXED12_ONE;
        record->updates_remaining = 0x2d;
        record->cache_tail.payload.collision_latch.impact_handled = 0;
        record->scale_z = 0x32c8;
        record->scale_y = 0x32c8;
        record->scale_x = 0x32c8;
        record->midpoint_collision_enabled = KF_TRUE;
        effect_play_spatial_sound(record, 0x20);
        break;
    case KF_EFFECT_KIND_28:
        record->scale_z = 0x800;
        record->scale_y = 0x800;
        record->scale_x = 0x800;
        /* fall through */
    case KF_EFFECT_KIND_1:
        record->render_flags = KF_EFFECT_RENDER_VISIBLE;
        record->animation_clip = KF_ANIMATION_CLIP_STATIC_OBJECT_FIRST;
        record->base_render_id = KF_EFFECT_MODEL_32;
        record->render_id = KF_EFFECT_MODEL_32;
        record->cache_tail.payload.kind1.collision_stage = KF_EFFECT_STAGE_TRAVEL;
        record->updates_remaining = 70;
        effect_play_spatial_sound(record, 0x1b);
        break;
    case KF_EFFECT_KIND_26:
        record->render_flags = KF_EFFECT_RENDER_VISIBLE;
        record->animation_clip = KF_ANIMATION_CLIP_STATIC_OBJECT_FIRST;
        record->base_render_id = KF_EFFECT_MODEL_36;
        record->render_id = KF_EFFECT_MODEL_36;
        record->cache_tail.payload.kind1.collision_stage = KF_EFFECT_STAGE_TRAVEL;
        record->updates_remaining = 70;
        record->scale_z = 600;
        record->scale_y = 600;
        record->scale_x = 600;
        record->cache_tail.payload.kind26.stage = KF_EFFECT_STAGE_TRAVEL;
        break;
    case KF_EFFECT_KIND_27:
        record->render_flags = KF_EFFECT_RENDER_VISIBLE;
        record->animation_clip = KF_ANIMATION_CLIP_STATIC_OBJECT_FIRST;
        record->base_render_id = KF_EFFECT_MODEL_37;
        record->render_id = KF_EFFECT_MODEL_37;
        record->cache_tail.payload.kind26.stage = KF_EFFECT_STAGE_TRAVEL;
        record->updates_remaining = 70;
        record->scale_z = 0;
        record->scale_y = 0;
        record->scale_x = 0;
        break;
    case KF_EFFECT_KIND_111: {
        u16 value = va[1];
        record->render_flags = KF_EFFECT_RENDER_HIDDEN;
        record->updates_remaining = 50;
        record->cache_tail.payload.kind111.actor_index = value;
        effect_play_spatial_sound(record, 0x21);
        break;
    }
    case KF_EFFECT_KIND_13:
        effect_pool_initialize_scaled(record, KF_EFFECT_MODEL_33, KF_FIXED12_ONE);
        record->updates_remaining = 50;
        effect_play_spatial_sound(record, 0x2a);
        break;
    case KF_EFFECT_KIND_0:
        effect_pool_initialize_scaled(record, KF_EFFECT_MODEL_14, 0x200);
        record->updates_remaining = 50;
        record->cache_tail.payload.collision_latch.impact_handled = 0;
        break;
    case KF_EFFECT_KIND_25: {
        const SVECTOR *angles;

        record->render_flags = KF_EFFECT_RENDER_VISIBLE;
        record->animation_clip = KF_ANIMATION_CLIP_STATIC_OBJECT_FIRST;
        record->render_queue_mode = KF_RENDER_QUEUE_BLEND_ADD;
        record->base_render_id = KF_EFFECT_MODEL_13;
        record->render_id = KF_EFFECT_MODEL_13;
        record->lighting_override_index = KF_LIGHTING_EFFECT;
        record->lighting_blend_q12 = KF_FIXED12_ONE;
        record->updates_remaining = 45;
        angles = (const SVECTOR *)va[1];
        record->rotation = *angles;
        record->rotation.vz = rand() >> KF_RANDOM_ANGLE_SHIFT;
        record->cache_tail.payload.collision_latch.impact_handled = 0;
        break;
    }
    case KF_EFFECT_KIND_5: {
        KfEffectKind5Fanout *fanout = &record->cache_tail.payload.kind5;
        u8 parameter;

        record->render_flags = KF_EFFECT_RENDER_HIDDEN;
        parameter = va[1];
        record->updates_remaining = 70;
        fanout->actor_index = parameter;
        effect_play_spatial_sound(record, 0x21);
        break;
    }
    case KF_EFFECT_KIND_105: {
        KfEffectKind105Attachment *attachment =
            &record->cache_tail.payload.kind105;

        effect_pool_initialize_scaled(record, KF_EFFECT_MODEL_14, KF_FIXED12_ONE);
        record->rotation.vz = rand();
        record->direction.vx += (rand() >> 9) - 32;
        record->direction.vy += (rand() >> 9) - 32;
        record->direction.vz += (rand() >> 9) - 32;
        attachment->parent_index = va[1];
        attachment->actor_index = va[2];
        attachment->vertex_index = va[3];
        record->updates_remaining = 70;
        break;
    }
    case KF_EFFECT_KIND_9: {
        KfEffectKind9Target *target = &record->cache_tail.payload.kind9;

        effect_pool_initialize_scaled(record, KF_EFFECT_MODEL_8, KF_FIXED12_ONE);
        record->direction.vx += (rand() >> 8) - 64;
        record->direction.vy += (rand() >> 8) - 64;
        record->direction.vz += (rand() >> 8) - 64;
        target->actor_index = va[1];
        record->updates_remaining = 100;
        record->cooldown = 3;
        effect_play_spatial_sound(record, 0x26);
        break;
    }
    case KF_EFFECT_KIND_53:
        effect_pool_initialize_scaled(record, KF_EFFECT_MODEL_33, 0x2000);
        goto randomize_33_53;
    case KF_EFFECT_KIND_33:
        effect_pool_initialize_scaled(record, KF_EFFECT_MODEL_33, KF_FIXED12_ONE);
    randomize_33_53: {
        s32 random_z;

        record->direction.vx += (rand() >> 8) - 64;
        record->direction.vy += (rand() >> 8) - 64;
        random_z = rand();
        record->updates_remaining = 100;
        record->cooldown = 3;
        record->direction.vz += (random_z >> 8) - 64;
        effect_play_spatial_sound(record, 0x21);
        break;
    }
    case KF_EFFECT_KIND_106:
        effect_pool_initialize_scaled(record, KF_EFFECT_MODEL_8, 0x2000);
        record->direction.vy -= 100;
        effect_play_spatial_sound(record, 0x24);
        record->updates_remaining = 100;
        break;
    case KF_EFFECT_KIND_8: {
        KfEffectKind8State *kind8 =
            &record->cache_tail.payload.kind8;

        effect_pool_initialize_scaled(record, KF_EFFECT_MODEL_8, KF_FIXED12_ONE);
        kind8->parent_index = va[1];
        kind8->vertical_step = va[2];
        record->updates_remaining = 50;
        break;
    }
    case KF_EFFECT_KIND_10: {
        KfEffectKind10Targeting *targeting =
            &record->cache_tail.payload.kind10;
        const SVECTOR *angles;

        record->render_flags = KF_EFFECT_RENDER_VISIBLE;
        record->animation_clip = KF_ANIMATION_CLIP_FIRST;
        record->render_queue_mode = KF_RENDER_QUEUE_BLEND_ADD;
        record->base_render_id = KF_EFFECT_MODEL_21;
        record->render_id = KF_EFFECT_MODEL_21;
        record->lighting_override_index = KF_LIGHTING_EFFECT;
        record->lighting_blend_q12 = KF_FIXED12_ONE;
        record->scale_z = 3000;
        record->scale_y = 3000;
        record->scale_x = 3000;
        angles = (const SVECTOR *)va[1];
        record->rotation = *angles;
        targeting->emissions_remaining = 0;
        record->updates_remaining = 150;
        effect_play_spatial_sound(record, 0x27);
        break;
    }
    case KF_EFFECT_KIND_6: {
        const SVECTOR *angles;
        KfEffectTrailRow *rows;
        s32 index;
        u32 slot;

        record->render_flags = KF_EFFECT_RENDER_HIDDEN;
        record->animation_clip = KF_ANIMATION_CLIP_STATIC_OBJECT_FIRST;
        record->render_queue_mode = KF_RENDER_QUEUE_BLEND_ADD;
        record->base_render_id = KF_EFFECT_MODEL_0;
        record->render_id = KF_EFFECT_MODEL_0;
        record->scale_z = KF_FIXED12_ONE;
        record->scale_y = KF_FIXED12_ONE;
        record->scale_x = KF_FIXED12_ONE;
        record->lighting_override_index = KF_LIGHTING_EFFECT;
        record->lighting_blend_q12 = KF_FIXED12_ONE;
        angles = (const SVECTOR *)va[1];
        record->rotation = *angles;
        slot = effect_trail_next_slot;
        rows = record->cache_tail.payload.trail.rows = effect_trail_rows[slot];
        effect_trail_next_slot = (slot + 1) & 3;
        for (index = 23; index != -1; index--, rows++) {
            rows->position = record->position;
            rows->rotation = record->rotation;
        }
        record->cache_tail.payload.trail.frame_index = 0;
        record->cache_tail.payload.trail.phase_counter = 0;
        record->updates_remaining = 150;
        effect_play_spatial_sound(record, 0x22);
        break;
    }
    case KF_EFFECT_KIND_107: {
        const SVECTOR *angles;

        record->render_flags = KF_EFFECT_RENDER_VISIBLE;
        record->animation_clip = KF_ANIMATION_CLIP_STATIC_OBJECT_FIRST;
        record->render_queue_mode = KF_RENDER_QUEUE_BLEND_ADD;
        record->base_render_id = KF_EFFECT_MODEL_35;
        record->render_id = KF_EFFECT_MODEL_35;
        record->lighting_override_index = KF_LIGHTING_EFFECT;
        record->lighting_blend_q12 = KF_FIXED12_ONE;
        record->scale_z = KF_FIXED12_ONE;
        record->scale_y = KF_FIXED12_ONE;
        record->scale_x = KF_FIXED12_ONE;
        angles = (const SVECTOR *)va[1];
        record->rotation = *angles;
        break;
    }
    case KF_EFFECT_KIND_121:
        effect_pool_initialize_scaled(record, KF_EFFECT_MODEL_46, KF_FIXED12_ONE);
        record->updates_remaining = 100;
        record->cache_tail.payload.kind103.remaining = va[1];
        effect_play_spatial_sound(record, 0x28);
        break;
    case KF_EFFECT_KIND_103:
        effect_pool_initialize_scaled(record, KF_EFFECT_MODEL_15, KF_FIXED12_ONE);
        record->updates_remaining = 100;
        record->cache_tail.payload.kind103.remaining = va[1];
        effect_play_spatial_sound(record, 0x28);
        break;
    case KF_EFFECT_KIND_122:
        effect_pool_initialize_scaled(record, KF_EFFECT_MODEL_47, KF_FIXED12_ONE);
        record->scale_y = va[1];
        record->updates_remaining = 15;
        break;
    case KF_EFFECT_KIND_104:
        effect_pool_initialize_scaled(record, KF_EFFECT_MODEL_16, KF_FIXED12_ONE);
        record->scale_y = va[1];
        record->updates_remaining = 15;
        break;
    case KF_EFFECT_KIND_54: {
        s32 value;
        KF_ENUM_PROMOTED(KfEffectRenderId) render_id;

        render_id = KF_EFFECT_MODEL_48;
        goto initialize_11_54;
    case KF_EFFECT_KIND_11:
        render_id = KF_EFFECT_MODEL_17;
    initialize_11_54:

        record->base_render_id = render_id;
        record->render_id = render_id;
        record->render_flags = KF_EFFECT_RENDER_VISIBLE;
        record->animation_clip = KF_ANIMATION_CLIP_STATIC_OBJECT_FIRST;
        record->render_queue_mode = KF_RENDER_QUEUE_BLEND_ADD;
        record->lighting_override_index = KF_LIGHTING_EFFECT;
        record->lighting_blend_q12 = KF_FIXED12_ONE;
        record->scale_z = 0;
        record->scale_y = 0;
        record->scale_x = 0;
        value = va[1];
        record->cache_tail.payload.scale_step_argument.scale_step = value / 4;
        break;
    }
    case KF_EFFECT_KIND_118:
    case KF_EFFECT_KIND_119: {
        const SVECTOR *angles;

        record->render_flags = KF_EFFECT_RENDER_VISIBLE;
        record->animation_clip = KF_ANIMATION_CLIP_STATIC_OBJECT_FIRST;
        record->base_render_id = KF_EFFECT_MODEL_19;
        record->render_id = KF_EFFECT_MODEL_19;
        record->updates_remaining = 0x23;
        angles = (const SVECTOR *)va[1];
        record->rotation = *angles;
        effect_play_spatial_sound(record, 0x20);
        break;
    }
    case KF_EFFECT_KIND_51:
    case KF_EFFECT_KIND_52:
        record->render_flags = KF_EFFECT_RENDER_VISIBLE;
        record->animation_clip = KF_ANIMATION_CLIP_STATIC_OBJECT_FIRST;
        record->base_render_id = KF_EFFECT_MODEL_37;
        record->render_id = KF_EFFECT_MODEL_37;
        record->cache_tail.payload.collision_latch.impact_handled = 0;
        record->scale_z = 0;
        record->scale_y = 0;
        record->scale_x = 0;
        break;
    case KF_EFFECT_KIND_2: {
        record->render_flags = KF_EFFECT_RENDER_VISIBLE;
        record->animation_clip = KF_ANIMATION_CLIP_STATIC_OBJECT_FIRST;
        record->base_render_id = KF_EFFECT_MODEL_9;
        record->render_id = KF_EFFECT_MODEL_9;
        record->lighting_override_index = KF_LIGHTING_EFFECT;
        record->lighting_blend_q12 = KF_FIXED12_ONE;
        record->scale_z = 0;
        record->scale_x = 0;
        record->cache_tail.payload.kind2.max_scale = va[1];
        record->cache_tail.payload.kind2.scale_step = va[2];
        third_parameter = va[3];
        record->cache_tail.payload.kind2.radial_damage_parameter = third_parameter;
        effect_play_spatial_sound(record, 0x1e);
        break;
    }
    case KF_EFFECT_KIND_20:
        record->render_flags = KF_EFFECT_RENDER_VISIBLE;
        record->animation_clip = KF_ANIMATION_CLIP_STATIC_OBJECT_FIRST;
        record->render_queue_mode = KF_RENDER_QUEUE_BLEND_ADD;
        record->base_render_id = KF_EFFECT_MODEL_11;
        record->render_id = KF_EFFECT_MODEL_11;
        record->lighting_override_index = KF_LIGHTING_EFFECT;
        record->lighting_blend_q12 = KF_FIXED12_ONE;
        record->scale_z = 0;
        record->scale_y = 0;
        record->scale_x = 0;
        break;
    case KF_EFFECT_KIND_12: {
        KfEffectKind12Aim *aim =
            &record->cache_tail.payload.kind12;
        const SVECTOR *angles;

        record->render_flags = KF_EFFECT_RENDER_VISIBLE;
        record->render_queue_mode = KF_RENDER_QUEUE_BLEND_ADD;
        record->animation_clip = KF_ANIMATION_CLIP_STATIC_OBJECT_FIRST;
        record->base_render_id = KF_EFFECT_MODEL_34;
        record->render_id = KF_EFFECT_MODEL_34;
        record->lighting_override_index = KF_LIGHTING_PRESET_49;
        record->lighting_blend_q12 = KF_FIXED12_ONE;
        angles = (const SVECTOR *)va[1];
        record->rotation = *angles;
        record->scale_z = 30000;
        record->scale_y = 30000;
        record->scale_x = 30000;
        aim->max_length = va[2];
        aim->scale = va[3];
        aim->turn_step = va[4];
        aim->close_scale = va[5];
        record->updates_remaining = va[6];
        break;
    }
    case KF_EFFECT_KIND_100: {
        const SVECTOR *angles;

        record->render_flags = KF_EFFECT_RENDER_VISIBLE;
        record->animation_clip = KF_ANIMATION_CLIP_STATIC_OBJECT_FIRST;
        record->base_render_id = KF_EFFECT_MODEL_34;
        record->render_id = KF_EFFECT_MODEL_34;
        record->lighting_override_index = KF_LIGHTING_EFFECT;
        record->lighting_blend_q12 = KF_FIXED12_ONE;
        angles = (const SVECTOR *)va[1];
        record->rotation = *angles;
        record->rotation.vy += (rand() >> 7) - 128;
        record->rotation.vx += (rand() >> 7) - 128;
        record->rotation.vz = 0;
        effect_play_spatial_sound(record, 0x29);
        break;
    }
    case KF_EFFECT_KIND_42: {
        record->render_flags = KF_EFFECT_RENDER_VISIBLE;
        record->animation_clip = KF_ANIMATION_CLIP_STATIC_OBJECT_FIRST;
        record->render_queue_mode = KF_RENDER_QUEUE_BLEND_ADD;
        record->base_render_id = KF_EFFECT_MODEL_17;
        record->render_id = KF_EFFECT_MODEL_17;
        record->lighting_override_index = KF_LIGHTING_EFFECT;
        record->lighting_blend_q12 = KF_FIXED12_ONE;
        record->scale_z = 0;
        record->scale_y = 0;
        record->scale_x = 0;
        record->cache_tail.payload.kind42.ticks_remaining = va[1];
        break;
    }
    case KF_EFFECT_KIND_113:
    case KF_EFFECT_KIND_115: {
        const SVECTOR *angles;

        record->render_flags = KF_EFFECT_RENDER_VISIBLE;
        record->animation_clip = KF_ANIMATION_CLIP_STATIC_OBJECT_FIRST;
        record->render_queue_mode = KF_RENDER_QUEUE_BLEND_ADD;
        record->base_render_id = KF_EFFECT_MODEL_13;
        record->render_id = KF_EFFECT_MODEL_13;
        record->lighting_override_index = KF_LIGHTING_EFFECT;
        record->lighting_blend_q12 = KF_FIXED12_ONE;
        record->updates_remaining = 45;
        angles = (const SVECTOR *)va[1];
        record->rotation = *angles;
        record->scale_z = 5000;
        record->scale_y = 5000;
        record->scale_x = 5000;
        effect_play_spatial_sound(record, 0x29);
        break;
    }
    case KF_EFFECT_KIND_46: {
        KfEffectKind46State *kind46 = &record->cache_tail.payload.kind46;
        u8 parameter;

        effect_pool_initialize_scaled(record, KF_EFFECT_MODEL_16, KF_FIXED12_ONE);
        record->scale_y = 0;
        kind46->phase = KF_EFFECT_STAGE_TRACK;
        parameter = va[1];
        record->direction.vy = 0;
        kind46->linked_effect_index = parameter;
        break;
    }
    case KF_EFFECT_KIND_45: {
        record->render_flags = KF_EFFECT_RENDER_VISIBLE;
        record->animation_clip = KF_ANIMATION_CLIP_STATIC_OBJECT_FIRST;
        record->render_queue_mode = KF_RENDER_QUEUE_BLEND_ADD;
        record->base_render_id = KF_EFFECT_MODEL_48;
        record->render_id = KF_EFFECT_MODEL_48;
        record->lighting_override_index = KF_LIGHTING_EFFECT;
        record->lighting_blend_q12 = KF_FIXED12_ONE;
        record->scale_z = 0;
        record->scale_y = 0;
        record->scale_x = 0;
        effect_play_spatial_sound(record, 0x17);
        record->cache_tail.payload.scale_step_argument.scale_step = va[1];
        break;
    }
    case KF_EFFECT_KIND_116:
        record->render_flags = KF_EFFECT_RENDER_HIDDEN;
        record->updates_remaining = 20;
        break;
    case KF_EFFECT_KIND_117:
        record->base_render_id = KF_EFFECT_MODEL_49;
        record->render_id = KF_EFFECT_MODEL_49;
        record->render_flags = KF_EFFECT_RENDER_VISIBLE;
        record->animation_clip = KF_ANIMATION_CLIP_STATIC_OBJECT_FIRST;
        record->render_queue_mode = KF_RENDER_QUEUE_BLEND_ADD;
        record->lighting_override_index = KF_LIGHTING_EFFECT;
        record->lighting_blend_q12 = KF_FIXED12_ONE;
        record->updates_remaining = 50;
        goto initialize_angles_34_35_117;
    case KF_EFFECT_KIND_40: {
        const SVECTOR *angles;

        record->base_render_id = KF_EFFECT_MODEL_10;
        record->render_id = KF_EFFECT_MODEL_10;
        record->render_flags = KF_EFFECT_RENDER_VISIBLE;
        record->animation_clip = KF_ANIMATION_CLIP_STATIC_OBJECT_FIRST;
        record->render_queue_mode = KF_RENDER_QUEUE_BLEND_ADD;
        record->lighting_override_index = KF_LIGHTING_EFFECT;
        record->lighting_blend_q12 = KF_FIXED12_ONE;
        record->updates_remaining = 50;
        angles = (const SVECTOR *)va[1];
        record->rotation = *angles;
        effect_play_spatial_sound(record, 0x32);
        break;
    }
    case KF_EFFECT_KIND_38:
    case KF_EFFECT_KIND_39: {
        const SVECTOR *angles;
        s32 random_x;

        record->render_flags = KF_EFFECT_RENDER_VISIBLE;
        record->animation_clip = KF_ANIMATION_CLIP_STATIC_OBJECT_FIRST;
        record->base_render_id = KF_EFFECT_MODEL_44;
        record->render_id = KF_EFFECT_MODEL_44;
        record->updates_remaining = 50;
        angles = (const SVECTOR *)va[1];
        record->rotation = *angles;
        record->rotation.vy += (rand() >> 6) - 256;
        random_x = rand();
        record->rotation.vz = 0;
        record->cache_tail.payload.collision_latch.impact_handled = 0;
        record->scale_z = 0x2000;
        record->scale_y = 0x2000;
        record->scale_x = 0x2000;
        record->rotation.vx += (random_x >> 6) - 256;
        effect_play_spatial_sound(record, 0x20);
        break;
    }
    case KF_EFFECT_KIND_34: {
        const SVECTOR *angles;
        KF_ENUM_PROMOTED(KfEffectRenderId) render_id;

        render_id = KF_EFFECT_MODEL_40;
        goto initialize_34_35;
    case KF_EFFECT_KIND_35:
        render_id = KF_EFFECT_MODEL_41;
    initialize_34_35:
        record->base_render_id = render_id;
        record->render_id = render_id;
        record->render_flags = KF_EFFECT_RENDER_VISIBLE;
        record->animation_clip = KF_ANIMATION_CLIP_STATIC_OBJECT_FIRST;
        record->render_queue_mode = KF_RENDER_QUEUE_BLEND_ADD;
        record->lighting_override_index = KF_LIGHTING_EFFECT;
        record->lighting_blend_q12 = KF_FIXED12_ONE;
        record->updates_remaining = 45;
    initialize_angles_34_35_117:
        angles = (const SVECTOR *)va[1];
        record->rotation = *angles;
        record->cache_tail.payload.collision_latch.impact_handled = 0;
        effect_play_spatial_sound(record, 0x20);
        break;
    }
    case KF_EFFECT_KIND_50:
        record->render_flags = KF_EFFECT_RENDER_VISIBLE;
        record->animation_clip = KF_ANIMATION_CLIP_STATIC_OBJECT_FIRST;
        record->render_queue_mode = KF_RENDER_QUEUE_BLEND_ADD;
        record->lighting_override_index = KF_LIGHTING_EFFECT;
        record->lighting_blend_q12 = KF_FIXED12_ONE;
        record->base_render_id = KF_EFFECT_MODEL_11;
        record->render_id = KF_EFFECT_MODEL_11;
        record->scale_z = 0;
        record->scale_y = 0;
        record->scale_x = 0;
        record->cache_tail.payload.kind50.stage = KF_EFFECT_STAGE_CHARGE;
        effect_play_spatial_sound(record, 0x18);
        break;
    case KF_EFFECT_KIND_101: {
        KfEffectKind101Motion *motion =
            &record->cache_tail.payload.kind101;
        s32 scale = va[1];
        KF_ENUM_PROMOTED(KfEffectRenderId) render_id =
            KF_ENUM_DECODE(KF_ENUM_PROMOTED(KfEffectRenderId), va[4]);

        effect_pool_initialize_scaled(record, render_id, scale);
        motion->scale_step = *(u16 *)(va + 2);
        record->updates_remaining = *(u16 *)(va + 3);
        motion->vertical_step = *(u16 *)(va + 5);
        break;
    }
    case KF_EFFECT_KIND_102: {
        u16 scale;
        s32 volume;
        s16 slot;

        record->render_flags = KF_EFFECT_RENDER_VISIBLE;
        record->animation_clip = KF_ANIMATION_CLIP_STATIC_OBJECT_FIRST;
        record->base_render_id = KF_EFFECT_MODEL_12;
        record->render_id = KF_EFFECT_MODEL_12;
        record->scale_y = 0;
        scale = va[1];
        record->scale_z = scale;
        record->scale_x = scale;
        record->cache_tail.payload.kind102.amplitude = va[2];
        if ((s32)(effect_kind102_sound_cooldown_frame - cd_state.frame_count) >= 0) {
            break;
        }
        effect_kind102_sound_cooldown_frame = cd_state.frame_count + 30;
        volume = record->cache_tail.payload.kind102.amplitude / 90;
        if (volume >= 128) {
            volume = 127;
        }
        slot = audio_state.voices.params[236].vab_slot_index;
        if (slot != KF_AUDIO_VAB_SLOT_NONE && audio_state.vab_slots[slot].vab_id != KF_AUDIO_VAB_ID_NONE &&
            audio_state.vab_slots[slot].vab_id != KF_AUDIO_VAB_ID_STREAM_PENDING) {
            audio_play_spatial_range(0xec, &record->position, volume, 28000,
                                     0x7148, 0);
            break;
        }
        slot = audio_state.voices.params[239].vab_slot_index;
        if (slot != KF_AUDIO_VAB_SLOT_NONE && audio_state.vab_slots[slot].vab_id != KF_AUDIO_VAB_ID_NONE &&
            audio_state.vab_slots[slot].vab_id != KF_AUDIO_VAB_ID_STREAM_PENDING) {
            audio_play_spatial_range(0xef, &record->position, volume, 28000,
                                     0x7148, 0);
        }
        break;
    }
    case KF_EFFECT_KIND_DEFENSE_BOOST:
        effect_pool_initialize_fixed(record, KF_EFFECT_MODEL_25);
        break;
    case KF_EFFECT_KIND_ATTACK_BOOST:
        effect_pool_initialize_fixed(record, KF_EFFECT_MODEL_26);
        break;
    case KF_EFFECT_KIND_16:
        record->render_flags = KF_EFFECT_RENDER_HIDDEN;
        record->updates_remaining = 8;
        audio_play_sound(0x2b, 120);
        break;
    case KF_EFFECT_KIND_14:
    case KF_EFFECT_KIND_19:
        record->render_flags = KF_EFFECT_RENDER_HIDDEN;
        record->updates_remaining = 16;
        audio_play_sound(0x2b, 120);
        break;
    case KF_EFFECT_KIND_22:
        effect_pool_initialize_scaled(record, KF_EFFECT_MODEL_30, KF_FIXED12_ONE);
        record->cooldown = 3;
        record->updates_remaining = 30;
        record->direction.vx += (rand() >> 9) - 32;
        record->direction.vy += (rand() >> 9) - 32;
        record->direction.vz += (rand() >> 9) - 32;
        break;
    case KF_EFFECT_KIND_3:
        record->render_flags = KF_EFFECT_RENDER_VISIBLE;
        record->animation_clip = KF_ANIMATION_CLIP_STATIC_OBJECT_FIRST;
        record->render_queue_mode = KF_RENDER_QUEUE_BLEND_ADD;
        record->base_render_id = KF_EFFECT_MODEL_11;
        record->render_id = KF_EFFECT_MODEL_11;
        record->lighting_override_index = KF_LIGHTING_EFFECT;
        record->lighting_blend_q12 = KF_FIXED12_ONE;
        record->scale_z = 0;
        record->scale_y = 0;
        record->scale_x = 0;
        effect_play_spatial_sound(record, 0x1f);
        break;
    case KF_EFFECT_KIND_114: {
        VECTOR candidate_position;

        record->render_flags = KF_EFFECT_RENDER_VISIBLE;
        record->animation_clip = KF_ANIMATION_CLIP_STATIC_OBJECT_FIRST;
        record->base_render_id = KF_EFFECT_MODEL_38;
        record->render_id = KF_EFFECT_MODEL_38;
        record->updates_remaining = 45;
        record->cache_tail.payload.kind114.origin_y = record->position.vy;
        candidate_position.vx = record->position.vx + (rand() >> 5) - 512;
        candidate_position.vz = record->position.vz + (rand() >> 5) - 512;
        if (collision_query_shapes_with_layer_sample(candidate_position.vx, record->position.vy,
                          candidate_position.vz, 10, 10) != KF_COLLISION_HIT_NONE) {
            candidate_position.vx = record->position.vx;
            candidate_position.vz = record->position.vz;
        }
        record->position.vx = candidate_position.vx - 2730;
        record->position.vz = candidate_position.vz - 2730;
        record->position.vy -= 16384;
        record->direction.vx = 100;
        record->direction.vy = 600;
        record->direction.vz = 100;
        record->scale_z = 0x4000;
        record->scale_y = 0x4000;
        record->scale_x = 0x4000;
        break;
    }
    case KF_EFFECT_KIND_48: {
        const SVECTOR *angles;
        KF_ENUM_PROMOTED(KfEffectRenderId) render_id;

        render_id = KF_EFFECT_MODEL_42;
        goto setup_render_id;
    case KF_EFFECT_KIND_47:
        render_id = KF_EFFECT_MODEL_43;
        goto setup_render_id;
    case KF_EFFECT_KIND_30:
        render_id = KF_EFFECT_MODEL_29;
        goto setup_render_id;
    case KF_EFFECT_KIND_29:
    case KF_EFFECT_KIND_31:
        render_id = KF_EFFECT_MODEL_28;
    setup_render_id:
        record->base_render_id = render_id;
        record->render_flags = KF_EFFECT_RENDER_VISIBLE;
        record->animation_clip = KF_ANIMATION_CLIP_STATIC_OBJECT_FIRST;
        record->lighting_override_index = KF_LIGHTING_NONE;
        record->render_id = record->base_render_id;
        angles = (const SVECTOR *)va[1];
        record->rotation = *angles;
        record->rotation.vz = 0;
        record->cache_tail.payload.ballistic.age = 0;
        break;
    }
    case KF_EFFECT_KIND_23: {
        effect_pool_initialize_scaled(record, KF_EFFECT_MODEL_8, 0x400);
        record->direction.vz = 0;
        record->direction.vy = 0;
        record->direction.vx = 0;
        record->phase = 9;
        record->cache_tail.payload.kind23.actor_index = va[1];
        record->cache_tail.payload.kind23.vertex_index = va[2];
        third_parameter = va[3];
        record->cache_tail.payload.kind23.release_animation_phase = third_parameter;
        effect_play_spatial_sound(record, 0x26);
        break;
    }
    case KF_EFFECT_KIND_24:
        record->render_flags = KF_EFFECT_RENDER_HIDDEN;
        record->updates_remaining = 70;
        break;
    case KF_EFFECT_KIND_109: {
        KfEffectKind109Target *target = &record->cache_tail.payload.kind109;

        effect_pool_initialize_scaled(record, KF_EFFECT_MODEL_14, 0x400);
        record->updates_remaining = 20;
        target->effect_index = va[1];
        record->direction.vx += (rand() >> 8) - 64;
        record->direction.vy += (rand() >> 8) - 64;
        record->direction.vz += (rand() >> 8) - 64;
        break;
    }
    case KF_EFFECT_KIND_120:
        effect_pool_initialize_scaled(record, KF_EFFECT_MODEL_50, 0);
        record->updates_remaining = 100;
        if (rand() < 4096) {
            effect_play_spatial_sound(record, 0x21);
        }
        break;
    /* The remaining kinds reach the retail table's free-slot sentinel.
     * The table dispatch itself remains indirect. */
    default:
        record->type = KF_EFFECT_SLOT_FREE;
        break;
    }
finish:
    return record;
}

ADDRESS(0x80042424, 0x90)
void effect_rotate_scale_offset_y(const SVECTOR *offset, VECTOR *output, s16 angle, s32 scale)
{
    SVECTOR scaled;
    SVECTOR rotation;
    MATRIX matrix;

    setVector(&scaled,
              (offset->vx * scale) >> KF_FIXED12_BITS,
              0,
              (offset->vz * scale) >> KF_FIXED12_BITS);
    setVector(&rotation, 0, angle, 0);
    RotMatrix(&rotation, &matrix);
    ApplyMatrix(&matrix, &scaled, output);
}

ADDRESS(0x800424b4, 0x1e0)
KF_ENUM_PARAM(KfEffectMotionResult, s32) effect_move_probe(s32 scale, s32 max_length, s32 probe_radius,
                      s32 probe_height_flags, SVECTOR *motion)
{
    KfEffectRecord *record = effect_state.current_record;
    s32 length;

    vector3s_scale_shift12(scale, motion);
    addVector(&record->direction, motion);
    length = fixed_vector3_length(record->direction.vx, record->direction.vy,
                                  record->direction.vz);
    if (length > max_length) {
        record->direction.vx = record->direction.vx * max_length / length;
        record->direction.vy = record->direction.vy * max_length / length;
        record->direction.vz = record->direction.vz * max_length / length;
    }
    addVector(&record->position, &record->direction);
    if (probe_height_flags == -1) {
        return KF_EFFECT_MOTION_CLEAR;
    }
    /* Any hit maps to BLOCKED (-1). */
    return KF_ENUM_DECODE(KF_ENUM_PROMOTED(KfEffectMotionResult),
                          -!!KF_ENUM_ENCODE(s32, effect_probe_collision_by_type(&record->position,
                              probe_radius,
                                                                               probe_height_flags)));
}

ADDRESS(0x80042694, 0x1b8)
KF_ENUM_PARAM(KfEffectMotionResult, s32) effect_aim_and_move(s32 max_length, s32 scale, s32 turn_step,
                        s32 probe_radius, s32 probe_height_flags, s32 proximity,
                        s32 close_scale, s32 target_filter)
{
    KfEffectRecord *record = effect_state.current_record;
    VECTOR target_position;
    struct KfEulerAngles target_angles;
    SVECTOR motion;
    s32 distance;
    KfActor *target;

    if ((record->type & KF_EFFECT_USE_PLAYER_MAGIC) == KF_EFFECT_TYPE_NONE) {
        goto player_target;
    }
    target = actor_find_best_in_cone(&record->position, record->rotation.vy,
                           record->rotation.vx, 24000, target_filter,
                           target_filter, &distance, 0);
    if (target == NULL) {
        goto move;
    }
    target_position.vx = target->position.vx;
    target_position.vy = target->position.vy - (target->collision_height >> 1);
    target_position.vz = target->position.vz;

aim:
    vector_displacement_to_pitch_yaw(target_position.vx - record->position.vx,
                  target_position.vy - record->position.vy,
                  target_position.vz - record->position.vz,
                  &target_angles);
    record->rotation.vx = angle_approach(record->rotation.vx,
                                        target_angles.x, turn_step);
    record->rotation.vy = angle_approach(record->rotation.vy,
                                        target_angles.y, turn_step);
    goto move;

player_target:
    distance = player_distance_to_point_in_cone(
        &record->position, record->rotation.vy, 24000, 0x1000);
    if (distance != -1) {
        target_position.vx = player_state.camera_position.vx;
        target_position.vy = player_state.camera_position.vy - KF_PLAYER_CAMERA_EYE_OFFSET;
        target_position.vz = player_state.camera_position.vz;
        goto aim;
    }

move:
    pitch_yaw_to_forward_vector((const struct KfEulerAngles *)&record->rotation,
                                &motion);
    if (distance >= 0 && distance <= proximity) {
        scale = close_scale;
    }
    return effect_move_probe(scale, max_length, probe_radius, probe_height_flags, &motion);
}

ADDRESS(0x8004284c, 0x1bc)
KF_ENUM_PARAM(KfEffectMotionResult, s32) effect_target_motion(const VECTOR *target, s32 max_length, s32 scale,
                         s32 settle_distance, s32 min_distance, s32 probe_radius,
                         s32 probe_height_flags)
{
    KfEffectRecord *record = effect_state.current_record;
    VECTOR delta;
    SVECTOR motion;
    s32 length;

    delta.vx = target->vx - record->position.vx;
    delta.vy = target->vy - record->position.vy;
    delta.vz = target->vz - record->position.vz;
    length = fixed_vector3_length(delta.vx, delta.vy, delta.vz);
    if (length <= min_distance) {
        return KF_EFFECT_MOTION_ARRIVED;
    }
    if (length <= settle_distance) {
        record->direction.vx = fixed_lerp_q12(0, record->direction.vx, 0xc00);
        record->direction.vy = fixed_lerp_q12(0, record->direction.vy, 0xc00);
        record->direction.vz = fixed_lerp_q12(0, record->direction.vz, 0xc00);
    }
    motion.vx = (delta.vx << KF_FIXED12_BITS) / length;
    motion.vy = (delta.vy << KF_FIXED12_BITS) / length;
    motion.vz = (delta.vz << KF_FIXED12_BITS) / length;
    return effect_move_probe(scale, max_length, probe_radius, probe_height_flags, &motion);
}

ADDRESS(0x80042a08, 0xac)
void effect_scale_step(s32 multiplier, s32 limit, s32 increment, s32 arg3, s32 arg5)
{
    KfEffectRecord *record = effect_state.current_record;
    s32 scaled_size;

    record->scale_y = record->scale_z = record->scale_x += increment;
    scaled_size = (multiplier * record->scale_x) >> KF_FIXED12_BITS;
    effect_apply_radial_magic_damage(&record->position,
                  scaled_size - ((multiplier * increment) >> KF_FIXED12_BITS),
                  scaled_size - 1, arg5, arg3, 0x1000);
    if (record->scale_x >= limit) {
        record->type = KF_EFFECT_SLOT_FREE;
    }
}

ADDRESS(0x80042ab4, 0x90)
void effect_spawn_zero_direction(KfEffectRecord *record, s32 mode)
{
    VECTOR position;
    SVECTOR direction;
    KfEffectRecord *spawned;

    effect_sample_world_vertex(record, mode, &position, (const SVECTOR *)&record->scale_x);
    direction.vx = record->rotation.vx;
    direction.vy = record->rotation.vy + (rand() >> 7) - 128;
    direction.vz = record->rotation.vz;
    spawned = effect_construct_record(10, record->type, KF_EFFECT_KIND_100, &position,
                            &effect_zero_direction, &direction);
    if (spawned != NULL) {
        spawned->phase = 2;
    }
}

ADDRESS(0x80042b44, 0x88)
b32 effect_spawn_at_lower_bound(const VECTOR *position, s32 arg1, s32 arg2,
                                s32 vertical_window)
{
    s32 lower_bound = bss_801c7540.collision_cache.heights.lower_bound;
    VECTOR spawn_position;
    SVECTOR direction;

    if (position->vy < lower_bound) {
        return KF_FALSE;
    }
    if (position->vy > vertical_window + lower_bound) {
        return KF_FALSE;
    }

    spawn_position.vx = position->vx;
    spawn_position.vz = position->vz;
    spawn_position.vy = lower_bound;
    effect_construct_record(10, KF_EFFECT_TYPE_NONE, KF_EFFECT_KIND_102, &spawn_position, &direction, arg1,
                            arg2);
    return KF_TRUE;
}

ADDRESS(0x80042bcc, 0x298)
void effect_spawn_motion(KfEffectRecord *record, s32 position_mode,
                   s32 motion_mode, s32 arg3, s32 arg4, s32 arg5,
                   s32 arg6, s32 arg7, ...)
{
    s32 *args;
    SVECTOR motion;
    VECTOR position_delta;
    VECTOR transformed;

    /* O32 places the remaining effect-mode operands after arg7. */
    args = &arg7;
    if (position_mode == -1) {
        position_delta.vx = position_delta.vy = position_delta.vz = 0;
    } else {
        effect_sample_rotated_vertex(record, position_mode, &position_delta,
                      (const SVECTOR *)&record->scale_x);
    }

    switch (motion_mode) {
    case -1:
        effect_sample_rotated_vertex(record, args[2], &transformed,
                      (const SVECTOR *)args[3]);
        copyVector(&motion, &transformed);
        motion_mode = args[1];
        break;
    case -2:
        motion.vx = (rand() * args[1] >> 15) + (u16)args[2];
        motion.vy = (rand() * args[3] >> 15) + (u16)args[4];
        motion.vz = (rand() * args[5] >> 15) + (u16)args[6];
        goto spawn;
    case -3:
        motion.vx = motion.vy = motion.vz = 0;
        motion_mode = args[1];
        break;
    default:
        pitch_yaw_to_forward_vector((const struct KfEulerAngles *)&record->rotation,
                                    &motion);
        vector3s_scale_shift12(30, &motion);
        motion.vx = -motion.vx;
        motion.vy = -motion.vy;
        motion.vz = -motion.vz;
        break;
    }

    motion.vx += ((record->direction.vx * motion_mode) >> 12);
    motion.vy += ((record->direction.vy * motion_mode) >> 12);
    motion.vz += ((record->direction.vz * motion_mode) >> 12);

spawn:
    addVector(&position_delta, &record->position);
    effect_construct_record(10, KF_EFFECT_TYPE_NONE, KF_EFFECT_KIND_101, &position_delta, &motion,
                  arg3, arg4, arg5, arg6, arg7);
}

ADDRESS(0x80042e64, 0x16c)
b32 effect_scatter_lower_bound(const VECTOR *origin, s32 count, s32 spread,
                  s32 scale_x, s32 scale_z, s32 variation)
{
    s32 lower_bound = bss_801c7540.collision_cache.heights.lower_bound;
    s32 offset_x = 0;
    s32 offset_z = 0;

    if (origin->vy >= lower_bound) {
        if (origin->vy > lower_bound + 500) {
            return KF_FALSE;
        }
        count--;
        if (count != -1) {
            do {
            VECTOR position;
            SVECTOR direction;
            s32 magnitude;

            position.vx = origin->vx + offset_x;
            position.vz = origin->vz + offset_z;
            position.vy = bss_801c7540.collision_cache.heights.lower_bound;
            magnitude = random_centered_triangular_scaled(variation) + 4096;
            effect_construct_record(10, KF_EFFECT_TYPE_NONE, KF_EFFECT_KIND_102, &position, &direction,
                          scale_x * magnitude >> KF_FIXED12_BITS,
                          scale_z * magnitude >> KF_FIXED12_BITS);

            count--;
            offset_x = (rand() * spread >> 14) - spread;
            offset_z = (rand() * spread >> 14) - spread;
            } while (count != -1);
        }
        return KF_TRUE;
    }
    return KF_FALSE;
}

ADDRESS(0x80042fd0, 0x18c)
KF_ENUM_PARAM(KfCollisionHitFlags, s32) effect_collision_step(s32 radius, s32 angle, s32 step)
{
    KfEffectRecord *record = effect_state.current_record;
    VECTOR previous;
    KF_ENUM_PARAM(KfCollisionHitFlags, s32) result;
    KfMapLayerMask next_layer;

    addVector(&record->position, &record->direction);
    result = effect_probe_collision_by_type(&record->position, radius, angle);
    if (record->midpoint_collision_enabled) {
        effect_collision_motion_step.vx = record->direction.vx >> 1;
        effect_collision_motion_step.vy = record->direction.vy >> 1;
        effect_collision_motion_step.vz = record->direction.vz >> 1;
        if (result == KF_COLLISION_HIT_NONE) {
            previous.vx = record->position.vx - effect_collision_motion_step.vx;
            previous.vy = record->position.vy - effect_collision_motion_step.vy;
            previous.vz = record->position.vz - effect_collision_motion_step.vz;
            result = effect_probe_collision_by_type(&previous, radius, angle);
            if (result != KF_COLLISION_HIT_NONE) {
                copyVector(&effect_collision_motion_step, &record->direction);
            }
        }
    }
    next_layer = KF_MAP_LAYER_SECOND;
    if (bss_801c7540.collision_cache.layer == 0) {
        next_layer = KF_MAP_LAYER_FIRST;
    }
    record->map_layer_mask = next_layer;
    record->rotation.vz = (record->rotation.vz - step) & KF_ANGLE_WRAP_MASK;
    return result;
}

ADDRESS(0x8004315c, 0xcc)
void effect_collision_backtrack(void)
{
    KfEffectRecord *record = effect_state.current_record;

    if (record->midpoint_collision_enabled) {
        record->position.vx -= effect_collision_motion_step.vx;
        record->position.vy -= effect_collision_motion_step.vy;
        record->position.vz -= effect_collision_motion_step.vz;
    } else {
        record->position.vx -= record->direction.vx;
        record->position.vy -= record->direction.vy;
        record->position.vz -= record->direction.vz;
    }
    record->position.vx -= record->direction.vx;
    record->position.vy -= record->direction.vy;
    record->position.vz -= record->direction.vz;
}

ADDRESS(0x80043228, 0x160)
void effect_spawn_radial_ring(s32 count, s32 radius, s32 vertical_angle, s32 arg3)
{
    KfEffectRecord *record = effect_state.current_record;
    SVECTOR direction;
    VECTOR position;
    s32 angle = rand();
    s32 angle_step = KF_ANGLE_FULL_TURN / count;

    position.vx = record->position.vx - record->direction.vx;
    position.vy = record->position.vy - record->direction.vy;
    position.vz = record->position.vz - record->direction.vz;
    direction.vy = vertical_angle;
    count--;
    while (count != -1) {
        direction.vx = (rcos(angle) * radius) >> KF_FIXED12_BITS;
        direction.vz = (rsin(angle) * radius) >> KF_FIXED12_BITS;
        angle += angle_step;
        count--;
        effect_construct_record(10, record->type | KF_EFFECT_TARGET_ACTORS_AND_PLAYER, KF_EFFECT_KIND_8,
                                &position, &direction,
                      effect_state.current_index, arg3);
    }
}

ADDRESS(0x80043388, 0x367c)
void effect_update_dispatch(void)
{
    KfEffectRecord *record = effect_state.current_record;
    KfMagicRecord *magic = effect_state.current_magic;
    KF_ENUM_STORAGE(KfEffectKind, u32) initial_kind = record->kind;
    s32 initial_phase = record->phase;
    KF_ENUM_PARAM(KfCollisionHitFlags, s32) collision;
    KF_ENUM_PROMOTED(KfEffectMotionResult) motion;
    VECTOR work_position;
    VECTOR work_target;

    switch (initial_kind) {
    case KF_EFFECT_KIND_29:
    case KF_EFFECT_KIND_31:
    case KF_EFFECT_KIND_48: {
        VECTOR midpoint;
        s16 age;
        s32 prior_y;
        s32 acceleration;

        acceleration = 10;
        goto ballistic_update;
    case KF_EFFECT_KIND_30:
    case KF_EFFECT_KIND_47:
        acceleration = 5;
    ballistic_update:
        if (initial_phase != 0) {
            break;
        }
        /* These kinds overlay the record tail with a Y origin and age. */
        if (record->cache_tail.payload.ballistic.age == 0) {
            audio_play_spatial_range(5, &record->position, 110,
                                     28000, 29000, 0);
        }
        age = record->cache_tail.payload.ballistic.age + 1;
        record->cache_tail.payload.ballistic.age = age;
        prior_y = record->position.vy;
        work_position.vy = record->cache_tail.payload.ballistic.origin_y +
                       record->direction.vy * age +
                       ((acceleration * age * age) >> 1);
        work_position.vx = record->position.vx + record->direction.vx;
        work_position.vz = record->position.vz + record->direction.vz;
        collision = effect_probe_collision_by_type(&work_position, 20, 20);
        if (collision == KF_COLLISION_HIT_NONE) {
            midpoint.vx = (work_position.vx + record->position.vx) >> 1;
            midpoint.vy = (work_position.vy + record->position.vy) >> 1;
            midpoint.vz = (work_position.vz + record->position.vz) >> 1;
            collision = effect_probe_collision_by_type(&midpoint, 20, 20);
        }
        record->position.vx = work_position.vx;
        record->position.vy = work_position.vy;
        record->position.vz = work_position.vz;
        effect_spawn_at_lower_bound(&record->position, 0x2000, 0x2000, 500);
        if (collision != KF_COLLISION_HIT_NONE) {
            effect_apply_current_magic_backstep(collision | KF_COLLISION_IMPACT_COUNTS_AS_PHYSICAL);
            record->type = KF_EFFECT_SLOT_FREE;
        } else {
            KfMapLayerMask layer = KF_MAP_LAYER_SECOND;
            if (bss_801c7540.collision_cache.layer == 0) {
                layer = KF_MAP_LAYER_FIRST;
            }
            record->map_layer_mask = layer;
            vector_displacement_to_pitch_yaw(record->direction.vx,
                          record->position.vy - prior_y,
                          record->direction.vz,
                          (struct KfEulerAngles *)&record->rotation);
        }
        break;
    case KF_EFFECT_KIND_7:
    case KF_EFFECT_KIND_49: {
    shared_growth_entry:
        if (initial_phase == 0) {
            collision = effect_collision_step(180, KF_COLLISION_HEIGHT_CHECK_FLOOR, -300);
            if (collision == KF_COLLISION_HIT_NONE) {
                break;
            }
            effect_scatter_lower_bound(&record->position, 3, 400, 0x2000, 0x2000, 0x400);
            effect_collision_backtrack();
            effect_apply_current_magic_backstep(collision);
            record->phase = 1;
            goto shared_growth_update;
        }
        if (initial_phase >= 3) {
            record->type = KF_EFFECT_SLOT_FREE;
            break;
        }
        goto shared_growth_update;
    }
    case KF_EFFECT_KIND_13:
    case KF_EFFECT_KIND_32: {
        if (initial_phase != 0) {
            goto kind13_nonzero_phase;
        }
        collision = effect_collision_step(180, 0, -300);
        if (collision == KF_COLLISION_HIT_NONE) {
            goto kind13_no_collision;
        }
        effect_collision_backtrack();
        effect_apply_current_magic_backstep(collision);
        record->phase = 1;
        goto shared_growth_update;
    kind13_no_collision:
        effect_spawn_at_lower_bound(&record->position, 0x2000, 0x2000, 500);
        break;
    kind13_nonzero_phase:
        if (initial_phase >= 3) {
            record->type = KF_EFFECT_SLOT_FREE;
            break;
        }
    shared_growth_update:
        record->animation_clip = KF_ENUM_DECODE(KF_ENUM_PROMOTED(KfAnimationClip), initial_phase - 128);
        record->scale_z = record->scale_y = record->scale_x = record->scale_x + 2048;
        record->phase++;
        break;
    }
    case KF_EFFECT_KIND_23:
        if (initial_phase == 9) {
            const KfEffectKind23Attachment *attachment = &record->cache_tail.payload.kind23;
            KfActor *actor = &actor_state.actors[attachment->actor_index];
            VECTOR vertex_offset;
            VECTOR actor_position;
            VECTOR old_position;
            VECTOR *position;

            if (actor->lifecycle != KF_ACTOR_LIFECYCLE_ACTIVE || actor->target_type != KF_ACTOR_TARGET_25) {
                record->type = KF_EFFECT_SLOT_FREE;
                break;
            }
            actor_sample_rotated_animation_vertex(actor, attachment->vertex_index,
                          &vertex_offset);
            position = actor_resolve_group_position(actor, &actor_position);
            old_position = record->position;
            record->position.vx = vertex_offset.vx + position->vx;
            record->position.vy = vertex_offset.vy + position->vy;
            record->position.vz = vertex_offset.vz + position->vz;
            record->scale_x += 512;
            if (record->scale_x > 0x1800) {
                record->scale_x = 0x1800;
            }
            record->scale_y = record->scale_z = record->scale_x;
            record->direction.vx = record->position.vx - old_position.vx;
            record->direction.vy = record->position.vy - old_position.vy;
            record->direction.vz = record->position.vz - old_position.vz;
            effect_spawn_motion(record, -1, -700, record->scale_x,
                           -300, 3, 8, 0);
            if (actor->animation_phase >= attachment->release_animation_phase) {
                actor_compute_target_direction(actor, &player_state.camera_position,
                               650, &record->position, &record->direction,
                               KF_ACTOR_PITCH_TRACK_TARGET, 0x400, 5);
                record->phase = 0;
                record->updates_remaining = 50;
            }
            break;
        }
        effect_spawn_motion(record, -1, 0x100, record->scale_x,
                       -300, 2, 8, 0);
        goto shared_growth_entry;
    }
    case KF_EFFECT_KIND_4:
        collision = effect_collision_step(180, 0, 0);
        if (collision != KF_COLLISION_HIT_NONE) {
            if ((collision & KF_COLLISION_HIT_SHAPE_MASK) != KF_COLLISION_HIT_NONE) {
                record->type = KF_EFFECT_SLOT_FREE;
            }
            if (record->cache_tail.payload.collision_latch.impact_handled == 0) {
                record->cache_tail.payload.collision_latch.impact_handled = 1;
                effect_apply_current_magic_backstep(collision);
            }
        } else {
            record->cache_tail.payload.collision_latch.impact_handled = 0;
        }
        record->rotation.vy += 750;
        effect_spawn_at_lower_bound(&record->position, 0x2000, 0x2000, 500);
        break;
    case KF_EFFECT_KIND_34:
    case KF_EFFECT_KIND_35: {
        s32 vertical_step;

        vertical_step = 0;
        goto collision_kind25_update;
    case KF_EFFECT_KIND_25:
        vertical_step = -30;
    collision_kind25_update:
        collision = effect_collision_step(250, 100, vertical_step);
        if (collision != KF_COLLISION_HIT_NONE) {
            if ((collision & KF_COLLISION_HIT_SHAPE_MASK) != KF_COLLISION_HIT_NONE) {
                record->type = KF_EFFECT_SLOT_FREE;
            }
            if (record->cache_tail.payload.collision_latch.impact_handled == 0) {
                record->cache_tail.payload.collision_latch.impact_handled = 1;
                effect_apply_current_magic_backstep(collision);
            }
        } else {
            record->cache_tail.payload.collision_latch.impact_handled = 0;
        }
        effect_spawn_at_lower_bound(&record->position, 0x2000, 0x2000, 500);
        break;
    }
    case KF_EFFECT_KIND_42:
        if (record->cache_tail.payload.kind42.ticks_remaining == 0) {
            effect_scale_step(0x3800, 0x1f8, 0x46, 0x800, 0x8000);
        } else {
            record->cache_tail.payload.kind42.ticks_remaining--;
        }
        break;
    case KF_EFFECT_KIND_115:
        motion = effect_aim_and_move(0x258, 0x28, 0x24, 0xb4,
                                  0x168, 0x1000, 0x104, 0x800);
        if (motion == KF_EFFECT_MOTION_BLOCKED || record->updates_remaining < 2) {
            effect_collision_backtrack();
            effect_construct_record(10, record->type | KF_EFFECT_TARGET_ACTORS_AND_PLAYER, KF_EFFECT_KIND_42,
                           &record->position, NULL, 0);
            effect_construct_record(10, record->type | KF_EFFECT_TARGET_ACTORS_AND_PLAYER, KF_EFFECT_KIND_42,
                           &record->position, NULL, 1);
            effect_construct_record(10, record->type | KF_EFFECT_TARGET_ACTORS_AND_PLAYER, KF_EFFECT_KIND_42,
                           &record->position, NULL, 2);
            effect_play_spatial_sound(record, 0x18);
            record->type = KF_EFFECT_SLOT_FREE;
        } else {
            effect_spawn_at_lower_bound(&record->position, 0x2000, 0x2000, 500);
        }
        break;
    case KF_EFFECT_KIND_113:
        collision = effect_collision_step(180, 360, 0);
        if (collision != KF_COLLISION_HIT_NONE || record->updates_remaining < 2) {
            effect_construct_record(10, record->type | KF_EFFECT_TARGET_ACTORS_AND_PLAYER, KF_EFFECT_KIND_42,
                           &record->position, NULL, 0);
            effect_construct_record(10, record->type | KF_EFFECT_TARGET_ACTORS_AND_PLAYER, KF_EFFECT_KIND_42,
                           &record->position, NULL, 1);
            effect_construct_record(10, record->type | KF_EFFECT_TARGET_ACTORS_AND_PLAYER, KF_EFFECT_KIND_42,
                           &record->position, NULL, 2);
            effect_play_spatial_sound(record, 0x17);
            record->type = KF_EFFECT_SLOT_FREE;
        } else {
            effect_spawn_at_lower_bound(&record->position, 0x2000, 0x2000, 500);
        }
        break;
    case KF_EFFECT_KIND_46: {
        KfEffectRecord *selected =
            &effect_state.records[record->cache_tail.payload.kind46.linked_effect_index];
        s32 height;
        s32 age;

        record->position.vx += record->direction.vx;
        record->position.vz += record->direction.vz;
        switch (record->cache_tail.payload.kind46.phase) {
        case KF_EFFECT_STAGE_TRACK: {
            collision = collision_query_world(record->position.vx, record->position.vy,
                                      record->position.vz, 10,
                                      record->scale_y,
                                      KF_COLLISION_QUERY_ACTORS | KF_COLLISION_QUERY_MAP_OBJECTS);
            effect_apply_current_magic_backstep(collision);
            collision_probe_floor_height(record->position.vx, selected->position.vy,
                          record->position.vz, 0, 0);
            record->position.vy = bss_801c7540.collision_cache.heights.result;
            height = bss_801c7540.collision_cache.heights.result -
                     bss_801c7540.collision_cache.heights.height_limit;
            if (height <= 32767) {
                record->scale_y = height;
            } else {
                record->scale_y = 32767;
            }
            if (selected->type == KF_EFFECT_SLOT_FREE) {
                record->cache_tail.payload.kind46.phase = KF_EFFECT_STAGE_COLLAPSE;
                record->cache_tail.payload.kind46.age_q12 = 0;
                record->scale_threshold.interpolation_start_y = record->scale_y;
            }
            break;
        }
        case KF_EFFECT_STAGE_COLLAPSE: {
            record->scale_y = fixed_lerp_q12(
                record->scale_threshold.interpolation_start_y, 0,
                (s16)record->cache_tail.payload.kind46.age_q12);
            age = record->cache_tail.payload.kind46.age_q12 + 512;
            record->cache_tail.payload.kind46.age_q12 = age;
            if ((s16)age >= KF_FIXED12_ONE) {
                record->type = KF_EFFECT_SLOT_FREE;
            }
            break;
        }
        default:
            break;
        }
        break;
    }
    case KF_EFFECT_KIND_45:
        effect_scale_step(0x4000, record->cache_tail.payload.scale_step_argument.scale_step,
                       0x46, 0x800, 0x8000);
        break;
    case KF_EFFECT_KIND_116: {
        VECTOR midpoint;

        midpoint.vx = record->position.vx + (record->direction.vx >> 1);
        midpoint.vy = record->position.vy + (record->direction.vy >> 1);
        midpoint.vz = record->position.vz + (record->direction.vz >> 1);
        record->position.vx += record->direction.vx;
        record->position.vy += record->direction.vy;
        record->position.vz += record->direction.vz;
        if (collision_query_shapes_with_layer_sample(midpoint.vx, midpoint.vy, midpoint.vz, 5,
                                                     10) != KF_COLLISION_HIT_NONE ||
            collision_query_shapes_with_layer_sample(record->position.vx, record->position.vy,
                           record->position.vz, 5, 10) != KF_COLLISION_HIT_NONE) {
            record->type = KF_EFFECT_SLOT_FREE;
        }
        effect_construct_record(10, record->type, KF_EFFECT_KIND_45, &record->position, NULL, 0x1a4);
        break;
    }
    case KF_EFFECT_KIND_117: {
        s32 actor_index;

        record->position.vx += record->direction.vx;
        record->position.vy += record->direction.vy;
        record->position.vz += record->direction.vz;
        work_position.vx = record->position.vx;
        work_position.vy = record->position.vy + 5000;
        work_position.vz = record->position.vz;
        actor_index = actor_find_overlap_excluding_target_type3(work_position.vx, work_position.vy, work_position.vz,
                                    100, 10000);
        if (actor_index != KF_ACTOR_INDEX_NONE) {
            effect_construct_record(10, record->type, KF_EFFECT_KIND_45,
                           &actor_state.actors[actor_index].position, NULL,
                           0x4ec);
        }
        effect_spawn_at_lower_bound(&work_position, 0x2000, 0x7fff, 10000);
        break;
    }
    case KF_EFFECT_KIND_40:
        collision = effect_collision_step(100, 200, 0);
        if (collision != KF_COLLISION_HIT_NONE) {
            effect_apply_current_magic_backstep(collision);
            record->type = KF_EFFECT_SLOT_FREE;
        } else {
            effect_spawn_at_lower_bound(&record->position, 0x2000, 0x2000, 500);
        }
        break;
    case KF_EFFECT_KIND_39: {
        s32 count;
        KfCollisionHitFlags flags;

        if (record->updates_remaining < 45) {
            goto kind39_spawn;
        }
        record->direction.vy += 10;
    case KF_EFFECT_KIND_38:
        if (effect_collision_step(100, 0, 0) != KF_COLLISION_HIT_NONE) {
            goto kind38_response;
        }
        goto kind38_finish;
    kind39_spawn:
        if (effect_aim_and_move(0x258, 0x28, 0x24, 0x32,
                          100, 0x1000, 0x104, 0x800) != KF_EFFECT_MOTION_BLOCKED) {
            goto kind38_finish;
        }
    kind38_response:
        {
            flags = bss_801c7540.collision_cache.flags;
            if ((flags & KF_COLLISION_HIT_ACTOR) != KF_COLLISION_HIT_NONE) {
                if (record->cache_tail.payload.collision_latch.impact_handled == 0) {
                    effect_apply_current_magic_backstep(flags);
                } else {
                    record->cache_tail.payload.collision_latch.impact_handled = 1;
                }
            } else {
                record->cache_tail.payload.collision_latch.impact_handled = 0;
            }
            for (count = 11; count != -1; count--) {
                effect_spawn_motion(record, -1, -2, 0xc00, -90, 16, 14, 5,
                               0x200, -256, 0x200, -256, 0x200, -256);
            }
            if ((bss_801c7540.collision_cache.flags & KF_COLLISION_HIT_SHAPE_MASK) != KF_COLLISION_HIT_NONE) {
                record->type = KF_EFFECT_SLOT_FREE;
            }
        }
    kind38_finish:
        effect_spawn_at_lower_bound(&record->position, 0x2000, 0x2000, 500);
        break;
    }
    case KF_EFFECT_KIND_50: {
        s32 distance;

        switch (record->cache_tail.payload.kind50.stage) {
        case KF_EFFECT_STAGE_CHARGE:
            record->scale_x = record->scale_y = record->scale_z = record->scale_z + 64;
            player_sample_weapon_world_vertex(0, &record->position);
            if (record->scale_x >= 256) {
                record->cache_tail.payload.kind50.stage = KF_EFFECT_STAGE_FLIGHT;
                player_probe_view_target_and_vectors(1000, NULL, &record->direction, &distance);
            }
            break;
        case KF_EFFECT_STAGE_FLIGHT:
            if (effect_collision_step(512, KF_COLLISION_HEIGHT_CHECK_FLOOR | 0x200,
                                      0) != KF_COLLISION_HIT_NONE) {
                record->cache_tail.payload.kind50.stage = KF_EFFECT_STAGE_BURST;
                effect_apply_radial_magic_damage(&record->position, 0, 0x400,
                               KF_RADIAL_MODE_REACH, 0x1000, 0x1000);
            }
            break;
        case KF_EFFECT_STAGE_BURST:
            if (record->scale_x >= 512) {
                record->type = KF_EFFECT_SLOT_FREE;
            }
            record->scale_x = record->scale_y = record->scale_z = record->scale_z + 64;
            break;
        }
        break;
    }
    case KF_EFFECT_KIND_28: {
        s32 radius;
        KfEffectStage stage;

        radius = 250;
        goto kind_one_update;
    case KF_EFFECT_KIND_1:
        radius = 500;
    kind_one_update:
        stage = record->cache_tail.payload.kind1.collision_stage;
        if (stage == KF_EFFECT_STAGE_FADE) {
            record->lighting_blend_q12 += 256;
            if (record->lighting_blend_q12 >= KF_FIXED12_ONE) {
                record->type = KF_EFFECT_SLOT_FREE;
            }
            break;
        }
        if (stage == KF_EFFECT_STAGE_BOUNCED) {
            record->direction.vy += 13;
        }
        record->rotation.vx += 200;
        collision = effect_collision_step(radius, radius * 2, 250);
        if (collision != KF_COLLISION_HIT_NONE) {
            effect_apply_current_magic_backstep(collision);
            if (record->cache_tail.payload.kind1.collision_stage == KF_EFFECT_STAGE_BOUNCED) {
                record->cache_tail.payload.kind1.collision_stage = KF_EFFECT_STAGE_FADE;
                record->render_queue_mode = KF_RENDER_QUEUE_BLEND_ADD;
                record->lighting_override_index = KF_LIGHTING_PRESET_42;
                record->lighting_blend_q12 = 0x400;
                break;
            }
            effect_collision_backtrack();
            record->cache_tail.payload.kind1.collision_stage = KF_EFFECT_STAGE_BOUNCED;
            record->direction.vz = 0;
            record->direction.vx = 0;
            record->direction.vy = -100;
        }
        effect_spawn_at_lower_bound(&record->position, 0x4000, 0x4000, 500);
        break;
    }
    case KF_EFFECT_KIND_26:
    case KF_EFFECT_KIND_27:
        record->type = KF_EFFECT_SOURCE_HAZARD | KF_EFFECT_TARGET_PLAYER;
        record->rotation.vy += 100;
        effect_apply_radial_magic_damage(&record->position, 0, record->scale_x,
                       KF_RADIAL_MODE_REACH, 0x400, 0x1000);
        record->type = KF_EFFECT_SOURCE_HAZARD | KF_EFFECT_TARGET_SHAPES_ONLY;
        switch (record->cache_tail.payload.kind26.stage) {
        case KF_EFFECT_STAGE_TRAVEL:
            if (effect_collision_step(100, 200, 0) != KF_COLLISION_HIT_NONE) {
                record->direction.vz = 0;
                record->direction.vy = 0;
                record->direction.vx = 0;
            }
            break;
        case KF_EFFECT_STAGE_SHRINK:
            record->scale_z = record->scale_z - 512;
            if (record->scale_z <= 0) {
                record->type = KF_EFFECT_SLOT_FREE;
            }
            break;
        default:
            break;
        }
        break;
    case KF_EFFECT_KIND_111: {
        SVECTOR spawn_direction;
        s32 spread;

        if (record->cache_tail.payload.kind111.actor_index == KF_EFFECT_TARGET_ACTOR_NONE) {
            work_position.vx = record->position.vx;
            work_position.vy = record->position.vy;
            work_position.vz = record->position.vz;
            spawn_direction.vz = 0;
            spawn_direction.vy = 0;
            spawn_direction.vx = 0;
            spread = 1000;
        } else {
            const KfActor *actor =
                &actor_state.actors[record->cache_tail.payload.kind111.actor_index];

            spread = actor->collision_radius;
            work_position.vx = actor->position.vx;
            work_position.vz = actor->position.vz;
            work_position.vy = actor->position.vy;
            spawn_direction = actor->motion.vector;
        }
        work_position.vx += ((rand() * spread) >> 14) - spread;
        work_position.vz += ((rand() * spread) >> 14) - spread;
        work_position.vy -= 2000;
        effect_construct_record(10, record->type | KF_EFFECT_TARGET_ACTORS_AND_PLAYER, KF_EFFECT_KIND_0,
                       &work_position, &spawn_direction);
        break;
    }
    case KF_EFFECT_KIND_0:
        record->direction.vy += 20;
        if (record->scale_x < 0xc00) {
            record->scale_x = record->scale_y = record->scale_z = record->scale_z + 0x100;
        }
        collision = effect_collision_step(180, 0, -300);
        if (collision != KF_COLLISION_HIT_NONE) {
            if (record->cache_tail.payload.collision_latch.impact_handled == 0 && rand() < 3000) {
                effect_apply_current_magic_backstep(collision);
            }
            if ((collision & KF_COLLISION_HIT_ACTOR) != KF_COLLISION_HIT_NONE) {
                record->cache_tail.payload.collision_latch.impact_handled = 1;
            }
            if ((collision & (KF_COLLISION_HIT_AXIS | KF_COLLISION_HIT_FLOOR)) != KF_COLLISION_HIT_NONE) {
                if (initial_phase == 1) {
                    record->type = KF_EFFECT_SLOT_FREE;
                } else {
                    s32 collision_height = bss_801c7540.collision_cache.heights.result;
                    record->phase = 1;
                    record->direction.vy = -200;
                    record->position.vy = collision_height;
                }
            }
        }
        effect_spawn_at_lower_bound(&record->position, 0x400, 0x400, 500);
        record->rotation.vz += 2700;
        break;
    case KF_EFFECT_KIND_103:
    case KF_EFFECT_KIND_121: {
        s32 index;
        VECTOR spawn_position;
        s32 distance;

        if (record->phase == 2) {
            record->scale_x = record->scale_x - 128;
            record->scale_y = record->scale_x;
            if (record->scale_x <= 0) {
                record->type = KF_EFFECT_SLOT_FREE;
            }
            break;
        }
        collision = effect_collision_step(50, KF_COLLISION_HEIGHT_CHECK_FLOOR, -300);
        if (collision != KF_COLLISION_HIT_NONE) {
            if ((collision & KF_COLLISION_HIT_SHAPE_MASK) != KF_COLLISION_HIT_NONE) {
                if (initial_phase == 0) {
                    effect_collision_backtrack();
                    record->direction.vz = 0;
                    record->direction.vx = 0;
                } else {
                    effect_collision_backtrack();
                    collision_probe_floor_height(record->position.vx,
                                  record->position.vy,
                                  record->position.vz, 50, 0);
                    if (bss_801c7540.collision_cache.heights.result <
                        bss_801c7540.collision_cache.heights.lower_bound) {
                        spawn_position.vy = bss_801c7540.collision_cache.heights.result;
                    } else {
                        spawn_position.vy = bss_801c7540.collision_cache.heights.lower_bound;
                    }
                    distance = spawn_position.vy - record->position.vy;
                    goto kind103_spawn;
                }
            }
            record->phase = 1;
        }
        if (initial_phase == 1) {
            record->direction.vy -= 70;
            record->direction.vx = fixed_lerp_q12(
                0, record->direction.vx, 3000);
            record->direction.vz = fixed_lerp_q12(
                0, record->direction.vz, 3000);
            if (bss_801c7540.collision_cache.heights.result <
                bss_801c7540.collision_cache.heights.lower_bound) {
                spawn_position.vy = bss_801c7540.collision_cache.heights.result;
            } else {
                spawn_position.vy = bss_801c7540.collision_cache.heights.lower_bound;
            }
            distance = spawn_position.vy - record->position.vy;
            if (distance >= 7000) {
                SVECTOR spawn_direction;

            kind103_spawn:
                record->phase = 2;
                spawn_position.vx = record->position.vx;
                spawn_position.vz = record->position.vz;
                /* Retail has no visible write to this stack direction. */
                effect_construct_record(10, record->type | KF_EFFECT_TARGET_ACTORS_AND_PLAYER,
                               initial_kind == KF_EFFECT_KIND_103 ? KF_EFFECT_KIND_104 : KF_EFFECT_KIND_122,
                               &spawn_position, &spawn_direction, distance);
                effect_construct_record(10, record->type, KF_EFFECT_KIND_2,
                               &spawn_position, &spawn_direction,
                               distance >> 1, distance >> 4, 0x800);
                effect_play_spatial_sound(record, 0x17);
            }
        } else if (initial_phase == 0) {
            if ((s16)record->cache_tail.payload.kind103.remaining-- <= 0) {
                record->phase = 1;
            }
        }
        for (index = 3; index != -1; index--) {
            SVECTOR random_direction;

            random_direction.vx = (rand() >> 7) - 128;
            random_direction.vy = (rand() >> 7) - 128;
            random_direction.vz = (rand() >> 7) - 128;
            effect_construct_record(10, KF_EFFECT_TYPE_NONE, KF_EFFECT_KIND_101, &record->position,
                           &random_direction, 0x800, -128, 5, 18, 0);
        }
        break;
    }
    case KF_EFFECT_KIND_104:
    case KF_EFFECT_KIND_122:
        if (initial_phase < 3) {
            SVECTOR local_direction;
            s32 size = record->scale_y;

            size = SquareRoot12(size * ((size * size) >> 12));
            size = SquareRoot12(size) >> 3;
            /* Retail passes this stack vector without a visible write on
             * this kind entry. */
            effect_construct_record(10, record->type | KF_EFFECT_TARGET_ACTORS_AND_PLAYER,
                           initial_kind == KF_EFFECT_KIND_104 ? KF_EFFECT_KIND_11 : KF_EFFECT_KIND_54,
                           &record->position, &local_direction, size);
        }
        {
            s32 distance;

            record->animation_clip = KF_ENUM_DECODE(KF_ENUM_PROMOTED(KfAnimationClip),
                                                    (initial_phase & 1) - 128);
            if (record->phase < 9) {
                distance = fixed_vector3_length(
                    player_state.camera_position.vx - record->position.vx,
                    player_state.camera_position.vy - record->position.vy,
                    player_state.camera_position.vz - record->position.vz);
                if (distance < 32001) {
                    s32 strength = (((rsin(record->phase << 8) >> 1) + 1024) *
                                    (32000 - distance)) / 32000;

                    interpolate_collision_filter_rows(200, 180, 160, 32000, strength);
                }
            }
            record->phase++;
            break;
        }
    case KF_EFFECT_KIND_11:
    case KF_EFFECT_KIND_54: {
        SVECTOR random_direction;

        effect_scale_step(0x3800, record->cache_tail.payload.scale_step_argument.scale_step,
                       0x80, 0x400, 0x8000);
        random_direction.vx = (rand() >> 6) - 256;
        random_direction.vz = (rand() >> 6) - 256;
        random_direction.vy = -(rand() >> 7) - 128;
        effect_construct_record(10, KF_EFFECT_TYPE_NONE, KF_EFFECT_KIND_101, &record->position,
                                &random_direction,
                       0xc00, -128, 15, 18, 10);
        record->rotation.vy += 64;
        break;
    }
    case KF_EFFECT_KIND_51:
        effect_scale_step(0x1000, 0x400, 0x80, 0x800, 0x8000);
        break;
    case KF_EFFECT_KIND_52:
        effect_scale_step(0x1000, 0x800, 0x100, 0x800, 0x8000);
        break;
    case KF_EFFECT_KIND_118: {
        KF_ENUM_PROMOTED(KfEffectKind) child_kind;

        child_kind = KF_EFFECT_KIND_51;
        goto child_impact_update;
    case KF_EFFECT_KIND_119:
        child_kind = KF_EFFECT_KIND_52;
    child_impact_update:
        if (effect_collision_step(140, 0, -200) != KF_COLLISION_HIT_NONE) {
            effect_construct_record(10, record->type | KF_EFFECT_TARGET_ACTORS_AND_PLAYER,
                           child_kind,
                           &record->position, NULL);
            record->type = KF_EFFECT_SLOT_FREE;
        }
        break;
    }
    case KF_EFFECT_KIND_2:
        record->scale_z = record->scale_x = record->scale_x + record->cache_tail.payload.kind2.scale_step;
        if (record->scale_x >= 0x300) {
            VECTOR elevated;

            elevated.vx = record->position.vx;
            elevated.vy = record->position.vy + 1000;
            elevated.vz = record->position.vz;
            effect_apply_radial_magic_damage(&elevated,
                           (record->scale_x -
                            record->cache_tail.payload.kind2.scale_step) * 4,
                           record->scale_x * 4 - 1,
                           2000, 0x1000,
                           record->cache_tail.payload.kind2.radial_damage_parameter);
        }
        if (record->scale_x > record->cache_tail.payload.kind2.max_scale) {
            record->type = KF_EFFECT_SLOT_FREE;
        }
        break;
    case KF_EFFECT_KIND_20:
        effect_scale_step(0x4000, 0x100, 0x20, 0x400, 0x8000);
        record->rotation.vy += 64;
        break;
    case KF_EFFECT_KIND_12: {
        s32 prior_phase = initial_phase;

        switch (prior_phase) {
        case 101:
            record->phase++;
            break;
        case 100: {
            KfEffectRecord *child = effect_construct_record(
                10, record->type | KF_EFFECT_TARGET_ACTORS_AND_PLAYER, KF_EFFECT_KIND_12, &record->position,
                NULL, &record->rotation);

            child->phase = 101;
            goto kind12_reset;
        }
        case 110:
        kind12_scale:
            effect_scale_step(0x3800, 0x31f, 0x46, 0x400, 0x8000);
            record->rotation.vy += 64;
            break;
        default:
            if (record->phase == 0) {
                effect_play_spatial_sound(record, 0x29);
            }
            record->phase++;
            motion = effect_aim_and_move(
                record->cache_tail.payload.kind12.max_length,
                record->cache_tail.payload.kind12.scale,
                record->cache_tail.payload.kind12.turn_step,
                0xa0,
                0, 6000, record->cache_tail.payload.kind12.close_scale, 0x800);
            if (motion != KF_EFFECT_MOTION_BLOCKED) {
                goto kind12_collision;
            }
            {
                KfEffectRecord *child;

                effect_play_spatial_sound(record, 0x18);
                child = effect_construct_record(10, record->type | KF_EFFECT_TARGET_ACTORS_AND_PLAYER,
                                                KF_EFFECT_KIND_12,
                                      &record->position, NULL,
                                      &record->rotation);
                child->phase = 101;
            }
            /* fall through */
        case 102:
        kind12_reset:
            record->render_id = KF_EFFECT_MODEL_17;
            record->scale_z = 0;
            record->scale_y = 0;
            record->scale_x = 0;
            record->phase = 110;
            record->type |= KF_EFFECT_TARGET_ACTORS_AND_PLAYER;
            goto kind12_scale;
        kind12_collision:
            effect_spawn_at_lower_bound(&record->position, 0x2000, 0x2000, 500);
            record->rotation.vz += 128;
            effect_spawn_motion(record, 2, 0x400, 0xc00, -300, 5, 33, 0);
            break;
        }
        break;
    }
    case KF_EFFECT_KIND_100: {
        SVECTOR local_direction;
        s32 phase = initial_phase;

        if (phase < 100) {
            if (phase < 4 || phase >= 71) {
                record->direction.vy += 10;
                if (effect_collision_step(100, 0, 0) == KF_COLLISION_HIT_NONE) {
                    effect_spawn_at_lower_bound(&record->position, 0x2000, 0x2000, 500);
                    record->phase++;
                    break;
                }
                goto kind100_miss;
            }
            if (effect_aim_and_move(600, 30, 64, 100,
                              0, 0x1000, 360, 0x800) != KF_EFFECT_MOTION_BLOCKED) {
                goto kind100_success;
            }
        }
    kind100_miss:
        /* Retail passes this stack local without a visible write on this path. */
        effect_construct_record(10, record->type | KF_EFFECT_TARGET_ACTORS_AND_PLAYER, KF_EFFECT_KIND_20,
                                &record->position,
                      &local_direction);
        effect_play_spatial_sound(record, 0x18);
        record->type = KF_EFFECT_SLOT_FREE;
        goto shared_phase_increment;
    kind100_success:
        effect_spawn_at_lower_bound(&record->position, 0x2000, 0x2000, 500);
        record->rotation.vz += 128;
        effect_spawn_motion(record, 5, 0x400, 0x800, -150, 10, 8, 0);
        record->phase++;
        break;
    }
    case KF_EFFECT_KIND_5: {
        s32 count;
        s32 step_size;
        s32 progress;
        s32 matches;
        const KfActor *actor;

        switch (initial_phase) {
        case 0: {
            progress = 0;
            if (record->cache_tail.payload.kind5.actor_index != KF_EFFECT_TARGET_ACTOR_NONE) {
                goto kind5_actor_count;
            }
        kind5_default_count:
            count = 16;
            step_size = 0;
            record->updates_remaining = 1;
            goto kind5_count_ready;
        kind5_actor_count:
            actor = &actor_state.actors[record->cache_tail.payload.kind5.actor_index];
            count = asset_vertex_count(actor->definition_id + 128,
                                       actor->animation_id);
            if (count == 0) {
                goto kind5_default_count;
            }
            if (count < 33) {
                step_size = 0x1000;
                goto kind5_count_ready;
            }
            step_size = (count << 12) / 32;
            count = 32;
        kind5_count_ready:
            record->cache_tail.payload.kind5.initial_child_count =
                record->cache_tail.payload.kind5.children_remaining = count;
            for (count--; count != -1; count--) {
                effect_construct_record(10, record->type, KF_EFFECT_KIND_105,
                               &record->position, &record->direction,
                               effect_state.current_index,
                               record->cache_tail.payload.kind5.actor_index, progress >> 12);
                progress += step_size;
            }
            record->phase = 1;
            break;
        }
        case 1: {
            KfEffectRecord *child;
            s32 index;

            if (record->updates_remaining >= 2 &&
                record->cache_tail.payload.kind5.children_remaining != 0) {
                break;
            }
            matches = 0;
            child = effect_state.records;
            for (index = KF_EFFECT_CAPACITY - 1; index != -1;
                 child++, index--) {
                const KfEffectKind105Attachment *attachment =
                    &child->cache_tail.payload.kind105;

                if (child->type == KF_EFFECT_SLOT_FREE ||
                    child->kind != KF_EFFECT_KIND_105 ||
                    attachment->parent_index != effect_state.current_index) {
                    continue;
                }
                if (child->phase == 1) {
                    child->updates_remaining = 1;
                    matches++;
                } else {
                    child->phase = 2;
                }
            }
            actor = &actor_state.actors[record->cache_tail.payload.kind5.actor_index];
            if (record->cache_tail.payload.kind5.initial_child_count != 0) {
                actor_apply_magic_to_actor(record->cache_tail.payload.kind5.actor_index,
                              (u16)effect_magic_power(record),
                              magic->damage_components[0], magic->damage_components[1],
                              magic->damage_components[2], magic->damage_components[3],
                              magic->damage_components[4], magic->damage_components[5],
                              magic->damage_components[6], magic->damage_components[7],
                              (matches << 12) / record->cache_tail.payload.kind5.initial_child_count,
                              KF_ENUM_DECODE(KF_ENUM_PROMOTED(KfActorDamageFlags),
                          KF_ENUM_ENCODE(u8, record->type & KF_EFFECT_SOURCE_MASK)) | KF_ACTOR_DAMAGE_MAGIC,
                              &actor->position);
            }
            record->type = KF_EFFECT_SLOT_FREE;
            break;
        }
        }
        break;
    }
    case KF_EFFECT_KIND_105: {
        VECTOR next_position;
        KfActor *actor;
        VECTOR vertex_offset;
        VECTOR actor_position;
        VECTOR *position;

        record->rotation.vz += 800;
        if (initial_phase != 2) {
            if (record->cache_tail.payload.kind105.actor_index == KF_EFFECT_TARGET_ACTOR_NONE) {
                goto kind105_collision;
            }
            actor = &actor_state.actors[record->cache_tail.payload.kind105.actor_index];

            actor_sample_rotated_animation_vertex(actor,
                          record->cache_tail.payload.kind105.vertex_index,
                          &vertex_offset);
            position = actor_resolve_group_position(actor, &actor_position);
            next_position.vx = vertex_offset.vx + position->vx;
            next_position.vy = vertex_offset.vy + position->vy;
            next_position.vz = vertex_offset.vz + position->vz;
            goto kind105_actor_phase;
        }
        record->scale_y = record->scale_x = record->scale_x - 128;
        record->direction.vy += 5;
    kind105_collision:
        collision = effect_collision_step(100, 0, 0);
        if (collision != KF_COLLISION_HIT_NONE
            && (collision & KF_COLLISION_HIT_SHAPE_MASK) != KF_COLLISION_HIT_NONE) {
            record->type = KF_EFFECT_SLOT_FREE;
        }
        break;
    kind105_actor_phase:
        if (initial_phase != 0) {
            if (initial_phase == 1) {
                goto kind105_phase1;
            }
            break;
        }
        motion = effect_target_motion(&next_position, 300, 50,
                                   500, 150, 10, 0);
        if (motion == KF_EFFECT_MOTION_ARRIVED) {
            KfEffectRecord *linked =
                &effect_state.records[record->cache_tail.payload.kind105.parent_index];
            KfEffectKind5Fanout *linked_fanout = &linked->cache_tail.payload.kind5;
            if (linked_fanout->children_remaining != 0) {
                --linked_fanout->children_remaining;
            }
            record->phase = 1;
        } else {
            if (motion == KF_EFFECT_MOTION_BLOCKED &&
                (bss_801c7540.collision_cache.flags & KF_COLLISION_HIT_SHAPE_MASK) != KF_COLLISION_HIT_NONE) {
                KfEffectRecord *linked =
                    &effect_state.records[record->cache_tail.payload.kind105.parent_index];
                KfEffectKind5Fanout *linked_fanout = &linked->cache_tail.payload.kind5;
                if (linked_fanout->children_remaining != 0) {
                    --linked_fanout->children_remaining;
                }
                record->type = KF_EFFECT_SLOT_FREE;
            }
            break;
        }
    kind105_phase1:
        /* Retail copies all four VECTOR words, including the pad;
         * no defining pad store is visible on this path. */
        record->position = next_position;
        if (record->updates_remaining < 2) {
            record->phase = 2;
            record->updates_remaining = (rand() >> 12) + 16;
            record->direction.vx = vertex_offset.vx >> 1;
            record->direction.vy = vertex_offset.vy >> 1;
            record->direction.vz = vertex_offset.vz >> 1;
        }
        break;
    }
    case KF_EFFECT_KIND_9: {
        const KfEffectKind9Target *target =
            &record->cache_tail.payload.kind9;
        u8 actor_index = target->actor_index;

        if (actor_index == KF_EFFECT_TARGET_ACTOR_PLAYER) {
            VECTOR target_position;

            target_position.vx = player_state.camera_position.vx;
            target_position.vy = player_state.camera_position.vy - KF_PLAYER_CAMERA_EYE_OFFSET;
            target_position.vz = player_state.camera_position.vz;
            motion = effect_target_motion(&target_position, 400, 60,
                                       3000, 0, 10, KF_COLLISION_HEIGHT_CHECK_FLOOR);
        } else if (actor_index != KF_EFFECT_TARGET_ACTOR_NONE) {
            VECTOR target_position;
            const KfActor *actor = &actor_state.actors[actor_index];

            target_position.vx = actor->position.vx;
            target_position.vy = actor->position.vy - (actor->collision_height >> 1);
            target_position.vz = actor->position.vz;
            motion = effect_target_motion(&target_position, 600, 50,
                                       0, 0, 10, KF_COLLISION_HEIGHT_CHECK_FLOOR);
        } else {
            goto kind9_unbound;
        }
        if (motion != KF_EFFECT_MOTION_BLOCKED) {
            effect_spawn_motion(record, -1, -3, 0xed8, -80,
                                6, 8, 0, 0x400);
            break;
        }
    kind9_impact:
        {
            s32 index;

            effect_apply_current_magic_backstep(bss_801c7540.collision_cache.flags);
            for (index = 11; index != -1; index--) {
                effect_spawn_motion(record, -1, -2, 0xc00, -90, 16, 8, 5,
                               0x200, -256, 0x200, -256, 0x200, -256);
            }
            effect_scatter_lower_bound(&record->position, 5, 0x400,
                           0x2000, 0x4000, 0x1000);
            record->type = KF_EFFECT_SLOT_FREE;
        }
        break;
    kind9_unbound:
        record->direction.vy += 10;
        collision = effect_collision_step(10, KF_COLLISION_HEIGHT_CHECK_FLOOR, 0);
        if (collision != KF_COLLISION_HIT_NONE) {
            goto kind9_impact;
        }
        effect_spawn_motion(record, -1, -3, 0xed8, -80,
                       6, 8, 0, 0x400);
        break;
    }
    case KF_EFFECT_KIND_53: {
        s32 motion_scale;
        s32 count;

        motion_scale = 7600;
        goto kind33_update;
    case KF_EFFECT_KIND_33:
        motion_scale = 3800;
    kind33_update:
        work_target.vx = player_state.camera_position.vx;
        work_target.vy = player_state.camera_position.vy - KF_PLAYER_CAMERA_EYE_OFFSET;
        work_target.vz = player_state.camera_position.vz;
        motion = effect_target_motion(&work_target, 400, 60, 3000, 0, 10, 0);
        effect_spawn_at_lower_bound(&record->position, 0x2000, 0x2000, 500);
        if (motion == KF_EFFECT_MOTION_BLOCKED) {
            effect_apply_current_magic_backstep(bss_801c7540.collision_cache.flags);
            for (count = 11; count != -1; count--) {
                effect_spawn_motion(record, -1, -2, 0xc00, -90, 16, 33, 5,
                               0x200, -256, 0x200, -256, 0x200, -256);
            }
            record->type = KF_EFFECT_SLOT_FREE;
        }
        effect_spawn_motion(record, -1, -3, motion_scale,
                       -motion_scale / 48, 6, 33, 0, 0x400);
        break;
    }
    case KF_EFFECT_KIND_106:
        switch (initial_phase) {
        case 0:
            record->direction.vy += 20;
            collision = effect_collision_step(140, KF_COLLISION_HEIGHT_CHECK_FLOOR, -300);
            if (collision != KF_COLLISION_HIT_NONE) {
                record->position.vx -= record->direction.vx;
                record->position.vy -= record->direction.vy;
                record->position.vz -= record->direction.vz;
                effect_spawn_radial_ring(1, 0, -400, 60);
                effect_spawn_radial_ring(6, 60, -330, 56);
                effect_spawn_radial_ring(8, 140, -170, 40);
                effect_play_spatial_sound(record, 0x25);
                record->cache_tail.payload.kind106.children_remaining = 15;
                record->phase = 1;
                record->render_flags = KF_EFFECT_RENDER_HIDDEN;
            } else {
                effect_spawn_motion(record, -1, -3, 6000, -800,
                               6, 8, 0, -1024);
            }
            break;
        case 1:
            if (record->cache_tail.payload.kind106.children_remaining == 0) {
                record->type = KF_EFFECT_SLOT_FREE;
            }
            break;
        }
        break;
    case KF_EFFECT_KIND_8: {
        KfEffectRecord *parent;

        switch (initial_phase) {
        case 0: {
            const KfEffectKind8State *kind8 =
                &record->cache_tail.payload.kind8;
            VECTOR next;

            record->direction.vy +=
                                   kind8->vertical_step;
            next.vx = record->position.vx + record->direction.vx;
            next.vy = record->position.vy + record->direction.vy;
            next.vz = record->position.vz + record->direction.vz;
            collision = effect_probe_collision_by_type(&next, 140, KF_COLLISION_HEIGHT_CHECK_FLOOR);
            if (collision != KF_COLLISION_HIT_NONE
                && (collision & KF_COLLISION_HIT_SHAPE_MASK) != KF_COLLISION_HIT_NONE) {
                next.vx = record->position.vx;
                next.vz = record->position.vz;
                if ((effect_probe_collision_by_type(&next, 140,
                                                    KF_COLLISION_HEIGHT_CHECK_FLOOR) & KF_COLLISION_HIT_SHAPE_MASK) == KF_COLLISION_HIT_NONE) {
                    goto kind8_reset_axes;
                }
                if (record->direction.vy < 0) {
                    record->direction.vy = 0;
                    next.vy = record->position.vy;
                } else {
                    parent = &effect_state.records[kind8->parent_index];
                    if (!effect_spawn_at_lower_bound(&next, 0x2000, 0x2000, 500)) {
                        s32 dx;
                        s32 dz;
                        s32 distance;

                        record->phase = 1;
                        record->render_flags = KF_EFFECT_RENDER_VISIBLE | KF_EFFECT_RENDER_PITCH_TRANSFORM;
                        record->render_id = KF_EFFECT_MODEL_20;
                        dx = record->position.vx - parent->position.vx;
                        dz = record->position.vz - parent->position.vz;
                        record->rotation.vz = 0;
                        record->direction.vx = vector_xz_to_angle(dx, dz);
                        distance = fixed_vector2_length(dx, dz);
                        record->direction.vz = distance;
                        record->direction.vy = distance;
                        record->scale_z = 0;
                        record->scale_y = 0;
                        record->scale_x = 0x400;
                        record->scale_threshold.next_probe_phase =
                            ((rand() * 1500) >> 15) + 256;
                        record->updates_remaining = 100;
                        break;
                    }
                    if (parent->cache_tail.payload.kind106.children_remaining != 0) {
                        parent->cache_tail.payload.kind106.children_remaining--;
                    }
                    record->type = KF_EFFECT_SLOT_FREE;
                    break;
                }
            }
            goto kind8_apply_next;
        kind8_reset_axes:
            record->direction.vz = 0;
            record->direction.vx = 0;
        kind8_apply_next:
            record->position.vx = next.vx;
            record->position.vy = next.vy;
            record->position.vz = next.vz;
            record->map_layer_mask = bss_801c7540.collision_cache.layer == 0 ? KF_MAP_LAYER_FIRST : KF_MAP_LAYER_SECOND;
            record->rotation.vz = (record->rotation.vz + 300) & KF_ANGLE_WRAP_MASK;
            effect_spawn_motion(record, -1, 0x400, 0x1000, -500, 2, 8, 0);
            break;
            break;
        }
        case 1: {
            const KfEffectKind8State *kind8 =
                &record->cache_tail.payload.kind8;
            struct KfVecXZi forward;

            parent = &effect_state.records[kind8->parent_index];
            angle_to_forward_xz(record->direction.vx, &forward);
            record->position.vx = parent->position.vx +
                                  ((record->direction.vz * forward.x) >> 12);
            record->position.vz = parent->position.vz +
                                  ((record->direction.vz * forward.z) >> 12);
            record->direction.vz = fixed_lerp_q12(
                record->direction.vy, 0, record->scale_z);
            record->scale_y = ((u32)(rsin(record->scale_z >> 1) * 25)) >> 5;
            record->scale_z += 64;
            if (record->scale_z >= record->scale_threshold.next_probe_phase) {
                record->scale_threshold.next_probe_phase += 2048;
                collision = collision_query_world(
                    record->position.vx, record->position.vy,
                    record->position.vz, 256, record->scale_y,
                    KF_COLLISION_QUERY_ACTORS | KF_COLLISION_QUERY_PLAYER);
                if (collision != KF_COLLISION_HIT_NONE) {
                    effect_apply_current_magic(collision, 5000, NULL);
                }
            }
            if (record->scale_z >= KF_FIXED12_ONE) {
                record->type = KF_EFFECT_SLOT_FREE;
                if (parent->cache_tail.payload.kind106.children_remaining != 0) {
                    parent->cache_tail.payload.kind106.children_remaining--;
                }
                break;
            }
            if (rand() < 2048) {
                effect_spawn_motion(record, -1, -2, 0x1800, -96,
                               12, 39, -17, 128, -64, 64,
                               -100, 128, -64);
            }
            break;
        }
        }
        break;
    }
    case KF_EFFECT_KIND_10: {
        KfEffectKind10Targeting *targeting =
            &record->cache_tail.payload.kind10;
        s32 distance;
        KfActor *actor;

        switch (initial_phase) {
        case 0:
            collision = effect_collision_step(250, KF_COLLISION_HEIGHT_CHECK_FLOOR, 0);
            if (collision == KF_COLLISION_HIT_NONE && record->updates_remaining >= 2) {
                goto kind10_normal;
            }
            {
                KfEffectRecord *child;

                effect_scatter_lower_bound(&record->position, 8, 400,
                               0x2000, 0x8000, 0x400);
                child = effect_construct_record(10, record->type | KF_EFFECT_TARGET_ACTORS_AND_PLAYER,
                                                KF_EFFECT_KIND_10,
                                      &record->position, NULL,
                                      &record->rotation);
                child->phase = 2;
                effect_play_spatial_sound(record, 0x17);
            }
            /* fall through */
        case 5:
            record->phase = 1;
            record->render_id = KF_EFFECT_MODEL_11;
            record->lighting_override_index = KF_LIGHTING_EFFECT;
            record->scale_z = 0;
            record->scale_y = 0;
            record->scale_x = 0;
            record->updates_remaining = -1;
            record->type |= KF_EFFECT_TARGET_ACTORS_AND_PLAYER;
            goto kind10_phase1;
        kind10_normal:
            record->animation_phase_q12 = (record->animation_phase_q12 + 128)
                                          & EFFECT_ANIMATION_PHASE_MASK;
            if (targeting->emissions_remaining != 0) {
                VECTOR target;
                VECTOR origin;
                SVECTOR direction;

                effect_sample_world_vertex(record, 0, &origin,
                               (const SVECTOR *)&record->scale_x);
                actor = &actor_state.actors[targeting->actor_index];
                target.vx = actor->position.vx;
                target.vy = actor->position.vy - (actor->collision_height >> 1);
                target.vz = actor->position.vz;
                vector_direction_scaled(&origin, &target, 800, &direction);
                effect_construct_record(10, record->type, KF_EFFECT_KIND_7, &origin, &direction);
                targeting->emissions_remaining--;
            } else if (rand() < 2048) {
                actor = actor_find_best_in_cone(
                    &record->position, record->rotation.vy,
                    record->rotation.vx, 25000, 800, 800,
                    &distance, 512);
                if (actor != NULL) {
                    targeting->emissions_remaining = 5;
                    targeting->actor_index = actor - actor_state.actors;
                }
            }
            if (rand() < 1024) {
                if (actor_find_best_in_cone(&record->position, record->rotation.vy,
                                   record->rotation.vx, 30000, 800, 800,
                                   &distance, 512) != NULL) {
                    effect_spawn_zero_direction(record, 0x2f);
                    effect_spawn_zero_direction(record, 0x32);
                }
            }
            break;
        case 1:
        kind10_phase1:
            effect_scale_step(0x4000, 0x800, 75, 0x400, 0x8000);
            record->rotation.vy += 64;
            break;
        default:
            record->phase++;
            break;
        }
        break;
    }
    shared_phase_increment:
        record->phase++;
        break;
    case KF_EFFECT_KIND_6: {
        KfEffectTrailRow *row;
        s32 angle;
        KfActor *actor;

        switch (initial_phase) {
        case 0: {
            s32 index;
            KfEffectRecord *child;
            KfEffectTrailChildLink *link;
            u8 parent_index;

            for (index = 0; index < 8; index++) {
                child = effect_construct_record(
                    10, KF_EFFECT_TYPE_NONE, KF_EFFECT_KIND_107, &record->position, &record->direction,
                    &record->rotation, index);

                if (index == 0) {
                    child->render_id = KF_EFFECT_MODEL_23;
                }
                parent_index = effect_state.current_index;
                link = &child->cache_tail.payload.trail_child;
                link->lag_index = index;
                link->parent_index = parent_index;
            }
            child->render_id = KF_EFFECT_MODEL_24;
            record->phase = 1;
        }
        /* The spawn tick also runs the phase-one update. */
        case 1:
            if (record->updates_remaining < 3) {
            kind6_phase3:
                record->phase = 3;
                record->updates_remaining = -1;
                record->cache_tail.payload.trail.phase_counter = 24;
                break;
            }
            if (effect_aim_and_move(250, 25, 32, 200,
                              0, 0x400, 100, 0x800) == KF_EFFECT_MOTION_BLOCKED) {
                record->updates_remaining = -1;
                if (bss_801c7540.collision_cache.flags != KF_COLLISION_HIT_ACTOR) {
                    goto kind6_phase3;
                }
                {
                    u8 actor_index;

                    effect_apply_current_magic(KF_COLLISION_IMPACT_HOLD_ACTOR_ANIMATION |
                                               KF_COLLISION_HIT_ACTOR, 5000, NULL);
                    actor_index = bss_801c7540.collision_cache.actor_index;
                    record->cache_tail.payload.trail.actor_index = actor_index;
                    actor = &actor_state.actors[record->cache_tail.payload.trail.actor_index];
                    if (actor->target_type == KF_ACTOR_TARGET_2 || actor->target_type == KF_ACTOR_TARGET_3) {
                        record->render_flags = KF_EFFECT_RENDER_VISIBLE;
                        record->render_id = KF_EFFECT_MODEL_22;
                        record->phase = 2;
                        record->cache_tail.payload.trail.phase_counter = 0;
                        record->rotation.vz = 0;
                        record->rotation.vx = 0;
                        record->rotation.pad = -vector_xz_to_angle(
                            record->position.vx - actor->position.vx,
                            record->position.vz - actor->position.vz);
                        goto phase_two;
                    }
                }
                goto kind6_phase3;
            } else {
                u8 frame = record->cache_tail.payload.trail.frame_index + 1;

                record->cache_tail.payload.trail.frame_index = frame;
                if (frame >= 24) {
                    record->cache_tail.payload.trail.frame_index = 0;
                }
                row = &record->cache_tail.payload.trail.rows[record->cache_tail.payload.trail.frame_index];
                angle = record->cache_tail.payload.trail.phase_counter << 4;
                record->cache_tail.payload.trail.phase_counter += 8;
                row->position.vx = record->position.vx;
                row->position.vz = record->position.vz;
                row->position.vy = record->position.vy + (rsin(angle) >> 3);
                row->rotation.vy = record->rotation.vy;
                row->rotation.vz = record->rotation.vz;
                row->rotation.vx = record->rotation.vx + (rcos(angle) >> 4);
            }
            break;
        case 2:
        phase_two: {
            VECTOR scratch;
            VECTOR *position;
            u8 frame;
            s32 radius;

            actor = &actor_state.actors[record->cache_tail.payload.trail.actor_index];
            actor->flags |= KF_ACTOR_FLAG_EFFECT_ANIMATION_HOLD;
            frame = record->cache_tail.payload.trail.frame_index + 1;
            radius = actor->collision_radius;
            record->cache_tail.payload.trail.frame_index = frame;
            if (frame >= 24) {
                record->cache_tail.payload.trail.frame_index = 0;
            }
            position = actor_resolve_group_position(actor, &scratch);
            record->position = *position;
            record->position.vy -= actor->collision_height >> 1;
            if (record->cache_tail.payload.trail.phase_counter < 17) {
                s32 scale = fixed_lerp_q12(
                    0, radius, record->cache_tail.payload.trail.phase_counter << 9);

                record->scale_z = scale;
                record->scale_x = scale;
                record->scale_y = fixed_lerp_q12(
                    0, actor->collision_height,
                    record->cache_tail.payload.trail.phase_counter * 350);
            } else if (record->cache_tail.payload.trail.phase_counter >= 60) {
                record->phase = 4;
                break;
            }
            {
                angle = record->rotation.pad;
                record->rotation.pad += 100000 / radius;
                record->cache_tail.payload.trail.phase_counter++;
                row = &record->cache_tail.payload.trail.rows[record->cache_tail.payload.trail.frame_index];
                radius = (radius * 25 << 8) >> 12;
                row->position.vx = record->position.vx +
                                   ((rsin(angle) * radius) >> KF_FIXED12_BITS);
                row->position.vz = record->position.vz +
                                   ((rcos(angle) * radius) >> KF_FIXED12_BITS);
                row->position.vy = record->position.vy +
                                   (rsin(angle << 1) >> 4);
                row->rotation.vy = -angle - KF_ANGLE_QUARTER_TURN;
                row->rotation.vz = 0;
                row->rotation.vx = rcos(angle << 1) >> 4;
            }
            break;
        }
        case 3: {
            u8 frame = ++record->cache_tail.payload.trail.frame_index;

            if (frame >= 24) {
                record->cache_tail.payload.trail.frame_index = 0;
            }
            if (record->cache_tail.payload.trail.phase_counter != 0) {
                record->cache_tail.payload.trail.phase_counter--;
                break;
            }
            goto kind6_release_actor;
        }
        case 4: {
            s32 index;

            for (index = 31; index != -1; index--) {
                effect_spawn_motion(record, -1, -2, 0xc00, -90,
                               16, 14, 5, 0x200, -256, 0x200,
                               -320, 0x200, -256);
            }
            goto kind6_release_actor;
        }
        }
        break;
    kind6_release_actor:
        actor = &actor_state.actors[record->cache_tail.payload.trail.actor_index];
        actor->flags &= ~KF_ACTOR_FLAG_EFFECT_ANIMATION_HOLD;
        record->type = KF_EFFECT_SLOT_FREE;
        break;
    }
    case KF_EFFECT_KIND_107: {
        KfEffectRecord *selected = &effect_state.records[
            record->cache_tail.payload.trail_child.parent_index];
        const KfEffectTrailRow *snapshot;
        s32 frame_index;

        if (initial_phase == 0 && selected->phase >= 3) {
            record->phase = 1;
            record->updates_remaining =
                record->cache_tail.payload.trail_child.lag_index * 3;
        }
        if (selected->phase == 4) {
            record->type = KF_EFFECT_SLOT_FREE;
            break;
        }
        frame_index = selected->cache_tail.payload.trail.frame_index -
                      record->cache_tail.payload.trail_child.lag_index * 3;
        if (frame_index < 0) {
            frame_index += 24;
        }
        snapshot = &selected->cache_tail.payload.trail.rows[frame_index];
        record->position = snapshot->position;
        record->rotation = snapshot->rotation;
        break;
    }
    case KF_EFFECT_KIND_101: {
        const KfEffectKind101Motion *motion =
            &record->cache_tail.payload.kind101;

        record->direction.vy +=
                               motion->vertical_step;
        record->position.vx += record->direction.vx;
        record->position.vz += record->direction.vz;
        record->scale_x += motion->scale_step;
        record->scale_y = record->scale_z = record->scale_x;
        record->position.vy += record->direction.vy;
        break;
    }
    case KF_EFFECT_KIND_102: {
        record->phase++;
        record->scale_y = (rsin(record->phase << 8) *
                           record->cache_tail.payload.kind102.amplitude) >> 12;
        if (record->phase >= 8) {
            record->type = KF_EFFECT_SLOT_FREE;
        }
        break;
    }
    case KF_EFFECT_KIND_DEFENSE_BOOST:
        if (player_state.defense_boost_timer == 0) {
            record->type = KF_EFFECT_SLOT_FREE;
        }
        record->rotation.vy += 128;
        break;
    case KF_EFFECT_KIND_ATTACK_BOOST:
        if (player_state.attack_boost_timer == 0) {
            record->type = KF_EFFECT_SLOT_FREE;
        }
        record->rotation.vy -= 128;
        break;
    case KF_EFFECT_KIND_16:
        if (record->updates_remaining == 4) {
            player_state.vitals.current_hp += 60;
            if (player_state.vitals.current_hp >
                player_state.vitals.maximum_hp) {
                player_state.vitals.current_hp =
                    player_state.vitals.maximum_hp;
            }
        }
        interpolate_collision_filter_rows(0xe6, 0xc8, 0xa0, 0x59d8,
                       rsin(record->updates_remaining << 8));
        break;
    case KF_EFFECT_KIND_14: {
        VECTOR spawn_position;
        SVECTOR spawn_direction;
        KfEffectRecord *spawned;

        if (record->updates_remaining == 12) {
            player_state.poison_timer = 0;
        }
        spawn_position.vx = (rand() >> 5) - 512;
        spawn_position.vy = (rand() >> 8) + 200;
        spawn_position.vz = 0x400;
        spawn_direction.vz = 0;
        spawn_direction.vx = 0;
        spawn_direction.vy = 0;
        spawned = effect_construct_record(10, KF_EFFECT_TYPE_NONE, KF_EFFECT_KIND_101, &spawn_position,
                                &spawn_direction, 700, -30, 10, 14, -10);
        spawned->map_layer_mask = KF_MAP_LAYER_BOTH;
        spawned->render_flags = KF_EFFECT_RENDER_ALWAYS_VISIBLE | KF_EFFECT_RENDER_SCREEN_SPACE;
        interpolate_collision_filter_rows(160, 180, 220, 18000,
                       rsin(record->updates_remaining << 7));
        break;
    }
    case KF_EFFECT_KIND_19: {
        VECTOR spawn_position;
        SVECTOR spawn_direction;
        KfEffectRecord *spawned;
        s32 count;

        if (record->updates_remaining == 12) {
            player_state.vitals.current_hp += 150;
            if (player_state.vitals.current_hp >
                player_state.vitals.maximum_hp) {
                player_state.vitals.current_hp =
                    player_state.vitals.maximum_hp;
            }
            player_cap_status_components(KF_PLAYER_STATUS_FIRST | KF_PLAYER_STATUS_SECOND |
                                         KF_PLAYER_STATUS_THIRD);
        }
        spawn_direction.vz = 0;
        spawn_direction.vx = 0;
        spawn_direction.vy = 0;
        for (count = 3; count != 0; count--) {
            spawn_position.vx = (rand() >> 5) - 512;
            spawn_position.vy = (rand() >> 8) + 200;
            spawn_position.vz = 0x400;
            spawned = effect_construct_record(10, KF_EFFECT_TYPE_NONE, KF_EFFECT_KIND_101, &spawn_position,
                                    &spawn_direction, 700, -30, 10, 18, -10);
            spawned->map_layer_mask = KF_MAP_LAYER_BOTH;
            spawned->render_flags = KF_EFFECT_RENDER_ALWAYS_VISIBLE | KF_EFFECT_RENDER_SCREEN_SPACE;
        }
        interpolate_collision_filter_rows(240, 240, 160, 18000,
                       rsin(record->updates_remaining << 7));
        break;
    }
    case KF_EFFECT_KIND_22:
        record->direction.vy += 10;
        collision = effect_collision_step(100, KF_COLLISION_HEIGHT_CHECK_FLOOR, -300);
        if (collision != KF_COLLISION_HIT_NONE) {
            effect_apply_current_magic_backstep(collision);
            record->type = KF_EFFECT_SLOT_FREE;
        }
        break;
    case KF_EFFECT_KIND_3:
        effect_scale_step(0x4000, 0x200, 0x40, 0xc00, 0x8000);
        break;
    case KF_EFFECT_KIND_114: {
        s32 count;

        record->position.vx += record->direction.vx;
        record->position.vy += record->direction.vy;
        record->position.vz += record->direction.vz;
        collision = collision_query_world(record->position.vx, record->position.vy,
                                  record->position.vz, 10, 10,
                                  KF_COLLISION_QUERY_ACTORS | KF_COLLISION_QUERY_MAP_OBJECTS |
                                      KF_COLLISION_QUERY_PLAYER);
        if (collision == KF_COLLISION_HIT_NONE) {
            collision_probe_floor_height(record->position.vx,
                          record->cache_tail.payload.kind114.origin_y,
                          record->position.vz, 0, 0);
            if (record->position.vy < bss_801c7540.collision_cache.heights.result) {
                goto kind114_particles;
            }
            record->position.vy = bss_801c7540.collision_cache.heights.result;
        }
        effect_construct_record(10, record->type | KF_EFFECT_TARGET_ACTORS_AND_PLAYER, KF_EFFECT_KIND_3,
                                &record->position, NULL, 0);
        record->type = KF_EFFECT_SLOT_FREE;
        break;
    kind114_particles:
        for (count = 1; count != -1; count--) {
            effect_spawn_motion(record, (rand() * 20) >> 15,
                           -3, 6000, -200, 5, 39, 0, 0x100);
        }
        break;
    }
    case KF_EFFECT_KIND_24: {
        VECTOR target;
        KF_ENUM_PROMOTED(KfEffectMotionResult) result;
        s32 count;

        target.vx = player_state.camera_position.vx;
        target.vy = player_state.camera_position.vy - KF_PLAYER_CAMERA_EYE_OFFSET;
        target.vz = player_state.camera_position.vz;
        result = effect_target_motion(&target, 300, 40, 2000, 0, 10, 0);
        if (result == KF_EFFECT_MOTION_BLOCKED) {
            effect_apply_current_magic_backstep(bss_801c7540.collision_cache.flags);
            for (count = 11; count != -1; count--) {
                effect_spawn_motion(record, -1, -2, 0xc00, -90, 16, 14, 5,
                               0x200, -256, 0x200, -256, 0x200, -256);
            }
            record->type = KF_EFFECT_SLOT_FREE;
        } else if (rand() < 16384) {
            effect_construct_record(10, KF_EFFECT_TYPE_NONE, KF_EFFECT_KIND_109, &record->position,
                           &record->direction, effect_state.current_index);
        }
        break;
    }
    case KF_EFFECT_KIND_109: {
        const KfEffectKind109Target *target =
            &record->cache_tail.payload.kind109;

        effect_target_motion(
            &effect_state.records[target->effect_index].position,
            500, 15, -1, 0, 0, -1);
        break;
    }
    case KF_EFFECT_KIND_120: {
        record->direction.vy += 20;
        if (record->scale_x < KF_FIXED12_ONE) {
            record->scale_x = record->scale_y = record->scale_z = record->scale_z + 0x200;
        }
        if (rand() >= 400) {
            collision = effect_collision_step(180, 0, -300);
            if (collision == KF_COLLISION_HIT_NONE
                || (collision & (KF_COLLISION_HIT_AXIS | KF_COLLISION_HIT_FLOOR)) == KF_COLLISION_HIT_NONE) {
                goto kind120_rotate;
            }
            record->position.vy = bss_801c7540.collision_cache.heights.result;
        }
        if (rand() < 8192) {
            effect_construct_record(10, record->type | KF_EFFECT_TARGET_ACTORS_AND_PLAYER, KF_EFFECT_KIND_42,
                           &record->position, NULL, 0);
            effect_construct_record(10, record->type | KF_EFFECT_TARGET_ACTORS_AND_PLAYER, KF_EFFECT_KIND_42,
                           &record->position, NULL, 1);
            effect_play_spatial_sound(record, 0x18);
        }
        record->type = KF_EFFECT_SLOT_FREE;
    kind120_rotate:
        record->rotation.vz += 2700;
        break;
    }
    default:
        break;
    }
    return;

}

ADDRESS(0x80046a04, 0x30)
void effect_pool_reset(void)
{
    KfEffectRecord *record = effect_state.records;
    u16 i;

    for (i = 0; i < KF_EFFECT_CAPACITY; i++) {
        record->type = KF_EFFECT_SLOT_FREE;
        record++;
    }
}

ADDRESS(0x80046a34, 0x2c)
void magic_load_records(const KfMagicRecord *records)
{
    const u32 *source = (const u32 *)records;
    u32 *destination = (u32 *)effect_state.magic_records;
    s32 count;

    for (count = sizeof effect_state.magic_records / sizeof *source; count != 0; count--) {
        *destination++ = *source++;
    }
}


ADDRESS(0x80046a60, 0xfc)
void effect_pool_sweep(void)
{
    KfEffectRecord *record = effect_state.records;

    effect_state.current_index = 0;
    do {
        if (record->type != KF_EFFECT_SLOT_FREE) {
            effect_state.current_record = record;
            effect_state.current_magic = &effect_state.magic_records[KF_ENUM_ENCODE(u8, record->kind)];
            if (record->updates_remaining != -1) {
                if (--record->updates_remaining == -1) {
                    record->type = KF_EFFECT_SLOT_FREE;
                } else {
                    effect_update_dispatch();
                }
            } else {
                effect_update_dispatch();
            }
        }
        effect_state.current_index++;
        record++;
    } while (effect_state.current_index < KF_EFFECT_CAPACITY);
}
