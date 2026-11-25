#include <kf/game/event_stream.h>
#include <kf/game/game.h>
#include <kf/lib/null.h>
#include <kf/game/map_object.h>
#include <kf/game/player.h>
#include <kf/lib/address.h>
#include <kf/game/actor.h>
#include <kf/game/callback.h>
#include <kf/game/event_counter.h>
#include <kf/game/event_state.h>
#include <kf/game/graphics.h>
#include <kf/game/map_cell.h>
#include <kf/game/menu.h>
#include <kf/lib/math.h>
#include <kf/game/audio.h>
#include <kf/game/cd.h>
#include <kf/game/collision_cache.h>
#include <kf/game/effect.h>
#include <kf/game/notify.h>
#include <kf/game/resources.h>
#include <stdarg.h>
#include <psyq/sdk.h>
#include <psyq/pad.h>
#include <kf/game/card.h>
#include <kf/game/card_payload.h>
#include <psyq/libc.h>

DATA(0x8006888c, 0x8, ".data")
u8 event_magic_unlock_ids_5a[8] = {7, 8, 9, 10, 0xff, 0, 0, 0};

DATA(0x80068894, 0x8, ".data")
u8 event_magic_unlock_ids_5b[8] = {14, 15, 0, 13, 0xff, 0, 0, 0};

DATA(0x8006889c, 0x8, ".data")
u8 event_magic_unlock_ids_5c[8] = {16, 1, 2, 3, 0xff, 0, 0, 0};

DATA(0x800688a4, 0x8, ".data")
u8 event_magic_unlock_ids_5d[8] = {4, 17, 5, 6, 0xff, 0, 0, 0};

DATA(0x800688ac, 0x8, ".data")
u8 event_magic_unlock_ids_5e[8] = {18, 19, 11, 12, 0xff, 0, 0, 0};

DATA(0x8009b52c, 0x78, ".bss")
u8 event_item_counters_table[KF_ITEM_ID_COUNT];

typedef void (*KfEventCommandCallback)(const VECTOR *position,
                                       const KfPlayerViewRotation *rotation,
                                       KF_ENUM_PARAM(KfObjectId, s32) command);

RODATA(0x80012890, 0x3a8)

/* Unreferenced return stubs; original roles and TU owner remain unresolved. */
ADDRESS(0x80046c54, 0x8)
void event_unused_stub_0(void)
{
}

ADDRESS(0x80046c5c, 0x8)
void event_unused_stub_1(void)
{
}

ADDRESS(0x80046c64, 0xb4)
void scene_position_from_camera_offset(s32 x, s32 y, s32 z, s32 pitch, s32 yaw,
                   s32 height_offset, s32 z_offset, VECTOR *output)
{
    SVECTOR offset;
    struct KfEulerAngles angles;
    s32 y_position;
    s32 camera_y;

    offset.vx = x;
    offset.vy = y + height_offset;
    offset.vz = z + z_offset;
    angles.x = -pitch;
    angles.y = yaw;
    angles.z = 0;
    vector_rotate_yxz(&angles, &offset, output);
    output->vx += cur_avatar_status.camera_position.vx;
    y_position = output->vy - KF_PLAYER_CAMERA_EYE_OFFSET;
    camera_y = height_offset + cur_avatar_status.camera_position.vy;
    output->vy = y_position + camera_y;
    output->vz += cur_avatar_status.camera_position.vz;
}

ADDRESS(0x80046d18, 0xcc)
void scene_pose_interpolate(
    KfMapObject *destination, const VECTOR *start_position,
    const VECTOR *end_position, const SVECTOR *start_angles,
    const SVECTOR *end_angles, s32 fraction)
{
    if (start_position != NULL) {
        destination->position.vx = fixed_lerp_q12(start_position->vx, end_position->vx, fraction);
        destination->position.vy = fixed_lerp_q12(start_position->vy, end_position->vy, fraction);
        destination->position.vz = fixed_lerp_q12(start_position->vz, end_position->vz, fraction);
    }

    if (start_angles != NULL) {
        destination->rotation.vx = angle_lerp_shortest_q12(start_angles->vx, end_angles->vx, fraction);
        destination->rotation.vz = angle_lerp_shortest_q12(start_angles->vz, end_angles->vz, fraction);
    }
}

#define ACTOR_ANIMATION_ADVANCE(actor, delta) do { \
    (actor)->animation_phase = \
        ((delta) + (actor)->animation_phase) & KF_ACTOR_ANIMATION_PHASE_MAX; \
} while (0)

enum {
    /* Clears the low bit of the step so the half step used by the phase tolerance is exact. */
    ACTOR_ANIMATION_STEP_EVEN_MASK = 0xfffe
};
ADDRESS(0x80046de4, 0xa4)
void actor_animation_seek_phase(KfActor *actor, KfAnimationClip state, u16 phase, s32 target_phase,
                                s32 phase_step)
{
    s32 step;
    s32 final_phase;
    u32 half_step;

    if ((u16)phase_step == 0) {
        return;
    }

    step = phase_step & ACTOR_ANIMATION_STEP_EVEN_MASK;
    half_step = (u32)step >> 1;
    final_phase = target_phase - half_step;
    actor->animation_id = state;
    actor->animation_phase = phase;

    while (!angle_within_tolerance(actor->animation_phase, (u16)final_phase, half_step)) {
        ACTOR_ANIMATION_ADVANCE(actor, step);
        render_game_frame(NULL, NULL);
    }

    actor->animation_phase = final_phase & KF_ACTOR_ANIMATION_PHASE_MAX;
    render_game_frame(NULL, NULL);
}

/* candidate->word_12.bytes.marker_state: CLEAR means no rewind is pending;
 * REWIND_ARMED is set when a rewind marker is executed, so a different actor
 * resuming the stream skips to the marker's continuation. */
enum {
    EVENT_MARKER_CLEAR = 0,
    EVENT_MARKER_REWIND_ARMED = 1
};

ADDRESS(0x80046e88, 0x5c)
u8 event_target_stream_find_marker(const KfTargetCandidate *candidate, u8 marker)
{
    const u8 *cursor = candidate->word_14.bytes;

    for (;;) {
        s32 code = *cursor++;

        if (code != KF_EVENT_STREAM_MARKER_RECORD) {
            if (code == KF_EVENT_STREAM_END) {
                return candidate->word_10.bytes.fallback_offset;
            }
            continue;
        }
        if (*cursor == marker) {
            const u8 *base = &candidate->word_12.bytes.marker_state;
            return cursor - base;
        }
        cursor++;
    }
}

ADDRESS(0x80046ee4, 0x11c)
u8 *event_target_stream_resolve_cursor(KfActor *actor)
{
    KfTargetCandidate *candidate =
        actor_status.target_groups[actor->group_index].targets[0].pointer;
    u8 *cursor = candidate->word_14.bytes;

    for (;;) {
        u8 code = *cursor;

        if (code != KF_EVENT_STREAM_CONDITIONAL_MARKER) {
            if (code != KF_EVENT_STREAM_START) {
                /* Retail retries this byte; the stream must supply a control code. */
                continue;
            }
            if (candidate->word_10.bytes.fallback_offset == 0) {
                cursor++;
                candidate->word_10.bytes.fallback_offset = cursor - candidate->word_14.bytes;
                return cursor;
            }
use_fallback:
            return candidate->word_14.bytes + candidate->word_10.bytes.fallback_offset;
        }
        if (cur_evt_state.control.bytes[cursor[1]] == cursor[2]) {
            u8 offset = event_target_stream_find_marker(candidate, cursor[3]);
            if (candidate->word_10.bytes.fallback_offset < offset) {
                candidate->word_10.bytes.fallback_offset = offset;
            }
            cur_evt_state.control.fields.stream_actor_definition_id = actor->definition_id;
            candidate->word_12.bytes.marker_state = EVENT_MARKER_CLEAR;
            goto use_fallback;
        }
        cursor += 4;
    }
}

ADDRESS(0x80047000, 0x444)
void event_target_stream_execute(KfActor *actor)
{
    KfTargetCandidate *candidate =
        actor_status.target_groups[actor->group_index].targets[0].pointer;
    u8 *cursor;
    s32 repeat;
    b32 restore_state;
    KfAnimationClip saved_state;
    s32 old_counter;
    s32 choice;

    if (candidate == NULL) {
        return;
    }
    if (candidate->type != KF_ACTOR_TARGET_EVENT_STREAM) {
        return;
    }
    restore_state = KF_FALSE;
    repeat = 0;
    if (candidate->word_10.bytes.fallback_offset == 0) {
        cur_evt_state.control.fields.stream_actor_definition_id = actor->definition_id;
    }
    cursor = event_target_stream_resolve_cursor(actor);
    if (cur_evt_state.control.fields.stream_actor_definition_id != actor->definition_id &&
        candidate->word_12.bytes.marker_state == EVENT_MARKER_REWIND_ARMED) {
        while (*cursor++ != KF_EVENT_STREAM_REWIND_MARKER) {
        }
        cursor++;
        candidate->word_10.bytes.fallback_offset = cursor - candidate->word_14.bytes;
        candidate->word_12.bytes.marker_state = EVENT_MARKER_CLEAR;
    }

    for (;;) {
        switch (*cursor) {
        case KF_EVENT_STREAM_REWIND_MARKER:
            candidate->word_12.bytes.marker_state = EVENT_MARKER_REWIND_ARMED;
            /* The two rewind opcodes share their byte-count operand. */
        case KF_EVENT_STREAM_REWIND:
        {
            u8 count = cursor[1];
            candidate->word_10.bytes.fallback_offset -= count;
            cursor -= count;
            continue;
        }
        case KF_EVENT_STREAM_BRANCH:
            if (cur_evt_state.control.bytes[cursor[1]] == cursor[2]) {
                candidate->word_10.bytes.fallback_offset = event_target_stream_find_marker(candidate, cursor[3]);
                cursor = candidate->word_14.bytes + candidate->word_10.bytes.fallback_offset;
            } else {
                cursor += 4;
                candidate->word_10.bytes.fallback_offset += 4;
            }
            continue;
        case KF_EVENT_STREAM_MARKER_RECORD:
            cursor += 2;
            candidate->word_10.bytes.fallback_offset += 2;
            continue;
        case KF_EVENT_STREAM_SKIP:
            break;
        case KF_EVENT_STREAM_CALLBACK:
            cursor++;
            candidate->word_10.bytes.fallback_offset++;
            ((void (*)(KfActor *, s32))asset_status.active_table[4])(actor, *cursor);
            break;
        case KF_EVENT_STREAM_REPEAT:
            cursor++;
            candidate->word_10.bytes.fallback_offset++;
            repeat = *cursor;
            break;
        case KF_EVENT_STREAM_RESET_MARKER:
            cur_evt_state.control.fields.stream_actor_definition_id = actor->definition_id;
            cursor++;
            candidate->word_10.bytes.fallback_offset++;
            candidate->word_12.bytes.marker_state = EVENT_MARKER_CLEAR;
            continue;
        case KF_EVENT_STREAM_SET_CONTROL:
            cur_evt_state.control.bytes[cursor[1]] = cursor[2];
            cursor += 2;
            candidate->word_10.bytes.fallback_offset += 2;
            break;
        case KF_EVENT_STREAM_END:
            goto after_script;
        default:
            if (!restore_state && candidate->animation_id != KF_ANIMATION_CLIP_NONE) {
                s32 phase = actor->animation_phase;
                saved_state = actor->animation_id;
                if (phase != 0) {
                    actor_animation_seek_phase(actor, actor->animation_id,
                                  phase, 0, actor->animation_step);
                }
                actor_animation_seek_phase(actor, candidate->animation_id, 0,
                              KF_ACTOR_ANIMATION_PHASE_MAX,
                              candidate->animation_step);
                restore_state = KF_TRUE;
            }
            menu_show_transition_image(KF_RESOURCE_ARCHIVE_TALK, candidate->word_0c.value + *cursor);
            break;
        }

        cursor++;
        candidate->word_10.bytes.fallback_offset++;
        if (repeat != 0) {
            repeat--;
            continue;
        }
        break;
    }

after_script:
    old_counter = event_item_counters_table[KF_ENUM_ENCODE(u8, KF_OBJECT_83)];
    switch (candidate->word_12.bytes.post_stream_menu_action & KF_EVENT_POST_STREAM_MENU_MASK) {
    case KF_EVENT_POST_STREAM_BUY_SELL:
        player_render_frame_and_release_pool();
        menu_item_buy_sell_controller(candidate->word_12.bytes.post_stream_menu_action &
                                      KF_EVENT_POST_STREAM_BUY_SELL_CHOICE_MASK);
        break;
    case KF_EVENT_POST_STREAM_STOCK_CHOICE:
        player_render_frame_and_release_pool();
        menu_item_stock_choice_controller();
        break;
    case KF_EVENT_POST_STREAM_TRADE:
        player_render_frame_and_release_pool();
        menu_item_trade_controller();
        break;
    case KF_EVENT_POST_STREAM_INVENTORY_CHOICE:
        player_render_frame_and_release_pool();
        choice = menu_choose_inventory_item();
        if (choice != KF_MENU_RESULT_CANCELLED) {
            render_game_frame(NULL, NULL);
            menu_show_transition_image(KF_RESOURCE_ARCHIVE_ITEM, choice + 360);
        }
        break;
    }
    if (event_item_counters_table[KF_ENUM_ENCODE(u8, KF_OBJECT_83)] < old_counter) {
        cur_evt_state.control.fields.counter_53_decreased = 1;
    }
    cur_evt_state.control.fields.stream_actor_definition_id = actor->definition_id;
    if (restore_state && candidate->word_10.bytes.completion_animation_id != KF_ANIMATION_CLIP_NONE) {
        actor_animation_seek_phase(actor, candidate->word_10.bytes.completion_animation_id, 0,
                      KF_ACTOR_ANIMATION_PHASE_MAX,
                      candidate->word_0e.value);
        actor->animation_id = saved_state;
    }
    cur_evt_state.interaction_handled = KF_TRUE;
    player_clear_motion();
}

ADDRESS(0x80047444, 0x8c)
void event_spawn_effect_object(KfMapObject *event, KF_ENUM_PARAM(KfObjectId, s32) object_id)
{
    KfMapObject *object = map_object_effect_pool_acquire(KF_MAP_OBJECT_EVENT_POOL_FIRST,
                                                         KF_MAP_OBJECT_EVENT_POOL_SIZE, -1);

    /* The byte store wraps pool slots 0x17c..0x18b to offsets 0..15. */
    event->tail.event_effect.effect_object_index = (object - map_objects_slots.objects) - (KF_MAP_OBJECT_EVENT_POOL_FIRST - (1 << 8));
    object->object_id = object_id;
    object->tail.fields.unknown_38 = KF_MAP_OBJECT_EVENT_DISARMED;
}

enum {
    /* Control byte of an object slot with no saved object: the low byte of KF_EVENT_CONTROL_OBJECT_NONE. */
    EVENT_CONTROL_OBJECT_LOW_NONE = KF_EVENT_CONTROL_OBJECT_NONE & 0xff,
    /* Scene sound cue and its volume. */
    EVENT_SCENE_SOUND_ID = 0x8009,
    EVENT_SCENE_SOUND_VOLUME = 0x6e
};
ADDRESS(0x800474d0, 0xc54)
void event_scene_command_dispatch(const VECTOR *position,
                                  const KfPlayerViewRotation *rotation,
                                  KF_ENUM_PARAM(KfObjectId, s32) command)
{
    s32 index;
    s32 object_control_offset;
    const u8 *magic_ids;

    cur_evt_state.interaction_handled = KF_FALSE;
    switch (command) {
    case KF_OBJECT_99:
    case KF_OBJECT_100:
    case KF_OBJECT_101:
    case KF_OBJECT_102:
    case KF_OBJECT_104:
    case KF_OBJECT_106:
    case KF_OBJECT_107:
    case KF_OBJECT_108:
    case KF_OBJECT_109:
        index = 0;
        for (;;) {
            KfMapObject *object;
            KF_ENUM_STORAGE(KfMapObjectMarkerCheck, s32) status;

            index = map_object_find_interaction_target(index, position, 800, 1700,
                                   rotation->angles[1], 512);
            if (index == -1) {
                break;
            }
            object = &map_objects_slots.objects[index];
            status = map_object_check_and_consume_marker(object, KF_ENUM_ENCODE(s32, command));
            switch (status) {
            case KF_MAP_OBJECT_MARKER_CONSUMED:
                audio_play_marker_consumed_sound();
                cur_evt_state.interaction_handled = KF_TRUE;
                break;
            case KF_MAP_OBJECT_MARKER_MISMATCH:
                notify_enqueue(object->tail.notification.linked_notification);
                cur_evt_state.interaction_handled = KF_TRUE;
                break;
            case KF_MAP_OBJECT_MARKER_REFUSED:
                notify_enqueue(KF_NOTIFICATION_4);
                cur_evt_state.interaction_handled = KF_TRUE;
                break;
            case KF_MAP_OBJECT_MARKER_NOT_APPLICABLE:
            default:
                index++;
                continue;
            }
            break;
        }
        break;
    case KF_OBJECT_114:
        object_control_offset = offsetof(KfEventControlFields, object_slots[0]);
        goto object_control_action;
    case KF_OBJECT_115:
        object_control_offset = offsetof(KfEventControlFields, object_slots[1]);
        goto object_control_action;
    case KF_OBJECT_116:
        object_control_offset = offsetof(KfEventControlFields, object_slots[2]);
object_control_action:
        index = map_object_find_interaction_target(0, position, 800, 1700,
                               rotation->angles[1], 512);
        if (index != -1) {
            KfMapObject *object = &map_objects_slots.objects[index];

            if (object->object_id == KF_OBJECT_189) {
                if (object->tail.event_effect.pending_event_command != KF_OBJECT_NONE) {
                    break;
                }
                object->tail.event_effect.pending_event_command = command;
                object->action_timer = 0;
                ((KfEventControlObjectSlot *)&cur_evt_state.control.bytes[object_control_offset])->object_index =
                    object - map_objects_slots.objects;
                ((KfEventControlObjectSlot *)&cur_evt_state.control.bytes[object_control_offset])->resource_id =
                    asset_status.active_resource_ids[KF_RESOURCE_SLOT_MAP_REGION];
                cur_evt_state.interaction_handled = KF_TRUE;
                game_counter_decrement(command);
                object->extra_40.bytes[0] = KF_MAP_OBJECT_LATCH_CLEAR;
                event_spawn_effect_object(object, command);
            } else if (map_object_check_and_consume_marker(object, KF_ENUM_ENCODE(s32, command)) == KF_MAP_OBJECT_MARKER_MISMATCH) {
                notify_enqueue(object->tail.notification.linked_notification);
                cur_evt_state.interaction_handled = KF_TRUE;
            }
        }
        break;
    case KF_OBJECT_111:
        object_control_offset = offsetof(KfEventControlFields, object_slots[0]);
        goto transition_action;
    case KF_OBJECT_112:
        object_control_offset = offsetof(KfEventControlFields, object_slots[1]);
        goto transition_action;
    case KF_OBJECT_113:
        object_control_offset = offsetof(KfEventControlFields, object_slots[2]);
transition_action: {
        u8 previous_value;
        KfMapObject *object;
        struct KfVecXZi forward;
        u16 object_index;
        s16 yaw;

        /* The retail gate checks only the low byte of the saved object index. */
        if (asset_status.active_resource_ids[KF_RESOURCE_SLOT_MAP_REGION] == 7 ||
            cur_evt_state.control.bytes[object_control_offset] == EVENT_CONTROL_OBJECT_LOW_NONE ||
            cur_avatar_status.vitals.current_mp < 10) {
            break;
        }
        cur_avatar_status.vitals.current_mp -= 10;
        render_frames_with_color_overlay(KF_COLOR_OVERLAY_ADD, 0, KF_FIXED12_ONE, 256);
        actor_disable_type3_transition_actors();
        previous_value = ((KfEventControlObjectSlot *)&cur_evt_state.control.bytes[object_control_offset])->resource_id;
        do {
            cd_request_yield();
            resource_advance_transition();
        } while (asset_status.transition_active);
        if (previous_value != asset_status.active_resource_ids[KF_RESOURCE_SLOT_MAP_REGION]) {
            resource_request_transition(previous_value, previous_value, previous_value,
                          KF_RESOURCE_REQUEST_KEEP, KF_RESOURCE_REQUEST_KEEP, KF_RESOURCE_OFFSET_NO_SHIFT, KF_RESOURCE_OFFSET_NO_SHIFT, KF_RESOURCE_OFFSET_NO_SHIFT);
        } else {
            resource_request_transition(KF_RESOURCE_REQUEST_KEEP, KF_RESOURCE_REQUEST_KEEP, previous_value, KF_RESOURCE_REQUEST_KEEP, KF_RESOURCE_REQUEST_KEEP,
                          KF_RESOURCE_OFFSET_NO_SHIFT, KF_RESOURCE_OFFSET_NO_SHIFT, KF_RESOURCE_OFFSET_NO_SHIFT);
        }
        do {
            cd_request_yield();
            resource_advance_transition();
        } while (asset_status.transition_active);
        cd_request_wait_idle();

        object_index = ((KfEventControlObjectSlot *)&cur_evt_state.control.bytes[object_control_offset])->object_index;
        object = &map_objects_slots.objects[object_index];
        angle_to_forward_xz(object->rotation.vy, &forward);
        vector2i_scale_shift11(1024, &forward);
        cur_avatar_status.death_state = KF_PLAYER_REACTION_NORMAL;
        cur_avatar_status.view_rotation_offset.components[2] = 0;
        cur_avatar_status.view_rotation_offset.components[1] = 0;
        cur_avatar_status.view_rotation_offset.components[0] = 0;
        cur_avatar_status.camera_rotation = cur_avatar_status.camera_rotation_target;
        cur_avatar_status.camera_position.vx = object->position.vx + forward.x;
        cur_avatar_status.camera_position.vz = object->position.vz + forward.z;
        cur_avatar_status.camera_position.vy = object->position.vy;
        yaw = object->rotation.vy + KF_ANGLE_HALF_TURN;
        cur_evt_state.interaction_handled = KF_TRUE;
        cur_avatar_status.camera_rotation_target.angles[1] = yaw;
        cur_avatar_status.camera_rotation.angles[1] = yaw;
        render_frames_with_color_overlay(KF_COLOR_OVERLAY_ADD, KF_FIXED12_ONE, KF_FIXED12_ONE, 0);
        if (scene_display_sys.asset_registry_entries[KF_MO_ON_DEMAND_MODEL_SLOT] == NULL) {
            resource_tmd_queue_read(KF_RESOURCE_ARCHIVE_MO, KF_MO_ON_DEMAND_MODEL_ENTRY, KF_MO_ON_DEMAND_MODEL_SLOT);
        }
        render_frames_with_color_overlay(KF_COLOR_OVERLAY_ADD, KF_FIXED12_ONE, 0, -256);
        render_set_color_overlay(KF_COLOR_OVERLAY_OFF, 0, 0, 0);
        resource_request_transition(KF_RESOURCE_REQUEST_KEEP, KF_RESOURCE_REQUEST_KEEP, KF_RESOURCE_REQUEST_KEEP, previous_value, previous_value,
                      KF_RESOURCE_OFFSET_NO_SHIFT, KF_RESOURCE_OFFSET_NO_SHIFT, KF_RESOURCE_OFFSET_NO_SHIFT);
        break;
    }
    case KF_OBJECT_103:
        index = map_object_find_interaction_target(0, position, 800, 1700,
                               rotation->angles[1], 512);
        if (index != -1) {
            KfMapObject *object = &map_objects_slots.objects[index];

            if (object->object_id == KF_OBJECT_184) {
                if (object->tail.event_effect.pending_event_command == KF_OBJECT_NONE) {
                    object->tail.event_effect.pending_event_command = command;
                    object->action_timer = 0;
                    cur_evt_state.interaction_handled = KF_TRUE;
                    game_counter_decrement(command);
                    event_spawn_effect_object(object, command);
                }
            } else if (map_object_check_and_consume_marker(object, KF_ENUM_ENCODE(s32, command)) == KF_MAP_OBJECT_MARKER_MISMATCH) {
                notify_enqueue(object->tail.notification.linked_notification);
                cur_evt_state.interaction_handled = KF_TRUE;
            }
        }
        break;
    case KF_OBJECT_84:
        cur_avatar_status.map_marker_visual_effect_timer = 1200;
        game_counter_decrement(KF_OBJECT_84);
        cur_evt_state.interaction_handled = KF_TRUE;
        break;
    case KF_OBJECT_88:
        index = map_object_find_interaction_target(0, position, 800, 1700,
                               rotation->angles[1], 512);
        if (index != -1) {
            KfMapObject *object = &map_objects_slots.objects[index];

            if (object->object_id == KF_OBJECT_157) {
                map_object_apply_marker_signal(object->tail.marker.marker_id);
            }
            cur_evt_state.interaction_handled = KF_TRUE;
        }
        audio_play_sound_at_volume_100(8);
        break;
    case KF_OBJECT_89: {
        KfMapObject *scan = map_objects_slots.objects;
        KfMapObject *nearest = NULL;
        s32 nearest_distance = 999999;
        s32 remaining = KF_MAP_OBJECT_CAPACITY - 1;

        for (; remaining != -1; scan++, remaining--) {
            KfMapObject *object = scan;
            KF_ENUM_PROMOTED(KfObjectId) object_id = object->object_id;
            s32 distance;

            if (object_id < KF_OBJECT_82) {
                continue;
            }
            if (object_id >= KF_OBJECT_84) {
                if (object_id >= KF_OBJECT_97) {
                    continue;
                }
                if (object_id < KF_OBJECT_90) {
                    continue;
                }
            }
            distance = vector_distance_between_with_reach(&object->position, 25000,
                                     &cur_avatar_status.camera_position, 0, 0);
            if (distance >= 0 && distance < nearest_distance) {
                nearest = object;
                nearest_distance = distance;
            }
        }
        if (nearest != NULL) {
            VECTOR sound_position;

            /* Retail writes this adjusted vector, then passes the object position. */
            sound_position.vx = nearest->position.vx;
            sound_position.vz = nearest->position.vz;
            sound_position.vy = nearest->position.vy +
                4 * (nearest->position.vy - sound_status.listener_position.vy);
            audio_play_spatial_range(EVENT_SCENE_SOUND_ID, &nearest->position, EVENT_SCENE_SOUND_VOLUME,
                                     25000, 29000, 0);
            cur_evt_state.interaction_handled = KF_TRUE;
        }
        break;
    }
    case KF_OBJECT_90:
        magic_ids = event_magic_unlock_ids_5a;
        goto magic_action;
    case KF_OBJECT_91:
        magic_ids = event_magic_unlock_ids_5b;
        goto magic_action;
    case KF_OBJECT_92:
        magic_ids = event_magic_unlock_ids_5c;
        goto magic_action;
    case KF_OBJECT_93:
        magic_ids = event_magic_unlock_ids_5d;
        goto magic_action;
    case KF_OBJECT_94:
        magic_ids = event_magic_unlock_ids_5e;
magic_action: {
        KfMapObject *object;
        VECTOR near_position;
        VECTOR far_position;
        s32 fraction;
        s32 spin;
        KF_ENUM_PROMOTED(KfEffectKind) magic_id;
        KfMagicRecord *magic_record;

        for (;;) {
            magic_id = KF_ENUM_DECODE(KF_ENUM_PROMOTED(KfEffectKind), *magic_ids++);
            if (magic_id == KF_MAGIC_NONE) {
                goto invoke_callback;
            }
            magic_record = &effect_status.magic_records[KF_ENUM_ENCODE(s32, magic_id)];
            if (magic_record->menu_available == 0) {
                magic_record->menu_available = 1;
                break;
            }
        }
        object = map_object_effect_pool_acquire(
            KF_MAP_OBJECT_SCATTER_POOL_FIRST, KF_MAP_OBJECT_EFFECT_POOL_SIZE,
            map_objects_slots.spawn_sequence_pool_15e);
        map_object_reset(object);
        game_counter_decrement(command);
        object->object_id = command;
        object->render_queue_mode = KF_RENDER_QUEUE_BLEND_ADD;
        object->layer_mask = KF_MAP_LAYER_BOTH;
        object->action = KF_MAP_OBJECT_OP_NONE;
        object->lighting_override_index = KF_LIGHTING_PRESET_42;
        object->rotation.vz = 0;
        object->rotation.vy = 0;
        object->rotation.vx = 0;

        scene_position_from_camera_offset(0, 200, 1500, cur_avatar_status.camera_rotation.angles[0],
                      cur_avatar_status.camera_rotation.angles[1], 0, 0,
                      &far_position);
        scene_position_from_camera_offset(0, 200, 1000, cur_avatar_status.camera_rotation.angles[0],
                      cur_avatar_status.camera_rotation.angles[1], 0, 0,
                      &near_position);
        spin = 0;
        for (fraction = 0; fraction < KF_FIXED12_ONE; fraction += 64) {
            scene_pose_interpolate(object, &near_position,
                          &far_position, NULL, NULL, fraction);
            object->rotation.vy += spin;
            spin += 4;
            object->lighting_blend_q12 = (rsin(fraction << 6) >> 2) + 1024;
            cd_request_service_vab();
            cd_request_service_stream();
            render_game_frame(NULL, NULL);
        }
        /* The first decay step precedes service; later steps follow it. */
        goto decay_update;
        for (;;) {
            cd_request_service_vab();
            cd_request_service_stream();
            render_game_frame(NULL, NULL);
decay_update:
            object->lighting_blend_q12 -= 64;
            object->rotation.vy += spin;
            spin += 8;
            if ((s16)object->lighting_blend_q12 <= 0) {
                break;
            }
        }
        object->lighting_blend_q12 = 0;
        for (;;) {
            object->rotation.vy += spin;
            object->lighting_blend_q12 += 128;
            spin += 8;
            if (object->lighting_blend_q12 >= KF_FIXED12_ONE) {
                break;
            }
            cd_request_service_vab();
            cd_request_service_stream();
            render_game_frame(NULL, NULL);
        }
        object->object_id = KF_OBJECT_NONE;
        notify_enqueue(KF_NOTIFICATION_1);
        cur_evt_state.interaction_handled = KF_TRUE;
        break;
    }
    case KF_OBJECT_82:
        if (collision_probe_forward_shape_0x20(position,
                           (const struct KfEulerAngles *)rotation) == 0) {
            index = map_object_find_interaction_target(0, position, 800, 1700,
                                   rotation->angles[1], 512);
            if (index == -1) {
                break;
            }
            if (map_objects_slots.objects[index].object_id != KF_OBJECT_195) {
                break;
            }
        }
        if (!game_counter_increment(KF_ITEM_RESTORE_HP_100)) {
            notify_enqueue(KF_NOTIFICATION_22);
            game_counter_decrement(KF_OBJECT_82);
        }
        cur_evt_state.interaction_handled = KF_TRUE;
        break;
    case KF_OBJECT_85: {
        KF_ENUM_STORAGE(KfMapLayerMask, s32) side =
            cur_avatar_status.map_layer_index == 0 ? KF_MAP_LAYER_FIRST : KF_MAP_LAYER_SECOND;
        s32 actor_distance;
        KfActor *actor = actor_find_best_in_cone(position, rotation->angles[1],
                                       rotation->angles[0], 8000, 500, 500,
                                       &actor_distance, -1);

        if (actor != NULL && actor->current_map_layer == side) {
            menu_show_transition_image(KF_RESOURCE_ARCHIVE_ITEM, actor->definition_id + 240);
            cur_evt_state.interaction_handled = KF_TRUE;
            break;
        }
        {
            s32 remaining;
            KfMapObject *scan;

            for (scan = map_objects_slots.objects,
                 remaining = KF_MAP_OBJECT_CAPACITY - 1;
                 remaining != -1; remaining--, scan++) {
                KfMapObject *object = scan;

                if (map_objects_slots.templates[KF_ENUM_ENCODE(u16,
                                                              object->object_id)].collision_kind != KF_MAP_OBJECT_OP_SCENE_INSPECT ||
                    object->extra_40.saved_layer.layer_mask != side) {
                    continue;
                }
                if (player_camera_within_map_region(object->position.vx >> KF_MAP_CELL_POSITION_SHIFT,
                                  object->position.vz >> KF_MAP_CELL_POSITION_SHIFT,
                                  object->tail.scene_inspect.region_width,
                                  object->tail.scene_inspect.region_depth, KF_MAP_REGION_HEIGHT_ANY)) {
                    menu_show_transition_image(KF_RESOURCE_ARCHIVE_ITEM, object->tail.scene_inspect.transition_image_id + 510);
                    cur_evt_state.interaction_handled = KF_TRUE;
                    break;
                }
            }
        }
        break;
    }
    case KF_OBJECT_86:
        game_counter_decrement(KF_OBJECT_86);
        cur_avatar_status.full_mp_timer = 900;
        cur_evt_state.interaction_handled = KF_TRUE;
        break;
    case KF_OBJECT_87:
        game_counter_decrement(KF_OBJECT_87);
        cur_avatar_status.magic_boost_timer = 900;
        cur_evt_state.interaction_handled = KF_TRUE;
        player_recalculate_combat_stats();
        break;
    default:
        /* IDs 83, 95..98, 105 and 110 have no action. */
        break;
    }

invoke_callback:
    ((KfEventCommandCallback)asset_status.active_table[2])(
        position, rotation, command);
    if (!cur_evt_state.interaction_handled) {
        notify_enqueue(KF_NOTIFICATION_NOTHING_HAPPENS);
    }
    player_clear_motion();
}
ADDRESS(0x80048124, 0x54)
b32 game_counter_decrement(KF_ENUM_PARAM(KfObjectId, s32) index)
{
    if (event_item_counters_table[KF_ENUM_ENCODE(s32, index)] != 0) {
        event_item_counters_table[KF_ENUM_ENCODE(s32, index)]--;
        return KF_FALSE;
    }

    return KF_TRUE;
}

ADDRESS(0x80048178, 0x90)
b32 game_counter_increment(KF_ENUM_PARAM(KfObjectId, s32) index)
{
    if (event_item_counters_table[KF_ENUM_ENCODE(s32, index)] < 99) {
        event_item_counters_table[KF_ENUM_ENCODE(s32, index)]++;
        asset_status.active_table[6]();
        return KF_FALSE;
    }

    notify_enqueue(KF_NOTIFICATION_CANNOT_CARRY_MORE);
    return KF_TRUE;
}

ADDRESS(0x80048208, 0x114)
void color_overlay_transition(s32 step, s32 first, s32 second, s32 third,
                              s32 target_first, s32 target_second,
                              s32 target_third)
{
    s32 fraction = 0;

    do {
        s32 current_first;
        s32 current_second;
        s32 current_third;

        reset_collision_rows_and_overlay();
        current_first = fixed_lerp_q12(first, target_first, fraction);
        current_second = fixed_lerp_q12(second, target_second, fraction);
        current_third = fixed_lerp_q12(third, target_third, fraction);
        accumulate_color_overlay(current_first, current_second, current_third, KF_FIXED12_ONE / 2);
        render_game_frame(NULL, NULL);
        fraction += step;
    } while (fraction < KF_FIXED12_ONE);

    reset_collision_rows_and_overlay();
    accumulate_color_overlay(target_first, target_second, target_third, KF_FIXED12_ONE / 2);
    render_game_frame(NULL, NULL);
}


enum {
    /* Camera yaw bounds used by the pose preview. */
    EVENT_POSE_YAW_SNAP_LOW = 0x100,
    EVENT_POSE_YAW_SNAP_HIGH = 0xf00,
    /* Per-frame spin, and hold progress that saturates at EVENT_POSE_HOLD_LIMIT. */
    EVENT_POSE_SPIN_STEP = 0x40,
    EVENT_POSE_HOLD_STEP = 0x40,
    EVENT_POSE_HOLD_LIMIT = 0x100,
    /* Return-to-rest angle tolerance and per-frame rotation step. */
    EVENT_POSE_RETURN_TOLERANCE = 0x80,
    EVENT_POSE_RETURN_STEP = 0x100
};
ADDRESS(0x8004831c, 0x6e0)
void event_map_object_interact(KfMapObject *object, ...)
{
    va_list arguments;
    s32 spawn_object_id;
    s32 spawned_id = -1;
    const KfMapObjectTemplate *object_template;
    SVECTOR next_angles;
    SVECTOR first_angles;
    VECTOR next_position;
    VECTOR first_position;
    s32 first_yaw;
    s32 target_yaw;
    s16 initial_render_depth_offset;
    s16 target_render_depth_offset;
    s32 fraction;
    u32 previous_buttons;
    u32 buttons; /* reused as the remove-object flag after the button wait */
    s32 hold_steps = 0;

    if (object == NULL) {
        va_start(arguments, object);
        spawn_object_id = va_arg(arguments, s32);
        va_end(arguments);
        spawned_id = spawn_object_id;
        object = map_object_effect_pool_acquire(
            KF_MAP_OBJECT_SCATTER_POOL_FIRST, KF_MAP_OBJECT_EFFECT_POOL_SIZE,
            map_objects_slots.spawn_sequence_pool_15e);
        map_object_reset(object);
        object->object_id = KF_ENUM_DECODE(KF_ENUM_PROMOTED(KfObjectId), spawn_object_id);
        object->layer_mask = KF_MAP_LAYER_BOTH;
        object->action = KF_MAP_OBJECT_OP_NONE;
        object->rotation.vz = 0;
        object->rotation.vy = 0;
        object->rotation.vx = 0;
        object->tail.fields.unknown_38 = KF_MAP_OBJECT_EVENT_ARMED;
    }

    object_template = &map_objects_slots.templates[KF_ENUM_ENCODE(u16, object->object_id)];
    initial_render_depth_offset = object->render_depth_offset;
    if (object->tail.fields.unknown_38 != KF_MAP_OBJECT_EVENT_ARMED) {
        return;
    }
    if (object_template->kind == KF_MAP_OBJECT_KIND_GOLD) {
        notify_enqueue(KF_NOTIFICATION_PAYLOAD_ID, object->tail.gold_reward.gold_amount);
        object->object_id = KF_OBJECT_NONE;
        cur_avatar_status.gold += object->tail.gold_reward.gold_amount;
        return;
    }

    if (object->action == KF_MAP_OBJECT_OP_OFFSET_MOTION && object->object_id == KF_OBJECT_103) {
        KfMapObject *child = &map_objects_slots.objects[object->extra_40.object_index];
        KfMapObject *next = &map_objects_slots.objects[
            child->tail.fields.unknown_3a.bytes.high];
        if (next->action == KF_MAP_OBJECT_OP_SIGNAL_DOOR && next->action_timer != 0) {
            return;
        }
    }
    if (object->object_id == KF_OBJECT_18) {
        object->object_id = KF_OBJECT_13;
    }

    first_yaw = cur_avatar_status.camera_rotation.angles[0] & KF_ANGLE_WRAP_MASK;
    target_yaw = first_yaw;
    if (first_yaw < KF_ANGLE_HALF_TURN) {
        if (first_yaw > EVENT_POSE_YAW_SNAP_LOW) {
            target_yaw = EVENT_POSE_YAW_SNAP_LOW;
        }
    } else if (first_yaw < EVENT_POSE_YAW_SNAP_HIGH) {
        target_yaw = EVENT_POSE_YAW_SNAP_HIGH;
    }
    previous_buttons = PadRead(1);
    target_render_depth_offset = (-object_template->params.pose.depth_offset) / 4 - 200;

    if (spawned_id != -1) {
        scene_position_from_camera_offset(0, 500, 1500, target_yaw,
                      cur_avatar_status.camera_rotation.angles[1],
                      object_template->params.pose.height_offset,
                      object_template->params.pose.depth_offset,
                      &object->position);
    } else {
        first_angles = object->rotation;
        first_position = object->position;
        scene_position_from_camera_offset(0, 500, 1500, target_yaw,
                      cur_avatar_status.camera_rotation.angles[1],
                      object_template->params.pose.height_offset,
                      object_template->params.pose.depth_offset,
                      &next_position);
        next_angles.vz = 0;
        next_angles.vx = 0;
        /* Retail copies the unassigned button word here. hold_steps starts at 0
           and the loop breaks only at 0x100, so this value is never observed. */
        for (fraction = 0, previous_buttons = buttons; fraction <= KF_FIXED12_ONE;
             fraction += KF_FIXED12_ONE / 8) {
            scene_pose_interpolate(object, &first_position,
                          &next_position, &first_angles, &next_angles,
                          fraction);
            object->render_depth_offset = value_approach(
                initial_render_depth_offset, target_render_depth_offset, fraction);
            cur_avatar_status.camera_rotation.angles[0] = angle_lerp_shortest_q12(
                first_yaw, target_yaw, fraction);
            cd_request_service_vab();
            cd_request_service_stream();
            render_game_frame(NULL, (const SVECTOR *)&cur_avatar_status.camera_rotation);
        }
    }
    object->render_depth_offset = target_render_depth_offset;

    for (;;) {
        buttons = PadRead(1);
        if (previous_buttons == 0 && buttons != 0 && hold_steps == EVENT_POSE_HOLD_LIMIT) {
            break;
        }
        previous_buttons = buttons;
        object->rotation.vy += EVENT_POSE_SPIN_STEP;
        hold_steps = hold_steps < EVENT_POSE_HOLD_LIMIT ? hold_steps + EVENT_POSE_HOLD_STEP : hold_steps;
        cd_request_service_vab();
        cd_request_service_stream();
        render_game_frame(NULL, (const SVECTOR *)&cur_avatar_status.camera_rotation);
    }

button_pressed:
    if ((buttons & DAT_8006e568) == 0 && spawned_id == -1) {
        goto return_pose;
    }
    if (!game_counter_increment(object->object_id)) {
        switch (object->object_id) {
        case KF_OBJECT_117: {
            s32 amount = ((rand() * 6) >> 15) + 4;
            if (event_item_counters_table[KF_ENUM_ENCODE(u8, KF_OBJECT_117)] < 99 - amount) {
                event_item_counters_table[KF_ENUM_ENCODE(u8, KF_OBJECT_117)] += amount;
            } else {
                event_item_counters_table[KF_ENUM_ENCODE(u8, KF_OBJECT_117)] = 99;
            }
            break;
        }
        case KF_OBJECT_118:
            if (event_item_counters_table[KF_ENUM_ENCODE(u8, KF_OBJECT_118)] < 95) {
                event_item_counters_table[KF_ENUM_ENCODE(u8, KF_OBJECT_118)] += 4;
            } else {
                event_item_counters_table[KF_ENUM_ENCODE(u8, KF_OBJECT_118)] = 99;
            }
            break;
        default:
            break;
        }
        buttons = 1;
        scene_position_from_camera_offset(-500, 500, 0, target_yaw,
                      cur_avatar_status.camera_rotation.angles[1], 0, 0,
                      &first_position);
        first_angles = object->rotation;
        initial_render_depth_offset = 0;
        goto interpolate_back;
    }
    if (spawned_id != -1) {
        object->object_id = KF_OBJECT_NONE;
        goto finish;
    }

return_pose:
    buttons = 0;
    while (!angle_within_tolerance(object->rotation.vy,
                                   first_angles.vy, EVENT_POSE_RETURN_TOLERANCE)) {
        object->rotation.vy = (object->rotation.vy + EVENT_POSE_RETURN_STEP) & KF_ANGLE_WRAP_MASK;
        render_game_frame(NULL, (const SVECTOR *)&cur_avatar_status.camera_rotation);
    }
    object->rotation.vy = first_angles.vy;

interpolate_back:
    next_angles = object->rotation;
    next_position = object->position;
    target_yaw = cur_avatar_status.camera_rotation.angles[0];
    for (fraction = 0; fraction <= KF_FIXED12_ONE; fraction += KF_FIXED12_ONE / 8) {
        scene_pose_interpolate(object, &next_position,
                      &first_position, &next_angles, &first_angles,
                      fraction);
        cur_avatar_status.camera_rotation.angles[0] = angle_lerp_shortest_q12(
            target_yaw, first_yaw, fraction);
        object->render_depth_offset = value_approach(
            target_render_depth_offset, initial_render_depth_offset, fraction);
        render_game_frame(NULL, (const SVECTOR *)&cur_avatar_status.camera_rotation);
    }
    if (buttons) {
        object->object_id = KF_OBJECT_NONE;
    } else if (object->object_id == KF_OBJECT_13) {
        object->object_id = KF_OBJECT_18;
    }
finish:
    player_clear_motion();
}

enum {
    /* Full-intensity colour channel, used as the third colour of each flash. */
    EVENT_FLASH_CHANNEL_MAX = 0xff,
    EVENT_HEAL_FLASH_FIRST = 0x80,
    EVENT_HEAL_FLASH_SECOND = 0xa0,
    EVENT_RESTORE_FLASH_FIRST = 0x80,
    EVENT_RESTORE_FLASH_SECOND = 0xc8,
    /* Actor overlap probe radius and height. */
    EVENT_INTERACT_ACTOR_RADIUS = 0x578,
    EVENT_INTERACT_ACTOR_HEIGHT = 0xc80,
    /* Angle gate for hinged and sliding container interactions. */
    EVENT_HINGED_CONTAINER_ANGLE_TOLERANCE = 0x155,
    /* Added to a screen-image value to select the item archive image. */
    EVENT_SCREEN_IMAGE_ID_BASE = 0x78
};
ADDRESS(0x800489fc, 0x660)
void event_world_dispatch_interaction(const VECTOR *position,
                                      const KfPlayerViewRotation *rotation)
{
    VECTOR probe;
    s32 object_index;
    KfMapObject *objects;
    KfMapObjectTemplate *templates;

    probe.vx = position->vx;
    probe.vy = position->vy + 500;
    probe.vz = position->vz;
    cur_evt_state.interaction_handled = KF_FALSE;
    if (collision_probe_forward_shape_0x20(&probe, (const struct KfEulerAngles *)rotation)) {
        color_overlay_transition(KF_FIXED12_ONE / 4, 0, 0, 0, EVENT_HEAL_FLASH_FIRST, EVENT_HEAL_FLASH_SECOND, EVENT_FLASH_CHANNEL_MAX);
        cur_avatar_status.vitals.current_hp = cur_avatar_status.vitals.maximum_hp;
        color_overlay_transition(KF_FIXED12_ONE / 4, EVENT_HEAL_FLASH_FIRST, EVENT_HEAL_FLASH_SECOND, EVENT_FLASH_CHANNEL_MAX, 0, 0, 0);
    }

    object_index = actor_find_overlap_excluding_target_type3(probe.vx, probe.vy, probe.vz, EVENT_INTERACT_ACTOR_RADIUS, EVENT_INTERACT_ACTOR_HEIGHT);
    if (object_index != KF_ACTOR_INDEX_NONE) {
        KfActor *actor = &actor_status.actors[object_index];
        s32 angle = vector_xz_to_angle(actor->position.vx - probe.vx,
                                       actor->position.vz - probe.vz);
        if (angle_within_tolerance(rotation->angles[1], angle, 300)) {
            event_target_stream_execute(actor);
        }
    }

    object_index = 0;
    objects = map_objects_slots.objects;
    templates = map_objects_slots.templates;
    for (;; object_index++) {
        KfMapObject *object;
        KF_ENUM_STORAGE(KfObjectId, u16) object_id;
        KF_ENUM_PROMOTED(KfMapObjectOperation) kind;

        object_index = map_object_find_interaction_target(object_index, &probe, 800, 2500,
                                      rotation->angles[1], 512);
        if (object_index == -1) {
            break;
        }
        /* Dead read, superseded by the template kind below. */
        kind = objects[object_index].action;
        object = &objects[object_index];
        object_id = object->object_id;
        cur_evt_state.interaction_handled = KF_TRUE;
        kind = templates[KF_ENUM_ENCODE(u16, object_id)].collision_kind;
        switch (kind) {
        case KF_MAP_OBJECT_OP_EFFECT_EMITTER:
        case KF_MAP_OBJECT_OP_NONE:
            if (object->tail.notification.default_notification != KF_NOTIFICATION_NONE) {
                notify_enqueue(object->tail.notification.default_notification);
            }
            break;
        case KF_MAP_OBJECT_OP_ITEM_PICKUP:
            event_map_object_interact(object);
            if (object->object_id == KF_OBJECT_NONE) {
                goto invoke_callback;
            }
            break;
        case KF_MAP_OBJECT_OP_ITEM_CONTAINER:
        case KF_MAP_OBJECT_OP_HIDDEN_ITEM_CONTAINER: {
            u16 linked_index = object->tail.linked_property.linked_object_index;
            if (linked_index == KF_MAP_OBJECT_INDEX_NONE ||
                objects[linked_index].object_id == KF_OBJECT_NONE) {
                notify_enqueue(object->tail.notification.default_notification);
                break;
            }
            {
                KfMapObject *linked = &objects[linked_index];
                KfMapLayerMask linked_state = object->extra_40.saved_layer.layer_mask;
                KF_ENUM_STORAGE(KfObjectId, u16) result_id;
                linked->tail.fields.unknown_38 = KF_MAP_OBJECT_EVENT_ARMED;
                linked->layer_mask = linked_state;
                event_map_object_interact(linked);
                result_id = linked->object_id;
                linked->layer_mask = KF_MAP_LAYER_NONE;
                linked->tail.fields.unknown_38 = KF_MAP_OBJECT_EVENT_DISARMED;
                if (result_id == KF_OBJECT_NONE) {
                    object->tail.linked_property.linked_object_index = KF_MAP_OBJECT_INDEX_NONE;
                }
            }
            break;
        }
        case KF_MAP_OBJECT_OP_SWITCH:
            if (object->action_timer == 0) {
                object->action_timer = 1;
            }
            break;
        case KF_MAP_OBJECT_OP_LIFT_DOOR:
            if (object->action_timer == 0) {
                if (object->tail.marker.marker_id == KF_MAP_OBJECT_MARKER_CLEARED) {
                    object->action_timer = 1;
                } else {
                    notify_enqueue(object->tail.notification.default_notification);
                }
            }
            break;
        case KF_MAP_OBJECT_OP_SIGNAL_DOOR:
        case KF_MAP_OBJECT_OP_HINGE:
            if (object->action_timer == 0) {
                if (object->tail.marker.marker_id >= KF_MAP_OBJECT_MARKER_OPEN_SIDES_FIRST &&
                    (((object->tail.marker.marker_id & KF_MAP_OBJECT_MARKER_OPEN_FRONT) &&
                      angle_within_tolerance(rotation->angles[1],
                                             object->rotation.vy, 900)) ||
                     ((object->tail.marker.marker_id & KF_MAP_OBJECT_MARKER_OPEN_BACK) &&
                      angle_within_tolerance(rotation->angles[1],
                                             object->rotation.vy + KF_ANGLE_HALF_TURN,
                                             900)))) {
                    object->action_timer = 1;
                    break;
                }
                if (object->tail.marker.marker_id == KF_ENUM_ENCODE(u8, KF_OBJECT_15)
                    && event_item_counters_table[KF_ENUM_ENCODE(u8, KF_OBJECT_15)] != 0) {
                    object->action_timer = 1;
                    break;
                }
                notify_enqueue(object->tail.notification.default_notification);
            }
            break;
        case KF_MAP_OBJECT_OP_ANIMATED_HAZARD:
            if (object->action_timer == 0 &&
                object->tail.collision_probe.marker_trigger_state == KF_MAP_OBJECT_PROBE_RUNNING) {
                object->action_timer = 1;
            }
            break;
        case KF_MAP_OBJECT_OP_HINGED_CONTAINER:
        case KF_MAP_OBJECT_OP_SLIDING_CONTAINER:
            if (!angle_within_tolerance(rotation->angles[1],
                                        object->rotation.vy + KF_ANGLE_HALF_TURN, EVENT_HINGED_CONTAINER_ANGLE_TOLERANCE)) {
                break;
            }
            /* Kind five enters the same state handler without the angle gate. */
        case KF_MAP_OBJECT_OP_ANIMATED_CONTAINER:
            switch (object->tail.marker.marker_id) {
            case KF_MAP_OBJECT_MARKER_TRIGGERED: {
                u16 linked_index = object->tail.linked_property.linked_object_index;
                if (linked_index != KF_MAP_OBJECT_INDEX_NONE) {
                    goto check_linked_object;
                }
            notify_six:
                notify_enqueue(KF_NOTIFICATION_6);
                break;
            check_linked_object:
                if (objects[linked_index].object_id == KF_OBJECT_NONE) {
                    goto notify_six;
                }
                break;
            }
            case KF_MAP_OBJECT_MARKER_CLEARED:
                object->tail.marker.marker_id = KF_MAP_OBJECT_MARKER_TRIGGERED;
                break;
            default:
                notify_enqueue(object->tail.notification.default_notification);
                break;
            }
            break;
        case KF_MAP_OBJECT_OP_RECALL_SOCKET:
            if (object->tail.event_effect.pending_event_command == KF_OBJECT_NONE) {
                notify_enqueue(KF_NOTIFICATION_16);
            }
            break;
        case KF_MAP_OBJECT_OP_PLAYER_REACTION:
            if (cur_avatar_status.death_state == KF_PLAYER_REACTION_NORMAL) {
                KfMapObjectRecord40 *record;
                player_begin_view_reaction(object_index);
                record = object->extra_40.record;
                if (record->reaction_mode == KF_MAP_OBJECT_REACTION_ARMED) {
                    record->reaction_mode = KF_MAP_OBJECT_REACTION_TRIGGERED;
                }
            }
            break;
        case KF_MAP_OBJECT_OP_SCREEN_IMAGE:
        case KF_MAP_OBJECT_OP_HIDDEN_SCREEN_IMAGE:
            menu_show_transition_image(KF_RESOURCE_ARCHIVE_ITEM, object->tail.pair_38.value_38 + EVENT_SCREEN_IMAGE_ID_BASE);
            break;
        case KF_MAP_OBJECT_OP_RESTORE_POINT:
            color_overlay_transition(KF_FIXED12_ONE / 8, 0, 0, 0, EVENT_RESTORE_FLASH_FIRST, EVENT_RESTORE_FLASH_SECOND, EVENT_FLASH_CHANNEL_MAX);
            cur_avatar_status.vitals.current_hp = cur_avatar_status.vitals.maximum_hp;
            color_overlay_transition(KF_FIXED12_ONE / 8, EVENT_RESTORE_FLASH_FIRST, EVENT_RESTORE_FLASH_SECOND, EVENT_FLASH_CHANNEL_MAX, 0, 0, 0);
            break;
        case KF_MAP_OBJECT_OP_SAVE_POINT:
            event_world_state_save_slot(asset_status.active_resource_ids[KF_RESOURCE_SLOT_MAP_REGION]);
            player_render_frame_and_release_pool();
            menu_card_save_browser();
            break;
        }
    }

invoke_callback:
    ((void (*)(const VECTOR *, const KfPlayerViewRotation *))
        asset_status.active_table[0])(&probe, rotation);
}


enum {
    KF_EVENT_WORLD_SAVE_ACTION_60 = 0xf0,
    KF_EVENT_WORLD_SAVE_ACTION_61 = 0xf1,
    KF_EVENT_WORLD_SAVE_ACTION_62 = 0xf2,
    KF_EVENT_WORLD_SAVE_ACTION_70 = 0xf3,
    KF_EVENT_WORLD_SAVE_EFFECT = 0xf4,
    KF_EVENT_WORLD_SAVE_STATE_BYTE = 0xfd,
    KF_EVENT_WORLD_SAVE_UNCHANGED = 0xfe,
    KF_EVENT_WORLD_SAVE_EMPTY = 0xff,
    /* Ends the saved actor-lifecycle and event-group lists. */
    KF_EVENT_WORLD_SAVE_LIST_END = 0xff
};



DATA(0x801b3084, 0x3918, ".bss")
KfEventState cur_evt_state;

ADDRESS(0x8004905c, 0xb0)
void event_state_initialize(void)
{
    u16 *offset;
    s32 index;
    KfEventControlObjectSlot *object_slots;

    repeat_store_word(cur_evt_state.control.clear_words, 0,
                      sizeof(cur_evt_state.control.clear_words) / sizeof(u32));
    repeat_store_word(cur_evt_state.arena.clear_words, 0,
                      sizeof(cur_evt_state.arena.clear_words) / sizeof(u32));
    object_slots = cur_evt_state.control.fields.object_slots;
    object_slots[2].object_index = KF_EVENT_CONTROL_OBJECT_NONE;
    object_slots[1].object_index = KF_EVENT_CONTROL_OBJECT_NONE;
    object_slots[0].object_index = KF_EVENT_CONTROL_OBJECT_NONE;
    memory_arena_initialize_blocks(&cur_evt_state.arena.first_block,
                                   sizeof(cur_evt_state.arena.bytes));
    offset = cur_evt_state.saved_offsets;
    for (index = KF_EVENT_SAVED_SLOT_COUNT - 1; index != -1; index--) {
        *offset++ = KF_EVENT_SAVED_OFFSET_NONE;
    }
    repeat_store_word((u32 *)event_item_counters_table, 0,
                      sizeof(event_item_counters_table) / sizeof(u32));
    event_item_counters_table[KF_ENUM_ENCODE(u8, KF_OBJECT_0)] = 1;
}

ADDRESS(0x8004910c, 0x30)
void callback_invoke_slot_04_zero(void)
{
    ((void (*)(s32))asset_status.active_table[1])(0);
}

ADDRESS(0x8004913c, 0x50)
void event_saved_offsets_decode(u8 **pointers)
{
    u16 *offset = cur_evt_state.saved_offsets;
    s32 index = KF_EVENT_SAVED_SLOT_COUNT - 1;
    u16 absent = KF_EVENT_SAVED_OFFSET_NONE;

    for (; index != -1; index--) {
        u16 value = *offset++;
        if (value == absent) {
            *pointers = NULL;
        } else {
            *pointers = value + cur_evt_state.arena.bytes;
        }
        pointers++;
    }
}

ADDRESS(0x8004918c, 0x70)
void event_arena_owner_pointers_add_delta(s32 delta)
{
    KfMemoryBlock *block = &cur_evt_state.arena.first_block;

    if (block->kind != KF_MEMORY_BLOCK_END) {
        do {
            KF_ENUM_PROMOTED(KfMemoryBlockKind) kind = block->kind;
            u32 step;
            if (kind < KF_MEMORY_BLOCK_KIND_COUNT) {
                if (kind != KF_MEMORY_BLOCK_FREE) {
                    block->owner = (u8 **)((u8 *)block->owner + delta);
                }
            }
            step = block->size + sizeof(*block);
            block = (KfMemoryBlock *)((u8 *)block + step);
        } while (block->kind != KF_MEMORY_BLOCK_END);
    }
}

ADDRESS(0x800491fc, 0x4c)
void event_saved_offsets_encode(u8 **pointers)
{
    u16 *offset = cur_evt_state.saved_offsets;
    s32 index;

    for (index = KF_EVENT_SAVED_SLOT_COUNT - 1; index != -1; index--) {
        u8 *value = *pointers;
        pointers++;
        if (value == NULL) {
            *offset = KF_EVENT_SAVED_OFFSET_NONE;
        } else {
            *offset = (u16)(value - cur_evt_state.arena.bytes);
        }
        offset++;
    }
}

ADDRESS(0x80049248, 0x70)
void event_arena_owner_pointers_subtract_delta(s32 delta)
{
    KfMemoryBlock *block = &cur_evt_state.arena.first_block;

    if (block->kind != KF_MEMORY_BLOCK_END) {
        do {
            KF_ENUM_PROMOTED(KfMemoryBlockKind) kind = block->kind;
            u32 step;
            if (kind < KF_MEMORY_BLOCK_KIND_COUNT) {
                if (kind != KF_MEMORY_BLOCK_FREE) {
                    block->owner = (u8 **)((u8 *)block->owner - delta);
                }
            }
            step = block->size + sizeof(*block);
            block = (KfMemoryBlock *)((u8 *)block + step);
        } while (block->kind != KF_MEMORY_BLOCK_END);
    }
}

ADDRESS(0x800492b8, 0x458)
void event_world_state_save_slot(s32 save_slot)
{
    u8 *saved[KF_EVENT_SAVED_SLOT_COUNT];
    u8 payload[3072];
    u8 *write = payload;
    KfActor *actor = actor_status.actors;
    KfTargetGroup *group;
    KfMapObject *object;
    s32 index;
    s32 size;
    u8 *block;

    for (index = 0; index < KF_ACTOR_CAPACITY; actor++, index++) {
        if (actor->slot_state != KF_ACTOR_SLOT_FREE &&
            actor->slot_state == KF_ACTOR_SLOT_PERSISTENT) {
            *write++ = index;
            if (actor->lifecycle == KF_ACTOR_LIFECYCLE_DISABLED) {
                *write = KF_ENUM_ENCODE(u8, KF_ACTOR_LIFECYCLE_DISABLED);
            } else {
                *write = KF_ENUM_ENCODE(u8, KF_ACTOR_LIFECYCLE_DORMANT);
            }
            write++;
        }
    }
    *write++ = KF_EVENT_WORLD_SAVE_LIST_END;

    group = actor_status.target_groups;
    for (index = 0; index < (s32)(sizeof(actor_status.target_groups) / sizeof(actor_status.target_groups[0]));
         group++, index++) {
        KfTargetCandidate *candidate;
        if (group->definition_id == KF_TARGET_GROUP_DEFINITION_END) {
            break;
        }
        candidate = group->targets[0].pointer;
        if (candidate != NULL && candidate->type == KF_ACTOR_TARGET_EVENT_STREAM) {
            *write++ = index;
            *write++ = candidate->word_10.bytes.fallback_offset;
            *write++ = candidate->word_12.bytes.marker_state;
        }
    }
    *write++ = KF_EVENT_WORLD_SAVE_LIST_END;

    object = map_objects_slots.objects;
    for (index = 0; index < KF_MAP_OBJECT_CAPACITY; object++, index++) {
        KF_ENUM_PROMOTED(KfObjectId) object_id = object->object_id;
        KF_ENUM_PROMOTED(KfMapObjectOperation) kind;
        if (object_id == KF_OBJECT_NONE) {
            *write++ = KF_EVENT_WORLD_SAVE_EMPTY;
            continue;
        }
        kind = map_objects_slots.templates[KF_ENUM_ENCODE(s32, object_id)].collision_kind;
        /* Save packets carry the low byte of the 16-bit template ID. */
        switch (kind) {
        case KF_MAP_OBJECT_OP_ITEM_PICKUP:
            switch (object->action) {
            case KF_MAP_OBJECT_OP_FALL_AND_TIP:
                *write++ = KF_EVENT_WORLD_SAVE_ACTION_60;
                *write++ = KF_ENUM_ENCODE(u8, object->object_id);
                *write++ = (u32)object->position.vx >> 2;
                *write++ = (u32)object->position.vx >> 10;
                *write++ = (u32)object->position.vz >> 2;
                *write++ = (u32)object->position.vz >> 10;
                *write++ = object->position.vy;
                *write++ = (u32)object->position.vy >> 8;
                *write++ = object->rotation.vy >> 4;
                break;
            case KF_MAP_OBJECT_OP_FALL_AND_SPIN:
                *write++ = KF_EVENT_WORLD_SAVE_ACTION_61;
                *write++ = KF_ENUM_ENCODE(u8, object->object_id);
                *write++ = (u32)object->position.vx >> 2;
                *write++ = (u32)object->position.vx >> 10;
                *write++ = (u32)object->position.vz >> 2;
                *write++ = (u32)object->position.vz >> 10;
                *write++ = object->position.vy;
                *write++ = (u32)object->position.vy >> 8;
                break;
            case KF_MAP_OBJECT_OP_BOUNCE:
                *write++ = KF_EVENT_WORLD_SAVE_ACTION_62;
                *write++ = KF_ENUM_ENCODE(u8, object->object_id);
                *write++ = (u32)object->position.vx >> 2;
                *write++ = (u32)object->position.vx >> 10;
                *write++ = (u32)object->position.vz >> 2;
                *write++ = (u32)object->position.vz >> 10;
                *write++ = object->position.vy;
                *write++ = (u32)object->position.vy >> 8;
                *write++ = object->tail.fields.unknown_3a.value >> 2;
                break;
            case KF_MAP_OBJECT_OP_OFFSET_MOTION:
                *write++ = KF_EVENT_WORLD_SAVE_ACTION_70;
                *write++ = KF_ENUM_ENCODE(u8, object->object_id);
                *write++ = object->tail.fields.unknown_38;
                break;
            default:
                *write++ = KF_EVENT_WORLD_SAVE_STATE_BYTE;
                *write++ = object->tail.fields.unknown_38;
                break;
            }
            break;
        case KF_MAP_OBJECT_OP_SWITCH:
            if (object->tail.action_83.transition_mode < KF_MAP_OBJECT_TRANSITION_TOGGLE_CLOSED) {
                break;
            }
        case KF_MAP_OBJECT_OP_0:
        case KF_MAP_OBJECT_OP_LIFT_DOOR:
        case KF_MAP_OBJECT_OP_SIGNAL_DOOR:
        case KF_MAP_OBJECT_OP_HINGE:
        case KF_MAP_OBJECT_OP_ANIMATED_CONTAINER:
        case KF_MAP_OBJECT_OP_HINGED_CONTAINER:
        case KF_MAP_OBJECT_OP_SLIDING_CONTAINER:
        case KF_MAP_OBJECT_OP_80:
        case KF_MAP_OBJECT_OP_ANIMATED_HAZARD:
        case KF_MAP_OBJECT_OP_PATTERN_GATE:
        case KF_MAP_OBJECT_OP_CELL_COPY_TOGGLE:
        case KF_MAP_OBJECT_OP_SIGNAL_CELL_COPY:
        case KF_MAP_OBJECT_OP_REGION0_ITEM_SOCKET:
        case KF_MAP_OBJECT_OP_REGION1_ITEM_SOCKET:
        case KF_MAP_OBJECT_OP_EVENT_BIT_ACTIVATED:
        case KF_MAP_OBJECT_OP_ITEM_DIAL:
            *write++ = KF_EVENT_WORLD_SAVE_STATE_BYTE;
            *write++ = object->tail.fields.unknown_38;
            break;
        case KF_MAP_OBJECT_OP_RECALL_SOCKET:
        case KF_MAP_OBJECT_OP_DOOR_SOCKET:
            *write++ = KF_EVENT_WORLD_SAVE_EFFECT;
            *write++ = KF_ENUM_ENCODE(u8, object->tail.event_effect.pending_event_command);
            *write++ = object->tail.event_effect.effect_object_index;
            break;
        default:
            *write++ = KF_EVENT_WORLD_SAVE_UNCHANGED;
            break;
        }
    }

    event_saved_offsets_decode(saved);
    event_arena_owner_pointers_add_delta((s32)saved);
    block = saved[save_slot];
    if (block != NULL) {
        memory_block_release(block);
    }
    size = (write - payload + 3) & ~3;
    block = memory_arena_allocate_block(&cur_evt_state.arena.first_block,
                                        size, &saved[save_slot]);
    if (block != NULL) {
        resource_copy_words((u32 *)block, (const u32 *)payload, size >> 2);
        event_saved_offsets_encode(saved);
        event_arena_owner_pointers_subtract_delta((s32)saved);
    }
}

enum {
    /* Action timer set once a restored object's position is applied. */
    EVENT_RESTORED_ACTION_TIMER = 0x63
};
ADDRESS(0x80049710, 0x378)
void event_world_state_restore_slot(s32 save_slot)
{
    u8 *saved[KF_EVENT_SAVED_SLOT_COUNT];
    u8 *stream;
    s32 index;
    KfMapObject *object;

    event_saved_offsets_decode(saved);
    stream = saved[save_slot];
    if (stream == NULL) {
        return;
    }

    for (;;) {
        s32 actor_index = *stream++;
        KfActor *actor;
        if (actor_index == KF_EVENT_WORLD_SAVE_LIST_END) {
            break;
        }
        actor = &actor_status.actors[actor_index];
        actor->lifecycle = KF_ENUM_DECODE(KF_ENUM_PROMOTED(KfActorLifecycle), *stream++);
    }

    for (;;) {
        s32 group_index = *stream++;
        KfTargetGroup *group;
        KfTargetCandidate *candidate;
        if (group_index == KF_EVENT_WORLD_SAVE_LIST_END) {
            break;
        }
        group = &actor_status.target_groups[group_index];
        candidate = group->targets[0].pointer;
        candidate->word_10.bytes.fallback_offset = *stream++;
        candidate->word_12.bytes.marker_state = *stream++;
    }

    object = map_objects_slots.objects;
    for (index = 0; index < KF_MAP_OBJECT_CAPACITY; object++, index++) {
        u8 opcode = *stream++;
        u16 x;
        u16 y;
        u16 z;

        switch (opcode) {
        case KF_EVENT_WORLD_SAVE_EMPTY:
            object->object_id = KF_OBJECT_NONE;
            break;
        case KF_EVENT_WORLD_SAVE_EFFECT:
            object->tail.event_effect.pending_event_command = KF_ENUM_DECODE(KF_ENUM_PROMOTED(KfObjectId),
                *stream++);
            object->tail.event_effect.effect_object_index = *stream++;
            break;
        case KF_EVENT_WORLD_SAVE_ACTION_60: {
            s32 x_high;
            s32 z_high;
            s32 y_high;
            s32 angle;

            map_object_reset(object);
            object->action = KF_MAP_OBJECT_OP_FALL_AND_TIP;
            object->object_id = KF_ENUM_DECODE(KF_ENUM_PROMOTED(KfObjectId), *stream++);
            x = *stream++;
            x_high = *stream++;
            z = *stream++;
            z_high = *stream++;
            y = *stream++;
            y_high = *stream++;
            angle = *stream++;
            x |= x_high << 8;
            z |= z_high << 8;
            y |= y_high << 8;
            object->rotation.vz = KF_ANGLE_QUARTER_TURN;
            object->rotation.vy = angle << 4;
apply_position:
            object->position.vx = x << 2;
            object->position.vz = z << 2;
            object->position.vy = (s16)y;
            object->action_timer = EVENT_RESTORED_ACTION_TIMER;
            collision_sample_map_cell_layer(object->position.vx, object->position.vy,
                           object->position.vz);
            object->layer_mask = KF_COLLISION_CACHE_LAYER == 0 ? KF_MAP_LAYER_FIRST : KF_MAP_LAYER_SECOND;
            object->tail.fields.unknown_38 = KF_MAP_OBJECT_EVENT_ARMED;
            break;
        }
        case KF_EVENT_WORLD_SAVE_ACTION_61:
            map_object_reset(object);
            object->action = KF_MAP_OBJECT_OP_FALL_AND_SPIN;
            object->object_id = KF_ENUM_DECODE(KF_ENUM_PROMOTED(KfObjectId), *stream++);
            x = *stream++;
            x |= *stream++ << 8;
            z = *stream++;
            z |= *stream++ << 8;
            y = *stream++;
            y |= *stream++ << 8;
            goto apply_position;
        case KF_EVENT_WORLD_SAVE_ACTION_62:
            map_object_reset(object);
            object->action = KF_MAP_OBJECT_OP_BOUNCE;
            object->object_id = KF_ENUM_DECODE(KF_ENUM_PROMOTED(KfObjectId), *stream++);
            x = *stream++;
            x |= *stream++ << 8;
            z = *stream++;
            z |= *stream++ << 8;
            y = *stream++;
            y |= *stream++ << 8;
            object->tail.fields.unknown_3a.value = (*stream << 2) + (rand() >> 13);
            stream++;
            if (object->object_id == KF_OBJECT_103) {
                object->rotation.vx = KF_ANGLE_QUARTER_TURN;
            }
            goto apply_position;
        case KF_EVENT_WORLD_SAVE_ACTION_70:
            map_object_reset(object);
            object->action = KF_MAP_OBJECT_OP_OFFSET_MOTION;
            object->object_id = KF_ENUM_DECODE(KF_ENUM_PROMOTED(KfObjectId), *stream++);
            /* The next byte is also the body of opcode 0xfd. */
        case KF_EVENT_WORLD_SAVE_STATE_BYTE:
            object->tail.fields.unknown_38 = *stream++;
            break;
        default:
            break;
        }
    }
}

ADDRESS(0x80049a88, 0x5b8)
void card_payload_capture_game_state(u8 *buffer)
{
    KfCardSavePayload *payload = (KfCardSavePayload *)buffer;
    KfCardPlayerSnapshot *saved = &payload->player;
    u8 *magic_flag = payload->magic_menu_available;
    KfMagicRecord *record = effect_status.magic_records;
    s32 i;

    memcpy((void *)buffer, (const void *)asset_status.active_resource_ids,
        sizeof asset_status.active_resource_ids);
    memcpy((void *)&payload->camera_position, (const void *)&cur_avatar_status.camera_position,
        sizeof payload->camera_position);
    memcpy((void *)(buffer + KF_CARD_SAVE_ROTATION_OFFSET),
        (const void *)&cur_avatar_status.camera_rotation_target, sizeof cur_avatar_status.camera_rotation_target);

    saved->experience = cur_avatar_status.experience;
    saved->next_level_experience = cur_avatar_status.next_level_experience;
    saved->gold = cur_avatar_status.gold;
    saved->map_layer_index = cur_avatar_status.map_layer_index;
    saved->vitals.maximum_hp = cur_avatar_status.vitals.maximum_hp;
    saved->vitals.current_hp = cur_avatar_status.vitals.current_hp;
    saved->vitals.maximum_mp = cur_avatar_status.vitals.maximum_mp;
    saved->vitals.current_mp = cur_avatar_status.vitals.current_mp;
    saved->base_physical_power = cur_avatar_status.base_physical_power;
    saved->base_magic = cur_avatar_status.base_magic;
    saved->physical_power_training = cur_avatar_status.physical_power_training;
    saved->magic_training = cur_avatar_status.magic_training;
    saved->poison_timer = cur_avatar_status.poison_timer;
    saved->curse_strength = cur_avatar_status.curse_strength;
    saved->curse_phase_limit = cur_avatar_status.curse_phase_limit;
    saved->darkness_phase = cur_avatar_status.darkness_phase;
    saved->darkness_phase_limit = cur_avatar_status.darkness_phase_limit;
    saved->slow_timer = cur_avatar_status.slow_timer;
    saved->paralysis_timer = cur_avatar_status.paralysis_timer;
    saved->defense_boost_timer = cur_avatar_status.defense_boost_timer;
    saved->attack_boost_timer = cur_avatar_status.attack_boost_timer;
    saved->magic_tint_phase = cur_avatar_status.magic_tint_phase;
    saved->magic_tint_phase_limit = cur_avatar_status.magic_tint_phase_limit;
    saved->map_marker_visual_effect_timer = cur_avatar_status.map_marker_visual_effect_timer;
    saved->full_mp_timer = cur_avatar_status.full_mp_timer;
    saved->magic_boost_timer = cur_avatar_status.magic_boost_timer;
    saved->level = cur_avatar_status.level;
    saved->unknown_09 = cur_avatar_status.unknown_09;
    saved->equipped_ids[0] = cur_avatar_status.equipped_head_id;
    saved->equipped_ids[1] = cur_avatar_status.equipped_body_id;
    saved->equipped_ids[2] = cur_avatar_status.equipped_arm_id;
    saved->equipped_ids[3] = cur_avatar_status.equipped_leg_id;
    saved->equipped_ids[4] = cur_avatar_status.equipped_shield_id;
    saved->equipped_ids[5] = cur_avatar_status.equipped_accessory_id;
    saved->equipped_ids[6] = cur_avatar_status.equipped_extra_id;
    saved->primary_magic_shortcut_id = cur_avatar_status.primary_magic_shortcut_id;
    saved->secondary_magic_shortcut_id = cur_avatar_status.secondary_magic_shortcut_id;
    saved->secondary_item_shortcut_id = cur_avatar_status.secondary_item_shortcut_id;
    saved->equipped_weapon_id = cur_avatar_status.equipped_weapon_id;
    saved->audio_effects_enabled = cur_avatar_status.audio_effects_enabled;
    saved->audio_music_enabled = cur_avatar_status.audio_music_enabled;
    saved->hud_gauges_enabled = cur_avatar_status.hud_gauges_enabled;
    saved->compass_enabled = cur_avatar_status.compass_enabled;
    saved->item_preview_enabled = cur_avatar_status.item_preview_enabled;
    saved->walking_bob_enabled = cur_avatar_status.walking_bob_enabled;

    for (i = 63; i != -1; --i) {
        *magic_flag++ = record->menu_available;
        ++record;
    }
    memcpy((void *)payload->game_counters, (const void *)event_item_counters_table,
        sizeof payload->game_counters);
    memcpy((void *)payload->event_control, (const void *)cur_evt_state.control.bytes,
        sizeof payload->event_control);
    memcpy((void *)payload->event_arena, (const void *)cur_evt_state.arena.bytes,
        sizeof payload->event_arena);
    memcpy((void *)payload->saved_event_offsets, (const void *)cur_evt_state.saved_offsets,
        sizeof payload->saved_event_offsets);
}

ADDRESS(0x8004a040, 0x5e0)
void card_payload_restore_game_state(const u8 *buffer)
{
    const KfCardSavePayload *payload = (const KfCardSavePayload *)buffer;
    const KfCardPlayerSnapshot *saved = &payload->player;
    const u8 *magic_flag = payload->magic_menu_available;
    KfMagicRecord *record = effect_status.magic_records;
    s32 i;

    memcpy((void *)asset_status.active_resource_ids, (const void *)buffer,
        sizeof asset_status.active_resource_ids);
    asset_status.active_resource_ids[KF_RESOURCE_SLOT_TMD] = asset_status.active_resource_ids[KF_RESOURCE_SLOT_MAP_REGION];
    asset_status.active_resource_ids[KF_RESOURCE_SLOT_TIM] = asset_status.active_resource_ids[KF_RESOURCE_SLOT_MAP_REGION];
    asset_status.active_resource_ids[KF_RESOURCE_SLOT_VAB] = asset_status.active_resource_ids[KF_RESOURCE_SLOT_MAP_REGION];
    asset_status.active_resource_ids[KF_RESOURCE_SLOT_SEQUENCE] = asset_status.active_resource_ids[KF_RESOURCE_SLOT_MAP_REGION];
    memcpy((void *)&cur_avatar_status.camera_position, (const void *)&payload->camera_position,
        sizeof cur_avatar_status.camera_position);
    memcpy((void *)&cur_avatar_status.camera_rotation_target,
        (const void *)(buffer + KF_CARD_SAVE_ROTATION_OFFSET), sizeof cur_avatar_status.camera_rotation_target);

    cur_avatar_status.experience = saved->experience;
    cur_avatar_status.next_level_experience = saved->next_level_experience;
    cur_avatar_status.gold = saved->gold;
    cur_avatar_status.map_layer_index = saved->map_layer_index;
    cur_avatar_status.vitals.maximum_hp = saved->vitals.maximum_hp;
    cur_avatar_status.vitals.current_hp = saved->vitals.current_hp;
    cur_avatar_status.vitals.maximum_mp = saved->vitals.maximum_mp;
    cur_avatar_status.vitals.current_mp = saved->vitals.current_mp;
    cur_avatar_status.base_physical_power = saved->base_physical_power;
    cur_avatar_status.base_magic = saved->base_magic;
    cur_avatar_status.physical_power_training = saved->physical_power_training;
    cur_avatar_status.magic_training = saved->magic_training;
    cur_avatar_status.poison_timer = saved->poison_timer;
    cur_avatar_status.curse_strength = saved->curse_strength;
    cur_avatar_status.curse_phase_limit = saved->curse_phase_limit;
    cur_avatar_status.darkness_phase = saved->darkness_phase;
    cur_avatar_status.darkness_phase_limit = saved->darkness_phase_limit;
    cur_avatar_status.slow_timer = saved->slow_timer;
    cur_avatar_status.paralysis_timer = saved->paralysis_timer;
    cur_avatar_status.defense_boost_timer = saved->defense_boost_timer;
    cur_avatar_status.attack_boost_timer = saved->attack_boost_timer;
    cur_avatar_status.magic_tint_phase = saved->magic_tint_phase;
    cur_avatar_status.magic_tint_phase_limit = saved->magic_tint_phase_limit;
    cur_avatar_status.map_marker_visual_effect_timer = saved->map_marker_visual_effect_timer;
    cur_avatar_status.full_mp_timer = saved->full_mp_timer;
    cur_avatar_status.magic_boost_timer = saved->magic_boost_timer;
    cur_avatar_status.level = saved->level;
    cur_avatar_status.unknown_09 = saved->unknown_09;
    cur_avatar_status.equipped_head_id = saved->equipped_ids[0];
    cur_avatar_status.equipped_body_id = saved->equipped_ids[1];
    cur_avatar_status.equipped_arm_id = saved->equipped_ids[2];
    cur_avatar_status.equipped_leg_id = saved->equipped_ids[3];
    cur_avatar_status.equipped_shield_id = saved->equipped_ids[4];
    cur_avatar_status.equipped_accessory_id = saved->equipped_ids[5];
    cur_avatar_status.equipped_extra_id = saved->equipped_ids[6];
    cur_avatar_status.primary_magic_shortcut_id = saved->primary_magic_shortcut_id;
    cur_avatar_status.secondary_magic_shortcut_id = saved->secondary_magic_shortcut_id;
    cur_avatar_status.secondary_item_shortcut_id = saved->secondary_item_shortcut_id;
    cur_avatar_status.equipped_weapon_id = saved->equipped_weapon_id;
    cur_avatar_status.audio_effects_enabled = saved->audio_effects_enabled;
    cur_avatar_status.audio_music_enabled = saved->audio_music_enabled;
    cur_avatar_status.hud_gauges_enabled = saved->hud_gauges_enabled;
    cur_avatar_status.compass_enabled = saved->compass_enabled;
    cur_avatar_status.item_preview_enabled = saved->item_preview_enabled;
    cur_avatar_status.walking_bob_enabled = saved->walking_bob_enabled;

    for (i = 63; i != -1; --i) {
        record->menu_available = *magic_flag++;
        ++record;
    }
    memcpy((void *)event_item_counters_table, (const void *)payload->game_counters,
        sizeof payload->game_counters);
    memcpy((void *)cur_evt_state.control.bytes, (const void *)payload->event_control,
        sizeof payload->event_control);
    memcpy((void *)cur_evt_state.arena.bytes, (const void *)payload->event_arena,
        sizeof payload->event_arena);
    memcpy((void *)cur_evt_state.saved_offsets, (const void *)payload->saved_event_offsets,
        sizeof payload->saved_event_offsets);
}
