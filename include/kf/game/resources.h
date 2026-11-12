#ifndef KF_GAME_RESOURCES_H
#define KF_GAME_RESOURCES_H

#include <kf/lib/bool.h>
#include <kf/lib/types.h>
#include <kf/game/render_types.h>
#include <psyq/sdk.h>

enum {
    KF_RESOURCE_ARCHIVE_MO = 0,
    KF_RESOURCE_ARCHIVE_RTMD = 1,
    KF_RESOURCE_ARCHIVE_RTIM = 2,
    KF_RESOURCE_ARCHIVE_TALK = 3,
    KF_RESOURCE_ARCHIVE_VAB = 4,
    KF_RESOURCE_ARCHIVE_FDAT = 5,
    KF_RESOURCE_ARCHIVE_ITEM = 6,
    KF_RESOURCE_REQUEST_KEEP = 0xff,
    KF_RESOURCE_REQUEST_START_SEQUENCE = 200,
    KF_RESOURCE_ACTIVE_UNINITIALIZED = 99,
    /* An active slot holding no resource. */
    KF_RESOURCE_ACTIVE_NONE = 0xff,
    KF_RESOURCE_OFFSET_NO_SHIFT = 127
};

extern u8 resource_tmd_arc_work[0x37000];

KF_ENUM_PARAM(KfMapLayerMask, u32) map_cell_layer_mask(const VECTOR *position);
KF_ENUM_PARAM(KfMapLayerMask, u32) map_cell_layer_mask_radius(
    const VECTOR *position, s32 radius);
b32 map_cell_visible(const VECTOR *position, s32 radius_x, s32 radius_z);

void translate_active_world_positions(s32 dx, s32 dy, s32 dz);
KF_VALUELESS_S32 resource_request_transition(u8 map_region_id, u8 tmd_id, u8 tim_id, u8 vab_id,
    u8 sequence_id, s8 offset_x, s8 offset_z, s8 offset_y);
void resource_advance_transition(void);
void resource_initialize_game_assets(void);
void resource_run_initial_transition(void);

void tim_upload_images(u8 *tim_data);
void resource_tmd_read_complete(u8 *data);
void resource_tmd_queue_read(s32 archive_slot, s32 entry, s32 registry_index);
void resource_vab_update_range(s32 archive_slot, s32 entry, s32 vab_slot,
    s32 count, b8 *flags);
void resource_tmd_update_range(s32 archive_slot, s32 entry, s32 registry_index,
    s32 count, b8 *flags);

#endif
