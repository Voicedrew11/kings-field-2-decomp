#include <kf/lib/address.h>
#include <kf/lib/null.h>
#include <kf/game/graphics.h>
#include <kf/game/memory.h>
#include <stdarg.h>

enum { FLOOR_ITEM_VRAM_PIXEL_BYTES = sizeof(u16) };

ADDRESS(0x8002db64, 0x3c)
KfFloorItem *floor_item_find_free(void)
{
    KfFloorItem *item = game_graphics_runtime.floor_items;
    s32 remaining = KF_FLOOR_ITEM_CAPACITY;

    do {
        if (item->kind == KF_FLOOR_ITEM_NONE) {
            return item;
        }
        item++;
    } while (--remaining != 0);

    return NULL;
}

/* A scrolling image passes its byte width and height after KIND. */
ADDRESS(0x8002dba0, 0xd8)
void floor_item_capture_image(s32 x, s32 y, u8 update_interval, u8 row_step,
                   s32 kind, ...)
{
    KfFloorItem *item = floor_item_find_free();
    va_list args;
    s32 width_bytes;
    u16 height;

    if (item != NULL) {
        item->kind = kind;
        item->row_offset = 0;
        item->update_interval = update_interval;
        item->row_step = row_step;
        item->frames_until_update = 0;
        item->rect.x = x;
        item->rect.y = y;
        if (kind == KF_FLOOR_ITEM_SCROLLING_IMAGE) {
            va_start(args, kind);
            width_bytes = va_arg(args, s32);
            height = va_arg(args, u16);
            item->rect.w = width_bytes >> 2;
            item->rect.h = height;
            item->pixels = (u_long *)memory_allocate(
                item->rect.w * (s16)height * FLOOR_ITEM_VRAM_PIXEL_BYTES);
        }
        StoreImage(&item->rect, item->pixels);
        DrawSync(0);
    }
}

ADDRESS(0x8002dc78, 0x164)
void floor_item_update_textures(void)
{
    KfFloorItem *item = game_graphics_runtime.floor_items;
    s32 remaining = KF_FLOOR_ITEM_CAPACITY - 1;
    RECT rect;

    do {
        if (item->frames_until_update == 0) {
            item->frames_until_update = item->update_interval;
            if (item->kind == KF_FLOOR_ITEM_SCROLLING_IMAGE) {
                item->row_offset += item->row_step;
                if ((s16)item->row_offset >= item->rect.h) {
                    item->row_offset -= item->rect.h;
                }
                setRECT(&rect, item->rect.x, item->rect.y + item->row_offset,
                        item->rect.w, item->rect.h - item->row_offset);
                LoadImage(&rect, item->pixels);
                if ((s16)item->row_offset != 0) {
                    u_long *pixels = ((rect.w * rect.h) >> 1) + item->pixels;
                    rect.y = item->rect.y;
                    rect.h = item->row_offset;
                    LoadImage(&rect, pixels);
                }
            }
        } else {
            item->frames_until_update--;
        }
        item++;
    } while (--remaining != -1);
}
