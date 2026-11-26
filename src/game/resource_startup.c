#include <kf/lib/address.h>
#include <kf/lib/null.h>
#include <kf/game/actor.h>
#include <kf/game/asset.h>
#include <kf/game/audio.h>
#include <kf/game/callback.h>
#include <kf/game/cd.h>
#include <kf/game/collision_cache.h>
#include <kf/game/effect.h>
#include <kf/game/event_state.h>
#include <kf/game/graphics.h>
#include <kf/game/map_cell.h>
#include <kf/game/map_object.h>
#include <kf/game/map_placed.h>
#include <kf/game/memory.h>
#include <kf/game/player.h>
#include <kf/game/pool.h>
#include <kf/game/resources.h>
#include <kf/game/tmd.h>
#include <psyq/audio.h>
#include <psyq/kernel.h>

enum {
    RESOURCE_SEQUENCE_FADE_START_VOLUME = 60,
    RESOURCE_SEQUENCE_FADE_STEP = 2
};

RODATA(0x80011000, 0x74)

DATA(0x80064b64, 0x80, ".data")
KfCallback callback_default_table[32] = {
    resource_noop_callback, resource_noop_callback, resource_noop_callback, resource_noop_callback,
    resource_noop_callback, resource_noop_callback, resource_noop_callback, resource_noop_callback,
    resource_noop_callback, resource_noop_callback, resource_noop_callback, resource_noop_callback,
    resource_noop_callback, resource_noop_callback, resource_noop_callback, resource_noop_callback,
    resource_noop_callback, resource_noop_callback, resource_noop_callback, resource_noop_callback,
    resource_noop_callback, resource_noop_callback, resource_noop_callback, resource_noop_callback,
    resource_noop_callback, resource_noop_callback, resource_noop_callback, resource_noop_callback,
    resource_noop_callback, resource_noop_callback, resource_noop_callback, resource_noop_callback,
};

DATA(0x800864e4, 0x15000, ".bss")
u8 tmd_archive_28_buffer[0x15000];

DATA(0x800fb014, 0x1968, ".bss")
u8 tmd_0_buffer[0x1968];

DATA(0x8012e9ac, 0x37000, ".bss")
u8 resource_tmd_arc_work[0x37000];

DATA(0x8017e05c, 0x1c, ".bss")
KfResourceState asset_status;

DATA(0x8019f07c, 0x14000, ".bss")
u8 rsc_hook_list[0x14000];

DATA(0x801d9ccc, 0x800, ".bss")
KfEquipmentRecord item_gear_recs[KF_EQUIPMENT_RECORD_COUNT];

ADDRESS(0x80015dcc, 0x8)
/* The default table serves callback slots with different caller arguments. */
void resource_noop_callback()
{
}

enum {
    /* FDAT archive entries: the initial block copied into the source buffer, and the map stream. */
    RESOURCE_FDAT_INITIAL_ENTRY = 0x30,
    RESOURCE_FDAT_STREAM_ENTRY = 0x2f,
    /* First asset id of the second TMD archive loaded from its own buffer. */
    RESOURCE_TMD_ARCHIVE_ASSET_BASE = 0x28
};

ADDRESS(0x80015dd4, 0x27c)
void resource_initialize_game_assets(void)
{
    u8 *source = (u8 *)KF_GAME_RESOURCE_ARENA_BASE;
    u8 *second_value = &asset_status.active_resource_ids[KF_RESOURCE_SLOT_TMD];

    asset_status.active_resource_ids[KF_RESOURCE_SLOT_VAB] = KF_RESOURCE_ACTIVE_NONE;
    asset_status.active_resource_ids[KF_RESOURCE_SLOT_TIM] = KF_RESOURCE_ACTIVE_NONE;
    *second_value = KF_RESOURCE_ACTIVE_NONE;
    asset_status.transition_active = KF_FALSE;
    asset_status.transition_phase = KF_RESOURCE_TRANSITION_BEGIN_MAP;
    asset_status.active_resource_ids[KF_RESOURCE_SLOT_MAP_REGION] = 0;
    *second_value = 0;
    asset_status.active_resource_ids[KF_RESOURCE_SLOT_TIM] = 0;
    asset_status.active_resource_ids[KF_RESOURCE_SLOT_VAB] = 0;
    asset_status.active_resource_ids[KF_RESOURCE_SLOT_SEQUENCE] = 0;
    asset_status.current_map_region_id = 0;

    cd_archive_open(KF_RESOURCE_ARCHIVE_MO, "COM\\MO.T");
    cd_archive_open(KF_RESOURCE_ARCHIVE_TALK, "COM\\TALK.T");
    cd_archive_open(KF_RESOURCE_ARCHIVE_VAB, "COM\\VAB.T");
    cd_archive_open(KF_RESOURCE_ARCHIVE_FDAT, "COM\\FDAT.T");
    cd_archive_open(KF_RESOURCE_ARCHIVE_RTIM, "COM\\RTIM.T");
    cd_archive_open(KF_RESOURCE_ARCHIVE_RTMD, "COM\\RTMD.T");
    cd_archive_open(KF_RESOURCE_ARCHIVE_ITEM, "COM\\ITEM.T");

    /* The archive copy destinations have provisional BSS extents. */
    cd_archive_read(KF_RESOURCE_ARCHIVE_FDAT, RESOURCE_FDAT_INITIAL_ENTRY, (u_long *)source);
    cd_map_stream_read(KF_RESOURCE_ARCHIVE_FDAT, RESOURCE_FDAT_STREAM_ENTRY);
    audio_queue_vab_stream(KF_RESOURCE_ARCHIVE_VAB, 0, 0);

    resource_copy_words((u32 *)&map_objects_slots, (u32 *)(source + 4), *(u32 *)source >> 2);
    source += *(u32 *)source + 4;
    resource_copy_words((u32 *)g_weapon_table, (u32 *)(source + 4), *(u32 *)source >> 2);
    source += *(u32 *)source + 4;
    resource_copy_words((u32 *)item_gear_recs, (u32 *)(source + 4), *(u32 *)source >> 2);
    source += *(u32 *)source + 4;
    resource_copy_words((u32 *)player_lv_grow_list, (u32 *)(source + 4), *(u32 *)source >> 2);
    source += *(u32 *)source + 4;
    resource_copy_words((u32 *)&effect_status, (u32 *)(source + 4), *(u32 *)source >> 2);
    source += *(u32 *)source + 4;
    resource_copy_words((u32 *)collision_default_rows, (u32 *)(source + 4), *(u32 *)source >> 2);
    source += *(u32 *)source + 4;
    resource_copy_words((u32 *)sound_status.voices.params, (u32 *)(source + 4), *(u32 *)source >> 2);
    source += *(u32 *)source + 4;
    resource_copy_words((u32 *)tmd_0_buffer, (u32 *)(source + 4), *(u32 *)source >> 2);
    source += *(u32 *)source + 4;
    asset_registry_load_tmd_archive(0, tmd_0_buffer);
    resource_copy_words((u32 *)tmd_archive_28_buffer, (u32 *)(source + 4), *(u32 *)source >> 2);
    asset_registry_load_tmd_archive(RESOURCE_TMD_ARCHIVE_ASSET_BASE, tmd_archive_28_buffer);
    game_initialize_session();
}

ADDRESS(0x80016050, 0x114)
void resource_run_initial_transition(void)
{
    u8 first;
    u8 second;
    u8 third;
    u8 fourth;
    u8 fifth;

    asset_status.transition_active = KF_TRUE;
    asset_status.transition_phase = KF_RESOURCE_TRANSITION_BEGIN_MAP;
    first = asset_status.active_resource_ids[KF_RESOURCE_SLOT_MAP_REGION];
    second = asset_status.active_resource_ids[KF_RESOURCE_SLOT_TMD];
    third = asset_status.active_resource_ids[KF_RESOURCE_SLOT_TIM];
    fourth = asset_status.active_resource_ids[KF_RESOURCE_SLOT_VAB];
    fifth = asset_status.active_resource_ids[KF_RESOURCE_SLOT_SEQUENCE];
    asset_status.active_resource_ids[KF_RESOURCE_SLOT_MAP_REGION] = KF_RESOURCE_ACTIVE_UNINITIALIZED;
    asset_status.active_resource_ids[KF_RESOURCE_SLOT_TMD] = KF_RESOURCE_ACTIVE_UNINITIALIZED;
    asset_status.active_resource_ids[KF_RESOURCE_SLOT_TIM] = KF_RESOURCE_ACTIVE_UNINITIALIZED;
    asset_status.active_resource_ids[KF_RESOURCE_SLOT_VAB] = KF_RESOURCE_ACTIVE_UNINITIALIZED;
    asset_status.active_resource_ids[KF_RESOURCE_SLOT_SEQUENCE] = KF_RESOURCE_ACTIVE_UNINITIALIZED;
    asset_status.transition_offset.x = KF_RESOURCE_OFFSET_NO_SHIFT;
    asset_status.transition_offset.z = KF_RESOURCE_OFFSET_NO_SHIFT;
    asset_status.transition_offset.y = KF_RESOURCE_OFFSET_NO_SHIFT;
    asset_status.requested_resource_ids[KF_RESOURCE_SLOT_MAP_REGION] = first;
    asset_status.requested_resource_ids[KF_RESOURCE_SLOT_TMD] = second;
    asset_status.requested_resource_ids[KF_RESOURCE_SLOT_TIM] = third;
    asset_status.requested_resource_ids[KF_RESOURCE_SLOT_VAB] = fourth;
    asset_status.requested_resource_ids[KF_RESOURCE_SLOT_SEQUENCE] = fifth;
    do {
        cd_request_yield();
        resource_advance_transition();
    } while (asset_status.transition_active);
    tmd_set_slot(KF_TMD_SLOT_MAP, (KfTmdHeader *)resource_tmd_arc_work);
    asset_status.active_table[5]();
}

ADDRESS(0x80016164, 0x178)
void translate_active_world_positions(s32 dx, s32 dy, s32 dz)
{
    KfEffectRecord *effect = effect_status.records;
    KfMapObject *object;
    KfActor *actor;
    u16 effect_remaining;
    u16 object_remaining;
    u16 actor_remaining;

    cur_avatar_status.camera_position.vx += dx;
    cur_avatar_status.camera_position.vz += dz;
    cur_avatar_status.camera_position.vy += dy;

    effect_remaining = KF_EFFECT_CAPACITY - 1;
    do {
        if (effect->type != KF_EFFECT_SLOT_FREE &&
            (effect->render_flags & KF_EFFECT_RENDER_TRANSFORM_MASK) !=
                KF_EFFECT_RENDER_SCREEN_SPACE) {
            effect->position.vx += dx;
            effect->position.vz += dz;
            effect->position.vy += dy;
        }
        effect++;
    } while (effect_remaining-- != 0);

    object = map_objects_slots.objects;
    object_remaining = KF_MAP_OBJECT_CAPACITY - 1;
    do {
        if (object->object_id != KF_OBJECT_NONE) {
            object->position.vx += dx;
            object->position.vz += dz;
            object->position.vy += dy;
        }
        object++;
    } while (object_remaining-- != 0);

    actor = actor_status.actors;
    actor_remaining = KF_ACTOR_CAPACITY - 1;
    do {
        if (actor->slot_state != KF_ACTOR_SLOT_FREE) {
            actor->position.vx += dx;
            actor->position.vz += dz;
            actor->position.vy += dy;
        }
        actor++;
    } while (actor_remaining-- != 0);
}

/* Retail treats $v0 as live at every exit (an int-returning function whose
 * returns carry no value); callers ignore the result. */
ADDRESS(0x800162dc, 0x55c)
KF_VALUELESS_S32 resource_request_transition(u8 map_region_id, u8 tmd_id, u8 tim_id, u8 vab_id,
                    u8 sequence_id, s8 offset_x, s8 offset_z, s8 offset_y)
{
    u8 current_map_region_id;
    u8 current_tmd_id;
    u8 current_tim_id;
    u8 current_vab_id;
    u8 current_sequence_id;
    u8 prior_map_region_id;
    u8 prior_tmd_id;
    u8 prior_tim_id;
    u8 prior_vab_id;
    u8 prior_sequence_id;

    if (sequence_id == KF_RESOURCE_REQUEST_START_SEQUENCE) {
        if (!sound_status.sequence_active) {
            audio_start_sequence();
        }
        return;
    }

    if (map_region_id == KF_RESOURCE_REQUEST_KEEP) {
        current_map_region_id = asset_status.active_resource_ids[KF_RESOURCE_SLOT_MAP_REGION];
        prior_map_region_id = asset_status.requested_resource_ids[KF_RESOURCE_SLOT_MAP_REGION];
        if (tmd_id == KF_RESOURCE_REQUEST_KEEP) {
            current_tmd_id = asset_status.active_resource_ids[KF_RESOURCE_SLOT_TMD];
            prior_tmd_id = asset_status.requested_resource_ids[KF_RESOURCE_SLOT_TMD];
        } else {
            prior_tmd_id = tmd_id;
            current_tmd_id = tmd_id;
        }
        if (tim_id == KF_RESOURCE_REQUEST_KEEP) {
            current_tim_id = asset_status.active_resource_ids[KF_RESOURCE_SLOT_TIM];
            prior_tim_id = asset_status.requested_resource_ids[KF_RESOURCE_SLOT_TIM];
        } else {
            prior_tim_id = tim_id;
            current_tim_id = tim_id;
        }
        if (vab_id == KF_RESOURCE_REQUEST_KEEP) {
            current_vab_id = asset_status.active_resource_ids[KF_RESOURCE_SLOT_VAB];
            prior_vab_id = asset_status.requested_resource_ids[KF_RESOURCE_SLOT_VAB];
        } else {
            prior_vab_id = vab_id;
            current_vab_id = vab_id;
        }
        if (sequence_id == KF_RESOURCE_REQUEST_KEEP) {
            current_sequence_id = asset_status.active_resource_ids[KF_RESOURCE_SLOT_SEQUENCE];
            prior_sequence_id = asset_status.requested_resource_ids[KF_RESOURCE_SLOT_SEQUENCE];
        } else {
            prior_sequence_id = sequence_id;
            current_sequence_id = sequence_id;
        }
    } else {
        prior_map_region_id = map_region_id;
        current_map_region_id = map_region_id;
        prior_tmd_id = prior_tim_id = prior_vab_id = prior_sequence_id = map_region_id;
        current_tmd_id = asset_status.active_resource_ids[KF_RESOURCE_SLOT_TMD];
        current_tim_id = asset_status.active_resource_ids[KF_RESOURCE_SLOT_TIM];
        current_vab_id = asset_status.active_resource_ids[KF_RESOURCE_SLOT_VAB];
        current_sequence_id = asset_status.active_resource_ids[KF_RESOURCE_SLOT_SEQUENCE];
    }

    if (asset_status.transition_active) {
        goto handle_active;
    }
    if (asset_status.active_resource_ids[KF_RESOURCE_SLOT_MAP_REGION] == current_map_region_id &&
        asset_status.active_resource_ids[KF_RESOURCE_SLOT_TMD] == current_tmd_id &&
        asset_status.active_resource_ids[KF_RESOURCE_SLOT_TIM] == current_tim_id &&
        asset_status.active_resource_ids[KF_RESOURCE_SLOT_VAB] == current_vab_id &&
        asset_status.active_resource_ids[KF_RESOURCE_SLOT_SEQUENCE] == current_sequence_id) {
        return;
    }

apply:
    if (asset_status.active_resource_ids[KF_RESOURCE_SLOT_MAP_REGION] != KF_RESOURCE_ACTIVE_UNINITIALIZED &&
        asset_status.active_resource_ids[KF_RESOURCE_SLOT_MAP_REGION] != current_map_region_id) {
        event_world_state_save_slot(asset_status.active_resource_ids[KF_RESOURCE_SLOT_MAP_REGION]);
    }
    if (cur_evt_state.control.fields.highest_requested_map_region_id < current_map_region_id) {
        cur_evt_state.control.fields.highest_requested_map_region_id = current_map_region_id;
    }
    asset_status.transition_active = KF_TRUE;
    asset_status.transition_phase = KF_RESOURCE_TRANSITION_BEGIN_MAP;
    asset_status.requested_resource_ids[KF_RESOURCE_SLOT_MAP_REGION] = map_region_id;
    asset_status.requested_resource_ids[KF_RESOURCE_SLOT_TMD] = tmd_id;
    asset_status.requested_resource_ids[KF_RESOURCE_SLOT_TIM] = tim_id;
    asset_status.requested_resource_ids[KF_RESOURCE_SLOT_VAB] = vab_id;
    asset_status.requested_resource_ids[KF_RESOURCE_SLOT_SEQUENCE] = sequence_id;
    asset_status.transition_offset.x = offset_x;
    asset_status.transition_offset.z = offset_z;
    asset_status.world_shift_applied = KF_FALSE;
    asset_status.transition_offset.y = offset_y;
    if (tmd_id == KF_RESOURCE_REQUEST_KEEP) {
        asset_status.tmd_object_limit_active = KF_FALSE;
    } else {
        asset_status.tmd_object_limit_active = KF_TRUE;
    }
    return;

handle_active:
    if ((asset_status.transition_active != KF_TRUE ||
         asset_status.requested_resource_ids[KF_RESOURCE_SLOT_MAP_REGION] == prior_map_region_id) &&
        asset_status.requested_resource_ids[KF_RESOURCE_SLOT_TMD] == prior_tmd_id &&
        asset_status.requested_resource_ids[KF_RESOURCE_SLOT_TIM] == prior_tim_id &&
        asset_status.requested_resource_ids[KF_RESOURCE_SLOT_VAB] == prior_vab_id &&
        asset_status.requested_resource_ids[KF_RESOURCE_SLOT_SEQUENCE] == prior_sequence_id) {
        return;
    }
    if ((asset_status.requested_resource_ids[KF_RESOURCE_SLOT_MAP_REGION] == KF_RESOURCE_REQUEST_KEEP &&
         asset_status.active_resource_ids[KF_RESOURCE_SLOT_MAP_REGION] == current_map_region_id) ||
        (asset_status.requested_resource_ids[KF_RESOURCE_SLOT_TMD] == KF_RESOURCE_REQUEST_KEEP &&
         asset_status.active_resource_ids[KF_RESOURCE_SLOT_TMD] == tmd_id) ||
        (asset_status.requested_resource_ids[KF_RESOURCE_SLOT_TIM] == KF_RESOURCE_REQUEST_KEEP &&
         asset_status.active_resource_ids[KF_RESOURCE_SLOT_TIM] == tim_id) ||
        (asset_status.requested_resource_ids[KF_RESOURCE_SLOT_VAB] == KF_RESOURCE_REQUEST_KEEP &&
         asset_status.active_resource_ids[KF_RESOURCE_SLOT_VAB] == vab_id) ||
        (asset_status.requested_resource_ids[KF_RESOURCE_SLOT_SEQUENCE] == KF_RESOURCE_REQUEST_KEEP &&
         asset_status.active_resource_ids[KF_RESOURCE_SLOT_SEQUENCE] == sequence_id)) {
        return;
    }

    if ((asset_status.requested_resource_ids[KF_RESOURCE_SLOT_MAP_REGION] != KF_RESOURCE_REQUEST_KEEP &&
         map_region_id == KF_RESOURCE_REQUEST_KEEP) ||
        (asset_status.requested_resource_ids[KF_RESOURCE_SLOT_TMD] != KF_RESOURCE_REQUEST_KEEP &&
         tmd_id == KF_RESOURCE_REQUEST_KEEP) ||
        (asset_status.requested_resource_ids[KF_RESOURCE_SLOT_TIM] != KF_RESOURCE_REQUEST_KEEP &&
         tim_id == KF_RESOURCE_REQUEST_KEEP) ||
        (asset_status.requested_resource_ids[KF_RESOURCE_SLOT_VAB] != KF_RESOURCE_REQUEST_KEEP &&
         vab_id == KF_RESOURCE_REQUEST_KEEP) ||
        (asset_status.requested_resource_ids[KF_RESOURCE_SLOT_SEQUENCE] != KF_RESOURCE_REQUEST_KEEP &&
         sequence_id == KF_RESOURCE_REQUEST_KEEP)) {
        while (asset_status.transition_active) {
            cd_request_yield();
            resource_advance_transition();
        }
    } else {
        for (;;) {
            EnterCriticalSection();
            if (asset_status.transition_phase != KF_RESOURCE_TRANSITION_PENDING_IO) {
                break;
            }
            ExitCriticalSection();
            cd_request_yield();
        }
    }
    ExitCriticalSection();
    if (asset_status.world_shift_applied != 0 && map_region_id != KF_RESOURCE_REQUEST_KEEP &&
        offset_x == KF_RESOURCE_OFFSET_NO_SHIFT) {
        offset_x = -asset_status.transition_offset.x;
        offset_y = -asset_status.transition_offset.y;
        offset_z = -asset_status.transition_offset.z;
    }
    goto apply;
}

ADDRESS(0x80016838, 0x14)
void resource_transition_set_load_map_cells(KfCdRequest *request)
{
    asset_status.transition_phase = KF_RESOURCE_TRANSITION_LOAD_MAP_CELLS;
}

ADDRESS(0x8001684c, 0x14)
void resource_transition_set_load_map_actors(KfCdRequest *request)
{
    asset_status.transition_phase = KF_RESOURCE_TRANSITION_LOAD_MAP_ACTORS;
}

ADDRESS(0x80016860, 0x14)
void resource_transition_set_queue_map_actors(KfCdRequest *request)
{
    asset_status.transition_phase = KF_RESOURCE_TRANSITION_QUEUE_MAP_ACTORS;
}

ADDRESS(0x80016874, 0x14)
void resource_transition_set_queue_tim(KfCdRequest *request)
{
    asset_status.transition_phase = KF_RESOURCE_TRANSITION_QUEUE_TIM;
}

ADDRESS(0x80016888, 0x14)
void resource_transition_set_finish_audio(KfCdRequest *request)
{
    asset_status.transition_phase = KF_RESOURCE_TRANSITION_FINISH_AUDIO;
}


ADDRESS(0x8001689c, 0x6b4)
void resource_advance_transition(void)
{
    u8 *buffer;
    u8 *stream;
    KfActor *actor;
    KfMapObject *object;
    KF_ENUM_STORAGE(KfResourceTransitionPhase, s32) phase;
    s32 index;

    if (!asset_status.transition_active) {
        return;
    }
    if (asset_status.transition_active != KF_TRUE) {
        return;
    }
    phase = asset_status.transition_phase;

    switch (phase) {
    case KF_RESOURCE_TRANSITION_BEGIN_MAP:
        if (asset_status.requested_resource_ids[KF_RESOURCE_SLOT_MAP_REGION] == KF_RESOURCE_REQUEST_KEEP) {
            goto phase_three;
        }
        asset_status.transition_phase = KF_RESOURCE_TRANSITION_PENDING_IO;
        object = map_objects_slots.objects;
        index = KF_MAP_OBJECT_CAPACITY - 1;
        do {
            object->action = KF_MAP_OBJECT_OP_NONE;
            index--;
            object++;
        } while (index != -1);
        cd_archive_queue_read(KF_RESOURCE_ARCHIVE_FDAT, asset_status.requested_resource_ids[KF_RESOURCE_SLOT_MAP_REGION] * 3,
            (u_long *)cdrom_ctl.stream_work_buffer,
            resource_transition_set_load_map_cells);
        return;

    case KF_RESOURCE_TRANSITION_LOAD_MAP_CELLS:
        buffer = cdrom_ctl.stream_work_buffer;
        asset_status.transition_phase = KF_RESOURCE_TRANSITION_PENDING_IO;
        /* Retail's default table contains 32 pointers to the no-op callback. */
        asset_status.active_table = callback_default_table;
        cd_archive_queue_read(KF_RESOURCE_ARCHIVE_FDAT, asset_status.requested_resource_ids[KF_RESOURCE_SLOT_MAP_REGION] * 3 + 2,
            (u_long *)rsc_hook_list,
            resource_transition_set_queue_map_actors);
        resource_copy_words((u32 *)&pl_ext_7540, (u32 *)(buffer + 4),
                            sizeof(pl_ext_7540.map_cells) / sizeof(u32));
        buffer += *(u32 *)buffer + 4;
        resource_copy_words((u32 *)KF_COLLISION_SHAPE_BANK,
            (u32 *)(buffer + 4), KF_COLLISION_SHAPE_BANK_BYTES / sizeof(u32));
        if (asset_status.transition_offset.x != KF_RESOURCE_OFFSET_NO_SHIFT) {
            asset_status.world_shift_applied = KF_TRUE;
            translate_active_world_positions(asset_status.transition_offset.x << KF_MAP_CELL_POSITION_SHIFT,
                -asset_status.transition_offset.y * 128,
                asset_status.transition_offset.z << KF_MAP_CELL_POSITION_SHIFT);
        }
        asset_status.current_map_region_id = asset_status.requested_resource_ids[KF_RESOURCE_SLOT_MAP_REGION];
        return;

    case KF_RESOURCE_TRANSITION_QUEUE_MAP_ACTORS:
        asset_status.transition_phase = KF_RESOURCE_TRANSITION_PENDING_IO;
        cd_archive_queue_read(KF_RESOURCE_ARCHIVE_FDAT, asset_status.requested_resource_ids[KF_RESOURCE_SLOT_MAP_REGION] * 3 + 1,
            (u_long *)cdrom_ctl.stream_work_buffer,
            resource_transition_set_load_map_actors);
        asset_status.active_table = (KfCallback *)rsc_hook_list;
        return;

    case KF_RESOURCE_TRANSITION_LOAD_MAP_ACTORS:
phase_three:
        if (asset_status.requested_resource_ids[KF_RESOURCE_SLOT_TMD] != KF_RESOURCE_REQUEST_KEEP) {
            asset_status.transition_phase = KF_RESOURCE_TRANSITION_PENDING_IO;
            cd_archive_queue_read(KF_RESOURCE_ARCHIVE_RTMD, asset_status.requested_resource_ids[KF_RESOURCE_SLOT_TMD],
                (u_long *)resource_tmd_arc_work,
                resource_transition_set_queue_tim);
        }
        if (asset_status.requested_resource_ids[KF_RESOURCE_SLOT_MAP_REGION] != KF_RESOURCE_REQUEST_KEEP) {
            stream = cdrom_ctl.stream_work_buffer;
            actor = actor_status.actors;
            index = KF_ACTOR_CAPACITY - 1;
            do {
                if (actor->animation_cache != NULL) {
                    pool_record_release(actor->animation_cache);
                }
                actor->slot_state = KF_ACTOR_SLOT_FREE;
                index--;
                actor++;
            } while (index != -1);
            object = map_objects_slots.objects;
            index = KF_MAP_OBJECT_CAPACITY - 1;
            do {
                if (object->tail.animation_cache != NULL) {
                    pool_record_release(object->tail.animation_cache);
                }
                if (object->action == KF_MAP_OBJECT_OP_PLAYER_REACTION) {
                    memory_free((u8 *)object->extra_40.record);
                    object->action = KF_MAP_OBJECT_OP_NONE;
                }
                object->object_id = KF_OBJECT_NONE;
                index--;
                object++;
            } while (index != -1);
            if (asset_status.transition_offset.x != KF_RESOURCE_OFFSET_NO_SHIFT) {
                KfMapOccupancyCell *cell = &pl_ext_7540.map_cells[0][0];
                index = KF_MAP_WORLD_GRID_SIDE * KF_MAP_WORLD_GRID_SIDE;
                do {
                    cell->layer[0].quarter_turns &= KF_MAP_CELL_QUARTER_TURN_MASK;
                    cell->layer[1].quarter_turns &= KF_MAP_CELL_QUARTER_TURN_MASK;
                    index--;
                    cell++;
                } while (index != 0);
                map_cell_add_layer_occupancy(cur_avatar_status.camera_position.vx,
                    cur_avatar_status.camera_position.vz, KF_PLAYER_COLLISION_RADIUS, 1);
            }
            resource_copy_words((u32 *)actor_status.target_groups,
                (u32 *)(stream + 4),
                (sizeof(actor_status.target_groups) + sizeof(actor_status.target_candidate_blob)) /
                    sizeof(u32));
            actor_fixup_group_targets();
            stream += *(u32 *)stream + 4;
            actor_load_records((const struct KfActorLoadRecord *)(stream + 4));
            stream += *(u32 *)stream + 4;
            map_object_initialize_from_placements((KfMapObjectPlacement *)(stream + 4));
            stream += *(u32 *)stream + 4;
            map_placed_expand_sources((KfMapPlacedSource *)(stream + 4));
            event_world_state_restore_slot(asset_status.requested_resource_ids[KF_RESOURCE_SLOT_MAP_REGION]);
            asset_status.active_table[5]();
            cur_avatar_status.force_actor_lifecycle_refresh = KF_TRUE;
        }
        if (asset_status.requested_resource_ids[KF_RESOURCE_SLOT_TMD] != KF_RESOURCE_REQUEST_KEEP) {
            return;
        }
        /* fall through */

    case KF_RESOURCE_TRANSITION_QUEUE_TIM:
        if (asset_status.requested_resource_ids[KF_RESOURCE_SLOT_TIM] != KF_RESOURCE_REQUEST_KEEP) {
            cd_map_stream_read(KF_RESOURCE_ARCHIVE_RTIM, asset_status.requested_resource_ids[KF_RESOURCE_SLOT_TIM]);
        }
        asset_status.tmd_object_limit_active = KF_FALSE;
        if (asset_status.requested_resource_ids[KF_RESOURCE_SLOT_VAB] != KF_RESOURCE_REQUEST_KEEP ||
            asset_status.requested_resource_ids[KF_RESOURCE_SLOT_SEQUENCE] != KF_RESOURCE_REQUEST_KEEP) {
            goto begin_phase_five;
        }

complete:
        if (asset_status.requested_resource_ids[KF_RESOURCE_SLOT_MAP_REGION] != KF_RESOURCE_REQUEST_KEEP) {
            asset_status.active_resource_ids[KF_RESOURCE_SLOT_MAP_REGION] =
                asset_status.requested_resource_ids[KF_RESOURCE_SLOT_MAP_REGION];
        }
        if (asset_status.requested_resource_ids[KF_RESOURCE_SLOT_TMD] != KF_RESOURCE_REQUEST_KEEP) {
            asset_status.active_resource_ids[KF_RESOURCE_SLOT_TMD] =
                asset_status.requested_resource_ids[KF_RESOURCE_SLOT_TMD];
        }
        if (asset_status.requested_resource_ids[KF_RESOURCE_SLOT_TIM] != KF_RESOURCE_REQUEST_KEEP) {
            asset_status.active_resource_ids[KF_RESOURCE_SLOT_TIM] =
                asset_status.requested_resource_ids[KF_RESOURCE_SLOT_TIM];
        }
        if (asset_status.requested_resource_ids[KF_RESOURCE_SLOT_VAB] != KF_RESOURCE_REQUEST_KEEP) {
            asset_status.active_resource_ids[KF_RESOURCE_SLOT_VAB] =
                asset_status.requested_resource_ids[KF_RESOURCE_SLOT_VAB];
        }
        if (asset_status.requested_resource_ids[KF_RESOURCE_SLOT_SEQUENCE] != KF_RESOURCE_REQUEST_KEEP) {
            asset_status.active_resource_ids[KF_RESOURCE_SLOT_SEQUENCE] =
                asset_status.requested_resource_ids[KF_RESOURCE_SLOT_SEQUENCE];
        }
        asset_status.transition_active = KF_FALSE;
        return;

begin_phase_five:
        asset_status.transition_phase = KF_RESOURCE_TRANSITION_FADE_AUDIO;
        asset_status.sequence_fade_volume = RESOURCE_SEQUENCE_FADE_START_VOLUME;
        sound_status.sequence_ready = KF_FALSE;
        /* fall through */
    case KF_RESOURCE_TRANSITION_FADE_AUDIO:
        if (sound_status.sequence_active) {
            asset_status.sequence_fade_volume -= RESOURCE_SEQUENCE_FADE_STEP;
            if (asset_status.sequence_fade_volume <= 0) {
                asset_status.sequence_fade_volume = 0;
            }
            SsSeqSetVol(sound_status.sequence_id,
                asset_status.sequence_fade_volume,
                asset_status.sequence_fade_volume);
            if (asset_status.sequence_fade_volume != 0) {
                return;
            }
            SsSeqStop(sound_status.sequence_id);
            SsSeqClose(sound_status.sequence_id);
            sound_status.sequence_active = KF_FALSE;
        }
        if (sound_status.vab_slots[1].vab_id != KF_AUDIO_VAB_ID_NONE) {
            sound_status.vab_slots[1].stream_slot = NULL;
        }
        if (asset_status.requested_resource_ids[KF_RESOURCE_SLOT_VAB] != KF_RESOURCE_REQUEST_KEEP) {
            audio_queue_vab_stream(KF_RESOURCE_ARCHIVE_VAB, asset_status.requested_resource_ids[KF_RESOURCE_SLOT_VAB] + 1, 1);
        }
        if (asset_status.requested_resource_ids[KF_RESOURCE_SLOT_SEQUENCE] != KF_RESOURCE_REQUEST_KEEP) {
            cd_archive_queue_read(KF_RESOURCE_ARCHIVE_VAB,
                asset_status.requested_resource_ids[KF_RESOURCE_SLOT_SEQUENCE] % 100 + 320,
                sound_status.sequence_buffer,
                resource_transition_set_finish_audio);
            asset_status.transition_phase = KF_RESOURCE_TRANSITION_PENDING_IO;
            return;
        }
        goto complete;

    case KF_RESOURCE_TRANSITION_FINISH_AUDIO:
        if (asset_status.requested_resource_ids[KF_RESOURCE_SLOT_SEQUENCE] < 100) {
            sound_status.sequence_ready = KF_TRUE;
            audio_start_sequence();
        }
        goto complete;
    default:
        break;
    }
}
