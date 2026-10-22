#include <kf/game/callback.h>
#include <kf/lib/null.h>
#include <kf/lib/bool.h>
#include <kf/game/card.h>
#include <kf/game/menu.h>
#include <kf/game/player.h>
#include <kf/lib/address.h>
#include <psyq/pad.h>
#include <kf/game/event_counter.h>
#include <kf/game/cd.h>
#include <kf/game/graphics.h>
#include <kf/game/memory.h>
#include <kf/game/map_cell.h>
#include <kf/game/resources.h>
#include <psyq/sdk.h>
#include <psyq/libc.h>
#include <kf/game/effect.h>
#include <kf/lib/math.h>
#include <kf/lib/types.h>
#include <kf/game/audio.h>
#include <psyq/audio.h>
#include <psyq/cd.h>
#include <psyq/kernel.h>
#include <kf/game/tmd.h>
#include <kf/game/pool.h>

RODATA(0x80011098, 0x6c)

/* Used before the unit's data below. C++ has no tentative definitions, so
 * the modern view keeps this one and omits the claimed repetition. */
static POLY_FT4 *current_poly_ft4;


enum {
    /* Three glyphs and the -1 terminator of the "none" label row. */
    MENU_NONE_OPTION_GLYPHS = 4,
    MENU_STATUS_RELIEF_CAP = 64,
    MENU_LOCATION_REGION_PLACE = 100000,
    MENU_LOCATION_LAYER_PLACE = 10000,
    MENU_LOCATION_CELL_X_PLACE = 100,
    MENU_MAP_ITEM_FIRST = 67,
    MENU_MAGIC_ACTION_FIRST = 14,
    MENU_MAGIC_ACTION_LAST = 19,
    MENU_MAGIC_ACTION_VISIBLE_ROWS = MENU_MAGIC_ACTION_LAST - MENU_MAGIC_ACTION_FIRST + 1,
    MENU_MAP_ARCHIVE_FIRST_ENTRY = 480,
    MENU_MAP_ARCHIVE_ENTRIES_PER_ITEM = 8,
    MENU_MAP_FACING_TILE_COUNT = 8,
    MENU_MAP_FACING_TILE_SHIFT = 9,
    MENU_MAP_FACING_TILE_HALF_ANGLE = 1 << (MENU_MAP_FACING_TILE_SHIFT - 1),
    MENU_MAP_FACING_TILE_U_STRIDE = 16,
    MENU_EFFECT_ITEM_COUNT = KF_ENUM_ENCODE(s32, KF_ITEM_MIXED_RESTORE) -
        KF_ENUM_ENCODE(s32, KF_ITEM_RELIEVE_AILMENTS) + 1
};

typedef char kf_card_directory_entry_size[sizeof(struct DIRENTRY) == 40 ? 1 : -1];

enum {
    CARD_MENU_ROW_CAPACITY = KF_CARD_DIRECTORY_CAPACITY / KF_CARD_FILE_BLOCKS + 1,
    CARD_MENU_VISIBLE_ROWS = 6,
    CARD_MENU_LIST_Y = 0x83,
    CARD_MENU_NO_PREVIEW_ITEM = 0xff,
    CARD_MENU_NO_LEVEL = KF_MENU_LIST_NO_BYTE,
    CARD_MENU_NEW_SLOT = 0xff
};


/* The templates copied here use four, five, or six 16-bit glyph codes. */
static inline void menu_copy_prefix8(s16 *destination, const s16 *source)
{
    memcpy((void *)destination, (const void *)source, 8);
}

static inline void menu_copy_prefix10(s16 *destination, const s16 *source)
{
    memcpy((void *)destination, (const void *)source, 10);
}

static inline void menu_copy_prefix12(s16 *destination, const s16 *source)
{
    memcpy((void *)destination, (const void *)source, 12);
}
/* Unreferenced empty function; its original role and TU owner are unresolved. */
ADDRESS(0x80018834, 0x8)
void menu_unused_stub(void)
{
}

ADDRESS(0x8001883c, 0x644)
s32 menu_run_root_controller(void)
{
    s32 cursor = 0;
    b32 confirmed = KF_FALSE;
    s32 result = KF_MENU_RESULT_PENDING;
    s32 selection = KF_MENU_SELECTION_NONE;
    s32 choice_result;
    s32 frame;
    u32 buttons;

    menu_enter_display_state(1);
    for (frame = 0; frame < 2; frame++) {
        menu_frame_begin();
        menu_draw_player_status();
        menu_draw_window(KF_MENU_WINDOW_ROOT, KF_MENU_ROOT_WINDOW_ROWS, cursor, confirmed);
        menu_present_frame();
    }
    menu_play_sound_cue(KF_MENU_SOUND_CURSOR);
    input_wait_release();

    for (;;) {
        if (selection != KF_MENU_SELECTION_NONE || result != KF_MENU_RESULT_PENDING) {
            input_wait_release();
        }

        switch (selection) {
        case KF_MENU_ROOT_USE_ITEM:
            choice_result = menu_item_selection_controller();
            result = choice_result;
            if (choice_result == KF_MENU_RESULT_CANCELLED) {
                result = KF_MENU_RESULT_PENDING;
            }
            break;
        case KF_MENU_ROOT_USE_MAGIC:
            choice_result = menu_choose_magic_action();
            result = choice_result;
            if (choice_result == KF_MENU_RESULT_CANCELLED) {
                result = KF_MENU_RESULT_PENDING;
            }
            break;
        case KF_MENU_ROOT_EQUIPMENT:
            menu_equipment_list_controller();
            break;
        case KF_MENU_ROOT_COMBAT_ATTRIBUTES:
            menu_show_combat_attributes();
            break;
        case KF_MENU_ROOT_DROP_ITEM:
            menu_item_use_controller();
            break;
        case KF_MENU_ROOT_SYSTEM:
            choice_result = menu_run_card_choice();
            result = choice_result;
            if (choice_result == KF_MENU_RESULT_CANCELLED) {
                result = KF_MENU_RESULT_PENDING;
            }
            break;
        case KF_MENU_ROOT_OPTIONS:
            menu_options_controller();
            break;
        }

        if (result != KF_MENU_RESULT_PENDING) {
            break;
        }

        cursor = menu_poll_choice_input(cursor, KF_MENU_ROOT_CANCEL_ROW,
            &selection, &confirmed, &result);
        buttons = PadRead(1);
        for (frame = 0; frame < 2; frame++) {
            menu_frame_begin();
            if ((buttons & PADR1) != 0 && (buttons & PADL1) != 0) {
                menu_draw_location_number();
            }
            menu_draw_player_status();
            menu_draw_window(KF_MENU_WINDOW_ROOT, KF_MENU_ROOT_WINDOW_ROWS, cursor, confirmed);
            menu_present_frame();
        }
    }

    if (result == KF_MENU_RESULT_CANCELLED) {
        menu_play_sound_cue(KF_MENU_SOUND_NONE);
    }
    if (result == KF_MENU_RESULT_GAME_LOADED) {
        menu_exit_display_state(KF_TRUE);
    } else {
        menu_exit_display_state(KF_FALSE);
    }
    if (result != KF_MENU_RESULT_CANCELLED &&
        result != KF_MENU_RESULT_GAME_LOADED &&
        (result & KF_MENU_MAGIC_ACTION_TAG) != 0) {
        player_select_magic_action(KF_ENUM_DECODE(KF_ENUM_PROMOTED(KfEffectKind),
                                                  result & KF_MENU_MAGIC_ACTION_ID_MASK));
        result = KF_MENU_RESULT_CANCELLED;
    }
    return result;
}

/* TODO(us-port): US-only function, not in the JP build; not reconstructed. */
ADDRESS(0x80018e80, 0x2ac)
void func_80018e80(void)
{
}

ADDRESS(0x8001912c, 0xd8)
void menu_draw_location_number(void)
{
    KfMenuGlyphString row;
    s32 camera_x;
    s32 camera_z;
    s32 grid_z;
    s32 prefix;
    u16 map_layer;
    s32 value;

    row.position.x = 252;
    row.position.y = 205;
    camera_x = player_state.camera_position.vx;
    camera_z = player_state.camera_position.vz;
    map_layer = player_state.map_layer_index;
    grid_z = camera_z >> KF_MAP_CELL_POSITION_SHIFT;
    prefix = resource_state.current_map_region_id * MENU_LOCATION_REGION_PLACE
           + map_layer * MENU_LOCATION_LAYER_PLACE
           + (camera_x >> KF_MAP_CELL_POSITION_SHIFT) * MENU_LOCATION_CELL_X_PLACE;
    value = prefix + grid_z;
    menu_format_number(value, 6, KF_FORMAT_PAD_ZEROES,
        KF_MENU_FORMAT_STYLE_PLAIN, row.glyphs.codes);
    menu_draw_number(&menu_sprite_defs[KF_MENU_SPRITE_NUMBER_ATLAS], &row);
}

ADDRESS(0x80019204, 0x240)
s32 menu_item_selection_controller(void)
{
    KfItemMenuList menu;
    KfMenuGlyphRow rows[54];
    u8 values[56];
    u8 indices[56];
    b32 confirmed = KF_FALSE;
    s32 result = KF_MENU_RESULT_PENDING;
    s32 count;
    s32 frame;
    u8 selected_item;

    count = menu_collect_masked_item_rows(game_counter_bytes, rows, values, indices,
                                          MENU_MAP_ITEM_FIRST, KF_ITEM_ID_LAST);
    menu_list_init(&menu.list, KF_MENU_WINDOW_ROOT, KF_MENU_ROOT_USE_ITEM);
    menu.list.entry_count = count;
    menu.rows = rows;
    menu.values = values;
    menu.list.glyphs_per_entry = KF_MENU_GLYPHS_PER_ROW;
    if (menu.list.entry_count != 0
        && menu_load_item_model(indices[menu.list.selected_index]) != 0) {
        return KF_MENU_RESULT_CANCELLED;
        }

    for (;;) {
        if (confirmed || result != KF_MENU_RESULT_PENDING) {
            input_wait_release();
        }

        if (confirmed == KF_TRUE) {
            result = menu_preview_choice(&menu.list, KF_MENU_CONFIRM_USE, KF_MENU_LIST_USE_ITEM, selected_item);
            if (result == KF_MENU_RESULT_CANCELLED) {
                result = KF_MENU_RESULT_PENDING;
            } else {
                result = selected_item;
            }
        }

        if (result != KF_MENU_RESULT_PENDING) {
            break;
        }

        menu_update_list_input(&menu.list, indices, &confirmed, &result);
        selected_item = indices[menu.list.selected_index];
        if (confirmed == KF_TRUE) {
            menu_play_sound_cue(KF_MENU_SOUND_CONFIRM);
            if (selected_item == MENU_MAP_ITEM_FIRST
                || selected_item == MENU_MAP_ITEM_FIRST + 1
                || selected_item == MENU_MAP_ITEM_FIRST + 2) {
                menu_release_item_model();
                menu_show_map_preview(selected_item);
                menu_play_sound_cue(KF_MENU_SOUND_CANCEL);
                if (menu_load_item_model(selected_item) != 0) {
                    return KF_MENU_RESULT_CANCELLED;
                }
                confirmed = KF_FALSE;
            }
        }
        for (frame = 0; frame < 2; frame++) {
            menu_frame_begin();
            if (menu.list.entry_count != 0) {
                menu_update_item_preview(selected_item);
            }
            menu_render_list(&menu.list, KF_MENU_LIST_USE_ITEM);
            menu_present_frame();
        }
    }

    menu_release_item_model();
    if ((u32)(result - KF_ENUM_ENCODE(s32, KF_ITEM_RELIEVE_AILMENTS)) < MENU_EFFECT_ITEM_COUNT) {
        menu_apply_item_effect(KF_ENUM_DECODE(KF_ENUM_PROMOTED(KfObjectId), result));
    }
    return result;
}

ADDRESS(0x80019444, 0xe4)
s32 menu_collect_masked_item_rows(const u8 *mask, KfMenuGlyphRow *rows,
    u8 *values, u8 *indices, s32 first, s32 last)
{
    const KfMenuGlyphRow *glyph = &menu_glyph_rows[first];
    const u8 *entry = mask + first;
    s32 index;
    s32 count = 0;

    last++;
    index = first;
    while (index < last) {
        if (*entry != 0) {
            *rows++ = *glyph;
            *values = *entry;
            *indices = index;
            values++;
            indices++;
            count++;
        }
        entry++;
        glyph++;
        index++;
    }
    return count;
}

ADDRESS(0x80019528, 0x1a0)
s32 menu_collect_available_item_rows(const u8 *mask, KfMenuGlyphRow *rows,
    u8 *values, u8 *indices, s32 first, s32 last)
{
    const KfMenuGlyphRow *glyph = &menu_glyph_rows[first];
    const u8 *entry = mask + first;
    s32 index;
    s32 count = 0;

    last++;
    index = first;
    while (index < last) {
        if (*entry != 0) {
            *values = *entry;
            if (index == KF_ENUM_ENCODE(u8, player_state.equipped_weapon_id)
                || index == KF_ENUM_ENCODE(u8, player_state.equipped_head_id)
                || index == KF_ENUM_ENCODE(u8, player_state.equipped_body_id)
                || index == KF_ENUM_ENCODE(u8, player_state.equipped_arm_id)
                || index == KF_ENUM_ENCODE(u8, player_state.equipped_leg_id)
                || index == KF_ENUM_ENCODE(u8, player_state.equipped_shield_id)
                || index == KF_ENUM_ENCODE(u8, player_state.equipped_accessory_id)
                || index == KF_ENUM_ENCODE(u8, player_state.equipped_extra_id)
                || index == KF_ENUM_ENCODE(u8, player_state.secondary_item_shortcut_id)) {
                --*values;
                }
            if (*values != 0) {
                *rows = *glyph;
                *indices = index;
                rows++;
                values++;
                indices++;
                count++;
            }
        }
        entry++;
        glyph++;
        index++;
    }
    return count;
}

/* TODO(us-port): US-only function, not in the JP build; not reconstructed. */
ADDRESS(0x800196c8, 0x10c)
void func_800196c8(void)
{
}

ADDRESS(0x800197d4, 0x2b4)
void menu_apply_item_effect(KF_ENUM_PARAM(KfObjectId, s32) item_id)
{
    u16 current_hp;

    if (game_counter_bytes[KF_ENUM_ENCODE(s32, item_id)] == 0) {
        return;
    }

    if (item_id == KF_ITEM_RELIEVE_AILMENTS) {
        if (player_state.paralysis_timer > 0) {
            player_state.paralysis_timer = 0;
        }
        if (player_state.slow_timer > MENU_STATUS_RELIEF_CAP) {
            player_state.slow_timer = MENU_STATUS_RELIEF_CAP;
        }
        player_cap_curse_strength();
        player_cap_darkness_phase();
    } else if (item_id == KF_ITEM_RESTORE_MP_40) {
        player_state.vitals.current_mp += 40;
    } else if (item_id == KF_ITEM_RAISE_BASE_MAGIC) {
        player_state.base_magic++;
        player_recalculate_combat_stats();
    } else if (item_id == KF_ITEM_RESTORE_HP_40) {
        player_state.vitals.current_hp += 40;
    } else if (item_id == KF_ITEM_CURE_POISON_RESTORE_HP_15) {
        current_hp = player_state.vitals.current_hp;
        player_state.poison_timer = 0;
        player_state.vitals.current_hp = current_hp + 15;
    } else if (item_id == KF_ITEM_FULL_RESTORE) {
        player_state.vitals.current_hp = player_state.vitals.maximum_hp;
        player_state.vitals.current_mp = player_state.vitals.maximum_mp;
        player_clear_and_cap_status_effects();
    } else if (item_id == KF_ITEM_RESTORE_HP_100) {
        player_state.vitals.current_hp += 100;
    } else if (item_id == KF_ITEM_RESTORE_MP_50) {
        player_state.vitals.current_mp += 50;
    }

    if (item_id == KF_ITEM_CLEAR_AILMENTS) {
        player_clear_and_cap_status_effects();
    } else if (item_id == KF_ITEM_MIXED_RESTORE) {
        player_state.vitals.current_hp += 100;
        player_state.vitals.current_mp += 50;
        player_clear_and_cap_status_effects();
    }

    if (player_state.vitals.current_hp > player_state.vitals.maximum_hp) {
        player_state.vitals.current_hp = player_state.vitals.maximum_hp;
    }
    if (player_state.vitals.current_mp > player_state.vitals.maximum_mp) {
        player_state.vitals.current_mp = player_state.vitals.maximum_mp;
    }

    game_counter_bytes[KF_ENUM_ENCODE(s32, item_id)]--;
    if ((u32)(KF_ENUM_ENCODE(s32, item_id) - KF_ENUM_ENCODE(s32, KF_ITEM_RESTORE_HP_100)) < 2 ||
        (u32)(KF_ENUM_ENCODE(s32, item_id) - KF_ENUM_ENCODE(s32, KF_ITEM_CLEAR_AILMENTS)) < 2) {
        game_counter_bytes[KF_ENUM_ENCODE(u8, KF_OBJECT_82)]++;
        }
    menu_play_sound_cue(KF_MENU_SOUND_ITEM_USED);
}

ADDRESS(0x80019a88, 0x6c)
void player_clear_and_cap_status_effects(void)
{
    if (player_state.paralysis_timer > 0) {
        player_state.paralysis_timer = 0;
    }
    if (player_state.slow_timer > MENU_STATUS_RELIEF_CAP) {
        player_state.slow_timer = MENU_STATUS_RELIEF_CAP;
    }
    player_state.poison_timer = 0;
    player_cap_curse_strength();
    player_cap_darkness_phase();
}

ADDRESS(0x80019af4, 0x30)
void player_cap_darkness_phase(void)
{
    if (player_state.darkness_phase > MENU_STATUS_RELIEF_CAP) {
        player_state.darkness_phase = MENU_STATUS_RELIEF_CAP;
        player_state.darkness_phase_limit = 0;
    }
}

ADDRESS(0x80019b24, 0x30)
void player_cap_curse_strength(void)
{
    if (player_state.curse_strength > MENU_STATUS_RELIEF_CAP) {
        player_state.curse_strength = MENU_STATUS_RELIEF_CAP;
        player_state.curse_phase_limit = 0;
    }
}

ADDRESS(0x80019b54, 0x4f8)
void menu_show_map_preview(s32 menu_code)
{
    /* One index holds the archive entry, then the facing tile. */
    u32 index;
    u32 map_index;
    u32 map_offset;
    u8 *image;
    s32 frame;
    s32 u0;

    map_index = (menu_code - MENU_MAP_ITEM_FIRST) & 0xff;
    map_offset = resource_state.current_map_region_id + MENU_MAP_ARCHIVE_FIRST_ENTRY;
    index = map_index * MENU_MAP_ARCHIVE_ENTRIES_PER_ITEM + map_offset;
    image = memory_allocate(cd_archive_entry_extent(KF_RESOURCE_ARCHIVE_ITEM, index, NULL));
    cd_archive_read(KF_RESOURCE_ARCHIVE_ITEM, index, (u_long *)image);
    tim_upload_images(image);

    for (frame = 0; frame < 2; frame++) {
        menu_frame_begin();

        primitive_buffer_begin_poly_ft4();
        setRGB0(current_poly_ft4, 0x7f, 0x7f, 0x7f);
        SetSemiTrans((void *)current_poly_ft4, 1);
        current_poly_ft4->tpage = 0x1f;
        current_poly_ft4->clut = 0x7fe4;
        setXYWH(current_poly_ft4, 0x3c, 0x14, 200, 200);
        setUVWH(current_poly_ft4, 0, 0, 200, 200);
        primitive_buffer_commit_poly_ft4(KF_MENU_CONTENT_OT_DEPTH);

        primitive_buffer_begin_poly_ft4();
        setRGB0(current_poly_ft4, 0x7f, 0x7f, 0x7f);
        SetSemiTrans((void *)current_poly_ft4, 1);
        current_poly_ft4->tpage = 0x1c;
        current_poly_ft4->clut = 0x7d25;
        setXYWH(current_poly_ft4,
            player_state.camera_position.vx / 819 + 52,
            212 - player_state.camera_position.vz / 819,
            15, 15);

        index = ((player_state.camera_rotation.angles[1] & KF_ANGLE_WRAP_MASK)
                       + MENU_MAP_FACING_TILE_HALF_ANGLE) >> MENU_MAP_FACING_TILE_SHIFT;
        if (index == MENU_MAP_FACING_TILE_COUNT) {
            index = 0;
        }
        u0 = index * MENU_MAP_FACING_TILE_U_STRIDE
             - MENU_MAP_FACING_TILE_COUNT * MENU_MAP_FACING_TILE_U_STRIDE;
        setUVWH(current_poly_ft4, u0, 0x90, 15, 15);
        /* The facing marker sorts just above the map image. */
        primitive_buffer_commit_poly_ft4(KF_MENU_CONTENT_OT_DEPTH - 1);

        menu_draw_nine_slice_panel(0x36, 0xe, 0xd4, 0xd4, 2, 2);
        menu_present_frame();
    }

    input_wait_release();
    while (PadRead(1) == 0) {}
    input_wait_release();
    memory_free(image);
}

ADDRESS(0x8001a04c, 0x19c)
s32 menu_choose_magic_action(void)
{
    KfMagicMenuList menu;
    KfMenuGlyphRow rows[20];
    s32 values[20];
    u8 indices[20];
    b32 confirmed = KF_FALSE;
    s32 result = KF_MENU_RESULT_PENDING;
    s32 count;
    s32 frame;

    count = menu_collect_available_magic_rows(effect_state.magic_records, rows, values, indices,
                                              MENU_MAGIC_ACTION_FIRST, MENU_MAGIC_ACTION_LAST);
    menu_list_init(&menu.list, KF_MENU_WINDOW_ROOT, KF_MENU_ROOT_USE_MAGIC);
    menu.list.entry_count = count;
    menu.list.visible_rows = MENU_MAGIC_ACTION_VISIBLE_ROWS;
    menu.rows = rows;
    menu.values = values;
    menu.list.list_y = 0x83;
    menu.list.glyphs_per_entry = KF_MENU_GLYPHS_PER_ROW;

    for (;;) {
        if (confirmed || result != KF_MENU_RESULT_PENDING) {
            input_wait_release();
        }

        if (confirmed == KF_TRUE) {
            result = menu_preview_choice(&menu.list, KF_MENU_CONFIRM_USE, KF_MENU_LIST_USE_MAGIC, 0xff);
            if (result == KF_MENU_RESULT_CANCELLED) {
                result = KF_MENU_RESULT_PENDING;
            } else {
                result = indices[menu.list.selected_index];
            }
        }

        if (result != KF_MENU_RESULT_PENDING) {
            break;
        }

        menu_update_list_input(&menu.list, NULL, &confirmed, &result);
        if (confirmed == KF_TRUE) {
            menu_play_sound_cue(KF_MENU_SOUND_CONFIRM);
        }
        for (frame = 0; frame < 2; frame++) {
            menu_frame_begin();
            menu_render_list(&menu.list, KF_MENU_LIST_USE_MAGIC);
            menu_present_frame();
        }
    }

    if (result != KF_MENU_RESULT_CANCELLED) {
        result |= KF_MENU_MAGIC_ACTION_TAG;
    }
    return result;
}

ADDRESS(0x8001a1e8, 0xf4)
s32 menu_collect_available_magic_rows(const KfMagicRecord *records,
    KfMenuGlyphRow *rows, s32 *values, u8 *indices, s32 first, s32 last)
{
    const KfMenuGlyphRow *glyph = &menu_glyph_rows_extra[first];
    const KfMagicRecord *entry = &records[first];
    s32 count = 0;
    s32 index;

    last++;
    index = first;
    while (index < last) {
        if (entry->menu_available == 1) {
            *rows++ = *glyph;
            *values = entry->mp_cost;
            *indices = index;
            values++;
            indices++;
            count++;
        }
        entry++;
        glyph++;
        index++;
    }
    return count;
}




enum {
    MENU_CATEGORY_WEAPON = 0,
    MENU_CATEGORY_PRIMARY_MAGIC = 1,
    MENU_CATEGORY_ARM = 2,
    MENU_CATEGORY_HEAD = 3,
    MENU_CATEGORY_BODY = 4,
    MENU_CATEGORY_LEG = 5,
    MENU_CATEGORY_SHIELD = 6,
    MENU_CATEGORY_ACCESSORY = 7,
    MENU_CATEGORY_EXTRA = 8,
    MENU_CATEGORY_SECONDARY_SHORTCUT = 9,
    MENU_CATEGORY_COUNT = 10
};

ADDRESS(0x8001a2dc, 0x208)
void menu_equipment_list_controller(void)
{
    KfMenuRenderList menu;
    KfMenuLabelSuffix initial_rows[MENU_CATEGORY_COUNT];
    KfMenuLabelSuffix current_rows[MENU_CATEGORY_COUNT];
    b32 confirmed = KF_FALSE;
    s32 result = KF_MENU_RESULT_PENDING;
    s32 frame;
    u32 choice;

    memcpy((void *)initial_rows, (const void *)menu_equipment_category_labels, sizeof initial_rows);
    menu_build_equipped_label_rows(current_rows);
    menu_list_init(&menu.list, KF_MENU_WINDOW_ROOT, KF_MENU_ROOT_EQUIPMENT);
    menu.list.entry_count = MENU_CATEGORY_COUNT;
    menu.list.visible_rows = MENU_CATEGORY_COUNT;
    menu.row_glyphs = initial_rows[0].codes;
    menu.detail_rows = current_rows;
    menu.list.list_y = 39;

    for (;;) {
        if (confirmed || result != KF_MENU_RESULT_PENDING) {
            input_wait_release();
        }

        if (confirmed == KF_TRUE) {
            choice = menu.list.selected_index;
            if (choice == MENU_CATEGORY_PRIMARY_MAGIC) {
                menu_choose_primary_magic_shortcut();
            } else if (choice == MENU_CATEGORY_SECONDARY_SHORTCUT) {
                menu_item_magic_controller();
            } else {
                menu_equipment_category_controller(choice);
            }
            menu_build_equipped_label_rows(current_rows);
        }

        if (result != KF_MENU_RESULT_PENDING) {
            break;
        }

        menu_update_list_input(&menu.list, NULL, &confirmed, &result);
        if (confirmed == KF_TRUE) {
            menu_play_sound_cue(KF_MENU_SOUND_CONFIRM);
        }
        for (frame = 0; frame < 2; frame++) {
            menu_frame_begin();
            menu_render_list(&menu.list, KF_MENU_LIST_EQUIPMENT);
            menu_present_frame();
        }
    }
}


ADDRESS(0x8001a4e4, 0x204)
void menu_build_equipped_label_rows(KfMenuLabelSuffix *rows)
{
    u8 selected[MENU_CATEGORY_COUNT];
    u8 *entry;
    s32 i;

    selected[MENU_CATEGORY_WEAPON] = KF_ENUM_ENCODE(u8, player_state.equipped_weapon_id);
    selected[MENU_CATEGORY_PRIMARY_MAGIC] = KF_ENUM_ENCODE(u8, player_state.primary_magic_shortcut_id);
    selected[MENU_CATEGORY_ARM] = KF_ENUM_ENCODE(u8, player_state.equipped_arm_id);
    selected[MENU_CATEGORY_HEAD] = KF_ENUM_ENCODE(u8, player_state.equipped_head_id);
    selected[MENU_CATEGORY_BODY] = KF_ENUM_ENCODE(u8, player_state.equipped_body_id);
    selected[MENU_CATEGORY_LEG] = KF_ENUM_ENCODE(u8, player_state.equipped_leg_id);
    selected[MENU_CATEGORY_SHIELD] = KF_ENUM_ENCODE(u8, player_state.equipped_shield_id);
    selected[MENU_CATEGORY_ACCESSORY] = KF_ENUM_ENCODE(u8, player_state.equipped_accessory_id);
    selected[MENU_CATEGORY_EXTRA] = KF_ENUM_ENCODE(u8, player_state.equipped_extra_id);
    if (player_state.secondary_magic_shortcut_id == KF_MAGIC_NONE) {
        selected[MENU_CATEGORY_SECONDARY_SHORTCUT] =
            KF_ENUM_ENCODE(u8, player_state.secondary_item_shortcut_id);
    } else {
        selected[MENU_CATEGORY_SECONDARY_SHORTCUT] =
            KF_ENUM_ENCODE(u8, player_state.secondary_magic_shortcut_id);
    }

    entry = selected;
    for (i = 0; i < MENU_CATEGORY_COUNT; rows++, i++, entry++) {
        u32 id = *entry;

        if (id != KF_EQUIPMENT_NONE) {
            if (i == MENU_CATEGORY_PRIMARY_MAGIC
                || (i == MENU_CATEGORY_SECONDARY_SHORTCUT
                    && player_state.secondary_magic_shortcut_id != KF_MAGIC_NONE)) {
                *rows = *(const KfMenuLabelSuffix *)menu_glyph_rows_extra[id].codes;
            } else {
                *rows = *(const KfMenuLabelSuffix *)menu_glyph_rows[id].codes;
            }
        } else {
            rows->codes[0] = -1;
        }
    }
}

ADDRESS(0x8001a6e8, 0x460)
void menu_equipment_category_controller(s32 category)
{
    KfItemMenuList menu;
    KfMenuGlyphRow rows[28];
    u8 values[32];
    u8 item_ids[32];
    b32 confirmed = KF_FALSE;
    s32 result = KF_MENU_RESULT_PENDING;
    s32 first;
    s32 last;
    s32 count;
    s32 frame;
    u8 selected_item;
    u8 other_slot_item_id;

    switch (category) {
    case MENU_CATEGORY_WEAPON:
        first = KF_ITEM_WEAPON_FIRST;
        last = KF_ITEM_WEAPON_LAST;
        break;
    case MENU_CATEGORY_ARM:
        first = KF_ITEM_ARM_FIRST;
        last = KF_ITEM_ARM_LAST;
        break;
    case MENU_CATEGORY_HEAD:
        first = KF_ITEM_HEAD_FIRST;
        last = KF_ITEM_HEAD_LAST;
        break;
    case MENU_CATEGORY_BODY:
        first = KF_ITEM_BODY_FIRST;
        last = KF_ITEM_BODY_LAST;
        break;
    case MENU_CATEGORY_LEG:
        first = KF_ITEM_LEG_FIRST;
        last = KF_ITEM_LEG_LAST;
        break;
    case MENU_CATEGORY_SHIELD:
        first = KF_ITEM_SHIELD_FIRST;
        last = KF_ITEM_SHIELD_LAST;
        break;
    case MENU_CATEGORY_ACCESSORY:
    case MENU_CATEGORY_EXTRA:
        other_slot_item_id = KF_ENUM_ENCODE(u8, category == MENU_CATEGORY_ACCESSORY
                                        ? player_state.equipped_extra_id
                                        : player_state.equipped_accessory_id);
        first = KF_ITEM_ACCESSORY_FIRST;
        if (other_slot_item_id != KF_EQUIPMENT_NONE) {
            game_counter_bytes[other_slot_item_id]--;
        }
        last = KF_ITEM_ACCESSORY_LAST;
        break;
    }

    count = menu_collect_masked_item_rows(game_counter_bytes, rows, values, item_ids,
        first, last);
    memcpy((void *)rows[count].codes, (const void *)menu_none_option_glyphs.codes,
        MENU_NONE_OPTION_GLYPHS * sizeof(s16));
    values[count] = KF_MENU_LIST_NO_BYTE;
    item_ids[count] = 0xff;
    count++;

    menu_list_init(&menu.list, KF_MENU_WINDOW_ROOT, KF_MENU_ROOT_EQUIPMENT);
    menu.list.entry_count = count;
    menu.rows = rows;
    menu.values = values;
    menu.list.glyphs_per_entry = KF_MENU_GLYPHS_PER_ROW;

    if (menu.list.entry_count != 0
        && menu_load_item_model(item_ids[menu.list.selected_index]) != 0) {
        return;
        }

    for (;;) {
        if (confirmed || result != KF_MENU_RESULT_PENDING) {
            input_wait_release();
        }

        if (confirmed == KF_TRUE) {
            result = menu_preview_choice(&menu.list, KF_MENU_CONFIRM_EQUIP, KF_MENU_LIST_EQUIPMENT_CATEGORY, selected_item);
            if (result == KF_MENU_RESULT_CANCELLED) {
                result = KF_MENU_RESULT_PENDING;
            } else {
                result = selected_item;
            }
        }

        if (result != KF_MENU_RESULT_PENDING) {
            break;
        }

        menu_update_list_input(&menu.list, item_ids, &confirmed, &result);
        selected_item = item_ids[menu.list.selected_index];
        if (confirmed == KF_TRUE) {
            menu_play_sound_cue(KF_MENU_SOUND_CONFIRM);
        }
        for (frame = 0; frame < 2; frame++) {
            menu_frame_begin();
            if (menu.list.entry_count != 0) {
                menu_update_item_preview(selected_item);
            }
            menu_render_list(&menu.list, KF_MENU_LIST_EQUIPMENT_CATEGORY);
            menu_present_frame();
        }
    }

    menu_release_item_model();
    if (result != KF_MENU_RESULT_CANCELLED) {
        switch (category) {
        case MENU_CATEGORY_WEAPON:
            player_equip_weapon(KF_ENUM_DECODE(KF_ENUM_PARAM(KfObjectId, u8), result));
            break;
        case MENU_CATEGORY_ARM:
            player_set_equipment_slot(KF_ENUM_DECODE(KF_ENUM_PARAM(KfObjectId, u8), result),
                                      KF_EQUIPMENT_SLOT_ARM);
            break;
        case MENU_CATEGORY_HEAD:
            player_set_equipment_slot(KF_ENUM_DECODE(KF_ENUM_PARAM(KfObjectId, u8), result),
                                      KF_EQUIPMENT_SLOT_HEAD);
            break;
        case MENU_CATEGORY_BODY:
            player_set_equipment_slot(KF_ENUM_DECODE(KF_ENUM_PARAM(KfObjectId, u8), result),
                                      KF_EQUIPMENT_SLOT_BODY);
            break;
        case MENU_CATEGORY_LEG:
            player_set_equipment_slot(KF_ENUM_DECODE(KF_ENUM_PARAM(KfObjectId, u8), result),
                                      KF_EQUIPMENT_SLOT_LEG);
            break;
        case MENU_CATEGORY_SHIELD:
            player_set_equipment_slot(KF_ENUM_DECODE(KF_ENUM_PARAM(KfObjectId, u8), result),
                                      KF_EQUIPMENT_SLOT_SHIELD);
            break;
        case MENU_CATEGORY_ACCESSORY:
            player_set_equipment_slot(KF_ENUM_DECODE(KF_ENUM_PARAM(KfObjectId, u8), result),
                                      KF_EQUIPMENT_SLOT_ACCESSORY);
            break;
        case MENU_CATEGORY_EXTRA:
            player_set_equipment_slot(KF_ENUM_DECODE(KF_ENUM_PARAM(KfObjectId, u8), result),
                                      KF_EQUIPMENT_SLOT_EXTRA);
            break;
        }
    }

    if ((category == MENU_CATEGORY_ACCESSORY || category == MENU_CATEGORY_ACCESSORY + 1) &&
        other_slot_item_id != KF_EQUIPMENT_NONE) {
        game_counter_bytes[other_slot_item_id]++;
        }
}

ADDRESS(0x8001ab48, 0x23c)
void menu_choose_primary_magic_shortcut(void)
{
    KfMagicMenuList menu;
    KfMenuGlyphRow rows[20];
    s32 values[20];
    u8 indices[20];
    b32 confirmed = KF_FALSE;
    s32 result = KF_MENU_RESULT_PENDING;
    s32 count;
    s32 frame;

    count = menu_collect_available_magic_rows(effect_state.magic_records, rows, values, indices, 0, 13);
    memcpy((void *)rows[count].codes, (const void *)menu_none_option_glyphs.codes,
        MENU_NONE_OPTION_GLYPHS * sizeof(s16));
    values[count] = KF_MENU_LIST_NO_NUMBER;
    indices[count] = KF_ENUM_ENCODE(u8, KF_MAGIC_NONE);
    count++;

    menu_list_init(&menu.list, KF_MENU_WINDOW_ROOT, KF_MENU_ROOT_EQUIPMENT);
    menu.list.entry_count = count;
    menu.list.visible_rows = 6;
    menu.rows = rows;
    menu.values = values;
    menu.list.list_y = 0x83;
    menu.list.glyphs_per_entry = KF_MENU_GLYPHS_PER_ROW;

    for (;;) {
        if (confirmed || result != KF_MENU_RESULT_PENDING) {
            input_wait_release();
        }

        if (confirmed == KF_TRUE) {
            result = menu_preview_choice(&menu.list, KF_MENU_CONFIRM_EQUIP, KF_MENU_LIST_MAGIC_SHORTCUT, 0xff);
            if (result == KF_MENU_RESULT_CANCELLED) {
                result = KF_MENU_RESULT_PENDING;
            } else {
                result = indices[menu.list.selected_index];
            }
        }

        if (result != KF_MENU_RESULT_PENDING) {
            break;
        }

        menu_update_list_input(&menu.list, NULL, &confirmed, &result);
        if (confirmed == KF_TRUE) {
            menu_play_sound_cue(KF_MENU_SOUND_CONFIRM);
        }
        for (frame = 0; frame < 2; frame++) {
            menu_frame_begin();
            menu_render_list(&menu.list, KF_MENU_LIST_MAGIC_SHORTCUT);
            menu_present_frame();
        }
    }

    if (result != KF_MENU_RESULT_CANCELLED) {
        player_set_primary_magic_shortcut_id(KF_ENUM_DECODE(KF_ENUM_PROMOTED(KfEffectKind), result));
    }
}

ADDRESS(0x8001ad84, 0x34c)
void menu_item_magic_controller(void)
{
    KfMenuRenderList menu;
    KfMenuGlyphRow rows[74];
    u8 counts[74];
    s32 numbers[74];
    u8 item_ids[74];
    u8 magic_ids[74];
    b32 confirmed = KF_FALSE;
    s32 result = KF_MENU_RESULT_PENDING;
    s32 count;
    s32 index;
    u8 selected_item;
    u8 saved_count_60;
    u8 saved_count_61;

    for (index = 0; index < KF_COUNTOF(counts); index++) {
        counts[index] = KF_MENU_LIST_NO_BYTE;
        numbers[index] = KF_MENU_LIST_NO_NUMBER;
        item_ids[index] = 0xff;
        magic_ids[index] = 0xff;
    }

    saved_count_60 = game_counter_bytes[KF_ENUM_ENCODE(u8, KF_OBJECT_96)];
    saved_count_61 = game_counter_bytes[KF_ENUM_ENCODE(u8, KF_OBJECT_97)];
    game_counter_bytes[KF_ENUM_ENCODE(u8, KF_OBJECT_96)] = 0;
    game_counter_bytes[KF_ENUM_ENCODE(u8, KF_OBJECT_97)] = 0;
    count = menu_collect_masked_item_rows(game_counter_bytes, rows, counts, item_ids, KF_ITEM_USABLE_FIRST,
        KF_ITEM_USABLE_LAST);
    game_counter_bytes[KF_ENUM_ENCODE(u8, KF_OBJECT_96)] = saved_count_60;
    game_counter_bytes[KF_ENUM_ENCODE(u8, KF_OBJECT_97)] = saved_count_61;

    count += menu_collect_available_magic_rows(effect_state.magic_records, &rows[count],
        &numbers[count], &magic_ids[count], 0, 19);
    memcpy((void *)rows[count].codes, (const void *)menu_none_option_glyphs.codes,
        MENU_NONE_OPTION_GLYPHS * sizeof(s16));
    count++;

    menu_list_init(&menu.list, KF_MENU_WINDOW_ROOT, KF_MENU_ROOT_EQUIPMENT);
    menu.list.entry_count = count;
    menu.row_glyphs = rows[0].codes;
    menu.byte_values = counts;
    menu.number_values = numbers;
    menu.list.glyphs_per_entry = KF_MENU_GLYPHS_PER_ROW;

    if (menu.list.entry_count != 0
        && menu_load_item_model(item_ids[menu.list.selected_index]) != 0) {
        return;
        }

    for (;;) {
        if (confirmed || result != KF_MENU_RESULT_PENDING) {
            input_wait_release();
        }

        if (confirmed == KF_TRUE) {
            result = menu_preview_choice(&menu.list, KF_MENU_CONFIRM_EQUIP, KF_MENU_LIST_ITEM_MAGIC, selected_item);
            if (result == KF_MENU_RESULT_CANCELLED) {
                result = KF_MENU_RESULT_PENDING;
            } else {
                result = menu.list.selected_index;
            }
        }

        if (result != KF_MENU_RESULT_PENDING) {
            break;
        }

        menu_update_list_input(&menu.list, item_ids, &confirmed, &result);
        selected_item = item_ids[menu.list.selected_index];
        if (confirmed == KF_TRUE) {
            menu_play_sound_cue(KF_MENU_SOUND_CONFIRM);
        }

        for (index = 0; index < 2; index++) {
            menu_frame_begin();
            if (menu.list.entry_count != 0) {
                menu_update_item_preview(selected_item);
            }
            menu_render_list(&menu.list, KF_MENU_LIST_ITEM_MAGIC);
            menu_present_frame();
        }
    }

    if (result != KF_MENU_RESULT_CANCELLED) {
        player_set_secondary_magic_shortcut_id(KF_ENUM_DECODE(KF_ENUM_PROMOTED(KfEffectKind),
                                                              magic_ids[result]));
        player_set_secondary_item_shortcut_id(KF_ENUM_DECODE(KF_ENUM_PARAM(KfObjectId, u8),
                                                             item_ids[result]));
    }
}


ADDRESS(0x8001b0d0, 0x88)
void menu_show_combat_attributes(void)
{
    s32 current = KF_MENU_RESULT_PENDING;
    s32 previous = KF_MENU_RESULT_PENDING;
    s32 frame;

    for (;;) {
        if (current != previous) {
            input_wait_release();
            return;
        }
        input_wait_brief_release();
        if (input_read_mark_active()) {
            menu_play_sound_cue(KF_MENU_SOUND_CANCEL);
            current = KF_MENU_RESULT_CANCELLED;
        }
        for (frame = 0; frame < 2; frame++) {
            menu_frame_begin();
            menu_draw_combat_attributes();
            menu_present_frame();
        }
    }
}

ADDRESS(0x8001b158, 0x204)
void menu_item_use_controller(void)
{
    KfItemMenuList menu;
    KfMenuGlyphRow rows[KF_ITEM_ID_COUNT];
    u8 values[KF_ITEM_ID_COUNT];
    u8 indices[KF_ITEM_ID_COUNT];
    b32 confirmed = KF_FALSE;
    s32 result = KF_MENU_RESULT_PENDING;
    s32 count;
    s32 frame;
    u8 selected_item;

    count = menu_collect_available_item_rows(game_counter_bytes, rows, values, indices, 0, KF_ITEM_ID_LAST);
    menu_list_init(&menu.list, KF_MENU_WINDOW_ROOT, KF_MENU_ROOT_DROP_ITEM);
    menu.list.entry_count = count;
    menu.rows = rows;
    menu.values = values;
    menu.list.glyphs_per_entry = KF_MENU_GLYPHS_PER_ROW;
    if (menu.list.entry_count != 0
        && menu_load_item_model(indices[menu.list.selected_index]) != 0) {
        return;
        }

    for (;;) {
        if (confirmed || result != KF_MENU_RESULT_PENDING) {
            input_wait_release();
        }

        if (confirmed == KF_TRUE) {
            result = menu_preview_choice(&menu.list, KF_MENU_CONFIRM_DROP, KF_MENU_LIST_DROP_ITEM,
                indices[menu.list.selected_index]);
            if (result == KF_MENU_RESULT_CANCELLED) {
                result = KF_MENU_RESULT_PENDING;
            } else {
                result = selected_item;
            }
        }

        if (result != KF_MENU_RESULT_PENDING) {
            break;
        }

        menu_update_list_input(&menu.list, indices, &confirmed, &result);
        selected_item = indices[menu.list.selected_index];
        if (confirmed == KF_TRUE) {
            menu_play_sound_cue(KF_MENU_SOUND_CONFIRM);
        }
        for (frame = 0; frame < 2; frame++) {
            menu_frame_begin();
            if (menu.list.entry_count != 0) {
                menu_update_item_preview(selected_item);
            }
            menu_render_list(&menu.list, KF_MENU_LIST_DROP_ITEM);
            menu_present_frame();
        }
    }

    menu_release_item_model();
    if (result != KF_MENU_RESULT_CANCELLED) {
        game_counter_bytes[result]--;
    }
}

ADDRESS(0x8001b35c, 0x198)
s32 menu_run_card_choice(void)
{
    KfMenuGlyphString labels[2];
    s32 cursor = 0;
    b32 confirmed = KF_FALSE;
    s32 result = KF_MENU_RESULT_PENDING;
    s32 selection = KF_MENU_SELECTION_NONE;
    s32 frame;
    s32 volume;

    for (;;) {
        if (selection != KF_MENU_SELECTION_NONE || result != KF_MENU_RESULT_PENDING) {
            input_wait_release();
        }

        switch (selection) {
        case KF_MENU_SYSTEM_LOAD_ROW:
            result = menu_card_load_browser();
            if (result == KF_MENU_RESULT_ACCEPTED) {
                result = KF_MENU_RESULT_GAME_LOADED;
            }
            break;
        case KF_MENU_SYSTEM_QUIT_ROW:
            result = menu_prompt_two_option();
            if (result == KF_MENU_RESULT_ACCEPTED) {
                result = KF_MENU_RESULT_QUIT_GAME;
            }
            break;
        }

        if (selection != KF_MENU_SELECTION_NONE && result == KF_MENU_RESULT_CANCELLED) {
            result = KF_MENU_RESULT_PENDING;
        }

        if (result != KF_MENU_RESULT_PENDING) {
            break;
        }

        cursor = menu_poll_choice_input(cursor, KF_MENU_SYSTEM_RETURN_ROW,
            &selection, &confirmed, &result);
        for (frame = 0; frame < 2; frame++) {
            menu_frame_begin();
            menu_draw_window(KF_MENU_WINDOW_SYSTEM, KF_MENU_SYSTEM_ROW_COUNT, cursor, confirmed);
            menu_present_frame();
        }
    }

    if (result == KF_MENU_RESULT_QUIT_GAME) {
        u32 cd_result;

        CdControl(CdlStop, NULL, (u8 *)&cd_result);
        volume = 60;
        menu_prepare_card_exit_rows(labels);
        for (;;) {
            menu_show_dialog_panel(KF_MENU_SYSTEM_QUIT_TITLE, labels, 2, 70, 87, 178, 66, 2, 0);
            if (volume > 0) {
                volume--;
                SsSeqSetVol(audio_state.sequence_id, volume, volume);
            }
        }
    }
    return result;
}

ADDRESS(0x8001b4f4, 0x2b4)
s32 menu_card_load_browser(void)
{
    struct DIRENTRY entries[KF_CARD_DIRECTORY_CAPACITY];
    KfCardMenuList menu;
    KfCardSlotGlyphRow glyph_rows[CARD_MENU_ROW_CAPACITY];
    s32 experience_values[CARD_MENU_ROW_CAPACITY];
    u8 levels[CARD_MENU_ROW_CAPACITY];
    s32 slot_ids[CARD_MENU_ROW_CAPACITY];
    KfMenuGlyphString dialog_rows[3];
    s32 matching_count;
    b32 confirmed = KF_FALSE;
    s32 result = KF_MENU_RESULT_PENDING;
    s32 count;
    s32 read_result;
    s32 frame;

    menu_prepare_card_browser_rows(dialog_rows);
    menu_show_dialog_panel(KF_MENU_SYSTEM_LOAD_TITLE, dialog_rows, 2, 70, 87, 178, 66, 2, 0);
    memory_card_start();
    memory_card_scan_save_entries(entries, &matching_count);
    count = menu_card_build_slot_rows(entries, glyph_rows[0].codes,
        experience_values, levels, slot_ids);
    menu_list_init(&menu.list, KF_MENU_WINDOW_SYSTEM, KF_MENU_SYSTEM_LOAD_ROW);
    menu.list.visible_rows = CARD_MENU_VISIBLE_ROWS;
    menu.list.list_y = CARD_MENU_LIST_Y;
    menu.list.entry_count = count;
    menu.rows = glyph_rows;
    menu.levels = levels;
    menu.experience_values = experience_values;

    for (;;) {
        if (confirmed || result != KF_MENU_RESULT_PENDING) {
            input_wait_release();
        }

        if (confirmed == KF_TRUE) {
            result = menu_preview_choice(&menu.list, KF_MENU_CONFIRM_LOAD, KF_MENU_LIST_CARD_LOAD, CARD_MENU_NO_PREVIEW_ITEM);
            if (result == KF_MENU_RESULT_CANCELLED) {
                result = KF_MENU_RESULT_PENDING;
            } else {
                result = slot_ids[menu.list.selected_index];
            }
        }
        if (result != KF_MENU_RESULT_PENDING) {
            break;
        }

        menu_update_list_input(&menu.list, NULL, &confirmed, &result);
        if (confirmed == KF_TRUE) {
            menu_play_sound_cue(KF_MENU_SOUND_CURSOR);
        }
        for (frame = 0; frame < 2; frame++) {
            menu_frame_begin();
            menu_render_list(&menu.list, KF_MENU_LIST_CARD_LOAD);
            menu_present_frame();
        }
    }

    if (result != KF_MENU_RESULT_CANCELLED) {
        menu_prepare_card_read_row(dialog_rows);
        menu_show_dialog_panel(KF_MENU_SYSTEM_LOAD_TITLE, dialog_rows, 1, 70, 87, 178, 66, 2, 0);
        read_result = memory_card_read_slot(result);
        if (read_result != KF_CARD_READ_OK) {
            menu_prepare_card_read_failure_rows(dialog_rows, read_result);
            menu_show_dialog_panel(KF_MENU_SYSTEM_LOAD_TITLE,
                dialog_rows, 3, 70, 87, 178, 81, 2, 0);
            input_wait_release();
            while (PadRead(1) == 0) {}
            input_wait_release();
            result = KF_MENU_RESULT_CANCELLED;
        } else {
            result = KF_MENU_RESULT_ACCEPTED;
        }
    }
    memory_card_stop();
    return result;
}

ADDRESS(0x8001b7a8, 0x104)
s32 menu_card_build_slot_rows(const struct DIRENTRY *card_entries, s16 *glyph_rows,
    s32 *experience_values, u8 *levels, s32 *slot_ids)
{
    s32 count = 0;
    s32 index;
    s32 experience;
    s32 level;
    s32 slot_id;

    for (index = 0; index < KF_CARD_DIRECTORY_CAPACITY; index++) {
        if (memory_card_read_slot_summary(card_entries->name,
            &experience, &level, &slot_id) == KF_FALSE) {
            *glyph_rows++ = 0x1012;
            *glyph_rows++ = 0x2d;
            *glyph_rows++ = 0xf;
            *glyph_rows++ = slot_id + 229;
            *glyph_rows = KF_MENU_TEXT_END;
            glyph_rows += 6;
            *experience_values++ = experience;
            *levels++ = level;
            *slot_ids++ = slot_id;
            count++;
        }
        ++card_entries;
    }
    return count;
}

ADDRESS(0x8001b8ac, 0x11c)
void menu_show_dialog_panel(s32 panel, const KfMenuGlyphString *rows, s32 count,
    s32 detail0, s32 detail1, s32 detail2, s32 detail3, s32 detail4,
    s32 detail5)
{
    s32 frame = 0;
    const KfMenuGlyphString *title = &menu_window_layout(KF_MENU_WINDOW_SYSTEM)->rows[panel];
    const KfMenuGlyphString *current;
    s32 row;

    do {
        menu_frame_begin();
        if (panel < KF_MENU_SYSTEM_TITLED_ROWS) {
            menu_blit_sprite_translucent(&menu_sprite_defs[KF_MENU_SPRITE_PANEL_BACKGROUND],
                &title->position);
            menu_draw_string(&menu_sprite_defs[KF_MENU_SPRITE_GLYPH_ATLAS], title);
        }
        current = rows;
        for (row = 0; row < count; row++, current++) {
            menu_draw_string(&menu_sprite_defs[KF_MENU_SPRITE_GLYPH_ATLAS], current);
        }
        menu_draw_nine_slice_panel(detail0, detail1, detail2, detail3, detail4, detail5);
        menu_present_frame();
        frame++;
    } while (frame < 2);
}

ADDRESS(0x8001b9c8, 0x1b4)
s32 menu_prompt_two_option(void)
{
    KfMenuGlyphString labels[2];
    b32 confirmed;
    s32 selected;
    s32 result;
    s32 current;
    s32 frame;
    s32 cursor_frame;

    current = 0;
    confirmed = KF_FALSE;
    result = KF_MENU_RESULT_PENDING;
    selected = KF_MENU_SELECTION_NONE;
    labels[0].position.x = 101;
    labels[0].position.y = 123;
    labels[0].glyphs.codes[0] = 89;
    labels[0].glyphs.codes[1] = 65;
    labels[0].glyphs.codes[2] = KF_MENU_TEXT_END;
    labels[1].position.x = 101;
    labels[1].position.y = 149;
    labels[1].glyphs.codes[0] = 65;
    labels[1].glyphs.codes[1] = 65;
    labels[1].glyphs.codes[2] = 67;
    labels[1].glyphs.codes[3] = KF_MENU_TEXT_END;

    for (;;) {
        if (selected != KF_MENU_SELECTION_NONE || result != KF_MENU_RESULT_PENDING) {
            input_wait_release();
        }

        switch (selected) {
        case KF_MENU_CHOICE_ACCEPT:
            result = KF_MENU_RESULT_ACCEPTED;
            break;
        case KF_MENU_CHOICE_DECLINE:
            result = KF_MENU_RESULT_CANCELLED;
            break;
        }

        if (result != KF_MENU_RESULT_PENDING) {
            break;
        }
        current = menu_poll_choice_input(current, KF_MENU_CHOICE_DECLINE,
            &selected, &confirmed, &result);
        for (frame = 0; frame < 2; frame++) {
            menu_frame_begin();
            cursor_frame = menu_cursor_animation_frame;
            menu_cursor_animation_frame = 0;
            menu_draw_window(KF_MENU_WINDOW_SYSTEM, KF_MENU_SYSTEM_ROW_COUNT,
                KF_MENU_SYSTEM_QUIT_ROW, KF_TRUE);
            menu_cursor_animation_frame = cursor_frame;
            menu_draw_two_option(&labels[0], &labels[1], current, confirmed);
            menu_present_frame();
        }
    }
    return result;
}
enum {
    KF_MENU_OPTION_EFFECTS_ROW = 0,
    KF_MENU_OPTION_MUSIC_ROW = 1,
    KF_MENU_OPTION_GAUGES_ROW = 2,
    KF_MENU_OPTION_COMPASS_ROW = 3,
    KF_MENU_OPTION_ITEM_PREVIEW_ROW = 4,
    KF_MENU_OPTION_WALKING_BOB_ROW = 5,
    KF_MENU_OPTION_COUNT = 6,
    KF_MENU_OPTION_CANCEL_ROW = KF_MENU_OPTION_COUNT
};

ADDRESS(0x8001bb7c, 0x2e4)
void menu_options_controller(void)
{
    KfPlayerOption selected[KF_MENU_OPTION_COUNT];
    KfMenuGlyphString labels[2];
    s32 choice = 0;
    s32 result = KF_MENU_RESULT_PENDING;
    s32 last_row = KF_MENU_OPTION_CANCEL_ROW;
    b32 confirmed;
    s32 frame;
    u32 buttons;

    labels[0].position.x = 180;
    labels[0].position.y = 45;
    labels[0].glyphs.codes[0] = 240;
    labels[0].glyphs.codes[1] = 241;
    labels[0].glyphs.codes[2] = KF_MENU_TEXT_END;
    labels[1].position.x = 254;
    labels[1].position.y = 45;
    labels[1].glyphs.codes[0] = 242;
    labels[1].glyphs.codes[1] = 243;
    labels[1].glyphs.codes[2] = 244;
    labels[1].glyphs.codes[3] = KF_MENU_TEXT_END;

    selected[KF_MENU_OPTION_EFFECTS_ROW] = player_state.audio_effects_enabled;
    selected[KF_MENU_OPTION_MUSIC_ROW] = player_state.audio_music_enabled;
    selected[KF_MENU_OPTION_GAUGES_ROW] = player_state.hud_gauges_enabled;
    selected[KF_MENU_OPTION_COMPASS_ROW] = player_state.compass_enabled;
    selected[KF_MENU_OPTION_ITEM_PREVIEW_ROW] = player_state.item_preview_enabled;
    selected[KF_MENU_OPTION_WALKING_BOB_ROW] = player_state.walking_bob_enabled;

    for (;;) {
        if (result != KF_MENU_RESULT_PENDING) {
            input_wait_release();
            break;
        }

        input_wait_brief_release();
        buttons = input_read_mark_active();
        confirmed = KF_FALSE;
        if (buttons & PADLup) {
            menu_cursor_animation_direction = KF_MENU_CURSOR_ANIMATION_FORWARD;
            menu_play_sound_cue(KF_MENU_SOUND_CURSOR);
            if (choice != 0) {
                choice--;
            } else {
                choice = last_row;
            }
        } else if (buttons & PADLdown) {
            menu_cursor_animation_direction = KF_MENU_CURSOR_ANIMATION_FORWARD;
            menu_play_sound_cue(KF_MENU_SOUND_CURSOR);
            if (choice != last_row) {
                choice++;
            } else {
                choice = 0;
            }
        } else if ((buttons & PADRright) || (buttons & PADLright)
            || (buttons & PADLleft)) {
            if (choice < last_row) {
                menu_play_sound_cue(KF_MENU_SOUND_CONFIRM);
                confirmed = KF_TRUE;
                selected[choice] = KF_ENUM_DECODE(KfPlayerOption, selected[choice] == KF_PLAYER_OPTION_OFF);
            } else if (buttons & PADRright) {
                menu_play_sound_cue(KF_MENU_SOUND_CONFIRM);
                result = KF_MENU_RESULT_CANCELLED;
                confirmed = KF_TRUE;
            }
        } else if (buttons & PADRdown) {
            menu_play_sound_cue(KF_MENU_SOUND_CANCEL);
            result = KF_MENU_RESULT_CANCELLED;
        }

        for (frame = 0; frame < 2; frame++) {
            menu_frame_begin();
            menu_draw_window(KF_MENU_WINDOW_OPTIONS, KF_MENU_OPTION_CANCEL_ROW + 1,
                choice, confirmed);
            menu_draw_options_rows(&labels[0], &labels[1], selected);
            menu_present_frame();
        }
    }

    player_state.audio_effects_enabled = selected[KF_MENU_OPTION_EFFECTS_ROW];
    player_state.audio_music_enabled = selected[KF_MENU_OPTION_MUSIC_ROW];
    player_state.hud_gauges_enabled = selected[KF_MENU_OPTION_GAUGES_ROW];
    player_state.compass_enabled = selected[KF_MENU_OPTION_COMPASS_ROW];
    player_state.item_preview_enabled = selected[KF_MENU_OPTION_ITEM_PREVIEW_ROW];
    player_state.walking_bob_enabled = selected[KF_MENU_OPTION_WALKING_BOB_ROW];
}

ADDRESS(0x8001be60, 0x30c)
s32 menu_card_browser(void)
{
    struct DIRENTRY entries[KF_CARD_DIRECTORY_CAPACITY];
    KfMenuGlyphString rows[4];
    s32 matching_count;
    s32 cursor = 0;
    b32 confirmed = KF_FALSE;
    s32 result = KF_MENU_RESULT_PENDING;
    s32 selection = KF_MENU_SELECTION_NONE;
    s32 status;
    s32 buttons;
    s32 frame;

    menu_enter_display_state(0);
    menu_prepare_card_browser_rows(rows);
    menu_show_dialog_panel(KF_MENU_SYSTEM_UNTITLED, rows, 2, 70, 87, 178, 66, 2, 0);
    memory_card_start();
    status = memory_card_probe_temporary_file();
    if (status != KF_CARD_PROBE_OK && status != KF_CARD_PROBE_CREATE_FAILED) {
        menu_build_card_probe_error_rows(rows);
        menu_show_dialog_panel(KF_MENU_SYSTEM_UNTITLED, rows, 3, 50, 87, 220, 66, 2, 0);
        input_wait_release();
        while (PadRead(1) == 0) {}
        input_wait_release();
        memory_card_stop();
        menu_exit_display_state(KF_FALSE);
        return KF_MENU_RESULT_CANCELLED;
    }

    status = memory_card_scan_save_entries(entries, &matching_count);
    if (matching_count == 0) {
        if (status == KF_TRUE) {
            menu_build_card_full_rows(rows);
            menu_show_dialog_panel(KF_MENU_SYSTEM_UNTITLED, rows, 4, 70, 87, 178, 96, 2, 0);
            input_wait_release();
            while (PadRead(1) == 0) {}
            input_wait_release();
        }
        memory_card_stop();
        menu_exit_display_state(KF_FALSE);
        return KF_MENU_RESULT_CANCELLED;
    }

    menu_play_sound_cue(KF_MENU_SOUND_CURSOR);
    input_wait_release();
    for (;;) {
        if (selection != KF_MENU_SELECTION_NONE) {
            input_wait_release();
        }
        switch (selection) {
        case KF_MENU_CARD_BROWSER_START_ROW:
            result = KF_MENU_RESULT_CANCELLED;
            break;
        case KF_MENU_CARD_BROWSER_LOAD_ROW:
            result = menu_card_load_slot_browser();
            if (result == KF_MENU_RESULT_CANCELLED) {
                result = KF_MENU_RESULT_PENDING;
                selection = KF_MENU_SELECTION_NONE;
            }
            break;
        }
        if (result != KF_MENU_RESULT_PENDING) {
            break;
        }

        input_wait_brief_release();
        buttons = input_read_mark_active();
        if ((buttons & PADLup) || (buttons & PADLdown)) {
            menu_cursor_animation_direction = KF_MENU_CURSOR_ANIMATION_FORWARD;
            menu_play_sound_cue(KF_MENU_SOUND_CURSOR);
            if (cursor == KF_MENU_CARD_BROWSER_START_ROW) {
                cursor = KF_MENU_CARD_BROWSER_LOAD_ROW;
            } else {
                cursor = KF_MENU_CARD_BROWSER_START_ROW;
            }
        } else if (buttons & PADRright) {
            menu_play_sound_cue(KF_MENU_SOUND_CONFIRM);
            confirmed = KF_TRUE;
            selection = cursor;
        }
        for (frame = 0; frame < 2; frame++) {
            menu_frame_begin();
            menu_draw_window(KF_MENU_WINDOW_CARD_BROWSER, KF_MENU_CARD_BROWSER_ROW_COUNT,
                cursor, confirmed);
            menu_present_frame();
        }
    }
    memory_card_stop();
    menu_exit_display_state(KF_FALSE);
    return result;
}

ADDRESS(0x8001c16c, 0x250)
s32 menu_card_load_slot_browser(void)
{
    struct DIRENTRY entries[KF_CARD_DIRECTORY_CAPACITY];
    KfCardMenuList menu;
    KfCardSlotGlyphRow glyph_rows[CARD_MENU_ROW_CAPACITY];
    s32 experience_values[CARD_MENU_ROW_CAPACITY];
    u8 levels[CARD_MENU_ROW_CAPACITY];
    s32 slot_ids[CARD_MENU_ROW_CAPACITY];
    KfMenuGlyphString dialog_rows[3];
    s32 matching_count;
    b32 confirmed = KF_FALSE;
    s32 result = KF_MENU_RESULT_PENDING;
    s32 count;
    s32 read_result;
    s32 frame;

    memory_card_scan_save_entries(entries, &matching_count);
    count = menu_card_build_slot_rows(entries, glyph_rows[0].codes,
        experience_values, levels, slot_ids);
    menu_list_init(&menu.list, KF_MENU_WINDOW_SYSTEM, KF_MENU_SYSTEM_LOAD_ROW);
    menu.list.visible_rows = CARD_MENU_VISIBLE_ROWS;
    menu.list.list_y = CARD_MENU_LIST_Y;
    menu.list.entry_count = count;
    menu.rows = glyph_rows;
    menu.levels = levels;
    menu.experience_values = experience_values;

    for (;;) {
        if (confirmed || result != KF_MENU_RESULT_PENDING) {
            input_wait_release();
        }

        if (confirmed == KF_TRUE) {
            result = menu_preview_choice(&menu.list, KF_MENU_CONFIRM_LOAD, KF_MENU_LIST_CARD_LOAD, CARD_MENU_NO_PREVIEW_ITEM);
            if (result == KF_MENU_RESULT_CANCELLED) {
                result = KF_MENU_RESULT_PENDING;
            } else {
                result = slot_ids[menu.list.selected_index];
            }
        }
        if (result != KF_MENU_RESULT_PENDING) {
            break;
        }

        menu_update_list_input(&menu.list, NULL, &confirmed, &result);
        if (confirmed == KF_TRUE) {
            menu_play_sound_cue(KF_MENU_SOUND_CURSOR);
        }
        for (frame = 0; frame < 2; frame++) {
            menu_frame_begin();
            menu_render_list(&menu.list, KF_MENU_LIST_CARD_LOAD);
            menu_present_frame();
        }
    }

    if (result != KF_MENU_RESULT_CANCELLED) {
        menu_prepare_card_read_row(dialog_rows);
        menu_show_dialog_panel(KF_MENU_SYSTEM_UNTITLED, dialog_rows, 1, 70, 87, 178, 66, 2, 0);
        read_result = memory_card_read_slot(result);
        if (read_result != KF_CARD_READ_OK) {
            menu_prepare_card_read_failure_rows(dialog_rows, read_result);
            menu_show_dialog_panel(KF_MENU_SYSTEM_UNTITLED, dialog_rows, 3, 70, 87, 178, 81, 2, 0);
            input_wait_release();
            while (PadRead(1) == 0) {}
            input_wait_release();
        }
    }
    return result;
}

ADDRESS(0x8001c3bc, 0xf8)
void menu_build_card_probe_error_rows(KfMenuGlyphString *row)
{
    row->position.x = 70;
    row->position.y = 105;
    memcpy((void *)row->glyphs.codes, (const void *)menu_label_suffixes[5].codes, sizeof(KfMenuLabelSuffix));
    row->glyphs.codes[8] = 84;
    row->glyphs.codes[9] = 65;
    row->glyphs.codes[10] = 83;
    row->glyphs.codes[11] = -1;
    row++;
    row->position.x = 70;
    row->position.y = 120;
    row->glyphs.codes[0] = 0x1008;
    row->glyphs.codes[1] = 45;
    row->glyphs.codes[2] = 32;
    row->glyphs.codes[3] = 88;
    row->glyphs.codes[4] = 13;
    row->glyphs.codes[5] = 45;
    row->glyphs.codes[6] = 0x101b;
    row->glyphs.codes[7] = 0x1045;
    row->glyphs.codes[8] = -1;
    row++;
    row->position.x = 182;
    row->position.y = 120;
    row->glyphs.codes[0] = 0x1052;
    row->glyphs.codes[1] = 70;
    row->glyphs.codes[2] = 94;
    row->glyphs.codes[3] = 77;
    row->glyphs.codes[4] = 103;
    row->glyphs.codes[5] = -1;
}

ADDRESS(0x8001c4b4, 0x170)
void menu_build_card_full_rows(KfMenuGlyphString *row)
{
    row->position.x = 104;
    row->position.y = 105;
    memcpy((void *)row->glyphs.codes, (const void *)menu_label_suffixes[5].codes, sizeof(KfMenuLabelSuffix));
    row++;
    row->position.x = 104;
    row->position.y = 120;
    row->glyphs.codes[0] = 65;
    row->glyphs.codes[1] = 59;
    row->glyphs.codes[2] = 0x2059;
    row->glyphs.codes[3] = 65;
    row->glyphs.codes[4] = 0x1052;
    row->glyphs.codes[5] = 76;
    row->glyphs.codes[6] = -1;
    row++;
    row->position.x = 104;
    row->position.y = 135;
    row->glyphs.codes[0] = 73;
    row->glyphs.codes[1] = 88;
    row->glyphs.codes[2] = 5;
    row->glyphs.codes[3] = 45;
    row->glyphs.codes[4] = 0x1013;
    row->glyphs.codes[5] = 85;
    row->glyphs.codes[6] = 89;
    row->glyphs.codes[7] = -1;
    row++;
    row->position.x = 104;
    row->position.y = 150;
    memcpy((void *)row->glyphs.codes, (const void *)menu_label_suffixes[2].codes, sizeof(KfMenuLabelSuffix));
}

ADDRESS(0x8001c624, 0x268)
void menu_card_save_browser(void)
{
    struct DIRENTRY entries[KF_CARD_DIRECTORY_CAPACITY];
    KfCardMenuList menu;
    KfCardSlotGlyphRow glyph_rows[CARD_MENU_ROW_CAPACITY];
    s32 experience_values[CARD_MENU_ROW_CAPACITY];
    u8 levels[CARD_MENU_ROW_CAPACITY];
    s32 slot_ids[CARD_MENU_ROW_CAPACITY];
    KfMenuGlyphString dialog_rows[2];
    s32 matching_count;
    b32 confirmed = KF_FALSE;
    s32 result = KF_MENU_RESULT_PENDING;
    s32 count;
    s32 frame;

    menu_enter_display_state(1);
    menu_prepare_card_browser_rows(dialog_rows);
    menu_draw_card_dialog_rows(dialog_rows, 2, 70, 87, 178, 66, 2, 0);
    memory_card_start();
    memory_card_probe_temporary_file();
    memory_card_scan_save_entries(entries, &matching_count);
    count = menu_card_build_slot_rows(entries, glyph_rows[0].codes,
        experience_values, levels, slot_ids);
    glyph_rows[count].codes[0] = 0xe0;
    glyph_rows[count].codes[1] = 0xe1;
    glyph_rows[count].codes[2] = 0xe2;
    glyph_rows[count].codes[3] = -1;
    experience_values[count] = KF_MENU_LIST_NO_NUMBER;
    levels[count] = CARD_MENU_NO_LEVEL;
    slot_ids[count] = CARD_MENU_NEW_SLOT;
    count++;

    menu_list_init(&menu.list, KF_MENU_WINDOW_SYSTEM, KF_MENU_SYSTEM_SAVE_TITLE);
    menu.list.visible_rows = CARD_MENU_VISIBLE_ROWS;
    menu.list.list_y = CARD_MENU_LIST_Y;
    menu.list.entry_count = count;
    menu.rows = glyph_rows;
    menu.levels = levels;
    menu.experience_values = experience_values;
    menu_play_sound_cue(KF_MENU_SOUND_CURSOR);
    input_wait_release();

    for (;;) {
        if (confirmed || result != KF_MENU_RESULT_PENDING) {
            input_wait_release();
        }
        if (confirmed == KF_TRUE) {
            result = menu_preview_choice(&menu.list, KF_MENU_CONFIRM_SAVE, KF_MENU_LIST_CARD_SAVE, CARD_MENU_NO_PREVIEW_ITEM);
            if (result == KF_MENU_RESULT_CANCELLED) {
                result = KF_MENU_RESULT_PENDING;
            } else {
                result = menu.list.selected_index;
            }
        }
        if (result != KF_MENU_RESULT_PENDING) {
            break;
        }

        menu_update_list_input(&menu.list, NULL, &confirmed, &result);
        if (confirmed == KF_TRUE) {
            menu_play_sound_cue(KF_MENU_SOUND_CURSOR);
        }
        for (frame = 0; frame < 2; frame++) {
            menu_frame_begin();
            menu_render_list(&menu.list, KF_MENU_LIST_CARD_SAVE);
            menu_present_frame();
        }
    }

    if (result != KF_MENU_RESULT_CANCELLED) {
        menu_card_save_slot(slot_ids[result]);
    }
    memory_card_stop();
    menu_exit_display_state(KF_FALSE);
}

/* Shows a card dialog and waits for a fresh button press. */
#define MENU_CARD_DIALOG_WAIT(rows, count, width, height, overlap_x) do { \
    menu_draw_card_dialog_rows(rows, count, 70, 87, width, height, overlap_x, 0); \
    input_wait_release(); \
    while (PadRead(1) == 0) {} \
} while (0)

ADDRESS(0x8001c88c, 0x1e4)
void menu_card_save_slot(s32 slot)
{
    KfMenuGlyphString rows[4];
    s32 result = memory_card_probe_temporary_file();
    s32 width = 2;
    s32 overlap_x;

    if (result != KF_CARD_PROBE_OK) {
        if (result != KF_CARD_PROBE_CREATE_FAILED) {
            menu_prepare_card_io_error_rows(rows);
            MENU_CARD_DIALOG_WAIT(rows, 3, 178, 81, width);
            input_wait_release();
            return;
        }
        if (menu_confirm_card_format(KF_MENU_CARD_FORMAT_WITH_NOTICE) == KF_MENU_RESULT_ACCEPTED) {
            memory_card_format();
        } else {
            menu_prepare_card_format_declined_rows(rows);
            MENU_CARD_DIALOG_WAIT(rows, 3, 192, 66, width);
            input_wait_release();
            return;
        }
    }

    menu_prepare_card_write_rows(rows);
    width = 178;
    overlap_x = 2;
    menu_draw_card_dialog_rows(rows, 2, 70, 87, width, 66, overlap_x, 0);
    result = memory_card_write_slot(slot);
    if (result == KF_CARD_WRITE_OK) {
        return;
    }
    if (result == KF_CARD_WRITE_IO_FAILURE) {
        menu_prepare_card_io_error_rows(rows);
    } else {
        menu_prepare_card_write_full_rows(rows);
    }
    MENU_CARD_DIALOG_WAIT(rows, 3, width, 81, overlap_x);

    input_wait_release();
}

ADDRESS(0x8001ca70, 0x2f0)
s32 menu_confirm_card_format(s32 kind)
{
    KfMenuGlyphString labels[7];
    s32 cursor = 0;
    b32 confirmed = KF_FALSE;
    s32 result = KF_MENU_RESULT_PENDING;
    s32 selection = KF_MENU_SELECTION_NONE;
    s32 frame;

    labels[0].position.x = menu_window_layout(KF_MENU_WINDOW_SYSTEM)->rows[0].position.x;
    labels[0].position.y = menu_window_layout(KF_MENU_WINDOW_SYSTEM)->rows[0].position.y;
    labels[0].glyphs.codes[0] = 89;
    labels[0].glyphs.codes[1] = 65;
    labels[0].glyphs.codes[2] = KF_MENU_TEXT_END;
    labels[1].position.x = menu_window_layout(KF_MENU_WINDOW_SYSTEM)->rows[1].position.x;
    labels[1].position.y = menu_window_layout(KF_MENU_WINDOW_SYSTEM)->rows[1].position.y;
    labels[1].glyphs.codes[0] = 65;
    labels[1].glyphs.codes[1] = 65;
    labels[1].glyphs.codes[2] = 67;
    labels[1].glyphs.codes[3] = KF_MENU_TEXT_END;

    if (kind == KF_MENU_CARD_FORMAT_WITH_NOTICE) {
        labels[2].position.x = 90;
        labels[2].position.y = 110;
        memcpy((void *)labels[2].glyphs.codes, (const void *)menu_label_suffixes[5].codes, sizeof(KfMenuLabelSuffix));
        labels[3].position.x = 90;
        labels[3].position.y = 125;
        memcpy((void *)labels[3].glyphs.codes, (const void *)menu_label_suffixes[6].codes, sizeof(KfMenuLabelSuffix));
        labels[4].position.x = 174;
        labels[4].position.y = 125;
        memcpy((void *)labels[4].glyphs.codes, (const void *)menu_label_suffixes[7].codes, sizeof(KfMenuLabelSuffix));
        labels[5].position.x = 90;
        labels[5].position.y = 140;
        memcpy((void *)labels[5].glyphs.codes, (const void *)menu_label_suffixes[6].codes, sizeof(KfMenuLabelSuffix));
        labels[6].position.x = 174;
        labels[6].position.y = 140;
        memcpy((void *)labels[6].glyphs.codes, (const void *)menu_label_suffixes[8].codes, sizeof(KfMenuLabelSuffix));
    }

    for (;;) {
        if (selection != KF_MENU_SELECTION_NONE || result != KF_MENU_RESULT_PENDING) {
            input_wait_release();
        }
        switch (selection) {
        case KF_MENU_CHOICE_ACCEPT:
            result = KF_MENU_RESULT_ACCEPTED;
            break;
        case KF_MENU_CHOICE_DECLINE:
            result = KF_MENU_RESULT_CANCELLED;
            break;
        }
        if (result != KF_MENU_RESULT_PENDING) {
            break;
        }

        cursor = menu_poll_choice_input(cursor, KF_MENU_CHOICE_DECLINE,
            &selection, &confirmed, &result);
        for (frame = 0; frame < 2; frame++) {
            menu_frame_begin();
            menu_draw_two_option(&labels[0], &labels[1], cursor, confirmed);
            menu_blit_sprite_translucent(&menu_sprite_defs[KF_MENU_SPRITE_PANEL_BACKGROUND],
                &menu_window_layout(KF_MENU_WINDOW_SYSTEM)->rows[KF_MENU_SYSTEM_SAVE_TITLE].position);
            menu_draw_string(&menu_sprite_defs[KF_MENU_SPRITE_GLYPH_ATLAS],
                &menu_window_layout(KF_MENU_WINDOW_SYSTEM)->rows[KF_MENU_SYSTEM_SAVE_TITLE]);
            if (kind == KF_MENU_CARD_FORMAT_WITH_NOTICE) {
                menu_draw_string(&menu_sprite_defs[KF_MENU_SPRITE_GLYPH_ATLAS], &labels[2]);
                menu_draw_string(&menu_sprite_defs[KF_MENU_SPRITE_GLYPH_ATLAS], &labels[3]);
                menu_draw_string(&menu_sprite_defs[KF_MENU_SPRITE_GLYPH_ATLAS], &labels[4]);
                menu_draw_string(&menu_sprite_defs[KF_MENU_SPRITE_GLYPH_ATLAS], &labels[5]);
                menu_draw_string(&menu_sprite_defs[KF_MENU_SPRITE_GLYPH_ATLAS], &labels[6]);
                menu_draw_nine_slice_panel(70, 92, 220, 81, 2, 0);
            }
            menu_present_frame();
        }
    }
    return result;
}


ADDRESS(0x8001cd60, 0x80)
void menu_prepare_card_browser_rows(KfMenuGlyphString *row)
{
    row->position.x = 90;
    row->position.y = 105;
    memcpy((void *)row->glyphs.codes, (const void *)menu_label_suffixes[0].codes,
        sizeof menu_label_suffixes[0]);
    row++;
    row->position.x = 90;
    row->position.y = 120;
    memcpy((void *)row->glyphs.codes, (const void *)menu_label_suffixes[1].codes,
        sizeof menu_label_suffixes[1]);
    row->glyphs.codes[10] = -1;
}

ADDRESS(0x8001cde0, 0xf8)
void menu_prepare_card_io_error_rows(KfMenuGlyphString *row)
{
    row->position.x = 90;
    row->position.y = 105;
    memcpy((void *)row->glyphs.codes, (const void *)menu_label_suffixes[2].codes,
        sizeof menu_label_suffixes[2]);
    row++;
    row->position.x = 90;
    row->position.y = 120;
    memcpy((void *)row->glyphs.codes, (const void *)menu_label_suffixes[3].codes,
        sizeof menu_label_suffixes[3]);
    row++;
    row->position.x = 90;
    row->position.y = 135;
    memcpy((void *)row->glyphs.codes, (const void *)menu_label_suffixes[4].codes,
        sizeof menu_label_suffixes[4]);
    row->glyphs.codes[10] = -1;
}

ADDRESS(0x8001ced8, 0xf8)
void menu_prepare_card_format_declined_rows(KfMenuGlyphString *row)
{
    row->position.x = 90;
    row->position.y = 105;
    memcpy((void *)row->glyphs.codes, (const void *)menu_label_suffixes[6].codes,
        sizeof menu_label_suffixes[6]);
    row++;
    row->position.x = 174;
    row->position.y = 105;
    memcpy((void *)row->glyphs.codes, (const void *)menu_label_suffixes[9].codes,
        sizeof menu_label_suffixes[9]);
    row++;
    row->position.x = 90;
    row->position.y = 120;
    memcpy((void *)row->glyphs.codes, (const void *)menu_label_suffixes[2].codes,
        sizeof menu_label_suffixes[2]);
}

ADDRESS(0x8001cfd0, 0xf8)
void menu_prepare_card_write_full_rows(KfMenuGlyphString *row)
{
    row->position.x = 102;
    row->position.y = 105;
    memcpy((void *)row->glyphs.codes, (const void *)menu_label_suffixes[2].codes,
        sizeof menu_label_suffixes[2]);
    row++;
    row->position.x = 102;
    row->position.y = 120;
    memcpy((void *)row->glyphs.codes, (const void *)menu_label_suffixes[0].codes,
        sizeof menu_label_suffixes[0]);
    row++;
    row->position.x = 102;
    row->position.y = 135;
    memcpy((void *)row->glyphs.codes, (const void *)menu_label_suffixes[12].codes,
        sizeof menu_label_suffixes[12]);
    row->glyphs.codes[10] = -1;
}

ADDRESS(0x8001d0c8, 0x80)
void menu_prepare_card_write_rows(KfMenuGlyphString *row)
{
    row->position.x = 102;
    row->position.y = 105;
    memcpy((void *)row->glyphs.codes, (const void *)menu_label_suffixes[0].codes,
        sizeof menu_label_suffixes[0]);
    row->glyphs.codes[7] = 85;
    row++;
    row->position.x = 102;
    row->position.y = 120;
    memcpy((void *)row->glyphs.codes, (const void *)menu_label_suffixes[11].codes,
        sizeof menu_label_suffixes[11]);
}

ADDRESS(0x8001d148, 0x80)
void menu_prepare_card_read_row(KfMenuGlyphString *row)
{
    row->position.x = 102;
    row->position.y = 112;
    memcpy((void *)row->glyphs.codes, (const void *)menu_label_suffixes[13].codes,
        sizeof menu_label_suffixes[13]);
}

ADDRESS(0x8001d1c8, 0x17c)
void menu_prepare_card_read_failure_rows(KfMenuGlyphString *row, s32 kind)
{
    row->position.x = 90;
    row->position.y = 105;
    if (kind == KF_CARD_READ_IO_FAILURE) {
        memcpy((void *)row->glyphs.codes, (const void *)menu_label_suffixes[10].codes,
            sizeof menu_label_suffixes[10]);
    } else {
        row->glyphs.codes[0] = 0x1012;
        row->glyphs.codes[1] = 45;
        row->glyphs.codes[2] = 15;
        row->glyphs.codes[3] = 3;
        row->glyphs.codes[4] = 40;
        row->glyphs.codes[5] = 45;
        row->glyphs.codes[6] = -1;
    }
    row++;
    row->position.x = 90;
    row->position.y = 120;
    memcpy((void *)row->glyphs.codes, (const void *)menu_label_suffixes[3].codes,
        sizeof menu_label_suffixes[3]);
    row++;
    row->position.x = 90;
    row->position.y = 135;
    memcpy((void *)row->glyphs.codes, (const void *)menu_label_suffixes[4].codes,
        sizeof menu_label_suffixes[4]);
    row->glyphs.codes[10] = -1;
}

ADDRESS(0x8001d344, 0xf8)
void menu_prepare_card_exit_rows(KfMenuGlyphString *row)
{
    row->position.x = 90;
    row->position.y = 105;
    memcpy((void *)row->glyphs.codes, (const void *)menu_label_suffixes[14].codes,
        sizeof menu_label_suffixes[14]);
    row++;
    row->position.x = 90;
    row->position.y = 120;
    memcpy((void *)row->glyphs.codes, (const void *)menu_label_suffixes[15].codes,
        sizeof menu_label_suffixes[15]);
    row->glyphs.codes[10] = -1;
}

ADDRESS(0x8001d43c, 0x108)
void menu_draw_card_dialog_rows(const KfMenuGlyphString *rows, s32 count,
    s32 x, s32 y, s32 width, s32 height, s32 overlap_x, s32 overlap_y)
{
    const KfMenuGlyphString *current;
    s32 frame;
    s32 row;

    for (frame = 0; frame < 2; frame++) {
        menu_frame_begin();
        menu_blit_sprite_translucent(&menu_sprite_defs[KF_MENU_SPRITE_PANEL_BACKGROUND],
            &menu_window_layout(KF_MENU_WINDOW_SYSTEM)->rows[KF_MENU_SYSTEM_SAVE_TITLE].position);
        menu_draw_string(&menu_sprite_defs[KF_MENU_SPRITE_GLYPH_ATLAS],
            &menu_window_layout(KF_MENU_WINDOW_SYSTEM)->rows[KF_MENU_SYSTEM_SAVE_TITLE]);
        current = rows;
        for (row = 0; row < count; row++, current++) {
            menu_draw_string(&menu_sprite_defs[KF_MENU_SPRITE_GLYPH_ATLAS], current);
        }
        menu_draw_nine_slice_panel(x, y, width, height, overlap_x, overlap_y);
        menu_present_frame();
    }
}


DATA(0x80064be4, 0xf0, ".data")
KfMenuSpriteDef menu_sprite_defs[KF_MENU_SPRITE_COUNT] = {
    {0x1d, 0x7f64, 240, 0, 7, 14},
    {0x1d, 0x7f64, 0, 0, 15, 15},
    {0x1c, 0x7ee4, 128, 128, 14, 14},
    {0x1e, 0x7fa4, 160, 0, 54, 24},
    {0x1e, 0x7fa4, 160, 32, 54, 24},
    {0x1e, 0x7fa4, 0, 0, 124, 24},
    {0x1e, 0x7fa4, 0, 32, 124, 24},
    {0x1e, 0x7fa4, 0, 64, 236, 6},
    {0x1e, 0x7fa4, 0, 72, 236, 14},
    {0x1e, 0x7fa4, 0, 104, 236, 6},
    {0x1e, 0x7fa4, 0, 88, 236, 14},
    {0x1e, 0x7fa4, 0, 120, 33, 33},
    {0x1e, 0x7fa4, 40, 120, 28, 33},
    {0x1e, 0x7fa4, 72, 120, 33, 33},
    {0x1e, 0x7fa4, 0, 160, 33, 28},
    {0x1e, 0x7fa4, 40, 160, 28, 28},
    {0x1e, 0x7fa4, 72, 160, 33, 28},
    {0x1e, 0x7fa4, 0, 192, 33, 33},
    {0x1e, 0x7fa4, 40, 192, 28, 33},
    {0x1e, 0x7fa4, 72, 192, 33, 33},
};
DATA(0x80064cd4, 0x9a0, ".data")
KfMenuWindowLayout menu_window_layouts[KF_MENU_WINDOW_COUNT] = {
    {
        {{0, 0}, {{-1}}},
        {
            {{31, 19}, {{0, 1, 18, 32, 109, 114, 66, -1}}},
            {{31, 45}, {{120, 121, 109, 114, 66, -1}}},
            {{31, 71}, {{112, 113, -1}}},
            {{31, 97}, {{137, 138, 57, 122, 208, -1}}},
            {{31, 123}, {{117, 82, 106, -1}}},
            {{31, 149}, {{11, 12, 18, 32, -1}}},
            {{31, 175}, {{4, 8219, 11, 56, 39, -1}}},
            {{31, 201}, {{98, 4179, 106, -1}}},
        },
    },
    {
        {{17, 19}, {{11, 12, 18, 32, -1}}},
        {
            {{31, 45}, {{44, 45, 4115, -1}}},
            {{31, 71}, {{4104, 45, 32, 109, 99, 97, 106, -1}}},
            {{31, 97}, {{98, 4179, 106, -1}}},
            {{17, 19}, {{13, 45, 4123, -1}}},
            {{17, 19}, {{44, 45, 4115, -1}}},
            {{17, 19}, {{4104, 45, 32, 109, 99, 97, 106, -1}}},
        },
    },
    {
        {{17, 19}, {{4, 8219, 11, 56, 39, -1}}},
        {
            {{31, 45}, {{213, 214, 215, -1}}},
            {{31, 71}, {{215, 216, -1}}},
            {{31, 97}, {{133, 134, 217, 218, -1}}},
            {{31, 123}, {{219, 220, 217, 218, -1}}},
            {{31, 149}, {{0, 1, 18, 32, 217, 218, -1}}},
            {{31, 175}, {{238, 239, 213, 214, -1}}},
            {{31, 201}, {{98, 4179, 106, -1}}},
        },
    },
    {
        {{17, 19}, {{116, 66, 57, 115, 106, -1}}},
        {
            {{31, 45}, {{116, 66, -1}}},
            {{31, 71}, {{115, 106, -1}}},
            {{31, 97}, {{98, 4179, 106, -1}}},
        },
    },
    {
        {{17, 19}, {{164, 206, -1}}},
        {
            {{31, 45}, {{116, 66, -1}}},
            {{31, 71}, {{256, 257, 109, 207, 106, -1}}},
            {{31, 97}, {{98, 4179, 106, -1}}},
        },
    },
    {
        {{17, 19}, {{0, -1}}},
        {
            {{31, 45}, {{258, 259, -1}}},
            {{31, 71}, {{141, 142, 206, -1}}},
        },
    },
    {
        {{0, 0}, {{-1}}},
        {
            {{98, 94}, {{89, 4171, 97, 106, -1}}},
            {{98, 120}, {{44, 45, 4115, 76, 106, -1}}},
        },
    },
    {0},
};

enum {
    KF_MENU_UPLOAD_SECOND_BUFFER_Y = 240,
    KF_MENU_CURSOR_FRAME_COUNT = 8
};
/* The controller copies the first MENU_CATEGORY_COUNT rows; nothing reads
   the eleventh, which keeps the same glyph-row layout. */
DATA(0x80065674, 0x108, ".data")
KfMenuLabelSuffix menu_equipment_category_labels[11] = {
    {{118, 119, -1, 0, 0, 0, 0, 0, 0, 0}},
    {{120, 121, -1, 0, 0, 0, 0, 0, 0, 0}},
    {{122, 123, 57, 124, -1, 0, 0, 0, 0, 0}},
    {{122, 123, 57, 125, -1, 0, 0, 0, 0, 0}},
    {{122, 123, 57, 126, -1, 0, 0, 0, 0, 0}},
    {{122, 123, 57, 127, -1, 0, 0, 0, 0, 0}},
    {{122, 123, 57, 128, -1, 0, 0, 0, 0, 0}},
    {{0, 1, 18, 32, 230, -1, 0, 0, 0, 0}},
    {{0, 1, 18, 32, 231, -1, 0, 0, 0, 0}},
    {{58, 4125, 15, 39, -1, 0, 0, 0, 0, 0}},
    {{5, 45, 4115, 88, 224, 225, 226, -1, 0, 0}},
};
DATA(0x8006577c, 0x18, ".data")
KfMenuLabelSuffix menu_none_option_glyphs = {{89, 4172, 76, -1, 0, 0, 0, 0, 0, 0}};
DATA(0x80065794, 0x120, ".data")
KfMenuLabelSuffix menu_header_labels[12] = {
    {{130, 131, 132, -1, 0, 0, 0, 0, 0, 0}},
    {{43, 4124, 42, -1, 0, 0, 0, 0, 0, 0}},
    {{137, 138, 139, -1, 0, 0, 0, 0, 0, 0}},
    {{122, 208, 139, -1, 0, 0, 0, 0, 0, 0}},
    {{133, 134, -1, 0, 0, 0, 0, 0, 0}},
    {{8217, 40, 40, 1, 4108, -1, 0, 0, 0, 0}},
    {{255, 255, 5, 45, 12, -1, 0, 0, 0, 0}},
    {{255, 255, 12, 44, 45, -1, 0, 0, 0, 0}},
    {{255, 8221, 1, 4110, 39, -1, 0, 0, 0, 0}},
    {{255, 255, 4111, 45, 7, -1, 0, 0, 0, 0}},
    {{255, 255, 255, 197, 198, -1, 0, 0, 0, 0}},
    {{4105, 45, 42, 4115, -1, 0, 0, 0, 0}},
};
DATA(0x800658b4, 0x270, ".data")
KfMenuLabelSuffix menu_label_suffixes[16] = {
    {{33, 34, 41, 45, 5, 45, 4115, 88, -1, 0}},
    {{16, 51, 53, 7, 109, 75, 82, 65, 94, 76}},
    {{13, 45, 4123, 4178, 70, 94, 77, 103, -1, 0}},
    {{33, 34, 41, 45, 5, 45, 4115, 109, -1, 0}},
    {{16, 51, 53, 7, 75, 82, 71, 4175, 74, 65}},
    {{33, 34, 41, 45, 5, 45, 4115, 4165, -1, 0}},
    {{27, 52, 45, 30, 53, 19, -1, 0, 0, 0}},
    {{74, 107, 82, 65, 94, 77, 103, -1, 0, 0}},
    {{75, 94, 76, 69, -1, 0, 0, 0, 0, 0}},
    {{109, 75, 84, 65, 83, -1, 0, 0, 0, 0}},
    {{44, 45, 4115, 4178, 70, 94, 77, 103, -1, 0}},
    {{13, 45, 4123, 75, 82, 65, 94, 76, -1, 0}},
    {{267, 268, 4165, 79, 105, 94, 77, 103, -1, 0}},
    {{44, 45, 4115, 75, 82, 65, 94, 76, -1, 0}},
    {{269, 270, 109, 86, 65, 82, -1, 0, 0, 0}},
    {{161, 271, 109, 263, 59, 82, 71, 4175, 74, 65}}
};
DATA(0x80065b24, 0xb40, ".data")
KfMenuGlyphRow menu_glyph_rows[KF_ITEM_ID_COUNT] = {
    {{4111, 4101, 45, -1, 0, 0, 0, 0, 0, 0, 0, 0}},
    {{11, 56, 45, 19, 14, 45, 4115, -1, 0, 0, 0, 0}},
    {{20, 1, 19, 14, 45, 4115, -1, 0, 0, 0, 0, 0}},
    {{34, 45, 21, 39, 4103, 12, 15, 45, -1, 0, 0, 0}},
    {{4121, 19, 42, 25, 39, 30, 45, -1, 0, 0, 0, 0}},
    {{4121, 12, 15, 45, 4115, 14, 45, 4115, -1, 0, 0, 0}},
    {{7, 43, 13, 39, 19, 0, 53, 7, 12, -1, 0, 0}},
    {{0, -1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}},
    {{0, -1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}},
    {{27, 43, 1, 32, 14, 45, 4115, -1, 0, 0, 0, 0}},
    {{160, 161, -1, 0, 0, 0, 0, 0, 0, 0, 0, 0}},
    {{12, 8217, 1, 4111, 45, -1, 0, 0, 0, 0, 0, 0}},
    {{0, 1, 12, 4123, 43, 45, 4115, -1, 0, 0, 0, 0}},
    {{33, 43, 42, 57, 2, 42, 88, 129, -1, 0, 0, 0}},
    {{32, 45, 39, 40, 1, 19, 14, 45, 4115, -1, 0, 0}},
    {{4111, 45, 7, 57, 12, 43, 1, 35, 45, -1, 0, 0}},
    {{4125, 2, -1, 0, 0, 0, 0, 0, 0, 0, 0, 0}},
    {{0, 42, 4121, 43, 12, 19, -1, 0, 0, 0, 0, 0}},
    {{0, -1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}},
    {{0, -1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}},
    {{0, -1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}},
    {{0, 1, 0, 39, 30, 12, 7, -1, 0, 0, 0, 0}},
    {{20, 1, 19, 28, 42, 32, -1, 0, 0, 0, 0, 0}},
    {{4103, 43, 45, 19, 28, 42, 32, -1, 0, 0, 0, 0}},
    {{4123, 40, 53, 4115, 7, 40, 2, 39, -1, 0, 0, 0}},
    {{162, 163, 88, 150, -1, 0, 0, 0, 0, 0, 0, 0}},
    {{33, 43, 42, 57, 2, 42, 88, 150, -1, 0, 0, 0}},
    {{0, -1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}},
    {{4123, 43, 12, 19, 8219, 43, 45, 19, -1, 0, 0, 0}},
    {{20, 1, 19, 8219, 43, 45, 19, -1, 0, 0, 0, 0}},
    {{0, 1, 12, 0, 45, 30, 45, -1, 0, 0, 0, 0}},
    {{211, 212, 88, 157, -1, 0, 0, 0, 0, 0, 0, 0}},
    {{33, 43, 42, 57, 2, 42, 88, 157, -1, 0, 0, 0}},
    {{0, -1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}},
    {{43, 4106, 45, 11, 45, 42, 4115, -1, 0, 0, 0, 0}},
    {{40, 45, 4107, 11, 45, 42, 4115, -1, 0, 0, 0, 0}},
    {{32, 45, 39, 4101, 45, 4115, -1, 0, 0, 0, 0, 0}},
    {{7, 41, 12, 15, 42, 4101, 45, 4115, -1, 0, 0, 0}},
    {{12, 5, 42, 11, 45, 42, 4115, -1, 0, 0, 0, 0}},
    {{33, 43, 42, 57, 2, 42, 88, 124, -1, 0, 0, 0}},
    {{0, -1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}},
    {{0, 1, 0, 39, 4103, 44, 45, 4123, -1, 0, 0, 0}},
    {{12, 19, 45, 39, 25, 39, 4115, -1, 0, 0, 0, 0}},
    {{11, 42, 4121, 45, 0, 45, 32, -1, 0, 0, 0, 0}},
    {{4114, 45, 34, 39, 25, 39, 4115, -1, 0, 0, 0, 0}},
    {{42, 1, 20, 12, 4103, 44, 45, 4123, -1, 0, 0, 0}},
    {{0, -1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}},
    {{0, 1, 0, 39, 4123, 45, 17, -1, 0, 0, 0, 0}},
    {{43, 53, 4103, 4101, 45, 4111, 45, -1, 0, 0, 0, 0}},
    {{11, 42, 4121, 45, 4123, 45, 17, -1, 0, 0, 0, 0}},
    {{4114, 12, 2, 52, 45, 5, 45, -1, 0, 0, 0, 0}},
    {{42, 1, 20, 12, 4123, 45, 17, -1, 0, 0, 0, 0}},
    {{0, -1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}},
    {{204, 88, 127, 156, -1, 0, 0, 0, 0, 0, 0, 0}},
    {{11, 45, 12, 88, 177, -1, 0, 0, 0, 0, 0, 0}},
    {{33, 43, 42, 57, 2, 42, 88, 127, 156, -1, 0, 0}},
    {{168, 205, 88, 158, 156, -1, 0, 0, 0, 0, 0, 0}},
    {{246, 81, 247, 248, 249, 88, 250, 156, -1, 0, 0, 0}},
    {{251, 83, 252, 88, 120, 253, 72, -1, 0, 0, 0, 0}},
    {{189, 254, 88, 158, 156, -1, 0, 0, 0, 0, 0, 0}},
    {{0, -1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}},
    {{0, -1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}},
    {{0, -1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}},
    {{0, -1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}},
    {{0, -1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}},
    {{0, -1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}},
    {{0, -1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}},
    {{165, 166, 88, 168, 169, -1, 0, 0, 0, 0, 0, 0}},
    {{141, 142, 159, 105, 88, 168, 169, -1, 0, 0, 0, 0}},
    {{181, 149, 88, 168, 169, -1, 0, 0, 0, 0, 0, 0}},
    {{6, 39, 5, -1, 0, 0, 0, 0, 0, 0, 0, 0}},
    {{4123, 40, 53, 4115, 12, 19, 45, 39, -1, 0, 0, 0}},
    {{32, 45, 39, 12, 19, 45, 39, -1, 0, 0, 0, 0}},
    {{4098, 48, 45, 4111, 1, 19, -1, 0, 0, 0, 0, 0}},
    {{143, 144, -1, 0, 0, 0, 0, 0, 0, 0, 0, 0}},
    {{136, 145, 75, 144, -1, 0, 0, 0, 0, 0, 0, 0}},
    {{148, 149, 144, 88, 151, -1, 0, 0, 0, 0, 0, 0}},
    {{146, 147, 88, 141, -1, 0, 0, 0, 0, 0, 0, 0}},
    {{146, 147, 88, 141, -1, 0, 0, 0, 0, 0, 0, 0}},
    {{146, 147, 88, 141, -1, 0, 0, 0, 0, 0, 0, 0}},
    {{146, 147, 88, 141, -1, 0, 0, 0, 0, 0, 0, 0}},
    {{136, 88, 141, -1, 0, 0, 0, 0, 0, 0, 0, 0}},
    {{141, 142, 88, 173, -1, 0, 0, 0, 0, 0, 0, 0}},
    {{11, 45, 12, 88, 196, -1, 0, 0, 0, 0, 0, 0}},
    {{171, 88, 172, -1, 0, 0, 0, 0, 0, 0, 0, 0}},
    {{174, 175, 88, 176, -1, 0, 0, 0, 0, 0, 0, 0}},
    {{11, 45, 12, 88, 191, -1, 0, 0, 0, 0, 0, 0}},
    {{199, 120, 88, 200, -1, 0, 0, 0, 0, 0, 0, 0}},
    {{188, 88, 111, -1, 0, 0, 0, 0, 0, 0, 0, 0}},
    {{74, 74, 99, 70, 88, 167, -1, 0, 0, 0, 0, 0}},
    {{190, 88, 7, 41, 12, 15, 42, -1, 0, 0, 0, 0}},
    {{141, 88, 7, 41, 12, 15, 42, -1, 0, 0, 0, 0}},
    {{187, 88, 7, 41, 12, 15, 42, -1, 0, 0, 0, 0}},
    {{188, 88, 7, 41, 12, 15, 42, -1, 0, 0, 0, 0}},
    {{189, 88, 7, 41, 12, 15, 42, -1, 0, 0, 0, 0}},
    {{4111, 45, 7, 7, 41, 12, 15, 42, -1, 0, 0, 0}},
    {{141, 142, -1, 0, 0, 0, 0, 0, 0, 0, 0, 0}},
    {{141, 142, 88, 69, 72, 104, -1, 0, 0, 0, 0, 0}},
    {{31, 11, 37, 2, -1, 0, 0, 0, 0, 0, 0, 0}},
    {{3, 42, 27, 88, 164, -1, 0, 0, 0, 0, 0, 0}},
    {{165, 166, 88, 164, -1, 0, 0, 0, 0, 0, 0, 0}},
    {{194, 195, 88, 164, -1, 0, 0, 0, 0, 0, 0, 0}},
    {{178, 88, 164, -1, 0, 0, 0, 0, 0, 0, 0, 0}},
    {{182, 183, 88, 164, -1, 0, 0, 0, 0, 0, 0, 0}},
    {{201, 149, 88, 164, -1, 0, 0, 0, 0, 0, 0, 0}},
    {{148, 88, 186, -1, 0, 0, 0, 0, 0, 0, 0, 0}},
    {{120, 192, 193, 88, 164, -1, 0, 0, 0, 0, 0, 0}},
    {{210, 88, 164, -1, 0, 0, 0, 0, 0, 0, 0, 0}},
    {{209, 88, 164, -1, 0, 0, 0, 0, 0, 0, 0, 0}},
    {{184, 185, 88, 164, -1, 0, 0, 0, 0, 0, 0, 0}},
    {{223, 110, 88, 127, -1, 0, 0, 0, 0, 0, 0, 0}},
    {{152, 88, 4104, 45, 19, -1, 0, 0, 0, 0, 0, 0}},
    {{153, 88, 4104, 45, 19, -1, 0, 0, 0, 0, 0, 0}},
    {{154, 155, 88, 4104, 45, 19, -1, 0, 0, 0, 0, 0}},
    {{152, 88, 164, -1, 0, 0, 0, 0, 0, 0, 0, 0}},
    {{153, 88, 164, -1, 0, 0, 0, 0, 0, 0, 0, 0}},
    {{154, 155, 88, 164, -1, 0, 0, 0, 0, 0, 0, 0}},
    {{141, 142, 88, 135, -1, 0, 0, 0, 0, 0, 0, 0}},
    {{3, 42, 27, 88, 135, -1, 0, 0, 0, 0, 0, 0}},
    {{40, 12, 19, 4104, 45, 19, -1, 0, 0, 0, 0, 0}},
};
DATA(0x80066664, 0x1e0, ".data")
KfMenuGlyphRow menu_glyph_rows_extra[20] = {
    {{2, 52, 45, 15, 45, 27, 52, 45, 42, -1, 0, 0}},
    {{12, 19, 45, 39, -1, 0, 0, 0, 0, 0, 0, 0}},
    {{0, 45, 12, 2, 51, 45, 4123, -1, 0, 0, 0, 0}},
    {{33, 18, 4, -1, 0, 0, 0, 0, 0, 0, 0, 0}},
    {{2, 49, 39, 4115, 5, 53, 15, 45, -1, 0, 0, 0}},
    {{0, 1, 12, 12, 19, 45, 32, -1, 0, 0, 0, 0}},
    {{27, 41, 45, 4108, -1, 0, 0, 0, 0, 0, 0, 0}},
    {{27, 48, 1, 0, 45, 4125, 45, 42, -1, 0, 0, 0}},
    {{27, 48, 1, 0, 45, 2, 52, 45, 42, -1, 0, 0}},
    {{27, 48, 1, 0, 45, 12, 19, 45, 32, -1, 0, 0}},
    {{27, 43, 1, 32, -1, 0, 0, 0, 0, 0, 0, 0}},
    {{40, 1, 19, 21, 39, 4103, 4125, 42, 19, -1, 0, 0}},
    {{27, 40, 53, 11, 55, -1, 0, 0, 0, 0, 0, 0}},
    {{11, 45, 12, -1, 0, 0, 0, 0, 0, 0, 0, 0}},
    {{4114, 49, 12, 8221, 1, 4110, 39, -1, 0, 0, 0, 0}},
    {{43, 4107, 12, 19, 27, 48, 1, 0, -1, 0, 0, 0}},
    {{0, 45, 12, 26, 45, 42, -1, 0, 0, 0, 0, 0}},
    {{31, 10, 1, 42, 11, 45, 42, 4115, -1, 0, 0, 0}},
    {{40, 1, 19, -1, 0, 0, 0, 0, 0, 0, 0, 0}},
    {{4123, 43, 12, -1, 0, 0, 0, 0, 0, 0, 0, 0}},
};
DATA(0x80066844, 0x2d0, ".data")
u8 menu_item_mask_pages[6][KF_ITEM_ID_COUNT] = {
    {
        0, 0, 0, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0,
        0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0,
        0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 1, 1, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    },
    {
        0, 1, 1, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 1, 1, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0,
        0, 1, 0, 1, 0, 0, 0, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    },
    {
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    },
    {
        0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0,
        0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1, 0, 0,
        0, 0, 0, 1, 0, 0, 0, 0, 1, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 0, 0, 0,
        0, 0, 0, 0, 1, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    },
    {
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0,
    },
    {
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    },
};
DATA(0x80066b14, 0x5a0, ".data")
u16 menu_item_code_primary[6][KF_ITEM_ID_COUNT] = {
    {
        50000, 50000, 50000, 990, 50000, 2850, 50000, 50000, 50000, 50000, 50000, 50000,
        50000, 50000, 50000, 50000, 1830, 50000, 50000, 50000, 50000, 50000, 680, 2950,
        50000, 50000, 50000, 50000, 50000, 50000, 50000, 50000, 50000, 50000, 50000, 780,
        50000, 50000, 50000, 50000, 50000, 50000, 1200, 3400, 50000, 50000, 50000, 50000,
        50000, 50000, 50000, 50000, 50000, 50000, 50000, 50000, 50000, 50000, 50000, 50000,
        50000, 50000, 50000, 50000, 50000, 50000, 50000, 50000, 50000, 50000, 50000, 320,
        50000, 50000, 23, 48, 50000, 50000, 50000, 50000, 50000, 50000, 50000, 50000,
        50000, 50000, 50000, 50000, 50000, 50000, 50000, 50000, 50000, 50000, 50000, 50000,
        50000, 50000, 50000, 50000, 50000, 50000, 50000, 50000, 50000, 50000, 50000, 50000,
        50000, 50000, 50000, 50000, 50000, 50000, 50000, 50000, 50000, 50000, 50000, 50000,
    },
    {
        50000, 180, 640, 50000, 50000, 50000, 4300, 50000, 50000, 50000, 50000, 50000,
        50000, 50000, 50000, 50000, 50000, 50000, 50000, 50000, 50000, 200, 780, 50000,
        50000, 50000, 50000, 50000, 330, 950, 50000, 50000, 50000, 50000, 150, 830,
        50000, 50000, 50000, 50000, 50000, 270, 50000, 3900, 50000, 50000, 50000, 350,
        1800, 4000, 50000, 50000, 50000, 50000, 50000, 50000, 50000, 50000, 50000, 50000,
        50000, 50000, 50000, 50000, 50000, 50000, 50000, 50000, 50000, 50000, 50000, 330,
        650, 17700, 20, 40, 50000, 50000, 50000, 50000, 50000, 50000, 50000, 50000,
        50000, 50000, 50000, 50000, 50000, 50000, 50000, 50000, 50000, 50000, 50000, 50000,
        50000, 50000, 50000, 50000, 50000, 50000, 50000, 50000, 50000, 50000, 50000, 50000,
        50000, 50000, 50000, 50000, 50000, 50000, 50000, 50000, 50000, 50000, 50000, 50000,
    },
    {
        50000, 50000, 50000, 50000, 50000, 50000, 50000, 50000, 50000, 50000, 50000, 50000,
        50000, 50000, 50000, 50000, 50000, 50000, 50000, 50000, 50000, 50000, 50000, 50000,
        50000, 50000, 50000, 50000, 50000, 50000, 50000, 50000, 50000, 50000, 180, 50000,
        2380, 50000, 50000, 50000, 50000, 50000, 50000, 50000, 50000, 50000, 50000, 50000,
        50000, 50000, 50000, 50000, 50000, 50000, 50000, 50000, 50000, 50000, 8250, 50000,
        50000, 50000, 50000, 50000, 50000, 50000, 50000, 50000, 6300, 50000, 50000, 50000,
        50000, 50000, 18, 33, 50000, 50000, 50000, 50000, 50000, 50000, 50000, 50000,
        50000, 50000, 4800, 50000, 50000, 50000, 50000, 50000, 50000, 50000, 50000, 50000,
        50000, 50000, 50000, 50000, 50000, 50000, 50000, 50000, 50000, 50000, 50000, 50000,
        50000, 50000, 50000, 50000, 50000, 50000, 50000, 50000, 50000, 50000, 50000, 50000,
    },
    {
        50000, 50000, 50000, 50000, 50000, 3250, 50000, 50000, 50000, 50000, 50000, 50000,
        50000, 50000, 50000, 50000, 2020, 50000, 50000, 50000, 50000, 50000, 50000, 3600,
        50000, 50000, 50000, 50000, 50000, 50000, 50000, 50000, 50000, 50000, 50000, 1000,
        50000, 19800, 50000, 50000, 50000, 50000, 50000, 3400, 50000, 50000, 50000, 50000,
        1600, 4200, 50000, 23800, 50000, 50000, 50000, 50000, 50000, 50000, 50000, 50000,
        50000, 50000, 50000, 50000, 50000, 50000, 50000, 50000, 50000, 50000, 50000, 50000,
        50000, 50000, 28, 52, 2800, 50000, 50000, 50000, 50000, 50000, 50000, 50000,
        15000, 50000, 50000, 2800, 50000, 50000, 50000, 50000, 50000, 50000, 50000, 50000,
        50000, 50000, 50000, 50000, 50000, 50000, 50000, 50000, 50000, 50000, 50000, 50000,
        50000, 50000, 50000, 50000, 50000, 50000, 50000, 50000, 50000, 50000, 50000, 50000,
    },
    {
        50000, 50000, 50000, 50000, 50000, 50000, 50000, 50000, 50000, 50000, 50000, 50000,
        50000, 50000, 50000, 50000, 50000, 50000, 50000, 50000, 50000, 50000, 50000, 50000,
        50000, 50000, 50000, 50000, 50000, 50000, 50000, 50000, 50000, 50000, 50000, 50000,
        50000, 50000, 50000, 50000, 50000, 50000, 50000, 50000, 50000, 50000, 50000, 50000,
        50000, 50000, 50000, 50000, 50000, 50000, 50000, 50000, 50000, 50000, 50000, 50000,
        50000, 50000, 50000, 50000, 50000, 50000, 50000, 50000, 50000, 50000, 50000, 50000,
        50000, 50000, 50000, 50000, 50000, 50000, 50000, 50000, 50000, 50000, 2, 50000,
        50000, 50000, 50000, 50000, 50000, 50000, 50000, 50000, 50000, 50000, 50000, 50000,
        50000, 50000, 50000, 50000, 50000, 50000, 50000, 50000, 50000, 50000, 50000, 50000,
        50000, 50000, 50000, 50000, 50000, 50000, 50000, 50000, 50000, 1, 50000, 50000,
    },
    {
        50000, 50000, 50000, 50000, 50000, 50000, 50000, 50000, 50000, 50000, 50000, 50000,
        50000, 50000, 50000, 50000, 50000, 50000, 50000, 50000, 50000, 50000, 50000, 50000,
        50000, 50000, 50000, 50000, 50000, 50000, 50000, 50000, 50000, 50000, 50000, 50000,
        50000, 50000, 50000, 50000, 50000, 50000, 50000, 50000, 50000, 50000, 50000, 50000,
        50000, 50000, 50000, 50000, 50000, 50000, 50000, 50000, 50000, 50000, 50000, 50000,
        50000, 50000, 50000, 50000, 50000, 50000, 50000, 50000, 50000, 50000, 50000, 50000,
        50000, 50000, 50000, 50000, 50000, 50000, 50000, 50000, 50000, 50000, 50000, 50000,
        50000, 50000, 50000, 50000, 50000, 50000, 50000, 50000, 50000, 50000, 50000, 50000,
        50000, 50000, 50000, 50000, 50000, 50000, 50000, 50000, 50000, 50000, 8350, 1530,
        13500, 50000, 50000, 50000, 50000, 50000, 50000, 50000, 50000, 50000, 50000, 50000,
    },
};
DATA(0x800670b4, 0x4b0, ".data")
u16 menu_item_code_secondary[5][KF_ITEM_ID_COUNT] = {
    {
        70, 120, 520, 750, 2000, 2100, 2700, 0, 0, 13500, 12800, 13000,
        15000, 26000, 28500, 30000, 1750, 2800, 0, 0, 0, 130, 550, 2000,
        14800, 16500, 23500, 0, 130, 570, 18500, 14500, 25000, 0, 50, 500,
        1300, 18000, 14300, 28000, 0, 150, 880, 1750, 12750, 21000, 0, 210,
        1500, 3500, 13300, 20000, 0, 1880, 2500, 8230, 9100, 11000, 6200, 3350,
        0, 0, 0, 0, 0, 0, 0, 4700, 3700, 6280, 0, 250,
        600, 4830, 8, 13, 1600, 700, 700, 700, 700, 700, 700, 3280,
        1030, 6000, 3300, 1800, 40000, 36500, 8500, 8500, 8500, 8500, 8500, 13500,
        530, 25, 0, 4500, 6500, 9750, 100, 280, 2400, 11000, 6000, 900,
        1900, 18000, 0, 2400, 2400, 2400, 1800, 1800, 1800, 20, 130, 0,
    },
    {
        80, 130, 450, 800, 1900, 1900, 2100, 0, 0, 7300, 8500, 13600,
        14300, 24800, 24800, 25000, 1630, 3200, 0, 0, 0, 140, 600, 2000,
        17000, 16000, 20000, 0, 230, 480, 12700, 14800, 20000, 0, 80, 450,
        1430, 15100, 14300, 20000, 0, 180, 1000, 2850, 11800, 21000, 0, 210,
        1500, 3000, 12700, 19800, 0, 1430, 1300, 7850, 8770, 11200, 7300, 3280,
        0, 0, 0, 0, 0, 0, 0, 4800, 3300, 6300, 0, 250,
        580, 4300, 12, 16, 1800, 800, 800, 800, 800, 800, 800, 2200,
        950, 5800, 2800, 1800, 38000, 3200, 7000, 7000, 7000, 7000, 8270, 13500,
        550, 26, 0, 350, 5220, 8500, 200, 350, 2000, 8640, 5880, 950,
        1880, 18000, 0, 2400, 2400, 2400, 1800, 1800, 1800, 20, 130, 0,
    },
    {
        80, 140, 480, 550, 1450, 1300, 1730, 0, 0, 4700, 7000, 7800,
        8150, 8500, 8500, 8330, 1780, 3500, 0, 0, 0, 160, 580, 2150,
        9200, 6380, 7700, 0, 250, 600, 4500, 4850, 7600, 0, 100, 580,
        1500, 4000, 3000, 5500, 0, 200, 650, 2230, 1200, 4700, 0, 180,
        1230, 3550, 1250, 5100, 0, 1700, 500, 5200, 1380, 1500, 5500, 3600,
        0, 0, 0, 0, 0, 0, 0, 3200, 4500, 1200, 0, 250,
        580, 2450, 13, 25, 2000, 950, 950, 950, 950, 950, 950, 1100,
        370, 1500, 1850, 600, 1330, 5110, 5800, 5800, 5800, 5800, 5980, 4800,
        500, 25, 0, 10, 4830, 6150, 550, 500, 1800, 3500, 4300, 1020,
        2530, 8320, 0, 2250, 2250, 2250, 1700, 1700, 1700, 20, 130, 0,
    },
    {
        60, 150, 600, 880, 1600, 1780, 2550, 0, 0, 8500, 11000, 11500,
        16500, 22000, 25300, 28800, 1700, 2800, 0, 0, 0, 200, 480, 2000,
        12200, 14300, 19800, 0, 100, 350, 13800, 15000, 21000, 0, 50, 370,
        1150, 16200, 14300, 22000, 0, 150, 730, 1630, 12750, 17700, 0, 250,
        1340, 3300, 13300, 16000, 0, 1570, 2500, 9650, 9100, 12000, 6200, 3350,
        0, 0, 0, 0, 0, 0, 0, 4700, 5200, 7850, 0, 250,
        435, 5120, 5, 10, 1600, 530, 530, 530, 530, 530, 530, 8200,
        950, 6650, 3300, 1800, 36000, 3850, 8500, 8500, 8500, 8500, 8500, 22750,
        530, 25, 0, 5000, 6500, 10800, 100, 250, 2400, 11000, 6350, 850,
        1760, 18000, 0, 2400, 2400, 2400, 1800, 1800, 1800, 20, 130, 0,
    },
    {
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 5000, 6500, 11300, 760, 580, 3800, 16200, 7800, 1400,
        10500, 22900, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    },
};
DATA(0x8006e5c0, 0x4, ".sdata")
b32 menu_item_model_allocation_pending = KF_FALSE;
DATA(0x8006e5c4, 0x4, ".sdata")
b32 input_press_pending = KF_FALSE;
DATA(0x8006e5c8, 0x4, ".sdata")
s32 menu_item_quantity = 1;
DATA(0x8006e5cc, 0x4, ".sdata")
s32 menu_cursor_animation_frame = 0;
DATA(0x8006e5d0, 0x4, ".sdata")
KfMenuCursorAnimation menu_cursor_animation_direction = KF_MENU_CURSOR_ANIMATION_FORWARD;
DATA(0x8006e914, 0x4, ".sbss")
#ifndef __cplusplus
static POLY_FT4 *current_poly_ft4;
#endif
DATA(0x8006e91c, 0x1, ".sbss")
static KfPlayerOption menu_saved_music_enabled;
DATA(0x8006e924, 0x4, ".sbss")
static u_long *menu_frame_upload_pixels;
DATA(0x8006e92c, 0x8, ".sbss")
static RECT menu_frame_upload_rect;
DATA(0x8006e934, 0x8, ".sbss")
static SVECTOR menu_item_preview_translation;
DATA(0x8006e93c, 0x8, ".sbss")
static SVECTOR menu_item_preview_rotation;
DATA(0x8006e944, 0x4, ".sbss")
static s32 menu_item_preview_rotation_step;
DATA(0x8006eb24, 0x18, ".bss")
static KfPrimitiveBuffer menu_saved_primitive_buffers[KF_DISPLAY_BUFFER_COUNT];

ADDRESS(0x8001d544, 0x178)
void menu_item_buy_sell_controller(s32 kind)
{
    s32 cursor = 0;
    b32 confirmed = KF_FALSE;
    s32 result = KF_MENU_RESULT_PENDING;
    s32 selection = KF_MENU_SELECTION_NONE;
    s32 frame;

    menu_enter_display_state(1);
    for (frame = 0; frame < 2; frame++) {
        menu_frame_begin();
        menu_draw_window(KF_MENU_WINDOW_SHOP, KF_MENU_SHOP_ROW_COUNT, cursor, confirmed);
        menu_present_frame();
    }
    menu_play_sound_cue(KF_MENU_SOUND_CURSOR);
    input_wait_release();
    for (;;) {
        if (selection != KF_MENU_SELECTION_NONE || result != KF_MENU_RESULT_PENDING) {
            input_wait_release();
        }

        switch (selection) {
        case KF_MENU_SHOP_BUY_ROW:
            menu_item_buy_controller(kind);
            break;
        case KF_MENU_SHOP_SELL_ROW:
            menu_item_sell_controller(kind);
            break;
        }

        if (result != KF_MENU_RESULT_PENDING) {
            break;
        }
        cursor = menu_poll_choice_input(cursor, KF_MENU_SHOP_RETURN_ROW,
            &selection, &confirmed, &result);
        for (frame = 0; frame < 2; frame++) {
            menu_frame_begin();
            menu_draw_window(KF_MENU_WINDOW_SHOP, KF_MENU_SHOP_ROW_COUNT, cursor, confirmed);
            menu_present_frame();
        }
    }
    menu_exit_display_state(KF_FALSE);
}

ADDRESS(0x8001d6bc, 0x310)
void menu_item_buy_controller(s32 kind)
{
    KfItemMenuList menu;
    KfMenuGlyphRow rows[40];
    u8 values[40];
    s32 prices[40];
    u8 indices[40];
    b32 confirmed = KF_FALSE;
    s32 result = KF_MENU_RESULT_PENDING;
    s32 count;
    s32 frame;
    s32 cost;
    u8 selected_item;
    u8 *counters = game_counter_bytes;

    if (counters[KF_ENUM_ENCODE(u8, KF_OBJECT_51)] == 0) {
        menu_item_mask_pages[3][KF_ENUM_ENCODE(u8, KF_OBJECT_51)] = 1;
    } else {
        menu_item_mask_pages[3][KF_ENUM_ENCODE(u8, KF_OBJECT_51)] = 0;
    }
    count = menu_collect_masked_item_rows(menu_item_mask_pages[kind], rows, values, indices,
        0, KF_ITEM_ID_LAST);
    menu_fill_item_counts_and_prices(counters, values, prices, indices, 0, count, kind);
    menu_list_init(&menu.list, KF_MENU_WINDOW_SHOP, 0);
    menu.list.entry_count = count;
    menu.rows = rows;
    menu.values = values;
    menu.prices = prices;
    menu.list.glyphs_per_entry = 12;

    if (menu_load_item_model(indices[menu.list.selected_index]) != 0) {
        return;
    }

    for (;;) {
        if (confirmed || result != KF_MENU_RESULT_PENDING) {
            input_wait_release();
        }

        if (confirmed == KF_TRUE) {
            result = menu_preview_choice(&menu.list, KF_MENU_CONFIRM_BUY, KF_MENU_LIST_SHOP_BUY, selected_item);
            if (result == KF_MENU_RESULT_CANCELLED) {
                result = KF_MENU_RESULT_PENDING;
            } else {
                result = selected_item;
            }
        }
        if (result != KF_MENU_RESULT_PENDING) {
            break;
        }

        menu_update_list_input(&menu.list, indices, &confirmed, &result);
        selected_item = indices[menu.list.selected_index];
        if (confirmed == KF_TRUE) {
            if (player_state.gold < (u32)(menu.prices[menu.list.selected_index]
                    * menu_item_quantity)
                    || counters[selected_item] + menu_item_quantity >= 100) {
                menu_play_sound_cue(KF_MENU_SOUND_CANCEL);
                confirmed = KF_FALSE;
            } else {
                menu_play_sound_cue(KF_MENU_SOUND_CONFIRM);
            }
        }

        for (frame = 0; frame < 2; frame++) {
            menu_frame_begin();
            if (menu.list.entry_count != 0) {
                menu_update_item_preview(selected_item);
            }
            menu_render_list(&menu.list, KF_MENU_LIST_SHOP_BUY);
            menu_present_frame();
        }
    }

    menu_release_item_model();
    if (result != KF_MENU_RESULT_CANCELLED) {
        cost = menu.prices[menu.list.selected_index] * menu_item_quantity;
        player_state.gold -= cost;
        counters[result] += (u8)menu_item_quantity;
    }
}

ADDRESS(0x8001d9cc, 0x74)
void menu_fill_item_counts_and_prices(const u8 *source, u8 *counts, s32 *prices,
    const u8 *indices, s32 first, s32 last, s32 group)
{
    const u16 *page;

    if (first < last) {
        const u16 (*pages)[KF_ITEM_ID_COUNT] = menu_item_code_primary;
        page = pages[group];
        do {
            *counts++ = source[*indices];
            *prices++ = page[*indices++];
            first++;
        } while (first < last);
    }
}

ADDRESS(0x8001da40, 0x2a0)
void menu_item_sell_controller(s32 kind)
{
    KfItemMenuList menu;
    KfMenuGlyphRow rows[KF_ITEM_ID_COUNT];
    u8 values[KF_ITEM_ID_COUNT];
    s32 prices[KF_ITEM_ID_COUNT];
    u8 indices[KF_ITEM_ID_COUNT];
    b32 confirmed = KF_FALSE;
    s32 result = KF_MENU_RESULT_PENDING;
    s32 count;
    s32 frame;
    s32 payment;
    u8 selected_item;

    count = menu_collect_available_item_rows(game_counter_bytes, rows, values, indices, 0, KF_ITEM_ID_LAST);
    menu_fill_item_prices(prices, indices, 0, count, kind);
    menu_list_init(&menu.list, KF_MENU_WINDOW_SHOP, 1);
    menu.list.entry_count = count;
    menu.rows = rows;
    menu.values = values;
    menu.prices = prices;
    menu.list.glyphs_per_entry = 12;

    if (menu.list.entry_count != 0
            && menu_load_item_model(indices[menu.list.selected_index]) != 0) {
        return;
            }

    for (;;) {
        if (confirmed || result != KF_MENU_RESULT_PENDING) {
            input_wait_release();
        }

        if (confirmed == KF_TRUE) {
            result = menu_preview_choice(&menu.list, KF_MENU_CONFIRM_SELL, KF_MENU_LIST_SHOP_SELL, selected_item);
            if (result == KF_MENU_RESULT_CANCELLED) {
                result = KF_MENU_RESULT_PENDING;
            } else {
                result = selected_item;
            }
        }
        if (result != KF_MENU_RESULT_PENDING) {
            break;
        }

        menu_update_list_input(&menu.list, indices, &confirmed, &result);
        selected_item = indices[menu.list.selected_index];
        if (confirmed == KF_TRUE) {
            if (values[menu.list.selected_index] < menu_item_quantity) {
                menu_play_sound_cue(KF_MENU_SOUND_CANCEL);
                confirmed = KF_FALSE;
            } else {
                menu_play_sound_cue(KF_MENU_SOUND_CONFIRM);
            }
        }

        for (frame = 0; frame < 2; frame++) {
            menu_frame_begin();
            if (menu.list.entry_count != 0) {
                menu_update_item_preview(selected_item);
            }
            menu_render_list(&menu.list, KF_MENU_LIST_SHOP_SELL);
            menu_present_frame();
        }
    }

    menu_release_item_model();
    if (result != KF_MENU_RESULT_CANCELLED) {
        payment = menu.prices[menu.list.selected_index] * menu_item_quantity;
        player_state.gold += payment;
        game_counter_bytes[result] -= (u8)menu_item_quantity;
    }
}

ADDRESS(0x8001dce0, 0x54)
void menu_fill_item_prices(s32 *prices, const u8 *indices, s32 first, s32 last, s32 group)
{
    const u16 *page;

    if (first < last) {
        const u16 (*pages)[KF_ITEM_ID_COUNT] = menu_item_code_secondary;
        page = pages[group];
        do {
            *prices++ = page[*indices++];
            first++;
        } while (first < last);
    }
}

ADDRESS(0x8001dd34, 0x228)
s32 menu_choose_inventory_item(void)
{
    KfItemMenuList menu;
    KfMenuGlyphRow rows[KF_ITEM_ID_COUNT];
    u8 values[KF_ITEM_ID_COUNT];
    u8 indices[KF_ITEM_ID_COUNT];
    b32 confirmed = KF_FALSE;
    s32 result = KF_MENU_RESULT_PENDING;
    s32 count;
    u8 selected_item;
    s32 frame;

    menu_enter_display_state(1);
    count = menu_collect_masked_item_rows(game_counter_bytes, rows, values, indices, 0, KF_ITEM_ID_LAST);
    menu_list_init(&menu.list, KF_MENU_WINDOW_TRADE, 0);
    menu.list.entry_count = count;
    menu.rows = rows;
    menu.values = values;
    menu.list.glyphs_per_entry = 12;
    selected_item = indices[menu.list.selected_index];
    if (menu_load_item_model(selected_item) != 0) {
        return KF_MENU_RESULT_CANCELLED;
    }

    for (frame = 0; frame < 2; frame++) {
        menu_frame_begin();
        if (menu.list.entry_count != 0) {
            menu_update_item_preview(selected_item);
        }
        menu_render_list(&menu.list, KF_MENU_LIST_INVENTORY);
        menu_present_frame();
    }
    menu_play_sound_cue(KF_MENU_SOUND_CURSOR);
    input_wait_release();

    for (;;) {
        if (confirmed || result != KF_MENU_RESULT_PENDING) {
            input_wait_release();
        }

        if (confirmed == KF_TRUE) {
            result = menu_preview_choice(&menu.list, KF_MENU_CONFIRM_LABEL_8, KF_MENU_LIST_INVENTORY, selected_item);
            if (result == KF_MENU_RESULT_CANCELLED) {
                result = KF_MENU_RESULT_PENDING;
            } else {
                result = selected_item;
            }
        }
        if (result != KF_MENU_RESULT_PENDING) {
            break;
        }

        menu_update_list_input(&menu.list, indices, &confirmed, &result);
        selected_item = indices[menu.list.selected_index];
        if (confirmed == KF_TRUE) {
            menu_play_sound_cue(KF_MENU_SOUND_CONFIRM);
        }
        for (frame = 0; frame < 2; frame++) {
            menu_frame_begin();
            if (menu.list.entry_count != 0) {
                menu_update_item_preview(selected_item);
            }
            menu_render_list(&menu.list, KF_MENU_LIST_INVENTORY);
            menu_present_frame();
        }
    }

    menu_release_item_model();
    menu_exit_display_state(KF_FALSE);
    return result;
}
ADDRESS(0x8001df5c, 0x394)
void menu_item_trade_controller(void)
{
    KfItemMenuList menu;
    KfMenuGlyphRow rows[40];
    u8 values[40];
    s32 prices[40];
    u8 indices[40];
    u8 *counters = game_counter_bytes;
    b32 confirmed = KF_FALSE;
    s32 result = KF_MENU_RESULT_PENDING;
    s32 count;
    s32 frame;
    s32 cost;
    u8 selected_item;

    menu_enter_display_state(1);
    count = menu_collect_masked_item_rows(menu_item_mask_pages[4], rows, values, indices,
        0, KF_ITEM_ID_LAST);
    menu_fill_item_counts_and_prices(counters, values, prices, indices, 0, count, 4);
    menu_list_init(&menu.list, KF_MENU_WINDOW_TRADE, 1);
    menu.list.entry_count = count;
    menu.rows = rows;
    menu.values = values;
    menu.prices = prices;
    menu.list.glyphs_per_entry = 12;

    selected_item = indices[menu.list.selected_index];
    if (menu_load_item_model(selected_item) != 0) {
        return;
    }

    for (frame = 0; frame < 2; frame++) {
        menu_frame_begin();
        if (menu.list.entry_count != 0) {
            menu_update_item_preview(selected_item);
        }
        menu_render_list(&menu.list, KF_MENU_LIST_TRADE);
        menu_present_frame();
    }
    menu_play_sound_cue(KF_MENU_SOUND_CURSOR);
    input_wait_release();

    for (;;) {
        if (confirmed || result != KF_MENU_RESULT_PENDING) {
            input_wait_release();
        }

        if (confirmed == KF_TRUE) {
            result = menu_preview_choice(&menu.list, KF_MENU_CONFIRM_LABEL_9, KF_MENU_LIST_TRADE, selected_item);
            if (result == KF_MENU_RESULT_CANCELLED) {
                result = KF_MENU_RESULT_PENDING;
            } else {
                result = selected_item;
            }
        }
        if (result != KF_MENU_RESULT_PENDING) {
            break;
        }

        menu_update_list_input(&menu.list, indices, &confirmed, &result);
        selected_item = indices[menu.list.selected_index];
        if (confirmed == KF_TRUE) {
            cost = menu.prices[menu.list.selected_index] * menu_item_quantity;
            if (counters[KF_ENUM_ENCODE(u8, KF_OBJECT_96)] < cost
                    || (selected_item == 117
                        ? counters[KF_ENUM_ENCODE(u8, KF_OBJECT_117)] + menu_item_quantity * 10 >= 100
                        : counters[selected_item] + menu_item_quantity >= 100)) {
                menu_play_sound_cue(KF_MENU_SOUND_CANCEL);
                confirmed = KF_FALSE;
            } else {
                menu_play_sound_cue(KF_MENU_SOUND_CONFIRM);
            }
        }

        for (frame = 0; frame < 2; frame++) {
            menu_frame_begin();
            if (menu.list.entry_count != 0) {
                menu_update_item_preview(selected_item);
            }
            menu_render_list(&menu.list, KF_MENU_LIST_TRADE);
            menu_present_frame();
        }
    }

    menu_release_item_model();
    if (result != KF_MENU_RESULT_CANCELLED) {
        cost = menu.prices[menu.list.selected_index] * menu_item_quantity;
        counters[KF_ENUM_ENCODE(u8, KF_OBJECT_96)] -= cost;
        if (result == KF_ENUM_ENCODE(s32, KF_OBJECT_117)) {
            counters[KF_ENUM_ENCODE(u8, KF_OBJECT_117)] += menu_item_quantity * 10;
        } else {
            counters[result] += (u8)menu_item_quantity;
        }
    }
    menu_exit_display_state(KF_FALSE);
}
ADDRESS(0x8001e2f0, 0x16c)
void menu_item_stock_choice_controller(void)
{
    s32 cursor = 0;
    b32 confirmed = KF_FALSE;
    s32 result = KF_MENU_RESULT_PENDING;
    s32 selection = KF_MENU_SELECTION_NONE;
    s32 frame;

    menu_enter_display_state(1);
    for (frame = 0; frame < 2; frame++) {
        menu_frame_begin();
        menu_draw_window(KF_MENU_WINDOW_STOCK, KF_MENU_STOCK_ROW_COUNT, cursor, confirmed);
        menu_present_frame();
    }
    menu_play_sound_cue(KF_MENU_SOUND_CURSOR);
    input_wait_release();
    for (;;) {
        if (selection != KF_MENU_SELECTION_NONE || result != KF_MENU_RESULT_PENDING) {
            input_wait_release();
        }

        switch (selection) {
        case KF_MENU_STOCK_BUY_ROW:
            menu_buy_masked_stock_items();
            break;
        case KF_MENU_STOCK_ROW_1:
            menu_buy_owned_items();
            break;
        }

        if (result != KF_MENU_RESULT_PENDING) {
            break;
        }
        cursor = menu_poll_choice_input(cursor, KF_MENU_STOCK_RETURN_ROW,
            &selection, &confirmed, &result);
        for (frame = 0; frame < 2; frame++) {
            menu_frame_begin();
            menu_draw_window(KF_MENU_WINDOW_STOCK, KF_MENU_STOCK_ROW_COUNT, cursor, confirmed);
            menu_present_frame();
        }
    }
    menu_exit_display_state(KF_FALSE);
}

ADDRESS(0x8001e45c, 0x2d8)
void menu_buy_masked_stock_items(void)
{
    KfItemMenuList menu;
    KfMenuGlyphRow rows[40];
    u8 values[40];
    s32 prices[40];
    u8 indices[40];
    u8 *counters = game_counter_bytes;
    b32 confirmed = KF_FALSE;
    s32 result = KF_MENU_RESULT_PENDING;
    s32 count;
    s32 frame;
    s32 cost;
    u8 selected_item;

    count = menu_collect_masked_item_rows(menu_item_mask_pages[5], rows, values, indices,
        0, KF_ITEM_ID_LAST);
    menu_fill_item_counts_and_prices(counters, values, prices, indices, 0, count, 5);
    menu_list_init(&menu.list, KF_MENU_WINDOW_STOCK, 0);
    menu.list.entry_count = count;
    menu.rows = rows;
    menu.values = values;
    menu.prices = prices;
    menu.list.glyphs_per_entry = 12;

    if (menu_load_item_model(indices[menu.list.selected_index]) != 0) {
        return;
    }

    for (;;) {
        if (confirmed || result != KF_MENU_RESULT_PENDING) {
            input_wait_release();
        }

        if (confirmed == KF_TRUE) {
            result = menu_preview_choice(&menu.list, KF_MENU_CONFIRM_BUY, KF_MENU_LIST_STOCK_BUY, selected_item);
            if (result == KF_MENU_RESULT_CANCELLED) {
                result = KF_MENU_RESULT_PENDING;
            } else {
                result = selected_item;
            }
        }
        if (result != KF_MENU_RESULT_PENDING) {
            break;
        }

        menu_update_list_input(&menu.list, indices, &confirmed, &result);
        selected_item = indices[menu.list.selected_index];
        if (confirmed == KF_TRUE) {
            if (player_state.gold < (u32)(menu.prices[menu.list.selected_index]
                    * menu_item_quantity)
                    || counters[selected_item] + menu_item_quantity >= 100) {
                menu_play_sound_cue(KF_MENU_SOUND_CANCEL);
                confirmed = KF_FALSE;
            } else {
                menu_play_sound_cue(KF_MENU_SOUND_CONFIRM);
            }
        }

        for (frame = 0; frame < 2; frame++) {
            menu_frame_begin();
            if (menu.list.entry_count != 0) {
                menu_update_item_preview(selected_item);
            }
            menu_render_list(&menu.list, KF_MENU_LIST_STOCK_BUY);
            menu_present_frame();
        }
    }

    menu_release_item_model();
    if (result != KF_MENU_RESULT_CANCELLED) {
        cost = menu.prices[menu.list.selected_index] * menu_item_quantity;
        player_state.gold -= cost;
        counters[result] += (u8)menu_item_quantity;
    }
}

ADDRESS(0x8001e734, 0x2e0)
void menu_buy_owned_items(void)
{
    KfItemMenuList menu;
    KfMenuGlyphRow rows[40];
    u8 values[40];
    s32 prices[40];
    u8 indices[40];
    u8 *counters = game_counter_bytes;
    b32 confirmed = KF_FALSE;
    s32 result = KF_MENU_RESULT_PENDING;
    s32 count;
    s32 frame;
    s32 cost;
    u8 selected_item;

    count = menu_collect_masked_item_rows(counters, rows, values, indices, KF_ITEM_KEY_FIRST,
        KF_ITEM_KEY_LAST);
    menu_fill_item_prices(prices, indices, 0, count, 4);
    menu_list_init(&menu.list, KF_MENU_WINDOW_STOCK, 1);
    menu.list.entry_count = count;
    menu.rows = rows;
    menu.values = values;
    menu.prices = prices;
    menu.list.glyphs_per_entry = 12;

    if (menu_load_item_model(indices[menu.list.selected_index]) != 0) {
        return;
    }

    for (;;) {
        if (confirmed || result != KF_MENU_RESULT_PENDING) {
            input_wait_release();
        }

        if (confirmed == KF_TRUE) {
            result = menu_preview_choice(&menu.list, KF_MENU_CONFIRM_LABEL_10, KF_MENU_LIST_STOCK_OWNED, selected_item);
            if (result == KF_MENU_RESULT_CANCELLED) {
                result = KF_MENU_RESULT_PENDING;
            } else {
                result = selected_item;
            }
        }
        if (result != KF_MENU_RESULT_PENDING) {
            break;
        }

        menu_update_list_input(&menu.list, indices, &confirmed, &result);
        selected_item = indices[menu.list.selected_index];
        if (confirmed == KF_TRUE) {
            if (player_state.gold < (u32)(menu.prices[menu.list.selected_index]
                    * menu_item_quantity)
                    || counters[selected_item] + menu_item_quantity >= 100) {
                menu_play_sound_cue(KF_MENU_SOUND_CANCEL);
                confirmed = KF_FALSE;
            } else {
                menu_play_sound_cue(KF_MENU_SOUND_CONFIRM);
            }
        }

        for (frame = 0; frame < 2; frame++) {
            menu_frame_begin();
            if (menu.list.entry_count != 0) {
                menu_update_item_preview(selected_item);
            }
            menu_render_list(&menu.list, KF_MENU_LIST_STOCK_OWNED);
            menu_present_frame();
        }
    }

    menu_release_item_model();
    if (result != KF_MENU_RESULT_CANCELLED) {
        cost = menu.prices[menu.list.selected_index] * menu_item_quantity;
        player_state.gold -= cost;
        counters[result] += (u8)menu_item_quantity;
    }
}
ADDRESS(0x8001ea14, 0x15c)
s32 menu_poll_choice_input(s32 index, s32 last, s32 *selection, b32 *confirmed,
    s32 *cancelled)
{
    u32 buttons;

    *selection = KF_MENU_SELECTION_NONE;
    *confirmed = KF_FALSE;
    input_wait_brief_release();
    buttons = input_read_mark_active();
    if (buttons & PADLup) {
        menu_cursor_animation_direction = KF_MENU_CURSOR_ANIMATION_FORWARD;
        menu_play_sound_cue(KF_MENU_SOUND_CURSOR);
        if (index != 0) {
            index--;
        } else {
            index = last;
        }
    } else if (buttons & PADLdown) {
        menu_cursor_animation_direction = KF_MENU_CURSOR_ANIMATION_FORWARD;
        menu_play_sound_cue(KF_MENU_SOUND_CURSOR);
        if (index != last) {
            index++;
        } else {
            index = 0;
        }
    } else if (buttons & PADRright) {
        menu_play_sound_cue(KF_MENU_SOUND_CONFIRM);
        *confirmed = KF_TRUE;
        if (index < last) {
            *selection = index;
        } else {
            *cancelled = KF_MENU_RESULT_CANCELLED;
        }
    } else if (buttons & PADRdown) {
        menu_play_sound_cue(KF_MENU_SOUND_CANCEL);
        *cancelled = KF_MENU_RESULT_CANCELLED;
    }
    return index;
}

ADDRESS(0x8001eb70, 0x6c0)
u32 menu_update_list_input(KfMenuList *list, const u8 *item_ids,
    b32 *confirmed, s32 *result)
{
    u32 buttons;

    *confirmed = KF_FALSE;
    input_wait_brief_release();
    buttons = input_read_mark_active();

    if (list->entry_count == 0) {
        if (buttons != 0) {
            menu_play_sound_cue(KF_MENU_SOUND_CANCEL);
            *result = KF_MENU_RESULT_CANCELLED;
        }
    } else if (buttons & PADLup) {
        menu_play_sound_cue(KF_MENU_SOUND_CURSOR);
        if (list->selected_index != 0) {
            list->selected_index--;
            if (list->cursor_row == 0) {
                list->scroll_offset--;
            } else {
                list->cursor_row--;
            }
        } else {
            list->selected_index = list->entry_count - 1;
            if (list->entry_count < list->visible_rows) {
                list->scroll_offset = 0;
                list->cursor_row = list->entry_count - 1;
            } else {
                list->scroll_offset = list->entry_count - list->visible_rows;
                list->cursor_row = list->visible_rows - 1;
            }
        }
        if (item_ids != NULL && menu_load_item_model(item_ids[list->selected_index]) != 0) {
            *result = KF_MENU_RESULT_CANCELLED;
        }
    } else if (buttons & PADLdown) {
        menu_play_sound_cue(KF_MENU_SOUND_CURSOR);
        if (list->selected_index < list->entry_count - 1) {
            list->selected_index++;
            if (list->cursor_row == list->visible_rows - 1) {
                list->scroll_offset++;
            } else {
                list->cursor_row++;
            }
        } else {
            list->selected_index = 0;
            list->scroll_offset = 0;
            list->cursor_row = 0;
        }
        if (item_ids != NULL && menu_load_item_model(item_ids[list->selected_index]) != 0) {
            *result = KF_MENU_RESULT_CANCELLED;
        }
    } else if (buttons & PADLright) {
        if (menu_item_quantity < 99) {
            menu_play_sound_cue(KF_MENU_SOUND_CURSOR);
            menu_item_quantity++;
        }
    } else if (buttons & PADLleft) {
        if (menu_item_quantity > 1) {
            menu_play_sound_cue(KF_MENU_SOUND_CURSOR);
            menu_item_quantity--;
        }
    } else if (buttons & PADRright) {
        *confirmed = KF_TRUE;
    } else if (buttons & PADRdown) {
        menu_play_sound_cue(KF_MENU_SOUND_CANCEL);
        *result = KF_MENU_RESULT_CANCELLED;
    }

    if (buttons & PADselect) {
        if (buttons & PADR1) {
            input_press_pending = KF_FALSE;
            menu_item_preview_rotation.vx += 16;
        }
        if (buttons & PADR2) {
            input_press_pending = KF_FALSE;
            menu_item_preview_rotation.vx -= 16;
        }
        if (buttons & PADL1) {
            input_press_pending = KF_FALSE;
            menu_item_preview_rotation.vz += 16;
        }
        if (buttons & PADL2) {
            input_press_pending = KF_FALSE;
            menu_item_preview_rotation.vz -= 16;
        }
        if (buttons & PADRup) {
            input_press_pending = KF_FALSE;
            menu_item_preview_rotation_step++;
        }
        if (buttons & PADRleft) {
            input_press_pending = KF_FALSE;
            menu_item_preview_rotation_step--;
        }
    }

    if (buttons & PADstart) {
        if (buttons & PADR1) {
            input_press_pending = KF_FALSE;
            menu_item_preview_translation.vx += 16;
        }
        if (buttons & PADR2) {
            input_press_pending = KF_FALSE;
            menu_item_preview_translation.vx -= 16;
        }
        if (buttons & PADL1) {
            input_press_pending = KF_FALSE;
            menu_item_preview_translation.vy += 16;
        }
        if (buttons & PADL2) {
            input_press_pending = KF_FALSE;
            menu_item_preview_translation.vy -= 16;
        }
        if (buttons & PADRup) {
            input_press_pending = KF_FALSE;
            menu_item_preview_translation.vz += 16;
        }
        if (buttons & PADRleft) {
            if (menu_item_preview_translation.vz > 500) {
                menu_item_preview_translation.vz -= 16;
            }
            input_press_pending = KF_FALSE;
        }
    }

    return buttons;
}

ADDRESS(0x8001f230, 0x91c)
void menu_draw_player_status(void)
{
    KfMenuGlyphString heading;
    KfMenuGlyphString amount;
    s32 row_step = 23;

    heading.position.x = 168;
    heading.position.y = 30;
    menu_copy_prefix8(heading.glyphs.codes, menu_header_labels[0].codes);
    menu_draw_string(&menu_sprite_defs[KF_MENU_SPRITE_GLYPH_ATLAS], &heading);
    amount.position.x = heading.position.x + 77;
    amount.position.y = heading.position.y;
    menu_format_number(player_state.experience, 7, KF_FORMAT_PAD_SPACES,
        KF_MENU_FORMAT_STYLE_PLAIN, amount.glyphs.codes);
    menu_draw_number(&menu_sprite_defs[KF_MENU_SPRITE_NUMBER_ATLAS], &amount);

    heading.position.y += row_step;
    menu_copy_prefix8(heading.glyphs.codes, menu_header_labels[1].codes);
    menu_draw_string(&menu_sprite_defs[KF_MENU_SPRITE_GLYPH_ATLAS], &heading);
    amount.position.y += row_step;
    menu_format_number(player_state.level, 7, KF_FORMAT_PAD_SPACES,
        KF_MENU_FORMAT_STYLE_PLAIN, amount.glyphs.codes);
    menu_draw_number(&menu_sprite_defs[KF_MENU_SPRITE_NUMBER_ATLAS], &amount);

    heading.glyphs.codes[0] = 227;
    heading.glyphs.codes[1] = 229;
    heading.glyphs.codes[2] = KF_MENU_TEXT_END;
    heading.position.y += row_step;
    menu_draw_string(&menu_sprite_defs[KF_MENU_SPRITE_GLYPH_ATLAS], &heading);
    amount.position.x = heading.position.x + 63;
    amount.position.y += row_step;
    menu_format_number(player_state.vitals.current_hp, 4, KF_FORMAT_PAD_SPACES,
        KF_MENU_FORMAT_STYLE_PLAIN, amount.glyphs.codes);
    menu_draw_number(&menu_sprite_defs[KF_MENU_SPRITE_NUMBER_ATLAS], &amount);
    amount.glyphs.codes[0] = 20;
    amount.glyphs.codes[1] = KF_MENU_TEXT_END;
    amount.position.x += 28;
    menu_draw_number(&menu_sprite_defs[KF_MENU_SPRITE_NUMBER_ATLAS], &amount);
    amount.position.x += 7;
    menu_format_number(player_state.vitals.maximum_hp, 4, KF_FORMAT_PAD_SPACES,
        KF_MENU_FORMAT_STYLE_PLAIN, amount.glyphs.codes);
    menu_draw_number(&menu_sprite_defs[KF_MENU_SPRITE_NUMBER_ATLAS], &amount);

    heading.glyphs.codes[0] = 228;
    heading.position.y += row_step;
    menu_draw_string(&menu_sprite_defs[KF_MENU_SPRITE_GLYPH_ATLAS], &heading);
    amount.position.x = heading.position.x + 63;
    amount.position.y += row_step;
    menu_format_number(player_state.vitals.current_mp, 4, KF_FORMAT_PAD_SPACES,
        KF_MENU_FORMAT_STYLE_PLAIN, amount.glyphs.codes);
    menu_draw_number(&menu_sprite_defs[KF_MENU_SPRITE_NUMBER_ATLAS], &amount);
    amount.glyphs.codes[0] = 20;
    amount.glyphs.codes[1] = KF_MENU_TEXT_END;
    amount.position.x += 28;
    menu_draw_number(&menu_sprite_defs[KF_MENU_SPRITE_NUMBER_ATLAS], &amount);
    amount.position.x += 7;
    menu_format_number(player_state.vitals.maximum_mp, 4, KF_FORMAT_PAD_SPACES,
        KF_MENU_FORMAT_STYLE_PLAIN, amount.glyphs.codes);
    menu_draw_number(&menu_sprite_defs[KF_MENU_SPRITE_NUMBER_ATLAS], &amount);

    heading.glyphs.codes[0] = 140;
    heading.glyphs.codes[1] = 139;
    heading.glyphs.codes[2] = KF_MENU_TEXT_END;
    heading.position.y += row_step;
    menu_draw_string(&menu_sprite_defs[KF_MENU_SPRITE_GLYPH_ATLAS], &heading);
    amount.position.x = heading.position.x + 84;
    amount.position.y = heading.position.y;
    menu_format_number(player_state.physical_power, 6, KF_FORMAT_PAD_SPACES,
        KF_MENU_FORMAT_STYLE_PLAIN, amount.glyphs.codes);
    menu_draw_number(&menu_sprite_defs[KF_MENU_SPRITE_NUMBER_ATLAS], &amount);

    heading.glyphs.codes[0] = 120;
    heading.position.y += row_step;
    menu_draw_string(&menu_sprite_defs[KF_MENU_SPRITE_GLYPH_ATLAS], &heading);
    amount.position.x = heading.position.x + 84;
    amount.position.y = heading.position.y;
    menu_format_number(player_state.magic, 6, KF_FORMAT_PAD_SPACES,
        KF_MENU_FORMAT_STYLE_PLAIN, amount.glyphs.codes);
    menu_draw_number(&menu_sprite_defs[KF_MENU_SPRITE_NUMBER_ATLAS], &amount);

    heading.position.y += row_step;
    menu_copy_prefix8(heading.glyphs.codes, menu_header_labels[4].codes);
    menu_draw_string(&menu_sprite_defs[KF_MENU_SPRITE_GLYPH_ATLAS], &heading);
    amount.position.x = heading.position.x + 56;
    amount.position.y += row_step;
    if (player_state.paralysis_timer != 0) {
        menu_copy_prefix12(amount.glyphs.codes, menu_header_labels[5].codes);
    } else if (player_state.curse_strength != 0) {
        menu_copy_prefix12(amount.glyphs.codes, menu_header_labels[6].codes);
    } else if (player_state.slow_timer != 0) {
        menu_copy_prefix12(amount.glyphs.codes, menu_header_labels[7].codes);
    } else if (player_state.poison_timer != 0) {
        menu_copy_prefix12(amount.glyphs.codes, menu_header_labels[8].codes);
    } else if (player_state.darkness_phase != 0) {
        menu_copy_prefix12(amount.glyphs.codes, menu_header_labels[9].codes);
    } else {
        menu_copy_prefix12(amount.glyphs.codes, menu_header_labels[10].codes);
    }
    menu_draw_string(&menu_sprite_defs[KF_MENU_SPRITE_GLYPH_ATLAS], &amount);

    heading.position.y += row_step;
    menu_copy_prefix10(heading.glyphs.codes, menu_header_labels[11].codes);
    menu_draw_string(&menu_sprite_defs[KF_MENU_SPRITE_GLYPH_ATLAS], &heading);
    amount.position.x = heading.position.x + 77;
    amount.position.y += row_step;
    menu_format_number(player_state.gold, 7, KF_FORMAT_PAD_SPACES,
        KF_MENU_FORMAT_STYLE_PLAIN, amount.glyphs.codes);
    menu_draw_number(&menu_sprite_defs[KF_MENU_SPRITE_NUMBER_ATLAS], &amount);
    menu_draw_nine_slice_panel(161, 14, 140, 205, 1, 2);
}

ADDRESS(0x8001fb4c, 0xa74)
void menu_draw_combat_attributes(void)
{
    KfMenuGlyphString label;
    KfMenuGlyphString number;

    label.position.x = 24;
    label.position.y = 28;
    menu_copy_prefix8(label.glyphs.codes, menu_header_labels[2].codes);
    menu_draw_string(&menu_sprite_defs[KF_MENU_SPRITE_GLYPH_ATLAS], &label);

    label.glyphs.codes[0] = 255;
    label.glyphs.codes[1] = 263;
    label.glyphs.codes[2] = 106;
    label.glyphs.codes[3] = KF_MENU_TEXT_END;
    label.position.y += 18;
    menu_draw_string(&menu_sprite_defs[KF_MENU_SPRITE_GLYPH_ATLAS], &label);
    number.position.x = label.position.x + 84;
    number.position.y = label.position.y;
    menu_format_number(player_state.attack_components[0], 6, KF_FORMAT_PAD_SPACES,
        KF_MENU_FORMAT_STYLE_PLAIN,
        number.glyphs.codes);
    menu_draw_number(&menu_sprite_defs[KF_MENU_SPRITE_NUMBER_ATLAS], &number);

    label.glyphs.codes[1] = 264;
    label.glyphs.codes[2] = 81;
    label.position.y += 18;
    menu_draw_string(&menu_sprite_defs[KF_MENU_SPRITE_GLYPH_ATLAS], &label);
    number.position.y = label.position.y;
    menu_format_number(player_state.attack_components[1], 6, KF_FORMAT_PAD_SPACES,
        KF_MENU_FORMAT_STYLE_PLAIN,
        number.glyphs.codes);
    menu_draw_number(&menu_sprite_defs[KF_MENU_SPRITE_NUMBER_ATLAS], &number);

    label.glyphs.codes[1] = 265;
    label.glyphs.codes[2] = 76;
    label.position.y += 18;
    menu_draw_string(&menu_sprite_defs[KF_MENU_SPRITE_GLYPH_ATLAS], &label);
    number.position.y = label.position.y;
    menu_format_number(player_state.attack_components[2], 6, KF_FORMAT_PAD_SPACES,
        KF_MENU_FORMAT_STYLE_PLAIN,
        number.glyphs.codes);
    menu_draw_number(&menu_sprite_defs[KF_MENU_SPRITE_NUMBER_ATLAS], &number);

    label.glyphs.codes[1] = 266;
    label.glyphs.codes[2] = 88;
    label.glyphs.codes[3] = 120;
    label.glyphs.codes[4] = 121;
    label.glyphs.codes[5] = KF_MENU_TEXT_END;
    label.position.y += 18;
    menu_draw_string(&menu_sprite_defs[KF_MENU_SPRITE_GLYPH_ATLAS], &label);
    number.position.y = label.position.y;
    menu_format_number(player_state.attack_components[3], 6, KF_FORMAT_PAD_SPACES,
        KF_MENU_FORMAT_STYLE_PLAIN,
        number.glyphs.codes);
    menu_draw_number(&menu_sprite_defs[KF_MENU_SPRITE_NUMBER_ATLAS], &number);

    label.glyphs.codes[1] = 170;
    label.position.y += 18;
    menu_draw_string(&menu_sprite_defs[KF_MENU_SPRITE_GLYPH_ATLAS], &label);
    number.position.y = label.position.y;
    menu_format_number(player_state.attack_components[4], 6, KF_FORMAT_PAD_SPACES,
        KF_MENU_FORMAT_STYLE_PLAIN,
        number.glyphs.codes);
    menu_draw_number(&menu_sprite_defs[KF_MENU_SPRITE_NUMBER_ATLAS], &number);

    label.glyphs.codes[1] = 187;
    label.position.y += 18;
    menu_draw_string(&menu_sprite_defs[KF_MENU_SPRITE_GLYPH_ATLAS], &label);
    number.position.y = label.position.y;
    menu_format_number(player_state.attack_components[5], 6, KF_FORMAT_PAD_SPACES,
        KF_MENU_FORMAT_STYLE_PLAIN,
        number.glyphs.codes);
    menu_draw_number(&menu_sprite_defs[KF_MENU_SPRITE_NUMBER_ATLAS], &number);

    label.glyphs.codes[1] = 188;
    label.position.y += 18;
    menu_draw_string(&menu_sprite_defs[KF_MENU_SPRITE_GLYPH_ATLAS], &label);
    number.position.y = label.position.y;
    menu_format_number(player_state.attack_components[6], 6, KF_FORMAT_PAD_SPACES,
        KF_MENU_FORMAT_STYLE_PLAIN,
        number.glyphs.codes);
    menu_draw_number(&menu_sprite_defs[KF_MENU_SPRITE_NUMBER_ATLAS], &number);

    label.glyphs.codes[1] = 141;
    label.position.y += 18;
    menu_draw_string(&menu_sprite_defs[KF_MENU_SPRITE_GLYPH_ATLAS], &label);
    number.position.y = label.position.y;
    menu_format_number(player_state.attack_components[7], 6, KF_FORMAT_PAD_SPACES,
        KF_MENU_FORMAT_STYLE_PLAIN,
        number.glyphs.codes);
    menu_draw_number(&menu_sprite_defs[KF_MENU_SPRITE_NUMBER_ATLAS], &number);

    label.position.x = 168;
    label.position.y = 28;
    menu_copy_prefix8(label.glyphs.codes, menu_header_labels[3].codes);
    menu_draw_string(&menu_sprite_defs[KF_MENU_SPRITE_GLYPH_ATLAS], &label);

    label.glyphs.codes[0] = 255;
    label.glyphs.codes[1] = 263;
    label.glyphs.codes[2] = 106;
    label.glyphs.codes[3] = KF_MENU_TEXT_END;
    label.position.y += 18;
    menu_draw_string(&menu_sprite_defs[KF_MENU_SPRITE_GLYPH_ATLAS], &label);
    number.position.x = label.position.x + 84;
    number.position.y = label.position.y;
    menu_format_number(player_state.combat_components[0], 6, KF_FORMAT_PAD_SPACES,
        KF_MENU_FORMAT_STYLE_PLAIN,
        number.glyphs.codes);
    menu_draw_number(&menu_sprite_defs[KF_MENU_SPRITE_NUMBER_ATLAS], &number);

    label.glyphs.codes[1] = 264;
    label.glyphs.codes[2] = 81;
    label.position.y += 18;
    menu_draw_string(&menu_sprite_defs[KF_MENU_SPRITE_GLYPH_ATLAS], &label);
    number.position.y = label.position.y;
    menu_format_number(player_state.combat_components[1], 6, KF_FORMAT_PAD_SPACES,
        KF_MENU_FORMAT_STYLE_PLAIN,
        number.glyphs.codes);
    menu_draw_number(&menu_sprite_defs[KF_MENU_SPRITE_NUMBER_ATLAS], &number);

    label.glyphs.codes[1] = 265;
    label.glyphs.codes[2] = 76;
    label.position.y += 18;
    menu_draw_string(&menu_sprite_defs[KF_MENU_SPRITE_GLYPH_ATLAS], &label);
    number.position.y = label.position.y;
    menu_format_number(player_state.combat_components[2], 6, KF_FORMAT_PAD_SPACES,
        KF_MENU_FORMAT_STYLE_PLAIN,
        number.glyphs.codes);
    menu_draw_number(&menu_sprite_defs[KF_MENU_SPRITE_NUMBER_ATLAS], &number);

    label.glyphs.codes[1] = 136;
    label.glyphs.codes[2] = KF_MENU_TEXT_END;
    label.position.y += 18;
    menu_draw_string(&menu_sprite_defs[KF_MENU_SPRITE_GLYPH_ATLAS], &label);
    number.position.y = label.position.y;
    menu_format_number(player_state.combat_components[3], 6, KF_FORMAT_PAD_SPACES,
        KF_MENU_FORMAT_STYLE_PLAIN,
        number.glyphs.codes);
    menu_draw_number(&menu_sprite_defs[KF_MENU_SPRITE_NUMBER_ATLAS], &number);

    label.glyphs.codes[1] = 120;
    label.glyphs.codes[2] = 88;
    label.glyphs.codes[3] = 120;
    label.glyphs.codes[4] = 121;
    label.glyphs.codes[5] = KF_MENU_TEXT_END;
    label.position.y += 18;
    menu_draw_string(&menu_sprite_defs[KF_MENU_SPRITE_GLYPH_ATLAS], &label);
    number.position.y = label.position.y;
    menu_format_number(player_state.combat_components[4], 6, KF_FORMAT_PAD_SPACES,
        KF_MENU_FORMAT_STYLE_PLAIN,
        number.glyphs.codes);
    menu_draw_number(&menu_sprite_defs[KF_MENU_SPRITE_NUMBER_ATLAS], &number);

    label.glyphs.codes[1] = 170;
    label.position.y += 18;
    menu_draw_string(&menu_sprite_defs[KF_MENU_SPRITE_GLYPH_ATLAS], &label);
    number.position.y = label.position.y;
    menu_format_number(player_state.combat_components[5], 6, KF_FORMAT_PAD_SPACES,
        KF_MENU_FORMAT_STYLE_PLAIN,
        number.glyphs.codes);
    menu_draw_number(&menu_sprite_defs[KF_MENU_SPRITE_NUMBER_ATLAS], &number);

    label.glyphs.codes[1] = 187;
    label.position.y += 18;
    menu_draw_string(&menu_sprite_defs[KF_MENU_SPRITE_GLYPH_ATLAS], &label);
    number.position.y = label.position.y;
    menu_format_number(player_state.combat_components[6], 6, KF_FORMAT_PAD_SPACES,
        KF_MENU_FORMAT_STYLE_PLAIN,
        number.glyphs.codes);
    menu_draw_number(&menu_sprite_defs[KF_MENU_SPRITE_NUMBER_ATLAS], &number);

    label.glyphs.codes[1] = 188;
    label.position.y += 18;
    menu_draw_string(&menu_sprite_defs[KF_MENU_SPRITE_GLYPH_ATLAS], &label);
    number.position.y = label.position.y;
    menu_format_number(player_state.combat_components[7], 6, KF_FORMAT_PAD_SPACES,
        KF_MENU_FORMAT_STYLE_PLAIN,
        number.glyphs.codes);
    menu_draw_number(&menu_sprite_defs[KF_MENU_SPRITE_NUMBER_ATLAS], &number);

    label.glyphs.codes[1] = 141;
    label.position.y += 18;
    menu_draw_string(&menu_sprite_defs[KF_MENU_SPRITE_GLYPH_ATLAS], &label);
    number.position.y = label.position.y;
    menu_format_number(player_state.combat_components[8], 6, KF_FORMAT_PAD_SPACES,
        KF_MENU_FORMAT_STYLE_PLAIN,
        number.glyphs.codes);
    menu_draw_number(&menu_sprite_defs[KF_MENU_SPRITE_NUMBER_ATLAS], &number);

    menu_draw_nine_slice_panel(17, 14, 140, 205, 1, 2);
    menu_draw_nine_slice_panel(161, 14, 140, 205, 1, 2);
}

ADDRESS(0x800205c0, 0x120)
void menu_draw_options_rows(KfMenuGlyphString *left, KfMenuGlyphString *right,
    const KfPlayerOption *selected)
{
    s32 left_y = left->position.y;
    s32 right_y = right->position.y;
    s32 row;

    row = 0;
    do {
        if (*selected == KF_PLAYER_OPTION_ON) {
            menu_blit_sprite_fixed_clut(&menu_sprite_defs[KF_MENU_SPRITE_SELECTION_CURSOR], &left->position);
            menu_blit_sprite_translucent(&menu_sprite_defs[KF_MENU_SPRITE_OPTION_HIGHLIGHT], &left->position);
            menu_blit_sprite_translucent(&menu_sprite_defs[KF_MENU_SPRITE_OPTION_BACKGROUND],
                &right->position);
        } else {
            menu_blit_sprite_fixed_clut(&menu_sprite_defs[KF_MENU_SPRITE_SELECTION_CURSOR], &right->position);
            menu_blit_sprite_translucent(&menu_sprite_defs[KF_MENU_SPRITE_OPTION_BACKGROUND],
                &left->position);
            menu_blit_sprite_translucent(&menu_sprite_defs[KF_MENU_SPRITE_OPTION_HIGHLIGHT],
                &right->position);
        }
        menu_draw_string(&menu_sprite_defs[KF_MENU_SPRITE_GLYPH_ATLAS], left);
        menu_draw_string(&menu_sprite_defs[KF_MENU_SPRITE_GLYPH_ATLAS], right);
        left->position.y += 26;
        right->position.y += 26;
        selected++;
        row++;
    } while (row < 6);
    left->position.y = left_y;
    right->position.y = right_y;
}

ADDRESS(0x800206e0, 0x1f8)
s32 menu_preview_choice(const KfMenuList *list, KfMenuConfirmKind label_kind,
    KfMenuListMode render_mode, u8 item_id)
{
    KfMenuGlyphString labels[2];
    s32 choice = KF_MENU_CHOICE_ACCEPT;
    s32 result = KF_MENU_RESULT_PENDING;
    b32 confirmed;
    s32 frame;
    u32 buttons;

    labels[0].position.x = menu_window_layout(KF_MENU_WINDOW_SYSTEM)->rows[0].position.x;
    labels[0].position.y = menu_window_layout(KF_MENU_WINDOW_SYSTEM)->rows[0].position.y;
    labels[1].position.x = menu_window_layout(KF_MENU_WINDOW_SYSTEM)->rows[1].position.x;
    labels[1].position.y = menu_window_layout(KF_MENU_WINDOW_SYSTEM)->rows[1].position.y;

    if (label_kind == KF_MENU_CONFIRM_USE) {
        labels[0].glyphs.codes[0] = 114;
        labels[0].glyphs.codes[1] = 66;
        labels[0].glyphs.codes[2] = KF_MENU_TEXT_END;
    } else if (label_kind == KF_MENU_CONFIRM_DROP) {
        labels[0].glyphs.codes[0] = 117;
        labels[0].glyphs.codes[1] = 82;
        labels[0].glyphs.codes[2] = 106;
        labels[0].glyphs.codes[3] = KF_MENU_TEXT_END;
    } else if (label_kind == KF_MENU_CONFIRM_YES_NO) {
        labels[0].glyphs.codes[0] = 89;
        labels[0].glyphs.codes[1] = 65;
        labels[0].glyphs.codes[2] = KF_MENU_TEXT_END;
    } else if (label_kind == KF_MENU_CONFIRM_BUY) {
        labels[0].glyphs.codes[0] = 116;
        labels[0].glyphs.codes[1] = 66;
        labels[0].glyphs.codes[2] = KF_MENU_TEXT_END;
    } else if (label_kind == KF_MENU_CONFIRM_SELL) {
        labels[0].glyphs.codes[0] = 115;
        labels[0].glyphs.codes[1] = 106;
        labels[0].glyphs.codes[2] = KF_MENU_TEXT_END;
    } else if (label_kind == KF_MENU_CONFIRM_EQUIP) {
        labels[0].glyphs.codes[0] = 112;
        labels[0].glyphs.codes[1] = 113;
        labels[0].glyphs.codes[2] = KF_MENU_TEXT_END;
    } else if (label_kind == KF_MENU_CONFIRM_LOAD) {
        labels[0].glyphs.codes[0] = 44;
        labels[0].glyphs.codes[1] = 45;
        labels[0].glyphs.codes[2] = 0x1013;
        labels[0].glyphs.codes[3] = KF_MENU_TEXT_END;
    } else if (label_kind == KF_MENU_CONFIRM_SAVE) {
        labels[0].glyphs.codes[0] = 13;
        labels[0].glyphs.codes[1] = 45;
        labels[0].glyphs.codes[2] = 0x101b;
        labels[0].glyphs.codes[3] = KF_MENU_TEXT_END;
    } else if (label_kind == KF_MENU_CONFIRM_LABEL_8) {
        labels[0].glyphs.codes[0] = 0x104;
        labels[0].glyphs.codes[1] = 71;
        labels[0].glyphs.codes[2] = KF_MENU_TEXT_END;
    } else if (label_kind == KF_MENU_CONFIRM_LABEL_9) {
        labels[0].glyphs.codes[0] = 0x105;
        labels[0].glyphs.codes[1] = 0x106;
        labels[0].glyphs.codes[2] = KF_MENU_TEXT_END;
    } else {
        labels[0].glyphs.codes[0] = 207;
        labels[0].glyphs.codes[1] = 106;
        labels[0].glyphs.codes[2] = KF_MENU_TEXT_END;
    }

    if (label_kind == KF_MENU_CONFIRM_YES_NO) {
        labels[1].glyphs.codes[0] = 65;
        labels[1].glyphs.codes[1] = 65;
        labels[1].glyphs.codes[2] = 67;
        goto labels_ready;
    finished:
        input_wait_release();
        return result;
    } else {
        labels[1].glyphs.codes[0] = 99;
        labels[1].glyphs.codes[1] = 97;
        labels[1].glyphs.codes[2] = 106;
    }
labels_ready:
    labels[1].glyphs.codes[3] = KF_MENU_TEXT_END;

    for (;;) {
        if (result != KF_MENU_RESULT_PENDING) {
            goto finished;
        }
        input_wait_brief_release();
        buttons = input_read_mark_active();
        confirmed = KF_FALSE;
        if ((buttons & PADLup) || (buttons & PADLdown)) {
            menu_cursor_animation_direction = KF_MENU_CURSOR_ANIMATION_FORWARD;
            menu_play_sound_cue(KF_MENU_SOUND_CURSOR);
            if (choice != KF_MENU_CHOICE_ACCEPT) {
                choice = KF_MENU_CHOICE_ACCEPT;
            } else {
                choice = KF_MENU_CHOICE_DECLINE;
            }
        } else if (buttons & PADRright) {
            menu_play_sound_cue(KF_MENU_SOUND_CONFIRM);
            confirmed = KF_TRUE;
            result = -choice;
        } else if (buttons & PADRdown) {
            menu_play_sound_cue(KF_MENU_SOUND_CANCEL);
            result = KF_MENU_RESULT_CANCELLED;
        }

        for (frame = 0; frame < 2; frame++) {
            menu_frame_begin();
            menu_update_item_preview(item_id);
            menu_render_list(list, render_mode);
            menu_draw_two_option(&labels[0], &labels[1], choice, confirmed);
            menu_present_frame();
        }
    }
}

ADDRESS(0x800208d8, 0x108)
void menu_draw_window(KfMenuWindowKind window_kind, s32 count, s32 highlight,
    b32 confirmation)
{
    const KfMenuWindowLayout *layout = menu_window_layout(window_kind);
    const KfMenuGlyphString *row = &layout->rows[0];
    s32 index;
    /* Retail reserves an unreferenced 8-byte frame slot. */
    s16 frame_reserve[4];

    if (layout->title.position.x != 0) {
        menu_blit_sprite_translucent(&menu_sprite_defs[KF_MENU_SPRITE_PANEL_BACKGROUND], &layout->title.position);
        menu_draw_string(&menu_sprite_defs[KF_MENU_SPRITE_GLYPH_ATLAS], &layout->title);
    }
    if (count > 0) {
        index = 0;
        do {
            if (index == highlight && confirmation == KF_TRUE) {
                menu_blit_sprite_translucent(&menu_sprite_defs[KF_MENU_SPRITE_CONFIRMED_ROW], &row->position);
            } else {
                menu_blit_sprite_translucent(&menu_sprite_defs[KF_MENU_SPRITE_PANEL_BACKGROUND], &row->position);
            }
            if (index == highlight) {
                menu_blit_sprite(&menu_sprite_defs[KF_MENU_SPRITE_SELECTION_CURSOR], &row->position);
            }
            menu_draw_string(&menu_sprite_defs[KF_MENU_SPRITE_GLYPH_ATLAS], row);
            row++;
            index++;
        } while (index < count);
    }
}
ADDRESS(0x800209e0, 0xa98)
void menu_render_list(const KfMenuList *menu, KfMenuListMode render_mode)
{
    const KfMenuRenderList *view = (const KfMenuRenderList *)menu;
    const KfMenuList *list = &view->list;
    const KfMenuSpriteDef *sprite;
    const s16 *row_glyphs = view->row_glyphs;
    const s16 *detail_codes = (const s16 *)view->detail_rows;
    const s32 *number_values = view->number_values;
    const u8 *byte_values = view->byte_values;
    KfMenuGlyphString text;
    s16 *out_codes;
    s32 row;
    s32 code;
    s32 value;

    if (list->title.position.x != 0) {
        menu_blit_sprite_translucent(&menu_sprite_defs[KF_MENU_SPRITE_PANEL_BACKGROUND], &list->title.position);
        menu_draw_string(&menu_sprite_defs[KF_MENU_SPRITE_GLYPH_ATLAS], &list->title);
    }

    detail_codes += list->scroll_offset * 10;
    row_glyphs += list->scroll_offset * list->glyphs_per_entry;
    number_values += list->scroll_offset;
    byte_values += list->scroll_offset;

    {
        for (row = 0; row < list->visible_rows && row < list->entry_count; row++) {
            text.position.x = list->list_x + 5;
            text.position.y = list->list_y + 5 + row * 14;
            out_codes = text.glyphs.codes;
            for (code = 0; code < list->glyphs_per_entry; code++) {
                *out_codes++ = *row_glyphs++;
            }
            menu_draw_string(&menu_sprite_defs[KF_MENU_SPRITE_GLYPH_ATLAS], &text);

            if (render_mode == KF_MENU_LIST_EQUIPMENT) {
                text.position.x += 84;
                out_codes = text.glyphs.codes;
                for (code = 0; code < 10; code++) {
                    *out_codes++ = *detail_codes++;
                }
                *out_codes = KF_MENU_TEXT_END;
                menu_draw_string(&menu_sprite_defs[KF_MENU_SPRITE_GLYPH_ATLAS], &text);
            }

            /* One unsigned range test over SHOP_BUY..TRADE, then INVENTORY. */
            if ((u32)(KF_ENUM_ENCODE(s32, render_mode) - KF_ENUM_ENCODE(s32, KF_MENU_LIST_SHOP_BUY))
                    <= (u32)(KF_ENUM_ENCODE(s32, KF_MENU_LIST_TRADE)
                        - KF_ENUM_ENCODE(s32, KF_MENU_LIST_SHOP_BUY))
                && render_mode != KF_MENU_LIST_INVENTORY) {
                value = *number_values;
                text.position.x += 140;
                if (render_mode == KF_MENU_LIST_TRADE) {
                    menu_format_number(value, 6, KF_FORMAT_PAD_SPACES,
                        KF_MENU_FORMAT_STYLE_TRAILING_11, text.glyphs.codes);
                } else {
                    menu_format_number(value, 6, KF_FORMAT_PAD_SPACES,
                        KF_MENU_FORMAT_STYLE_TRAILING_13, text.glyphs.codes);
                }
                menu_draw_number(&menu_sprite_defs[KF_MENU_SPRITE_NUMBER_ATLAS], &text);
                number_values++;
            }

            if (render_mode == KF_MENU_LIST_USE_MAGIC || render_mode == KF_MENU_LIST_MAGIC_SHORTCUT) {
                value = *number_values++;
                if (value != KF_MENU_LIST_NO_NUMBER) {
                    text.position.x += 189;
                    menu_format_number(value, 3, KF_FORMAT_PAD_SPACES,
                        KF_MENU_FORMAT_STYLE_PAIR_15_16, text.glyphs.codes);
                    menu_draw_number(&menu_sprite_defs[KF_MENU_SPRITE_NUMBER_ATLAS], &text);
                }
            }

            if (render_mode == KF_MENU_LIST_CARD_LOAD || render_mode == KF_MENU_LIST_CARD_SAVE) {
                value = *number_values++;
                if (value != KF_MENU_LIST_NO_NUMBER) {
                    text.position.x += 98;
                    menu_format_number(value, 7, KF_FORMAT_PAD_SPACES,
                        KF_MENU_FORMAT_STYLE_TRIPLE_12_18_16, text.glyphs.codes);
                    menu_draw_number(&menu_sprite_defs[KF_MENU_SPRITE_NUMBER_ATLAS], &text);
                }
                value = *byte_values++;
                if (value != KF_MENU_LIST_NO_BYTE) {
                    text.position.x += 91;
                    menu_format_number(value, 3, KF_FORMAT_PAD_SPACES,
                        KF_MENU_FORMAT_STYLE_PAIR_14_17, text.glyphs.codes);
                    menu_draw_number(&menu_sprite_defs[KF_MENU_SPRITE_NUMBER_ATLAS], &text);
                }
            }

            if (render_mode != KF_MENU_LIST_USE_MAGIC && render_mode != KF_MENU_LIST_EQUIPMENT && render_mode != KF_MENU_LIST_MAGIC_SHORTCUT
                && render_mode != KF_MENU_LIST_CARD_LOAD && render_mode != KF_MENU_LIST_CARD_SAVE
                && render_mode != KF_MENU_LIST_ITEM_MAGIC) {
                value = *byte_values++;
                if (value != KF_MENU_LIST_NO_BYTE) {
                    text.position.x = list->list_x + 208;
                    menu_format_number(value, 2, KF_FORMAT_PAD_SPACES,
                        KF_MENU_FORMAT_STYLE_SINGLE_PREFIX, text.glyphs.codes);
                    menu_draw_number(&menu_sprite_defs[KF_MENU_SPRITE_NUMBER_ATLAS], &text);
                }
            }
            if (render_mode == KF_MENU_LIST_ITEM_MAGIC) {
                value = *byte_values++;
                if (value != KF_MENU_LIST_NO_BYTE) {
                    text.position.x += 203;
                    menu_format_number(value, 2, KF_FORMAT_PAD_SPACES,
                        KF_MENU_FORMAT_STYLE_SINGLE_PREFIX, text.glyphs.codes);
                    menu_draw_number(&menu_sprite_defs[KF_MENU_SPRITE_NUMBER_ATLAS], &text);
                }
                value = *number_values++;
                if (value != KF_MENU_LIST_NO_NUMBER) {
                    text.position.x += 189;
                    menu_format_number(value, 3, KF_FORMAT_PAD_SPACES,
                        KF_MENU_FORMAT_STYLE_PAIR_15_16, text.glyphs.codes);
                    menu_draw_number(&menu_sprite_defs[KF_MENU_SPRITE_NUMBER_ATLAS], &text);
                }
            }
        }
    }

    sprite = &menu_sprite_defs[KF_MENU_SPRITE_LIST_TOP];
    primitive_buffer_begin_poly_ft4();
    setRGB0(current_poly_ft4, 255, 255, 255);
    current_poly_ft4->tpage = sprite->tpage;
    current_poly_ft4->clut = sprite->clut;
    setXYWH(current_poly_ft4, list->list_x, list->list_y,
        sprite->width, sprite->height);
    setUVWH(current_poly_ft4, sprite->u, sprite->v,
        sprite->width, sprite->height);
    SetSemiTrans((void *)current_poly_ft4, 1);
    primitive_buffer_commit_poly_ft4(KF_MENU_WIDGET_OT_DEPTH);

    for (row = 0; row < list->visible_rows; row++) {
        sprite = row == list->cursor_row
            ? &menu_sprite_defs[KF_MENU_SPRITE_LIST_SELECTED_ROW]
            : &menu_sprite_defs[KF_MENU_SPRITE_LIST_ROW];
        primitive_buffer_begin_poly_ft4();
        setRGB0(current_poly_ft4, 255, 255, 255);
        current_poly_ft4->tpage = sprite->tpage;
        current_poly_ft4->clut = sprite->clut;
        setXYWH(current_poly_ft4, list->list_x, list->list_y + row * 14 + 5,
            sprite->width, sprite->height);
        setUVWH(current_poly_ft4, sprite->u, sprite->v,
            sprite->width, sprite->height);
        SetSemiTrans((void *)current_poly_ft4, 1);
        primitive_buffer_commit_poly_ft4(KF_MENU_WIDGET_OT_DEPTH);
    }

    sprite = &menu_sprite_defs[KF_MENU_SPRITE_LIST_BOTTOM];
    primitive_buffer_begin_poly_ft4();
    setRGB0(current_poly_ft4, 255, 255, 255);
    current_poly_ft4->tpage = sprite->tpage;
    current_poly_ft4->clut = sprite->clut;
    setXYWH(current_poly_ft4, list->list_x,
        list->list_y + list->visible_rows * 14 + 5,
        sprite->width, sprite->height);
    setUVWH(current_poly_ft4, sprite->u, sprite->v,
        sprite->width, sprite->height);
    SetSemiTrans((void *)current_poly_ft4, 1);
    primitive_buffer_commit_poly_ft4(KF_MENU_WIDGET_OT_DEPTH);

    if (render_mode == KF_MENU_LIST_SHOP_BUY || render_mode == KF_MENU_LIST_SHOP_SELL
        || render_mode == KF_MENU_LIST_STOCK_BUY || render_mode == KF_MENU_LIST_STOCK_OWNED) {
        menu_draw_status_counters(KF_MENU_COUNTERS_GOLD);
    } else if (render_mode == KF_MENU_LIST_TRADE) {
        menu_draw_status_counters(KF_MENU_COUNTERS_OBJECT_96);
    }
    if (render_mode == KF_MENU_LIST_CARD_LOAD || render_mode == KF_MENU_LIST_CARD_SAVE) {
        menu_render_list_mode_8_9_noop();
    }
}
enum { KF_PREVIEW_ANGLE_MASK = 0xfff };

ADDRESS(0x80021478, 0xf4)
void menu_draw_two_option(const KfMenuGlyphString *accept_label,
    const KfMenuGlyphString *decline_label, s32 selected_choice, b32 confirmation)
{
    if (selected_choice == KF_MENU_CHOICE_ACCEPT) {
        menu_blit_sprite(&menu_sprite_defs[KF_MENU_SPRITE_SELECTION_CURSOR],
            &accept_label->position);
    } else {
        menu_blit_sprite(&menu_sprite_defs[KF_MENU_SPRITE_SELECTION_CURSOR],
            &decline_label->position);
    }
    if (confirmation == KF_TRUE) {
        if (selected_choice == KF_MENU_CHOICE_ACCEPT) {
            menu_blit_sprite_translucent(&menu_sprite_defs[KF_MENU_SPRITE_OPTION_HIGHLIGHT],
                &accept_label->position);
            menu_blit_sprite_translucent(&menu_sprite_defs[KF_MENU_SPRITE_OPTION_BACKGROUND],
                &decline_label->position);
        } else {
            menu_blit_sprite_translucent(&menu_sprite_defs[KF_MENU_SPRITE_OPTION_BACKGROUND],
                &accept_label->position);
            menu_blit_sprite_translucent(&menu_sprite_defs[KF_MENU_SPRITE_OPTION_HIGHLIGHT],
                &decline_label->position);
        }
    } else {
        menu_blit_sprite_translucent(&menu_sprite_defs[KF_MENU_SPRITE_OPTION_BACKGROUND],
            &accept_label->position);
        menu_blit_sprite_translucent(&menu_sprite_defs[KF_MENU_SPRITE_OPTION_BACKGROUND],
            &decline_label->position);
    }
    menu_draw_string(&menu_sprite_defs[KF_MENU_SPRITE_GLYPH_ATLAS], accept_label);
    menu_draw_string(&menu_sprite_defs[KF_MENU_SPRITE_GLYPH_ATLAS], decline_label);
}

ADDRESS(0x8002156c, 0x154)
void menu_update_item_preview(s32 item_id)
{
    MATRIX rotation;
    /* Retail reserves two unreferenced matrices in the frame. */
    MATRIX frame_reserve[2];
    MATRIX light;
    MATRIX lit;
    MATRIX color;

    if (player_state.item_preview_enabled == KF_PLAYER_OPTION_OFF || (u8)item_id == 0xff) {
        return;
    }

    rotation.t[0] = menu_item_preview_translation.vx;
    rotation.t[1] = menu_item_preview_translation.vy;
    rotation.t[2] = menu_item_preview_translation.vz;
    menu_item_preview_rotation.vy += menu_item_preview_rotation_step;
    menu_item_preview_rotation.vx &= KF_PREVIEW_ANGLE_MASK;
    menu_item_preview_rotation.vy &= KF_PREVIEW_ANGLE_MASK;
    menu_item_preview_rotation.vz &= KF_PREVIEW_ANGLE_MASK;
    RotMatrix(&menu_item_preview_rotation, &rotation);

    light.m[0][0] = 0x800;
    light.m[0][1] = 0x800;
    light.m[0][2] = -0x800;
    light.m[1][0] = 0;
    light.m[1][1] = 0;
    light.m[1][2] = -0x800;
    light.m[2][0] = KF_FIXED12_ONE;
    light.m[2][1] = 0;
    light.m[2][2] = -0x800;
    MulMatrix0(&light, &rotation, &lit);
    SetLightMatrix(&lit);

    color.m[0][0] = KF_FIXED12_ONE;
    color.m[0][1] = KF_FIXED12_ONE;
    color.m[0][2] = KF_FIXED12_ONE;
    color.m[1][0] = KF_FIXED12_ONE;
    color.m[1][1] = KF_FIXED12_ONE;
    color.m[1][2] = KF_FIXED12_ONE;
    color.m[2][0] = KF_FIXED12_ONE;
    color.m[2][1] = KF_FIXED12_ONE;
    color.m[2][2] = KF_FIXED12_ONE;
    SetColorMatrix(&color);
    SetRotMatrix(&rotation);
    SetTransMatrix(&rotation);
    menu_render_item_model();
    current_poly_ft4 = (POLY_FT4 *)game_graphics_runtime.display_state.primitive_buffer->cursor;
}

ADDRESS(0x800216c0, 0x1f4)
void menu_draw_status_counters(s32 kind)
{
    KfMenuGlyphString heading;
    KfMenuGlyphString amount;

    heading.position.x = 183;
    heading.position.y = 33;
    if (kind == KF_MENU_COUNTERS_OBJECT_96) {
        heading.glyphs.codes[0] = 7;
        heading.glyphs.codes[1] = 41;
        heading.glyphs.codes[2] = 12;
        heading.glyphs.codes[3] = 15;
        heading.glyphs.codes[4] = 42;
        heading.glyphs.codes[5] = KF_MENU_TEXT_END;
    } else {
        heading.glyphs.codes[0] = 221;
        heading.glyphs.codes[1] = 222;
        heading.glyphs.codes[2] = 209;
        heading.glyphs.codes[3] = KF_MENU_TEXT_END;
    }
    menu_blit_sprite_translucent(&menu_sprite_defs[KF_MENU_SPRITE_PANEL_BACKGROUND], &heading.position);
    menu_draw_string(&menu_sprite_defs[KF_MENU_SPRITE_GLYPH_ATLAS], &heading);

    amount.position.x = heading.position.x + 56;
    amount.position.y = heading.position.y;
    if (kind == KF_MENU_COUNTERS_OBJECT_96) {
        menu_format_number(game_counter_bytes[KF_ENUM_ENCODE(u8, KF_OBJECT_96)], 7, KF_FORMAT_PAD_SPACES,
            KF_MENU_FORMAT_STYLE_TRAILING_11,
                           amount.glyphs.codes);
    } else {
        menu_format_number(player_state.gold, 7, KF_FORMAT_PAD_SPACES,
            KF_MENU_FORMAT_STYLE_TRAILING_13, amount.glyphs.codes);
    }
    menu_draw_number(&menu_sprite_defs[KF_MENU_SPRITE_NUMBER_ATLAS], &amount);

    if (kind != KF_MENU_COUNTERS_GOLD_ONLY) {
        heading.position.x = 183;
        heading.position.y = 61;
        heading.glyphs.codes[0] = 202;
        heading.glyphs.codes[1] = 203;
        heading.glyphs.codes[2] = KF_MENU_TEXT_END;
        menu_blit_sprite_translucent(&menu_sprite_defs[KF_MENU_SPRITE_PANEL_BACKGROUND], &heading.position);
        menu_draw_string(&menu_sprite_defs[KF_MENU_SPRITE_GLYPH_ATLAS], &heading);
        amount.position.x = heading.position.x + 91;
        amount.position.y = heading.position.y;
        menu_format_number(menu_item_quantity, 2, KF_FORMAT_PAD_SPACES,
            KF_MENU_FORMAT_STYLE_SINGLE_PREFIX, amount.glyphs.codes);
        menu_draw_number(&menu_sprite_defs[KF_MENU_SPRITE_NUMBER_ATLAS], &amount);
    }
}
ADDRESS(0x800218b4, 0x1d0)
void menu_blit_sprite_translucent(
    const KfMenuSpriteDef *sprite, const KfMenuPoint *position)
{
    primitive_buffer_begin_poly_ft4();
    setRGB0(current_poly_ft4, 0xff, 0xff, 0xff);
    current_poly_ft4->tpage = sprite->tpage;
    current_poly_ft4->clut = sprite->clut;
    setXYWH(current_poly_ft4,
        position->x - KF_MENU_TRANSLUCENT_SPRITE_OFFSET,
        position->y - KF_MENU_TRANSLUCENT_SPRITE_OFFSET,
        sprite->width, sprite->height);
    setUVWH(current_poly_ft4, sprite->u, sprite->v, sprite->width, sprite->height);
    SetSemiTrans((void *)current_poly_ft4, 1);
    primitive_buffer_commit_poly_ft4(KF_MENU_WIDGET_OT_DEPTH);
}

enum {
    KF_MENU_CURSOR_COLOR = 0x40,
    KF_MENU_CURSOR_LEFT_MARGIN = 8,
    KF_MENU_CURSOR_TEXTURE_PAGE_BASE = 500,
    KF_MENU_CURSOR_CLUT_BASE = 36
};

ADDRESS(0x80021a84, 0x1d8)
void menu_blit_sprite(const KfMenuSpriteDef *sprite, const KfMenuPoint *position)
{
    s32 frame = menu_cursor_animation_frame;

    primitive_buffer_begin_poly_ft4();
    setRGB0(current_poly_ft4,
        KF_MENU_CURSOR_COLOR, KF_MENU_CURSOR_COLOR, KF_MENU_CURSOR_COLOR);
    current_poly_ft4->tpage = sprite->tpage;
    current_poly_ft4->clut = ((frame + KF_MENU_CURSOR_TEXTURE_PAGE_BASE) << 6)
        + KF_MENU_CURSOR_CLUT_BASE;
    setXYWH(current_poly_ft4,
        position->x - (sprite->width + KF_MENU_CURSOR_LEFT_MARGIN),
        position->y, sprite->width, sprite->height);
    setUVWH(current_poly_ft4,
        sprite->u + (frame << 4), sprite->v, sprite->width, sprite->height);
    primitive_buffer_commit_poly_ft4(KF_MENU_WIDGET_OT_DEPTH);
}

enum {
    KF_MENU_FIXED_CURSOR_COLOR = 0x38,
    KF_MENU_FIXED_CURSOR_CLUT = 0x7d24,
    KF_MENU_FIXED_CURSOR_LEFT_MARGIN = 7
};

ADDRESS(0x80021c5c, 0x1b4)
void menu_blit_sprite_fixed_clut(
    const KfMenuSpriteDef *sprite, const KfMenuPoint *position)
{
    primitive_buffer_begin_poly_ft4();
    setRGB0(current_poly_ft4,
        KF_MENU_FIXED_CURSOR_COLOR,
        KF_MENU_FIXED_CURSOR_COLOR,
        KF_MENU_FIXED_CURSOR_COLOR);
    current_poly_ft4->tpage = sprite->tpage;
    current_poly_ft4->clut = KF_MENU_FIXED_CURSOR_CLUT;
    setXYWH(current_poly_ft4,
        position->x - (sprite->width + KF_MENU_FIXED_CURSOR_LEFT_MARGIN),
        position->y, sprite->width, sprite->height);
    setUVWH(current_poly_ft4,
        sprite->u, sprite->v, sprite->width, sprite->height);
    primitive_buffer_commit_poly_ft4(KF_MENU_WIDGET_OT_DEPTH);
}
static inline void menu_begin_text_glyph(
    const KfMenuSpriteDef *font, const KfMenuGlyphString *string, s32 x_offset)
{
    primitive_buffer_begin_poly_ft4();
    current_poly_ft4->tpage = font->tpage;
    current_poly_ft4->clut = font->clut;
    setXYWH(current_poly_ft4, string->position.x + x_offset,
        string->position.y, font->width, font->height);
}

/* Selects the 15-by-15 font cell of a glyph code's low twelve bits. The
 * do/while contour weights its temporaries as one loop level deeper. */
#define MENU_SET_GLYPH_UV(p, code, w, h) do { \
    u32 glyph = (code) & KF_MENU_TEXT_GLYPH_MASK; \
    setUVWH(p, (glyph % KF_MENU_FONT_COLUMNS) * KF_MENU_FONT_CELL_WIDTH, \
        (glyph / KF_MENU_FONT_COLUMNS) * KF_MENU_FONT_CELL_HEIGHT, w, h); \
} while (0)

ADDRESS(0x80021e10, 0x1bc)
void menu_draw_string(const KfMenuSpriteDef *font, const KfMenuGlyphString *string)
{
    const s16 *code = string->glyphs.codes;
    s32 i;
    s32 x_offset;
    /* Retail reserves an unreferenced 8-byte frame slot. */
    s16 frame_reserve[4];

    for (i = 0; *code != KF_MENU_TEXT_END; code++, i++) {
        x_offset = i * KF_MENU_GLYPH_ADVANCE;
        menu_begin_text_glyph(font, string, x_offset);
        MENU_SET_GLYPH_UV(current_poly_ft4, *code, font->width, font->height);
        primitive_buffer_commit_poly_ft4(KF_MENU_CONTENT_OT_DEPTH);

        if (*code & KF_MENU_TEXT_DAKUTEN) {
            menu_begin_text_glyph(font, string, x_offset);
            setUVWH(current_poly_ft4, KF_MENU_DAKUTEN_U, KF_MENU_KANA_MARK_V,
                font->width, font->height);
            primitive_buffer_commit_poly_ft4(KF_MENU_CONTENT_OT_DEPTH);
        }

        if (*code & KF_MENU_TEXT_HANDAKUTEN) {
            menu_begin_text_glyph(font, string, x_offset);
            setUVWH(current_poly_ft4, KF_MENU_HANDAKUTEN_U, KF_MENU_KANA_MARK_V,
                font->width, font->height);
            primitive_buffer_commit_poly_ft4(KF_MENU_CONTENT_OT_DEPTH);
        }
    }
}

ADDRESS(0x80021fcc, 0x2ec)
void menu_draw_number(const KfMenuSpriteDef *font, const KfMenuGlyphString *string)
{
    const s16 *code;
    s32 x_offset;

    code = string->glyphs.codes;
    if (*code == KF_MENU_TEXT_END) {
        return;
    }
    x_offset = 0;
    do {
        primitive_buffer_begin_poly_ft4();
        current_poly_ft4->tpage = font->tpage;
        current_poly_ft4->clut = font->clut;
        setXYWH(current_poly_ft4,
            string->position.x + x_offset, string->position.y,
            font->width, font->height);
        if (*code < KF_MENU_NUMBER_COLUMN_ROWS) {
            setUVWH(current_poly_ft4,
                font->u, *code * KF_MENU_FONT_CELL_HEIGHT,
                font->width, font->height);
        } else {
            setUVWH(current_poly_ft4,
                (u8)(font->u + KF_MENU_NUMBER_COLUMN_WIDTH),
                (*code - KF_MENU_NUMBER_COLUMN_ROWS) * KF_MENU_FONT_CELL_HEIGHT,
                font->width, font->height);
        }
        primitive_buffer_commit_poly_ft4(KF_MENU_CONTENT_OT_DEPTH);
        code++;
        x_offset += KF_MENU_DIGIT_ADVANCE;
    } while (*code != KF_MENU_TEXT_END);
}
ADDRESS(0x800222b8, 0x270)
void menu_draw_nine_slice_panel(s32 x, s32 y, s32 width, s32 height,
    s32 overlap_x, s32 overlap_y)
{
    enum { MENU_PANEL_BASE_SPAN = 94 };
    const KfMenuSpriteDef *tile;
    u16 clut;
    s32 row;
    s32 column;
    s32 tile_index;
    s32 draw_x;
    s32 draw_y;
    s32 draw_width;
    s32 draw_height;

    width -= MENU_PANEL_BASE_SPAN;
    height -= MENU_PANEL_BASE_SPAN;
    tile_index = 9;
    tile = &menu_sprite_defs[KF_MENU_SPRITE_PANEL_TOP_LEFT];
    draw_y = y;
    for (row = 0; row < 3; row++) {
        draw_height = tile->height;
        if (row == 1) {
            draw_y += 33;
            draw_height += height + overlap_y;
        } else if (row == 2) {
            draw_y += 28 + height;
        }
        draw_x = x;
        for (column = 0; column < 3; column++) {
            draw_width = tile->width;
            if (column == 1) {
                draw_x += 33;
                draw_width += width + overlap_x;
            } else if (column == 2) {
                draw_x += 28 + width;
            }
            primitive_buffer_begin_poly_ft4();
            setRGB0(current_poly_ft4, 0xff, 0xff, 0xff);
            SetSemiTrans((void *)current_poly_ft4, 1);
            current_poly_ft4->tpage = tile->tpage;
            clut = tile->clut;
            setXYWH(current_poly_ft4, draw_x, draw_y, draw_width, draw_height);
            current_poly_ft4->clut = clut;
            setUVWH(current_poly_ft4, tile->u, tile->v, tile->width, tile->height);
            primitive_buffer_commit_poly_ft4(KF_MENU_WIDGET_OT_DEPTH);
            tile_index++;
            tile = &menu_sprite_defs[tile_index + 2];
        }
    }
}

ADDRESS(0x80022528, 0x8)
void menu_render_list_mode_8_9_noop(void)
{
}

ADDRESS(0x80022530, 0x178)
void menu_frame_begin(void)
{
    game_graphics_runtime.display_state.buffer_index =
        display_next_buffer(game_graphics_runtime.display_state.buffer_index);
    game_graphics_runtime.display_state.primitive_buffer =
        &game_graphics_runtime.display_state.primitive_buffers[
            KF_ENUM_ENCODE(u8, game_graphics_runtime.display_state.buffer_index)];
    game_graphics_runtime.display_state.ordering_table =
        game_graphics_runtime.display_state.ordering_tables[
            KF_ENUM_ENCODE(u8, game_graphics_runtime.display_state.buffer_index)].entries;
    ClearOTagR(game_graphics_runtime.display_state.ordering_table,
        KF_GAME_ORDERING_TABLE_LENGTH);
    game_graphics_runtime.display_state.primitive_buffer->cursor =
        game_graphics_runtime.display_state.primitive_buffer->start;
    current_poly_ft4 = (POLY_FT4 *)
        game_graphics_runtime.display_state.primitive_buffer->cursor;

    if (game_graphics_runtime.display_state.buffer_index == KF_DISPLAY_BUFFER_SECOND) {
        menu_frame_upload_rect.x = 0;
        menu_frame_upload_rect.y = KF_MENU_UPLOAD_SECOND_BUFFER_Y;
    } else {
        menu_frame_upload_rect.x = 0;
        menu_frame_upload_rect.y = 0;
    }

    if (menu_cursor_animation_direction == KF_MENU_CURSOR_ANIMATION_FORWARD) {
        menu_cursor_animation_frame++;
    } else if (menu_cursor_animation_direction == KF_MENU_CURSOR_ANIMATION_BACKWARD) {
        menu_cursor_animation_frame--;
    }
    if (menu_cursor_animation_frame >= KF_MENU_CURSOR_FRAME_COUNT) {
        menu_cursor_animation_frame = KF_MENU_CURSOR_FRAME_COUNT - 1;
        menu_cursor_animation_direction = KF_MENU_CURSOR_ANIMATION_BACKWARD;
    }
    if (menu_cursor_animation_frame < 0) {
        menu_cursor_animation_frame = 0;
        menu_cursor_animation_direction = KF_MENU_CURSOR_ANIMATION_HOLD;
    }
}

ADDRESS(0x800226a8, 0xac)
void menu_present_frame(void)
{
    DrawSync(0);
    VSync(0);
    PutDrawEnv(&game_graphics_runtime.display_draw_environments[
        KF_ENUM_ENCODE(u8, game_graphics_runtime.display_state.buffer_index)]);
    PutDispEnv(&game_graphics_runtime.display_disp_environments[
        KF_ENUM_ENCODE(u8, game_graphics_runtime.display_state.buffer_index)]);
    LoadImage(&menu_frame_upload_rect, menu_frame_upload_pixels);
    DrawOTag(game_graphics_runtime.display_state.ordering_table
        + (KF_GAME_ORDERING_TABLE_LENGTH - 1));
}

ADDRESS(0x80022754, 0x174)
void menu_enter_display_state(s32 mode)
{
    /* Retail reserves an unreferenced 8-byte frame slot. */
    s16 frame_reserve[4];

    pool_release_all();
    game_graphics_runtime.display_draw_environments[0].isbg = 0;
    game_graphics_runtime.display_draw_environments[0].dfe = 0;
    game_graphics_runtime.display_draw_environments[1].isbg = 0;
    game_graphics_runtime.display_draw_environments[1].dfe = 0;

    menu_saved_primitive_buffers[0] =
        game_graphics_runtime.display_state.primitive_buffers[0];
    menu_saved_primitive_buffers[1] =
        game_graphics_runtime.display_state.primitive_buffers[1];
    game_graphics_runtime.display_state.primitive_buffers[0].end =
        game_graphics_runtime.display_state.primitive_buffers[0].start + 0x6400;
    game_graphics_runtime.display_state.primitive_buffers[1].start =
        game_graphics_runtime.display_state.primitive_buffers[0].end;
    game_graphics_runtime.display_state.primitive_buffers[1].end =
        game_graphics_runtime.display_state.primitive_buffers[1].start + 0x6400;
    menu_frame_upload_pixels = (u_long *)
        game_graphics_runtime.display_state.primitive_buffers[1].end;

    if (game_graphics_runtime.display_state.buffer_index == KF_DISPLAY_BUFFER_SECOND) {
        menu_frame_upload_rect.x = 0;
        menu_frame_upload_rect.y = KF_MENU_UPLOAD_SECOND_BUFFER_Y;
    } else {
        menu_frame_upload_rect.x = 0;
        menu_frame_upload_rect.y = 0;
    }
    menu_frame_upload_rect.w = 320;
    menu_frame_upload_rect.h = 240;
    StoreImage(&menu_frame_upload_rect, menu_frame_upload_pixels);
    DrawSync(0);

    menu_saved_music_enabled = player_state.audio_music_enabled;
    if (player_state.audio_music_enabled == KF_PLAYER_OPTION_ON && audio_state.sequence_active == KF_TRUE) {
        SsSeqPause(audio_state.sequence_id);
    }
}

ADDRESS(0x800228c8, 0x110)
void menu_exit_display_state(b32 stop_sequence)
{
    KfPlayerOption music_enabled;

    game_graphics_runtime.display_state.primitive_buffers[0] =
        menu_saved_primitive_buffers[0];
    game_graphics_runtime.display_state.primitive_buffers[1] =
        menu_saved_primitive_buffers[1];
    game_graphics_runtime.display_draw_environments[0].isbg = 1;
    game_graphics_runtime.display_draw_environments[0].dfe = 1;
    game_graphics_runtime.display_draw_environments[1].isbg = 1;
    game_graphics_runtime.display_draw_environments[1].dfe = 1;

    if (stop_sequence == KF_TRUE) {
        audio_stop_sequence();
    } else {
        music_enabled = player_state.audio_music_enabled;
        if (music_enabled != menu_saved_music_enabled) {
            if (music_enabled == KF_PLAYER_OPTION_OFF) {
                audio_stop_sequence();
            } else {
                audio_start_sequence();
            }
        } else if (music_enabled == KF_PLAYER_OPTION_ON && audio_state.sequence_active == KF_TRUE) {
            SsSeqReplay(audio_state.sequence_id);
        }
    }
}

enum {
    KF_MENU_PRIMITIVE_BRIGHTNESS = 0x68
};

ADDRESS(0x800229d8, 0x50)
void primitive_buffer_begin_poly_ft4(void)
{
    SetPolyFT4(current_poly_ft4);
    setRGB0(current_poly_ft4,
        KF_MENU_PRIMITIVE_BRIGHTNESS,
        KF_MENU_PRIMITIVE_BRIGHTNESS,
        KF_MENU_PRIMITIVE_BRIGHTNESS);
}

ADDRESS(0x80022a28, 0x50)
void primitive_buffer_commit_poly_ft4(s32 depth)
{
    AddPrim((void *)&game_graphics_runtime.display_state.ordering_table[depth], (void *)current_poly_ft4);
    current_poly_ft4++;
    game_graphics_runtime.display_state.primitive_buffer->cursor = (u8 *)current_poly_ft4;
}

ADDRESS(0x80022a78, 0xa8)
void menu_list_init(KfMenuList *list, KfMenuWindowKind window_kind, s32 row)
{
    KfMenuGlyphString *source;
    s16 *title_codes;
    s16 *source_codes;
    s32 i;

    list->title.position.x = 17;
    list->title.position.y = 19;
    title_codes = list->title.glyphs.codes;
    source = &menu_window_layout(window_kind)->rows[row];
    source_codes = source->glyphs.codes;
    for (i = 0; i < KF_MENU_LIST_TITLE_COPY_GLYPHS; i++) {
        *title_codes++ = *source_codes++;
    }
    list->list_x = 0x2a;
    list->list_y = 0x9f;
    list->entry_count = 0;
    list->visible_rows = 4;
    list->scroll_offset = 0;
    list->selected_index = 0;
    list->cursor_row = 0;
    list->glyphs_per_entry = 10;
}

enum { KF_MENU_FORMAT_BLANK = 10 };

ADDRESS(0x80022b20, 0x18c)
void menu_format_number(s32 value, s32 count, KfFormatPaddingMode padding_mode,
    KfMenuFormatStyle style, s16 *out)
{
    s32 i;
    s16 blank;
    s16 *cursor;
    /* Retail reserves an unreferenced 8-byte frame slot. */
    s16 frame_reserve[4];

    if (style == KF_MENU_FORMAT_STYLE_SINGLE_PREFIX || style == KF_MENU_FORMAT_STYLE_TRAILING_13
        || style == KF_MENU_FORMAT_STYLE_TRAILING_11) {
        count++;
    } else if (style == KF_MENU_FORMAT_STYLE_PAIR_15_16
        || style == KF_MENU_FORMAT_STYLE_PAIR_14_17) {
        count += 2;
    } else if (style == KF_MENU_FORMAT_STYLE_TRIPLE_12_18_16) {
        count += 3;
    }

    i = 0;
    blank = padding_mode == KF_FORMAT_PAD_SPACES ? KF_MENU_FORMAT_BLANK : 0;
    if (count > 0) {
        cursor = out;
        do {
            *cursor++ = blank;
            i++;
        } while (i < count);
    }
    out[count] = KF_MENU_TEXT_END;

    if (style == KF_MENU_FORMAT_STYLE_SINGLE_PREFIX) {
        out[0] = 19;
    } else if (style == KF_MENU_FORMAT_STYLE_TRAILING_13) {
        out[count - 1] = 13;
        count--;
    } else if (style == KF_MENU_FORMAT_STYLE_PAIR_15_16) {
        out[0] = 15;
        out[1] = 16;
    } else if (style == KF_MENU_FORMAT_STYLE_TRIPLE_12_18_16) {
        out[0] = 12;
        out[1] = 18;
        out[2] = 16;
    } else if (style == KF_MENU_FORMAT_STYLE_PAIR_14_17) {
        out[0] = 14;
        out[1] = 17;
    } else if (style == KF_MENU_FORMAT_STYLE_TRAILING_11) {
        out[count - 1] = 11;
        count--;
    }

    for (i = count - 1; i >= 0; i--) {
        out[i] = value % 10;
        value /= 10;
        if (value == 0) {
            i = -1;
        }
    }
}

ADDRESS(0x80022cac, 0xd4)
s32 menu_load_item_model(u8 item_id)
{
    u8 *allocation;

    menu_item_quantity = 1;
    if (player_state.item_preview_enabled == KF_PLAYER_OPTION_OFF) {
        return 0;
    }

    menu_release_item_model();
    if (item_id != 0xff) {
        allocation = memory_allocate(cd_archive_entry_extent(KF_RESOURCE_ARCHIVE_ITEM, item_id, NULL));
        cd_archive_read(KF_RESOURCE_ARCHIVE_ITEM, item_id, (u_long *)allocation);
        tmd_register(KF_TMD_SLOT_MENU_ITEM, (KfTmdHeader *)allocation);
        menu_item_model_allocation_pending = KF_TRUE;
    }
    setVector(&menu_item_preview_translation, 0, 0, 1000);
    setVector(&menu_item_preview_rotation, 0, 0, 0);
    menu_item_preview_rotation_step = 4;
    return 0;
}

ADDRESS(0x80022d80, 0x44)
void menu_release_item_model(void)
{
    if (menu_item_model_allocation_pending == KF_TRUE) {
        memory_free((u8 *)game_graphics_runtime.tmd_state.slots[KF_TMD_SLOT_MENU_ITEM]);
        menu_item_model_allocation_pending = KF_FALSE;
    }
}
ADDRESS(0x80022dc4, 0x94)
void menu_play_sound_cue(s32 cue)
{
    if (cue == KF_MENU_SOUND_CURSOR) {
        audio_key_on(KF_MENU_SOUND_CURSOR, 48, 48, 0);
        SsSeqCalledTbyT();
    } else if ((u32)cue - KF_MENU_SOUND_CONFIRM < 2u) {
        audio_key_on(cue, 48, 48, 0);
        SsSeqCalledTbyT();
    } else if (cue == KF_MENU_SOUND_ITEM_USED) {
        audio_key_on(KF_MENU_SOUND_ITEM_USED, 0, 58, 0);
        VSync(0);
        VSync(0);
        audio_key_on(KF_MENU_SOUND_ITEM_USED, 58, 0, 0);
    }
}

ADDRESS(0x80022e58, 0x38)
u32 input_read_mark_active(void)
{
    u32 buttons = PadRead(1);
    if (buttons != 0) {
        input_press_pending = KF_TRUE;
    }
    return buttons;
}

ADDRESS(0x80022e90, 0x6c)
void input_wait_brief_release(void)
{
    s32 polls;

    if (input_press_pending == KF_TRUE) {
        input_press_pending = KF_FALSE;
        for (polls = 0; PadRead(1) != 0;) {
            if (polls++ < 6) {
                VSync(0);
            } else {
                menu_cursor_animation_frame = 0;
                break;
            }
        }
    }
}
