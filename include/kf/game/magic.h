#ifndef KF_GAME_MAGIC_H
#define KF_GAME_MAGIC_H

#include <kf/lib/types.h>
#include <kf/lib/enum.h>

/*
 * Spell and effect identities share one namespace (KF1 KfEffectKind): the
 * magic shortcuts, queued magic action and weapon magic IDs are passed
 * unchanged to effect_construct_record, whose record kind indexes
 * magic_records for kinds below KF_MAGIC_RECORD_COUNT. Kinds from 100 are
 * child or variant effects without a magic row; actor_dispatch_group_effect
 * also accepts 108, 110, 112 and 123, which spawn actors or remap to another
 * kind instead of constructing themselves. Names describe the record's
 * update in effect_update_dispatch (a spell ID's name is the record its cast
 * finally constructs). Members that differ only in render model, scale or
 * acceleration keep their decimal encoding as a WIP name.
 */
KF_ENUM_BEGIN(KfEffectKind, u8)
    KF_EFFECT_KIND_FALLING_DROP = 0,
    KF_EFFECT_KIND_1 = 1,
    KF_EFFECT_KIND_EXPANDING_RING = 2,
    KF_EFFECT_KIND_DROP_FROM_ABOVE_BURST = 3,
    KF_EFFECT_KIND_4 = 4,
    KF_EFFECT_KIND_VERTEX_SWARM = 5,
    KF_EFFECT_KIND_HOLDING_TRAIL = 6,
    KF_EFFECT_KIND_7 = 7,
    KF_EFFECT_KIND_RING_WAVE = 8,
    KF_EFFECT_KIND_HOMING = 9,
    KF_EFFECT_KIND_ACTOR_TURRET = 10,
    KF_EFFECT_KIND_11 = 11,
    KF_EFFECT_KIND_12 = 12,
    KF_EFFECT_KIND_13 = 13,
    KF_EFFECT_KIND_CURE_POISON = 14,
    KF_EFFECT_KIND_DEFENSE_BOOST = 15,
    KF_EFFECT_KIND_HEAL = 16,
    KF_EFFECT_KIND_ATTACK_BOOST = 17,
    KF_EFFECT_KIND_18 = 18,
    KF_EFFECT_KIND_HEAL_AND_CURE = 19,
    KF_EFFECT_KIND_20 = 20,
    KF_EFFECT_KIND_22 = 22,
    KF_EFFECT_KIND_ATTACHED_RELEASE = 23,
    KF_EFFECT_KIND_PLAYER_HOMING_WITH_FOLLOWERS = 24,
    KF_EFFECT_KIND_25 = 25,
    KF_EFFECT_KIND_26 = 26,
    KF_EFFECT_KIND_27 = 27,
    KF_EFFECT_KIND_28 = 28,
    KF_EFFECT_KIND_29 = 29,
    KF_EFFECT_KIND_30 = 30,
    KF_EFFECT_KIND_31 = 31,
    KF_EFFECT_KIND_32 = 32,
    KF_EFFECT_KIND_PLAYER_HOMING = 33,
    KF_EFFECT_KIND_34 = 34,
    KF_EFFECT_KIND_35 = 35,
    KF_EFFECT_KIND_38 = 38,
    KF_EFFECT_KIND_39 = 39,
    KF_EFFECT_KIND_40 = 40,
    KF_EFFECT_KIND_DELAYED_BURST = 42,
    KF_EFFECT_KIND_43 = 43,
    KF_EFFECT_KIND_44 = 44,
    KF_EFFECT_KIND_45 = 45,
    KF_EFFECT_KIND_46 = 46,
    KF_EFFECT_KIND_47 = 47,
    KF_EFFECT_KIND_48 = 48,
    KF_EFFECT_KIND_49 = 49,
    KF_EFFECT_KIND_CHARGED_BURST = 50,
    KF_EFFECT_KIND_51 = 51,
    KF_EFFECT_KIND_52 = 52,
    KF_EFFECT_KIND_PLAYER_HOMING_FAST = 53,
    KF_EFFECT_KIND_54 = 54,
    KF_EFFECT_KIND_100 = 100,
    KF_EFFECT_KIND_PARTICLE = 101,
    KF_EFFECT_KIND_LOWER_BOUND_RIPPLE = 102,
    KF_EFFECT_KIND_103 = 103,
    KF_EFFECT_KIND_104 = 104,
    KF_EFFECT_KIND_VERTEX_SWARM_PARTICLE = 105,
    KF_EFFECT_KIND_RING_WAVE_BURST = 106,
    KF_EFFECT_KIND_HOLDING_TRAIL_SEGMENT = 107,
    KF_EFFECT_KIND_ACTOR_FORWARD_SHOT = 108,
    KF_EFFECT_KIND_RECORD_FOLLOWER = 109,
    KF_EFFECT_KIND_SPAWN_ASCENDING_SPIN_ACTOR = 110,
    KF_EFFECT_KIND_FALLING_DROP_EMITTER = 111,
    KF_EFFECT_KIND_SPAWN_COLLISION_MOVE_ACTOR = 112,
    KF_EFFECT_KIND_113 = 113,
    KF_EFFECT_KIND_DROP_FROM_ABOVE = 114,
    KF_EFFECT_KIND_115 = 115,
    KF_EFFECT_KIND_116 = 116,
    KF_EFFECT_KIND_117 = 117,
    KF_EFFECT_KIND_118 = 118,
    KF_EFFECT_KIND_119 = 119,
    KF_EFFECT_KIND_120 = 120,
    KF_EFFECT_KIND_121 = 121,
    KF_EFFECT_KIND_122 = 122,
    KF_EFFECT_KIND_123 = 123,
    KF_MAGIC_NONE = 0xff
KF_ENUM_END(KfEffectKind)

/*
 * Actor damage flags passed to actor_apply_magic_to_actor. The low bits are
 * the damage mode that selects physical or magic training credit; bits 4-5
 * keep the source class of the effect type that caused it (0x10 player,
 * 0x20 hazards, which actor flag 0x100000 ignores).
 */
/* Radial magic mode (effect_apply_radial_magic_damage, actor_apply_area_magic,
 * player_apply_radial_damage): REACH measures to the target's reach cylinder,
 * REACH_ABOVE also misses a target whose top is above the origin, and any
 * other value is the vertical window passed to the point-distance test. */
enum {
    KF_RADIAL_MODE_REACH = 0x8000,
    KF_RADIAL_MODE_REACH_ABOVE = 0x8001
};

KF_ENUM_BEGIN(KfActorDamageFlags, u8)
    KF_ACTOR_DAMAGE_NONE = 0,
    KF_ACTOR_DAMAGE_PHYSICAL = 1,
    KF_ACTOR_DAMAGE_MAGIC = 2,
    KF_ACTOR_DAMAGE_MODE_MASK = 3,
    KF_ACTOR_DAMAGE_FROM_PLAYER = 0x10,
    KF_ACTOR_DAMAGE_FROM_HAZARD = 0x20,
    KF_ACTOR_DAMAGE_SOURCE_MASK = 0x30
KF_ENUM_END(KfActorDamageFlags)
KF_ENUM_FLAGS(KfActorDamageFlags, u8)

#endif
