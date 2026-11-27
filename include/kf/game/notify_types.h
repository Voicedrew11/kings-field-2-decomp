#ifndef KF_GAME_NOTIFY_TYPES_H
#define KF_GAME_NOTIFY_TYPES_H

#include <kf/lib/types.h>
#include <kf/lib/enum.h>

/* Cells of the notification message atlas (row id % 18, column id / 18).
 * KF2's atlas differs from KF1's; members are named from their consumers,
 * since the message text is an image. PAYLOAD carries a
 * number (the gold reward) drawn with four digits. */
KF_ENUM_BEGIN(KfNotificationId, u8)
    KF_NOTIFICATION_LEVEL_UP = 0,
    KF_NOTIFICATION_MAGIC_LEARNED = 1,
    KF_NOTIFICATION_SOCKET_REFUSED = 4,
    KF_NOTIFICATION_CONTAINER_ALREADY_OPENED = 6,
    KF_NOTIFICATION_RECALL_SOCKET_EMPTY = 16,
    /* game_counter_increment refuses a counter already at 99. */
    KF_NOTIFICATION_CANNOT_CARRY_MORE = 18,
    /* An unhandled scene command, or a shortcut item with none left. */
    KF_NOTIFICATION_NOTHING_HAPPENS = 20,
    KF_NOTIFICATION_PAYLOAD_ID = 21,
    KF_NOTIFICATION_REFILL_REFUSED = 22,
    KF_NOTIFICATION_PHYSICAL_POWER_INCREASED = 31,
    KF_NOTIFICATION_MAGIC_POWER_INCREASED = 32,
    /* The full-MP, magic-boost and map-marker timers ran out. */
    KF_NOTIFICATION_EFFECT_EXPIRED = 34,
    KF_NOTIFICATION_MAX_QUEUED_ID = 239,
    KF_NOTIFICATION_NONE = 0xff
KF_ENUM_END(KfNotificationId)

/* notification_update: wait for a message, fade in, hold, fade out. */
KF_ENUM_BEGIN(KfNotificationPhase, u8)
    KF_NOTIFICATION_IDLE = 0,
    KF_NOTIFICATION_FADE_IN = 1,
    KF_NOTIFICATION_HOLD = 2,
    KF_NOTIFICATION_FADE_OUT = 3
KF_ENUM_END(KfNotificationPhase)

#endif
