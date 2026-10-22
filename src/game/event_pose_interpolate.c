#include <kf/game/event_stream.h>
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
u8 event_magic_unlock_ids_5e[8] = {18, 19, 11, 12, 0xff, 0xff, 0xff, 0xff};

DATA(0x8009b52c, 0x78, ".bss")
u8 game_counter_bytes[KF_ITEM_ID_COUNT];

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
    output->vx += player_state.camera_position.vx;
    y_position = output->vy - KF_PLAYER_CAMERA_EYE_OFFSET;
    camera_y = height_offset + player_state.camera_position.vy;
    output->vy = y_position + camera_y;
    output->vz += player_state.camera_position.vz;
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

    step = phase_step & 0xfffe;
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
        actor_state.target_groups[actor->group_index].targets[0].pointer;
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
        if (event_state.control.bytes[cursor[1]] == cursor[2]) {
            u8 offset = event_target_stream_find_marker(candidate, cursor[3]);
            if (candidate->word_10.bytes.fallback_offset < offset) {
                candidate->word_10.bytes.fallback_offset = offset;
            }
            event_state.control.fields.stream_actor_definition_id = actor->definition_id;
            candidate->word_12.bytes.marker_state = 0;
            goto use_fallback;
        }
        cursor += 4;
    }
}

ADDRESS(0x80047000, 0x444)
void event_target_stream_execute(KfActor *actor)
{
    KfTargetCandidate *candidate =
        actor_state.target_groups[actor->group_index].targets[0].pointer;
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
        event_state.control.fields.stream_actor_definition_id = actor->definition_id;
    }
    cursor = event_target_stream_resolve_cursor(actor);
    if (event_state.control.fields.stream_actor_definition_id != actor->definition_id &&
        candidate->word_12.bytes.marker_state == 1) {
        while (*cursor++ != KF_EVENT_STREAM_REWIND_MARKER) {
        }
        cursor++;
        candidate->word_10.bytes.fallback_offset = cursor - candidate->word_14.bytes;
        candidate->word_12.bytes.marker_state = 0;
    }

    for (;;) {
        switch (*cursor) {
        case KF_EVENT_STREAM_REWIND_MARKER:
            candidate->word_12.bytes.marker_state = 1;
            /* The two rewind opcodes share their byte-count operand. */
        case KF_EVENT_STREAM_REWIND:
        {
            u8 count = cursor[1];
            candidate->word_10.bytes.fallback_offset -= count;
            cursor -= count;
            continue;
        }
        case KF_EVENT_STREAM_BRANCH:
            if (event_state.control.bytes[cursor[1]] == cursor[2]) {
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
            ((void (*)(KfActor *, s32))resource_state.active_table[4])(actor, *cursor);
            break;
        case KF_EVENT_STREAM_REPEAT:
            cursor++;
            candidate->word_10.bytes.fallback_offset++;
            repeat = *cursor;
            break;
        case KF_EVENT_STREAM_RESET_MARKER:
            event_state.control.fields.stream_actor_definition_id = actor->definition_id;
            cursor++;
            candidate->word_10.bytes.fallback_offset++;
            candidate->word_12.bytes.marker_state = 0;
            continue;
        case KF_EVENT_STREAM_SET_CONTROL:
            event_state.control.bytes[cursor[1]] = cursor[2];
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
    old_counter = game_counter_bytes[KF_ENUM_ENCODE(u8, KF_OBJECT_83)];
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
    if (game_counter_bytes[KF_ENUM_ENCODE(u8, KF_OBJECT_83)] < old_counter) {
        event_state.control.fields.counter_53_decreased = 1;
    }
    event_state.control.fields.stream_actor_definition_id = actor->definition_id;
    if (restore_state && candidate->word_10.bytes.completion_animation_id != KF_ANIMATION_CLIP_NONE) {
        actor_animation_seek_phase(actor, candidate->word_10.bytes.completion_animation_id, 0,
                      KF_ACTOR_ANIMATION_PHASE_MAX,
                      candidate->word_0e.value);
        actor->animation_id = saved_state;
    }
    event_state.interaction_handled = KF_TRUE;
    player_clear_motion();
}

ADDRESS(0x80047444, 0x8c)
void event_spawn_effect_object(KfMapObject *event, KF_ENUM_PARAM(KfObjectId, s32) object_id)
{
    KfMapObject *object = map_object_effect_pool_acquire(KF_MAP_OBJECT_EVENT_POOL_FIRST,
                                                         KF_MAP_OBJECT_EVENT_POOL_SIZE, -1);

    /* The byte store wraps pool slots 0x17c..0x18b to offsets 0..15. */
    event->tail.event_effect.effect_object_index = (object - map_object_state.objects) - 0x7c;
    object->object_id = object_id;
    object->tail.fields.unknown_38 = KF_MAP_OBJECT_EVENT_DISARMED;
}

ADDRESS(0x800474d0, 0xc54)
void event_scene_command_dispatch(const VECTOR *position,
                                  const KfPlayerViewRotation *rotation,
                                  KF_ENUM_PARAM(KfObjectId, s32) command)
{
    s32 index;
    s32 object_control_offset;
    const u8 *magic_ids;

    event_state.interaction_handled = KF_FALSE;
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
            object = &map_object_state.objects[index];
            status = map_object_check_and_consume_marker(object, KF_ENUM_ENCODE(s32, command));
            switch (status) {
            case KF_MAP_OBJECT_MARKER_CONSUMED:
                audio_play_sound_64();
                event_state.interaction_handled = KF_TRUE;
                break;
            case KF_MAP_OBJECT_MARKER_MISMATCH:
                notify_enqueue(object->tail.notification.linked_notification);
                event_state.interaction_handled = KF_TRUE;
                break;
            case KF_MAP_OBJECT_MARKER_REFUSED:
                notify_enqueue(KF_NOTIFICATION_4);
                event_state.interaction_handled = KF_TRUE;
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
            KfMapObject *object = &map_object_state.objects[index];

            if (object->object_id == KF_OBJECT_189) {
                if (object->tail.event_effect.pending_event_command != KF_OBJECT_NONE) {
                    break;
                }
                object->tail.event_effect.pending_event_command = command;
                object->action_timer = 0;
                ((KfEventControlObjectSlot *)&event_state.control.bytes[object_control_offset])->object_index =
                    object - map_object_state.objects;
                ((KfEventControlObjectSlot *)&event_state.control.bytes[object_control_offset])->resource_id =
                    resource_state.active_resource_ids[KF_RESOURCE_SLOT_MAP_REGION];
                event_state.interaction_handled = KF_TRUE;
                game_counter_decrement(command);
                object->extra_40.bytes[0] = KF_MAP_OBJECT_LATCH_CLEAR;
                event_spawn_effect_object(object, command);
            } else if (map_object_check_and_consume_marker(object, KF_ENUM_ENCODE(s32, command)) == KF_MAP_OBJECT_MARKER_MISMATCH) {
                notify_enqueue(object->tail.notification.linked_notification);
                event_state.interaction_handled = KF_TRUE;
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
        if (resource_state.active_resource_ids[KF_RESOURCE_SLOT_MAP_REGION] == 7 ||
            event_state.control.bytes[object_control_offset] == 0xff ||
            player_state.vitals.current_mp < 10) {
            break;
        }
        player_state.vitals.current_mp -= 10;
        render_frames_with_color_overlay(KF_COLOR_OVERLAY_ADD, 0, KF_FIXED12_ONE, 256);
        actor_disable_type3_transition_actors();
        previous_value = ((KfEventControlObjectSlot *)&event_state.control.bytes[object_control_offset])->resource_id;
        do {
            cd_request_yield();
            resource_advance_transition();
        } while (resource_state.transition_active);
        if (previous_value != resource_state.active_resource_ids[KF_RESOURCE_SLOT_MAP_REGION]) {
            resource_request_transition(previous_value, previous_value, previous_value,
                          KF_RESOURCE_REQUEST_KEEP, KF_RESOURCE_REQUEST_KEEP, KF_RESOURCE_OFFSET_NO_SHIFT, KF_RESOURCE_OFFSET_NO_SHIFT, KF_RESOURCE_OFFSET_NO_SHIFT);
        } else {
            resource_request_transition(KF_RESOURCE_REQUEST_KEEP, KF_RESOURCE_REQUEST_KEEP, previous_value, KF_RESOURCE_REQUEST_KEEP, KF_RESOURCE_REQUEST_KEEP,
                          KF_RESOURCE_OFFSET_NO_SHIFT, KF_RESOURCE_OFFSET_NO_SHIFT, KF_RESOURCE_OFFSET_NO_SHIFT);
        }
        do {
            cd_request_yield();
            resource_advance_transition();
        } while (resource_state.transition_active);
        cd_request_wait_idle();

        object_index = ((KfEventControlObjectSlot *)&event_state.control.bytes[object_control_offset])->object_index;
        object = &map_object_state.objects[object_index];
        angle_to_forward_xz(object->rotation.vy, &forward);
        vector2i_scale_shift11(1024, &forward);
        player_state.death_state = KF_PLAYER_REACTION_NORMAL;
        player_state.view_rotation_offset.components[2] = 0;
        player_state.view_rotation_offset.components[1] = 0;
        player_state.view_rotation_offset.components[0] = 0;
        player_state.camera_rotation = player_state.camera_rotation_target;
        player_state.camera_position.vx = object->position.vx + forward.x;
        player_state.camera_position.vz = object->position.vz + forward.z;
        player_state.camera_position.vy = object->position.vy;
        yaw = object->rotation.vy + KF_ANGLE_HALF_TURN;
        event_state.interaction_handled = KF_TRUE;
        player_state.camera_rotation_target.angles[1] = yaw;
        player_state.camera_rotation.angles[1] = yaw;
        render_frames_with_color_overlay(KF_COLOR_OVERLAY_ADD, KF_FIXED12_ONE, KF_FIXED12_ONE, 0);
        if (game_graphics_runtime.asset_registry_entries[0x181] == NULL) {
            resource_tmd_queue_read(KF_RESOURCE_ARCHIVE_MO, 0x101, 0x181);
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
            KfMapObject *object = &map_object_state.objects[index];

            if (object->object_id == KF_OBJECT_184) {
                if (object->tail.event_effect.pending_event_command == KF_OBJECT_NONE) {
                    object->tail.event_effect.pending_event_command = command;
                    object->action_timer = 0;
                    event_state.interaction_handled = KF_TRUE;
                    game_counter_decrement(command);
                    event_spawn_effect_object(object, command);
                }
            } else if (map_object_check_and_consume_marker(object, KF_ENUM_ENCODE(s32, command)) == KF_MAP_OBJECT_MARKER_MISMATCH) {
                notify_enqueue(object->tail.notification.linked_notification);
                event_state.interaction_handled = KF_TRUE;
            }
        }
        break;
    case KF_OBJECT_84:
        player_state.map_marker_visual_effect_timer = 1200;
        game_counter_decrement(KF_OBJECT_84);
        event_state.interaction_handled = KF_TRUE;
        break;
    case KF_OBJECT_88:
        index = map_object_find_interaction_target(0, position, 800, 1700,
                               rotation->angles[1], 512);
        if (index != -1) {
            KfMapObject *object = &map_object_state.objects[index];

            if (object->object_id == KF_OBJECT_157) {
                map_object_apply_marker_signal(object->tail.marker.marker_id);
            }
            event_state.interaction_handled = KF_TRUE;
        }
        audio_play_sound_at_volume_100(8);
        break;
    case KF_OBJECT_89: {
        KfMapObject *scan = map_object_state.objects;
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
                                     &player_state.camera_position, 0, 0);
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
                4 * (nearest->position.vy - audio_state.listener_position.vy);
            audio_play_spatial_range(0x8009, &nearest->position, 0x6e,
                                     25000, 29000, 0);
            event_state.interaction_handled = KF_TRUE;
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
            magic_record = &effect_state.magic_records[KF_ENUM_ENCODE(s32, magic_id)];
            if (magic_record->menu_available == 0) {
                magic_record->menu_available = 1;
                break;
            }
        }
        object = map_object_effect_pool_acquire(
            KF_MAP_OBJECT_SCATTER_POOL_FIRST, KF_MAP_OBJECT_EFFECT_POOL_SIZE,
            map_object_state.spawn_sequence_pool_15e);
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

        scene_position_from_camera_offset(0, 200, 1500, player_state.camera_rotation.angles[0],
                      player_state.camera_rotation.angles[1], 0, 0,
                      &far_position);
        scene_position_from_camera_offset(0, 200, 1000, player_state.camera_rotation.angles[0],
                      player_state.camera_rotation.angles[1], 0, 0,
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
        event_state.interaction_handled = KF_TRUE;
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
            if (map_object_state.objects[index].object_id != KF_OBJECT_195) {
                break;
            }
        }
        if (!game_counter_increment(KF_ITEM_RESTORE_HP_100)) {
            notify_enqueue(KF_NOTIFICATION_22);
            game_counter_decrement(KF_OBJECT_82);
        }
        event_state.interaction_handled = KF_TRUE;
        break;
    case KF_OBJECT_85: {
        KF_ENUM_STORAGE(KfMapLayerMask, s32) side =
            player_state.map_layer_index == 0 ? KF_MAP_LAYER_FIRST : KF_MAP_LAYER_SECOND;
        s32 actor_distance;
        KfActor *actor = actor_find_best_in_cone(position, rotation->angles[1],
                                       rotation->angles[0], 8000, 500, 500,
                                       &actor_distance, -1);

        if (actor != NULL && actor->current_map_layer == side) {
            menu_show_transition_image(KF_RESOURCE_ARCHIVE_ITEM, actor->definition_id + 240);
            event_state.interaction_handled = KF_TRUE;
            break;
        }
        {
            s32 remaining;
            KfMapObject *scan;

            for (scan = map_object_state.objects,
                 remaining = KF_MAP_OBJECT_CAPACITY - 1;
                 remaining != -1; remaining--, scan++) {
                KfMapObject *object = scan;

                if (map_object_state.templates[KF_ENUM_ENCODE(u16,
                                                              object->object_id)].collision_kind != KF_MAP_OBJECT_OP_SCENE_INSPECT ||
                    object->extra_40.saved_layer.layer_mask != side) {
                    continue;
                }
                if (player_camera_within_map_region(object->position.vx >> KF_MAP_CELL_POSITION_SHIFT,
                                  object->position.vz >> KF_MAP_CELL_POSITION_SHIFT,
                                  object->tail.scene_inspect.region_width,
                                  object->tail.scene_inspect.region_depth, 0x8000)) {
                    menu_show_transition_image(KF_RESOURCE_ARCHIVE_ITEM, object->tail.scene_inspect.transition_image_id + 510);
                    event_state.interaction_handled = KF_TRUE;
                    break;
                }
            }
        }
        break;
    }
    case KF_OBJECT_86:
        game_counter_decrement(KF_OBJECT_86);
        player_state.full_mp_timer = 900;
        event_state.interaction_handled = KF_TRUE;
        break;
    case KF_OBJECT_87:
        game_counter_decrement(KF_OBJECT_87);
        player_state.magic_boost_timer = 900;
        event_state.interaction_handled = KF_TRUE;
        player_recalculate_combat_stats();
        break;
    default:
        /* IDs 83, 95..98, 105 and 110 have no action. */
        break;
    }

invoke_callback:
    ((KfEventCommandCallback)resource_state.active_table[2])(
        position, rotation, command);
    if (!event_state.interaction_handled) {
        notify_enqueue(KF_NOTIFICATION_NOTHING_HAPPENS);
    }
    player_clear_motion();
}
ADDRESS(0x80048124, 0x54)
b32 game_counter_decrement(KF_ENUM_PARAM(KfObjectId, s32) index)
{
    if (game_counter_bytes[KF_ENUM_ENCODE(s32, index)] != 0) {
        game_counter_bytes[KF_ENUM_ENCODE(s32, index)]--;
        return KF_FALSE;
    }

    return KF_TRUE;
}

ADDRESS(0x80048178, 0x90)
b32 game_counter_increment(KF_ENUM_PARAM(KfObjectId, s32) index)
{
    if (game_counter_bytes[KF_ENUM_ENCODE(s32, index)] < 99) {
        game_counter_bytes[KF_ENUM_ENCODE(s32, index)]++;
        resource_state.active_table[6]();
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
        accumulate_color_overlay(current_first, current_second, current_third, 0x800);
        render_game_frame(NULL, NULL);
        fraction += step;
    } while (fraction < KF_FIXED12_ONE);

    reset_collision_rows_and_overlay();
    accumulate_color_overlay(target_first, target_second, target_third, 0x800);
    render_game_frame(NULL, NULL);
}
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
    s32 current_yaw;
    s32 target_yaw;
    s16 initial_render_depth_offset;
    s16 target_render_depth_offset;
    s32 fraction;
    u32 previous_buttons;
    u32 buttons; /* reused as the remove-object flag after the button wait */

    if (object == NULL) {
        va_start(arguments, object);
        spawn_object_id = va_arg(arguments, s32);
        va_end(arguments);
        spawned_id = spawn_object_id;
        object = map_object_effect_pool_acquire(
            KF_MAP_OBJECT_SCATTER_POOL_FIRST, KF_MAP_OBJECT_EFFECT_POOL_SIZE,
            map_object_state.spawn_sequence_pool_15e);
        map_object_reset(object);
        object->object_id = KF_ENUM_DECODE(KF_ENUM_PROMOTED(KfObjectId), spawn_object_id);
        object->layer_mask = KF_MAP_LAYER_BOTH;
        object->action = KF_MAP_OBJECT_OP_NONE;
        object->rotation.vz = 0;
        object->rotation.vy = 0;
        object->rotation.vx = 0;
        object->tail.fields.unknown_38 = KF_MAP_OBJECT_EVENT_ARMED;
    }

    object_template = &map_object_state.templates[KF_ENUM_ENCODE(u16, object->object_id)];
    initial_render_depth_offset = object->render_depth_offset;
    if (object->tail.fields.unknown_38 != KF_MAP_OBJECT_EVENT_ARMED) {
        return;
    }
    if (object_template->kind == KF_MAP_OBJECT_KIND_GOLD) {
        notify_enqueue(KF_NOTIFICATION_PAYLOAD_ID, object->tail.gold_reward.gold_amount);
        object->object_id = KF_OBJECT_NONE;
        player_state.gold += object->tail.gold_reward.gold_amount;
        return;
    }

    if (object->action == KF_MAP_OBJECT_OP_OFFSET_MOTION && object->object_id == KF_OBJECT_103) {
        KfMapObject *child = &map_object_state.objects[object->extra_40.object_index];
        KfMapObject *next = &map_object_state.objects[
            child->tail.fields.unknown_3a.bytes.high];
        if (next->action == KF_MAP_OBJECT_OP_SIGNAL_DOOR && next->action_timer != 0) {
            return;
        }
    }
    if (object->object_id == KF_OBJECT_18) {
        object->object_id = KF_OBJECT_13;
    }

    first_yaw = player_state.camera_rotation.angles[0] & KF_ANGLE_WRAP_MASK;
    target_yaw = first_yaw;
    if (first_yaw < 0x800) {
        if (first_yaw > 0x100) {
            target_yaw = 0x100;
        }
    } else if (first_yaw < 0xf00) {
        target_yaw = 0xf00;
    }
    previous_buttons = PadRead(1);
    target_render_depth_offset = (-object_template->params.pose.depth_offset) / 4 - 200;

    if (spawned_id != -1) {
        scene_position_from_camera_offset(0, 500, 1500, target_yaw,
                      player_state.camera_rotation.angles[1],
                      object_template->params.pose.height_offset,
                      object_template->params.pose.depth_offset,
                      &object->position);
    } else {
        first_angles = object->rotation;
        first_position = object->position;
        scene_position_from_camera_offset(0, 500, 1500, target_yaw,
                      player_state.camera_rotation.angles[1],
                      object_template->params.pose.height_offset,
                      object_template->params.pose.depth_offset,
                      &next_position);
        next_angles.vz = 0;
        next_angles.vx = 0;
        for (fraction = 0; fraction <= KF_FIXED12_ONE; fraction += 0x200) {
            buttons = PadRead(1);
            if (previous_buttons == 0 && buttons != 0) {
                goto button_pressed;
            }
            previous_buttons = buttons;
            scene_pose_interpolate(object, &first_position,
                          &next_position, &first_angles, &next_angles,
                          fraction);
            object->render_depth_offset = value_approach(
                initial_render_depth_offset, target_render_depth_offset, fraction);
            player_state.camera_rotation.angles[0] = angle_lerp_shortest_q12(
                first_yaw, target_yaw, fraction);
            cd_request_service_vab();
            cd_request_service_stream();
            render_game_frame(NULL, (const SVECTOR *)&player_state.camera_rotation);
        }
    }
    object->render_depth_offset = target_render_depth_offset;

    for (;;) {
        buttons = PadRead(1);
        if (previous_buttons == 0 && buttons != 0) {
            break;
        }
        object->rotation.vy += 0x40;
        cd_request_service_vab();
        cd_request_service_stream();
        render_game_frame(NULL, (const SVECTOR *)&player_state.camera_rotation);
        previous_buttons = buttons;
    }

button_pressed:
    if ((buttons & 0x20) == 0 && spawned_id == -1) {
        goto return_pose;
    }
    if (!game_counter_increment(object->object_id)) {
        switch (object->object_id) {
        case KF_OBJECT_117: {
            s32 amount = ((rand() * 6) >> 15) + 4;
            if (game_counter_bytes[KF_ENUM_ENCODE(u8, KF_OBJECT_117)] < 99 - amount) {
                game_counter_bytes[KF_ENUM_ENCODE(u8, KF_OBJECT_117)] += amount;
            } else {
                game_counter_bytes[KF_ENUM_ENCODE(u8, KF_OBJECT_117)] = 99;
            }
            break;
        }
        case KF_OBJECT_118:
            if (game_counter_bytes[KF_ENUM_ENCODE(u8, KF_OBJECT_118)] < 95) {
                game_counter_bytes[KF_ENUM_ENCODE(u8, KF_OBJECT_118)] += 4;
            } else {
                game_counter_bytes[KF_ENUM_ENCODE(u8, KF_OBJECT_118)] = 99;
            }
            break;
        default:
            break;
        }
        buttons = 1;
        scene_position_from_camera_offset(-500, 500, 0, target_yaw,
                      player_state.camera_rotation.angles[1], 0, 0,
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
                                   first_angles.vy, 0x80)) {
        object->rotation.vy = (object->rotation.vy + 0x100) & KF_ANGLE_WRAP_MASK;
        render_game_frame(NULL, (const SVECTOR *)&player_state.camera_rotation);
    }
    object->rotation.vy = first_angles.vy;

interpolate_back:
    next_angles = object->rotation;
    next_position = object->position;
    current_yaw = player_state.camera_rotation.angles[0];
    for (fraction = 0; fraction <= KF_FIXED12_ONE; fraction += 0x200) {
        scene_pose_interpolate(object, &next_position,
                      &first_position, &next_angles, &first_angles,
                      fraction);
        player_state.camera_rotation.angles[0] = angle_lerp_shortest_q12(
            current_yaw, first_yaw, fraction);
        object->render_depth_offset = value_approach(
            target_render_depth_offset, initial_render_depth_offset, fraction);
        render_game_frame(NULL, (const SVECTOR *)&player_state.camera_rotation);
    }
    if (buttons) {
        object->object_id = KF_OBJECT_NONE;
    } else if (object->object_id == KF_OBJECT_13) {
        object->object_id = KF_OBJECT_18;
    }
finish:
    player_clear_motion();
}

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
    event_state.interaction_handled = KF_FALSE;
    if (collision_probe_forward_shape_0x20(&probe, (const struct KfEulerAngles *)rotation)) {
        color_overlay_transition(0x400, 0, 0, 0, 0x80, 0xa0, 0xff);
        player_state.vitals.current_hp = player_state.vitals.maximum_hp;
        color_overlay_transition(0x400, 0x80, 0xa0, 0xff, 0, 0, 0);
    }

    object_index = actor_find_overlap_excluding_target_type3(probe.vx, probe.vy, probe.vz, 0x578, 0xc80);
    if (object_index != KF_ACTOR_INDEX_NONE) {
        KfActor *actor = &actor_state.actors[object_index];
        s32 angle = vector_xz_to_angle(actor->position.vx - probe.vx,
                                       actor->position.vz - probe.vz);
        if (angle_within_tolerance(rotation->angles[1], angle, 300)) {
            event_target_stream_execute(actor);
        }
    }

    object_index = 0;
    objects = map_object_state.objects;
    templates = map_object_state.templates;
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
        event_state.interaction_handled = KF_TRUE;
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
            if (linked_index == 0xffff ||
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
                    object->tail.linked_property.linked_object_index = 0xffff;
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
                    && game_counter_bytes[KF_ENUM_ENCODE(u8, KF_OBJECT_15)] != 0) {
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
                                        object->rotation.vy + KF_ANGLE_HALF_TURN, 0x155)) {
                break;
            }
            /* Kind five enters the same state handler without the angle gate. */
        case KF_MAP_OBJECT_OP_ANIMATED_CONTAINER:
            switch (object->tail.marker.marker_id) {
            case KF_MAP_OBJECT_MARKER_TRIGGERED: {
                u16 linked_index = object->tail.linked_property.linked_object_index;
                if (linked_index != 0xffff) {
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
            if (player_state.death_state == KF_PLAYER_REACTION_NORMAL) {
                KfMapObjectRecord40 *record;
                player_begin_view_reaction(object_index);
                record = object->extra_40.record;
                if (record->reaction_mode == 1) {
                    record->reaction_mode = 5;
                }
            }
            break;
        case KF_MAP_OBJECT_OP_SCREEN_IMAGE:
        case KF_MAP_OBJECT_OP_HIDDEN_SCREEN_IMAGE:
            menu_show_transition_image(KF_RESOURCE_ARCHIVE_ITEM, object->tail.pair_38.value_38 + 0x78);
            break;
        case KF_MAP_OBJECT_OP_RESTORE_POINT:
            color_overlay_transition(0x200, 0, 0, 0, 0x80, 0xc8, 0xff);
            player_state.vitals.current_hp = player_state.vitals.maximum_hp;
            color_overlay_transition(0x200, 0x80, 0xc8, 0xff, 0, 0, 0);
            break;
        case KF_MAP_OBJECT_OP_SAVE_POINT:
            event_world_state_save_slot(resource_state.active_resource_ids[KF_RESOURCE_SLOT_MAP_REGION]);
            player_render_frame_and_release_pool();
            menu_card_save_browser();
            break;
        }
    }

invoke_callback:
    ((void (*)(const VECTOR *, const KfPlayerViewRotation *))
        resource_state.active_table[0])(&probe, rotation);
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
KfEventState event_state;

ADDRESS(0x8004905c, 0xb0)
void event_state_initialize(void)
{
    u16 *offset;
    s32 index;
    KfEventControlObjectSlot *object_slots;

    repeat_store_word(event_state.control.clear_words, 0,
                      sizeof(event_state.control.clear_words) / sizeof(u32));
    repeat_store_word(event_state.arena.clear_words, 0,
                      sizeof(event_state.arena.clear_words) / sizeof(u32));
    object_slots = event_state.control.fields.object_slots;
    object_slots[2].object_index = KF_EVENT_CONTROL_OBJECT_NONE;
    object_slots[1].object_index = KF_EVENT_CONTROL_OBJECT_NONE;
    object_slots[0].object_index = KF_EVENT_CONTROL_OBJECT_NONE;
    memory_arena_initialize_blocks(&event_state.arena.first_block,
                                   sizeof(event_state.arena.bytes));
    offset = event_state.saved_offsets;
    for (index = KF_EVENT_SAVED_SLOT_COUNT - 1; index != -1; index--) {
        *offset++ = KF_EVENT_SAVED_OFFSET_NONE;
    }
    repeat_store_word((u32 *)game_counter_bytes, 0,
                      sizeof(game_counter_bytes) / sizeof(u32));
    game_counter_bytes[KF_ENUM_ENCODE(u8, KF_OBJECT_0)] = 1;
}

ADDRESS(0x8004910c, 0x30)
void callback_invoke_slot_04_zero(void)
{
    ((void (*)(s32))resource_state.active_table[1])(0);
}

ADDRESS(0x8004913c, 0x50)
void event_saved_offsets_decode(u8 **pointers)
{
    u16 *offset = event_state.saved_offsets;
    s32 index = KF_EVENT_SAVED_SLOT_COUNT - 1;
    u16 absent = KF_EVENT_SAVED_OFFSET_NONE;

    for (; index != -1; index--) {
        u16 value = *offset++;
        if (value == absent) {
            *pointers = NULL;
        } else {
            *pointers = value + event_state.arena.bytes;
        }
        pointers++;
    }
}

ADDRESS(0x8004918c, 0x70)
void event_arena_owner_pointers_add_delta(s32 delta)
{
    KfMemoryBlock *block = &event_state.arena.first_block;

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
    u16 *offset = event_state.saved_offsets;
    s32 index;

    for (index = KF_EVENT_SAVED_SLOT_COUNT - 1; index != -1; index--) {
        u8 *value = *pointers;
        pointers++;
        if (value == NULL) {
            *offset = KF_EVENT_SAVED_OFFSET_NONE;
        } else {
            *offset = (u16)(value - event_state.arena.bytes);
        }
        offset++;
    }
}

ADDRESS(0x80049248, 0x70)
void event_arena_owner_pointers_subtract_delta(s32 delta)
{
    KfMemoryBlock *block = &event_state.arena.first_block;

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
    KfActor *actor = actor_state.actors;
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

    group = actor_state.target_groups;
    for (index = 0; index < (s32)(sizeof(actor_state.target_groups) / sizeof(actor_state.target_groups[0]));
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

    object = map_object_state.objects;
    for (index = 0; index < KF_MAP_OBJECT_CAPACITY; object++, index++) {
        KF_ENUM_PROMOTED(KfObjectId) object_id = object->object_id;
        KF_ENUM_PROMOTED(KfMapObjectOperation) kind;
        if (object_id == KF_OBJECT_NONE) {
            *write++ = KF_EVENT_WORLD_SAVE_EMPTY;
            continue;
        }
        kind = map_object_state.templates[KF_ENUM_ENCODE(s32, object_id)].collision_kind;
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
    block = memory_arena_allocate_block(&event_state.arena.first_block,
                                        size, &saved[save_slot]);
    if (block != NULL) {
        resource_copy_words((u32 *)block, (const u32 *)payload, size >> 2);
        event_saved_offsets_encode(saved);
        event_arena_owner_pointers_subtract_delta((s32)saved);
    }
}

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
        actor = &actor_state.actors[actor_index];
        actor->lifecycle = KF_ENUM_DECODE(KF_ENUM_PROMOTED(KfActorLifecycle), *stream++);
    }

    for (;;) {
        s32 group_index = *stream++;
        KfTargetGroup *group;
        KfTargetCandidate *candidate;
        if (group_index == KF_EVENT_WORLD_SAVE_LIST_END) {
            break;
        }
        group = &actor_state.target_groups[group_index];
        candidate = group->targets[0].pointer;
        candidate->word_10.bytes.fallback_offset = *stream++;
        candidate->word_12.bytes.marker_state = *stream++;
    }

    object = map_object_state.objects;
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
            object->action_timer = 0x63;
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
    KfMagicRecord *record = effect_state.magic_records;
    s32 i;

    memcpy((void *)buffer, (const void *)resource_state.active_resource_ids,
        sizeof resource_state.active_resource_ids);
    memcpy((void *)&payload->camera_position, (const void *)&player_state.camera_position,
        sizeof payload->camera_position);
    memcpy((void *)(buffer + KF_CARD_SAVE_ROTATION_OFFSET),
        (const void *)&player_state.camera_rotation_target, sizeof player_state.camera_rotation_target);

    saved->experience = player_state.experience;
    saved->next_level_experience = player_state.next_level_experience;
    saved->gold = player_state.gold;
    saved->map_layer_index = player_state.map_layer_index;
    saved->vitals.maximum_hp = player_state.vitals.maximum_hp;
    saved->vitals.current_hp = player_state.vitals.current_hp;
    saved->vitals.maximum_mp = player_state.vitals.maximum_mp;
    saved->vitals.current_mp = player_state.vitals.current_mp;
    saved->base_physical_power = player_state.base_physical_power;
    saved->base_magic = player_state.base_magic;
    saved->physical_power_training = player_state.physical_power_training;
    saved->magic_training = player_state.magic_training;
    saved->poison_timer = player_state.poison_timer;
    saved->curse_strength = player_state.curse_strength;
    saved->curse_phase_limit = player_state.curse_phase_limit;
    saved->darkness_phase = player_state.darkness_phase;
    saved->darkness_phase_limit = player_state.darkness_phase_limit;
    saved->slow_timer = player_state.slow_timer;
    saved->paralysis_timer = player_state.paralysis_timer;
    saved->defense_boost_timer = player_state.defense_boost_timer;
    saved->attack_boost_timer = player_state.attack_boost_timer;
    saved->magic_tint_phase = player_state.magic_tint_phase;
    saved->magic_tint_phase_limit = player_state.magic_tint_phase_limit;
    saved->map_marker_visual_effect_timer = player_state.map_marker_visual_effect_timer;
    saved->full_mp_timer = player_state.full_mp_timer;
    saved->magic_boost_timer = player_state.magic_boost_timer;
    saved->level = player_state.level;
    saved->unknown_09 = player_state.unknown_09;
    saved->equipped_ids[0] = player_state.equipped_head_id;
    saved->equipped_ids[1] = player_state.equipped_body_id;
    saved->equipped_ids[2] = player_state.equipped_arm_id;
    saved->equipped_ids[3] = player_state.equipped_leg_id;
    saved->equipped_ids[4] = player_state.equipped_shield_id;
    saved->equipped_ids[5] = player_state.equipped_accessory_id;
    saved->equipped_ids[6] = player_state.equipped_extra_id;
    saved->primary_magic_shortcut_id = player_state.primary_magic_shortcut_id;
    saved->secondary_magic_shortcut_id = player_state.secondary_magic_shortcut_id;
    saved->secondary_item_shortcut_id = player_state.secondary_item_shortcut_id;
    saved->equipped_weapon_id = player_state.equipped_weapon_id;
    saved->audio_effects_enabled = player_state.audio_effects_enabled;
    saved->audio_music_enabled = player_state.audio_music_enabled;
    saved->hud_gauges_enabled = player_state.hud_gauges_enabled;
    saved->compass_enabled = player_state.compass_enabled;
    saved->item_preview_enabled = player_state.item_preview_enabled;
    saved->walking_bob_enabled = player_state.walking_bob_enabled;

    for (i = 63; i != -1; --i) {
        *magic_flag++ = record->menu_available;
        ++record;
    }
    memcpy((void *)payload->game_counters, (const void *)game_counter_bytes,
        sizeof payload->game_counters);
    memcpy((void *)payload->event_control, (const void *)event_state.control.bytes,
        sizeof payload->event_control);
    memcpy((void *)payload->event_arena, (const void *)event_state.arena.bytes,
        sizeof payload->event_arena);
    memcpy((void *)payload->saved_event_offsets, (const void *)event_state.saved_offsets,
        sizeof payload->saved_event_offsets);
}

ADDRESS(0x8004a040, 0x5e0)
void card_payload_restore_game_state(const u8 *buffer)
{
    const KfCardSavePayload *payload = (const KfCardSavePayload *)buffer;
    const KfCardPlayerSnapshot *saved = &payload->player;
    const u8 *magic_flag = payload->magic_menu_available;
    KfMagicRecord *record = effect_state.magic_records;
    s32 i;

    memcpy((void *)resource_state.active_resource_ids, (const void *)buffer,
        sizeof resource_state.active_resource_ids);
    resource_state.active_resource_ids[KF_RESOURCE_SLOT_TMD] = resource_state.active_resource_ids[KF_RESOURCE_SLOT_MAP_REGION];
    resource_state.active_resource_ids[KF_RESOURCE_SLOT_TIM] = resource_state.active_resource_ids[KF_RESOURCE_SLOT_MAP_REGION];
    resource_state.active_resource_ids[KF_RESOURCE_SLOT_VAB] = resource_state.active_resource_ids[KF_RESOURCE_SLOT_MAP_REGION];
    resource_state.active_resource_ids[KF_RESOURCE_SLOT_SEQUENCE] = resource_state.active_resource_ids[KF_RESOURCE_SLOT_MAP_REGION];
    memcpy((void *)&player_state.camera_position, (const void *)&payload->camera_position,
        sizeof player_state.camera_position);
    memcpy((void *)&player_state.camera_rotation_target,
        (const void *)(buffer + KF_CARD_SAVE_ROTATION_OFFSET), sizeof player_state.camera_rotation_target);

    player_state.experience = saved->experience;
    player_state.next_level_experience = saved->next_level_experience;
    player_state.gold = saved->gold;
    player_state.map_layer_index = saved->map_layer_index;
    player_state.vitals.maximum_hp = saved->vitals.maximum_hp;
    player_state.vitals.current_hp = saved->vitals.current_hp;
    player_state.vitals.maximum_mp = saved->vitals.maximum_mp;
    player_state.vitals.current_mp = saved->vitals.current_mp;
    player_state.base_physical_power = saved->base_physical_power;
    player_state.base_magic = saved->base_magic;
    player_state.physical_power_training = saved->physical_power_training;
    player_state.magic_training = saved->magic_training;
    player_state.poison_timer = saved->poison_timer;
    player_state.curse_strength = saved->curse_strength;
    player_state.curse_phase_limit = saved->curse_phase_limit;
    player_state.darkness_phase = saved->darkness_phase;
    player_state.darkness_phase_limit = saved->darkness_phase_limit;
    player_state.slow_timer = saved->slow_timer;
    player_state.paralysis_timer = saved->paralysis_timer;
    player_state.defense_boost_timer = saved->defense_boost_timer;
    player_state.attack_boost_timer = saved->attack_boost_timer;
    player_state.magic_tint_phase = saved->magic_tint_phase;
    player_state.magic_tint_phase_limit = saved->magic_tint_phase_limit;
    player_state.map_marker_visual_effect_timer = saved->map_marker_visual_effect_timer;
    player_state.full_mp_timer = saved->full_mp_timer;
    player_state.magic_boost_timer = saved->magic_boost_timer;
    player_state.level = saved->level;
    player_state.unknown_09 = saved->unknown_09;
    player_state.equipped_head_id = saved->equipped_ids[0];
    player_state.equipped_body_id = saved->equipped_ids[1];
    player_state.equipped_arm_id = saved->equipped_ids[2];
    player_state.equipped_leg_id = saved->equipped_ids[3];
    player_state.equipped_shield_id = saved->equipped_ids[4];
    player_state.equipped_accessory_id = saved->equipped_ids[5];
    player_state.equipped_extra_id = saved->equipped_ids[6];
    player_state.primary_magic_shortcut_id = saved->primary_magic_shortcut_id;
    player_state.secondary_magic_shortcut_id = saved->secondary_magic_shortcut_id;
    player_state.secondary_item_shortcut_id = saved->secondary_item_shortcut_id;
    player_state.equipped_weapon_id = saved->equipped_weapon_id;
    player_state.audio_effects_enabled = saved->audio_effects_enabled;
    player_state.audio_music_enabled = saved->audio_music_enabled;
    player_state.hud_gauges_enabled = saved->hud_gauges_enabled;
    player_state.compass_enabled = saved->compass_enabled;
    player_state.item_preview_enabled = saved->item_preview_enabled;
    player_state.walking_bob_enabled = saved->walking_bob_enabled;

    for (i = 63; i != -1; --i) {
        record->menu_available = *magic_flag++;
        ++record;
    }
    memcpy((void *)game_counter_bytes, (const void *)payload->game_counters,
        sizeof payload->game_counters);
    memcpy((void *)event_state.control.bytes, (const void *)payload->event_control,
        sizeof payload->event_control);
    memcpy((void *)event_state.arena.bytes, (const void *)payload->event_arena,
        sizeof payload->event_arena);
    memcpy((void *)event_state.saved_offsets, (const void *)payload->saved_event_offsets,
        sizeof payload->saved_event_offsets);
}
