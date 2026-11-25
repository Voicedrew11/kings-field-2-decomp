#ifndef KF_GAME_AUDIO_H
#define KF_GAME_AUDIO_H
#include <kf/lib/bool.h>
#include <kf/lib/offsetof.h>
#include <kf/lib/types.h>
#include <kf/game/cd.h>
#include <psyq/sdk.h>

/* GAME.EXE sound effects: sound ids index the loaded sound table; bit 15 of
 * a spatial sound id selects an alternate panning mode. */
KF_ENUM_BEGIN(KfAudioPlaybackResult, s32)
    KF_AUDIO_NOT_PLAYED = 0,
    KF_AUDIO_PLAYED = 1
KF_ENUM_END(KfAudioPlaybackResult)

enum {
    KF_AUDIO_DEFAULT_MAX_DISTANCE = 0x4800,
    KF_AUDIO_DEFAULT_ATTENUATION_DISTANCE = 0x6000,
    KF_AUDIO_ALTERNATE_PAN_FLAG = 0x8000,
    KF_AUDIO_SOUND_INDEX_MASK = 0xfff,
    KF_AUDIO_SOUND_NONE = 0xff,
    KF_AUDIO_VAB_ID_NONE = -1,
    KF_AUDIO_VAB_SLOT_NONE = -1,
    KF_AUDIO_VOICE_ID_NONE = -1,
    KF_AUDIO_VAB_SLOT_COUNT = 130,
    KF_AUDIO_VOICE_HANDLE_COUNT = 10,
    KF_AUDIO_SOUND_PARAM_COUNT = 256,
    KF_AUDIO_SPU_VOICE_COUNT = 24
};

/* Stream slots are queued, retained while requested, then made reclaimable. */
KF_ENUM_BEGIN(KfAudioVabStreamState, s16)
    KF_AUDIO_VAB_STREAM_FREE = 0,
    KF_AUDIO_VAB_STREAM_IN_USE = 1,
    KF_AUDIO_VAB_STREAM_RECLAIMABLE = 2,
    KF_AUDIO_VAB_STREAM_LOADING = 3
KF_ENUM_END(KfAudioVabStreamState)

enum {
    KF_AUDIO_VAB_ID_STREAM_PENDING = 0xfe,
    KF_AUDIO_VAB_STREAM_POOL_COUNT = 5,
    KF_AUDIO_VAB_STREAM_SLOT_FOR_VAB_1 = 5,
    KF_AUDIO_VAB_STREAM_SLOT_FOR_VAB_0 = 6,
    KF_AUDIO_VAB_STREAM_SLOT_COUNT = 7
};

struct KfEulerAngles;

typedef struct KfAudioVabStreamSlot {
    KfAudioVabStreamState state;
    u8 *buffer;
} KfAudioVabStreamSlot;

typedef char kf_audio_vab_stream_slot_size[
    sizeof(KfAudioVabStreamSlot) == 8 ? 1 : -1];
typedef char kf_audio_vab_stream_slot_buffer_offset[
    offsetof(KfAudioVabStreamSlot, buffer) == 4 ? 1 : -1];

typedef struct KfAudioVabSlot {
    s16 vab_id;
    KfAudioVabStreamSlot *stream_slot;
} KfAudioVabSlot;

typedef char kf_audio_vab_slot_size[sizeof(KfAudioVabSlot) == 8 ? 1 : -1];
typedef char kf_audio_vab_slot_stream_slot_offset[
    offsetof(KfAudioVabSlot, stream_slot) == 4 ? 1 : -1];

typedef struct KfAudioVoiceHandle {
    s16 voice_id;
    s16 sound_id;
} KfAudioVoiceHandle;

typedef struct KfAudioVoiceParams {
    s16 vab_slot_index;
    s16 program;
    s16 tone;
    s16 note;
    u16 priority;
} KfAudioVoiceParams;

typedef char kf_audio_voice_params_size[sizeof(KfAudioVoiceParams) == 10 ? 1 : -1];
typedef char kf_audio_voice_priority_offset[
    offsetof(KfAudioVoiceParams, priority) == 8 ? 1 : -1];

typedef struct KfAudioVoiceState {
    KfAudioVoiceHandle handles[KF_AUDIO_VOICE_HANDLE_COUNT];
    /* Sound IDs are byte-indexed; the loaded parameter rows fill this span. */
    KfAudioVoiceParams params[KF_AUDIO_SOUND_PARAM_COUNT];
} KfAudioVoiceState;

/* The startup clear bounds this GAME audio state at 0xe9c bytes. Sequence
 * fields and the 130-entry VAB slot stride have direct retail witnesses. */
typedef struct KfGameAudioState {
    u_long *sequence_buffer;
    s16 sequence_id;
    b32 sequence_active;
    b32 sequence_ready;
    VECTOR listener_position;
    u16 listener_layer;
    SVECTOR listener_rotation;
    KfAudioVabSlot vab_slots[KF_AUDIO_VAB_SLOT_COUNT];
    KfAudioVoiceState voices;
    KfAudioVabStreamSlot vab_stream_slots[KF_AUDIO_VAB_STREAM_SLOT_COUNT];
} KfGameAudioState;

typedef char kf_game_audio_state_size[sizeof(KfGameAudioState) == 0xe9c ? 1 : -1];
typedef char kf_game_audio_sequence_active_offset[
    offsetof(KfGameAudioState, sequence_active) == 8 ? 1 : -1];
typedef char kf_game_audio_vab_offset[
    offsetof(KfGameAudioState, vab_slots) == 0x2c ? 1 : -1];
typedef char kf_game_audio_listener_position_offset[
    offsetof(KfGameAudioState, listener_position) == 0x10 ? 1 : -1];
typedef char kf_game_audio_listener_layer_offset[
    offsetof(KfGameAudioState, listener_layer) == 0x20 ? 1 : -1];
typedef char kf_game_audio_listener_rotation_offset[
    offsetof(KfGameAudioState, listener_rotation) == 0x22 ? 1 : -1];
typedef char kf_game_audio_handles_offset[
    offsetof(KfGameAudioState, voices.handles) == 0x43c ? 1 : -1];
typedef char kf_game_audio_params_offset[
    offsetof(KfGameAudioState, voices.params) == 0x464 ? 1 : -1];
typedef char kf_game_audio_params_236_offset[
    offsetof(KfGameAudioState, voices.params[236].vab_slot_index) == 0xd9c ? 1 : -1];
typedef char kf_game_audio_params_239_offset[
    offsetof(KfGameAudioState, voices.params[239].vab_slot_index) == 0xdba ? 1 : -1];
typedef char kf_game_audio_stream_slots_offset[
    offsetof(KfGameAudioState, vab_stream_slots) == 0xe64 ? 1 : -1];

extern KfGameAudioState sound_status;

void audio_initialize_runtime(void);
void audio_shutdown(void);
void audio_start_sequence(void);
void audio_stop_sequence(void);
KfAudioPlaybackResult audio_play_spatial(
    s32 sound, const VECTOR *position, s32 volume, s32 max_distance,
    s32 attenuation_distance, s32 note_offset);
KfAudioPlaybackResult audio_play_spatial_default_range(
    s32 sound, const VECTOR *position, s16 volume, s32 note_offset);
KfAudioPlaybackResult audio_play_spatial_range(
    s32 sound, const VECTOR *position, s16 volume, s32 max_distance,
    s32 attenuation_distance, s32 note_offset);
void audio_play_sound(s32 sound, s32 volume);
void audio_update_listener(const VECTOR *position, const SVECTOR *rotation);
void audio_play_marker_consumed_sound(void);
void audio_play_sound_at_volume_100(s32 sound);
void audio_refresh_voice_handles(void);
void audio_key_off_handle(KfAudioVoiceHandle *handle);
KfAudioVoiceHandle *audio_allocate_voice_handle(s32 sound_id);
void audio_key_on(s32 sound, s32 left_volume, s32 right_volume, s32 note_offset);
KfAudioVabStreamSlot *audio_acquire_vab_stream_slot(void);
void audio_vab_stream_callback(KfCdRequest *request);
void audio_queue_vab_stream(s32 archive_slot, s32 entry, s32 vab_slot_index);

#endif
