#ifndef KF_GAME_CD_H
#define KF_GAME_CD_H
#include <kf/lib/bool.h>
#include <kf/lib/offsetof.h>
#include <kf/lib/types.h>
#include <psyq/cd.h>
#include <psyq/sdk.h>

/* GAME.EXE CD layer: whole-sector file reads and the sector-indexed
 * CD\COM\*.T archives. */
enum {
    KF_CD_SECTOR_BYTES = 0x800,
    KF_CD_SECTOR_SHIFT = 11,
    KF_CD_SECTOR_WORDS = KF_CD_SECTOR_BYTES / 4,
    KF_CD_STREAM_CHUNK_SECTORS = 16,
    KF_CD_STREAM_CHUNK_BYTES = KF_CD_STREAM_CHUNK_SECTORS * KF_CD_SECTOR_BYTES,
    KF_CD_IMAGE_RECORD_HEADER_BYTES = 16,
    KF_CD_ARCHIVE_SLOTS = 8,
    /* Seed of the per-entry archive checksum held in each entry's last word. */
    KF_CD_CHECKSUM_SEED = 0x12345678
};

/* cd_report_error codes (the retail reporter is empty). player_update_frame
 * reports code 3 when Start is pressed. */
enum {
    KF_CD_ERROR_SEARCH = 0,
    KF_CD_ERROR_READ = 1,
    KF_CD_ERROR_3 = 3
};

/* Location and byte size of a file, the leading fields of CdlFILE. */
typedef struct KfCdExtent {
    CdlLOC pos;
    u_long size;
} KfCdExtent;

/* Open .T archive: sector 0's `count + 1` sector offsets and the file extent. */
typedef struct KfCdArchive {
    u16 *sector_offsets;
    KfCdExtent extent;
} KfCdArchive;

typedef char kf_cd_archive_size[sizeof(KfCdArchive) == 12 ? 1 : -1];

enum { KF_CD_REQUEST_CAPACITY = 16 };

/* Ring entry kind; the service switch dispatches on it and IDLE frees it. */
KF_ENUM_BEGIN(KfCdRequestKind, u8)
    KF_CD_REQUEST_IDLE = 0,
    KF_CD_REQUEST_CHECKSUM_READ = 0x10,
    KF_CD_REQUEST_SECTOR_CALLBACK = 0x20,
    KF_CD_REQUEST_VAB_READ = 0x30,
    KF_CD_REQUEST_IMAGE_STREAM = 0x40
KF_ENUM_END(KfCdRequestKind)

/* Completion callbacks first acknowledge the seek, then the sector read. */
KF_ENUM_BEGIN(KfCdRequestPhase, u8)
    KF_CD_REQUEST_PHASE_SEEK = 0,
    KF_CD_REQUEST_PHASE_READ = 1
KF_ENUM_END(KfCdRequestPhase)

/* VAB request payload phases, advanced by the CD completion callback. */
KF_ENUM_BEGIN(KfCdVabPhase, u8)
    KF_CD_VAB_PHASE_HEAD = 0,
    KF_CD_VAB_PHASE_BODY_READ = 1,
    KF_CD_VAB_PHASE_BODY_READY = 2
KF_ENUM_END(KfCdVabPhase)

/* Image-stream handshake: the sector callback marks a chunk ready and the
 * stream service consumes it. */
KF_ENUM_BEGIN(KfCdStreamState, u8)
    KF_CD_STREAM_WAITING = 0,
    KF_CD_STREAM_CHUNK_READY = 1
KF_ENUM_END(KfCdStreamState)

enum {
    KF_CD_VAB_BODY_CHUNK_SECTORS = 18,
    KF_CD_VAB_BODY_CHUNK_BYTES = KF_CD_VAB_BODY_CHUNK_SECTORS * KF_CD_SECTOR_BYTES
};

typedef struct KfCdRequest KfCdRequest;
typedef void (*KfCdRequestCallback)(KfCdRequest *request);
struct KfAudioVabStreamSlot;

typedef struct KfCdRequestPayloadVab {
    KfCdVabPhase phase;
    s16 slot_index;
    struct KfAudioVabStreamSlot *stream_slot;
} KfCdRequestPayloadVab;

typedef union KfCdRequestPayload {
    KfCdRequestPayloadVab vab;
    RECT image_rect;
} KfCdRequestPayload;

typedef char kf_cd_request_payload_size[
    sizeof(KfCdRequestPayload) == 8 ? 1 : -1];
typedef char kf_cd_vab_slot_index_offset[
    offsetof(KfCdRequestPayloadVab, slot_index) == 2 ? 1 : -1];
typedef char kf_cd_vab_stream_slot_offset[
    offsetof(KfCdRequestPayloadVab, stream_slot) == 4 ? 1 : -1];

/* One queued asynchronous CD request. */
struct KfCdRequest {
    KfCdRequestKind kind;
    KfCdRequestPhase phase;
    CdlLOC location;
    CdlLOC initial_location;
    u_long *destination;
    s32 sector_count;
    KfCdRequestCallback on_complete;
    KfCdRequestPayload payload;
    s16 chunk_sectors;
    s16 remaining_sectors;
    KfCdStreamState stream_complete;
};

typedef char kf_cd_request_size[sizeof(KfCdRequest) == 40 ? 1 : -1];
typedef char kf_cd_request_destination_offset[
    offsetof(KfCdRequest, destination) == 0x0c ? 1 : -1];
typedef char kf_cd_request_stream_complete_offset[
    offsetof(KfCdRequest, stream_complete) == 0x24 ? 1 : -1];

void cd_stream_limit_chunk(KfCdRequest *request);

/* CD layer state: event handles, VSync counters and the request ring. The
 * ring is addressed relative to the counters and ends at the tail pointer,
 * so they are one object. */
typedef struct KfCdState {
    long vsync_event;
    u32 vsync_count;
    u32 frame_count;
    long error_event;
    long complete_event;
    u8 unknown_14[4];
    long data_ready_event;
    KfCdRequest requests[KF_CD_REQUEST_CAPACITY];
    KfCdRequest *tail;
    KfCdRequest *current;
} KfCdState;

typedef char kf_cd_state_size[sizeof(KfCdState) == 0x2a4 ? 1 : -1];

extern char cd_path_prefix[5];
extern char cd_version_suffix[3];
extern KfCdArchive cd_archives[KF_CD_ARCHIVE_SLOTS];
extern KfCdState cd_state;
extern u8 cd_stream_work_buffer[];
extern u8 DAT_800864dc;

s32 cd_bcd_to_int(u8 bcd);
u32 cd_int_to_bcd(u8 value);
u32 cd_location_to_sector(CdlLOC *location);
void cd_sector_to_location(CdlLOC *location, u32 sector);
void cd_location_add(CdlLOC *base, u32 sector_offset, CdlLOC *result);
void cd_vsync_handler(void);
void cd_wait_two_vsyncs(void);
void cd_request_advance(KfCdRequest *request);
void cd_complete_handler(void);
void cd_data_ready_handler(void);
void cd_error_handler(void);
void cd_request_wait_idle(void);
void cd_request_wait_done(KfCdRequest *request);
KfCdRequest *cd_request_enqueue(KF_ENUM_PARAM(KfCdRequestKind, s32) kind, CdlLOC *location, u32 byte_size,
    u_long *destination, KfCdRequestCallback on_complete);
void cd_request_yield(void);
void cd_request_service_vab(void);
void cd_request_service_stream(void);
b32 cd_sectors_corrupt(u32 *data, s32 sector_count);
u32 cd_archive_entry_extent(u16 slot, u16 entry, CdlLOC *location);
void cd_archive_queue_read(u16 slot, u16 entry, u_long *destination,
    KfCdRequestCallback on_complete);
void cd_archive_queue_sector_callback_read(u16 slot, u16 entry, u_long *destination,
    KfCdRequestCallback on_complete);
void cd_archive_queue_stream_read(u16 slot, u16 entry, u_long *destination,
    KfCdRequestCallback on_complete);
u32 cd_archive_entry_size(u16 slot, u16 entry);
void cd_report_error(s32 code);
void cd_read_sectors(CdlLOC *location, u_long *destination, s32 sector_count);
u8 *cd_extent_load(KfCdExtent *extent);
void cd_extent_read_into(u_long *destination, KfCdExtent *extent, u32 size);
void cd_archive_read(u16 slot, u16 entry, u_long *destination);
u8 *cd_file_load(const char *name);
s32 cd_file_load_into(u_long *destination, const char *name, u32 size);
void cd_archive_open(u16 slot, const char *name);
void cd_archive_read_chunked(u16 slot, u16 entry, u8 *destination,
    KfCdRequestCallback on_complete);
void cd_stream_mark_complete(KfCdRequest *request);
void cd_map_stream_read(s32 slot, s32 entry);
void cd_initialize(void);
void cd_close_events(void);

#endif
