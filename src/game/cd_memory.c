#include <kf/lib/address.h>
#include <kf/lib/memory_layout.h>
#include <kf/lib/null.h>
#include <kf/game/cd.h>
#include <kf/game/memory.h>
#include <psyq/kernel.h>
#include <psyq/libc.h>
#include <psyq/pad.h>
#include <psyq/sdk.h>

/* KSEG0 base of main RAM, which malloc results must fall within. */
#define MEMORY_RAM_BASE 0x80000000u

#define NEXT_BLOCK(block) ((KfMemoryBlock *)((u8 *)((block) + 1) + (block)->size))
#define MEMORY_BLOCK(data) ((KfMemoryBlock *)(data) - 1)

enum {
    CD_READ_ATTEMPTS = 3,
    CD_READ_SYNC_POLL = 1,
    CD_PATH_BYTES = 64,
    MEMORY_ARENA_END_MARKER_BYTES = sizeof(u32)
};

/* LIBAPI's HwCdRom, RCntCNT3, EvSpINT, EvSpCOMP, EvSpDR, EvSpERROR and
 * EvMdINTR, which the Psy-Q 3.0 kit's headers do not define. */
#define CD_EVENT_CLASS_CDROM 0xf0000003
#define CD_EVENT_CLASS_VSYNC_COUNTER 0xf2000003
enum {
    CD_EVENT_SPEC_INTERRUPT = 0x0002,
    CD_EVENT_SPEC_COMPLETE = 0x0020,
    CD_EVENT_SPEC_DATA_READY = 0x0040,
    CD_EVENT_SPEC_ERROR = 0x8000,
    CD_EVENT_MODE_INTERRUPT = 0x1000
};

RODATA(0x80011074, 0x21)

DATA(0x8006e5b4, 0x5, ".sdata")
char cd_path_prefix[5] = "\\CD\\";
DATA(0x8006e5bc, 0x3, ".sdata")
char cd_version_suffix[3] = ";1";

DATA(0x8009bfe4, 0x5f000, ".bss")
u8 game_resource_arena[KF_GAME_RESOURCE_ARENA_CAPACITY];

DATA(0x801b6ca4, 0x2a4, ".bss")
KfCdState cd_state;
DATA(0x801b6f48, 0x60, ".bss")
KfCdArchive cd_archives[KF_CD_ARCHIVE_SLOTS];

/* Provisional extent: the first map payload copies 0xfa00 bytes from this
 * buffer, while the following BSS identity begins at 0x801c7fac. */
DATA(0x801b6fa8, 0x11000, ".bss")
u8 cd_stream_work_buffer[0x11000];

ADDRESS(0x80016f50, 0xc)
void cd_stream_mark_complete(KfCdRequest *request)
{
    request->stream_complete = KF_CD_STREAM_CHUNK_READY;
}

ADDRESS(0x80016f5c, 0x30)
void cd_map_stream_read(s32 slot, s32 entry)
{
    cd_archive_read_chunked(slot, entry, cd_stream_work_buffer,
        cd_stream_mark_complete);
}

ADDRESS(0x80016f8c, 0x3c)
void cd_stream_limit_chunk(KfCdRequest *request)
{
    if (request->remaining_sectors > KF_CD_STREAM_CHUNK_SECTORS) {
        request->sector_count = KF_CD_STREAM_CHUNK_SECTORS;
        request->remaining_sectors -= KF_CD_STREAM_CHUNK_SECTORS;
    } else {
        request->sector_count = request->remaining_sectors;
        request->remaining_sectors = 0;
    }
}

ADDRESS(0x80016fc8, 0x27c)
void cd_request_service_stream(void)
{
    KfCdRequest *request;
    u16 *source;
    s32 consumed;

    EnterCriticalSection();
    request = cd_state.current;
    if (request->kind != KF_CD_REQUEST_IMAGE_STREAM
        || request->stream_complete != KF_CD_STREAM_CHUNK_READY) {
        goto leave_critical;
    }
    consumed = 0;
    source = (u16 *)request->destination;
    request->stream_complete = KF_CD_STREAM_WAITING;
    ExitCriticalSection();

    for (;;) {
        s32 available = KF_CD_STREAM_CHUNK_BYTES - consumed;
        s16 height = request->payload.image_rect.h;

        if (height != 0) {
            s32 width = request->payload.image_rect.w;
            s32 bytes = (height * width) << 1;

            if (available < bytes) {
                s32 rows = (available / width) >> 1;
                if ((s16)rows != 0) {
                    request->payload.image_rect.h = rows;
                    LoadImage(&request->payload.image_rect, (u_long *)source);
                    request->payload.image_rect.h = height - rows;
                    request->payload.image_rect.y = rows + request->payload.image_rect.y;
                }
                break;
            }
            request->payload.image_rect.h = height;
            LoadImage(&request->payload.image_rect, (u_long *)source);
            request->payload.image_rect.h = 0;
            consumed += bytes;
            source = (u16 *)((u8 *)source + bytes);
            continue;
        }

        if ((u32)available < KF_CD_IMAGE_RECORD_HEADER_BYTES) {
            break;
        }
        if (source[0] != source[4] || source[1] != source[5] ||
            source[2] != source[6] || source[3] != source[7] ||
            source[2] == 0 || source[3] == 0) {
            request->payload.image_rect.h = 0;
            request->stream_complete = KF_CD_STREAM_WAITING;
            request->remaining_sectors = request->chunk_sectors;
            request->location = request->initial_location;
            cd_stream_limit_chunk(request);
            request->phase = KF_CD_REQUEST_PHASE_SEEK;
            CdSeekP(&request->location);
            return;
        }

        consumed += KF_CD_IMAGE_RECORD_HEADER_BYTES;
        if (source[0] == 0xffff) {
            DrawSync(0);
            request->sector_count = 0;
            cd_request_advance(request);
            return;
        }
        setRECT(&request->payload.image_rect, source[0], source[1],
                source[2], source[3]);
        source += 8;
    }

    cd_location_add(&request->location, request->sector_count,
        &request->location);
    cd_stream_limit_chunk(request);
    request->phase = KF_CD_REQUEST_PHASE_SEEK;
    DrawSync(0);

    CdSeekP(&request->location);
    return;

leave_critical:
    ExitCriticalSection();
}

ADDRESS(0x80017244, 0x30)
const u32 *resource_copy_words(u32 *destination, const u32 *source, u32 word_count)
{
    while (word_count-- != 0) {
        *destination++ = *source++;
    }
    return source;
}

ADDRESS(0x80017274, 0x30)
const u16 *resource_copy_halfwords(u16 *destination, const u16 *source, u32 halfword_count)
{
    while (halfword_count-- != 0) {
        *destination++ = *source++;
    }
    return source;
}

ADDRESS(0x800172a4, 0x24)
u32 *repeat_store_word(u32 *destination, u32 value, s32 count)
{
    while (count-- != 0) {
        *destination++ = value;
    }
    return destination;
}

ADDRESS(0x800172c8, 0x24)
u16 *repeat_store_halfword(u16 *destination, u16 value, s32 count)
{
    while (count-- != 0) {
        *destination++ = value;
    }
    return destination;
}

ADDRESS(0x800172ec, 0x84)
void memory_arena_coalesce_free(KfMemoryBlock *block)
{
    KfMemoryBlock *first = block;

    if (block->kind == KF_MEMORY_BLOCK_END) {
        return;
    }
    do {
        if (block->kind == KF_MEMORY_BLOCK_FREE) {
            block = NEXT_BLOCK(block);
            if (block->kind != KF_MEMORY_BLOCK_FREE) {
                return;
            }
            first->size += sizeof(KfMemoryBlock) + block->size;
        } else {
            block = NEXT_BLOCK(block);
        }
    } while (block->kind != KF_MEMORY_BLOCK_END);
}

ADDRESS(0x80017370, 0x20)
void memory_arena_free(KfMemoryBlock *block)
{
    u8 **owner = block->owner;

    block->kind = KF_MEMORY_BLOCK_FREE;
    if (owner != NULL) {
        *owner = NULL;
        block->owner = NULL;
    }
}

ADDRESS(0x80017390, 0x158)
KfMemoryBlock *memory_arena_find_block(KfMemoryBlock *arena, u32 size)
{
    KfMemoryBlock *block = arena;
    KfMemoryBlock *first;

    while (block->kind != KF_MEMORY_BLOCK_END) {
        if (block->kind == KF_MEMORY_BLOCK_FREE && block->size >= size) {
            return block;
        }
        block = NEXT_BLOCK(block);
    }

    block = arena;
    while (block->kind != KF_MEMORY_BLOCK_END) {
        if (block->kind == KF_MEMORY_BLOCK_RECLAIMABLE) {
            first = block;
            while (block->kind == KF_MEMORY_BLOCK_RECLAIMABLE) {
                block = NEXT_BLOCK(block);
            }
            if ((u32)((u8 *)block - (u8 *)first - sizeof(KfMemoryBlock)) >= size) {
                block = first;
                while (block->kind == KF_MEMORY_BLOCK_RECLAIMABLE) {
                    memory_arena_free(block);
                    block = NEXT_BLOCK(block);
                }
                memory_arena_coalesce_free(first);
                return first;
            }
        } else {
            block = NEXT_BLOCK(block);
        }
    }
    return NULL;
}

ADDRESS(0x800174e8, 0x98)
void memory_arena_wait_pending(KfMemoryBlock *arena)
{
    KfMemoryBlock *block = arena;

    while (block->kind != KF_MEMORY_BLOCK_END) {
        while (block->kind == KF_MEMORY_BLOCK_PENDING) {
            cd_request_wait_idle();
        }
        block = NEXT_BLOCK(block);
    }
}

ADDRESS(0x80017580, 0xe4)
void memory_arena_compact(KfMemoryBlock *arena)
{
    KfMemoryBlock *read_block = arena;
    KfMemoryBlock *write_block = arena;

    memory_arena_wait_pending(arena);
    if (read_block->kind != KF_MEMORY_BLOCK_END) {
        do {
            KfMemoryBlock *next = (KfMemoryBlock *)((u8 *)read_block + read_block->size + sizeof(KfMemoryBlock));

            if (read_block->kind == KF_MEMORY_BLOCK_RECLAIMABLE) {
                memory_arena_free(read_block);
            }
            if (read_block->kind == KF_MEMORY_BLOCK_OWNED) {
                if (read_block != write_block) {
                    u32 *source = (u32 *)read_block;
                    u32 *destination = (u32 *)write_block;
                    u32 word_count = (read_block->size + sizeof(KfMemoryBlock)) >> 2;

                    do {
                        *destination++ = *source++;
                    } while (--word_count != 0);
                    *write_block->owner = (u8 *)(write_block + 1);
                    write_block = (KfMemoryBlock *)destination;
                } else {
                    write_block = next;
                }
            }
            read_block = next;
        } while (read_block->kind != KF_MEMORY_BLOCK_END);
    }
    write_block->kind = KF_MEMORY_BLOCK_FREE;
    write_block->size = (u8 *)read_block - (u8 *)write_block - sizeof(KfMemoryBlock);
}

ADDRESS(0x80017664, 0x20)
void memory_arena_initialize_blocks(KfMemoryBlock *arena, u32 capacity)
{
    arena->kind = KF_MEMORY_BLOCK_FREE;
    arena->size = capacity - sizeof(KfMemoryBlock) - MEMORY_ARENA_END_MARKER_BYTES;
    capacity -= MEMORY_ARENA_END_MARKER_BYTES;
    arena = (KfMemoryBlock *)((u8 *)arena + capacity);
    arena->kind = KF_MEMORY_BLOCK_END;
}

ADDRESS(0x80017684, 0xb8)
u8 *memory_arena_allocate_block(KfMemoryBlock *arena, u32 size, u8 **owner)
{
    KfMemoryBlock *block = memory_arena_find_block(arena, size);
    u32 remainder;
    u32 usable;
    u32 available;
    u8 *data;

    if (block == NULL) {
        memory_arena_compact(arena);
        block = memory_arena_find_block(arena, size);
        if (block == NULL) {
            return NULL;
        }
    }
    available = block->size;
    usable = available - sizeof(KfMemoryBlock);
    remainder = usable - size;
    if ((s32)remainder >= 2060) {
        KfMemoryBlock *payload_end = (KfMemoryBlock *)((u8 *)block + size);
        payload_end[1].kind = KF_MEMORY_BLOCK_FREE;
        payload_end[1].size = remainder;
        payload_end[1].owner = NULL;
    } else {
        size = available;
    }
    block->kind = KF_MEMORY_BLOCK_OWNED;
    block->size = size;
    block->owner = owner;
    data = (u8 *)(block + 1);
    if (owner != NULL) {
        *owner = data;
    }
    return data;
}

ADDRESS(0x8001773c, 0x20)
void memory_block_release(u8 *data)
{
    memory_arena_free(MEMORY_BLOCK(data));
}

ADDRESS(0x8001775c, 0x8)
void memory_block_set_kind(u8 *data, KfMemoryBlockKind kind)
{
    MEMORY_BLOCK(data)->kind = kind;
}

ADDRESS(0x80017764, 0xc)
KfMemoryBlockKind memory_block_kind(u8 *data)
{
    return MEMORY_BLOCK(data)->kind;
}

ADDRESS(0x80017770, 0x8)
void memory_block_set_flags(u8 *data, u8 flags)
{
    MEMORY_BLOCK(data)->flags = flags;
}

ADDRESS(0x80017778, 0xc)
u8 memory_block_flags(u8 *data)
{
    return MEMORY_BLOCK(data)->flags;
}

ADDRESS(0x80017784, 0x8)
void memory_block_set_tag(u8 *data, u16 tag)
{
    MEMORY_BLOCK(data)->tag = tag;
}

ADDRESS(0x8001778c, 0xc)
u16 memory_block_tag(u8 *data)
{
    return MEMORY_BLOCK(data)->tag;
}

ADDRESS(0x80017798, 0x38)
u8 *memory_malloc_checked(u32 size)
{
    u8 *block = (u8 *)malloc(size);

    if ((u32)block + MEMORY_RAM_BASE >= KF_MAIN_RAM_BYTES) {
        return NULL;
    }
    return block;
}

ADDRESS(0x800177d0, 0x28)
u8 *memory_allocate(u32 size)
{
    return memory_malloc_checked((size + 3) & ~3);
}

ADDRESS(0x800177f8, 0x20)
void memory_free(u8 *data)
{
    free((void *)data);
}

/* Services the VAB and stream requests at the head of the CD queue, then
 * waits for the GPU and the next VSync. */
ADDRESS(0x80017818, 0x38)
void cd_request_yield(void)
{
    cd_request_service_vab();
    cd_request_service_stream();
    DrawSync(0);
    VSync(0);
}

ADDRESS(0x80017850, 0x30)
void cd_vsync_handler(void)
{
    cd_state.vsync_count++;
    cd_state.frame_count++;
}

ADDRESS(0x80017880, 0x60)
void cd_wait_two_vsyncs(void)
{
    for (;;) {
        EnterCriticalSection();
        if (cd_state.vsync_count < 2) {
            ExitCriticalSection();
            VSync(0);
        } else {
            cd_state.vsync_count = 0;
            ExitCriticalSection();
            break;
        }
    }
}

ADDRESS(0x800178e0, 0x6c)
void cd_request_advance(KfCdRequest *request)
{
    KfCdRequest *next = request;

    next->kind = KF_CD_REQUEST_IDLE;
    next->phase = KF_CD_REQUEST_PHASE_SEEK;
    next++;
    if (next == cd_state.requests + KF_CD_REQUEST_CAPACITY) {
        next -= KF_CD_REQUEST_CAPACITY;
    }
    cd_state.current = next;
    if (next->kind != KF_CD_REQUEST_IDLE) {
        CdSeekP(&next->location);
    } else {
        CdPause();
    }
}

ADDRESS(0x8001794c, 0x14c)
void cd_complete_handler(void)
{
    KfCdRequest *request = cd_state.current;

    switch (request->kind) {
    case KF_CD_REQUEST_IDLE:
        break;
    case KF_CD_REQUEST_CHECKSUM_READ:
    case KF_CD_REQUEST_VAB_READ:
    case KF_CD_REQUEST_IMAGE_STREAM:
        switch (request->phase) {
        case KF_CD_REQUEST_PHASE_SEEK:
            CdRead(request->sector_count, request->destination, CdlModeSpeed);
            request->phase = KF_CD_REQUEST_PHASE_READ;
            break;
        case KF_CD_REQUEST_PHASE_READ:
            if (request->kind == KF_CD_REQUEST_CHECKSUM_READ) {
                if (cd_sectors_corrupt((u32 *)request->destination,
                        request->sector_count)) {
                    request->phase = KF_CD_REQUEST_PHASE_SEEK;
                    CdSeekP(&request->initial_location);
                    break;
                }
                if (request->on_complete != NULL) {
                    ((void (*)(u_long *))request->on_complete)(request->destination);
                }
                cd_request_advance(request);
            } else if (request->on_complete != NULL) {
                request->on_complete(request);
            }
            break;
        }
        break;
    case KF_CD_REQUEST_SECTOR_CALLBACK:
        if (request->phase == KF_CD_REQUEST_PHASE_SEEK) {
            CdRead2(CdlModeSpeed);
            request->phase = KF_CD_REQUEST_PHASE_READ;
        }
        break;
    }
}

ADDRESS(0x80017a98, 0x7c)
void cd_data_ready_handler(void)
{
    KfCdRequest *request = cd_state.current;

    if (request->kind != KF_CD_REQUEST_IDLE &&
        request->kind == KF_CD_REQUEST_SECTOR_CALLBACK) {
        CdGetSector((void *)request->destination, KF_CD_SECTOR_WORDS);
        request->sector_count--;
        if (request->on_complete != NULL) {
            request->on_complete(request);
        }
        if (request->sector_count == 0) {
            cd_request_advance(request);
        }
    }
}

ADDRESS(0x80017b14, 0x8)
void cd_error_handler(void)
{
}

enum {
    CD_SECTORS_PER_SECOND = 75,
    CD_SECTORS_PER_MINUTE = 60 * CD_SECTORS_PER_SECOND
};

ADDRESS(0x80017b1c, 0x20)
s32 cd_bcd_to_int(u8 bcd)
{
    return (bcd >> 4) * 10 + (bcd & 0xf);
}

ADDRESS(0x80017b3c, 0x2c)
u32 cd_int_to_bcd(u8 value)
{
    return itob(value);
}

/* Absolute sector of a BCD location, without CdPosToInt's 150-sector lead-in. */
ADDRESS(0x80017b68, 0x80)
u32 cd_location_to_sector(CdlLOC *location)
{
    return cd_bcd_to_int(location->minute) * CD_SECTORS_PER_MINUTE
        + cd_bcd_to_int(location->second) * CD_SECTORS_PER_SECOND
        + cd_bcd_to_int(location->sector);
}

ADDRESS(0x80017be8, 0x84)
void cd_sector_to_location(CdlLOC *location, u32 sector)
{
    u32 rest;

    location->minute = cd_int_to_bcd(sector / CD_SECTORS_PER_MINUTE);
    rest = sector % CD_SECTORS_PER_MINUTE;
    location->second = cd_int_to_bcd(rest / CD_SECTORS_PER_SECOND);
    location->sector = cd_int_to_bcd(rest % CD_SECTORS_PER_SECOND);
    location->track = 0;
}

ADDRESS(0x80017c6c, 0x3c)
void cd_location_add(CdlLOC *base, u32 sector_offset, CdlLOC *result)
{
    cd_sector_to_location(result, cd_location_to_sector(base) + sector_offset);
}

ADDRESS(0x80017ca8, 0xb0)
void cd_request_wait_idle(void)
{
    for (;;) {
        EnterCriticalSection();
        if (cd_state.current == cd_state.tail && cd_state.current->kind == KF_CD_REQUEST_IDLE) {
            break;
        }
        ExitCriticalSection();
        cd_request_yield();
    }
    ExitCriticalSection();
}

ADDRESS(0x80017d58, 0x58)
void cd_request_wait_done(KfCdRequest *request)
{
    for (;;) {
        EnterCriticalSection();
        if (request->kind == KF_CD_REQUEST_IDLE) {
            break;
        }
        ExitCriticalSection();
        cd_request_yield();
    }
    ExitCriticalSection();
}

/* Nonzero unless the last word of DATA equals KF_CD_CHECKSUM_SEED plus the
 * sum of every preceding word. */
ADDRESS(0x80017db0, 0x54)
b32 cd_sectors_corrupt(u32 *data, s32 sector_count)
{
    u32 sum = KF_CD_CHECKSUM_SEED;
    s32 last = sector_count * KF_CD_SECTOR_WORDS - 1;
    u32 *word = data;
    s32 remaining;

    for (remaining = last - 1; remaining != -1; remaining--) {
        sum += *word++;
    }
    return sum != data[last];
}

ADDRESS(0x80017e04, 0x118)
KfCdRequest *cd_request_enqueue(KF_ENUM_PARAM(KfCdRequestKind, s32) kind, CdlLOC *location, u32 byte_size,
    u_long *destination, KfCdRequestCallback on_complete)
{
    KfCdRequest *request = cd_state.tail;

    cd_request_wait_done(request);
    EnterCriticalSection();
    request->kind = kind;
    request->phase = KF_CD_REQUEST_PHASE_SEEK;
    request->location = *location;
    request->initial_location = request->location;
    request->destination = destination;
    request->sector_count = (byte_size + KF_CD_SECTOR_BYTES - 1) >> KF_CD_SECTOR_SHIFT;
    request->on_complete = on_complete;
    cd_state.tail = cd_state.tail + 1;
    if (cd_state.tail == cd_state.requests + KF_CD_REQUEST_CAPACITY) {
        cd_state.tail = cd_state.requests;
    }
    if (cd_state.current == request) {
        ExitCriticalSection();
    CdSeekP(location);
    } else {
        ExitCriticalSection();
    }
    return request;
}

ADDRESS(0x80017f1c, 0x88)
u32 cd_archive_entry_extent(u16 slot, u16 entry, CdlLOC *location)
{
    KfCdArchive *archive = &cd_archives[slot];
    u32 start = archive->sector_offsets[entry];

    if (location != NULL) {
        cd_location_add(&archive->extent.pos, start, location);
    }
    return (archive->sector_offsets[entry + 1] - start) << KF_CD_SECTOR_SHIFT;
}

ADDRESS(0x80017fa4, 0x54)
void cd_archive_queue_read(u16 slot, u16 entry, u_long *destination,
    KfCdRequestCallback on_complete)
{
    CdlLOC location;
    u32 size = cd_archive_entry_extent(slot, entry, &location);

    cd_request_enqueue(KF_CD_REQUEST_CHECKSUM_READ, &location, size, destination, on_complete);
}

ADDRESS(0x80017ff8, 0x54)
void cd_archive_queue_sector_callback_read(u16 slot, u16 entry, u_long *destination,
    KfCdRequestCallback on_complete)
{
    CdlLOC location;
    u32 size = cd_archive_entry_extent(slot, entry, &location);

    cd_request_enqueue(KF_CD_REQUEST_SECTOR_CALLBACK, &location, size, destination, on_complete);
}

ADDRESS(0x8001804c, 0x54)
void cd_archive_queue_stream_read(u16 slot, u16 entry, u_long *destination,
    KfCdRequestCallback on_complete)
{
    CdlLOC location;
    u32 size = cd_archive_entry_extent(slot, entry, &location);

    cd_request_enqueue(KF_CD_REQUEST_VAB_READ, &location, size, destination, on_complete);
}

ADDRESS(0x800180a0, 0xc4)
void cd_archive_read_chunked(u16 slot, u16 entry, u8 *destination,
    KfCdRequestCallback on_complete)
{
    KfCdRequest *request = cd_state.tail;
    CdlLOC location;
    u32 size;

    cd_request_wait_done(request);
    request->payload.image_rect.h = 0;
    request->stream_complete = KF_CD_STREAM_WAITING;
    size = cd_archive_entry_extent(slot, entry, &location);
    request->remaining_sectors = size >> KF_CD_SECTOR_SHIFT;
    request->chunk_sectors = request->remaining_sectors;
    if (request->remaining_sectors <= KF_CD_STREAM_CHUNK_SECTORS) {
        request->remaining_sectors = 0;
        cd_request_enqueue(KF_CD_REQUEST_IMAGE_STREAM, &location, size,
            (u_long *)destination, on_complete);
    } else {
        request->remaining_sectors -= KF_CD_STREAM_CHUNK_SECTORS;
        cd_request_enqueue(KF_CD_REQUEST_IMAGE_STREAM, &location, KF_CD_STREAM_CHUNK_BYTES,
            (u_long *)destination, on_complete);
    }
}

ADDRESS(0x80018164, 0x44)
u32 cd_archive_entry_size(u16 slot, u16 entry)
{
    u16 *offsets = cd_archives[slot].sector_offsets;
    u32 start = offsets[entry];
    u32 end = offsets[entry + 1];

    return (end - start) << KF_CD_SECTOR_SHIFT;
}

/* Error hook left empty in retail. */
ADDRESS(0x800181a8, 0x8)
void cd_report_error(s32 code)
{
}

/* Reads SECTOR_COUNT sectors synchronously, with the request queue idle and
 * its CD events disabled. */
ADDRESS(0x800181b0, 0x160)
void cd_read_sectors(CdlLOC *location, u_long *destination, s32 sector_count)
{
    s32 attempt = 0;
    b32 failed = KF_TRUE;
    s32 result;

    cd_request_wait_idle();
    DisableEvent(cd_state.error_event);
    DisableEvent(cd_state.complete_event);
    DisableEvent(cd_state.data_ready_event);
    CdPause();
    for (; attempt < CD_READ_ATTEMPTS; attempt++) {
        CdControl(CdlSetloc, (u_char *)location, NULL);
        CdRead(sector_count, destination, CdlModeSpeed);
        while ((result = CdReadSync(CD_READ_SYNC_POLL, NULL)) > 0) {
        }
        CdPause();
        if (result == 0) {
            failed = KF_FALSE;
            break;
        }
    }
    EnableEvent(cd_state.error_event);
    EnableEvent(cd_state.complete_event);
    EnableEvent(cd_state.data_ready_event);
    if (failed == KF_TRUE) {
        cd_report_error(KF_CD_ERROR_READ);
    }
}

ADDRESS(0x80018310, 0x58)
u8 *cd_extent_load(KfCdExtent *extent)
{
    s32 size = (extent->size + KF_CD_SECTOR_BYTES - 1) & -KF_CD_SECTOR_BYTES;
    u8 *data = memory_allocate(size);

    cd_read_sectors(&extent->pos, (u_long *)data, size >> KF_CD_SECTOR_SHIFT);
    return data;
}

/* Reads SIZE bytes of the extent, or all of it when SIZE is zero. */
ADDRESS(0x80018368, 0x38)
void cd_extent_read_into(u_long *destination, KfCdExtent *extent, u32 size)
{
    if (size == 0) {
        size = extent->size;
    }
    cd_read_sectors(&extent->pos, destination,
        (size + KF_CD_SECTOR_BYTES - 1) >> KF_CD_SECTOR_SHIFT);
}

/* Reads archive entry ENTRY until its checksum holds. */
ADDRESS(0x800183a0, 0x94)
void cd_archive_read(u16 slot, u16 entry, u_long *destination)
{
    KfCdArchive *archive = &cd_archives[slot];
    u16 *offsets = archive->sector_offsets;
    s32 sector_count = offsets[entry + 1] - offsets[entry];
    CdlLOC location;

    cd_location_add(&archive->extent.pos, offsets[entry], &location);
    do {
        cd_read_sectors(&location, destination, sector_count);
    } while (cd_sectors_corrupt((u32 *)destination, sector_count));
}

/* Loads \CD\<name>;1 into a fresh allocation. */
ADDRESS(0x80018434, 0xb4)
u8 *cd_file_load(const char *name)
{
    char path[CD_PATH_BYTES];
    CdlFILE file;
    u32 size;
    u8 *data;

    memcpy((void *)path, (const void *)cd_path_prefix, sizeof cd_path_prefix);
    strcat(path, name);
    strcat(path, cd_version_suffix);
    cd_request_wait_idle();
    if (CdSearchFile(&file, path) == NULL) {
        cd_report_error(KF_CD_ERROR_SEARCH);
    }
    size = (file.size + KF_CD_SECTOR_BYTES - 1) & -KF_CD_SECTOR_BYTES;
    data = memory_allocate(size);
    cd_read_sectors(&file.pos, (u_long *)data, size >> KF_CD_SECTOR_SHIFT);
    return data;
}

/* Loads SIZE bytes of \CD\<name>;1, or all of it when SIZE is zero. */
ADDRESS(0x800184e8, 0xb8)
s32 cd_file_load_into(u_long *destination, const char *name, u32 size)
{
    char path[CD_PATH_BYTES];
    CdlFILE file;
    /* Unused: the OPEN/END cd_file_load_into seeks back to this location
     * itself; this copy reads through cd_read_sectors but keeps the local,
     * and retail keeps its frame slot. */
    CdlLOC start;

    memcpy((void *)path, (const void *)cd_path_prefix, sizeof cd_path_prefix);
    strcat(path, name);
    strcat(path, cd_version_suffix);
    cd_request_wait_idle();
    if (CdSearchFile(&file, path) == NULL) {
        cd_report_error(KF_CD_ERROR_SEARCH);
    }
    if (size == 0) {
        size = file.size;
    }
    cd_read_sectors(&file.pos, destination,
        (size + KF_CD_SECTOR_BYTES - 1) >> KF_CD_SECTOR_SHIFT);
    return 0;
}

/* Opens \CD\<name>;1 as archive SLOT: keeps its extent and copies the
 * sector-offset table out of sector 0. */
ADDRESS(0x800185a0, 0xf0)
void cd_archive_open(u16 slot, const char *name)
{
    u16 table[(KF_CD_SECTOR_BYTES + 0x40) / 2];
    CdlFILE file;
    char path[CD_PATH_BYTES];
    KfCdArchive *archive = &cd_archives[slot];

    memcpy((void *)path, (const void *)cd_path_prefix, sizeof cd_path_prefix);
    strcat(path, name);
    strcat(path, cd_version_suffix);
    if (CdSearchFile(&file, path) == NULL) {
        printf("CdSearchFile error  FileName=%s\n", path);
    }
    archive->extent.pos = file.pos;
    archive->extent.size = file.size;
    cd_extent_read_into((u_long *)table, &archive->extent, KF_CD_SECTOR_BYTES);
    archive->sector_offsets = (u16 *)memory_allocate((table[0] + 1) * 2);
    resource_copy_halfwords(archive->sector_offsets, &table[1], table[0] + 1);
}

ADDRESS(0x80018690, 0x13c)
void cd_initialize(void)
{
    KfCdRequest *request;
    s32 index;

    EnterCriticalSection();
    cd_state.vsync_count = 0;
    cd_state.vsync_event = OpenEvent(CD_EVENT_CLASS_VSYNC_COUNTER, CD_EVENT_SPEC_INTERRUPT,
        CD_EVENT_MODE_INTERRUPT, cd_vsync_handler);
    EnableEvent(cd_state.vsync_event);
    cd_state.frame_count = 0;
    request = cd_state.requests;
    for (index = 0; index < KF_CD_REQUEST_CAPACITY; index++) {
        request->kind = KF_CD_REQUEST_IDLE;
        request->phase = KF_CD_REQUEST_PHASE_SEEK;
        request++;
    }
    cd_state.tail = cd_state.current = cd_state.requests;
    cd_state.error_event = OpenEvent(CD_EVENT_CLASS_CDROM, CD_EVENT_SPEC_ERROR,
        CD_EVENT_MODE_INTERRUPT, cd_error_handler);
    cd_state.complete_event = OpenEvent(CD_EVENT_CLASS_CDROM, CD_EVENT_SPEC_COMPLETE,
        CD_EVENT_MODE_INTERRUPT, cd_complete_handler);
    cd_state.data_ready_event = OpenEvent(CD_EVENT_CLASS_CDROM, CD_EVENT_SPEC_DATA_READY,
        CD_EVENT_MODE_INTERRUPT, cd_data_ready_handler);
    EnableEvent(cd_state.error_event);
    EnableEvent(cd_state.complete_event);
    EnableEvent(cd_state.data_ready_event);
    ExitCriticalSection();
}

ADDRESS(0x800187cc, 0x68)
void cd_close_events(void)
{
    EnterCriticalSection();
    CloseEvent(cd_state.vsync_event);
    CloseEvent(cd_state.error_event);
    CloseEvent(cd_state.complete_event);
    CloseEvent(cd_state.data_ready_event);
    ExitCriticalSection();
}
