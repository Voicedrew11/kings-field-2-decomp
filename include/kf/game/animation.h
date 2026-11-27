#ifndef KF_GAME_ANIMATION_H
#define KF_GAME_ANIMATION_H

#include <kf/lib/bool.h>
#include <kf/lib/types.h>
#include <kf/game/asset.h>
#include <kf/game/pool.h>
#include <psyq/sdk.h>

enum { KF_PLAYER_WEAPON_ASSET_INDEX = 0x20 };

typedef struct KfAnimClip {
    u16 keyframe_count;
    u16 unused_02;
    u32 keyframe_offsets[1];
} KfAnimClip;

typedef struct KfAnimKeyframe {
    u16 reverse;
    u16 duration;
    u16 rest_index;
    u16 morph_count;
} KfAnimKeyframe;

typedef char kf_anim_keyframe_prefix_size[sizeof(KfAnimKeyframe) == 8 ? 1 : -1];

#define ASSET_CLIP(asset, offset) ((KfAnimClip *)ASSET_BYTES(asset, offset))
#define ASSET_KEYFRAME(asset, offset) ((KfAnimKeyframe *)ASSET_BYTES(asset, offset))
#define ASSET_MORPH(asset, offset) ((const s16 *)ASSET_BYTES(asset, offset))

KfAnimKeyframe *animation_select_keyframe(KfAssetHeader *asset,
    KF_ENUM_PARAM(KfAnimationClip, s32) clip_index, s32 phase, s32 *keyframe_index,
    u32 *blend_fraction);
void animation_expand_sparse_vertices(SVECTOR *vertices, const SVECTOR *base,
                                      const s16 *encoded);
void animation_decode_sparse_vertices(SVECTOR *vertices, const s16 *encoded);
void animation_apply_sparse_morph(SVECTOR *vertices, const s16 *encoded, s32 blend_fraction);
const s16 *animation_find_sparse_vertex(const s16 *encoded, s32 vertex_index);
b32 animation_sample_vertex(s32 asset_index, KF_ENUM_PARAM(KfAnimationClip, s32) clip, s32 phase,
                            s32 vertex_index,
                  SVECTOR *output);

#endif
