#include <kf/lib/address.h>
#include <kf/lib/null.h>
#include <kf/game/audio.h>
#include <kf/game/cd.h>
#include <kf/game/collision_cache.h>
#include <kf/game/player.h>
#include <kf/lib/audio.h>
#include <kf/lib/math.h>
#include <psyq/audio.h>
#include <psyq/kernel.h>
#include <LIBSPU.H>

enum {
    AUDIO_VAB_STREAM_BUFFER_BYTES = 0x1000,
    AUDIO_VAB_STREAM_BUFFER_COUNT = KF_AUDIO_VAB_STREAM_POOL_COUNT + 1,
    AUDIO_MAIN_VAB_HEADER_BUFFER_BYTES = 0x2800,
    AUDIO_SEQUENCE_BUFFER_BYTES = 0x3000,
    AUDIO_PRIORITY_ABOVE_MAX = 1 << 16,
    AUDIO_NOTE_MAX = 127,
    AUDIO_VOLUME_MAX = 127,
    AUDIO_VAB_TRANSFER_MORE_DATA = -2,
    AUDIO_REVERB_DEPTH = 40,
    AUDIO_SEQUENCE_VOLUME = 60,
    AUDIO_MAIN_VAB_SLOT = 0,
    AUDIO_SEQUENCE_VAB_SLOT = 1,
    AUDIO_SPATIAL_MIN_LEVEL = 20,
    AUDIO_SPATIAL_PAN_ATTENUATION_THRESHOLD = 64,
    AUDIO_SPATIAL_PAN_DIVISOR = 3400
};

/* SDK-required 2-by-1 sequence workspace; original allocation extent is WIP. */
DATA(0x8009b5e4, 0x158, ".bss")
char audio_sequence_table[SS_SEQ_TABSIZ * KF_AUDIO_SEQUENCE_CAPACITY * KF_AUDIO_TRACKS_PER_SEQUENCE];

/* Five pooled VAB headers and the dedicated stream-slot-6 header use 0x1000 bytes each. */
DATA(0x801659ac, 0x6000, ".bss")
u8 audio_vab_stream_buffers[AUDIO_VAB_STREAM_BUFFER_COUNT][AUDIO_VAB_STREAM_BUFFER_BYTES];
typedef char kf_audio_vab_stream_buffers_size[
    sizeof(audio_vab_stream_buffers) == 0x6000 ? 1 : -1];

DATA(0x80195d74, 0x2800, ".bss")
u8 audio_main_vab_header_buffer[AUDIO_MAIN_VAB_HEADER_BUFFER_BYTES];

DATA(0x80198574, 0xe9c, ".bss")
KfGameAudioState audio_state;

DATA(0x80199584, 0x3000, ".bss")
u8 audio_sequence_buffer[AUDIO_SEQUENCE_BUFFER_BYTES];

ADDRESS(0x80013a40, 0x120)
void audio_initialize_runtime(void)
{
    KfAudioVabSlot *vab_slot;
    KfAudioVoiceHandle *voice;
    KfAudioVabStreamSlot *stream_slot;
    u8 *stream_buffer;
    s32 index;

    SsInit();
    SsSetMVol(0, 0);
    SsSetTableSize(audio_sequence_table, KF_AUDIO_SEQUENCE_CAPACITY,
        KF_AUDIO_TRACKS_PER_SEQUENCE);
    SsSetTickMode(SS_TICK60);
    SsStart2();
    SsUtSetReverbType(SS_REV_TYPE_STUDIO_C);
    SsUtReverbOn();
    SsUtSetReverbDepth(AUDIO_REVERB_DEPTH, AUDIO_REVERB_DEPTH);

    audio_state.sequence_buffer = (u_long *)audio_sequence_buffer;
    audio_state.sequence_active = KF_FALSE;
    audio_state.sequence_ready = KF_FALSE;
    vab_slot = audio_state.vab_slots;
    index = KF_AUDIO_VAB_SLOT_COUNT - 1;
    do {
        vab_slot->vab_id = KF_AUDIO_VAB_ID_NONE;
        vab_slot->stream_slot = NULL;
        vab_slot++;
        index--;
    } while (index != -1);

    voice = audio_state.voices.handles;
    index = KF_AUDIO_VOICE_HANDLE_COUNT - 1;
    do {
        voice->voice_id = KF_AUDIO_VOICE_ID_NONE;
        voice++;
        index--;
    } while (index != -1);

    stream_slot = audio_state.vab_stream_slots;
    index = 0;
    stream_buffer = audio_vab_stream_buffers[1];
    for (; index < KF_AUDIO_VAB_STREAM_SLOT_COUNT; index++) {
        stream_slot->state = KF_AUDIO_VAB_STREAM_FREE;
        stream_slot->buffer = stream_buffer;
        stream_slot++;
        stream_buffer += AUDIO_VAB_STREAM_BUFFER_BYTES;
    }
    audio_state.vab_stream_slots[KF_AUDIO_VAB_STREAM_SLOT_FOR_VAB_1].buffer =
        audio_main_vab_header_buffer;
    audio_state.vab_stream_slots[KF_AUDIO_VAB_STREAM_SLOT_FOR_VAB_0].buffer =
        audio_vab_stream_buffers[0];
}

ADDRESS(0x80013b60, 0x98)
void audio_start_sequence(void)
{
    if (player_state.audio_music_enabled != KF_PLAYER_OPTION_OFF && audio_state.sequence_ready != 0) {
        audio_state.sequence_id = SsSeqOpen(
            audio_state.sequence_buffer, audio_state.vab_slots[AUDIO_SEQUENCE_VAB_SLOT].vab_id);
        SsSeqSetVol(audio_state.sequence_id, AUDIO_SEQUENCE_VOLUME, AUDIO_SEQUENCE_VOLUME);
        SsSeqPlay(audio_state.sequence_id, SSPLAY_PLAY, SSPLAY_INFINITY);
        audio_state.sequence_active = KF_TRUE;
        SsSetMVol(AUDIO_VOLUME_MAX, AUDIO_VOLUME_MAX);
    }
}

ADDRESS(0x80013bf8, 0x58)
void audio_stop_sequence(void)
{
    if (audio_state.sequence_active == KF_TRUE) {
        SsSeqStop(audio_state.sequence_id);
        SsSeqClose(audio_state.sequence_id);
        audio_state.sequence_active = KF_FALSE;
    }
}

ADDRESS(0x80013c50, 0xb8)
void audio_shutdown(void)
{
    KfAudioVabSlot *slot;
    s32 index;

    SsSetMVol(0, 0);
    if (audio_state.sequence_active == KF_TRUE) {
        SsSeqSetVol(audio_state.sequence_id, 0, 0);
        SsSeqStop(audio_state.sequence_id);
        SsSeqClose(audio_state.sequence_id);
    }
    slot = audio_state.vab_slots;
    index = KF_AUDIO_VAB_SLOT_COUNT - 1;
    do {
        if (slot->vab_id != KF_AUDIO_VAB_ID_NONE) {
            SsVabClose(slot->vab_id);
        }
        slot++;
        index--;
    } while (index != -1);
    SsEnd();
}

ADDRESS(0x80013d08, 0x2c4)
KfAudioPlaybackResult audio_play_spatial(
    s32 sound, const VECTOR *position, s32 volume, s32 max_distance,
    s32 attenuation_distance, s32 note_offset)
{
    s32 delta_x = (position->vx - audio_state.listener_position.vx) >> 3;
    s32 delta_y = (position->vy - audio_state.listener_position.vy) >> 3;
    s32 delta_z = (position->vz - audio_state.listener_position.vz) >> 3;
    s32 attenuation = SquareRoot0(delta_x * delta_x + delta_y * delta_y +
                                  delta_z * delta_z) << 3;
    s32 level;
    s32 angle;
    s32 listener_angle;
    s32 left;
    s32 right;
    s32 alternate_pan = sound & KF_AUDIO_ALTERNATE_PAN_FLAG;
    sound &= KF_AUDIO_SOUND_INDEX_MASK;

    if (attenuation >= max_distance) {
        return KF_AUDIO_NOT_PLAYED;
    }

    attenuation = ((attenuation_distance - attenuation) << 7) /
                  attenuation_distance;
    level = (attenuation * volume) >> 7;
    collision_sample_map_cell_layer(position->vx, position->vy, position->vz);
    if (KF_COLLISION_CACHE_LAYER != audio_state.listener_layer) {
        level = (attenuation * volume) >> 8;
    }
    if (level < AUDIO_SPATIAL_MIN_LEVEL) {
        return KF_AUDIO_NOT_PLAYED;
    }
    if (level > AUDIO_VOLUME_MAX) {
        level = AUDIO_VOLUME_MAX;
    }

    angle = vector_xz_to_angle(
        position->vx - audio_state.listener_position.vx,
        position->vz - audio_state.listener_position.vz);
    listener_angle = audio_state.listener_rotation.vy - KF_ANGLE_QUARTER_TURN;
    angle = (angle - listener_angle) & KF_ANGLE_WRAP_MASK;
    if (angle >= KF_ANGLE_HALF_TURN) {
        angle = KF_ANGLE_FULL_TURN - angle;
    }
    angle >>= 1;
    if (attenuation >= AUDIO_SPATIAL_PAN_ATTENUATION_THRESHOLD && !alternate_pan) {
        angle = (((angle - 512) * (256 - 2 * attenuation)) >> 7) + 512;
    }

    left = (level * rsin(angle)) / AUDIO_SPATIAL_PAN_DIVISOR;
    if (left > AUDIO_VOLUME_MAX) {
        left = AUDIO_VOLUME_MAX;
    }
    right = (level * rcos(angle)) / AUDIO_SPATIAL_PAN_DIVISOR;
    if (right > AUDIO_VOLUME_MAX) {
        right = AUDIO_VOLUME_MAX;
    }
    audio_key_on(sound, left, right, note_offset);
    return KF_AUDIO_PLAYED;
}

ADDRESS(0x80013fcc, 0x34)
KfAudioPlaybackResult audio_play_spatial_default_range(
    s32 sound, const VECTOR *position, s16 volume, s32 note_offset)
{
    return audio_play_spatial(sound, position, volume, KF_AUDIO_DEFAULT_MAX_DISTANCE,
        KF_AUDIO_DEFAULT_ATTENUATION_DISTANCE, note_offset);
}

ADDRESS(0x80014000, 0x34)
KfAudioPlaybackResult audio_play_spatial_range(
    s32 sound, const VECTOR *position, s16 volume, s32 max_distance,
    s32 attenuation_distance, s32 note_offset)
{
    return audio_play_spatial(
        sound, position, volume, max_distance, attenuation_distance, note_offset);
}

ADDRESS(0x80014034, 0x78)
void audio_key_off_handle(KfAudioVoiceHandle *handle)
{
    KfAudioVoiceParams *voice = &audio_state.voices.params[(u8)handle->sound_id];
    KfAudioVabSlot *vab = &audio_state.vab_slots[voice->vab_slot_index];

    if (vab->vab_id != KF_AUDIO_VAB_ID_NONE && vab->vab_id != KF_AUDIO_VAB_ID_STREAM_PENDING) {
        SsUtKeyOff(handle->voice_id, vab->vab_id, voice->program, voice->tone, voice->note);
    }
}

ADDRESS(0x800140ac, 0xac)
void audio_update_listener(const VECTOR *position, const SVECTOR *rotation)
{
    if (position != NULL) {
        audio_state.listener_position = *position;
        collision_sample_map_cell_layer(position->vx, position->vy, position->vz);
        audio_state.listener_layer = KF_COLLISION_CACHE_LAYER;
    }
    if (rotation != NULL) {
        audio_state.listener_rotation = *rotation;
    }
}

ADDRESS(0x80014158, 0x24)
void audio_play_sound(s32 sound, s32 volume)
{
    audio_key_on(sound, volume, volume, 0);
}

ADDRESS(0x8001417c, 0x64)
void audio_refresh_voice_handles(void)
{
    u8 status[KF_AUDIO_SPU_VOICE_COUNT];
    KfAudioVoiceHandle *handle;
    s32 index;

    SpuGetAllKeysStatus((char *)status);
    handle = audio_state.voices.handles;
    index = KF_AUDIO_VOICE_HANDLE_COUNT - 1;
    do {
        if (handle->voice_id != KF_AUDIO_VOICE_ID_NONE && status[handle->voice_id] == 0) {
            handle->voice_id = KF_AUDIO_VOICE_ID_NONE;
        }
        handle++;
        index--;
    } while (index != -1);
}

ADDRESS(0x800141e0, 0x114)
KfAudioVoiceHandle *audio_allocate_voice_handle(s32 sound_id)
{
    KfAudioVoiceHandle *handle;
    KfAudioVoiceHandle *lowest_priority_handle;
    KfAudioVoiceState *voices;
    KfAudioVoiceParams *params;
    s32 lowest_priority;
    s32 index;

    audio_refresh_voice_handles();
    handle = audio_state.voices.handles;
    index = KF_AUDIO_VOICE_HANDLE_COUNT - 1;
    do {
        if (handle->voice_id == KF_AUDIO_VOICE_ID_NONE) {
            return handle;
        }
        handle++;
        index--;
    } while (index != -1);

    handle = audio_state.voices.handles;
    index = KF_AUDIO_VOICE_HANDLE_COUNT - 1;
    do {
        if (handle->sound_id == sound_id) {
            audio_key_off_handle(handle);
            handle->voice_id = KF_AUDIO_VOICE_ID_NONE;
            return handle;
        }
        handle++;
        index--;
    } while (index != -1);

    lowest_priority = AUDIO_PRIORITY_ABOVE_MAX;
    voices = &audio_state.voices;
    handle = voices->handles;
    index = KF_AUDIO_VOICE_HANDLE_COUNT - 1;
    params = voices->params;
    do {
        u16 priority = params[(u8)handle->sound_id].priority;

        if (priority < lowest_priority) {
            lowest_priority = priority;
            lowest_priority_handle = handle;
        }
        handle++;
        index--;
    } while (index != -1);
    audio_key_off_handle(lowest_priority_handle);
    lowest_priority_handle->voice_id = KF_AUDIO_VOICE_ID_NONE;
    return lowest_priority_handle;
}

ADDRESS(0x800142f4, 0x11c)
void audio_key_on(s32 sound, s32 left_volume, s32 right_volume, s32 note_offset)
{
    KfAudioVoiceParams *voice;
    KfAudioVabSlot *vab;
    KfAudioVoiceHandle *handle;

    if (sound == KF_AUDIO_SOUND_NONE || player_state.audio_effects_enabled == KF_PLAYER_OPTION_OFF) {
        return;
    }
    voice = &audio_state.voices.params[(u8)sound];
    if (voice->vab_slot_index == KF_AUDIO_VAB_SLOT_NONE) {
        return;
    }
    vab = &audio_state.vab_slots[voice->vab_slot_index];
    if (vab->vab_id == KF_AUDIO_VAB_ID_NONE || vab->vab_id == KF_AUDIO_VAB_ID_STREAM_PENDING) {
        return;
    }
    handle = audio_allocate_voice_handle(sound);
    note_offset += voice->note;
    if (note_offset < 0) {
        note_offset = 0;
    } else if (note_offset > AUDIO_NOTE_MAX) {
        note_offset = AUDIO_NOTE_MAX;
    }
    handle->voice_id = SsUtKeyOn(vab->vab_id, voice->program, voice->tone,
        note_offset, 0, left_volume, right_volume);
    handle->sound_id = sound;
}

ADDRESS(0x80014410, 0x124)
void audio_vab_stream_callback(KfCdRequest *request)
{
    KfAudioVabSlot *vab_slot;

    switch (request->payload.vab.phase) {
    case KF_CD_VAB_PHASE_HEAD:
        vab_slot = &audio_state.vab_slots[request->payload.vab.slot_index];
        if (cd_sectors_corrupt(
                (u32 *)request->payload.vab.stream_slot->buffer,
                request->sector_count)) {
            request->phase = KF_CD_REQUEST_PHASE_SEEK;
            CdSeekP(&request->initial_location);
            return;
        }
        if (vab_slot->vab_id != KF_AUDIO_VAB_ID_NONE) {
            SsVabClose(vab_slot->vab_id);
        }
        vab_slot->vab_id = SsVabOpenHead(
            request->payload.vab.stream_slot->buffer, -1);
        if (vab_slot->vab_id == KF_AUDIO_VAB_ID_NONE) {
            cd_request_advance(request);
            return;
        }
        cd_location_add(&request->location, request->sector_count, &request->location);
        request->payload.vab.phase = KF_CD_VAB_PHASE_BODY_READ;
        request->phase = KF_CD_REQUEST_PHASE_SEEK;
        request->destination = (u_long *)cd_stream_work_buffer;
        request->sector_count = KF_CD_VAB_BODY_CHUNK_SECTORS;
        CdSeekP(&request->location);
        break;
    case KF_CD_VAB_PHASE_BODY_READ:
        request->payload.vab.phase = KF_CD_VAB_PHASE_BODY_READY;
        break;
    default:
        break;
    }
}

ADDRESS(0x80014534, 0x13c)
void cd_request_service_vab(void)
{
    KfCdRequest *request;
    KfAudioVabSlot *vab_slot;
    CdlLOC *location;
    s16 result;

    EnterCriticalSection();
    request = cd_state.current;
    if (request->kind == KF_CD_REQUEST_VAB_READ &&
        request->payload.vab.phase == KF_CD_VAB_PHASE_BODY_READY) {
        ExitCriticalSection();
        location = &request->location;
        vab_slot = &audio_state.vab_slots[request->payload.vab.slot_index];
        for (;;) {
            result = SsVabTransBodyPartly((u8 *)request->destination,
                KF_CD_VAB_BODY_CHUNK_BYTES, vab_slot->vab_id);
            if (result == -1) {
                SsVabClose(vab_slot->vab_id);
                cd_request_advance(request);
                continue;
            }
            if (result == AUDIO_VAB_TRANSFER_MORE_DATA) {
                request->phase = KF_CD_REQUEST_PHASE_SEEK;
                request->payload.vab.phase = KF_CD_VAB_PHASE_BODY_READ;
                cd_location_add(location, request->sector_count, location);
                CdSeekP(location);
                SsVabTransCompleted(SS_WAIT_COMPLETED);
                return;
            } else if (result == vab_slot->vab_id) {
                cd_request_advance(request);
                SsVabTransCompleted(SS_WAIT_COMPLETED);
                request->payload.vab.stream_slot->state =
                    KF_AUDIO_VAB_STREAM_IN_USE;
                request->sector_count = 0;
            }
            return;
        }
    } else {
        ExitCriticalSection();
    }
}

ADDRESS(0x80014670, 0xdc)
KfAudioVabStreamSlot *audio_acquire_vab_stream_slot(void)
{
    KfAudioVabStreamSlot *stream_slot;
    KfAudioVabSlot *vab_slot;
    s32 index;

    stream_slot = audio_state.vab_stream_slots;
    index = KF_AUDIO_VAB_STREAM_POOL_COUNT - 1;
    do {
        if (stream_slot->state == KF_AUDIO_VAB_STREAM_FREE) {
            return stream_slot;
        }
        index--;
        stream_slot++;
    } while (index != -1);

    stream_slot = audio_state.vab_stream_slots;
    index = KF_AUDIO_VAB_STREAM_POOL_COUNT - 1;
    do {
        if (stream_slot->state == KF_AUDIO_VAB_STREAM_RECLAIMABLE) {
            vab_slot = audio_state.vab_slots;
            index = KF_AUDIO_VAB_SLOT_COUNT - 1;
            do {
                if (vab_slot->vab_id != KF_AUDIO_VAB_ID_NONE && vab_slot->stream_slot == stream_slot) {
                    SsVabClose(vab_slot->vab_id);
                    vab_slot->vab_id = KF_AUDIO_VAB_ID_NONE;
                    vab_slot->stream_slot = NULL;
                    break;
                }
                index--;
                vab_slot++;
            } while (index != -1);
            stream_slot->state = KF_AUDIO_VAB_STREAM_FREE;
            return stream_slot;
        }
        index--;
        stream_slot++;
    } while (index != -1);
    return NULL;
}

ADDRESS(0x8001474c, 0xd0)
void audio_queue_vab_stream(s32 archive_slot, s32 entry, s32 vab_slot_index)
{
    KfAudioVabSlot *vab_slot;
    KfAudioVabStreamSlot *stream_slot;
    KfCdRequest *request;

    vab_slot = &audio_state.vab_slots[vab_slot_index];
    switch (vab_slot_index) {
    case AUDIO_MAIN_VAB_SLOT:
        stream_slot = &audio_state.vab_stream_slots[KF_AUDIO_VAB_STREAM_SLOT_FOR_VAB_0];
        break;
    case AUDIO_SEQUENCE_VAB_SLOT:
        stream_slot = &audio_state.vab_stream_slots[KF_AUDIO_VAB_STREAM_SLOT_FOR_VAB_1];
        break;
    default:
        stream_slot = audio_acquire_vab_stream_slot();
        if (stream_slot == NULL) {
            return;
        }
        vab_slot->vab_id = KF_AUDIO_VAB_ID_STREAM_PENDING;
        break;
    }
    vab_slot->stream_slot = stream_slot;
    stream_slot->state = KF_AUDIO_VAB_STREAM_LOADING;
    request = cd_state.tail;
    cd_request_wait_done(request);
    request->payload.vab.phase = KF_CD_VAB_PHASE_HEAD;
    request->payload.vab.stream_slot = stream_slot;
    request->payload.vab.slot_index = vab_slot_index;
    cd_archive_queue_stream_read(
        archive_slot, entry * 2, (u_long *)stream_slot->buffer,
        audio_vab_stream_callback);
}
