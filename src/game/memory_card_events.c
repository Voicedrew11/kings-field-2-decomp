#include <kf/lib/address.h>
#include <kf/lib/bool.h>
#include <kf/lib/null.h>
#include <kf/game/card.h>
#include <kf/game/player.h>
#include <psyq/convert.h>
#include <psyq/kernel.h>
#include <psyq/libc.h>
#include <psyq/pad.h>
#include <psyq/sdk.h>
#include <sys/fcntl.h>

RODATA(0x80011104, 0x22)

DATA(0x80067564, 0x10, ".data")
char memory_card_file_prefix[16] = "BISLPS-00069";

enum {
    CARD_FILENAME_PREFIX_LENGTH = 12,
    CARD_PATH_DEVICE_PREFIX_LENGTH = 5,
    CARD_PATH_SLOT_DIGIT_OFFSET = CARD_PATH_DEVICE_PREFIX_LENGTH + CARD_FILENAME_PREFIX_LENGTH,
    CARD_USED_BYTES_LIMIT_FOR_NEW_FILE =
        KF_CARD_DIRECTORY_CAPACITY * (KF_CARD_BLOCK_BYTES / KF_CARD_FILE_BLOCKS) -
        KF_CARD_BLOCK_BYTES,
    CARD_SHIFT_JIS_DIGIT_LEAD = 0x82,
    CARD_SHIFT_JIS_ZERO_TRAIL = 0x4f,
    CARD_SHIFT_JIS_SPACE_LE = 0x4081,
    CARD_TITLE_SLOT_DIGIT_OFFSET = 30,
    CARD_TITLE_EXPERIENCE_FIRST_BYTE = 0x28,
    CARD_TITLE_EXPERIENCE_LAST_PAIR = 25,
    CARD_TITLE_EXPERIENCE_DIGITS = 6,
    CARD_TITLE_LEVEL_FIRST_BYTE = 0x3a,
    CARD_TITLE_LEVEL_LAST_PAIR = 30,
    CARD_TITLE_LEVEL_DIGITS = 2,
    CARD_ICON_VRAM_X = 800,
    CARD_ICON_VRAM_Y = 240,
    CARD_ICON_VRAM_WIDTH = 4,
    CARD_ICON_VRAM_HEIGHT = 16
};

DATA(0x80067574, 0x120, ".data")
KfCardAssets memory_card_assets = {
    "\202j\202h\202m\202f\201f\202r\201@\202e"
    "\202h\202d\202k\202c\201@\202Q\201|\201@"
    "\201@\202d\202w\202o\201@\201@\201@\201@"
    "\201@\201@\201@\202k\202u\201@\201@",
    {
        {0x0000, 0x2942, 0x2d63, 0x35a4, 0x39c5, 0x4206, 0x4a47, 0x4e68,
         0x56a9, 0x5eea, 0x1ce0, 0x0012, 0x0015, 0x0857, 0x211a, 0x4400},
        {0x0000, 0x431c, 0x4b5d, 0x579f, 0x4f2f, 0x5771, 0x63f4, 0x7ab5,
         0x7b18, 0x7b7b, 0x7355, 0x7b98, 0x7bbb, 0x3af7, 0x435a, 0x5968},
        {0x0000, 0x0dc3, 0x1a05, 0x2648, 0x328a, 0x3ecd, 0x4b10, 0x0018,
         0x14bb, 0x295f, 0x00c0, 0x7fda, 0x7fdc, 0x3f18, 0x4b7b, 0x57de},
        {0x0000, 0x00b2, 0x0d15, 0x1d99, 0x2e1d, 0x3ed8, 0x4b3b, 0x1665,
         0x1ee7, 0x2769, 0x7def, 0x7ab3, 0x7314, 0x0a12, 0x390a, 0x4400},
        {0x0000, 0x0142, 0x01a4, 0x0206, 0x0268, 0x02ea, 0x0011, 0x0015,
         0x0019, 0x001e, 0x00e0, 0x000b, 0x7314, 0x0a12, 0x390a, 0x4400},
        {0x0000, 0x42d7, 0x4b19, 0x575c, 0x5b7d, 0x198d, 0x1108, 0x0884,
         0x0000, 0x7bde, 0x1dae, 0x154b, 0x10e8, 0x08a5, 0x0442, 0x0000},
        {0x0000, 0x1675, 0x1a96, 0x22d8, 0x2b1a, 0x335c, 0x3b9e, 0x6000,
         0x6ca5, 0x7d4a, 0x7def, 0x7fda, 0x012b, 0x0a12, 0x390a, 0x4400}
    }
};

DATA(0x8006e5d4, 0x1, ".sdata")
u8 memory_card_loaded_slot = 0;

SDATA(0x8006e5d8, 0xb)

DATA(0x8006e94c, 0x4, ".sbss")
static long memory_card_io_end_event;
DATA(0x8006e954, 0x4, ".sbss")
static long memory_card_timeout_event;
DATA(0x8006e95c, 0x4, ".sbss")
static long memory_card_new_device_event;
DATA(0x8006e964, 0x4, ".sbss")
static long memory_card_error_event;
/* Four more private small-BSS slots that no retail code reads or writes;
   their types and roles are unresolved. */
DATA(0x8006e96c, 0x4, ".sbss")
static long memory_card_unreferenced_word_0;
DATA(0x8006e974, 0x4, ".sbss")
static long memory_card_unreferenced_word_1;
DATA(0x8006e97c, 0x4, ".sbss")
static long memory_card_unreferenced_word_2;
DATA(0x8006e984, 0x4, ".sbss")
static long memory_card_unreferenced_word_3;
DATA(0x8006e98c, 0x4, ".sbss")
static u8 *memory_card_buffer;

/* Retail reserves 0x3f00 bytes: LIBCD's ISO9660 statics start right after
   them at 0x80072a3c, so each KF_CARD_BLOCK_BYTES transfer through
   memory_card_buffer runs 0x100 bytes into that library scratch. */
DATA(0x8006eb3c, 0x3f00, ".bss")
static u8 memory_card_buffer_storage[0x3f00];
/* LIBAPI's HwCARD, EvSpIOE, EvSpTIMOUT, EvSpNEW, EvSpERROR and EvMdNOINTR,
 * which the Psy-Q 3.0 kit's headers do not define. */
#define CARD_EVENT_CLASS 0xf4000001
enum {
    CARD_EVENT_SPEC_IO_END = 0x0004,
    CARD_EVENT_SPEC_TIMEOUT = 0x0100,
    CARD_EVENT_SPEC_NEW_DEVICE = 0x2000,
    CARD_EVENT_SPEC_ERROR = 0x8000,
    CARD_EVENT_MODE_NO_INTERRUPT = 0x2000
};

enum {
    CARD_READ_MAX_RETRIES = 2,
    CARD_WRITE_MAX_RETRIES = 3
};

ADDRESS(0x80022efc, 0x30)
void input_wait_release(void)
{
    input_press_pending = KF_FALSE;
    while (PadRead(1) != 0) {
    }
}

ADDRESS(0x80022f2c, 0xe8)
void memory_card_initialize(void)
{
    memory_card_io_end_event = OpenEvent(CARD_EVENT_CLASS, CARD_EVENT_SPEC_IO_END,
        CARD_EVENT_MODE_NO_INTERRUPT, NULL);
    memory_card_timeout_event = OpenEvent(CARD_EVENT_CLASS, CARD_EVENT_SPEC_TIMEOUT,
        CARD_EVENT_MODE_NO_INTERRUPT, NULL);
    memory_card_new_device_event = OpenEvent(CARD_EVENT_CLASS, CARD_EVENT_SPEC_NEW_DEVICE,
        CARD_EVENT_MODE_NO_INTERRUPT, NULL);
    memory_card_error_event = OpenEvent(CARD_EVENT_CLASS, CARD_EVENT_SPEC_ERROR,
        CARD_EVENT_MODE_NO_INTERRUPT, NULL);
    EnableEvent(memory_card_io_end_event);
    EnableEvent(memory_card_timeout_event);
    EnableEvent(memory_card_new_device_event);
    EnableEvent(memory_card_error_event);
    memory_card_buffer = memory_card_buffer_storage;
}

ADDRESS(0x80023014, 0x60)
void memory_card_shutdown_events(void)
{
    CloseEvent(memory_card_io_end_event);
    CloseEvent(memory_card_timeout_event);
    CloseEvent(memory_card_new_device_event);
    CloseEvent(memory_card_error_event);
    StopCARD();
}

ADDRESS(0x80023074, 0x28)
void memory_card_start(void)
{
    StartCARD();
    _bu_init();
}

ADDRESS(0x8002309c, 0x28)
void memory_card_stop(void)
{
    StopCARD();
    PadInit(0);
}

ADDRESS(0x800230c4, 0xec)
s32 memory_card_probe_temporary_file(void)
{
    char path[26] = "bu00:BISLPS-00069TEMP    ";
    s32 status;
    s32 handle;

    memory_card_clear_events();
    _card_info(0);
    status = memory_card_wait_event();
    if (status != KF_CARD_EVENT_NEW_DEVICE && status != KF_CARD_EVENT_IO_END) {
        return status;
    }
    handle = open(path, FCREAT);
    close(handle);
    bios_delete(path);
    return handle == -1 ? KF_CARD_PROBE_CREATE_FAILED : KF_CARD_PROBE_OK;
}
ADDRESS(0x800231b0, 0x1dc)
b32 memory_card_scan_save_entries(struct DIRENTRY *entries, s32 *matching_count)
{
    struct DIRENTRY ordered[KF_CARD_DIRECTORY_CAPACITY];
    struct DIRENTRY *first_entry;
    s32 total_size = 0;
    char slot_digit[2] = " ";
    s32 i;
    s32 slot;

    first_entry = entries;
    memset((void *)first_entry, 0, sizeof(ordered));
    *matching_count = 0;
    if (firstfile("bu00:*", first_entry) == first_entry) {
        do {
            total_size += entries->size;
            if (strncmp(entries->name, memory_card_file_prefix, CARD_FILENAME_PREFIX_LENGTH) == 0) {
                ++*matching_count;
            }
            ++entries;
        } while (nextfile(entries) == entries);
    }

    entries = first_entry;
    memset((void *)ordered, 0, sizeof(ordered));
    for (i = 0; i < KF_CARD_DIRECTORY_CAPACITY; ++i) {
        if (strncmp(entries->name, memory_card_file_prefix, CARD_FILENAME_PREFIX_LENGTH) == 0) {
            slot_digit[0] = entries->name[CARD_FILENAME_PREFIX_LENGTH];
            slot = atoi(slot_digit) - 1;
            memcpy((void *)&ordered[slot], (const void *)entries, sizeof(*entries));
        }
        ++entries;
    }
    memcpy((void *)first_entry, (const void *)ordered, sizeof(ordered));
    return total_size > CARD_USED_BYTES_LIMIT_FOR_NEW_FILE;
}

ADDRESS(0x8002338c, 0x280)
b32 memory_card_read_slot_summary(const char *filename, s32 *experience, s32 *level,
    s32 *slot)
{
    KfCardHeader header;
    char path[40] = "bu00:";
    char slot_digit[2] = " ";
    s16 encoded;
    s32 handle;
    s32 weight;
    s32 i;

    if (strncmp(filename, memory_card_file_prefix, CARD_FILENAME_PREFIX_LENGTH) != 0) {
        return KF_TRUE;
    }
    strcat(path, filename);
    handle = open(path, FREAD);
    if (handle == -1 || read(handle, (void *)&header, sizeof(header)) != sizeof(header)) {
        return KF_TRUE;
    }
    close(handle);

    *experience = 0;
    weight = 100000;
    for (i = 0; i < CARD_TITLE_EXPERIENCE_DIGITS; ++i) {
        memcpy((void *)&encoded, (const void *)&header.title[CARD_TITLE_EXPERIENCE_FIRST_BYTE + i * 2], sizeof(encoded));
        if (encoded != CARD_SHIFT_JIS_SPACE_LE) {
            encoded = ((s32)encoded >> 8) - CARD_SHIFT_JIS_ZERO_TRAIL;
            *experience += encoded * weight;
        }
        weight /= 10;
    }

    *level = 0;
    weight = 10;
    for (i = 0; i < CARD_TITLE_LEVEL_DIGITS; ++i) {
        memcpy((void *)&encoded, (const void *)&header.title[CARD_TITLE_LEVEL_FIRST_BYTE + i * 2], sizeof(encoded));
        if (encoded != CARD_SHIFT_JIS_SPACE_LE) {
            encoded = ((s32)encoded >> 8) - CARD_SHIFT_JIS_ZERO_TRAIL;
            *level += encoded * weight;
        }
        weight /= 10;
    }

    slot_digit[0] = filename[CARD_FILENAME_PREFIX_LENGTH];
    *slot = atoi(slot_digit);
    return KF_FALSE;
}

ADDRESS(0x8002360c, 0x2c)
b32 memory_card_format(void)
{
    return format("bu00:") != 1;
}

ADDRESS(0x80023638, 0x12c)
s32 memory_card_read_slot(s32 slot)
{
    char path[40] = "bu00:";
    s32 attempt;
    s32 handle;
    s32 status;
    u8 *buffer;
    u32 checksum;

    attempt = 0;
retry:
    strcat(path, memory_card_file_prefix);
    path[CARD_PATH_SLOT_DIGIT_OFFSET] = slot + '0';
    path[CARD_PATH_SLOT_DIGIT_OFFSET + 1] = '\0';
    handle = open(path, FREAD);
    if (handle == -1 || read(handle, (void *)memory_card_buffer, KF_CARD_BLOCK_BYTES)
            != KF_CARD_BLOCK_BYTES) {
        status = KF_CARD_READ_IO_FAILURE;
    } else {
        close(handle);
        buffer = memory_card_buffer;
        checksum = memory_card_payload_byte_sum(buffer + KF_CARD_HEADER_BYTES);
        if (((KfCardHeader *)buffer)->payload_checksum == checksum) {
            card_payload_restore_game_state(memory_card_buffer + KF_CARD_HEADER_BYTES);
            memory_card_loaded_slot = slot;
            return KF_CARD_READ_OK;
        }
        status = KF_CARD_READ_CHECKSUM_FAILURE;
    }
    if (attempt < CARD_READ_MAX_RETRIES) {
        attempt++;
        goto retry;
    }
    return status;
}

ADDRESS(0x80023764, 0x55c)
s32 memory_card_write_slot(s32 slot)
{
    struct DIRENTRY entries[KF_CARD_DIRECTORY_CAPACITY];
    KfCardHeader header;
    char path[40] = "bu00:";
    b32 occupied[KF_CARD_DIRECTORY_CAPACITY];
    RECT icon_rect;
    char slot_digit[10] = " ";
    s32 matching_count;
    b32 card_full;
    b32 present;
    s32 index;
    s32 entry_slot;
    s32 handle;
    u32 checksum;
    s32 attempt = 0;
    s32 transferred;

    memset((void *)occupied, 0, sizeof(occupied));
    memset((void *)entries, 0, sizeof(entries));
    card_full = memory_card_scan_save_entries(entries, &matching_count);
    present = KF_FALSE;

    for (index = 0; index < KF_CARD_DIRECTORY_CAPACITY; index++) {
        if (strncmp(entries[index].name, memory_card_file_prefix, CARD_FILENAME_PREFIX_LENGTH) == 0) {
            slot_digit[0] = entries[index].name[CARD_FILENAME_PREFIX_LENGTH];
            entry_slot = atoi(slot_digit);
            occupied[entry_slot - 1] = KF_TRUE;
            if (entry_slot == slot) {
                present = KF_TRUE;
            }
        }
    }

    if (!present) {
        if (card_full == KF_TRUE) {
            return KF_CARD_WRITE_NO_SPACE;
        }
        for (index = 0; index < KF_CARD_DIRECTORY_CAPACITY; index++) {
            if (occupied[index] == KF_FALSE) {
                slot = index + 1;
                index = KF_CARD_DIRECTORY_CAPACITY;
            }
        }
    }

retry:
    strcat(path, memory_card_file_prefix);
    path[CARD_PATH_SLOT_DIGIT_OFFSET] = slot + '0';
    path[CARD_PATH_SLOT_DIGIT_OFFSET + 1] = '\0';
    header.magic[0] = 'S';
    header.magic[1] = 'C';
    header.icon_type = KF_CARD_ICON_TYPE_THREE_FRAMES;
    header.block_count = KF_CARD_FILE_BLOCKS;
    strcpy(header.title, memory_card_assets.title);
    memory_card_write_title_stats(&header, slot);
    /* Retail indexes the seven stored palettes directly with the one-based slot. */
    memcpy((void *)header.icon_palette, (const void *)memory_card_assets.icon_palette[slot - 1],
        sizeof(header.icon_palette));

    setRECT(&icon_rect, CARD_ICON_VRAM_X,
            CARD_ICON_VRAM_Y + slot * CARD_ICON_VRAM_HEIGHT,
            CARD_ICON_VRAM_WIDTH, CARD_ICON_VRAM_HEIGHT);
    StoreImage(&icon_rect, (u_long *)header.icon_frames[0]);
    icon_rect.x = CARD_ICON_VRAM_X + CARD_ICON_VRAM_WIDTH;
    StoreImage(&icon_rect, (u_long *)header.icon_frames[1]);
    icon_rect.x = CARD_ICON_VRAM_X + CARD_ICON_VRAM_WIDTH * 2;
    StoreImage(&icon_rect, (u_long *)header.icon_frames[2]);

    memset((void *)memory_card_buffer, 0, KF_CARD_BLOCK_BYTES);
    card_payload_capture_game_state(memory_card_buffer + KF_CARD_HEADER_BYTES);
    header.payload_checksum = memory_card_payload_byte_sum(
        memory_card_buffer + KF_CARD_HEADER_BYTES);
    checksum = header.payload_checksum;
    memcpy((void *)memory_card_buffer, (const void *)&header, sizeof(header));

    if (!present) {
        handle = open(path, FCREAT | (KF_CARD_FILE_BLOCKS << 16));
        if (handle == -1) {
            return KF_CARD_WRITE_IO_FAILURE;
        }
        close(handle);
        present = KF_TRUE;
    }
    handle = open(path, FWRITE);
    if (handle != -1) {
        transferred = write(handle, (const void *)memory_card_buffer, KF_CARD_BLOCK_BYTES);
        if (transferred == KF_CARD_BLOCK_BYTES) {
            close(handle);
            handle = open(path, FREAD);
            if (handle != -1) {
                transferred = read(handle, (void *)memory_card_buffer, KF_CARD_BLOCK_BYTES);
                if (transferred == KF_CARD_BLOCK_BYTES) {
                    close(handle);
                    if (checksum == memory_card_payload_byte_sum(
                            memory_card_buffer + KF_CARD_HEADER_BYTES)) {
                        memory_card_loaded_slot = slot;
                        return KF_CARD_WRITE_OK;
                    }
                }
            }
        }
    }
    if (attempt < CARD_WRITE_MAX_RETRIES) {
        attempt++;
        goto retry;
    }
    return KF_CARD_WRITE_IO_FAILURE;
}

ADDRESS(0x80023cc0, 0x110)
void memory_card_write_title_stats(KfCardHeader *header, s32 slot_glyph)
{
    s32 experience = player_state.experience;
    s32 level = player_state.level;
    s32 index;

    header->title[CARD_TITLE_SLOT_DIGIT_OFFSET] = CARD_SHIFT_JIS_DIGIT_LEAD;
    header->title[CARD_TITLE_SLOT_DIGIT_OFFSET + 1] = slot_glyph + CARD_SHIFT_JIS_ZERO_TRAIL;
    for (index = 0; index < CARD_TITLE_EXPERIENCE_DIGITS; index++) {
        s32 digit = experience % 10;

        header->title[(CARD_TITLE_EXPERIENCE_LAST_PAIR - index) * 2] = CARD_SHIFT_JIS_DIGIT_LEAD;
        header->title[1 + (CARD_TITLE_EXPERIENCE_LAST_PAIR - index) * 2] = digit + CARD_SHIFT_JIS_ZERO_TRAIL;
        experience /= 10;
        if (experience == 0) {
            index = CARD_TITLE_EXPERIENCE_DIGITS;
        }
    }

    for (index = 0; index < CARD_TITLE_LEVEL_DIGITS; index++) {
        s32 digit = level % 10;

        header->title[(CARD_TITLE_LEVEL_LAST_PAIR - index) * 2] = CARD_SHIFT_JIS_DIGIT_LEAD;
        header->title[1 + (CARD_TITLE_LEVEL_LAST_PAIR - index) * 2] = digit + CARD_SHIFT_JIS_ZERO_TRAIL;
        level /= 10;
        if (level == 0) {
            index = CARD_TITLE_LEVEL_DIGITS;
        }
    }
}

ADDRESS(0x80023dd0, 0x24)
u32 memory_card_payload_byte_sum(const u8 *payload)
{
    u32 sum = 0;
    s32 index;

    for (index = KF_CARD_PAYLOAD_BYTES - 1; index >= 0; index--, payload++) {
        sum += *payload;
    }
    return sum;
}

ADDRESS(0x80023df4, 0x80)
s32 memory_card_wait_event(void)
{
    for (;;) {
        if (TestEvent(memory_card_io_end_event) == 1) {
            return KF_CARD_EVENT_IO_END;
        }
        if (TestEvent(memory_card_timeout_event) == 1) {
            return KF_CARD_EVENT_TIMEOUT;
        }
        if (TestEvent(memory_card_new_device_event) == 1) {
            return KF_CARD_EVENT_NEW_DEVICE;
        }
        if (TestEvent(memory_card_error_event) == 1) {
            return KF_CARD_EVENT_ERROR;
        }
    }
}

ADDRESS(0x80023e74, 0x58)
void memory_card_clear_events(void)
{
    TestEvent(memory_card_io_end_event);
    TestEvent(memory_card_timeout_event);
    TestEvent(memory_card_new_device_event);
    TestEvent(memory_card_error_event);
}
