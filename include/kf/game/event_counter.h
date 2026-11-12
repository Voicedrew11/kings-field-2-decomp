#ifndef KF_GAME_EVENT_COUNTER_H
#define KF_GAME_EVENT_COUNTER_H

#include <kf/lib/bool.h>
#include <kf/lib/types.h>
#include <kf/game/item.h>

/* Event reset clears 0x1e words at this base. The menu reads byte +0x60. */
extern u8 event_item_counters_table[KF_ITEM_ID_COUNT];

b32 game_counter_decrement(KF_ENUM_PARAM(KfObjectId, s32) index);
b32 game_counter_increment(KF_ENUM_PARAM(KfObjectId, s32) index);

#endif
