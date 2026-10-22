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
u8 resource_tmd_archive_28_workspace[0x15000];

DATA(0x800fb014, 0x1960, ".bss")
u8 resource_tmd_archive_0_workspace[0x1960];

DATA(0x8012e9ac, 0x37000, ".bss")
u8 resource_tmd_workspace[0x37000];

DATA(0x8017e05c, 0x1c, ".bss")
KfResourceState resource_state;

DATA(0x8019f07c, 0x14000, ".bss")
u8 resource_callback_table_workspace[0x14000];

DATA(0x801d9ccc, 0x800, ".bss")
KfEquipmentRecord player_equipment_records[KF_EQUIPMENT_RECORD_COUNT];

ADDRESS(0x80015dcc, 0x8)
/* The default table serves callback slots with different caller arguments. */
void resource_noop_callback()
{
}

ADDRESS(0x80015dd4, 0x27c)
void resource_initialize_game_assets(void)
{
    u8 *source = (u8 *)KF_GAME_RESOURCE_ARENA_BASE;
    u8 *second_value = &resource_state.active_resource_ids[KF_RESOURCE_SLOT_TMD];

    resource_state.active_resource_ids[KF_RESOURCE_SLOT_VAB] = KF_RESOURCE_ACTIVE_NONE;
    resource_state.active_resource_ids[KF_RESOURCE_SLOT_TIM] = KF_RESOURCE_ACTIVE_NONE;
    *second_value = KF_RESOURCE_ACTIVE_NONE;
    resource_state.transition_active = KF_FALSE;
    resource_state.transition_phase = KF_RESOURCE_TRANSITION_BEGIN_MAP;
    resource_state.active_resource_ids[KF_RESOURCE_SLOT_MAP_REGION] = 0;
    *second_value = 0;
    resource_state.active_resource_ids[KF_RESOURCE_SLOT_TIM] = 0;
    resource_state.active_resource_ids[KF_RESOURCE_SLOT_VAB] = 0;
    resource_state.active_resource_ids[KF_RESOURCE_SLOT_SEQUENCE] = 0;
    resource_state.current_map_region_id = 0;

    cd_archive_open(KF_RESOURCE_ARCHIVE_MO, "COM\\MO.T");
    cd_archive_open(KF_RESOURCE_ARCHIVE_TALK, "COM\\TALK.T");
    cd_archive_open(KF_RESOURCE_ARCHIVE_VAB, "COM\\VAB.T");
    cd_archive_open(KF_RESOURCE_ARCHIVE_FDAT, "COM\\FDAT.T");
    cd_archive_open(KF_RESOURCE_ARCHIVE_RTIM, "COM\\RTIM.T");
    cd_archive_open(KF_RESOURCE_ARCHIVE_RTMD, "COM\\RTMD.T");
    cd_archive_open(KF_RESOURCE_ARCHIVE_ITEM, "COM\\ITEM.T");

    /* The archive copy destinations have provisional BSS extents. */
    cd_archive_read(KF_RESOURCE_ARCHIVE_FDAT, 0x30, (u_long *)source);
    cd_map_stream_read(KF_RESOURCE_ARCHIVE_FDAT, 0x2f);
    audio_queue_vab_stream(KF_RESOURCE_ARCHIVE_VAB, 0, 0);

    resource_copy_words((u32 *)&map_object_state, (u32 *)(source + 4), *(u32 *)source >> 2);
    source += *(u32 *)source + 4;
    resource_copy_words((u32 *)player_weapon_records, (u32 *)(source + 4), *(u32 *)source >> 2);
    source += *(u32 *)source + 4;
    resource_copy_words((u32 *)player_equipment_records, (u32 *)(source + 4), *(u32 *)source >> 2);
    source += *(u32 *)source + 4;
    resource_copy_words((u32 *)player_level_growth_table, (u32 *)(source + 4), *(u32 *)source >> 2);
    source += *(u32 *)source + 4;
    resource_copy_words((u32 *)&effect_state, (u32 *)(source + 4), *(u32 *)source >> 2);
    source += *(u32 *)source + 4;
    resource_copy_words((u32 *)collision_default_rows, (u32 *)(source + 4), *(u32 *)source >> 2);
    source += *(u32 *)source + 4;
    resource_copy_words((u32 *)audio_state.voices.params, (u32 *)(source + 4), *(u32 *)source >> 2);
    source += *(u32 *)source + 4;
    resource_copy_words((u32 *)resource_tmd_archive_0_workspace, (u32 *)(source + 4), *(u32 *)source >> 2);
    source += *(u32 *)source + 4;
    asset_registry_load_tmd_archive(0, resource_tmd_archive_0_workspace);
    resource_copy_words((u32 *)resource_tmd_archive_28_workspace, (u32 *)(source + 4), *(u32 *)source >> 2);
    asset_registry_load_tmd_archive(0x28, resource_tmd_archive_28_workspace);
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

    resource_state.transition_active = KF_TRUE;
    resource_state.transition_phase = KF_RESOURCE_TRANSITION_BEGIN_MAP;
    first = resource_state.active_resource_ids[KF_RESOURCE_SLOT_MAP_REGION];
    second = resource_state.active_resource_ids[KF_RESOURCE_SLOT_TMD];
    third = resource_state.active_resource_ids[KF_RESOURCE_SLOT_TIM];
    fourth = resource_state.active_resource_ids[KF_RESOURCE_SLOT_VAB];
    fifth = resource_state.active_resource_ids[KF_RESOURCE_SLOT_SEQUENCE];
    resource_state.active_resource_ids[KF_RESOURCE_SLOT_MAP_REGION] = KF_RESOURCE_ACTIVE_UNINITIALIZED;
    resource_state.active_resource_ids[KF_RESOURCE_SLOT_TMD] = KF_RESOURCE_ACTIVE_UNINITIALIZED;
    resource_state.active_resource_ids[KF_RESOURCE_SLOT_TIM] = KF_RESOURCE_ACTIVE_UNINITIALIZED;
    resource_state.active_resource_ids[KF_RESOURCE_SLOT_VAB] = KF_RESOURCE_ACTIVE_UNINITIALIZED;
    resource_state.active_resource_ids[KF_RESOURCE_SLOT_SEQUENCE] = KF_RESOURCE_ACTIVE_UNINITIALIZED;
    resource_state.transition_offset.x = KF_RESOURCE_OFFSET_NO_SHIFT;
    resource_state.transition_offset.z = KF_RESOURCE_OFFSET_NO_SHIFT;
    resource_state.transition_offset.y = KF_RESOURCE_OFFSET_NO_SHIFT;
    resource_state.requested_resource_ids[KF_RESOURCE_SLOT_MAP_REGION] = first;
    resource_state.requested_resource_ids[KF_RESOURCE_SLOT_TMD] = second;
    resource_state.requested_resource_ids[KF_RESOURCE_SLOT_TIM] = third;
    resource_state.requested_resource_ids[KF_RESOURCE_SLOT_VAB] = fourth;
    resource_state.requested_resource_ids[KF_RESOURCE_SLOT_SEQUENCE] = fifth;
    do {
        cd_request_yield();
        resource_advance_transition();
    } while (resource_state.transition_active);
    tmd_set_slot(KF_TMD_SLOT_MAP, (KfTmdHeader *)resource_tmd_workspace);
    resource_state.active_table[5]();
}

ADDRESS(0x80016164, 0x178)
void translate_active_world_positions(s32 dx, s32 dy, s32 dz)
{
    KfEffectRecord *effect = effect_state.records;
    KfMapObject *object;
    KfActor *actor;
    u16 effect_remaining;
    u16 object_remaining;
    u16 actor_remaining;

    player_state.camera_position.vx += dx;
    player_state.camera_position.vz += dz;
    player_state.camera_position.vy += dy;

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

    object = map_object_state.objects;
    object_remaining = KF_MAP_OBJECT_CAPACITY - 1;
    do {
        if (object->object_id != KF_OBJECT_NONE) {
            object->position.vx += dx;
            object->position.vz += dz;
            object->position.vy += dy;
        }
        object++;
    } while (object_remaining-- != 0);

    actor = actor_state.actors;
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
        if (!audio_state.sequence_active) {
            audio_start_sequence();
        }
        return;
    }

    if (map_region_id == KF_RESOURCE_REQUEST_KEEP) {
        current_map_region_id = resource_state.active_resource_ids[KF_RESOURCE_SLOT_MAP_REGION];
        prior_map_region_id = resource_state.requested_resource_ids[KF_RESOURCE_SLOT_MAP_REGION];
        if (tmd_id == KF_RESOURCE_REQUEST_KEEP) {
            current_tmd_id = resource_state.active_resource_ids[KF_RESOURCE_SLOT_TMD];
            prior_tmd_id = resource_state.requested_resource_ids[KF_RESOURCE_SLOT_TMD];
        } else {
            prior_tmd_id = tmd_id;
            current_tmd_id = tmd_id;
        }
        if (tim_id == KF_RESOURCE_REQUEST_KEEP) {
            current_tim_id = resource_state.active_resource_ids[KF_RESOURCE_SLOT_TIM];
            prior_tim_id = resource_state.requested_resource_ids[KF_RESOURCE_SLOT_TIM];
        } else {
            prior_tim_id = tim_id;
            current_tim_id = tim_id;
        }
        if (vab_id == KF_RESOURCE_REQUEST_KEEP) {
            current_vab_id = resource_state.active_resource_ids[KF_RESOURCE_SLOT_VAB];
            prior_vab_id = resource_state.requested_resource_ids[KF_RESOURCE_SLOT_VAB];
        } else {
            prior_vab_id = vab_id;
            current_vab_id = vab_id;
        }
        if (sequence_id == KF_RESOURCE_REQUEST_KEEP) {
            current_sequence_id = resource_state.active_resource_ids[KF_RESOURCE_SLOT_SEQUENCE];
            prior_sequence_id = resource_state.requested_resource_ids[KF_RESOURCE_SLOT_SEQUENCE];
        } else {
            prior_sequence_id = sequence_id;
            current_sequence_id = sequence_id;
        }
    } else {
        prior_map_region_id = map_region_id;
        current_map_region_id = map_region_id;
        prior_tmd_id = prior_tim_id = prior_vab_id = prior_sequence_id = map_region_id;
        current_tmd_id = resource_state.active_resource_ids[KF_RESOURCE_SLOT_TMD];
        current_tim_id = resource_state.active_resource_ids[KF_RESOURCE_SLOT_TIM];
        current_vab_id = resource_state.active_resource_ids[KF_RESOURCE_SLOT_VAB];
        current_sequence_id = resource_state.active_resource_ids[KF_RESOURCE_SLOT_SEQUENCE];
    }

    if (resource_state.transition_active) {
        goto handle_active;
    }
    if (resource_state.active_resource_ids[KF_RESOURCE_SLOT_MAP_REGION] == current_map_region_id &&
        resource_state.active_resource_ids[KF_RESOURCE_SLOT_TMD] == current_tmd_id &&
        resource_state.active_resource_ids[KF_RESOURCE_SLOT_TIM] == current_tim_id &&
        resource_state.active_resource_ids[KF_RESOURCE_SLOT_VAB] == current_vab_id &&
        resource_state.active_resource_ids[KF_RESOURCE_SLOT_SEQUENCE] == current_sequence_id) {
        return;
    }

apply:
    if (resource_state.active_resource_ids[KF_RESOURCE_SLOT_MAP_REGION] != KF_RESOURCE_ACTIVE_UNINITIALIZED &&
        resource_state.active_resource_ids[KF_RESOURCE_SLOT_MAP_REGION] != current_map_region_id) {
        event_world_state_save_slot(resource_state.active_resource_ids[KF_RESOURCE_SLOT_MAP_REGION]);
    }
    if (event_state.control.fields.highest_requested_map_region_id < current_map_region_id) {
        event_state.control.fields.highest_requested_map_region_id = current_map_region_id;
    }
    resource_state.transition_active = KF_TRUE;
    resource_state.transition_phase = KF_RESOURCE_TRANSITION_BEGIN_MAP;
    resource_state.requested_resource_ids[KF_RESOURCE_SLOT_MAP_REGION] = map_region_id;
    resource_state.requested_resource_ids[KF_RESOURCE_SLOT_TMD] = tmd_id;
    resource_state.requested_resource_ids[KF_RESOURCE_SLOT_TIM] = tim_id;
    resource_state.requested_resource_ids[KF_RESOURCE_SLOT_VAB] = vab_id;
    resource_state.requested_resource_ids[KF_RESOURCE_SLOT_SEQUENCE] = sequence_id;
    resource_state.transition_offset.x = offset_x;
    resource_state.transition_offset.z = offset_z;
    resource_state.world_shift_applied = KF_FALSE;
    resource_state.transition_offset.y = offset_y;
    if (tmd_id == KF_RESOURCE_REQUEST_KEEP) {
        resource_state.tmd_object_limit_active = KF_FALSE;
    } else {
        resource_state.tmd_object_limit_active = KF_TRUE;
    }
    return;

handle_active:
    if ((resource_state.transition_active != KF_TRUE ||
         resource_state.requested_resource_ids[KF_RESOURCE_SLOT_MAP_REGION] == prior_map_region_id) &&
        resource_state.requested_resource_ids[KF_RESOURCE_SLOT_TMD] == prior_tmd_id &&
        resource_state.requested_resource_ids[KF_RESOURCE_SLOT_TIM] == prior_tim_id &&
        resource_state.requested_resource_ids[KF_RESOURCE_SLOT_VAB] == prior_vab_id &&
        resource_state.requested_resource_ids[KF_RESOURCE_SLOT_SEQUENCE] == prior_sequence_id) {
        return;
    }
    if ((resource_state.requested_resource_ids[KF_RESOURCE_SLOT_MAP_REGION] == KF_RESOURCE_REQUEST_KEEP &&
         resource_state.active_resource_ids[KF_RESOURCE_SLOT_MAP_REGION] == current_map_region_id) ||
        (resource_state.requested_resource_ids[KF_RESOURCE_SLOT_TMD] == KF_RESOURCE_REQUEST_KEEP &&
         resource_state.active_resource_ids[KF_RESOURCE_SLOT_TMD] == tmd_id) ||
        (resource_state.requested_resource_ids[KF_RESOURCE_SLOT_TIM] == KF_RESOURCE_REQUEST_KEEP &&
         resource_state.active_resource_ids[KF_RESOURCE_SLOT_TIM] == tim_id) ||
        (resource_state.requested_resource_ids[KF_RESOURCE_SLOT_VAB] == KF_RESOURCE_REQUEST_KEEP &&
         resource_state.active_resource_ids[KF_RESOURCE_SLOT_VAB] == vab_id) ||
        (resource_state.requested_resource_ids[KF_RESOURCE_SLOT_SEQUENCE] == KF_RESOURCE_REQUEST_KEEP &&
         resource_state.active_resource_ids[KF_RESOURCE_SLOT_SEQUENCE] == sequence_id)) {
        return;
    }

    if ((resource_state.requested_resource_ids[KF_RESOURCE_SLOT_MAP_REGION] != KF_RESOURCE_REQUEST_KEEP &&
         map_region_id == KF_RESOURCE_REQUEST_KEEP) ||
        (resource_state.requested_resource_ids[KF_RESOURCE_SLOT_TMD] != KF_RESOURCE_REQUEST_KEEP &&
         tmd_id == KF_RESOURCE_REQUEST_KEEP) ||
        (resource_state.requested_resource_ids[KF_RESOURCE_SLOT_TIM] != KF_RESOURCE_REQUEST_KEEP &&
         tim_id == KF_RESOURCE_REQUEST_KEEP) ||
        (resource_state.requested_resource_ids[KF_RESOURCE_SLOT_VAB] != KF_RESOURCE_REQUEST_KEEP &&
         vab_id == KF_RESOURCE_REQUEST_KEEP) ||
        (resource_state.requested_resource_ids[KF_RESOURCE_SLOT_SEQUENCE] != KF_RESOURCE_REQUEST_KEEP &&
         sequence_id == KF_RESOURCE_REQUEST_KEEP)) {
        while (resource_state.transition_active) {
            cd_request_yield();
            resource_advance_transition();
        }
    } else {
        for (;;) {
            EnterCriticalSection();
            if (resource_state.transition_phase != KF_RESOURCE_TRANSITION_PENDING_IO) {
                break;
            }
            ExitCriticalSection();
            cd_request_yield();
        }
    }
    ExitCriticalSection();
    if (resource_state.world_shift_applied != 0 && map_region_id != KF_RESOURCE_REQUEST_KEEP &&
        offset_x == KF_RESOURCE_OFFSET_NO_SHIFT) {
        offset_x = -resource_state.transition_offset.x;
        offset_y = -resource_state.transition_offset.y;
        offset_z = -resource_state.transition_offset.z;
    }
    goto apply;
}

ADDRESS(0x80016838, 0x14)
void resource_transition_set_phase_1(KfCdRequest *request)
{
    resource_state.transition_phase = KF_RESOURCE_TRANSITION_LOAD_MAP_CELLS;
}

ADDRESS(0x8001684c, 0x14)
void resource_transition_set_phase_3(KfCdRequest *request)
{
    resource_state.transition_phase = KF_RESOURCE_TRANSITION_LOAD_MAP_ACTORS;
}

ADDRESS(0x80016860, 0x14)
void resource_transition_set_phase_2(KfCdRequest *request)
{
    resource_state.transition_phase = KF_RESOURCE_TRANSITION_QUEUE_MAP_ACTORS;
}

ADDRESS(0x80016874, 0x14)
void resource_transition_set_phase_4(KfCdRequest *request)
{
    resource_state.transition_phase = KF_RESOURCE_TRANSITION_QUEUE_TIM;
}

ADDRESS(0x80016888, 0x14)
void resource_transition_set_phase_6(KfCdRequest *request)
{
    resource_state.transition_phase = KF_RESOURCE_TRANSITION_FINISH_AUDIO;
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

    if (!resource_state.transition_active) {
        return;
    }
    if (resource_state.transition_active != KF_TRUE) {
        return;
    }
    phase = resource_state.transition_phase;

    switch (phase) {
    case KF_RESOURCE_TRANSITION_BEGIN_MAP:
        if (resource_state.requested_resource_ids[KF_RESOURCE_SLOT_MAP_REGION] == KF_RESOURCE_REQUEST_KEEP) {
            goto phase_three;
        }
        resource_state.transition_phase = KF_RESOURCE_TRANSITION_PENDING_IO;
        object = map_object_state.objects;
        index = KF_MAP_OBJECT_CAPACITY - 1;
        do {
            object->action = KF_MAP_OBJECT_OP_NONE;
            index--;
            object++;
        } while (index != -1);
        cd_archive_queue_read(KF_RESOURCE_ARCHIVE_FDAT, resource_state.requested_resource_ids[KF_RESOURCE_SLOT_MAP_REGION] * 3,
            (u_long *)cd_stream_work_buffer,
            resource_transition_set_phase_1);
        return;

    case KF_RESOURCE_TRANSITION_LOAD_MAP_CELLS:
        buffer = cd_stream_work_buffer;
        resource_state.transition_phase = KF_RESOURCE_TRANSITION_PENDING_IO;
        /* Retail's default table contains 32 pointers to the no-op callback. */
        resource_state.active_table = callback_default_table;
        cd_archive_queue_read(KF_RESOURCE_ARCHIVE_FDAT, resource_state.requested_resource_ids[KF_RESOURCE_SLOT_MAP_REGION] * 3 + 2,
            (u_long *)resource_callback_table_workspace,
            resource_transition_set_phase_2);
        resource_copy_words((u32 *)&bss_801c7540, (u32 *)(buffer + 4),
                            sizeof(bss_801c7540.map_cells) / sizeof(u32));
        buffer += *(u32 *)buffer + 4;
        resource_copy_words((u32 *)KF_COLLISION_SHAPE_BANK,
            (u32 *)(buffer + 4), KF_COLLISION_SHAPE_BANK_BYTES / sizeof(u32));
        if (resource_state.transition_offset.x != KF_RESOURCE_OFFSET_NO_SHIFT) {
            resource_state.world_shift_applied = KF_TRUE;
            translate_active_world_positions(resource_state.transition_offset.x << KF_MAP_CELL_POSITION_SHIFT,
                -resource_state.transition_offset.y * 128,
                resource_state.transition_offset.z << KF_MAP_CELL_POSITION_SHIFT);
        }
        resource_state.current_map_region_id = resource_state.requested_resource_ids[KF_RESOURCE_SLOT_MAP_REGION];
        return;

    case KF_RESOURCE_TRANSITION_QUEUE_MAP_ACTORS:
        resource_state.transition_phase = KF_RESOURCE_TRANSITION_PENDING_IO;
        cd_archive_queue_read(KF_RESOURCE_ARCHIVE_FDAT, resource_state.requested_resource_ids[KF_RESOURCE_SLOT_MAP_REGION] * 3 + 1,
            (u_long *)cd_stream_work_buffer,
            resource_transition_set_phase_3);
        resource_state.active_table = (KfCallback *)resource_callback_table_workspace;
        return;

    case KF_RESOURCE_TRANSITION_LOAD_MAP_ACTORS:
phase_three:
        if (resource_state.requested_resource_ids[KF_RESOURCE_SLOT_TMD] != KF_RESOURCE_REQUEST_KEEP) {
            resource_state.transition_phase = KF_RESOURCE_TRANSITION_PENDING_IO;
            cd_archive_queue_read(KF_RESOURCE_ARCHIVE_RTMD, resource_state.requested_resource_ids[KF_RESOURCE_SLOT_TMD],
                (u_long *)resource_tmd_workspace,
                resource_transition_set_phase_4);
        }
        if (resource_state.requested_resource_ids[KF_RESOURCE_SLOT_MAP_REGION] != KF_RESOURCE_REQUEST_KEEP) {
            stream = cd_stream_work_buffer;
            actor = actor_state.actors;
            index = KF_ACTOR_CAPACITY - 1;
            do {
                if (actor->animation_cache != NULL) {
                    pool_record_release(actor->animation_cache);
                }
                actor->slot_state = KF_ACTOR_SLOT_FREE;
                index--;
                actor++;
            } while (index != -1);
            object = map_object_state.objects;
            index = KF_MAP_OBJECT_CAPACITY - 1;
            do {
                if (object->tail.animated.animation_cache != NULL) {
                    pool_record_release(object->tail.animated.animation_cache);
                }
                if (object->action == KF_MAP_OBJECT_OP_PLAYER_REACTION) {
                    memory_free((u8 *)object->extra_40.record);
                    object->action = KF_MAP_OBJECT_OP_NONE;
                }
                object->object_id = KF_OBJECT_NONE;
                index--;
                object++;
            } while (index != -1);
            if (resource_state.transition_offset.x != KF_RESOURCE_OFFSET_NO_SHIFT) {
                KfMapOccupancyCell *cell = &bss_801c7540.map_cells[0][0];
                index = KF_MAP_WORLD_GRID_SIDE * KF_MAP_WORLD_GRID_SIDE;
                do {
                    cell->layer[0].quarter_turns &= KF_MAP_CELL_QUARTER_TURN_MASK;
                    cell->layer[1].quarter_turns &= KF_MAP_CELL_QUARTER_TURN_MASK;
                    index--;
                    cell++;
                } while (index != 0);
                map_cell_add_layer_occupancy(player_state.camera_position.vx,
                    player_state.camera_position.vz, KF_PLAYER_COLLISION_RADIUS, 1);
            }
            resource_copy_words((u32 *)actor_state.target_groups,
                (u32 *)(stream + 4),
                (sizeof(actor_state.target_groups) + sizeof(actor_state.target_candidate_blob)) /
                    sizeof(u32));
            actor_fixup_group_targets();
            stream += *(u32 *)stream + 4;
            actor_load_records((const struct KfActorLoadRecord *)(stream + 4));
            stream += *(u32 *)stream + 4;
            map_object_initialize_from_placements((KfMapObjectPlacement *)(stream + 4));
            stream += *(u32 *)stream + 4;
            map_placed_expand_sources((KfMapPlacedSource *)(stream + 4));
            event_world_state_restore_slot(resource_state.requested_resource_ids[KF_RESOURCE_SLOT_MAP_REGION]);
            resource_state.active_table[5]();
            player_state.force_actor_lifecycle_refresh = KF_TRUE;
        }
        if (resource_state.requested_resource_ids[KF_RESOURCE_SLOT_TMD] != KF_RESOURCE_REQUEST_KEEP) {
            return;
        }
        /* fall through */

    case KF_RESOURCE_TRANSITION_QUEUE_TIM:
        if (resource_state.requested_resource_ids[KF_RESOURCE_SLOT_TIM] != KF_RESOURCE_REQUEST_KEEP) {
            cd_map_stream_read(KF_RESOURCE_ARCHIVE_RTIM, resource_state.requested_resource_ids[KF_RESOURCE_SLOT_TIM]);
        }
        resource_state.tmd_object_limit_active = KF_FALSE;
        if (resource_state.requested_resource_ids[KF_RESOURCE_SLOT_VAB] != KF_RESOURCE_REQUEST_KEEP ||
            resource_state.requested_resource_ids[KF_RESOURCE_SLOT_SEQUENCE] != KF_RESOURCE_REQUEST_KEEP) {
            goto begin_phase_five;
        }

complete:
        if (resource_state.requested_resource_ids[KF_RESOURCE_SLOT_MAP_REGION] != KF_RESOURCE_REQUEST_KEEP) {
            resource_state.active_resource_ids[KF_RESOURCE_SLOT_MAP_REGION] =
                resource_state.requested_resource_ids[KF_RESOURCE_SLOT_MAP_REGION];
        }
        if (resource_state.requested_resource_ids[KF_RESOURCE_SLOT_TMD] != KF_RESOURCE_REQUEST_KEEP) {
            resource_state.active_resource_ids[KF_RESOURCE_SLOT_TMD] =
                resource_state.requested_resource_ids[KF_RESOURCE_SLOT_TMD];
        }
        if (resource_state.requested_resource_ids[KF_RESOURCE_SLOT_TIM] != KF_RESOURCE_REQUEST_KEEP) {
            resource_state.active_resource_ids[KF_RESOURCE_SLOT_TIM] =
                resource_state.requested_resource_ids[KF_RESOURCE_SLOT_TIM];
        }
        if (resource_state.requested_resource_ids[KF_RESOURCE_SLOT_VAB] != KF_RESOURCE_REQUEST_KEEP) {
            resource_state.active_resource_ids[KF_RESOURCE_SLOT_VAB] =
                resource_state.requested_resource_ids[KF_RESOURCE_SLOT_VAB];
        }
        if (resource_state.requested_resource_ids[KF_RESOURCE_SLOT_SEQUENCE] != KF_RESOURCE_REQUEST_KEEP) {
            resource_state.active_resource_ids[KF_RESOURCE_SLOT_SEQUENCE] =
                resource_state.requested_resource_ids[KF_RESOURCE_SLOT_SEQUENCE];
        }
        resource_state.transition_active = KF_FALSE;
        return;

begin_phase_five:
        resource_state.transition_phase = KF_RESOURCE_TRANSITION_FADE_AUDIO;
        resource_state.sequence_fade_volume = RESOURCE_SEQUENCE_FADE_START_VOLUME;
        audio_state.sequence_ready = KF_FALSE;
        /* fall through */
    case KF_RESOURCE_TRANSITION_FADE_AUDIO:
        if (audio_state.sequence_active) {
            resource_state.sequence_fade_volume -= RESOURCE_SEQUENCE_FADE_STEP;
            if (resource_state.sequence_fade_volume <= 0) {
                resource_state.sequence_fade_volume = 0;
            }
            SsSeqSetVol(audio_state.sequence_id,
                resource_state.sequence_fade_volume,
                resource_state.sequence_fade_volume);
            if (resource_state.sequence_fade_volume != 0) {
                return;
            }
            SsSeqStop(audio_state.sequence_id);
            SsSeqClose(audio_state.sequence_id);
            audio_state.sequence_active = KF_FALSE;
        }
        if (audio_state.vab_slots[1].vab_id != KF_AUDIO_VAB_ID_NONE) {
            audio_state.vab_slots[1].stream_slot = NULL;
        }
        if (resource_state.requested_resource_ids[KF_RESOURCE_SLOT_VAB] != KF_RESOURCE_REQUEST_KEEP) {
            audio_queue_vab_stream(KF_RESOURCE_ARCHIVE_VAB, resource_state.requested_resource_ids[KF_RESOURCE_SLOT_VAB] + 1, 1);
        }
        if (resource_state.requested_resource_ids[KF_RESOURCE_SLOT_SEQUENCE] != KF_RESOURCE_REQUEST_KEEP) {
            cd_archive_queue_read(KF_RESOURCE_ARCHIVE_VAB,
                resource_state.requested_resource_ids[KF_RESOURCE_SLOT_SEQUENCE] % 100 + 320,
                audio_state.sequence_buffer,
                resource_transition_set_phase_6);
            resource_state.transition_phase = KF_RESOURCE_TRANSITION_PENDING_IO;
            return;
        }
        goto complete;

    case KF_RESOURCE_TRANSITION_FINISH_AUDIO:
        if (resource_state.requested_resource_ids[KF_RESOURCE_SLOT_SEQUENCE] < 100) {
            audio_state.sequence_ready = KF_TRUE;
            audio_start_sequence();
        }
        goto complete;
    default:
        break;
    }
}
