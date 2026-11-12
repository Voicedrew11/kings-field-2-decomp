#ifndef KF_GAME_TMD_H
#define KF_GAME_TMD_H

#include <kf/lib/types.h>
#include <psyq/sdk.h>

/* Standard Sony TMD layout as registered by GAME.EXE. */
enum {
    KF_TMD_HEADER_BYTES = 12,
    /* Slot 0 holds the map-cell TMD read into resource_tmd_arc_work. */
    KF_TMD_SLOT_MAP = 0,
    KF_TMD_SLOT_MENU_ITEM = 3
};

typedef struct KfTmdHeader {
    u32 id;
    u32 flags;
    u32 object_count;
} KfTmdHeader;

/* Standard 0x1c-byte object-table record in an unlinked TMD payload. */
typedef struct KfTmdObject {
    u32 vertex_offset;
    u32 vertex_count;
    u32 normal_offset;
    u32 normal_count;
    u32 primitive_offset;
    u32 primitive_count;
    s32 scale;
} KfTmdObject;

typedef char kf_tmd_object_size[sizeof(KfTmdObject) == 0x1c ? 1 : -1];

/* Object records follow a KfTmdHeader; vertex offsets count from its end. */
#define TMD_OBJECTS(asset) ((KfTmdObject *)((asset) + 1))
#define TMD_OBJECT_VERTICES(asset, object) \
    ((SVECTOR *)((u8 *)(asset) + KF_TMD_HEADER_BYTES + (object)->vertex_offset))
#define TMD_SECTION(asset, offset) ((u8 *)(asset) + ((offset) + KF_TMD_HEADER_BYTES))

void tmd_select(u16 slot);
KfTmdObject *tmd_get_object(u16 index);
void tmd_set_current_vertices(SVECTOR *vertices);
void tmd_select_object_vertices(u16 index);
void tmd_prepare_primitive_indices(KfTmdHeader *tmd);
void tmd_register(u16 slot, KfTmdHeader *tmd);
void tmd_set_slot(u16 slot, KfTmdHeader *tmd);
void tmd_project_vertices_with_fog(s32 count);
void tmd_project_vertices_mark_clipped(s32 count);
void tmd_transform_vertices(s32 count);
void tmd_transform_vertices_depth(s32 count, s16 depth);
void tmd_project_vertices(s32 count);

#endif
