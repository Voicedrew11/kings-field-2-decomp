#include <kf/lib/address.h>
#include <kf/lib/null.h>
#include <kf/game/callback.h>
#include <kf/game/game.h>
#include <kf/game/cd.h>
#include <kf/game/effect.h>
#include <kf/game/graphics.h>
#include <kf/game/notify.h>
#include <kf/game/player.h>
#include <kf/game/resources.h>
#include <kf/lib/math.h>
#include <psyq/pad.h>
#include <psyq/sdk.h>
#include <kf/game/actor.h>
#include <kf/game/animation.h>
#include <kf/game/collision_cache.h>
#include <kf/game/asset.h>
#include <kf/game/map_cell.h>
#include <psyq/libc.h>
#include <kf/game/audio.h>
#include <kf/game/event_counter.h>
#include <kf/game/event_state.h>
#include <kf/game/map_object.h>
#include <kf/game/menu.h>
#include <kf/game/pool.h>
#include <kf/lib/types.h>

void player_apply_map_object_reaction(KfMapObject *object);


DATA(0x80067694, 0x28, ".data")
KfPlayerMagicSpawnRecord player_magic_spawn_records[5] = {
    {{0, 0, 100, 0}},
    {{-1000, 0, 0, 46}},
    {{-2000, 0, -2000, 46}},
    {{1000, 0, 0, 46}},
    {{2000, 0, -2000, 46}}
};

DATA(0x800676bc, 0x20, ".data")
SVECTOR player_magic_square_offsets[4] = {
    {-400, -400, 200, 0},
    {-400, 400, 200, 0},
    {400, -400, 200, 0},
    {400, 400, 200, 0}
};

DATA(0x800676dc, 0xc, ".data")
KfPlayerMagicIdSequence player_magic_id_sequence = {
    {39, 40, 60, 66, 84, 86, 39, 40, 60, 66, 84, 86}
};


DATA(0x8006e5e4, 0x8, ".sdata")
static RECT player_status_texture_row_0 = {0x240, 0x119, 16, 1};
DATA(0x8006e5ec, 0x8, ".sdata")
static RECT player_status_texture_row_1 = {0x240, 0x117, 16, 1};
DATA(0x8006e5f4, 0x8, ".sdata")
static RECT player_status_texture_row_2 = {0x240, 0x11a, 16, 1};
DATA(0x8006e5fc, 0x8, ".sdata")
static RECT player_status_texture_row_3 = {0x240, 0x118, 16, 1};



DATA(0x8007682c, 0x4b0, ".bss")
KfPlayerLevelGrowth player_level_growth_table[KF_PLAYER_LEVEL_GROWTH_COUNT];

DATA(0x80076cdc, 0xc000, ".bss")
KfWeaponAssetBuffer player_weapon_asset_buffer;


DATA(0x80199414, 0x160, ".bss")
KfPlayerState player_state;

DATA(0x801c7fbc, 0x4c8, ".bss")
KfWeaponRecordGame player_weapon_records[18];

DATA(0x801c8484, 0x11844, ".bss")
KfBss801c7540 bss_801c7540;




ADDRESS(0x800240b8, 0x9c)
void player_restore_equipment_effects(void)
{
    player_set_equipment_slot(KF_OBJECT_NONE, KF_EQUIPMENT_SLOT_NONE);
    player_equip_weapon(player_state.equipped_weapon_id);
    player_reset_view();
    if (player_state.defense_boost_timer != 0) {
        effect_construct_record(10, KF_EFFECT_USE_PLAYER_MAGIC, KF_EFFECT_KIND_DEFENSE_BOOST,
                                &player_state.camera_position, NULL);
    }
    if (player_state.attack_boost_timer != 0) {
        effect_construct_record(10, KF_EFFECT_USE_PLAYER_MAGIC, KF_EFFECT_KIND_ATTACK_BOOST,
                                &player_state.camera_position, NULL);
    }
    player_recalculate_combat_stats();
}

ADDRESS(0x80024154, 0x208)
void player_reload_map_resources(
    s32 first, s32 second, s32 third, s32 fourth, s32 fifth, s32 optional_resource)
{
    cd_request_wait_idle();
    resource_state.active_resource_ids[KF_RESOURCE_SLOT_MAP_REGION] = KF_RESOURCE_ACTIVE_UNINITIALIZED;
    resource_state.active_resource_ids[KF_RESOURCE_SLOT_TMD] = KF_RESOURCE_ACTIVE_UNINITIALIZED;
    resource_state.active_resource_ids[KF_RESOURCE_SLOT_TIM] = KF_RESOURCE_ACTIVE_UNINITIALIZED;
    resource_state.active_resource_ids[KF_RESOURCE_SLOT_VAB] = KF_RESOURCE_ACTIVE_UNINITIALIZED;
    resource_state.active_resource_ids[KF_RESOURCE_SLOT_SEQUENCE] = KF_RESOURCE_ACTIVE_UNINITIALIZED;
    resource_request_transition(first, second, third,
                                KF_RESOURCE_REQUEST_KEEP, KF_RESOURCE_REQUEST_KEEP,
                                KF_RESOURCE_OFFSET_NO_SHIFT, KF_RESOURCE_OFFSET_NO_SHIFT,
                                KF_RESOURCE_OFFSET_NO_SHIFT);
    do {
        cd_request_yield();
        resource_advance_transition();
    } while (resource_state.transition_active);
    cd_request_wait_idle();
    DrawSync(0);
    VSync(0);
    DrawSync(0);
    VSync(0);
    if (optional_resource != KF_RESOURCE_REQUEST_KEEP) {
        resource_request_transition(KF_RESOURCE_REQUEST_KEEP, KF_RESOURCE_REQUEST_KEEP,
                                    optional_resource, KF_RESOURCE_REQUEST_KEEP,
                                    KF_RESOURCE_REQUEST_KEEP, KF_RESOURCE_OFFSET_NO_SHIFT,
                                    KF_RESOURCE_OFFSET_NO_SHIFT, KF_RESOURCE_OFFSET_NO_SHIFT);
        do {
            cd_request_yield();
            resource_advance_transition();
        } while (resource_state.transition_active);
        cd_request_wait_idle();
        DrawSync(0);
        VSync(0);
        DrawSync(0);
        VSync(0);
    }

    player_sync_position_to_map();
    reset_collision_rows_and_overlay();
    render_frames_with_color_overlay(KF_COLOR_OVERLAY_SUBTRACT | KF_COLOR_OVERLAY_FRONT, KF_FIXED12_ONE, KF_FIXED12_ONE, 0);
    if (game_graphics_runtime.asset_registry_entries[0x181] == NULL) {
        resource_tmd_queue_read(KF_RESOURCE_ARCHIVE_MO, 0x101, 0x181);
    }
    cd_request_wait_idle();
    render_frames_with_color_overlay(KF_COLOR_OVERLAY_SUBTRACT | KF_COLOR_OVERLAY_FRONT, KF_FIXED12_ONE, 0, -128);
    resource_request_transition(KF_RESOURCE_REQUEST_KEEP, KF_RESOURCE_REQUEST_KEEP,
                                KF_RESOURCE_REQUEST_KEEP, fourth, fifth,
                                KF_RESOURCE_OFFSET_NO_SHIFT, KF_RESOURCE_OFFSET_NO_SHIFT,
                                KF_RESOURCE_OFFSET_NO_SHIFT);
}

enum {
    KF_PLAYER_VALUE_SCALE_BITS = 6
};

ADDRESS(0x8002435c, 0x54)
s32 player_charge_gain_for_rank(s32 value, s32 rank)
{
    s32 scaled = ((value << KF_PLAYER_VALUE_SCALE_BITS) / (rank + 1)) + 1;
    if (scaled >= KF_PLAYER_CHARGE_FULL) {
        return KF_PLAYER_CHARGE_FULL;
    }
    return scaled;
}

ADDRESS(0x800243b0, 0x11c)
void player_add_equipment_bonuses(KF_ENUM_PARAM(KfObjectId, s32) item_id)
{
    const KfEquipmentRecord *record;

    if (item_id == KF_OBJECT_NONE) {
        return;
    }
    record = &player_equipment_records[KF_ENUM_ENCODE(s32, item_id) - KF_EQUIPMENT_RECORD_FIRST_ID];
    player_state.combat_components[0] += record->bonus_components[0];
    player_state.combat_components[1] += record->bonus_components[1];
    player_state.combat_components[2] += record->bonus_components[2];
    player_state.combat_components[3] += record->bonus_components[3];
    player_state.combat_components[4] += record->bonus_components[4];
    player_state.combat_components[5] += record->bonus_components[5];
    player_state.combat_components[6] += record->bonus_components[6];
    player_state.combat_components[7] += record->bonus_components[7];
    player_state.combat_components[8] += record->bonus_components[8];
}

enum {
    PLAYER_CURSE_POWER_PENALTY = 20,
    PLAYER_STATUS_DEFENSE_BONUS = 50,
    PLAYER_STATUS_POWER_BONUS = 30,
    PLAYER_WEAPON_ATTACK_PENALTY = 10,
    PLAYER_BONUS_OVERFLOW_LIMIT = 0x7fff,
    PLAYER_POWER_CAP_THRESHOLD = KF_PLAYER_POWER_MAX + 1
};

ADDRESS(0x800244cc, 0x6b0)
void player_recalculate_combat_stats(void)
{
    const KfWeaponRecordGame *weapon;
    s32 power;

    player_state.attack_components[0] = 0;
    player_state.attack_components[1] = 0;
    player_state.attack_components[2] = 0;
    player_state.attack_components[3] = 0;
    player_state.attack_components[4] = 0;
    player_state.attack_components[5] = 0;
    player_state.attack_components[6] = 0;
    player_state.attack_components[7] = 0;
    player_state.combat_components[0] = 0;
    player_state.combat_components[1] = 0;
    player_state.combat_components[2] = 0;
    player_state.combat_components[3] = 0;
    player_state.combat_components[4] = 0;
    player_state.combat_components[5] = 0;
    player_state.combat_components[6] = 0;
    player_state.combat_components[7] = 0;
    player_state.combat_components[8] = 0;

    player_state.physical_power = player_state.base_physical_power;
    player_state.magic = player_state.base_magic;
    if (player_state.curse_strength != 0) {
        power = player_state.physical_power - PLAYER_CURSE_POWER_PENALTY;
        if (power < 0) {
            power = 0;
        }
        player_state.physical_power = power;
    }

    if (player_state.equipped_weapon_id != KF_OBJECT_NONE) {
        weapon = &player_weapon_records[KF_ENUM_ENCODE(u8, player_state.equipped_weapon_id)];
        player_state.attack_components[0] += weapon->attack_components[0];
        player_state.attack_components[1] += weapon->attack_components[1];
        player_state.attack_components[2] += weapon->attack_components[2];
        player_state.attack_components[3] += weapon->attack_components[3];
        player_state.attack_components[4] += weapon->attack_components[4];
        player_state.attack_components[5] += weapon->attack_components[5];
        player_state.attack_components[6] += weapon->attack_components[6];
        player_state.attack_components[7] += weapon->attack_components[7];
    }

    player_add_equipment_bonuses(player_state.equipped_head_id);
    player_add_equipment_bonuses(player_state.equipped_body_id);
    player_add_equipment_bonuses(player_state.equipped_arm_id);
    player_add_equipment_bonuses(player_state.equipped_leg_id);
    player_add_equipment_bonuses(player_state.equipped_shield_id);
    player_add_equipment_bonuses(player_state.equipped_accessory_id);
    player_add_equipment_bonuses(player_state.equipped_extra_id);

    if (player_state.defense_boost_timer != 0) {
        player_state.combat_components[5] += PLAYER_STATUS_DEFENSE_BONUS;
    }
    if (player_state.attack_boost_timer != 0) {
        player_state.combat_components[1] += PLAYER_STATUS_POWER_BONUS;
        player_state.combat_components[0] += PLAYER_STATUS_POWER_BONUS;
        player_state.combat_components[2] += PLAYER_STATUS_POWER_BONUS;
    }
    if (player_state.magic_boost_timer != 0) {
        player_state.magic += PLAYER_STATUS_POWER_BONUS;
    }

    if (player_state.equipped_weapon_id == KF_OBJECT_10) {
        player_state.combat_components[1] -= PLAYER_WEAPON_ATTACK_PENALTY;
        player_state.combat_components[2] -= PLAYER_WEAPON_ATTACK_PENALTY;
    }
    if (player_state.equipped_weapon_id == KF_OBJECT_11) {
        player_state.combat_components[4] -= PLAYER_WEAPON_ATTACK_PENALTY;
        player_state.combat_components[5] -= PLAYER_WEAPON_ATTACK_PENALTY;
        player_state.combat_components[7] -= PLAYER_WEAPON_ATTACK_PENALTY;
        player_state.combat_components[8] -= PLAYER_WEAPON_ATTACK_PENALTY;
    }
    if (player_state.equipped_accessory_id == KF_ITEM_ATTACK_BONUS_ACCESSORY
        || player_state.equipped_extra_id == KF_ITEM_ATTACK_BONUS_ACCESSORY) {
        player_state.attack_components[1] += 5;
        player_state.attack_components[2] += 5;
        player_state.attack_components[3] += 22;
        player_state.attack_components[7] += 12;
    }
    if (player_state.equipped_accessory_id == KF_ITEM_MAGIC_BONUS_ACCESSORY
        || player_state.equipped_extra_id == KF_ITEM_MAGIC_BONUS_ACCESSORY) {
        player_state.magic += 8;
    }
    if (player_state.equipped_accessory_id == KF_ITEM_PHYSICAL_POWER_BONUS_ACCESSORY
        || player_state.equipped_extra_id == KF_ITEM_PHYSICAL_POWER_BONUS_ACCESSORY) {
        player_state.physical_power += 8;
    }

    if (player_state.physical_power > PLAYER_BONUS_OVERFLOW_LIMIT) {
        player_state.physical_power = 0;
    } else if (player_state.physical_power >= PLAYER_POWER_CAP_THRESHOLD) {
        player_state.physical_power = KF_PLAYER_POWER_MAX;
    }
    if (player_state.magic > PLAYER_BONUS_OVERFLOW_LIMIT) {
        player_state.magic = 0;
    } else if (player_state.magic >= PLAYER_POWER_CAP_THRESHOLD) {
        player_state.magic = KF_PLAYER_POWER_MAX;
    }
    if (player_state.attack_components[0] > PLAYER_BONUS_OVERFLOW_LIMIT) {
        player_state.attack_components[0] = 0;
    }
    if (player_state.attack_components[1] > PLAYER_BONUS_OVERFLOW_LIMIT) {
        player_state.attack_components[1] = 0;
    }
    if (player_state.attack_components[2] > PLAYER_BONUS_OVERFLOW_LIMIT) {
        player_state.attack_components[2] = 0;
    }
    if (player_state.attack_components[3] > PLAYER_BONUS_OVERFLOW_LIMIT) {
        player_state.attack_components[3] = 0;
    }
    if (player_state.attack_components[4] > PLAYER_BONUS_OVERFLOW_LIMIT) {
        player_state.attack_components[4] = 0;
    }
    if (player_state.attack_components[5] > PLAYER_BONUS_OVERFLOW_LIMIT) {
        player_state.attack_components[5] = 0;
    }
    if (player_state.attack_components[6] > PLAYER_BONUS_OVERFLOW_LIMIT) {
        player_state.attack_components[6] = 0;
    }
    if (player_state.attack_components[7] > PLAYER_BONUS_OVERFLOW_LIMIT) {
        player_state.attack_components[7] = 0;
    }
    if (player_state.combat_components[0] > PLAYER_BONUS_OVERFLOW_LIMIT) {
        player_state.combat_components[0] = 0;
    }
    if (player_state.combat_components[1] > PLAYER_BONUS_OVERFLOW_LIMIT) {
        player_state.combat_components[1] = 0;
    }
    if (player_state.combat_components[2] > PLAYER_BONUS_OVERFLOW_LIMIT) {
        player_state.combat_components[2] = 0;
    }
    if (player_state.combat_components[3] > PLAYER_BONUS_OVERFLOW_LIMIT) {
        player_state.combat_components[3] = 0;
    }
    if (player_state.combat_components[4] > PLAYER_BONUS_OVERFLOW_LIMIT) {
        player_state.combat_components[4] = 0;
    }
    if (player_state.combat_components[5] > PLAYER_BONUS_OVERFLOW_LIMIT) {
        player_state.combat_components[5] = 0;
    }
    if (player_state.combat_components[6] > PLAYER_BONUS_OVERFLOW_LIMIT) {
        player_state.combat_components[6] = 0;
    }
    if (player_state.combat_components[7] > PLAYER_BONUS_OVERFLOW_LIMIT) {
        player_state.combat_components[7] = 0;
    }
    if (player_state.combat_components[8] > PLAYER_BONUS_OVERFLOW_LIMIT) {
        player_state.combat_components[8] = 0;
    }
}

ADDRESS(0x80024b7c, 0x98)
void player_increment_physical_power_training(void)
{
    player_state.physical_power_training++;
    if (player_state.physical_power_training >= KF_PLAYER_TRAINING_POINTS_PER_GAIN) {
        player_state.base_physical_power++;
        player_state.physical_power_training = 0;
        if (player_state.base_physical_power > KF_PLAYER_POWER_MAX) {
            player_state.base_physical_power = KF_PLAYER_POWER_MAX;
        } else {
            notify_enqueue(KF_NOTIFICATION_PHYSICAL_POWER_INCREASED);
        }
        player_recalculate_combat_stats();
    }
}

ADDRESS(0x80024c14, 0x98)
void player_increment_magic_training(void)
{
    player_state.magic_training++;
    if (player_state.magic_training >= KF_PLAYER_TRAINING_POINTS_PER_GAIN) {
        player_state.base_magic++;
        player_state.magic_training = 0;
        if (player_state.base_magic > KF_PLAYER_POWER_MAX) {
            player_state.base_magic = KF_PLAYER_POWER_MAX;
        } else {
            notify_enqueue(KF_NOTIFICATION_MAGIC_POWER_INCREASED);
        }
        player_recalculate_combat_stats();
    }
}

ADDRESS(0x80024cac, 0x220)
void player_add_experience(s16 amount)
{
    const KfPlayerLevelGrowth *growth;
    u8 level;

    player_state.experience += amount;
    if (player_state.experience > KF_PLAYER_EXPERIENCE_MAX) {
        player_state.experience = KF_PLAYER_EXPERIENCE_MAX;
    }
    while (player_state.experience >= player_state.next_level_experience) {
        level = player_state.level;
        if (player_state.level >= KF_PLAYER_LEVEL_MAX) {
            break;
        }
        player_state.level = level + 1;
        if (level >= KF_PLAYER_LEVEL_GROWTH_COUNT) {
            growth = &player_level_growth_table[KF_PLAYER_LEVEL_GROWTH_COUNT - 1];
            player_state.vitals.maximum_hp +=
                growth->maximum_hp
                - growth[-1].maximum_hp;
            player_state.vitals.maximum_mp +=
                growth->maximum_mp
                - growth[-1].maximum_mp;
            player_state.base_physical_power += growth->physical_power_step;
            player_state.base_magic += growth->magic_step;
            player_state.next_level_experience +=
                growth->experience_threshold
                - growth[-1].experience_threshold;
        } else {
            growth = &player_level_growth_table[level];
            player_state.vitals.maximum_hp = growth->maximum_hp;
            player_state.vitals.maximum_mp = growth->maximum_mp;
            player_state.base_physical_power += growth->physical_power_step;
            player_state.base_magic += growth->magic_step;
            player_state.next_level_experience = growth->experience_threshold;
        }
        if (player_state.vitals.maximum_hp > KF_PLAYER_VITAL_MAX) {
            player_state.vitals.maximum_hp = KF_PLAYER_VITAL_MAX;
        }
        if (player_state.vitals.maximum_mp > KF_PLAYER_VITAL_MAX) {
            player_state.vitals.maximum_mp = KF_PLAYER_VITAL_MAX;
        }
        if (player_state.base_physical_power > KF_PLAYER_POWER_MAX) {
            player_state.base_physical_power = KF_PLAYER_POWER_MAX;
        }
        if (player_state.base_magic > KF_PLAYER_POWER_MAX) {
            player_state.base_magic = KF_PLAYER_POWER_MAX;
        }
        player_recalculate_combat_stats();
        notify_enqueue(KF_NOTIFICATION_LEVEL_UP);
    }
}

enum {
    KF_DAMAGE_SUBUNITS_PER_HP = 16,
    KF_DAMAGE_POWER_DIVISOR = 5,
    KF_DAMAGE_THRESHOLD_MULTIPLIER = 2
};

ADDRESS(0x80024ecc, 0xc4)
s32 player_calculate_damage_component(s32 base_power, s32 defense, s32 attack)
{
    s32 threshold = base_power;
    s32 defense_scaled = defense;
    s32 attack_scaled = attack;
    s32 excess;

    threshold = (threshold * player_state.damage_scale) >> 8;
    attack_scaled <<= 4;
    defense_scaled <<= 4;
    if (attack_scaled == 0) {
        return 0;
    }
    threshold += defense_scaled;
    excess = attack_scaled - threshold;
    if (excess < 0) {
        excess = 0;
    }
    if (threshold == 0) {
        threshold = KF_DAMAGE_SUBUNITS_PER_HP;
    }
    return (excess + (attack_scaled * attack_scaled)
                     / (threshold * KF_DAMAGE_THRESHOLD_MULTIPLIER))
           / KF_DAMAGE_POWER_DIVISOR;
}

ADDRESS(0x80024f90, 0x50)
void player_adjust_hp_unclamped(s32 delta)
{
    s32 hp = player_state.vitals.current_hp + delta;

    if (hp <= 0) {
        hp = 0;
        player_death_begin(NULL);
    }
    player_state.vitals.current_hp = hp;
}


RODATA(0x80011128, 0x224)

enum {
    KF_PLAYER_DAMAGE_ORIGIN_HEIGHT = 850,
    KF_PLAYER_DAMAGE_MOTION_MIN = 80,
    KF_PLAYER_DAMAGE_MOTION_MAX = 300,
    KF_PLAYER_DAMAGE_STRONG_MAX = 600,
    KF_PLAYER_DAMAGE_VERTICAL_LIMIT = 200,
    KF_PLAYER_DAMAGE_DURATION_BASE = 1300,
    KF_PLAYER_DAMAGE_DURATION_MIN = 70
};

ADDRESS(0x80024fe0, 0x34c)
void player_apply_damage_reaction(const VECTOR *origin, s32 damage, s32 reaction_flags)
{
    struct KfEulerAngles angles;
    SVECTOR direction;
    s32 remaining;
    s32 intensity;
    s32 magnitude;
    s16 duration;
    s32 direction_index;
    s32 origin_height;

    if (player_state.death_state == KF_PLAYER_REACTION_DEATH || damage == 0) {
        return;
    }
    remaining = player_state.vitals.current_hp - damage;
    if (remaining <= 0) {
        remaining = 0;
    }
    intensity = (damage << 11) / player_state.vitals.maximum_hp;
    player_state.vitals.current_hp = remaining;

    if (origin != NULL) {
        magnitude = intensity >> 2;
        origin_height = origin->vy + KF_PLAYER_DAMAGE_ORIGIN_HEIGHT;
        vector_displacement_to_pitch_yaw(
            player_state.camera_position.vx - origin->vx,
            player_state.camera_position.vy - origin_height,
            player_state.camera_position.vz - origin->vz, &angles);
        if (magnitude < KF_PLAYER_DAMAGE_MOTION_MIN) {
            magnitude = KF_PLAYER_DAMAGE_MOTION_MIN;
        }
        if (reaction_flags & KF_PLAYER_DAMAGE_REACTION_STRONG) {
            magnitude *= 5;
            if (magnitude > KF_PLAYER_DAMAGE_STRONG_MAX) {
                magnitude = KF_PLAYER_DAMAGE_STRONG_MAX;
            }
        } else if (magnitude > KF_PLAYER_DAMAGE_MOTION_MAX) {
            magnitude = KF_PLAYER_DAMAGE_MOTION_MAX;
        }
        pitch_yaw_to_forward_vector(&angles, &direction);
        vector3s_scale_shift12(magnitude, &direction);
        if (direction.vy > KF_PLAYER_DAMAGE_VERTICAL_LIMIT) {
            direction.vy = KF_PLAYER_DAMAGE_VERTICAL_LIMIT;
        } else if (direction.vy < -KF_PLAYER_DAMAGE_VERTICAL_LIMIT) {
            direction.vy = -KF_PLAYER_DAMAGE_VERTICAL_LIMIT;
        }
    } else {
        direction.vz = 0;
        direction.vy = 0;
        direction.vx = 0;
    }

    duration = (KF_PLAYER_DAMAGE_DURATION_BASE - (intensity >> 1)) >> 2;
    if (duration <= KF_PLAYER_DAMAGE_DURATION_MIN) {
        duration = KF_PLAYER_DAMAGE_DURATION_MIN;
    }
    if (remaining == 0) {
        player_death_begin(&direction);
        player_state.damage_red_overlay_scale = 3500;
        player_state.damage_red_overlay_decay = duration;
        return;
    }
    if (origin == NULL) {
        player_state.damage_red_overlay_scale = 3500;
        player_state.damage_red_overlay_decay = duration;
        return;
    }

    intensity = SquareRoot0(intensity << 2);
    if (intensity < 16) {
        intensity = 16;
    } else if (intensity > 42) {
        intensity = 42;
    }
    if (reaction_flags & KF_PLAYER_DAMAGE_REACTION_STRONG) {
        intensity <<= 1;
    }
    if (reaction_flags & KF_PLAYER_DAMAGE_REACTION_ROTATION_ONLY) {
        angles.x = 0;
        if (rand() >= 16385) {
            angles.z = intensity;
        } else {
            angles.z = -intensity;
        }
        angles.y = 0;
        player_begin_rotation_only_damage_reaction(&direction, (const SVECTOR *)&angles, duration);
        return;
    }

    direction_index = ((angles.y - player_state.camera_rotation.angles[1] + 256) >> 9) & 7;
    switch (direction_index) {
    case 0:
        angles.x = -intensity;
        angles.z = 0;
        break;
    case 1:
        angles.x = -intensity;
        angles.z = intensity;
        break;
    case 2:
        angles.x = 0;
        angles.z = intensity;
        break;
    case 3:
        angles.x = intensity;
        angles.z = intensity;
        break;
    case 4:
        angles.x = intensity;
        angles.z = 0;
        break;
    case 5:
        angles.x = intensity;
        angles.z = -intensity;
        break;
    case 6:
        angles.x = 0;
        angles.z = -intensity;
        break;
    case 7:
        angles.x = -intensity;
        angles.z = -intensity;
        break;
    default:
        break;
    }
    angles.y = 0;
    player_begin_moving_damage_reaction(&direction, (const SVECTOR *)&angles, duration);
}

enum {
    KF_PLAYER_STATUS_CAP = 64
};

ADDRESS(0x8002532c, 0xc4)
void player_cap_status_components(KfPlayerStatusMask mask)
{
    if ((mask & KF_PLAYER_STATUS_FIRST) != KF_PLAYER_STATUS_NONE) {
        if (player_state.curse_strength > KF_PLAYER_STATUS_CAP) {
            player_state.curse_strength = KF_PLAYER_STATUS_CAP;
        }
        player_state.curse_phase_limit = 0;
    }
    if ((mask & KF_PLAYER_STATUS_SECOND) != KF_PLAYER_STATUS_NONE) {
        if (player_state.darkness_phase > KF_PLAYER_STATUS_CAP) {
            player_state.darkness_phase = KF_PLAYER_STATUS_CAP;
        }
        player_state.darkness_phase_limit = 0;
    }
    if ((mask & (KF_PLAYER_STATUS_FIRST | KF_PLAYER_STATUS_SECOND)) != KF_PLAYER_STATUS_NONE) {
        player_state.poison_timer = 0;
    }
    if ((mask & (KF_PLAYER_STATUS_FIRST | KF_PLAYER_STATUS_THIRD)) != KF_PLAYER_STATUS_NONE) {
        if (player_state.slow_timer > KF_PLAYER_STATUS_CAP) {
            player_state.slow_timer = KF_PLAYER_STATUS_CAP;
        }
    }
    if ((mask & KF_PLAYER_STATUS_THIRD) != KF_PLAYER_STATUS_NONE) {
        player_state.paralysis_timer = 0;
    }
}

enum {
    KF_PLAYER_STATUS_GUARD_CHANCE = 16384,
    KF_PLAYER_POISON_ROLL_SCALE = 100,
    KF_PLAYER_POISON_ROLL_SHIFT = 15
};

/* The encoded low-nibble status kind is one greater than this switch index. */
enum {
    PLAYER_DAMAGE_STATUS_CURSE = 1,
    PLAYER_DAMAGE_STATUS_DARKNESS = 2,
    PLAYER_DAMAGE_STATUS_POISON = 3,
    PLAYER_DAMAGE_STATUS_PARALYSIS = 4,
    PLAYER_DAMAGE_STATUS_SLOW = 5,
    PLAYER_DAMAGE_STATUS_MP_DRAIN = 6,
    PLAYER_DAMAGE_STATUS_POISON_CLEAR = 7
};

ADDRESS(0x800253f0, 0x3fc)
void player_apply_damage(u16 damage0, u16 damage1, u16 damage2, u16 damage_flags,
                   u16 damage3, u16 damage4, u16 damage5, u16 damage6,
                   u16 damage7, u16 scale_q16, u16 multiplier_tenths,
                   const VECTOR *origin)
{
    s32 total;
    s32 mp_loss;
    s32 damage_loss;
    s32 curse_phase_limit;
    s32 darkness_phase_limit;
    s32 paralysis_duration;
    s32 slow_duration;
    s32 poison_duration;
    u16 flags = damage_flags;

    if (player_state.weapon_guard_active != 0) {
        return;
    }
    if (player_state.equipped_accessory_id == KF_ITEM_STATUS_GUARD_ACCESSORY
        || player_state.equipped_extra_id == KF_ITEM_STATUS_GUARD_ACCESSORY) {
        if (rand() < KF_PLAYER_STATUS_GUARD_CHANCE) {
            flags &= ~KF_PLAYER_DAMAGE_GUARDED_STATUS;
        }
    }
    if (player_state.equipped_accessory_id == KF_ITEM_STATUS_DURATION_HALVING_ACCESSORY
        || player_state.equipped_extra_id == KF_ITEM_STATUS_DURATION_HALVING_ACCESSORY) {
        curse_phase_limit = 300;
        darkness_phase_limit = 250;
        poison_duration = 300;
        paralysis_duration = 100;
        slow_duration = 300;
    } else {
        curse_phase_limit = 600;
        darkness_phase_limit = 500;
        poison_duration = 600;
        paralysis_duration = 200;
        slow_duration = 600;
    }

    switch (flags & KF_PLAYER_DAMAGE_STATUS_MASK) {
    case PLAYER_DAMAGE_STATUS_CURSE:
        player_state.curse_phase_limit = curse_phase_limit;
        player_state.curse_strength = 1;
        player_recalculate_combat_stats();
        break;
    case PLAYER_DAMAGE_STATUS_DARKNESS:
        player_state.darkness_phase_limit = darkness_phase_limit;
        break;
    case PLAYER_DAMAGE_STATUS_POISON:
        if (player_state.equipped_accessory_id == KF_ITEM_POISON_GUARD_ACCESSORY
            || player_state.equipped_extra_id == KF_ITEM_POISON_GUARD_ACCESSORY) {
            if (rand() < KF_PLAYER_STATUS_GUARD_CHANCE) {
                break;
            }
        }
        if (player_state.combat_components[KF_PLAYER_COMBAT_POISON_RESISTANCE]
            < ((rand() * KF_PLAYER_POISON_ROLL_SCALE) >> KF_PLAYER_POISON_ROLL_SHIFT)) {
            player_state.poison_timer = poison_duration;
        }
        break;
    case PLAYER_DAMAGE_STATUS_PARALYSIS:
        player_state.paralysis_timer = paralysis_duration;
        break;
    case PLAYER_DAMAGE_STATUS_SLOW:
        player_state.slow_timer = slow_duration;
        break;
    case PLAYER_DAMAGE_STATUS_POISON_CLEAR:
        player_state.poison_timer = 0;
        break;
    case PLAYER_DAMAGE_STATUS_MP_DRAIN:
        mp_loss = player_state.vitals.maximum_mp / 6;
        if (mp_loss < player_state.vitals.current_mp) {
            player_state.vitals.current_mp -= mp_loss;
        } else {
            player_state.vitals.current_mp = 0;
        }
        break;
    }

    total = player_calculate_damage_component(
        player_state.physical_power, player_state.combat_components[0], damage0);
    total += player_calculate_damage_component(
        player_state.physical_power, player_state.combat_components[1], damage1);
    total += player_calculate_damage_component(
        player_state.physical_power, player_state.combat_components[2], damage2);
    total += player_calculate_damage_component(
        player_state.physical_power, player_state.combat_components[4], damage3);
    total += player_calculate_damage_component(
        player_state.physical_power, player_state.combat_components[5], damage4);
    total += player_calculate_damage_component(
        player_state.physical_power, player_state.combat_components[6], damage5);
    total += player_calculate_damage_component(
        player_state.physical_power, player_state.combat_components[7], damage6);
    total += player_calculate_damage_component(
        player_state.physical_power, player_state.combat_components[8], damage7);
    total = (scale_q16 * total + 0x8000) >> 16;
    damage_loss = multiplier_tenths * total / 10;
    player_apply_damage_reaction(origin, damage_loss, flags);
}

enum {
    KF_RADIAL_REACH_OFFSET = 800,
    KF_RADIAL_NO_REACTION_ORIGIN = 0x8000,
    KF_RADIAL_BASE_SCALE_MASK = 0x7fff
};

ADDRESS(0x800257ec, 0x230)
void player_apply_radial_damage(VECTOR *position, s32 start, s32 end, s32 mode,
                   u16 falloff, u16 damage0, u16 damage1, u16 damage2,
                   u16 damage3, u16 damage4, u16 damage5, u16 damage6,
                   u16 damage7, u16 damage8, s32 scale_and_flags, u16 record_id)
{
    const VECTOR *reaction_origin = position;
    s32 distance;
    u16 attenuation;
    u16 base_scale;

    if (scale_and_flags & KF_RADIAL_NO_REACTION_ORIGIN) {
        reaction_origin = NULL;
    }
    base_scale = scale_and_flags & KF_RADIAL_BASE_SCALE_MASK;

    if (mode == KF_RADIAL_MODE_REACH) {
        distance = vector_distance_between_with_reach(position, end, &player_state.camera_position,
                                  KF_RADIAL_REACH_OFFSET, KF_PLAYER_HEIGHT);
    } else if (mode == KF_RADIAL_MODE_REACH_ABOVE) {
        if (position->vy < player_state.camera_position.vy - KF_PLAYER_HEIGHT) {
            distance = KF_DISTANCE_OUTSIDE_REACH;
        } else {
            distance = vector_distance_between_with_reach(position, end,
                                      &player_state.camera_position,
                                      KF_RADIAL_REACH_OFFSET, KF_PLAYER_HEIGHT);
        }
    } else {
        distance = player_distance_to_point(position->vx, position->vy,
                                            position->vz, end, mode);
        if (distance == KF_DISTANCE_NONE) {
            distance = KF_DISTANCE_OUTSIDE_REACH;
        }
    }

    if (distance < start) {
        return;
    }

    if (falloff != KF_FIXED12_ONE) {
        u16 ratio = (distance << KF_FIXED12_BITS) / end;
        u16 factor = KF_FIXED12_ONE
                   - ((u32)(ratio * (KF_FIXED12_ONE - falloff)) >> KF_FIXED12_BITS);
        attenuation = (base_scale * factor) >> KF_FIXED12_BITS;
    } else {
        attenuation = base_scale;
    }

    player_apply_damage(damage0, damage1, damage2, damage3,
                  damage4, damage5, damage6, damage7,
                  damage8, attenuation, record_id, reaction_origin);
}

ADDRESS(0x80025a1c, 0x78)
void player_get_camera_pose(VECTOR *position, SVECTOR *angles)
{
    position->vx = player_state.camera_position.vx;
    position->vz = player_state.camera_position.vz;
    position->vy = player_state.camera_vertical_offset + player_state.camera_position.vy
                 + player_state.landing_vertical_offset - KF_PLAYER_CAMERA_EYE_OFFSET;
    angles->vx = player_state.camera_rotation.angles[0];
    angles->vy = player_state.camera_rotation.angles[1];
    angles->vz = player_state.camera_rotation.angles[2];
}

ADDRESS(0x80025a94, 0xb8)
void player_reset_status(void)
{
    player_state.magic_boost_timer = 0;
    player_state.full_mp_timer = 0;
    player_state.map_marker_visual_effect_timer = 0;
    player_state.magic_tint_phase_limit = 0;
    player_state.magic_tint_phase = 0;
    player_state.attack_boost_timer = 0;
    player_state.defense_boost_timer = 0;
    player_state.paralysis_timer = 0;
    player_state.slow_timer = 0;
    player_state.darkness_phase_limit = 0;
    player_state.darkness_phase = 0;
    player_state.curse_phase_limit = 0;
    player_state.curse_strength = 0;
    player_state.poison_timer = 0;
    player_state.fatal_fall_latch = KF_FALSE;
    player_state.vitals.current_hp = player_state.vitals.maximum_hp;
    player_state.vitals.current_mp = player_state.vitals.maximum_mp;
    player_reset_view();
}

enum {
    PLAYER_INITIAL_LEVEL = 1,
    PLAYER_INITIAL_CAMERA_PITCH = 2500,
    PLAYER_INITIAL_CAMERA_X = 0x11800,
    PLAYER_INITIAL_CAMERA_Y = -12800,
    PLAYER_INITIAL_CAMERA_Z = 0x18000,
    PLAYER_INITIAL_MAP_LAYER = 5
};

ADDRESS(0x80025b4c, 0x180)
void player_initialize_state(void)
{
    s32 i;
    KfMagicRecord *entry;

    player_state.experience = 0;
    player_state.level = PLAYER_INITIAL_LEVEL;
    player_state.gold = 0;
    player_state.damage_red_overlay_scale = 0;
    player_state.damage_red_overlay_decay = 0;
    player_state.equipped_head_id = KF_OBJECT_NONE;
    player_state.equipped_body_id = KF_OBJECT_NONE;
    player_state.equipped_leg_id = KF_OBJECT_NONE;
    player_state.equipped_shield_id = KF_OBJECT_NONE;
    player_state.equipped_arm_id = KF_OBJECT_NONE;
    player_state.equipped_accessory_id = KF_OBJECT_NONE;
    player_state.equipped_extra_id = KF_OBJECT_NONE;
    player_state.vitals.maximum_hp = player_level_growth_table[0].maximum_hp;
    player_state.vitals.maximum_mp = player_level_growth_table[0].maximum_mp;
    player_state.base_physical_power = player_level_growth_table[0].physical_power_step;
    player_state.base_magic = player_level_growth_table[0].magic_step;
    player_state.next_level_experience = player_level_growth_table[0].experience_threshold;
    player_set_equipment_slot(KF_OBJECT_NONE, KF_EQUIPMENT_SLOT_NONE);
    player_state.selected_magic_record = NULL;
    player_set_primary_magic_shortcut_id(KF_MAGIC_NONE);
    player_equip_weapon(KF_OBJECT_0);
    player_set_secondary_magic_shortcut_id(KF_MAGIC_NONE);
    player_set_secondary_item_shortcut_id(KF_OBJECT_NONE);
    player_state.camera_rotation_target.angles[0] = 0;
    player_state.camera_rotation_target.angles[1] = PLAYER_INITIAL_CAMERA_PITCH;
    player_state.camera_rotation_target.angles[2] = 0;
    player_state.camera_position.vy = PLAYER_INITIAL_CAMERA_Y;
    player_state.camera_position.vx = PLAYER_INITIAL_CAMERA_X;
    player_state.camera_position.vz = PLAYER_INITIAL_CAMERA_Z;
    player_state.map_layer_index = PLAYER_INITIAL_MAP_LAYER;
    entry = effect_state.magic_records;
    for (i = KF_MAGIC_RECORD_COUNT - 1; i != -1; i--) {
        entry->menu_available = 0;
        entry++;
    }
    player_reset_status();
}

ADDRESS(0x80025ccc, 0x6c)
void game_initialize_session(void)
{
    player_state.weapon_asset_buffer = (KfAssetHeader *)&player_weapon_asset_buffer;
    player_initialize_state();
    player_state.audio_effects_enabled = KF_PLAYER_OPTION_ON;
    player_state.audio_music_enabled = KF_PLAYER_OPTION_ON;
    player_state.hud_gauges_enabled = KF_PLAYER_OPTION_ON;
    player_state.compass_enabled = KF_PLAYER_OPTION_ON;
    player_state.item_preview_enabled = KF_PLAYER_OPTION_ON;
    player_state.walking_bob_enabled = KF_PLAYER_OPTION_ON;
    player_state.force_actor_lifecycle_refresh = KF_FALSE;
}

ADDRESS(0x80025d38, 0x70)
void player_clear_motion(void)
{
    player_state.yaw_step = 0;
    player_state.pitch_step = 0;
    player_state.movement_speed.unsigned_value = 0;
    player_state.forward_velocity = 0;
    player_state.strafe_velocity = 0;
    player_state.pad_buttons.current &= DAT_8006e568 | DAT_8006e570 | DAT_8006e574 | DAT_8006e578;
}

ADDRESS(0x80025da8, 0xb0)
void player_sync_position_to_map(void)
{
    KF_ENUM_STORAGE(KfMapLayerMask, s32) layer;

    player_state.equipment_effect_ticks = 0;
    layer = KF_MAP_LAYER_SECOND;
    if (player_state.map_layer_index == 0) {
        layer = KF_MAP_LAYER_FIRST;
    }
    player_state.camera_position.vy =
        collision_sample_map_layer_height(layer, player_state.camera_position.vx,
                      player_state.camera_position.vz,
                      KF_PLAYER_COLLISION_RADIUS, KF_PLAYER_HEIGHT);
    player_update_collision_bounds();
    player_state.force_actor_lifecycle_refresh = KF_TRUE;
    player_state.vertical_motion_state = KF_PLAYER_VERTICAL_GROUNDED;
    player_state.vertical_velocity = 0;
    player_state.death_state = KF_PLAYER_REACTION_NORMAL;
    player_clear_motion();
    map_cell_add_layer_occupancy(player_state.camera_position.vx,
                  player_state.camera_position.vz, KF_PLAYER_COLLISION_RADIUS, 1);
}

ADDRESS(0x80025e58, 0xc8)
s32 player_distance_to_point_in_cone(
    const VECTOR *point, s16 facing, s32 max_distance, s32 angle_tolerance)
{
    s32 distance;
    s16 delta;

    distance = player_distance_to_point(point->vx, KF_DISTANCE_IGNORE_HEIGHT, point->vz, max_distance, 0);
    if (distance != KF_DISTANCE_NONE) {
        delta = (vector_xz_to_angle(
                     player_state.camera_position.vx - point->vx,
                     player_state.camera_position.vz - point->vz)
                 - facing) & KF_ANGLE_WRAP_MASK;
        delta = angle_error_magnitude(delta);
        if (angle_tolerance < delta) {
            distance = KF_DISTANCE_NONE;
        }
    }
    return distance;
}

ADDRESS(0x80025f20, 0x50)
s32 player_distance_to_point(
    s32 point_x, s32 point_y, s32 point_z, s32 max_distance, s32 point_height)
{
    return vector_distance_to_point(
        &player_state.camera_position, point_x, point_y, point_z, max_distance,
        KF_PLAYER_HEIGHT, point_height);
}



ADDRESS(0x80025f70, 0x10)
void player_set_primary_magic_shortcut_id(KfEffectKind value)
{
    player_state.primary_magic_shortcut_id = value;
}

ADDRESS(0x80025f80, 0x28)
void player_set_secondary_magic_shortcut_id(KfEffectKind value)
{
    player_state.secondary_magic_shortcut_id = value;
    if (value != KF_MAGIC_NONE) {
        player_state.secondary_item_shortcut_id = KF_OBJECT_NONE;
    }
}

ADDRESS(0x80025fa8, 0x28)
void player_set_secondary_item_shortcut_id(KF_ENUM_PARAM(KfObjectId, u8) value)
{
    player_state.secondary_item_shortcut_id = value;
    if (value != KF_OBJECT_NONE) {
        player_state.secondary_magic_shortcut_id = KF_MAGIC_NONE;
    }
}

ADDRESS(0x80025fd0, 0x240)
void player_set_equipment_slot(KF_ENUM_PARAM(KfObjectId, u8) item_id, KfEquipmentSlot slot)
{
    switch (slot) {
    case KF_EQUIPMENT_SLOT_HEAD:
        player_state.equipped_head_id = item_id;
        break;
    case KF_EQUIPMENT_SLOT_BODY:
        player_state.equipped_body_id = item_id;
        break;
    case KF_EQUIPMENT_SLOT_LEG:
        player_state.equipped_leg_id = item_id;
        break;
    case KF_EQUIPMENT_SLOT_SHIELD:
        player_state.equipped_shield_id = item_id;
        break;
    case KF_EQUIPMENT_SLOT_ARM:
        player_state.equipped_arm_id = item_id;
        break;
    case KF_EQUIPMENT_SLOT_ACCESSORY:
        player_state.equipped_accessory_id = item_id;
        break;
    case KF_EQUIPMENT_SLOT_EXTRA:
        player_state.equipped_extra_id = item_id;
        break;
    default:
        break;
    }

    if (player_state.equipped_head_id != KF_OBJECT_NONE) {
        player_state.equipped_head_record = &player_equipment_records[KF_ENUM_ENCODE(u8,
            player_state.equipped_head_id) - KF_EQUIPMENT_RECORD_FIRST_ID];
    } else {
        player_state.equipped_head_record = NULL;
    }
    if (player_state.equipped_body_id != KF_OBJECT_NONE) {
        player_state.equipped_body_record = &player_equipment_records[KF_ENUM_ENCODE(u8,
            player_state.equipped_body_id) - KF_EQUIPMENT_RECORD_FIRST_ID];
    } else {
        player_state.equipped_body_record = NULL;
    }
    if (player_state.equipped_leg_id != KF_OBJECT_NONE) {
        player_state.equipped_leg_record = &player_equipment_records[KF_ENUM_ENCODE(u8,
            player_state.equipped_leg_id) - KF_EQUIPMENT_RECORD_FIRST_ID];
    } else {
        player_state.equipped_leg_record = NULL;
    }
    if (player_state.equipped_shield_id != KF_OBJECT_NONE) {
        player_state.equipped_shield_record = &player_equipment_records[KF_ENUM_ENCODE(u8,
            player_state.equipped_shield_id) - KF_EQUIPMENT_RECORD_FIRST_ID];
    } else {
        player_state.equipped_shield_record = NULL;
    }
    if (player_state.equipped_arm_id != KF_OBJECT_NONE) {
        player_state.equipped_arm_record = &player_equipment_records[KF_ENUM_ENCODE(u8,
            player_state.equipped_arm_id) - KF_EQUIPMENT_RECORD_FIRST_ID];
    } else {
        player_state.equipped_arm_record = NULL;
    }
    if (player_state.equipped_accessory_id != KF_OBJECT_NONE) {
        player_state.equipped_accessory_record = &player_equipment_records[KF_ENUM_ENCODE(u8,
            player_state.equipped_accessory_id) - KF_EQUIPMENT_RECORD_FIRST_ID];
    } else {
        player_state.equipped_accessory_record = NULL;
    }
    if (player_state.equipped_extra_id != KF_OBJECT_NONE) {
        player_state.equipped_extra_record = &player_equipment_records[KF_ENUM_ENCODE(u8,
            player_state.equipped_extra_id) - KF_EQUIPMENT_RECORD_FIRST_ID];
    } else {
        player_state.equipped_extra_record = NULL;
    }
    player_recalculate_combat_stats();
}

enum {
    PLAYER_WEAPON_CHARGE_DELAY_UPDATES = 10,
    PLAYER_WEAPON_ARCHIVE_FIRST_ENTRY = 49
};

ADDRESS(0x80026210, 0xb8)
void player_equip_weapon(KF_ENUM_PARAM(KfObjectId, u8) weapon_id)
{
    player_state.attack_charge_current = 0;
    player_state.attack_charge_committed = 0;
    player_state.weapon_charge_delay = PLAYER_WEAPON_CHARGE_DELAY_UPDATES;
    player_state.equipped_weapon_id = weapon_id;
    if (weapon_id != KF_OBJECT_NONE) {
        player_state.equipped_weapon_record = &player_weapon_records[KF_ENUM_ENCODE(u8, weapon_id)];
        cd_archive_read(KF_RESOURCE_ARCHIVE_FDAT,
                        KF_ENUM_ENCODE(u8, weapon_id) + PLAYER_WEAPON_ARCHIVE_FIRST_ENTRY,
                        (u_long *)player_state.weapon_asset_buffer);
        asset_registry_set(KF_PLAYER_WEAPON_ASSET_INDEX,
                           player_state.weapon_asset_buffer);
    }
    player_state.weapon_attack_phase = KF_WEAPON_ATTACK_INACTIVE;
    player_state.weapon_animation_cache = NULL;
    player_state.weapon_magic_shots_remaining = 0;
    player_state.weapon_guard_active = KF_FALSE;
    player_recalculate_combat_stats();
}

ADDRESS(0x800262c8, 0x124)
void player_begin_weapon_attack(KF_ENUM_PARAM(KfAnimationClip, s32) mode)
{
    if (player_state.weapon_attack_phase != KF_WEAPON_ATTACK_INACTIVE
        || player_state.equipped_weapon_id == KF_OBJECT_NONE
        || player_state.paralysis_timer != 0) {
        return;
    }

    player_state.weapon_attack_mode = mode;
    player_state.weapon_attack_phase = 0;
    if (mode == KF_ANIMATION_CLIP_FIRST) {
        player_state.weapon_attack_window = player_state.equipped_weapon_record->normal_attack_end_phase;
        player_state.weapon_next_sound_phase = player_state.equipped_weapon_record->normal_attack_sound_phase;
    } else {
        player_state.weapon_attack_window = player_state.equipped_weapon_record->alternate_attack_window_start;
        player_state.weapon_next_sound_phase = player_state.equipped_weapon_record->alternate_attack_sound_start_phase;
    }
    player_state.attack_charge_committed = player_state.attack_charge_current;
    if (player_state.attack_charge_current == KF_PLAYER_CHARGE_FULL
        && player_state.magic_charge == KF_PLAYER_CHARGE_FULL) {
        player_state.weapon_attack_fully_charged = KF_TRUE;
        player_state.weapon_magic_shots_configured = player_state.equipped_weapon_record->magic_shots;
    } else {
        player_state.weapon_attack_fully_charged = KF_FALSE;
    }
    player_state.attack_charge_current = 0;
    player_state.unknown_9c[0] = 0;
}

ADDRESS(0x800263ec, 0x1a0)
KfActor *player_probe_view_target_and_vectors(s32 scale, VECTOR *position, SVECTOR *direction,
                       s32 *distance)
{
    struct KfEulerAngles angles;
    KfActor *actor;
    KfTargetCandidate *target;
    s32 height;

    if (position != NULL) {
        angles.x = -player_state.camera_rotation.angles[0];
        angles.y = player_state.camera_rotation.angles[1];
        angles.z = player_state.camera_rotation.angles[2];
        vector_rotate_yxz(&angles, &player_state.magic_origin_offset, position);
        position->vx += player_state.camera_position.vx;
        height = position->vy - KF_PLAYER_CAMERA_EYE_OFFSET;
        position->vy = height + player_state.camera_position.vy;
        position->vz += player_state.camera_position.vz;
    }

    actor = actor_find_best_in_cone(&player_state.camera_position,
                          player_state.camera_rotation.angles[1],
                          player_state.camera_rotation.angles[0], 0x55f0,
                          0x200, 0x200, distance, 0);
    actor_state.player_view_target = actor;
    if (actor != NULL) {
        target = actor_find_target_of_type(&actor_state.target_groups[actor->group_index],
                                           KF_ACTOR_TARGET_130);
        if (target != NULL &&
            (actor->flags & KF_ACTOR_FLAG_BLOCK_PLAYER_TARGETING) == KF_ACTOR_FLAGS_NONE) {
            actor_set_target(actor, target);
        }
    }

    if (direction != NULL) {
        angles.x = player_state.camera_rotation.angles[0];
        angles.y = player_state.camera_rotation.angles[1];
        angles.z = player_state.camera_rotation.angles[2];
        pitch_yaw_to_forward_vector(&angles, direction);
        vector3s_scale_shift12(scale, direction);
    }
    return actor;
}

ADDRESS(0x8002658c, 0x918)
void player_dispatch_magic_effect(KF_ENUM_PARAM(KfEffectKind, s32) effect_id, ...)
{
    VECTOR position;
    SVECTOR direction;
    /* Retail reserves an unreferenced 8-byte frame slot. */
    s16 frame_reserve[4];
    s32 distance;
    s32 adjusted_distance;
    KfActor *actor;
    KfEffectRecord *effect;
    s32 i;
    s32 kind;
    s32 rotation_scale;
    s32 target_scale;
    s32 simple_scale;
    s32 case3_z;
    char *arguments;
    const VECTOR *override_position;
    /* The cursor stays on the named argument and is advanced before each
     * read, so every optional pointer is read one word above effect_id. */
    arguments = (char *)&effect_id;

    switch (effect_id) {
    case KF_EFFECT_KIND_7:
        simple_scale = 1000;
simple_probe:
        player_probe_view_target_and_vectors(simple_scale, &position, &direction, &distance);
emit_simple_effect:
        effect_construct_record(10, KF_EFFECT_USE_PLAYER_MAGIC | KF_EFFECT_TARGET_ACTORS, effect_id,
                                &position, &direction);
        break;
    case KF_EFFECT_KIND_2:
        effect_construct_record(10, KF_EFFECT_USE_PLAYER_MAGIC | KF_EFFECT_TARGET_ACTORS_AND_PLAYER,
                                effect_id, &player_state.camera_position,
                       NULL, 0x1000, 0x100, 0x1000);
        break;
    case KF_EFFECT_KIND_3:
        actor = player_probe_view_target_and_vectors(5000, &position, &direction, &distance);
        if (actor == NULL) {
            /* Both axes share the reused sum/base temporaries, and the camera
             * height passes through rotation_scale (set elsewhere), so no load
             * here starts a single-set register and the z loads stay after the
             * x store. */
            s32 sum;
            s32 base;

            sum = direction.vx;
            base = position.vx;
            sum += base;
            position.vx = sum;
            rotation_scale = player_state.camera_position.vy;
            position.vy = rotation_scale;
            sum = direction.vz;
            base = position.vz;
            case3_z = sum + base;
            goto case3_store_z;
        }
        position.vx = ((s32)actor->motion.vector.vx << 14) / 600 + actor->position.vx;
        position.vy = ((s32)actor->motion.vector.vy << 14) / 600 + actor->position.vy;
        position.vz = ((s32)actor->motion.vector.vz << 14) / 600 + actor->position.vz;
        if (collision_query_shapes_with_layer_sample(position.vx, position.vy, position.vz, 10, 10) !=
            KF_COLLISION_HIT_NONE) {
            position.vx = actor->position.vx;
            position.vy = actor->position.vy;
            case3_z = actor->position.vz;
            goto case3_store_z;
        }
        goto case3_emit;
case3_store_z:
        position.vz = case3_z;
case3_emit:
        effect_construct_record(10, KF_EFFECT_USE_PLAYER_MAGIC | KF_EFFECT_TARGET_ACTORS, KF_EFFECT_KIND_114,
                                &position, NULL);
        break;
    case KF_EFFECT_KIND_0:
        actor = player_probe_view_target_and_vectors(5000, &position, &direction, &distance);
        if (actor == NULL) {
            kind = KF_EFFECT_TARGET_ACTOR_NONE;
        } else {
            kind = actor - actor_state.actors;
        }
        position.vx += direction.vx;
        position.vz += direction.vz;
        effect_construct_record(10, KF_EFFECT_USE_PLAYER_MAGIC | KF_EFFECT_TARGET_ACTORS, KF_EFFECT_KIND_111,
                                &position, NULL, kind);
        break;
    case KF_EFFECT_KIND_13: {
        const SVECTOR *sequence = player_magic_square_offsets;
        for (i = 3; i != -1; i--) {
            player_state.magic_origin_offset = *sequence;
            player_probe_view_target_and_vectors(800, &position, &direction, &distance);
            sequence++;
            direction.vx += -32 + (rand() >> 9);
            direction.vy += -32 + (rand() >> 9);
            direction.vz += -32 + (rand() >> 9);
            effect_construct_record(10, KF_EFFECT_USE_PLAYER_MAGIC | KF_EFFECT_TARGET_ACTORS, effect_id,
                                    &position, &direction);
        }
        break;
    }
    case KF_EFFECT_KIND_51:
        effect_id = KF_EFFECT_KIND_118;
        goto simple_effect;
    case KF_EFFECT_KIND_52:
        effect_id = KF_EFFECT_KIND_119;
        /* fall through */
    case KF_EFFECT_KIND_4:
simple_effect:
        rotation_scale = 700;
probe_rotation_effect:
        player_probe_view_target_and_vectors(rotation_scale, &position, &direction, &distance);
emit_rotation_effect:
        effect_construct_record(10, KF_EFFECT_USE_PLAYER_MAGIC | KF_EFFECT_TARGET_ACTORS, effect_id,
                                &position, &direction,
                       &player_state.camera_rotation);
        break;
    case KF_EFFECT_KIND_11:
        player_probe_view_target_and_vectors(600, &position, &direction, &adjusted_distance);
        if (adjusted_distance != -1) {
            adjusted_distance = adjusted_distance / 600 - 8;
            if (adjusted_distance < 2) {
                adjusted_distance = 2;
            }
        } else {
            adjusted_distance = 10;
        }
        direction.vy = 0;
        effect_construct_record(10, KF_EFFECT_USE_PLAYER_MAGIC | KF_EFFECT_TARGET_ACTORS, KF_EFFECT_KIND_103,
                                &position, &direction, adjusted_distance);
        break;
    case KF_EFFECT_KIND_5:
        target_scale = 200;
select_actor_effect:
        actor = player_probe_view_target_and_vectors(target_scale, &position, &direction, &distance);
        if (actor == NULL) {
            kind = KF_EFFECT_TARGET_ACTOR_NONE;
        } else {
            kind = actor - actor_state.actors;
        }
        effect_construct_record(10, KF_EFFECT_USE_PLAYER_MAGIC | KF_EFFECT_TARGET_ACTORS, effect_id,
                                &position, &direction, kind);
        break;
    case KF_EFFECT_KIND_9:
        target_scale = 500;
        goto select_actor_effect;
    case KF_EFFECT_KIND_8:
        player_probe_view_target_and_vectors(700, &position, &direction, &distance);
        effect_construct_record(10, KF_EFFECT_USE_PLAYER_MAGIC | KF_EFFECT_TARGET_ACTORS, KF_EFFECT_KIND_106,
                                &position, &direction,
                       &player_state.camera_rotation);
        break;
    case KF_EFFECT_KIND_10:
        rotation_scale = 300;
        goto probe_rotation_effect;
    case KF_EFFECT_KIND_6:
        rotation_scale = 250;
        goto probe_rotation_effect;
    case KF_EFFECT_KIND_12: {
        s16 old_yaw = player_state.camera_rotation.angles[1];
        player_state.camera_rotation.angles[1] -=
            (u16)player_state.magic_origin_offset.vx * 2;
        player_probe_view_target_and_vectors(150, &position, &direction, &distance);
        player_state.magic_origin_offset.vx += 100;
        player_state.camera_rotation.angles[1] = old_yaw;
        effect_construct_record(10, KF_EFFECT_USE_PLAYER_MAGIC | KF_EFFECT_TARGET_ACTORS, effect_id,
                                &position, &direction,
                       &player_state.camera_rotation, 600, 60, 128, 140, 160);
        break;
    }
    case KF_EFFECT_KIND_1:
        simple_scale = 500;
        goto simple_probe;
    case KF_EFFECT_KIND_43:
        effect_id = KF_EFFECT_KIND_115;
        goto sequence_effect;
    case KF_EFFECT_KIND_42:
        effect_id = KF_EFFECT_KIND_113;
sequence_effect: {
        const KfPlayerMagicSpawnRecord *record = player_magic_spawn_records;
        player_state.magic_origin_offset = record->offset;
        player_probe_view_target_and_vectors(600, &position, &direction, &distance);
        effect = effect_construct_record(10, KF_EFFECT_USE_PLAYER_MAGIC | KF_EFFECT_TARGET_ACTORS, effect_id,
                               &position, &direction, &player_state.camera_rotation);
        if (effect != NULL) {
            s32 index = effect - effect_state.records;
            record++;
            for (i = 3; i != -1; i--) {
                player_state.magic_origin_offset = record->offset;
                player_probe_view_target_and_vectors(600, &position, &direction, &distance);
                effect_construct_record(10, KF_EFFECT_USE_PLAYER_MAGIC | KF_EFFECT_TARGET_ACTORS,
                                        record->fields.effect_kind,
                                        &position, &direction, index);
                record++;
            }
        }
        break;
    }
    case KF_EFFECT_KIND_44:
        player_probe_view_target_and_vectors(1000, &position, &direction, &distance);
        effect_id = KF_EFFECT_KIND_117;
        goto emit_rotation_effect;
    case KF_EFFECT_KIND_45:
        player_probe_view_target_and_vectors(1000, &position, &direction, &distance);
        effect_id = KF_EFFECT_KIND_116;
        goto emit_simple_effect;
    case KF_EFFECT_KIND_40:
        rotation_scale = 1000;
        goto probe_rotation_effect;
    case KF_EFFECT_KIND_39:
        player_probe_view_target_and_vectors(50, NULL, &direction, &distance);
        override_position = *(const VECTOR **)(arguments += 4);
        position = *override_position;
        goto emit_rotation_effect;
    case KF_EFFECT_KIND_49: {
        const VECTOR *override_position;
        /* Cases 49 and 50 omit the rotation argument. */
        player_probe_view_target_and_vectors(550, NULL, &direction, &distance);
        override_position = *(const VECTOR **)(arguments += 4);
        position = *override_position;
        goto emit_simple_effect;
    }
    case KF_EFFECT_KIND_50: {
        const VECTOR *override_position;
        override_position = *(const VECTOR **)(arguments += 4);
        position = *override_position;
        goto emit_simple_effect;
    }
    case KF_EFFECT_KIND_34:
    case KF_EFFECT_KIND_35:
    case KF_EFFECT_KIND_38:
        player_probe_view_target_and_vectors(900, &position, &direction, &distance);
        effect_construct_record(10, KF_EFFECT_USE_PLAYER_MAGIC | KF_EFFECT_TARGET_ACTORS, effect_id,
                                &position, &direction,
                       &player_state.camera_rotation);
        break;
    case KF_EFFECT_KIND_DEFENSE_BOOST:
        effect_construct_record(10, KF_EFFECT_USE_PLAYER_MAGIC, KF_EFFECT_KIND_DEFENSE_BOOST,
                                &player_state.camera_position,
                       &direction);
        player_state.defense_boost_timer = 900;
        player_recalculate_combat_stats();
        break;
    case KF_EFFECT_KIND_ATTACK_BOOST:
        effect_construct_record(10, KF_EFFECT_USE_PLAYER_MAGIC, KF_EFFECT_KIND_ATTACK_BOOST,
                                &player_state.camera_position,
                       &direction);
        player_state.attack_boost_timer = 900;
        player_recalculate_combat_stats();
        break;
    case KF_EFFECT_KIND_14:
        effect_construct_record(10, KF_EFFECT_USE_PLAYER_MAGIC, KF_EFFECT_KIND_14,
                                &player_state.camera_position,
                       &direction);
        break;
    case KF_EFFECT_KIND_16:
        effect_construct_record(10, KF_EFFECT_USE_PLAYER_MAGIC, KF_EFFECT_KIND_16,
                                &player_state.camera_position,
                       &direction);
        break;
    case KF_EFFECT_KIND_19:
        effect_construct_record(10, KF_EFFECT_USE_PLAYER_MAGIC, KF_EFFECT_KIND_19,
                                &player_state.camera_position,
                       &direction);
        break;
    default:
        break;
    }
}




ADDRESS(0x80026ea4, 0x134)
void player_sample_weapon_world_vertex(s32 vertex_index, VECTOR *output)
{
    SVECTOR offset;
    struct KfEulerAngles angles;

    angles.x = player_state.camera_rotation.angles[0]
             - player_state.equipped_weapon_record->rotation_offset_x;
    angles.y = player_state.camera_rotation.angles[1]
             - player_state.equipped_weapon_record->rotation_offset_y;
    angles.z = player_state.camera_rotation.angles[2]
             + player_state.equipped_weapon_record->rotation_offset_z;
    animation_sample_vertex(KF_PLAYER_WEAPON_ASSET_INDEX, player_state.weapon_attack_mode,
                  player_state.weapon_attack_phase, vertex_index, &offset);
    offset.vx -= player_state.equipped_weapon_record->position_offset_x;
    offset.vy += player_state.equipped_weapon_record->position_offset_y;
    offset.vz -= player_state.equipped_weapon_record->position_offset_z;
    vector_rotate_yxz(&angles, &offset, output);
    output->vx += player_state.camera_position.vx;
    output->vz += player_state.camera_position.vz;
    {
        s32 y = output->vy - KF_PLAYER_CAMERA_EYE_OFFSET;
        s32 camera_y = player_state.camera_vertical_offset + player_state.camera_position.vy
                     + player_state.landing_vertical_offset;
        output->vy = y + camera_y;
    }
}

enum { PLAYER_WEAPON_MAGIC_POWER_MINIMUM = 60 };

ADDRESS(0x80026fd8, 0x34)
b32 player_meets_weapon_magic_power_requirement(void)
{
    return player_state.physical_power >= PLAYER_WEAPON_MAGIC_POWER_MINIMUM
        && player_state.magic >= PLAYER_WEAPON_MAGIC_POWER_MINIMUM;
}



enum {
    WEAPON_ATTACK_EVENT_DISABLED_PHASE = 5000,
    WEAPON_EFFECT_HELD_PHASE = 99
};

ADDRESS(0x8002700c, 0x1c4)
void player_dispatch_weapon_magic(KF_ENUM_PARAM(KfEffectKind, s32) magic_id, b32 consume_mp,
                                  s32 effect_parameter)
{
    KfMagicRecord *record = &effect_state.magic_records[KF_ENUM_ENCODE(s32, magic_id)];
    VECTOR position;

    if (player_state.vitals.current_mp < record->mp_cost) {
        return;
    }
    player_state.selected_magic_record = record;
    if (consume_mp) {
        player_state.magic_charge = 0;
        player_state.vitals.current_mp -= record->mp_cost;
    }

    switch (magic_id) {
    case KF_EFFECT_KIND_39:
        player_sample_weapon_world_vertex(player_magic_id_sequence.effect_ids[effect_parameter], &position);
        player_dispatch_magic_effect(magic_id, &position);
        break;
    case KF_EFFECT_KIND_49:
    case KF_EFFECT_KIND_50:
        player_sample_weapon_world_vertex(0, &position);
        player_dispatch_magic_effect(magic_id, &position);
        break;
    case KF_EFFECT_KIND_40:
        effect_parameter <<= 9;
        player_state.magic_origin_offset.vx = rcos(effect_parameter) >> 3;
        player_state.magic_origin_offset.vy = rsin(effect_parameter) >> 3;
        player_state.magic_origin_offset.vz = 600;
        player_dispatch_magic_effect(magic_id);
        break;
    case KF_EFFECT_KIND_38:
        for (effect_parameter = 0; effect_parameter < KF_ANGLE_WRAP_MASK; effect_parameter += 684) {
            player_state.magic_origin_offset.vx = rcos(effect_parameter) >> 3;
            player_state.magic_origin_offset.vy = rsin(effect_parameter) >> 3;
            player_state.magic_origin_offset.vz = 400;
            player_dispatch_magic_effect(magic_id);
        }
        break;
    default:
        player_state.magic_origin_offset.vx = 200;
        player_state.magic_origin_offset.vy = 200;
        player_state.magic_origin_offset.vz = 400;
        player_dispatch_magic_effect(magic_id);
        break;
    }
}

ADDRESS(0x800271d0, 0xbf0)
void player_update_weapon_attack(void)
{
    KF_ENUM_PROMOTED(KfObjectId) weapon_id = player_state.equipped_weapon_id;
    KfWeaponRecordGame *weapon = player_state.equipped_weapon_record;
    s16 phase;
    s32 phase_step;
    s32 phase_end;
    s32 sound_end;
    s32 sound_step;
    s32 hit_step;
    SVECTOR initial_vertex;
    struct KfEulerAngles rotation;
    VECTOR world_position;
    VECTOR step;
    VECTOR damage_position;
    VECTOR last_world;
    KfEffectRecord *effect;
    const VECTOR *damage_origin;
    s32 damage_amount;
    s32 index;
    s32 i;

    switch (weapon_id) {
    case KF_OBJECT_16:
    case KF_OBJECT_17:
        phase = player_state.weapon_attack_phase;
        if (phase == KF_WEAPON_ATTACK_INACTIVE) {
            goto special_idle;
        }
        switch (player_state.weapon_attack_mode) {
        case KF_ANIMATION_CLIP_FIRST:
            if (phase == 0) {
                KF_ENUM_PROMOTED(KfEffectKind) effect_kind;
                KF_ENUM_PROMOTED(KfObjectId) counter;

                switch (weapon_id) {
                case KF_OBJECT_16:
                    effect_kind = KF_EFFECT_KIND_31;
                    counter = KF_OBJECT_117;
                    break;
                case KF_OBJECT_17:
                    effect_kind = KF_EFFECT_KIND_30;
                    counter = KF_OBJECT_118;
                    break;
                default:
                    break;
                }

                if (player_state.equipped_accessory_id == KF_OBJECT_59
                    || player_state.equipped_extra_id == KF_OBJECT_59) {
                    /* Selects the paired ballistic kind 47 or 48. */
                    effect_kind = KF_ENUM_DECODE(KF_ENUM_PROMOTED(KfEffectKind),
                                                 KF_ENUM_ENCODE(s32, effect_kind) + 17);
                }
                if (game_counter_bytes[KF_ENUM_ENCODE(s32, counter)] != 0) {
                    game_counter_bytes[KF_ENUM_ENCODE(s32, counter)]--;
                    player_state.weapon_effect = effect_construct_record(
                        10, KF_EFFECT_USE_PLAYER_MAGIC | KF_EFFECT_TARGET_ACTORS, effect_kind, &player_state.camera_position,
                        NULL, &player_state.camera_rotation);
                    effect = player_state.weapon_effect;
                    if (effect != NULL) {
                        effect->phase = WEAPON_EFFECT_HELD_PHASE;
                    }
                } else {
                    player_state.weapon_effect = NULL;
                }
            }

            player_state.weapon_attack_phase += weapon->attack_phase_step;
            if (player_state.weapon_attack_phase >= KF_ANGLE_WRAP_MASK) {
                player_state.weapon_attack_phase = KF_ANGLE_WRAP_MASK;
            }
            if (player_state.weapon_attack_phase >= weapon->normal_attack_end_phase) {
                if (player_state.attack_charge_current == 0
                    && player_state.equipped_weapon_id == KF_OBJECT_16) {
                    audio_play_sound(3, 110);
                }
                player_state.attack_charge_current =
                    ((player_state.weapon_attack_phase - weapon->normal_attack_end_phase) * KF_PLAYER_CHARGE_FULL)
                    / (KF_ANGLE_WRAP_MASK - weapon->normal_attack_end_phase);
            } else {
                player_state.attack_charge_current = 0;
            }

            effect = player_state.weapon_effect;
            if (effect != NULL) {
                rotation.x = player_state.camera_rotation.angles[0] - weapon->rotation_offset_x;
                rotation.y = player_state.camera_rotation.angles[1] - weapon->rotation_offset_y;
                rotation.z = player_state.camera_rotation.angles[2] + weapon->rotation_offset_z;
                animation_sample_vertex(32, player_state.weapon_attack_mode,
                               player_state.weapon_attack_phase,
                               weapon->initial_vertex_index, &initial_vertex);
                initial_vertex.vx -= weapon->position_offset_x;
                initial_vertex.vy += weapon->position_offset_y;
                initial_vertex.vz -= weapon->position_offset_z;
                vector_rotate_yxz(&rotation, &initial_vertex, &world_position);
                effect->position.vx = player_state.camera_position.vx + world_position.vx;
                effect->position.vz = player_state.camera_position.vz + world_position.vz;
                effect->position.vy = player_state.camera_position.vy + world_position.vy
                                    + player_state.camera_vertical_offset + player_state.landing_vertical_offset
                                    - KF_PLAYER_CAMERA_EYE_OFFSET;

                animation_sample_vertex(32, player_state.weapon_attack_mode,
                               player_state.weapon_attack_phase,
                               weapon->final_vertex_index, &initial_vertex);
                initial_vertex.vx -= weapon->position_offset_x;
                initial_vertex.vy += weapon->position_offset_y;
                initial_vertex.vz -= weapon->position_offset_z;
                vector_rotate_yxz(&rotation, &initial_vertex, &last_world);
                vector_displacement_to_pitch_yaw(world_position.vx - last_world.vx,
                              world_position.vy - last_world.vy,
                              world_position.vz - last_world.vz,
                              (struct KfEulerAngles *)&effect->rotation);
            }
            if ((player_state.pad_buttons.current & DAT_8006e570) != 0) {
                return;
            }
            if (effect != NULL) {
                effect->phase = 0;
                pitch_yaw_to_forward_vector(
                    (const struct KfEulerAngles *)&effect->rotation,
                    &effect->direction);
                vector3s_scale_shift12((player_state.attack_charge_current * 900) / KF_PLAYER_CHARGE_FULL,
                                       &effect->direction);
                effect->updates_remaining = 50;
                effect->cache_tail.payload.ballistic.origin_y = effect->position.vy;
            }
            player_state.weapon_attack_mode = KF_ANIMATION_CLIP_SECOND;
            player_state.weapon_attack_phase = 0;
            return;
        case KF_ANIMATION_CLIP_SECOND:
            player_state.weapon_attack_phase = phase + 400;
            if (player_state.weapon_attack_phase >= KF_ANGLE_WRAP_MASK) {
                player_state.weapon_attack_phase = KF_WEAPON_ATTACK_INACTIVE;
                player_state.attack_charge_current = 0;
            }
            break;
        }
        return;
special_idle:
        player_state.attack_charge_current = 0;
        player_state.weapon_charge_delay = 0;
        return;
    case KF_OBJECT_NONE:
        return;
    }

    if (player_state.weapon_attack_phase != KF_WEAPON_ATTACK_INACTIVE) {
        if (player_state.weapon_attack_mode == KF_ANIMATION_CLIP_FIRST) {
            phase_step = weapon->attack_phase_step;
            phase_end = weapon->normal_attack_end_phase;
            sound_end = weapon->normal_attack_sound_phase;
            hit_step = 0;
            sound_step = 0;
        } else {
            phase_step = weapon->alternate_attack_phase_step;
            phase_end = weapon->alternate_attack_end_phase;
            hit_step = weapon->magic_phase_step;
            sound_end = weapon->alternate_attack_sound_end_phase;
            sound_step = weapon->alternate_attack_sound_phase_step;
        }
        player_state.weapon_attack_phase += phase_step;

        if (player_state.weapon_attack_mode == KF_ANIMATION_CLIP_FIRST
            && weapon->initial_effect_id != KF_MAGIC_NONE
            && player_state.weapon_attack_fully_charged != 0
            && player_meets_weapon_magic_power_requirement()
            && (player_state.pad_buttons.current & DAT_8006e574) != 0) {
            if (player_state.weapon_attack_phase >= weapon->magic_window_start
                && player_state.weapon_attack_phase <= weapon->magic_window_end) {
                if (player_state.weapon_magic_shots_configured != 0) {
                    player_dispatch_weapon_magic(weapon->initial_effect_id,
                                   player_state.weapon_magic_shots_configured == weapon->magic_shots,
                                   player_state.weapon_magic_shots_configured);
                    player_state.weapon_magic_shots_configured--;
                }
            } else {
                player_state.weapon_magic_shots_configured = 0;
            }
        }

        if (player_state.weapon_attack_phase >= player_state.weapon_next_sound_phase
            && player_state.weapon_attack_phase
                 < player_state.weapon_next_sound_phase + phase_step) {
            audio_play_sound(weapon->sound_id, 80);
            if (player_state.weapon_next_sound_phase >= sound_end) {
                player_state.weapon_next_sound_phase = WEAPON_ATTACK_EVENT_DISABLED_PHASE;
            } else {
                player_state.weapon_next_sound_phase += sound_step;
            }
        }

        if (player_state.weapon_attack_phase >= player_state.weapon_attack_window
            && player_state.weapon_attack_phase
                 < player_state.weapon_attack_window + phase_step) {
            player_state.weapon_guard_active = KF_FALSE;
            if (player_state.weapon_attack_mode == KF_ANIMATION_CLIP_SECOND) {
                if (player_state.equipped_weapon_id == KF_OBJECT_13
                    && (player_state.pad_buttons.current & DAT_8006e574) != 0) {
                    player_state.weapon_attack_phase -= phase_step;
                    player_state.weapon_guard_active = KF_TRUE;
                    return;
                }
                if (weapon->release_effect_id != KF_MAGIC_NONE) {
                    player_dispatch_weapon_magic(weapon->release_effect_id,
                                   player_state.weapon_attack_phase >= phase_end,
                                   (player_state.weapon_attack_phase - weapon->alternate_attack_window_start)
                                       / hit_step);
                }
            }

            if (player_state.weapon_attack_phase >= phase_end) {
                s32 coordinate;

                player_state.weapon_attack_window = WEAPON_ATTACK_EVENT_DISABLED_PHASE;
                player_state.weapon_charge_delay = 10;
                damage_amount = player_state.attack_charge_committed;
                damage_origin = &damage_position;
                player_state.attack_charge_current = 0;
                coordinate = player_state.camera_position.vx;
                damage_position.vx = coordinate;
                damage_position.vy = player_state.camera_position.vy - 1000;
                coordinate = player_state.camera_position.vz;
                damage_position.vz = coordinate;
            } else {
                player_state.weapon_attack_window += hit_step;
                damage_origin = NULL;
                damage_amount = player_state.attack_charge_committed >> 2;
            }

            initial_vertex.vx = 0;
            initial_vertex.vy = 0;
            initial_vertex.vz = weapon->attack_angle;
            rotation.x = -player_state.camera_rotation_target.angles[0];
            rotation.y = player_state.camera_rotation_target.angles[1];
            rotation.z = 0;
            vector_rotate_yxz(&rotation, &initial_vertex, &step);
            step.vy = ((3600 - weapon->attack_angle) * step.vy) / 1800;
            step.vx /= 4;
            step.vy /= 4;
            step.vz /= 4;
            world_position.vx = player_state.camera_position.vx;
            world_position.vy = player_state.camera_position.vy - 800;
            world_position.vz = player_state.camera_position.vz;
            for (i = 3; i != -1; i--) {
                world_position.vx += step.vx;
                world_position.vy += step.vy;
                world_position.vz += step.vz;
                index = actor_find_overlap_excluding_target_type3(world_position.vx, world_position.vy,
                                       world_position.vz, 400, 600);
                if (index != KF_ACTOR_INDEX_NONE) {
                    KfActor *actor = &actor_state.actors[index];
                    KfTargetGroup *group = &actor_state.target_groups[actor->group_index];
                    s32 bearing = vector_xz_to_angle(actor->position.vx - player_state.camera_position.vx,
                                                      actor->position.vz - player_state.camera_position.vz);
                    if (angle_within_tolerance(player_state.camera_rotation.angles[1],
                                               bearing, group->player_facing_tolerance)
                        && angle_within_tolerance(actor->rotation.y,
                                                  bearing + 0x800, group->actor_facing_tolerance)) {
                        actor_apply_magic_to_actor(index, player_state.physical_power,
                                       player_state.attack_components[0],
                                       player_state.attack_components[1],
                                       player_state.attack_components[2],
                                       player_state.attack_components[3],
                                       player_state.attack_components[4],
                                       player_state.attack_components[5],
                                       player_state.attack_components[6],
                                       player_state.attack_components[7],
                                       damage_amount,
                                       KF_ACTOR_DAMAGE_FROM_PLAYER | KF_ACTOR_DAMAGE_PHYSICAL, damage_origin);
                        break;
                    }
                }
            }
        }

        if (player_state.weapon_attack_phase > KF_ANGLE_WRAP_MASK) {
            player_state.weapon_attack_phase = KF_WEAPON_ATTACK_INACTIVE;
            player_state.weapon_magic_shots_configured = 0;
        }
        return;
    }
    if ((player_state.pad_buttons.current & DAT_8006e570) == 0) {
        if (player_state.weapon_charge_delay == 0) {
            s32 gain = player_charge_gain_for_rank(player_state.physical_power,
                                      weapon->charge_rank) * 2;
            if (player_state.equipped_leg_id == KF_OBJECT_44) {
                gain >>= 1;
            }
            if ((player_state.equipped_accessory_id == KF_ITEM_PHYSICAL_POWER_BONUS_ACCESSORY
                 || player_state.equipped_extra_id == KF_ITEM_PHYSICAL_POWER_BONUS_ACCESSORY)
                && player_state.equipped_weapon_id == KF_OBJECT_13) {
                gain *= 2;
            }
            player_state.attack_charge_current += gain;
            if (player_state.attack_charge_current > KF_PLAYER_CHARGE_FULL) {
                player_state.attack_charge_current = KF_PLAYER_CHARGE_FULL;
            }
        } else {
            player_state.weapon_charge_delay--;
        }
    }
}




ADDRESS(0x80027dc0, 0x2c0)
void player_select_magic_action(KF_ENUM_PARAM(KfEffectKind, s32) magic_id)
{
    KfMagicRecord *record;
    u16 mp_cost;
    /* Retail reserves an unreferenced 8-byte frame slot. */
    s16 frame_reserve[4];

    if (player_state.queued_magic_action.magic_id != KF_MAGIC_NONE ||
        magic_id == KF_MAGIC_NONE) {
        return;
    }

    record = &effect_state.magic_records[KF_ENUM_ENCODE(s32, magic_id)];
    if (player_state.vitals.current_mp < record->mp_cost) {
        return;
    }

    if (player_state.equipped_weapon_id == KF_OBJECT_12 && magic_id < KF_EFFECT_KIND_11) {
        if (magic_id >= KF_EFFECT_KIND_7) {
            return;
        }
    }
    if (player_state.equipped_body_id == KF_OBJECT_31 && magic_id >= KF_EFFECT_KIND_11) {
        if (magic_id < KF_EFFECT_KIND_13) {
            return;
        }
        if (magic_id < KF_EFFECT_KIND_20) {
            if (magic_id >= KF_EFFECT_KIND_18) {
                return;
            }
        }
    }

    switch (magic_id) {
    case KF_EFFECT_KIND_14:
    case KF_EFFECT_KIND_16:
    case KF_EFFECT_KIND_19:
        break;
    case KF_EFFECT_KIND_DEFENSE_BOOST:
        if (player_state.defense_boost_timer != 0) {
            return;
        }
        break;
    case KF_EFFECT_KIND_ATTACK_BOOST:
        if (player_state.attack_boost_timer != 0) {
            return;
        }
        break;
    case KF_EFFECT_KIND_18:
        player_state.magic_tint_phase_limit = 900;
        player_state.vitals.current_mp -= record->mp_cost;
        return;
    default:
        goto charge_gate;
    }
    player_state.queued_magic_action.casts_remaining = 1;
    player_state.queued_magic_action.repeat_interval = 1;

charge_gate:
    if (player_state.magic_charge < KF_PLAYER_CHARGE_FULL) {
        return;
    }
    player_state.magic_charge = 0;
    mp_cost = record->mp_cost;
    player_state.queued_magic_action.magic_id = magic_id;
    player_state.magic_origin_offset.vx = -200;
    player_state.magic_origin_offset.vy = 200;
    player_state.magic_origin_offset.vz = 400;
    player_state.queued_magic_action.countdown = 1;
    player_state.vitals.current_mp -= mp_cost;

    switch (magic_id) {
    case KF_EFFECT_KIND_10:
        player_state.magic_origin_offset.vx = 0;
        player_state.magic_origin_offset.vy = -512;
        player_state.magic_origin_offset.vz = 2000;
        /* Retail falls through to the shared action-byte stores. */
    case KF_EFFECT_KIND_1:
    case KF_EFFECT_KIND_4:
    case KF_EFFECT_KIND_5:
    case KF_EFFECT_KIND_6:
    case KF_EFFECT_KIND_7:
    case KF_EFFECT_KIND_8:
    case KF_EFFECT_KIND_11:
    case KF_EFFECT_KIND_18:
        player_state.queued_magic_action.casts_remaining = 1;
        player_state.queued_magic_action.repeat_interval = 1;
        break;
    case KF_EFFECT_KIND_12:
        player_state.magic_origin_offset.vx = -200;
        player_state.queued_magic_action.casts_remaining = 5;
        player_state.queued_magic_action.repeat_interval = 2;
        break;
    case KF_EFFECT_KIND_9:
        player_state.queued_magic_action.casts_remaining = 6;
        player_state.queued_magic_action.repeat_interval = 1;
        break;
    case KF_EFFECT_KIND_0:
    case KF_EFFECT_KIND_2:
        player_state.queued_magic_action.casts_remaining = 1;
        player_state.queued_magic_action.repeat_interval = 1;
        player_state.magic_origin_offset.vz = 0;
        player_state.magic_origin_offset.vy = 0;
        player_state.magic_origin_offset.vx = 0;
        break;
    case KF_EFFECT_KIND_13:
        player_state.queued_magic_action.casts_remaining = 7;
        player_state.queued_magic_action.repeat_interval = 1;
        break;
    case KF_EFFECT_KIND_3:
        player_state.queued_magic_action.casts_remaining = 6;
        player_state.queued_magic_action.repeat_interval = 2;
        break;
    default:
        break;
    }

    player_state.selected_magic_record = record;
}

#define PLAYER_MOVE_COLLISION_MODE \
    (KF_COLLISION_QUERY_SHAPES | KF_COLLISION_QUERY_ACTORS | KF_COLLISION_QUERY_MAP_OBJECTS)

enum {
    PLAYER_MOVE_SLIDE_RADIUS = 880,
    PLAYER_MOVE_DEFLECTION_ANGLE = 32,
    PLAYER_MOVE_STEP = 22,
    PLAYER_MOVE_STEP_UP_TOLERANCE = 1280,
    PLAYER_MOVE_COLLISION_RETRY_LIMIT = 2
};

ADDRESS(0x80028080, 0x43c)
b32 player_move_horizontal(s32 heading, s32 distance)
{
    s32 dx = (-rsin(heading) * distance) >> KF_FIXED12_BITS;
    s32 dz = (rcos(heading) * distance) >> KF_FIXED12_BITS;
    s32 initial_dx = dx;
    s32 initial_dz = dz;
    VECTOR next;
    KF_ENUM_PARAM(KfCollisionHitFlags, s32) flags;
    s32 angle;
    s32 radius;
    s32 slide_distance;
    b32 slide_attempted;
    s32 collision_retry;
    b32 result;
    b32 diagonal_retry;
    b32 high_collision;
    SVECTOR delta;
    KF_ENUM_STORAGE(KfQuarterTurn, s32) diagonal_kind;

    result = KF_FALSE;
    collision_retry = 0;
    slide_attempted = KF_FALSE;
    diagonal_retry = KF_FALSE;

retry: {
        next.vx = player_state.camera_position.vx + dx;
        next.vz = player_state.camera_position.vz + dz;
        flags = collision_query_world(next.vx, player_state.camera_position.vy, next.vz,
                              KF_PLAYER_COLLISION_RADIUS, KF_PLAYER_HEIGHT,
                              PLAYER_MOVE_COLLISION_MODE);
        if (flags == KF_COLLISION_HIT_NONE) {
        accept_position:
            player_state.camera_position.vx = next.vx;
            player_state.camera_position.vz = next.vz;
            player_state.map_layer_index = KF_COLLISION_CACHE_LAYER;
            result = KF_TRUE;
            goto done;
        }

        high_collision = KF_FALSE;
        do {
            if ((flags & ~(KF_COLLISION_HIT_AXIS | KF_COLLISION_HIT_FLOOR)) == KF_COLLISION_HIT_NONE) {
                s32 collision_height = KF_COLLISION_CACHE_RESULT;
                high_collision = KF_TRUE;
                if (collision_height + PLAYER_MOVE_STEP_UP_TOLERANCE >= player_state.camera_position.vy
                    && player_state.death_state == KF_PLAYER_REACTION_NORMAL
                    && (KF_COLLISION_CACHE_HEIGHT_LIMIT - collision_height)
                           < -KF_PLAYER_HEIGHT) {
                    goto accept_position;
                }
            }
        } while (0);

        if ((flags & (KF_COLLISION_HIT_ACTOR | KF_COLLISION_HIT_MAP_OBJECT)) != KF_COLLISION_HIT_NONE) {
            collision_retry++;
            if (collision_retry == PLAYER_MOVE_COLLISION_RETRY_LIMIT) {
                goto done;
            }
            collision_cache_load_hit_bounds();
            delta.vx = (u16)KF_COLLISION_CACHE_POSITION.vx
                     - (u16)player_state.camera_position.vx;
            delta.vz = (u16)KF_COLLISION_CACHE_POSITION.vz
                     - (u16)player_state.camera_position.vz;
            angle = vector_xz_to_angle(delta.vx, delta.vz);
            angle = angle_mod_delta_le_half_turn(heading, angle)
                ? angle + (KF_ANGLE_HALF_TURN - PLAYER_MOVE_DEFLECTION_ANGLE)
                : angle + (KF_ANGLE_HALF_TURN + PLAYER_MOVE_DEFLECTION_ANGLE);
            angle &= KF_ANGLE_WRAP_MASK;
            radius = KF_COLLISION_CACHE_RADIUS + PLAYER_MOVE_SLIDE_RADIUS;
            delta.vx = (-rsin(angle) * radius) >> KF_FIXED12_BITS;
            delta.vz = (rcos(angle) * radius) >> KF_FIXED12_BITS;
            next.vx = KF_COLLISION_CACHE_POSITION.vx + delta.vx;
            next.vz = KF_COLLISION_CACHE_POSITION.vz + delta.vz;
            dx = next.vx - player_state.camera_position.vx;
            dz = next.vz - player_state.camera_position.vz;
            goto retry;
        }

        if (!slide_attempted) {
            slide_distance = distance - PLAYER_MOVE_STEP;
            if (slide_distance >= 0) {
                do {
                    next.vx = player_state.camera_position.vx
                           + ((-rsin(heading) * slide_distance) >> KF_FIXED12_BITS);
                    next.vz = player_state.camera_position.vz
                           + ((rcos(heading) * slide_distance) >> KF_FIXED12_BITS);
                    if (collision_query_world(next.vx, player_state.camera_position.vy,
                                       next.vz, KF_PLAYER_COLLISION_RADIUS,
                                       KF_PLAYER_HEIGHT,
                                       PLAYER_MOVE_COLLISION_MODE) == KF_COLLISION_HIT_NONE) {
                        player_state.camera_position.vx = next.vx;
                        player_state.camera_position.vz = next.vz;
                        break;
                    }
                    slide_distance -= PLAYER_MOVE_STEP;
                } while (slide_distance >= 0);
            }
            slide_attempted = KF_TRUE;
        }

        if (high_collision || (flags & KF_COLLISION_HIT_AXIS) != KF_COLLISION_HIT_NONE) {
        axis_retry:
            if (dx != 0) {
                dx = 0;
                goto retry;
            }
            if (dz != 0) {
                dz = 0;
                dx = initial_dx;
                goto retry;
            }
        }
        if ((flags & KF_COLLISION_HIT_DIAGONAL) != KF_COLLISION_HIT_NONE) {
            do {
                if (diagonal_retry) {
                    goto axis_retry;
                }
                diagonal_retry = KF_TRUE;
            } while (0);
            diagonal_kind = KF_ENUM_DECODE(KfQuarterTurn,
                KF_COLLISION_CACHE_SHAPE->quarter_turns & KF_MAP_CELL_QUARTER_TURN_MASK);
            if (diagonal_kind == KF_QUARTER_TURN_0 || diagonal_kind == KF_QUARTER_TURN_2) {
                dx = (initial_dx + initial_dz) >> 1;
                dz = dx;
            } else {
                dx = (initial_dx - initial_dz) >> 1;
                dz = -dx;
            }
            goto retry;
        }
        result = KF_FALSE;
    }
done:
    player_state.frame_displacement.vx = dx;
    player_state.frame_displacement.vz = dz;
    return result;
}

#define PLAYER_MOTION_COLLISION_MASK \
    (KF_COLLISION_QUERY_SHAPES | KF_COLLISION_QUERY_ACTORS | KF_COLLISION_QUERY_MAP_OBJECTS)

enum {
    COLLISION_DEPTH_ARM_HEIGHT = 200,
    COLLISION_DEPTH_DEATH_LIMIT = 32000,
    PLAYER_LANDING_SOUND_ID = 12,
    PLAYER_LANDING_SOUND_MIN_MAGNITUDE = 320,
    PLAYER_LANDING_SOUND_MAX_EXCESS = 896,
    PLAYER_LANDING_SOUND_BASE_VOLUME = 32
};

ADDRESS(0x800284bc, 0x60)
void player_check_fall_death(void)
{
    if (player_state.vertical_velocity >= COLLISION_DEPTH_ARM_HEIGHT
        && (KF_COLLISION_CACHE_RESULT - player_state.camera_position.vy)
               > COLLISION_DEPTH_DEATH_LIMIT) {
        player_death_begin(NULL);
        player_state.fatal_fall_latch = KF_TRUE;
    }
}

ADDRESS(0x8002851c, 0x44)
void player_play_landing_sound(s32 magnitude)
{
    s32 volume = magnitude;

    if (volume >= PLAYER_LANDING_SOUND_MIN_MAGNITUDE) {
        volume -= PLAYER_LANDING_SOUND_MIN_MAGNITUDE;
        if (volume > PLAYER_LANDING_SOUND_MAX_EXCESS) {
            volume = PLAYER_LANDING_SOUND_MAX_EXCESS;
        }
        audio_play_sound(PLAYER_LANDING_SOUND_ID,
                         (volume >> 3) + PLAYER_LANDING_SOUND_BASE_VOLUME);
    }
}

ADDRESS(0x80028560, 0x5ac)
void player_update_vertical_motion(void)
{
    s32 next_y;
    s32 height_difference;
    KF_ENUM_PARAM(KfCollisionHitFlags, s32) collision_flags;
    s32 impact;
    s32 bob;
    s32 movement_speed;
    const s32 *floor_result;

    collision_probe_floor_height(player_state.camera_position.vx,
                  player_state.camera_position.vy,
                  player_state.camera_position.vz, KF_PLAYER_COLLISION_RADIUS, KF_PLAYER_HEIGHT);
    player_state.frame_displacement.vy = 0;

    switch (player_state.vertical_motion_state) {
    case KF_PLAYER_VERTICAL_GROUNDED:
        break;

    case KF_PLAYER_VERTICAL_FALLING:
        player_check_fall_death();
        player_state.camera_position.vy += player_state.vertical_velocity;
        player_state.frame_displacement.vy = player_state.vertical_velocity;
        player_state.vertical_velocity += 40;
        if (KF_COLLISION_CACHE_RESULT + 100 < player_state.camera_position.vy) {
            player_state.camera_position.vy = KF_COLLISION_CACHE_RESULT;
            player_state.vertical_motion_state = KF_PLAYER_VERTICAL_GROUNDED;
        }
        goto finish;

    case KF_PLAYER_VERTICAL_STEP_UP:
        player_check_fall_death();
        player_state.camera_position.vy += player_state.vertical_velocity;
        if (player_state.camera_position.vy <= KF_COLLISION_CACHE_RESULT
            || player_state.vertical_velocity >= 0) {
            if (KF_COLLISION_CACHE_RESULT
                < player_state.camera_position.vy - player_state.vertical_velocity) {
                player_state.camera_position.vy = KF_COLLISION_CACHE_RESULT;
            }
            player_state.vertical_motion_state = KF_PLAYER_VERTICAL_GROUNDED;
        }
        player_state.frame_displacement.vy = player_state.vertical_velocity;
        player_state.vertical_velocity += 5;
        goto finish;

    case KF_PLAYER_VERTICAL_DEEP_FALL:
        player_check_fall_death();
        next_y = player_state.camera_position.vy + player_state.vertical_velocity;
        collision_flags = collision_query_world(player_state.camera_position.vx, next_y,
                                         player_state.camera_position.vz, KF_PLAYER_COLLISION_RADIUS,
                                         KF_PLAYER_HEIGHT, PLAYER_MOTION_COLLISION_MASK);
        if (collision_flags == KF_COLLISION_HIT_NONE) {
            player_state.frame_displacement.vy = player_state.vertical_velocity;
            player_state.vertical_velocity += 40;
            player_state.vertical_motion_pitch_offset = player_state.vertical_velocity >> 1;
            player_state.camera_position.vy = next_y;
            goto finish;
        }
        if (player_state.vertical_velocity < 0) {
            player_state.vertical_velocity = 0;
            goto finish;
        }
        player_play_landing_sound(player_state.vertical_velocity);
        if (player_state.vertical_velocity >= 480) {
            impact = (player_state.vertical_velocity * player_state.vertical_velocity) >> 12;
            player_apply_damage_reaction(NULL, (impact * impact * impact) / 0x1ccf0, 0);
        }
        if ((collision_flags & KF_COLLISION_HIT_FLOOR) != KF_COLLISION_HIT_NONE) {
            player_state.camera_position.vy = KF_COLLISION_CACHE_RESULT;
        } else {
            collision_cache_load_hit_bounds();
            next_y = KF_COLLISION_CACHE_POSITION.vy
                   - KF_COLLISION_CACHE_INTERACTION_HEIGHT - 1;
            collision_flags = collision_query_world(player_state.camera_position.vx, next_y,
                               player_state.camera_position.vz, KF_PLAYER_COLLISION_RADIUS,
                               KF_PLAYER_HEIGHT, PLAYER_MOTION_COLLISION_MASK);
            if (collision_flags == KF_COLLISION_HIT_NONE) {
                player_state.camera_position.vy = next_y;
            }
        }
        player_state.landing_vertical_offset = 1;
        player_state.vertical_motion_state = KF_PLAYER_VERTICAL_LANDING;
        /* Fall through: the landing response starts this frame. */
    case KF_PLAYER_VERTICAL_LANDING:
        if (player_state.landing_vertical_offset > 0) {
            player_state.landing_vertical_offset += player_state.vertical_velocity >> 2;
        }
        player_state.vertical_velocity -= 100;
        if (player_state.vertical_motion_pitch_offset > 0) {
            if (player_state.vertical_velocity > 0) {
                player_state.vertical_motion_pitch_offset += 10;
            } else {
                player_state.vertical_motion_pitch_offset -= 30;
            }
        }
        if (player_state.landing_vertical_offset <= 0
            && player_state.vertical_motion_pitch_offset <= 0) {
            player_state.vertical_motion_state = KF_PLAYER_VERTICAL_GROUNDED;
            player_state.vertical_velocity = 0;
            player_state.landing_vertical_offset = 0;
            player_state.vertical_motion_pitch_offset = 0;
        }
        break;

    default:
        goto finish;
    }

    floor_result = &KF_COLLISION_CACHE_RESULT;
    height_difference = *floor_result - player_state.camera_position.vy;
    if (height_difference < 0) {
        if (height_difference >= -256) {
            if (height_difference < -128) {
                player_state.camera_position.vy -= 128;
            } else {
                player_state.camera_position.vy = *floor_result;
            }
            goto finish;
        }
        if (height_difference >= -512) {
            player_state.camera_position.vy -= 256;
            goto finish;
        }
        movement_speed = player_state.movement_speed.signed_value;
        player_state.vertical_motion_state = KF_PLAYER_VERTICAL_STEP_UP;
        player_state.vertical_velocity = movement_speed > 200 ? -300 : -150;
    } else {
        if (height_difference <= 0) {
            goto finish;
        }
        collision_flags = collision_query_world(player_state.camera_position.vx,
                           player_state.camera_position.vy + 1,
                           player_state.camera_position.vz, KF_PLAYER_COLLISION_RADIUS,
                           KF_PLAYER_HEIGHT, PLAYER_MOTION_COLLISION_MASK);
        if (collision_flags != KF_COLLISION_HIT_NONE) {
            goto finish;
        }
        if (height_difference <= 256) {
            if (height_difference >= 129) {
                player_state.camera_position.vy += 128;
            } else {
                player_state.camera_position.vy = *floor_result;
            }
            goto finish;
        }
        if (height_difference <= 512) {
            player_state.camera_position.vy += 256;
            goto finish;
        }
        player_state.vertical_motion_state = height_difference > 1024
            ? KF_PLAYER_VERTICAL_DEEP_FALL : KF_PLAYER_VERTICAL_FALLING;
        player_state.vertical_velocity = 40;
    }
    player_state.landing_vertical_offset = 0;
    player_state.vertical_motion_pitch_offset = 0;

finish:
    if (player_state.vertical_motion_state == KF_PLAYER_VERTICAL_GROUNDED) {
        if (player_state.walking_bob_enabled != KF_PLAYER_OPTION_OFF) {
            player_state.walking_bob_phase =
                (player_state.walking_bob_phase + player_state.movement_speed.unsigned_value)
                & KF_ANGLE_WRAP_MASK;
            bob = rsin(player_state.walking_bob_phase) >> 5;
            if (bob < 0) {
                bob = -bob;
            }
            player_state.camera_vertical_offset = bob - (bob >> 2);
        } else {
            player_state.camera_vertical_offset = 0;
        }
    }
    player_update_collision_bounds();
}

ADDRESS(0x80028b0c, 0x2ac)
b32 player_move_reaction_with_collision(void)
{
    VECTOR next;
    KF_ENUM_PARAM(KfCollisionHitFlags, s32) flags;
    s32 length;
    s32 remaining;
    s32 minimum_length;

    next.vx = player_state.camera_position.vx + player_state.reaction.damage.rotation.vx;
    next.vy = player_state.camera_position.vy + player_state.reaction.damage.rotation.vy;
    next.vz = player_state.camera_position.vz + player_state.reaction.damage.rotation.vz;

    flags = collision_query_world(next.vx, next.vy, next.vz,
                                  KF_PLAYER_COLLISION_RADIUS, KF_PLAYER_HEIGHT,
                                  PLAYER_MOTION_COLLISION_MASK);
    if (flags == KF_COLLISION_HIT_NONE) {
    accept:
        if (player_state.reaction.damage.rotation.vy >= 160
            && KF_COLLISION_CACHE_RESULT - player_state.camera_position.vy
                   > COLLISION_DEPTH_DEATH_LIMIT) {
            player_death_begin(NULL);
            player_state.fatal_fall_latch = KF_TRUE;
        }
        player_state.camera_position.vx = next.vx;
        player_state.camera_position.vy = next.vy;
        player_state.camera_position.vz = next.vz;
        player_state.reaction.damage.rotation.vy += 32;
        goto accepted;
    }

    next.vy = player_state.camera_position.vy;
    flags = collision_query_world(next.vx, next.vy, next.vz,
                                  KF_PLAYER_COLLISION_RADIUS, KF_PLAYER_HEIGHT,
                                  PLAYER_MOTION_COLLISION_MASK);
    if (flags == KF_COLLISION_HIT_NONE) {
        player_state.reaction.damage.rotation.vy = 1;
        flags = collision_query_world(next.vx, next.vy, next.vz,
                                      KF_PLAYER_COLLISION_RADIUS, KF_PLAYER_HEIGHT,
                                      PLAYER_MOTION_COLLISION_MASK);
        if (flags == KF_COLLISION_HIT_NONE) {
            minimum_length = 32;
        scale_motion:
            length = fixed_vector2_length(player_state.reaction.damage.rotation.vx,
                                          player_state.reaction.damage.rotation.vz);
            if (length <= minimum_length) {
                player_state.reaction.damage.rotation.vz = 0;
                player_state.reaction.damage.rotation.vx = 0;
                goto exhausted;
            }
            remaining = length - minimum_length;
            player_state.reaction.damage.rotation.vx =
                (player_state.reaction.damage.rotation.vx * remaining) / length;
            player_state.reaction.damage.rotation.vz =
                (player_state.reaction.damage.rotation.vz * remaining) / length;
            goto accept;
        }
    }

    if ((flags & ~(KF_COLLISION_HIT_AXIS | KF_COLLISION_HIT_FLOOR)) != KF_COLLISION_HIT_NONE) {
        return KF_TRUE;
    }
    if (KF_COLLISION_CACHE_RESULT + 256 < player_state.camera_position.vy) {
        return KF_TRUE;
    }
    next.vy = KF_COLLISION_CACHE_RESULT;
    minimum_length = 56;
    goto scale_motion;

exhausted:
    return KF_TRUE;

accepted:
    player_update_collision_bounds();
    return KF_FALSE;
}

enum {
    PLAYER_YAW_ACCEL_SHIFT = 2,
    PLAYER_PITCH_STEP = 3,
    PLAYER_PITCH_STEP_LIMIT = 32,
    PLAYER_CAMERA_PITCH_LIMIT = 700
};

ADDRESS(0x80028db8, 0x31c)
void player_update_camera_rotation(void)
{
    if (player_state.pad_buttons.current & DAT_8006e59c) {
        player_state.yaw_step += player_state.turn_step_limit >> PLAYER_YAW_ACCEL_SHIFT;
        if (player_state.yaw_step > player_state.turn_step_limit) {
            player_state.yaw_step = player_state.turn_step_limit;
        }
    } else if (player_state.pad_buttons.current & DAT_8006e598) {
        player_state.yaw_step -= player_state.turn_step_limit >> PLAYER_YAW_ACCEL_SHIFT;
        if (player_state.yaw_step < -player_state.turn_step_limit) {
            player_state.yaw_step = -player_state.turn_step_limit;
        }
    } else if (player_state.yaw_step > 0) {
        player_state.yaw_step -= player_state.turn_step_limit >> PLAYER_YAW_ACCEL_SHIFT;
        if (player_state.yaw_step < 0) {
            player_state.yaw_step = 0;
        }
    } else if (player_state.yaw_step < 0) {
        player_state.yaw_step += player_state.turn_step_limit >> PLAYER_YAW_ACCEL_SHIFT;
        if (player_state.yaw_step > 0) {
            player_state.yaw_step = 0;
        }
    }

    player_state.camera_rotation_target.angles[1] =
        (player_state.camera_rotation_target.angles[1] + player_state.yaw_step)
        & KF_ANGLE_WRAP_MASK;

    if (player_state.pad_buttons.current & DAT_8006e584) {
        player_state.pitch_step += PLAYER_PITCH_STEP;
        if (player_state.pitch_step > PLAYER_PITCH_STEP_LIMIT) {
            player_state.pitch_step = PLAYER_PITCH_STEP_LIMIT;
        }
    } else if (player_state.pad_buttons.current & DAT_8006e58c) {
        player_state.pitch_step -= PLAYER_PITCH_STEP;
        if (player_state.pitch_step < -PLAYER_PITCH_STEP_LIMIT) {
            player_state.pitch_step = -PLAYER_PITCH_STEP_LIMIT;
        }
    } else if (player_state.pitch_step > 0) {
        player_state.pitch_step -= PLAYER_PITCH_STEP;
        if (player_state.pitch_step < 0) {
            player_state.pitch_step = 0;
        }
    } else if (player_state.pitch_step < 0) {
        player_state.pitch_step += PLAYER_PITCH_STEP;
        if (player_state.pitch_step > 0) {
            player_state.pitch_step = 0;
        }
    }

    if (player_state.pitch_step > 0) {
        if (angle_mod_delta_le_half_turn(
                (player_state.camera_rotation_target.angles[0] =
                    (player_state.camera_rotation_target.angles[0] + player_state.pitch_step)
                    & KF_ANGLE_WRAP_MASK),
                PLAYER_CAMERA_PITCH_LIMIT)) {
            player_state.camera_rotation_target.angles[0] = PLAYER_CAMERA_PITCH_LIMIT;
        }
    } else if (player_state.pitch_step < 0) {
        if (angle_mod_delta_le_half_turn(
                -PLAYER_CAMERA_PITCH_LIMIT,
                (player_state.camera_rotation_target.angles[0] =
                    (player_state.camera_rotation_target.angles[0] + player_state.pitch_step)
                    & KF_ANGLE_WRAP_MASK))) {
            player_state.camera_rotation_target.angles[0] = -PLAYER_CAMERA_PITCH_LIMIT;
        }
    }
}

ADDRESS(0x800290d4, 0x48c)
void player_update_horizontal_motion(void)
{
    s16 forward;
    s16 strafe;
    s32 forward_square;
    s32 strafe_square;
    s16 magnitude;

    if (player_state.pad_buttons.current & DAT_8006e590) {
        forward = player_state.forward_velocity + (player_state.movement_step_limit >> 2);
        if (forward > player_state.movement_step_limit) {
            player_state.forward_velocity = player_state.movement_step_limit;
        } else {
            player_state.forward_velocity = forward;
        }
    } else if (player_state.pad_buttons.current & DAT_8006e594) {
        forward = player_state.forward_velocity - (player_state.movement_step_limit >> 2);
        if (forward >= -player_state.movement_step_limit) {
            player_state.forward_velocity = forward;
        } else {
            player_state.forward_velocity = -player_state.movement_step_limit;
        }
    } else if (player_state.forward_velocity > 0) {
        player_state.forward_velocity -= player_state.movement_step_limit >> 3;
        if (player_state.forward_velocity < 0) {
            player_state.forward_velocity = 0;
        }
    } else if (player_state.forward_velocity < 0) {
        player_state.forward_velocity += player_state.movement_step_limit >> 3;
        if (player_state.forward_velocity > 0) {
            player_state.forward_velocity = 0;
        }
    }

    if (player_state.pad_buttons.current & DAT_8006e580) {
        strafe = player_state.strafe_velocity + (player_state.movement_step_limit >> 2);
        if (strafe > player_state.movement_step_limit) {
            player_state.strafe_velocity = player_state.movement_step_limit;
        } else {
            player_state.strafe_velocity = strafe;
        }
    } else if (player_state.pad_buttons.current & DAT_8006e588) {
        strafe = player_state.strafe_velocity - (player_state.movement_step_limit >> 2);
        if (strafe >= -player_state.movement_step_limit) {
            player_state.strafe_velocity = strafe;
        } else {
            player_state.strafe_velocity = -player_state.movement_step_limit;
        }
    } else if (player_state.strafe_velocity > 0) {
        player_state.strafe_velocity -= player_state.movement_step_limit >> 2;
        if (player_state.strafe_velocity < 0) {
            player_state.strafe_velocity = 0;
        }
    } else if (player_state.strafe_velocity < 0) {
        player_state.strafe_velocity += player_state.movement_step_limit >> 2;
        if (player_state.strafe_velocity > 0) {
            player_state.strafe_velocity = 0;
        }
    }

    strafe_square = player_state.strafe_velocity;
    strafe_square *= strafe_square;
    forward_square = player_state.forward_velocity;
    forward_square *= forward_square;
    magnitude = SquareRoot0(strafe_square + forward_square);
    if (magnitude == 0) {
        forward = 0;
        strafe = 0;
    } else {
        strafe = strafe_square / magnitude;
        if (player_state.strafe_velocity < 0) {
            strafe = -(strafe_square / magnitude);
        }
        forward = forward_square / magnitude;
        if (player_state.forward_velocity < 0) {
            forward = -(forward_square / magnitude);
        }
    }

    player_state.movement_speed.unsigned_value = SquareRoot0(strafe * strafe + forward * forward);
    if (forward >= 0) {
        player_move_horizontal(player_state.camera_rotation_target.angles[1], forward);
    } else {
        player_move_horizontal(
            (player_state.camera_rotation_target.angles[1] + KF_ANGLE_HALF_TURN)
                & KF_ANGLE_WRAP_MASK,
            -forward);
    }
    if (strafe > 0) {
        player_move_horizontal(
            (player_state.camera_rotation_target.angles[1] - KF_ANGLE_QUARTER_TURN)
                & KF_ANGLE_WRAP_MASK,
            strafe);
    } else if (strafe < 0) {
        player_move_horizontal(
            (player_state.camera_rotation_target.angles[1] + KF_ANGLE_QUARTER_TURN)
                & KF_ANGLE_WRAP_MASK,
            -strafe);
    } else {
        player_state.frame_displacement.vz = 0;
        player_state.frame_displacement.vx = 0;
    }
}


ADDRESS(0x80029560, 0x1c)
b32 item_id_is_71_to_80(KF_ENUM_PARAM(KfObjectId, s32) value)
{
    if (value < KF_OBJECT_81) {
        if (value >= KF_ITEM_RELIEVE_AILMENTS) {
            return KF_TRUE;
        }
    }
    return KF_FALSE;
}

ADDRESS(0x8002957c, 0x5ec)
void player_update_actions_and_charge(void)
{
    const u16 *attack_mask;
    s32 charge_gain;
    u8 timer;

    player_magic_attack_masks[3] = 0xffff;
    player_magic_attack_masks[0] = DAT_8006e568;
    player_magic_attack_masks[1] = DAT_8006e570;
    player_magic_attack_masks[2] = DAT_8006e574;

    if (player_state.weapon_magic_shots_configured == 0
        && (player_state.pad_buttons.current & DAT_8006e574)
        && !(player_state.pad_buttons.halves.previous & DAT_8006e574)
        && player_state.weapon_magic_shots_remaining == 0) {
        player_select_magic_action(player_state.primary_magic_shortcut_id);
    }

    if ((player_state.pad_buttons.current & DAT_8006e578)
        && !(player_state.pad_buttons.halves.previous & DAT_8006e578)) {
        if (player_state.secondary_magic_shortcut_id != KF_MAGIC_NONE) {
            player_select_magic_action(player_state.secondary_magic_shortcut_id);
        }
        if (player_state.secondary_item_shortcut_id != KF_OBJECT_NONE) {
            if (game_counter_bytes[KF_ENUM_ENCODE(u8, player_state.secondary_item_shortcut_id)] != 0) {
                if (item_id_is_71_to_80(player_state.secondary_item_shortcut_id) != 0) {
                    menu_apply_item_effect(player_state.secondary_item_shortcut_id);
                } else {
                    event_scene_command_dispatch(&player_state.camera_position,
                                  &player_state.camera_rotation_target,
                                  player_state.secondary_item_shortcut_id);
                }
            } else {
                notify_enqueue(KF_NOTIFICATION_NOTHING_HAPPENS);
            }
        }
    }

    if (player_state.queued_magic_action.magic_id != KF_MAGIC_NONE) {
        timer = player_state.queued_magic_action.countdown - 1;
        player_state.queued_magic_action.countdown = timer;
        if (timer == 0) {
            player_dispatch_magic_effect(player_state.queued_magic_action.magic_id);
            timer = player_state.queued_magic_action.casts_remaining - 1;
            player_state.queued_magic_action.casts_remaining = timer;
            if (timer == 0) {
                player_state.queued_magic_action.magic_id = KF_MAGIC_NONE;
            } else {
                player_state.queued_magic_action.countdown = player_state.queued_magic_action.repeat_interval;
            }
        }
    }

    if ((player_state.pad_buttons.current & DAT_8006e568)
        && (player_state.pad_buttons.halves.previous & DAT_8006e568)
        && player_state.equipped_shield_id != KF_OBJECT_50) {
        if (player_state.movement_speed_adjustment_decay_latch != 0) {
            player_state.movement_speed_adjustment_decay_latch--;
        }
        player_state.weapon_charge_delay = 1;
        player_state.attack_charge_current -= 500;
        if ((s16)player_state.attack_charge_current <= 0) {
            player_state.attack_charge_current = 0;
            player_state.weapon_charge_delay = 40;
        }
        player_state.magic_charge -= 500;
        if ((s16)player_state.magic_charge <= 0) {
            player_state.magic_charge = 0;
        }
        player_state.damage_scale -= 128;
        if (player_state.damage_scale < 1024) {
            player_state.damage_scale = 1024;
        }
        return;
    }

    if (player_state.selected_magic_record == NULL) {
        player_state.magic_charge += player_charge_gain_for_rank(player_state.magic, 0);
    } else {
        charge_gain = player_charge_gain_for_rank(player_state.magic,
                                    player_state.selected_magic_record->charge_rate);
        if (player_state.equipped_head_id == KF_OBJECT_24) {
            charge_gain >>= 1;
        }
        player_state.magic_charge += charge_gain;
    }
    if (player_state.magic_charge > KF_PLAYER_CHARGE_FULL) {
        player_state.magic_charge = KF_PLAYER_CHARGE_FULL;
    }
    player_state.damage_scale += 128;
    if (player_state.damage_scale > 4096) {
        player_state.damage_scale = 4096;
    }
    player_state.movement_speed_adjustment_decay_latch = 1;

    if (player_state.weapon_magic_shots_remaining != 0) {
        player_state.weapon_magic_shots_remaining--;
    } else {
        player_state.magic_attack_mask_cursor = &player_magic_attack_masks[0];
    }

    if ((player_state.pad_buttons.current & (DAT_8006e568 | DAT_8006e570 | DAT_8006e574)) != 0
        && (player_state.pad_buttons.halves.previous & (DAT_8006e568 | DAT_8006e570 | DAT_8006e574)) == 0
        && player_state.equipped_weapon_record->alternate_attack_phase_step != 0) {
        if (!player_meets_weapon_magic_power_requirement()) {
            goto cancel_weapon_attack;
        }
        attack_mask = player_state.magic_attack_mask_cursor;
        if ((player_state.pad_buttons.current & attack_mask[0]) == 0) {
            goto cancel_weapon_attack;
        }
        if (attack_mask == &player_magic_attack_masks[0]
            && (player_state.attack_charge_current != KF_PLAYER_CHARGE_FULL
                || player_state.magic_charge != KF_PLAYER_CHARGE_FULL)) {
            goto cancel_weapon_attack;
        }
        player_state.magic_attack_mask_cursor = attack_mask + 1;
        if (attack_mask[1] == 0xffff) {
            player_begin_weapon_attack(KF_ANIMATION_CLIP_SECOND);
            return;
        }
        player_state.weapon_magic_shots_remaining = 20;
        goto after_weapon_attack;
cancel_weapon_attack:
        player_state.weapon_magic_shots_remaining = 0;
    }

after_weapon_attack:
    if (player_state.weapon_charge_delay == 0
        && player_state.weapon_magic_shots_remaining == 0
        && (player_state.pad_buttons.current & DAT_8006e570)
        && !(player_state.pad_buttons.halves.previous & DAT_8006e570)) {
        player_begin_weapon_attack(KF_ANIMATION_CLIP_FIRST);
    }
}

ADDRESS(0x80029b68, 0xe8)
void player_update_reaction_rotation_offsets(void)
{
    player_state.reaction.damage.motion.vx = angle_velocity_step(
        0, player_state.reaction_rotation_offset[0], player_state.reaction.damage.motion.vx, 8, 4);
    player_state.reaction.damage.motion.vy = angle_velocity_step(
        0, player_state.reaction_rotation_offset[1], player_state.reaction.damage.motion.vy, 8, 4);
    player_state.reaction.damage.motion.vz = angle_velocity_step(
        0, player_state.reaction_rotation_offset[2], player_state.reaction.damage.motion.vz, 8, 4);

    player_state.reaction_rotation_offset[0] += player_state.reaction.damage.motion.vx;
    player_state.reaction_rotation_offset[1] += player_state.reaction.damage.motion.vy;
    player_state.reaction_rotation_offset[2] += player_state.reaction.damage.motion.vz;
}

ADDRESS(0x80029c50, 0x6c)
void player_render_frame_and_release_pool(void)
{
    KfPlayerOption saved_hud_gauges = player_state.hud_gauges_enabled;
    KfPlayerOption saved_compass = player_state.compass_enabled;

    player_state.hud_gauges_enabled = KF_PLAYER_OPTION_OFF;
    player_state.compass_enabled = KF_PLAYER_OPTION_OFF;
    render_game_frame(NULL, NULL);
    player_state.hud_gauges_enabled = saved_hud_gauges;
    player_state.compass_enabled = saved_compass;
    pool_release_all();
}

ADDRESS(0x80029cbc, 0x1a0)
void player_handle_interaction_and_menu(void)
{
    s32 value;

    if ((player_state.pad_buttons.current & DAT_8006e568)
        && !(player_state.pad_buttons.halves.previous & DAT_8006e568)) {
        if (player_state.death_state == KF_PLAYER_REACTION_MAP_OBJECT_FOLLOW) {
            player_apply_map_object_reaction(
                &map_object_state.objects[player_state.reaction.view.map_object_index]);
        } else {
            event_world_dispatch_interaction(&player_state.camera_position,
                                             &player_state.camera_rotation_target);
        }
    }
    if (!((player_state.pad_buttons.current & DAT_8006e56c)
          && !(player_state.pad_buttons.halves.previous & DAT_8006e56c))
        || player_state.weapon_attack_phase != -1) {
        return;
    }

    player_render_frame_and_release_pool();
    value = menu_run_root_controller();
    if (value >= 0) {
        if (item_id_is_71_to_80(KF_ENUM_DECODE(KF_ENUM_PROMOTED(KfObjectId), value)) == 0) {
            render_game_frame(NULL, NULL);
            event_scene_command_dispatch(&player_state.camera_position,
                                         &player_state.camera_rotation_target,
                                         KF_ENUM_DECODE(KF_ENUM_PROMOTED(KfObjectId), value));
        }
    } else if (value == KF_MENU_RESULT_QUIT_GAME) {
        game_main_exit_flag = GAME_MAIN_EXIT_ALTERNATE;
    } else if (value == KF_MENU_RESULT_GAME_LOADED) {
        s32 resource;
        player_restore_equipment_effects();
        resource = resource_state.active_resource_ids[0];
        player_reload_map_resources(resource, resource, resource, resource, resource,
                                    KF_RESOURCE_REQUEST_KEEP);
    }
    player_clear_motion();
}

ADDRESS(0x80029e5c, 0x68)
void player_reset_reaction_state(void)
{
    player_state.death_state = KF_PLAYER_REACTION_NORMAL;
    player_state.forward_velocity = 0;
    player_state.strafe_velocity = 0;
    player_state.reaction_rotation_offset[2] = 0;
    player_state.reaction_rotation_offset[1] = 0;
    player_state.reaction_rotation_offset[0] = 0;
    player_state.view_rotation_offset.components[2] = 0;
    player_state.view_rotation_offset.components[1] = 0;
    player_state.view_rotation_offset.components[0] = 0;
    player_state.camera_yaw_roll_offsets[1] = 0;
    player_state.camera_yaw_roll_offsets[0] = 0;
    player_state.vertical_motion_pitch_offset = 0;
}

ADDRESS(0x80029ec4, 0x1c)
void player_begin_map_object_view_follow(u8 map_object_index)
{
    player_state.death_state = KF_PLAYER_REACTION_MAP_OBJECT_FOLLOW;
    player_state.reaction.view.map_object_index = map_object_index;
}

ADDRESS(0x80029ee0, 0x1e8)
void player_apply_map_object_reaction(KfMapObject *object)
{
    KfMapObjectRecord40 *record;

    if (player_state.death_state != KF_PLAYER_REACTION_MAP_OBJECT_FOLLOW) {
        return;
    }
    if (player_state.reaction.view.map_object_index != object - map_object_state.objects) {
        return;
    }

    record = object->extra_40.record;
    player_state.camera_rotation_target.angles[0] += player_state.view_rotation_offset.components[0];
    player_state.camera_rotation_target.angles[1] += player_state.view_rotation_offset.components[1];
    player_state.camera_rotation_target.angles[2] += player_state.view_rotation_offset.components[2];
    player_state.view_rotation_offset.components[0] = 0;
    player_state.view_rotation_offset.components[1] = 0;
    player_state.view_rotation_offset.components[2] = 0;

    if (record->reaction_mode == 0) {
        SVECTOR rotation;
        rotation.vx = (record->reaction_rotation_vector.vx * record->reaction_rotation_scale_q15) >> 15;
        rotation.vy = ((record->reaction_rotation_vector.vy * record->reaction_rotation_scale_q15) >> 15) + 512;
        rotation.vz = (record->reaction_rotation_vector.vz * record->reaction_rotation_scale_q15) >> 15;
        player_begin_rotation_reaction(&rotation);
    } else {
        struct KfVecXZi offset;
        angle_to_forward_xz(object->rotation.vy + KF_ANGLE_QUARTER_TURN, &offset);
        vector2i_scale_shift11(900, &offset);
        player_state.death_state = KF_PLAYER_REACTION_POSITION_RECOVERY;
        player_state.reaction.position.recovery_step = 0;
        player_state.reaction.position.position.vx = object->position.vx + offset.x;
        player_state.reaction.position.position.vz = object->position.vz + offset.z;
        player_state.reaction.position.position.vy = object->position.vy;
    }
}

ADDRESS(0x8002a0c8, 0x54)
void player_begin_view_reaction(u8 map_object_index)
{
    player_state.death_state = KF_PLAYER_REACTION_MAP_OBJECT_APPROACH;
    player_state.reaction.view.map_object_index = map_object_index;
    player_state.reaction.view.approach_step = 0;
    player_state.reaction.view.rotation = player_state.camera_rotation_target;
}

ADDRESS(0x8002a11c, 0x3c)
void player_begin_rotation_reaction(const SVECTOR *rotation)
{
    player_state.death_state = KF_PLAYER_REACTION_ROTATION;
    player_state.reaction.damage.rotation = *rotation;
}

ADDRESS(0x8002a158, 0x94)
void player_begin_moving_damage_reaction(const SVECTOR *rotation, const SVECTOR *motion, s16 duration)
{
    player_state.death_state = KF_PLAYER_REACTION_MOVING_DAMAGE;
    player_state.reaction.damage.rotation = *rotation;
    player_state.reaction.damage.motion = *motion;
    player_state.damage_red_overlay_scale = 3500;
    player_state.damage_red_overlay_decay = duration;
    player_state.vertical_motion_state = KF_PLAYER_VERTICAL_DEEP_FALL;
    player_state.vertical_velocity = player_state.reaction.damage.rotation.vy;
}

ADDRESS(0x8002a1ec, 0x78)
void player_begin_rotation_only_damage_reaction(const SVECTOR *rotation, const SVECTOR *motion, s16 duration)
{
    player_state.death_state = KF_PLAYER_REACTION_ROTATION_DAMAGE;
    player_state.reaction.damage.rotation = *rotation;
    player_state.reaction.damage.motion = *motion;
    player_state.damage_red_overlay_scale = 3500;
    player_state.damage_red_overlay_decay = duration;
}

enum {
    KF_PLAYER_DEATH_SOUND = 0xb,
    KF_PLAYER_DEATH_VOLUME = 110
};

ADDRESS(0x8002a264, 0x88)
void player_death_begin(const SVECTOR *rotation)
{
    if (player_state.death_state != KF_PLAYER_REACTION_DEATH) {
        player_state.death_state = KF_PLAYER_REACTION_DEATH;
        audio_play_sound(KF_PLAYER_DEATH_SOUND, KF_PLAYER_DEATH_VOLUME);
        if (rotation != NULL) {
            player_state.reaction.damage.rotation = *rotation;
        }
        player_state.reaction.damage.motion.vx = 0;
        player_state.death_transition_frame = 0;
    }
}

ADDRESS(0x8002a2ec, 0x2c)
void player_begin_actor_overlap_bob(void)
{
    if (player_state.death_state == KF_PLAYER_REACTION_NORMAL) {
        player_state.death_state = KF_PLAYER_REACTION_OVERLAP_BOB;
        player_state.reaction.damage.rotation.vx = 0;
    }
}

ADDRESS(0x8002a318, 0xc4)
s16 status_phase_step_scaled(s16 *phase, u16 *secondary, s32 duration, s32 scale)
{
    s16 current = *phase;
    s16 other = 0;
    s16 result;

    if (secondary != NULL) {
        other = *secondary;
    }
    if (current != 0 || other != 0) {
        if (current < other) {
            current++;
        } else {
            other = 0;
            current--;
        }
        if (current < duration) {
            result = current * scale / duration;
        } else {
            result = scale;
        }
        *phase = current;
        if (secondary != NULL) {
            *secondary = other;
        }
        return result;
    }
    return -1;
}

ADDRESS(0x8002a3dc, 0x74)
void player_adjust_hp(s32 delta)
{
    s32 value = player_state.vitals.current_hp;

    value += delta;

    if (value <= 0) {
        player_state.vitals.current_hp = 0;
        player_death_begin(NULL);
        return;
    }
    if (player_state.vitals.maximum_hp < value) {
        player_state.vitals.current_hp = player_state.vitals.maximum_hp;
    } else {
        player_state.vitals.current_hp = value;
    }
}

ADDRESS(0x8002a450, 0x58)
void player_adjust_mp(s32 delta)
{
    s32 value = player_state.vitals.current_mp;

    value += delta;

    if (value <= 0) {
        player_state.vitals.current_mp = 0;
        return;
    }
    if (player_state.vitals.maximum_mp < value) {
        player_state.vitals.current_mp = player_state.vitals.maximum_mp;
    } else {
        player_state.vitals.current_mp = value;
    }
}

ADDRESS(0x8002a4a8, 0xa8)
void player_apply_equipment_hp_tick(const KfEquipmentRecord *equipment)
{
    if (equipment->hp_regen_interval != 0
        && player_state.equipment_effect_ticks % equipment->hp_regen_interval == 0) {
        player_adjust_hp(1);
    }
    if (equipment->hp_drain_interval != 0
        && player_state.equipment_effect_ticks % equipment->hp_drain_interval == 0) {
        player_adjust_hp(-1);
    }
}

ADDRESS(0x8002a550, 0x1164)
void player_update_frame(void)
{
    s16 value;
    s32 index;
    u8 object_index;
    u8 step;

    actor_state.actor_overlap_exclusion_flags = KF_ACTOR_FLAG_STATIC_COLLISION_ONLY;
    map_cell_add_layer_occupancy(player_state.camera_position.vx,
                   player_state.camera_position.vz, KF_PLAYER_COLLISION_RADIUS, -1);
    value = status_phase_step_scaled(&player_state.darkness_phase,
                           &player_state.darkness_phase_limit, 64, 0xc00);
    if (value != -1) {
        interpolate_collision_filter_rows(10, 10, 10, 0xef9, value);
    }
    value = status_phase_step_scaled(&player_state.magic_tint_phase,
                           (u16 *)&player_state.magic_tint_phase_limit, 64, 0xe10);
    if (value != -1) {
        interpolate_collision_filter_rows(220, 220, 160, 18000, value);
    }

    if (player_state.damage_red_overlay_scale != 0) {
        player_state.damage_red_overlay_scale -= player_state.damage_red_overlay_decay;
        if (player_state.damage_red_overlay_scale < 0) {
            player_state.damage_red_overlay_scale = 0;
        }
        {
            s16 scale;

            value = player_state.damage_red_overlay_scale;
            if (value > 4095) {
                scale = value = 4096;
            } else {
                scale = value;
            }
            accumulate_color_overlay(60, 0, 0, scale);
        }
    }

    player_state.pad_buttons.current = PadRead(1);
    if (player_state.pad_buttons.current & DAT_8006e578) {
        cd_report_error(KF_CD_ERROR_3);
    }
    if (player_state.pad_buttons.current & DAT_8006e57c) {
        player_state.pad_buttons.current = DAT_8006e56c;
    }

    player_state.movement_step_limit = 200;
    player_state.turn_step_limit = 28;
    if ((player_state.pad_buttons.current & (DAT_8006e590 | DAT_8006e594)) == 0) {
        player_state.turn_step_limit = 35;
    }
    if (player_state.slow_timer != 0) {
        if (player_state.slow_timer < 64) {
            player_state.movement_speed_adjustment_q12 += 100;
            if (player_state.movement_speed_adjustment_q12 > 0) {
                player_state.movement_speed_adjustment_q12 = 0;
            }
        } else {
            player_state.movement_speed_adjustment_q12 -= 100;
            if (player_state.movement_speed_adjustment_q12 < -3300) {
                player_state.movement_speed_adjustment_q12 = -3300;
            }
        }
        player_state.turn_step_limit >>= 1;
        player_state.slow_timer--;
    } else {
        if (player_state.movement_speed_adjustment_decay_latch == 0) {
            player_state.movement_speed_adjustment_q12 += 800;
            if (player_state.movement_speed_adjustment_q12 > 2800) {
                player_state.movement_speed_adjustment_q12 = 2800;
            }
        } else {
            player_state.movement_speed_adjustment_q12 -= 800;
            if (player_state.movement_speed_adjustment_q12 < 0) {
                player_state.movement_speed_adjustment_q12 = 0;
            }
        }
    }
    player_state.movement_step_limit +=
        (player_state.movement_speed_adjustment_q12 * player_state.movement_step_limit) >> KF_FIXED12_BITS;
    if (player_state.paralysis_timer != 0) {
        player_state.movement_step_limit = 0;
        player_state.turn_step_limit = 0;
        player_state.paralysis_timer--;
        if ((player_state.paralysis_timer & 3) == 0) {
            accumulate_color_overlay(60, 30, 0, 0xc00);
        }
    }
    if (player_state.weapon_guard_active != 0) {
        player_state.movement_step_limit >>= 1;
        player_state.turn_step_limit >>= 1;
    }

    switch (player_state.death_state) {
    case KF_PLAYER_REACTION_NORMAL:
        player_update_actions_and_charge();
        player_update_camera_rotation();
        player_update_horizontal_motion();
        goto update_reaction_pose;
    case KF_PLAYER_REACTION_MAP_OBJECT_FOLLOW: {
        KfMapObject *object;

        object_index = player_state.reaction.view.map_object_index;
        object = &map_object_state.objects[object_index];
        player_update_actions_and_charge();
        player_update_camera_rotation();
        player_state.frame_displacement.vx = (s16)object->position.vx
                                - (s16)player_state.camera_position.vx;
        player_state.frame_displacement.vy = (s16)object->position.vy
                                - (s16)player_state.camera_position.vy;
        player_state.frame_displacement.vz = (s16)object->position.vz
                                - (s16)player_state.camera_position.vz;
        player_state.camera_position = object->position;
        player_state.view_rotation_offset.vector = object->rotation;
    }
update_reaction_view:
        player_handle_interaction_and_menu();
        break;
    case KF_PLAYER_REACTION_MAP_OBJECT_APPROACH: {
        KfMapObject *object;
        s32 fraction;

        object_index = player_state.reaction.view.map_object_index;
        object = &map_object_state.objects[object_index];
        player_update_actions_and_charge();
        fraction = player_state.reaction.view.approach_step << 7;
        player_state.camera_position.vx = fixed_lerp_q12(
            player_state.camera_position.vx, object->position.vx, fraction);
        player_state.camera_position.vy = fixed_lerp_q12(
            player_state.camera_position.vy, object->position.vy, fraction);
        player_state.camera_position.vz = fixed_lerp_q12(
            player_state.camera_position.vz, object->position.vz, fraction);
        player_state.view_rotation_offset.components[0] = angle_lerp_shortest_q12(0, object->rotation.vx, fraction);
        player_state.view_rotation_offset.components[1] = angle_lerp_shortest_q12(0, object->rotation.vy, fraction);
        player_state.view_rotation_offset.components[2] = angle_lerp_shortest_q12(0, object->rotation.vz, fraction);
        player_state.camera_rotation_target.angles[0] = angle_lerp_shortest_q12(
            player_state.reaction.view.rotation.angles[0], 0, fraction);
        player_state.camera_rotation_target.angles[1] = angle_lerp_shortest_q12(
            player_state.reaction.view.rotation.angles[1], 0, fraction);
        player_state.camera_rotation_target.angles[2] = angle_lerp_shortest_q12(
            player_state.reaction.view.rotation.angles[2], 0, fraction);
        step = player_state.reaction.view.approach_step++;
        if (step > 31) {
            player_begin_map_object_view_follow(player_state.reaction.view.map_object_index);
        }
        break;
    }
    case KF_PLAYER_REACTION_POSITION_RECOVERY: {
        s32 fraction;

        ++player_state.reaction.position.recovery_step;
        fraction = player_state.reaction.position.recovery_step << 8;
        player_state.camera_position.vx = fixed_lerp_q12(
            player_state.camera_position.vx,
            player_state.reaction.position.position.vx, fraction);
        player_state.camera_position.vy = fixed_lerp_q12(
            player_state.camera_position.vy,
            player_state.reaction.position.position.vy, fraction);
        player_state.camera_position.vz = fixed_lerp_q12(
            player_state.camera_position.vz,
            player_state.reaction.position.position.vz, fraction);
        if (player_state.reaction.position.recovery_step > 15) {
            player_reset_reaction_state();
        }
        break;
    }
    case KF_PLAYER_REACTION_ROTATION:
        if (player_move_reaction_with_collision()) {
            player_reset_reaction_state();
        }
        goto update_reaction_view;
    case KF_PLAYER_REACTION_OVERLAP_BOB: {
        u16 angle_phase;

        player_update_actions_and_charge();
        player_update_camera_rotation();
        player_update_horizontal_motion();
        player_state.camera_vertical_offset = 0;
        player_update_vertical_motion();
        player_state.camera_vertical_offset += -256
                                   + (s16)(rcos((s16)player_state.reaction.angle_phase) >> 4);
        player_state.view_rotation_offset.components[2] = rsin((s16)player_state.reaction.angle_phase) >> 6;
        angle_phase = (player_state.reaction.angle_phase + 128) & KF_ANGLE_WRAP_MASK;
        player_state.reaction.angle_phase = angle_phase;
        if (angle_phase == 0) {
            player_state.view_rotation_offset.components[2] = 0;
            player_reset_reaction_state();
        }
        goto update_reaction_view;
    }
    case KF_PLAYER_REACTION_MOVING_DAMAGE:
        player_update_actions_and_charge();
        player_update_camera_rotation();
        player_update_horizontal_motion();
        player_state.reaction.damage.rotation.vy = 1;
        player_move_reaction_with_collision();
        player_update_reaction_rotation_offsets();
        if (player_state.reaction_rotation_offset[0] == 0
            && player_state.reaction_rotation_offset[1] == 0
            && player_state.reaction_rotation_offset[2] == 0) {
            player_reset_reaction_state();
        }
update_reaction_pose:
        player_update_vertical_motion();
        goto update_reaction_view;
    case KF_PLAYER_REACTION_ROTATION_DAMAGE:
        player_update_reaction_rotation_offsets();
        if (player_state.reaction_rotation_offset[0] == 0
            && player_state.reaction_rotation_offset[1] == 0
            && player_state.reaction_rotation_offset[2] == 0) {
            player_reset_reaction_state();
        }
        goto update_reaction_view;
    case KF_PLAYER_REACTION_DEATH:
        player_state.vitals.current_hp = 0;
        value = angle_velocity_step(-KF_ANGLE_QUARTER_TURN, player_state.reaction_rotation_offset[0],
                                    player_state.reaction.damage.motion.vx, 8, 4);
        player_state.reaction.damage.motion.vx = value;
        player_state.reaction_rotation_offset[0] += (value * 3) >> 1;
        {
            s16 offset = player_state.camera_vertical_offset;
            player_state.camera_vertical_offset = offset < 1500 ? offset + 500 : 1500;
        }
        player_move_reaction_with_collision();
        player_state.death_transition_frame++;
        if (player_state.death_transition_frame == 31
            && player_state.collision_lower_clearance > -1001
            && player_state.collision_upper_clearance >= 0
            && player_state.fatal_fall_latch == 0
            && (player_state.equipped_accessory_id == KF_ITEM_STATUS_GUARD_ACCESSORY
                || player_state.equipped_extra_id == KF_ITEM_STATUS_GUARD_ACCESSORY)
            && game_counter_bytes[KF_ENUM_ENCODE(u8, KF_OBJECT_83)] != 0) {
            game_counter_bytes[KF_ENUM_ENCODE(u8, KF_OBJECT_83)]--;
            render_frames_with_color_overlay(KF_COLOR_OVERLAY_ADD, 0, KF_FIXED12_ONE, 512);
            player_reset_status();
            render_frames_with_color_overlay(KF_COLOR_OVERLAY_ADD, KF_FIXED12_ONE, 0, -512);
        }
        if (player_state.death_transition_frame > 31) {
            if (player_state.death_transition_frame < 65) {
                s32 shade = fixed_lerp_q12(0, 255,
                                            (player_state.death_transition_frame - 32) * 128);
                render_set_color_overlay(KF_COLOR_OVERLAY_SUBTRACT | KF_COLOR_OVERLAY_FRONT, shade, shade, shade);
            } else {
                KfEffectRecord *effect = effect_state.records;
                s32 count;

                for (count = KF_EFFECT_CAPACITY; count != 0; count--) {
                    effect->type = KF_EFFECT_SLOT_FREE;
                    effect++;
                }
                if (game_counter_bytes[KF_ENUM_ENCODE(u8, KF_ITEM_FULL_RESTORE)] != 0
                    && (event_state.control.fields.post_death_reload_flags &
                        KF_EVENT_POST_DEATH_RELOAD_ENABLED) != 0) {
                    actor_disable_type3_transition_actors();
                    player_state.camera_rotation_target.angles[0] = 0;
                    player_state.camera_rotation_target.angles[1] = 0xc00;
                    player_state.camera_rotation_target.angles[2] = 0;
                    player_state.camera_position.vy = -0x2480;
                    player_state.camera_position.vx = 0x1e000;
                    player_state.camera_position.vz = 0x22000;
                    player_state.map_layer_index = 5;
                    game_counter_bytes[KF_ENUM_ENCODE(u8, KF_ITEM_FULL_RESTORE)]--;
                    event_world_state_save_slot(resource_state.active_resource_ids[0]);
                    player_reset_status();
                    player_reload_map_resources(1, 1, 1, 1, 1, 0x43);
                } else {
                    player_initialize_state();
                    event_state_initialize();
                    player_reload_map_resources(0, 0, 0, 0, 0, KF_RESOURCE_REQUEST_KEEP);
                }
            }
        }
        break;
    default:
        break;
    }

    player_state.pad_buttons.halves.previous = player_state.pad_buttons.current;
    if (player_state.poison_timer != 0) {
        if (player_state.poison_timer % 30 == 0) {
            player_state.damage_red_overlay_scale = 2400;
            player_state.damage_red_overlay_decay = 60;
            player_adjust_hp_unclamped(-1);
        }
        player_state.poison_timer--;
    }
    if (player_state.defense_boost_timer != 0 && --player_state.defense_boost_timer == 0) {
        player_recalculate_combat_stats();
    }
    if (player_state.attack_boost_timer != 0 && --player_state.attack_boost_timer == 0) {
        player_recalculate_combat_stats();
    }
    if (player_state.full_mp_timer != 0) {
        player_state.full_mp_timer--;
        player_state.vitals.current_mp = player_state.vitals.maximum_mp;
        if (player_state.full_mp_timer == 0) {
            notify_enqueue(KF_NOTIFICATION_EFFECT_EXPIRED);
        }
    }
    if (player_state.magic_boost_timer != 0 && --player_state.magic_boost_timer == 0) {
        player_recalculate_combat_stats();
        notify_enqueue(KF_NOTIFICATION_EFFECT_EXPIRED);
    }
    if (player_state.map_marker_visual_effect_timer != 0) {
        if (player_state.map_marker_visual_effect_timer == 1) {
            MoveImage(&player_status_texture_row_0, 0x240, 0x103);
            MoveImage(&player_status_texture_row_2, 0x240, 0x106);
            notify_enqueue(KF_NOTIFICATION_EFFECT_EXPIRED);
            player_state.map_marker_visual_effect_timer = 0;
            map_object_refresh_cell_markers(KF_MAP_CELL_MARKER_PLACE);
        } else {
            if ((player_state.map_marker_visual_effect_timer & 7) == 0) {
                MoveImage(&player_status_texture_row_1, 0x240, 0x103);
            }
            if ((player_state.map_marker_visual_effect_timer & 7) == 4) {
                MoveImage(&player_status_texture_row_3, 0x240, 0x106);
            }
            if ((player_state.map_marker_visual_effect_timer & 7) == 1) {
                map_object_refresh_cell_markers(KF_MAP_CELL_MARKER_CLEAR);
            }
            player_state.map_marker_visual_effect_timer--;
        }
    }
    value = status_phase_step_scaled(&player_state.curse_strength,
                           &player_state.curse_phase_limit, 64, 0xc00);
    if (value != -1) {
        if (player_state.curse_strength == 0) {
            player_recalculate_combat_stats();
        }
        accumulate_color_overlay(0x46, 0x46, 30, value);
    }

    player_state.camera_rotation.angles[0] =
        player_state.camera_rotation_target.angles[0]
        + player_state.reaction_rotation_offset[0] + player_state.view_rotation_offset.components[0]
        + player_state.vertical_motion_pitch_offset;
    player_state.camera_rotation.angles[1] =
        player_state.camera_rotation_target.angles[1]
        + player_state.reaction_rotation_offset[1] + player_state.view_rotation_offset.components[1]
        + player_state.camera_yaw_roll_offsets[0];
    player_state.camera_rotation.angles[2] =
        player_state.camera_rotation_target.angles[2]
        + player_state.reaction_rotation_offset[2] + player_state.view_rotation_offset.components[2]
        + player_state.camera_yaw_roll_offsets[1];
    map_cell_add_layer_occupancy(player_state.camera_position.vx,
                   player_state.camera_position.vz, KF_PLAYER_COLLISION_RADIUS, 1);
    actor_state.actor_overlap_exclusion_flags = KF_ACTOR_FLAGS_NONE;
    index = actor_find_overlap_excluding_target_type3(player_state.camera_position.vx,
                           player_state.camera_position.vy,
                           player_state.camera_position.vz, 1, KF_PLAYER_HEIGHT);
    if (index != KF_ACTOR_INDEX_NONE &&
        (actor_state.actors[index].flags & KF_ACTOR_FLAG_PLAYER_OVERLAP_BOB) != KF_ACTOR_FLAGS_NONE) {
        player_begin_actor_overlap_bob();
    }
    player_update_weapon_attack();
    if (player_state.equipped_weapon_id != KF_OBJECT_NONE) {
        KfWeaponRecordGame *weapon = player_state.equipped_weapon_record;
        if (weapon->hp_regen_interval != 0
            && player_state.equipment_effect_ticks % weapon->hp_regen_interval == 0) {
            player_adjust_hp(1);
        }
        weapon = player_state.equipped_weapon_record;
        if (weapon->mp_regen_interval != 0
            && player_state.equipment_effect_ticks % weapon->mp_regen_interval == 0) {
            player_adjust_mp(1);
        }
    }
    if (player_state.equipped_head_id != KF_OBJECT_NONE) {
        player_apply_equipment_hp_tick(player_state.equipped_head_record);
    }
    if (player_state.equipped_body_id != KF_OBJECT_NONE) {
        player_apply_equipment_hp_tick(player_state.equipped_body_record);
    }
    if (player_state.equipped_arm_id != KF_OBJECT_NONE) {
        player_apply_equipment_hp_tick(player_state.equipped_arm_record);
    }
    if (player_state.equipped_leg_id != KF_OBJECT_NONE) {
        player_apply_equipment_hp_tick(player_state.equipped_leg_record);
    }
    if (player_state.equipped_shield_id != KF_OBJECT_NONE) {
        player_apply_equipment_hp_tick(player_state.equipped_shield_record);
    }
    if (player_state.equipped_accessory_id != KF_OBJECT_NONE) {
        player_apply_equipment_hp_tick(player_state.equipped_accessory_record);
    }
    if (player_state.equipped_extra_id != KF_OBJECT_NONE) {
        player_apply_equipment_hp_tick(player_state.equipped_extra_record);
    }
    if (player_state.equipped_head_id == KF_OBJECT_25 && rand() < 36) {
        player_state.magic_origin_offset.vx = 0;
        player_state.magic_origin_offset.vy = -300;
        player_state.magic_origin_offset.vz = 400;
        player_dispatch_magic_effect(KF_EFFECT_KIND_11);
    }
    if (player_state.equipped_body_id == KF_OBJECT_31) {
        interpolate_collision_filter_rows(20, 20, 20, 5000, 0x800);
    }
    player_state.equipment_effect_ticks++;
}
