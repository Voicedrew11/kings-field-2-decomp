#include <stdarg.h>
#include <kf/lib/null.h>
#include <kf/lib/address.h>
#include <kf/game/callback.h>
#include <kf/game/cd.h>
#include <kf/game/map_cell.h>
#include <kf/game/map_cell_pattern.h>
#include <kf/game/map_object.h>
#include <kf/game/player.h>
#include <psyq/libc.h>
#include <kf/lib/math.h>
#include <kf/game/animation.h>

RODATA(0x80011484, 0x498)

ADDRESS(0x8003623c, 0x30)
KfAudioPlaybackResult map_object_play_spatial_sound(KfMapObject *object, s32 sound)
{
    return audio_play_spatial_default_range(sound, &object->position, 120, 0);
}

ADDRESS(0x8003626c, 0x5c)
void map_object_pool_reset(void)
{
    KfMapObject *object;
    u16 remaining;

    object = map_object_state.objects;
    remaining = KF_MAP_OBJECT_CAPACITY - 1;
    do {
        object->object_id = KF_OBJECT_NONE;
        object->action = KF_MAP_OBJECT_OP_NONE;
        object->tail.reset_words[2] = 0;
        object->tail.reset_words[1] = 0;
        object->tail.reset_words[0] = 0;
        object++;
    } while (remaining-- != 0);

    map_object_state.placement_drop_sequence = 0;
    map_object_state.definition_drop_sequence = 0;
    map_object_state.spawn_sequence_pool_15e = 0;
}

ADDRESS(0x800362c8, 0x48)
void map_object_reset(KfMapObject *object)
{
    object->asset_clip_selector = KF_ANIMATION_CLIP_STATIC_OBJECT_FIRST;
    object->phase_q12 = 0;
    object->render_queue_mode = KF_RENDER_QUEUE_TEXTURED;
    object->layer_mask = KF_MAP_LAYER_NONE;
    object->rotation.vz = 0;
    object->rotation.vx = 0;
    object->rotation.vy = 0;
    object->scale.vx = object->scale.vy = object->scale.vz = KF_FIXED12_ONE;
    object->action = KF_MAP_OBJECT_OP_NONE;
    object->render_depth_offset = 0;
    object->lighting_override_index = KF_LIGHTING_NONE;
    object->lighting_blend_q12 = 0;
}

ADDRESS(0x80036310, 0xd4)
void map_object_set_property(s32 index, KfMapObjectProperty property, ...)
{
    KfMapObject *object;
    KfMapObjectTemplate *object_template;
    va_list arguments;

    if (index == KF_MAP_OBJECT_INDEX_NONE) {
        return;
    }

    object = &map_object_state.objects[index];
    object_template = &map_object_state.templates[KF_ENUM_ENCODE(u16, object->object_id)];
    va_start(arguments, property);
    switch (property) {
    case KF_MAP_OBJECT_PROPERTY_CLEAR_LAYER_AND_STATE:
        object->layer_mask = KF_MAP_LAYER_NONE;
        object->tail.fields.unknown_38 = KF_MAP_OBJECT_EVENT_DISARMED;
        if (object_template->kind == KF_MAP_OBJECT_KIND_10) {
            object->rotation.vz = KF_ANGLE_QUARTER_TURN;
        }
        break;
    case KF_MAP_OBJECT_PROPERTY_SET_LAYER_MASK:
        object->layer_mask = KF_ENUM_DECODE(KfMapLayerMask, va_arg(arguments, u8));
        break;
    case KF_MAP_OBJECT_PROPERTY_ARM_EVENT:
        object->tail.fields.unknown_38 = KF_MAP_OBJECT_EVENT_ARMED;
        break;
    case KF_MAP_OBJECT_PROPERTY_SET_RENDER_DEPTH:
        object->render_depth_offset = va_arg(arguments, u16);
        break;
    }
    va_end(arguments);
}

ADDRESS(0x800363e4, 0xf4)
void map_object_set_cell_marker(KfMapObject *object, KfMapCellMarkerMode mode, u8 marker)
{
    KfMapOccupancyCell *cell;
    u8 *cell_marker;

    if (mode == KF_MAP_CELL_MARKER_PLACE && player_state.map_marker_visual_effect_timer == 0) {
        s32 cell_z = object->position.vz >> KF_MAP_CELL_POSITION_SHIFT;
        s32 cell_x = object->position.vx >> KF_MAP_CELL_POSITION_SHIFT;
        KfMapOccupancyCell *row = bss_801c7540.map_cells[cell_z];
        cell = &row[cell_x];
        cell_marker = &cell->layer[0].object_index;
        if (object->layer_mask != KF_MAP_LAYER_FIRST) {
            cell_marker = &cell->layer[1].object_index;
        }
        *cell_marker = marker;
        object->scale.vx = object->scale.vy = object->scale.vz = 0;
    } else {
        s32 cell_z = object->position.vz >> KF_MAP_CELL_POSITION_SHIFT;
        s32 cell_x = object->position.vx >> KF_MAP_CELL_POSITION_SHIFT;
        KfMapOccupancyCell *row = bss_801c7540.map_cells[cell_z];
        cell = &row[cell_x];
        cell_marker = &cell->layer[0].object_index;
        if (object->layer_mask != KF_MAP_LAYER_FIRST) {
            cell_marker = &cell->layer[1].object_index;
        }
        *cell_marker = KF_MAP_CELL_MARKER_CLEARED;
        object->scale.vx = object->scale.vy = object->scale.vz = KF_FIXED12_ONE;
    }
}

ADDRESS(0x800364d8, 0xf4)
void map_object_refresh_cell_markers(KfMapCellMarkerMode mode)
{
    KfMapObject *object;
    KfMapObjectTemplate *object_template;
    s32 index;

    object = map_object_state.objects;
    for (index = KF_MAP_OBJECT_CAPACITY; index != 0; object++, index--) {
        if (object->action != KF_MAP_OBJECT_OP_ANIMATED_CONTAINER) {
            if (object->action != KF_MAP_OBJECT_OP_ANIMATED_HAZARD) {
                continue;
            }
            if (object->tail.fields.unknown_3a.bytes.high == 0xff) {
                continue;
            }
            object_template = &map_object_state.templates[KF_ENUM_ENCODE(u16, object->object_id)];
            map_object_set_cell_marker(object, mode,
                                       object_template->params.marker.marker_action_51);
        } else {
            if (object->tail.marker.marker_id == KF_MAP_OBJECT_MARKER_TRIGGERED) {
                continue;
            }
            /* The marker mode doubles as hide (0) / restore-layer (1). */
            map_object_set_property(object->tail.linked_property.linked_object_index,
                                    KF_ENUM_DECODE(KfMapObjectProperty, KF_ENUM_VALUE(mode)),
                                    object->layer_mask);
            object_template = &map_object_state.templates[KF_ENUM_ENCODE(u16, object->object_id)];
            map_object_set_cell_marker(object, mode,
                                       object_template->params.marker.marker_action_05);
        }
    }
}

ADDRESS(0x800365cc, 0x7e4)
void map_object_initialize_from_placements(const KfMapObjectPlacement *placements)
{
    KfMapObject *object = map_object_state.objects;
    u32 frame_count = cd_state.frame_count;
    s32 index;

    for (index = KF_MAP_OBJECT_PLACED_COUNT - 1; index != -1; placements++, object++, index--) {
        const KfMapObjectTemplate *object_template;
        KfMapOccupancyCell *cell;
        KfMapOccupancyCell *row;
        KfMapOccupancyLayer *layer;

        if (placements->object_id == KF_OBJECT_PLACEMENT_NONE) {
            object->object_id = KF_OBJECT_NONE;
        } else {
            object->object_id = placements->object_id;
        }
        object->action = KF_MAP_OBJECT_OP_NONE;
        if (object->object_id == KF_OBJECT_NONE) {
            continue;
        }

        object_template = &map_object_state.templates[KF_ENUM_ENCODE(u16, object->object_id)];
        object->action_timer = 0;
        object->asset_clip_selector = KF_ANIMATION_CLIP_STATIC_OBJECT_FIRST;
        object->phase_q12 = 0;
        object->render_queue_mode = KF_RENDER_QUEUE_TEXTURED;
        object->rotation.vz = 0;
        object->rotation.vx = 0;
        object->rotation.vy = -(s32)placements->rotation_y & KF_ANGLE_WRAP_MASK;
        object->scale.vz = KF_FIXED12_ONE;
        object->scale.vy = KF_FIXED12_ONE;
        object->scale.vx = KF_FIXED12_ONE;
        object->layer_mask = placements->layer_mask;
        object->collision_flags = object_template->collision_flags;
        object->render_depth_offset = object_template->initial_render_depth_offset;
        object->lighting_override_index = KF_LIGHTING_NONE;
        object->lighting_blend_q12 = 0;
        if ((object->collision_flags & KF_MAP_OBJECT_FLAG_UNBIASED_DEPTH) != KF_MAP_OBJECT_FLAGS_NONE) {
            object->render_queue_mode = KF_RENDER_QUEUE_TEXTURED_UNBIASED;
        }
        object->collision_height = object_template->interaction_height;
        object->position.vx = ((u32)placements->region_x << KF_MAP_CELL_POSITION_SHIFT) + placements->local_x;
        object->position.vz = ((u32)placements->region_z << KF_MAP_CELL_POSITION_SHIFT) + placements->local_z;
        row = bss_801c7540.map_cells[placements->region_z];
        cell = &row[placements->region_x];
        layer = cell->layer;
        if (object->layer_mask != KF_MAP_LAYER_FIRST) {
            layer++;
        }
        object->position.vy = placements->height - ((s32)layer->elevation << KF_MAP_CELL_ELEVATION_SHIFT);
        object->tail.placement.copy_words = placements->tail_words;
        memset((void *)&object->extra_40, 0xff, sizeof object->extra_40);

        if (object_template->collision_radius != 0) {
            map_cell_add_layer_occupancy(object->position.vx, object->position.vz,
                          object_template->collision_radius, 1);
        }

        switch (object_template->collision_kind) {
        case KF_MAP_OBJECT_OP_ITEM_PICKUP:
            if (object_template->kind != KF_MAP_OBJECT_KIND_GOLD) {
                if (object->tail.initial_rotation.rotation_x_code != 0xff) {
                    object->rotation.vx =
                        object->tail.initial_rotation.rotation_x_code << 6;
                }
                if (object->tail.initial_rotation.rotation_y_code != 0xff) {
                    object->rotation.vy =
                        object->tail.initial_rotation.rotation_y_code << 6;
                }
                if (object->tail.initial_rotation.rotation_z_code != 0xff) {
                    object->rotation.vz =
                        object->tail.initial_rotation.rotation_z_code << 6;
                }
                if (object->tail.fields.unknown_39 == 0) {
                    object->action = KF_MAP_OBJECT_OP_BOB;
                    object->render_queue_mode = KF_RENDER_QUEUE_BLEND_ADD;
                    object->lighting_override_index = KF_LIGHTING_EFFECT;
                    object->lighting_blend_q12 = KF_FIXED12_ONE;
                    object->extra_40.bob_base_y = object->position.vy;
                }
            }
            break;
        case KF_MAP_OBJECT_OP_LIFT_DOOR:
            object->action = KF_MAP_OBJECT_OP_LIFT_DOOR;
            object->asset_clip_selector = KF_ANIMATION_CLIP_FIRST;
            map_cell_apply_rotated_pattern(object->layer_mask, object->position.vx,
                          object->position.vz, object->rotation.vy,
                          map_object_cell_patterns[8], 0, KF_PATTERN_LIGHTING_UNCHANGED);
            break;
        case KF_MAP_OBJECT_OP_HINGE:
            map_cell_copy_rotated_fields(object->layer_mask,
                          object->tail.cell_copy.source_x + 2,
                          object->tail.cell_copy.source_z,
                          object->tail.cell_copy.destination_x,
                          object->tail.cell_copy.destination_z,
                          2, 2, 0, KF_MAP_CELL_COPY_OBJECT_FIELDS);
            object->action = KF_MAP_OBJECT_OP_HINGE;
            object->action_timer = 2;
            object->extra_40.hinge.progress_ticks = KF_MAP_OBJECT_HINGE_IDLE;
            object->extra_40.hinge.base_yaw = object->rotation.vy;
            map_cell_add_layer_occupancy(object->position.vx, object->position.vz, 3000, 1);
            break;
        case KF_MAP_OBJECT_OP_SIGNAL_DOOR: {
            object->asset_clip_selector = KF_ANIMATION_CLIP_FIRST;
            map_cell_copy_rotated_fields(object->layer_mask,
                          object->tail.cell_copy.source_x +
                              object_template->params.marker.cell_width * 2,
                          object->tail.cell_copy.source_z,
                          object->tail.cell_copy.destination_x,
                          object->tail.cell_copy.destination_z,
                          object_template->params.marker.cell_width,
                          object_template->params.marker.cell_height,
                          object->rotation.vy, KF_MAP_CELL_COPY_OBJECT_FIELDS);
            object->action = KF_MAP_OBJECT_OP_SIGNAL_DOOR;
            map_cell_add_layer_occupancy(object->position.vx, object->position.vz, 0x1130, 1);
            break;
        }
        case KF_MAP_OBJECT_OP_SWITCH:
            object->asset_clip_selector = KF_ANIMATION_CLIP_FIRST;
            object->action = KF_MAP_OBJECT_OP_SWITCH;
            object->action_timer = 9;
            break;
        case KF_MAP_OBJECT_OP_HINGED_CONTAINER:
            object->action = KF_MAP_OBJECT_OP_HINGED_CONTAINER;
            break;
        case KF_MAP_OBJECT_OP_SLIDING_CONTAINER:
            object->action = KF_MAP_OBJECT_OP_SLIDING_CONTAINER;
            break;
        case KF_MAP_OBJECT_OP_ANIMATED_CONTAINER:
            object->action = KF_MAP_OBJECT_OP_ANIMATED_CONTAINER;
            object->asset_clip_selector = KF_ANIMATION_CLIP_FIRST;
            map_object_set_cell_marker(object, KF_MAP_CELL_MARKER_PLACE, object_template->params.marker.marker_action_05);
            break;
        case KF_MAP_OBJECT_OP_RESOURCE_TRIGGER:
            object->layer_mask = KF_MAP_LAYER_NONE;
            object->action = KF_MAP_OBJECT_OP_RESOURCE_TRIGGER;
            object->position.vx -= placements->local_x;
            object->position.vz -= placements->local_z;
            object->position.vy -= placements->height;
            if (placements->local_x == 0xff) {
                object->extra_40.resource_offsets.offset_y = 0x7f;
                object->extra_40.resource_offsets.offset_z = 0x7f;
                object->extra_40.resource_offsets.offset_x = 0x7f;
            } else {
                object->extra_40.resource_offsets.offset_x =
                    (s8)placements->local_x - (s8)placements->region_x;
                object->extra_40.resource_offsets.offset_z =
                    (s8)placements->local_z - (s8)placements->region_z;
                object->extra_40.resource_offsets.offset_y =
                    (s8)placements->height -
                    (s8)(-object->position.vy >> KF_MAP_CELL_ELEVATION_SHIFT);
            }
            break;
        case KF_MAP_OBJECT_OP_WARP:
            object->asset_clip_selector = KF_ANIMATION_CLIP_FIRST;
            object->phase_q12 = 0;
            object->action = KF_MAP_OBJECT_OP_WARP;
            object->extra_40.bytes[0] = KF_MAP_OBJECT_LATCH_CLEAR;
            break;
        case KF_MAP_OBJECT_OP_REGION_TRIGGER:
            object->layer_mask = KF_MAP_LAYER_NONE;
            object->action = KF_MAP_OBJECT_OP_REGION_TRIGGER;
            object->extra_40.bytes[0] = KF_MAP_OBJECT_LATCH_CLEAR;
            break;
        case KF_MAP_OBJECT_OP_SCREEN_IMAGE:
            if (object->tail.initial_rotation.rotation_x_code != 0xff) {
                object->rotation.vx =
                    object->tail.initial_rotation.rotation_x_code << 6;
            }
            if (object->tail.initial_rotation.rotation_y_code != 0xff) {
                object->rotation.vy =
                    object->tail.initial_rotation.rotation_y_code << 6;
            }
            if (object->tail.initial_rotation.rotation_z_code != 0xff) {
                object->rotation.vz =
                    object->tail.initial_rotation.rotation_z_code << 6;
            }
            break;
        case KF_MAP_OBJECT_OP_AMBIENT_SOUND:
            object->layer_mask = KF_MAP_LAYER_NONE;
            object->action = KF_MAP_OBJECT_OP_AMBIENT_SOUND;
            object->extra_40.next_sound_frame = frame_count +
                object->tail.ambient_sound.repeat_delay_units * 6;
            break;
        case KF_MAP_OBJECT_OP_ANIMATED_MODEL:
            object->action = KF_MAP_OBJECT_OP_ANIMATED_MODEL;
            break;
        case KF_MAP_OBJECT_OP_PATTERN_GATE:
            object->asset_clip_selector = KF_ANIMATION_CLIP_FIRST;
            object->extra_40.saved_layer.layer_mask = object->layer_mask;
            object->layer_mask = KF_MAP_LAYER_BOTH;
            object->action = KF_MAP_OBJECT_OP_PATTERN_GATE;
            map_cell_apply_rotated_pattern(object->extra_40.saved_layer.layer_mask, object->position.vx,
                          object->position.vz, object->rotation.vy,
                          map_object_cell_patterns[
                              object_template->params.pattern.pattern_pair_index * 2 +
                              (object->tail.action_84_pattern.pattern_flags & KF_MAP_OBJECT_PATTERN_VARIANT)],
                          KF_MAP_OBJECT_PATTERN_OFF, KF_PATTERN_LIGHTING_CLEAR_FLAGS);
            object->scale.vz = 0;
            object->scale.vy = 0;
            object->scale.vx = 0;
            break;
        case KF_MAP_OBJECT_OP_ANIMATED_HAZARD:
            object->asset_clip_selector = KF_ANIMATION_CLIP_FIRST;
            object->action = KF_MAP_OBJECT_OP_ANIMATED_HAZARD;
            object->extra_40.bytes[0] = KF_MAP_OBJECT_LATCH_CLEAR;
            if (object->tail.fields.unknown_3a.bytes.high != 0xff) {
                map_object_set_cell_marker(object, KF_MAP_CELL_MARKER_PLACE,
                                           object_template->params.marker.marker_action_51);
            }
            break;
        case KF_MAP_OBJECT_OP_CELL_COPY_TOGGLE:
            object->asset_clip_selector = KF_ANIMATION_CLIP_FIRST;
            object->action = KF_MAP_OBJECT_OP_CELL_COPY_TOGGLE;
            object->action_timer = 1;
            break;
        case KF_MAP_OBJECT_OP_LAYER_FADE: {
            KfMapOccupancyCell *kind59_row;
            KfMapOccupancyCell *kind59_cell;
            KfMapOccupancyLayer *kind59_layer;

            object->action = KF_MAP_OBJECT_OP_LAYER_FADE;
            object->render_queue_mode = KF_RENDER_QUEUE_BLEND_ADD;
            object->lighting_override_index = KF_LIGHTING_PRESET_42;
            object->lighting_blend_q12 = KF_FIXED12_ONE;
            object->position.vy += 0x100;
            kind59_row = bss_801c7540.map_cells[object->position.vz >> KF_MAP_CELL_POSITION_SHIFT];
            kind59_cell = &kind59_row[object->position.vx >> KF_MAP_CELL_POSITION_SHIFT];
            kind59_layer = kind59_cell->layer;
            if (object->layer_mask != KF_MAP_LAYER_FIRST) {
                kind59_layer++;
            }
            kind59_layer->collision_shape_id = 0x75;
            object->extra_40.layer_fade.original_layer_mask = object->layer_mask;
            object->layer_mask = KF_MAP_LAYER_NONE;
            break;
        }
        case KF_MAP_OBJECT_OP_11:
        case KF_MAP_OBJECT_OP_HIDDEN_SCREEN_IMAGE:
            object->layer_mask = KF_MAP_LAYER_NONE;
            break;
        case KF_MAP_OBJECT_OP_GROW_ITEM:
            object->action = KF_MAP_OBJECT_OP_GROW_ITEM;
            break;
        case KF_MAP_OBJECT_OP_RAISED_ADDITIVE:
            object->action = KF_MAP_OBJECT_OP_RAISED_ADDITIVE;
            object->render_queue_mode = KF_RENDER_QUEUE_BLEND_ADD;
            object->position.vy -=
                (u8)object->tail.fields.spawn_sequence * 0x100;
            break;
        case KF_MAP_OBJECT_OP_RESTORE_POINT:
            object->action = KF_MAP_OBJECT_OP_RESTORE_POINT;
            object->extra_40.next_sound_frame = frame_count + 30;
            break;
        case KF_MAP_OBJECT_OP_RECALL_SOCKET:
            object->action = KF_MAP_OBJECT_OP_RECALL_SOCKET;
            break;
        case KF_MAP_OBJECT_OP_DOOR_SOCKET:
            object->action = KF_MAP_OBJECT_OP_DOOR_SOCKET;
            break;
        case KF_MAP_OBJECT_OP_ITEM_CONTAINER:
            object->action = KF_MAP_OBJECT_OP_ITEM_CONTAINER;
            object->extra_40.saved_layer.layer_mask = object->layer_mask;
            break;
        case KF_MAP_OBJECT_OP_HIDDEN_ITEM_CONTAINER:
            object->action = KF_MAP_OBJECT_OP_ITEM_CONTAINER;
            /* Fall through: this kind saves the original layer. */
        case KF_MAP_OBJECT_OP_SCENE_INSPECT:
            object->extra_40.saved_layer.layer_mask = object->layer_mask;
            object->layer_mask = KF_MAP_LAYER_NONE;
            break;
        default:
            ((void (*)(KfMapObject *, const KfMapObjectTemplate *))
                 resource_state.active_table[8])(object, object_template);
            break;
        case KF_MAP_OBJECT_OP_33:
        case KF_MAP_OBJECT_OP_NONE:
            break;
        }
        if (object->object_id == KF_OBJECT_163) {
            object->render_queue_mode = KF_RENDER_QUEUE_BLEND_ADD_QUARTER;
        }
    }
}

ADDRESS(0x80036db0, 0x118)
s32 map_object_find_collision_at_point(s32 x, s32 y, s32 z, s32 radius, s32 point_height)
{
    KfMapObject *object = map_object_state.objects;
    s32 index;

    for (index = 0; index < KF_MAP_OBJECT_CAPACITY; index++, object++) {
        if (object->object_id == KF_OBJECT_NONE) {
            continue;
        }
        if (object->layer_mask == KF_MAP_LAYER_NONE) {
            continue;
        }
        if (object == map_object_state.current_collision_object) {
            continue;
        }
        if (map_object_state.templates[KF_ENUM_ENCODE(u16, object->object_id)].collision_radius == 0) {
            continue;
        }
        if (vector_distance_to_point(
                &object->position, x, y, z,
                map_object_state.templates[KF_ENUM_ENCODE(u16, object->object_id)].collision_radius + radius,
                object->collision_height, point_height) != KF_DISTANCE_NONE) {
            return index;
        }
    }
    return -1;
}

enum { MAP_OBJECT_HINGE_INTERACTION_FORWARD = 0x700 };

ADDRESS(0x80036ec8, 0x22c)
s32 map_object_find_interaction_target(s32 first_index, const VECTOR *position, s32 radius,
                   s32 point_height, s32 angle, s32 tolerance)
{
    KfMapObject *object;
    KfMapObjectTemplate *object_template;
    SVECTOR forward;
    VECTOR rotated;
    MATRIX rotation;
    s16 index;
    s32 direction;

    object = &map_object_state.objects[first_index];
    for (index = first_index; index < KF_MAP_OBJECT_CAPACITY; index++, object++) {
        if (object->object_id == KF_OBJECT_NONE) {
            continue;
        }
        object_template = &map_object_state.templates[KF_ENUM_ENCODE(u16, object->object_id)];
        if (object_template->collision_kind == KF_MAP_OBJECT_OP_HINGE) {
            forward.vx = MAP_OBJECT_HINGE_INTERACTION_FORWARD;
            forward.vy = 0;
            forward.vz = 0;
            matrix_set_rotation_y(object->rotation.vy, &rotation);
            ApplyMatrix(&rotation, &forward, &rotated);
            rotated.vx += position->vx;
            rotated.vz += position->vz;
            if (vector_distance_to_point(&object->position,
                                         rotated.vx, position->vy, rotated.vz,
                                         object_template->interaction_radius + radius,
                                         object->collision_height, point_height) == KF_DISTANCE_NONE) {
                continue;
            }
            direction = vector_xz_to_angle(
                object->position.vx - rotated.vx,
                object->position.vz - rotated.vz);
            if (!angle_within_tolerance(angle, direction, tolerance)) {
                continue;
            }
        } else {
            if (vector_distance_to_point(&object->position,
                                         position->vx, position->vy, position->vz,
                                         object_template->interaction_radius + radius,
                                         object_template->interaction_height, point_height) == KF_DISTANCE_NONE) {
                continue;
            }
            if ((object->collision_flags & KF_MAP_OBJECT_INTERACTION_ANY_ANGLE) == KF_MAP_OBJECT_FLAGS_NONE) {
                direction = vector_xz_to_angle(
                    object->position.vx - position->vx,
                    object->position.vz - position->vz);
                if (!angle_within_tolerance(angle, direction, tolerance)) {
                    continue;
                }
            }
        }
        return index;
    }
    return -1;
}

ADDRESS(0x800370f4, 0x20)
void map_object_start_action_if_idle(KfMapObject *object, KfMapObjectOperation action)
{
    if (object->action == KF_MAP_OBJECT_OP_NONE) {
        object->action = action;
        object->action_timer = KF_MAP_OBJECT_ACTION_TIMER_INIT;
    }
}

ADDRESS(0x80037114, 0x88)
KfMapObject *map_object_effect_pool_acquire(s32 first_index, s32 count, s32 sequence)
{
    KfMapObject *object = &map_object_state.objects[first_index];
    KfMapObject *oldest = NULL;
    s32 oldest_age = 0;
    s32 age;

    do {
        if (object->object_id == KF_OBJECT_NONE) {
            return object;
        }
        if (sequence != -1) {
            age = sequence - object->tail.fields.spawn_sequence;
            if (age < 0) {
                age += KF_MAP_OBJECT_SPAWN_SEQUENCE_MODULUS;
            }
            if (oldest_age < age) {
                oldest = object;
                oldest_age = age;
            }
        }
        object++;
    } while (--count != 0);
    return oldest;
}

ADDRESS(0x8003719c, 0x174)
void map_object_spawn_effect(KfMapObjectDropSource source, KF_ENUM_PARAM(KfObjectId, u8) object_id,
                             const VECTOR *position, s32 height_offset)
{
    KfMapObject *object;
    KfMapObjectTemplate *object_template;

    if (source == KF_MAP_OBJECT_DROP_FROM_PLACEMENT) {
        object = map_object_effect_pool_acquire(KF_MAP_OBJECT_PLACEMENT_DROP_FIRST,
                                                KF_MAP_OBJECT_EFFECT_POOL_SIZE,
                                                map_object_state.placement_drop_sequence);
        object->tail.fields.spawn_sequence = map_object_state.placement_drop_sequence++;
        object->object_id = object_id;
    } else {
        object = map_object_effect_pool_acquire(KF_MAP_OBJECT_DEFINITION_DROP_FIRST,
                                                KF_MAP_OBJECT_EFFECT_POOL_SIZE,
                                                map_object_state.definition_drop_sequence);
        object->tail.fields.spawn_sequence = map_object_state.definition_drop_sequence++;
        object->object_id = object_id;
    }
    map_object_reset(object);
    object_template = &map_object_state.templates[KF_ENUM_ENCODE(u16, object->object_id)];
    object->position.vx = position->vx;
    object->position.vy = height_offset + position->vy;
    object->position.vz = position->vz;
    object->rotation.vy = rand() >> KF_RANDOM_ANGLE_SHIFT;

    switch (object_template->kind) {
    case KF_MAP_OBJECT_KIND_10:
    case KF_MAP_OBJECT_KIND_13:
    case KF_MAP_OBJECT_KIND_16:
        map_object_start_action_if_idle(object, KF_MAP_OBJECT_OP_FALL_AND_TIP);
        break;
    case KF_MAP_OBJECT_KIND_17:
        map_object_start_action_if_idle(object, KF_MAP_OBJECT_OP_FALL_AND_SPIN);
        break;
    case KF_MAP_OBJECT_KIND_11:
    case KF_MAP_OBJECT_KIND_12:
    case KF_MAP_OBJECT_KIND_14:
    case KF_MAP_OBJECT_KIND_15:
    case KF_MAP_OBJECT_KIND_18:
    case KF_MAP_OBJECT_KIND_19:
    case KF_MAP_OBJECT_KIND_GOLD:
        map_object_start_action_if_idle(object, KF_MAP_OBJECT_OP_BOUNCE);
        if (object_id == KF_OBJECT_103) {
            object->rotation.pad = 0x400;
        } else {
            object->rotation.pad = 0;
        }
        break;
    }
    object->tail.motion.motion_velocity.value = 0;
    object->tail.fields.unknown_38 = KF_MAP_OBJECT_EVENT_ARMED;
}

enum { KF_MAP_OBJECT_SCATTER_RADIUS = 600 };



ADDRESS(0x80037310, 0x124)
void map_object_spawn_scattered_effect(u16 effect_id, const VECTOR *origin,
                                       s32 height_offset)
{
    KfMapObject *object;
    u16 angle;

    object = map_object_effect_pool_acquire(KF_MAP_OBJECT_SCATTER_POOL_FIRST,
                                            KF_MAP_OBJECT_EFFECT_POOL_SIZE,
                                            map_object_state.spawn_sequence_pool_15e);
    map_object_reset(object);
    object->tail.scattered_effect.spawn_sequence = map_object_state.spawn_sequence_pool_15e++;
    object->object_id = KF_OBJECT_70;
    object->tail.scattered_effect.effect_id = effect_id;
    angle = (u16)(rand() >> KF_RANDOM_ANGLE_SHIFT);
    object->position.vx = origin->vx +
        ((rsin(angle) * KF_MAP_OBJECT_SCATTER_RADIUS) >> KF_FIXED12_BITS);
    object->position.vy = origin->vy + height_offset;
    object->position.vz = origin->vz +
        ((rcos(angle) * KF_MAP_OBJECT_SCATTER_RADIUS) >> KF_FIXED12_BITS);
    object->rotation.vy = rand() >> KF_RANDOM_ANGLE_SHIFT;
    object->tail.fields.unknown_38 = KF_MAP_OBJECT_EVENT_ARMED;
    map_object_start_action_if_idle(object, KF_MAP_OBJECT_OP_BOUNCE);
    object->tail.motion.motion_velocity.signed_value = -120;
}

ADDRESS(0x80037434, 0x1b8)
void map_object_apply_marker_signal(u8 identifier)
{
    KfMapObject *object = map_object_state.objects;
    u16 remaining;

    if (identifier == 0xff) {
        return;
    }
    remaining = KF_MAP_OBJECT_CAPACITY - 1;
    do {
        switch (object->action) {
        case KF_MAP_OBJECT_OP_80:
        case KF_MAP_OBJECT_OP_PATTERN_GATE:
        case KF_MAP_OBJECT_OP_SIGNAL_CELL_COPY:
        case KF_MAP_OBJECT_OP_SIGNAL_EVENT_BIT:
        case KF_MAP_OBJECT_OP_EVENT_BIT_ACTIVATED:
            if (object->tail.marker.marker_id == identifier) {
                object->tail.marker.marker_id = KF_MAP_OBJECT_MARKER_CLEARED;
            }
            break;
        case KF_MAP_OBJECT_OP_CELL_COPY_TOGGLE:
            if (object->tail.action_88_cell_copy.marker_id == identifier) {
                object->action_timer = 1;
                object->tail.action_88_cell_copy.transition_mode = KF_ENUM_DECODE(KfMapObjectCellCopyMode,
                    object->tail.action_88_cell_copy.transition_mode == KF_MAP_OBJECT_CELL_COPY_REVERT);
            }
            break;
        case KF_MAP_OBJECT_OP_ANIMATED_HAZARD:
            if (object->tail.action_51_marker.marker_id == identifier) {
                object->tail.collision_probe.marker_trigger_state =
                    object->tail.collision_probe.marker_trigger_state == KF_MAP_OBJECT_PROBE_STOPPED
                        ? KF_MAP_OBJECT_PROBE_RUNNING : KF_MAP_OBJECT_PROBE_STOPPED;
            }
            break;
        case KF_MAP_OBJECT_OP_LAYER_FADE:
            if (object->tail.action_89_layer_fade.marker_id == identifier) {
                object->action_timer = 1;
            }
            break;
        case KF_MAP_OBJECT_OP_LIFT_DOOR:
        case KF_MAP_OBJECT_OP_SIGNAL_DOOR:
        case KF_MAP_OBJECT_OP_HINGE:
            if (identifier >= 150 && identifier < 199) {
                if ((object->tail.marker.marker_id & KF_MAP_OBJECT_MARKER_PAIR_MASK) == identifier) {
                    object->tail.marker.marker_id ^= 1;
                }
            } else {
                u8 marker = object->tail.marker.marker_id;

                if (marker == identifier) {
                    if (marker >= 200) {
                        object->tail.marker.marker_id = KF_MAP_OBJECT_MARKER_CLEARED;
                    } else if (object->action_timer == 0) {
                        object->action_timer = 1;
                        if (marker >= 100) {
                            object->tail.marker.marker_id = KF_MAP_OBJECT_MARKER_CLEARED;
                        }
                    }
                }
            }
            break;
        }
        object++;
    } while (remaining-- != 0);
}

ADDRESS(0x800375ec, 0x90)
KfMapObjectMarkerCheck map_object_check_and_consume_marker(KfMapObject *object, s32 marker)
{
    switch (object->action) {
    case KF_MAP_OBJECT_OP_LIFT_DOOR:
    case KF_MAP_OBJECT_OP_SIGNAL_DOOR:
    case KF_MAP_OBJECT_OP_HINGE:
        if (object->action_timer != 0) {
            return KF_MAP_OBJECT_MARKER_NOT_APPLICABLE;
        }
        /* Fall through to the active marker check. */
    case KF_MAP_OBJECT_OP_ANIMATED_CONTAINER:
    case KF_MAP_OBJECT_OP_HINGED_CONTAINER:
    case KF_MAP_OBJECT_OP_SLIDING_CONTAINER:
        if (object->tail.marker.marker_id >= KF_MAP_OBJECT_MARKER_TRIGGERED) {
            return KF_MAP_OBJECT_MARKER_ALREADY_CLEARED;
        }
        if (object->tail.marker.marker_id == marker) {
            object->tail.marker.marker_id = KF_MAP_OBJECT_MARKER_CLEARED;
            return KF_MAP_OBJECT_MARKER_CONSUMED;
        }
        return KF_MAP_OBJECT_MARKER_MISMATCH;
    case KF_MAP_OBJECT_OP_RECALL_SOCKET:
    case KF_MAP_OBJECT_OP_DOOR_SOCKET:
        return KF_MAP_OBJECT_MARKER_REFUSED;
    default:
        return KF_MAP_OBJECT_MARKER_NOT_APPLICABLE;
    }
}

ADDRESS(0x8003767c, 0x74)
void map_object_process_marker_all(u8 marker)
{
    KfMapObject *object = map_object_state.objects;
    u16 remaining;

    if (marker == 0xff) {
        return;
    }
    remaining = KF_MAP_OBJECT_CAPACITY - 1;
    do {
        map_object_check_and_consume_marker(object, marker);
        object++;
    } while (remaining-- != 0);
}

ADDRESS(0x800376f0, 0x120)
void map_object_sample_world_vertex(KfMapObject *object, s32 vertex_index, VECTOR *result)
{
    struct KfEulerAngles angles;
    SVECTOR vertex;

    animation_sample_vertex(
        KF_ENUM_ENCODE(u16, object->object_id) + 0x100, object->asset_clip_selector, object->phase_q12,
        vertex_index, &vertex);

    vertex.vx = (vertex.vx * object->scale.vx) >> KF_FIXED12_BITS;
    vertex.vy = (vertex.vy * object->scale.vy) >> KF_FIXED12_BITS;
    vertex.vz = (vertex.vz * object->scale.vz) >> KF_FIXED12_BITS;

    angles.x = object->rotation.vx;
    angles.y = object->rotation.vy + KF_ANGLE_HALF_TURN;
    angles.z = object->rotation.vz;
    vector_rotate_yxz(&angles, &vertex, result);

    addVector(result, &object->position);
}

ADDRESS(0x80037810, 0x90)
b32 player_camera_within_map_region(s32 x, s32 z, s32 width, s32 depth, s32 height)
{
    s32 camera_x = player_state.camera_position.vx >> KF_MAP_CELL_POSITION_SHIFT;
    s32 camera_z = player_state.camera_position.vz >> KF_MAP_CELL_POSITION_SHIFT;

    if (camera_x < x || camera_x >= x + width ||
        camera_z < z || camera_z >= z + depth) {
        goto outside;
    }
    if (height == KF_MAP_REGION_HEIGHT_ANY) {
        return KF_TRUE;
    }
    if (height + 2048 < player_state.camera_position.vy ||
        player_state.camera_position.vy < height - 3200) {
        goto outside;
    }
    return KF_TRUE;
outside:
    return KF_FALSE;
}

enum {
    KF_MAP_OBJECT_MOTION_COMPLETE_ACTION = 0x63,
    KF_MAP_OBJECT_MOTION_STEP = 204,
    KF_MAP_OBJECT_MOTION_LIMIT = 0xfff
};

ADDRESS(0x800378a0, 0x2bc)
b32 map_object_step_offset_motion(KfMapObject *source, KfMapObject *target,
                   SVECTOR *start_offset, SVECTOR *end_offset,
                   b32 brighten, s32 duration)
{
    VECTOR start_position;
    VECTOR end_position;
    s32 fraction;

    switch (source->action_timer) {
    case 0:
        if (source->tail.event_effect.pending_event_command == KF_OBJECT_NONE) {
            return KF_FALSE;
        }
        if (target->tail.fields.unknown_38 == KF_MAP_OBJECT_EVENT_DISARMED) {
            source->extra_40.offset_motion.elapsed_frames = 0;
        } else {
            source->extra_40.offset_motion.elapsed_frames = duration - 1;
        }
        source->action_timer = 1;
        map_object_reset(target);
        target->action = KF_MAP_OBJECT_OP_OFFSET_MOTION;
        target->layer_mask = source->layer_mask;
        if (brighten) {
            target->asset_clip_selector = KF_ANIMATION_CLIP_FIRST;
        } else {
            target->asset_clip_selector = KF_ANIMATION_CLIP_STATIC_OBJECT_FIRST;
        }
        vector_rotate_yxz((const struct KfEulerAngles *)&source->rotation,
                          start_offset, &target->position);
        addVector(&target->position, &source->position);
        target->rotation.vy = source->rotation.vy;
        target->extra_40.object_index = source - map_object_state.objects;
        break;
    case 1:
        break;
    default:
        return KF_FALSE;
    }

    if (brighten) {
        target->phase_q12 += KF_MAP_OBJECT_MOTION_STEP;
        if (target->phase_q12 >= KF_FIXED12_ONE) {
            target->phase_q12 = KF_MAP_OBJECT_MOTION_LIMIT;
        }
    }
    vector_rotate_yxz((const struct KfEulerAngles *)&source->rotation,
                      start_offset, &start_position);
    vector_rotate_yxz((const struct KfEulerAngles *)&source->rotation,
                      end_offset, &end_position);
    source->extra_40.offset_motion.elapsed_frames++;
    fraction = ((s32)source->extra_40.offset_motion.elapsed_frames << KF_FIXED12_BITS) / duration;
    target->position.vx = source->position.vx +
        fixed_lerp_q12(start_position.vx, end_position.vx, fraction);
    target->position.vy = source->position.vy +
        fixed_lerp_q12(start_position.vy, end_position.vy, fraction);
    target->position.vz = source->position.vz +
        fixed_lerp_q12(start_position.vz, end_position.vz, fraction);
    if (source->extra_40.offset_motion.elapsed_frames >= duration) {
        if (brighten) {
            target->phase_q12 = KF_MAP_OBJECT_MOTION_LIMIT;
        }
        map_object_play_spatial_sound(target, 0x42);
        target->tail.fields.unknown_38 = KF_MAP_OBJECT_EVENT_ARMED;
        source->action_timer = KF_MAP_OBJECT_MOTION_COMPLETE_ACTION;
        return KF_TRUE;
    }
    return KF_FALSE;
}

/* Unreferenced in every retail build. The jump tables' .align 3 offsets on
   both sides place this word at the end of the unit's .rdata, after the
   marker-check table; GCC 2.5.7 emits an initialized C static const at its
   declaration even when nothing reads it. */
static const u32 map_object_unreferenced_rodata_word = 0;

DATA(0x8006877c, 0x10e, ".data")
KfMapCellPattern map_object_cell_patterns
    [KF_MAP_OBJECT_PATTERN_GROUPS][KF_MAP_OBJECT_PATTERN_ROWS] = {
    {
        { {{0x01, 0x01, 0x14, 0x01}, {0x68, 0x69, 0xfe, 0xfe}}, 0, 0 },
        { {{0x01, 0x01, 0x14, 0x01}, {0x68, 0x69, 0xfe, 0xfe}}, -1, 0 },
        { {{0xff, 0xff, 0xff, 0xff}, {0xff, 0xff, 0xff, 0xff}}, 0, 0 },
    },
    {
        { {{0x01, 0x6b, 0x14, 0x01}, {0x68, 0x69, 0xfe, 0xfe}}, 0, 0 },
        { {{0x01, 0x6b, 0x14, 0x01}, {0x68, 0x69, 0xfe, 0xfe}}, -1, 0 },
        { {{0xff, 0xff, 0xff, 0xff}, {0xff, 0xff, 0xff, 0xff}}, 0, 0 },
    },
    {
        { {{0x01, 0x01, 0x15, 0x17}, {0x68, 0x69, 0xfe, 0xfe}}, 0, 0 },
        { {{0x01, 0x01, 0x15, 0x17}, {0x68, 0x69, 0xfe, 0xfe}}, -1, 0 },
        { {{0xff, 0xff, 0xff, 0xff}, {0xff, 0xff, 0xff, 0xff}}, 0, 0 },
    },
    {
        { {{0x01, 0x6b, 0x15, 0x17}, {0x68, 0x69, 0xfe, 0xfe}}, 0, 0 },
        { {{0x01, 0x6b, 0x15, 0x17}, {0x68, 0x69, 0xfe, 0xfe}}, -1, 0 },
        { {{0xff, 0xff, 0xff, 0xff}, {0xff, 0xff, 0xff, 0xff}}, 0, 0 },
    },
    {
        { {{0x00, 0x00, 0x18, 0x00}, {0xaa, 0xab, 0xfe, 0xfe}}, 0, 0 },
        { {{0x00, 0x00, 0x18, 0x00}, {0xaa, 0xab, 0xfe, 0xfe}}, -1, 0 },
        { {{0xff, 0xff, 0xff, 0xff}, {0xff, 0xff, 0xff, 0xff}}, 0, 0 },
    },
    {
        { {{0x00, 0x00, 0x00, 0x00}, {0x00, 0x00, 0x00, 0x00}}, 0, 0 },
        { {{0x00, 0x00, 0x00, 0x00}, {0x00, 0x00, 0x00, 0x00}}, 0, 0 },
        { {{0x00, 0x00, 0x00, 0x00}, {0x00, 0x00, 0x00, 0x00}}, 0, 0 },
    },
    {
        { {{0x00, 0x00, 0x19, 0x16}, {0xaa, 0xab, 0xfe, 0xfe}}, 0, 0 },
        { {{0x00, 0x00, 0x19, 0x16}, {0xaa, 0xab, 0xfe, 0xfe}}, -1, 0 },
        { {{0xff, 0xff, 0xff, 0xff}, {0xff, 0xff, 0xff, 0xff}}, 0, 0 },
    },
    {
        { {{0x00, 0x00, 0x00, 0x00}, {0x00, 0x00, 0x00, 0x00}}, 0, 0 },
        { {{0x00, 0x00, 0x00, 0x00}, {0x00, 0x00, 0x00, 0x00}}, 0, 0 },
        { {{0x00, 0x00, 0x00, 0x00}, {0x00, 0x00, 0x00, 0x00}}, 0, 0 },
    },
    {
        { {{0x01, 0xfe, 0xff, 0xff}, {0x3c, 0xfe, 0xff, 0xff}}, 0, 0 },
        { {{0x01, 0xfe, 0xff, 0xff}, {0x3c, 0xfe, 0xff, 0xff}}, -1, 0 },
        { {{0xff, 0xff, 0xff, 0xff}, {0xff, 0xff, 0xff, 0xff}}, 0, 0 },
    },
};

DATA(0x80175914, 0x8744, ".bss")
KfMapObjectStateGame map_object_state;
