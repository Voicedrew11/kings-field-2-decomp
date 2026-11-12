#ifndef KF_GAME_CALLBACK_H
#define KF_GAME_CALLBACK_H

#include <kf/lib/bool.h>
#include <kf/lib/offsetof.h>
#include <kf/lib/types.h>

/* The initialized table has 32 function-pointer rows; the BSS table's extent
 * remains open. Callback signatures are resolved at their individual uses. */
typedef void (*KfCallback)();

enum {
    KF_RESOURCE_SLOT_MAP_REGION = 0,
    KF_RESOURCE_SLOT_TMD = 1,
    KF_RESOURCE_SLOT_TIM = 2,
    KF_RESOURCE_SLOT_VAB = 3,
    KF_RESOURCE_SLOT_SEQUENCE = 4,
    KF_RESOURCE_SLOT_COUNT = 5
};

typedef struct KfResourceTransitionOffset {
    s8 x;
    s8 z;
    s8 y;
} KfResourceTransitionOffset;
typedef char kf_resource_transition_offset_size[
    sizeof(KfResourceTransitionOffset) == 3 ? 1 : -1];

/* resource_advance_transition steps; each CD completion callback advances
 * the phase and PENDING_IO waits for the queued read. */
KF_ENUM_BEGIN(KfResourceTransitionPhase, s16)
    KF_RESOURCE_TRANSITION_BEGIN_MAP = 0,
    KF_RESOURCE_TRANSITION_LOAD_MAP_CELLS = 1,
    KF_RESOURCE_TRANSITION_QUEUE_MAP_ACTORS = 2,
    KF_RESOURCE_TRANSITION_LOAD_MAP_ACTORS = 3,
    KF_RESOURCE_TRANSITION_QUEUE_TIM = 4,
    KF_RESOURCE_TRANSITION_FADE_AUDIO = 5,
    KF_RESOURCE_TRANSITION_FINISH_AUDIO = 6,
    KF_RESOURCE_TRANSITION_PENDING_IO = 0xf0
KF_ENUM_END(KfResourceTransitionPhase)

/* Startup clears this 0x1c-byte runtime state. Its active table pointer is
 * replaced by both an initialized table and a BSS table. */
typedef struct KfResourceState {
    KfBoolS16 transition_active;
    KfResourceTransitionPhase transition_phase;
    u8 active_resource_ids[KF_RESOURCE_SLOT_COUNT];
    u8 current_map_region_id;
    KfCallback *active_table;
    u8 requested_resource_ids[KF_RESOURCE_SLOT_COUNT];
    b8 world_shift_applied;
    b8 tmd_object_limit_active;
    KfResourceTransitionOffset transition_offset;
    s16 sequence_fade_volume;
} KfResourceState;

typedef char kf_resource_state_size[sizeof(KfResourceState) == 0x1c ? 1 : -1];
typedef char kf_resource_state_table_offset[offsetof(KfResourceState, active_table) == 0xc ? 1 : -1];
typedef char kf_resource_state_active_resource_ids_offset[offsetof(KfResourceState, active_resource_ids) == 0x04 ? 1 : -1];
typedef char kf_resource_state_current_map_region_id_offset[offsetof(KfResourceState, current_map_region_id) == 0x09 ? 1 : -1];
typedef char kf_resource_state_requested_resource_ids_offset[offsetof(KfResourceState, requested_resource_ids) == 0x10 ? 1 : -1];
typedef char kf_resource_state_world_shift_applied_offset[offsetof(KfResourceState, world_shift_applied) == 0x15 ? 1 : -1];
typedef char kf_resource_state_tmd_object_limit_active_offset[offsetof(KfResourceState, tmd_object_limit_active) == 0x16 ? 1 : -1];
typedef char kf_resource_state_transition_offset_offset[offsetof(KfResourceState, transition_offset) == 0x17 ? 1 : -1];
typedef char kf_resource_state_sequence_fade_volume_offset[offsetof(KfResourceState, sequence_fade_volume) == 0x1a ? 1 : -1];

extern KfResourceState asset_status;
extern KfCallback callback_default_table[32];

void callback_invoke_slot_04_zero(void);
void resource_noop_callback();
/* CD request completion callbacks; the finished request is unused. */
struct KfCdRequest;
void resource_transition_set_phase_1(struct KfCdRequest *request);
void resource_transition_set_phase_2(struct KfCdRequest *request);
void resource_transition_set_phase_3(struct KfCdRequest *request);
void resource_transition_set_phase_4(struct KfCdRequest *request);
void resource_transition_set_phase_6(struct KfCdRequest *request);

#endif
