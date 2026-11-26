#ifndef KF_GAME_ITEM_H
#define KF_GAME_ITEM_H

#include <kf/lib/types.h>
#include <kf/lib/enum.h>

/*
 * Inventory items, equipment, death drops and map objects share one ID
 * namespace (KF1 KfObjectId). Item use passes the inventory ID unchanged as
 * the scene command (event_scene_command_dispatch), which decrements
 * event_item_counters_table[id] and can store it as a map object's ID
 * (event_spawn_effect_object); pickups increment the counter of the object's
 * own ID; actor death drops spawn map objects by ID. event_item_counters_table
 * covers IDs 0..119; map-object templates extend the namespace to 319, so
 * the domain is a halfword while byte fields keep their byte storage.
 *
 * Weapons are 0..17 (g_weapon_table) and equipment records start at
 * 21. Members without behavioural evidence keep their decimal encoding as a
 * WIP name. Behaviour seen in the consumers: using 77..80 returns an 82,
 * and using 82 at object 195 yields a 77; 84 starts the map-marker effect,
 * 86 and 87 start the full-MP and magic-boost timers, 90..94 unlock spell
 * lists, 99..109 are consumed by map-object markers, 114..116 arm the
 * object-control events of object 189, and weapons 16/17 spend 117/118.
 * The accessory names describe one consumer each: 54 also spends an 83 to
 * survive a fall, and 57 doubles weapon 13's charge.
 */
KF_ENUM_BEGIN(KfObjectId, u16)
    KF_ITEM_STARTING_WEAPON = 0,
    KF_OBJECT_10 = 10,
    KF_OBJECT_11 = 11,
    KF_OBJECT_12 = 12,
    KF_ITEM_GUARDING_WEAPON = 13,
    KF_OBJECT_15 = 15,
    KF_OBJECT_16 = 16,
    KF_OBJECT_17 = 17,
    KF_OBJECT_18 = 18,
    KF_ITEM_HALF_MAGIC_CHARGE_HEAD = 24,
    KF_ITEM_RANDOM_CAST_HEAD = 25,
    KF_ITEM_MAGIC_LIMITING_BODY = 31,
    KF_ITEM_HALF_WEAPON_CHARGE_LEG = 44,
    KF_ITEM_CHARGE_KEEPING_SHIELD = 50,
    KF_OBJECT_51 = 51,
    KF_ITEM_POISON_GUARD_ACCESSORY = 53,
    KF_ITEM_STATUS_GUARD_ACCESSORY = 54,
    KF_ITEM_ATTACK_BONUS_ACCESSORY = 55,
    KF_ITEM_MAGIC_BONUS_ACCESSORY = 56,
    KF_ITEM_PHYSICAL_POWER_BONUS_ACCESSORY = 57,
    KF_ITEM_STATUS_DURATION_HALVING_ACCESSORY = 58,
    KF_ITEM_BALLISTIC_VARIANT_ACCESSORY = 59,
    KF_OBJECT_SCATTERED_EFFECT = 70,
    KF_ITEM_RELIEVE_AILMENTS = 71,
    KF_ITEM_RESTORE_MP_40 = 72,
    KF_ITEM_RAISE_BASE_MAGIC = 73,
    KF_ITEM_RESTORE_HP_40 = 74,
    KF_ITEM_CURE_POISON_RESTORE_HP_15 = 75,
    KF_ITEM_FULL_RESTORE = 76,
    KF_ITEM_RESTORE_HP_100 = 77,
    KF_ITEM_RESTORE_MP_50 = 78,
    KF_ITEM_CLEAR_AILMENTS = 79,
    KF_ITEM_MIXED_RESTORE = 80,
    KF_OBJECT_81 = 81,
    KF_ITEM_EMPTY_CONTAINER = 82,
    KF_ITEM_FALL_SURVIVAL_CONSUMABLE = 83,
    KF_ITEM_MAP_MARKER_EFFECT = 84,
    KF_ITEM_INSPECTION = 85,
    KF_ITEM_FULL_MP_TIMER = 86,
    KF_ITEM_MAGIC_BOOST_TIMER = 87,
    KF_ITEM_MARKER_SIGNAL = 88,
    KF_ITEM_LOCATE_ITEMS = 89,
    KF_OBJECT_90 = 90,
    KF_OBJECT_91 = 91,
    KF_OBJECT_92 = 92,
    KF_OBJECT_93 = 93,
    KF_OBJECT_94 = 94,
    KF_ITEM_TRADE_CURRENCY = 96,
    KF_OBJECT_97 = 97,
    KF_OBJECT_99 = 99,
    KF_OBJECT_100 = 100,
    KF_OBJECT_101 = 101,
    KF_OBJECT_102 = 102,
    KF_OBJECT_103 = 103,
    KF_OBJECT_104 = 104,
    KF_OBJECT_106 = 106,
    KF_OBJECT_107 = 107,
    KF_OBJECT_108 = 108,
    KF_OBJECT_109 = 109,
    KF_OBJECT_111 = 111,
    KF_OBJECT_112 = 112,
    KF_OBJECT_113 = 113,
    KF_OBJECT_114 = 114,
    KF_OBJECT_115 = 115,
    KF_OBJECT_116 = 116,
    KF_OBJECT_117 = 117,
    KF_OBJECT_118 = 118,
    KF_OBJECT_MARKER_SIGNAL_RECEIVER = 157,
    KF_OBJECT_163 = 163,
    KF_OBJECT_ITEM_RECEPTACLE = 184,
    KF_OBJECT_CONTROL_SOCKET = 189,
    KF_OBJECT_CONTAINER_REFILL = 195,
    KF_OBJECT_NONE = 0xff,
    /* Serialized placement rows mark an empty slot with the full halfword. */
    KF_OBJECT_PLACEMENT_NONE = 0xffff
KF_ENUM_END(KfObjectId)

/* Inventory ID ranges: event_item_counters_table and the per-item menu tables cover
 * KF_ITEM_ID_COUNT IDs. The equipment menu lists one inclusive range per
 * category, the item-use menu the usable range, and menu_buy_owned_items
 * the key items that map-object markers consume. */
enum {
    KF_ITEM_ID_COUNT = 0x78,
    KF_ITEM_ID_LAST = KF_ITEM_ID_COUNT - 1,
    KF_ITEM_WEAPON_FIRST = 0,
    KF_ITEM_WEAPON_LAST = 20,
    KF_ITEM_HEAD_FIRST = 21,
    KF_ITEM_HEAD_LAST = 27,
    KF_ITEM_BODY_FIRST = 28,
    KF_ITEM_BODY_LAST = 33,
    KF_ITEM_ARM_FIRST = 34,
    KF_ITEM_ARM_LAST = 40,
    KF_ITEM_LEG_FIRST = 41,
    KF_ITEM_LEG_LAST = 46,
    KF_ITEM_SHIELD_FIRST = 47,
    KF_ITEM_SHIELD_LAST = 52,
    KF_ITEM_ACCESSORY_FIRST = 53,
    KF_ITEM_ACCESSORY_LAST = 59,
    KF_ITEM_USABLE_FIRST = 70,
    KF_ITEM_USABLE_LAST = 116,
    KF_ITEM_KEY_FIRST = 99,
    KF_ITEM_KEY_LAST = 109
};

#endif
