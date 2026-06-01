/**
 * @file render_objects.c renders hud elements
 * A more suitable name may be print.c
 **/

#include <libultraship.h>
#include <libultra/gbi.h>
#include <libc/math.h>
#include <stdio.h>
#include <string.h>
#include <mk64.h>
#include <align_asset_macro.h>
#include <macros.h>
#include <defines.h>
#include <common_structs.h>
#include <actor_types.h>
#include "camera.h"
#include "memory.h"
#include "math_util.h"
#include "math_util_2.h"
#include "objects.h"
#include "waypoints.h"
#include "bomb_kart.h"
#include <assets/models/common_data.h>
#include "render_player.h"
#include "animation.h"
#include "code_80005FD0.h"
#include "code_80057C60.h"
#include "code_8006E9C0.h"
#include "render_objects.h"
#include "update_objects.h"
#include "menu_items.h"
#include "collision.h"
#include "main.h"
#include "menus.h"
#include "code_80086E70.h"
#include "code_800029B0.h"
#include "courses/all_course_data.h"
#include <vehicles.h>
#include "data/some_data.h"
#include <assets/textures/some_data.h>
#include <assets/models/tracks/luigi_raceway/luigi_raceway_data.h>
#include <assets/models/tracks/moo_moo_farm/moo_moo_farm_data.h>
#include <assets/models/tracks/bowsers_castle/bowsers_castle_data.h>
#include <assets/models/tracks/frappe_snowland/frappe_snowland_data.h>
#include "port/Game.h"
#include "port/Engine.h"
#include "kart_transmission.h"
#include "kart_character_stats.h"
#include "hud_layout.h"

#define KART_RPM_DISPLAY_MULTIPLIER_DEFAULT 4.5f
#define KART_RPM_METER_MOTION_MAX_DEFAULT 7200.0f
#define KART_RPM_METER_CHARACTER_RANGE_SCALE_DEFAULT 0.35f

#include "engine/Matrix.h"
#include "engine/tracks/Track.h"
#include "engine/TrackBrowser.h"
#include "engine/sky/Sky.h"

#include "port/interpolation/FrameInterpolation.h"
#include "assets/textures/tracks/sherbet_land/sherbet_land_data.h"

Lights1 D_800E45C0[] = {
    gdSPDefLights1(100, 0, 0, 100, 0, 0, 0, -120, 0),
    gdSPDefLights1(100, 100, 0, 255, 255, 0, 0, -120, 0),
    gdSPDefLights1(100, 100, 100, 255, 255, 255, 0, -120, 0),
    gdSPDefLights1(100, 0, 0, 100, 0, 0, 0, -120, 0),
};

Lights1 D_800E4620 = gdSPDefLights1(150, 180, 250, 255, 255, 255, 0, 0, 0);

Lights1 D_800E46B0 = gdSPDefLights1(85, 85, 85, 255, 255, 255, 0, 0, 120);

Lights1 D_800E46C8 = gdSPDefLights1(85, 85, 85, 255, 255, 255, 0, 0, 120);

Lights1 D_800E46E0 = gdSPDefLights1(85, 85, 85, 255, 255, 255, 0, 0, 120);

void func_800431B0(Vec3f pos, Vec3su orientation, f32 scale, Vtx* vtx) {
    rsp_set_matrix_transformation(pos, orientation, scale);
    gSPVertex(gDisplayListHead++, vtx, 4, 0);
    gSPDisplayList(gDisplayListHead++, common_rectangle_display);
}

void func_80043220(Vec3f pos, Vec3su orientation, f32 scale, Gfx* gfx) {
    rsp_set_matrix_transformation(pos, orientation, scale);
    gSPDisplayList(gDisplayListHead++, D_0D0077A0);
    gSPDisplayList(gDisplayListHead++, gfx);
}

UNUSED void func_80043288(Vec3f pos, Vec3su orientation, f32 arg2, Gfx* gfx) {
    rsp_set_matrix_transformation(pos, orientation, arg2);
    gSPDisplayList(gDisplayListHead++, D_0D0077A0);
    gSPClearGeometryMode(gDisplayListHead++, G_CULL_BOTH);
    gSPDisplayList(gDisplayListHead++, gfx);
    gSPSetGeometryMode(gDisplayListHead++, G_CULL_BACK);
}

void func_80043328(Vec3f arg0, Vec3su arg1, f32 arg2, Gfx* gfx) {
    rsp_set_matrix_transformation(arg0, arg1, arg2);
    gSPDisplayList(gDisplayListHead++, D_0D0077D0);
    gSPDisplayList(gDisplayListHead++, gfx);
}

UNUSED void func_80043390(Vec3f arg0, Vec3su arg1, f32 arg2, Gfx* gfx) {
    rsp_set_matrix_transformation(arg0, arg1, arg2);
    gSPDisplayList(gDisplayListHead++, D_0D0077F8);
    gSPDisplayList(gDisplayListHead++, gfx);
}

UNUSED void func_800433F8(Vec3f arg0, Vec3su arg1, f32 arg2, Gfx* gfx) {
    rsp_set_matrix_transformation(arg0, arg1, arg2);
    gSPDisplayList(gDisplayListHead++, D_0D007828);
    gSPDisplayList(gDisplayListHead++, gfx);
}

UNUSED void func_80043460(Vec3f arg0, Vec3su arg1, f32 arg2, Gfx* gfx) {
    rsp_set_matrix_transformation(arg0, arg1, arg2);
    gSPDisplayList(gDisplayListHead++, D_0D007828);
    gSPClearGeometryMode(gDisplayListHead++, G_CULL_BOTH);
    gSPDisplayList(gDisplayListHead++, gfx);
    gSPSetGeometryMode(gDisplayListHead++, G_CULL_BACK);
}

void func_80043500(Vec3f arg0, Vec3su arg1, f32 arg2, Gfx* gfx) {
    rsp_set_matrix_transformation(arg0, arg1, arg2);
    gSPDisplayList(gDisplayListHead++, D_0D007850);
    gSPClearGeometryMode(gDisplayListHead++, G_CULL_BOTH);
    gSPDisplayList(gDisplayListHead++, gfx);
    gSPSetGeometryMode(gDisplayListHead++, G_CULL_BACK);
}

void func_800435A0(Vec3f arg0, Vec3su arg1, f32 arg2, Gfx* gfx, s32 arg4) {
    rsp_set_matrix_transformation_inverted_x_y_orientation(arg0, arg1, arg2);

    gSPDisplayList(gDisplayListHead++, D_0D007878);
    gDPSetPrimColor(gDisplayListHead++, 0, 0, 0xFF, 0xFF, 0xFF, arg4);
    gSPClearGeometryMode(gDisplayListHead++, G_CULL_BOTH);
    gSPDisplayList(gDisplayListHead++, gfx);
    gSPSetGeometryMode(gDisplayListHead++, G_CULL_BACK);
}

UNUSED void func_80043668(Vec3f arg0, Vec3su arg1, f32 arg2, Gfx* gfx) {
    rsp_set_matrix_transformation(arg0, arg1, arg2);
    gSPDisplayList(gDisplayListHead++, D_0D0078A0);
    gSPDisplayList(gDisplayListHead++, gfx);
}

UNUSED void func_800436D0(s32 arg0, s32 arg1, u16 arg2, f32 arg3, Vtx* vtx) {
    func_80042330(arg0, arg1, arg2, arg3);
    gSPDisplayList(gDisplayListHead++, D_0D0078A0);
    gSPVertex(gDisplayListHead++, vtx, 3, 0);
    gSPDisplayList(gDisplayListHead++, D_0D006930);
}

UNUSED void func_80043764(s32 arg0, s32 arg1, u16 arg2, f32 arg3, Vtx* vtx) {
    func_80042330(arg0, arg1, arg2, arg3);
    gSPDisplayList(gDisplayListHead++, D_0D0078A0);
    gSPVertex(gDisplayListHead++, vtx, 4, 0);
    gSPDisplayList(gDisplayListHead++, common_rectangle_display);
}

UNUSED void func_800437F8(s32 arg0, s32 arg1, u16 arg2, f32 arg3, Vtx* vtx, s32 arg5) {
    func_80042330(arg0, arg1, arg2, arg3);
    gSPDisplayList(gDisplayListHead++, D_0D0078A0);

    gDPSetRenderMode(gDisplayListHead++, G_RM_CLD_SURF, G_RM_CLD_SURF2);
    vtx[0].v.cn[3] = arg5;
    vtx[1].v.cn[3] = arg5;
    vtx[2].v.cn[3] = arg5;
    vtx[3].v.cn[3] = arg5;
    gSPVertex(gDisplayListHead++, vtx, 4, 0);
    gSPDisplayList(gDisplayListHead++, common_rectangle_display);
}

UNUSED void func_800438C4(s32 arg0, s32 arg1, u16 arg2, f32 arg3, Vtx* vtx, s32 arg5) {
    vtx[1].v.ob[0] = arg5;
    vtx[2].v.ob[0] = arg5;
    func_80042330(arg0, arg1, arg2, arg3);
    gSPDisplayList(gDisplayListHead++, D_0D0078A0);
    gDPSetRenderMode(gDisplayListHead++, G_RM_CLD_SURF, G_RM_CLD_SURF2);
    gSPVertex(gDisplayListHead++, vtx, 4, 0);
    gSPDisplayList(gDisplayListHead++, common_rectangle_display);
}

UNUSED void func_8004398C(s32 arg0, s32 arg1, u16 arg2, f32 arg3, Vtx* vtx, s32 arg5) {
    vtx[0].v.ob[0] = arg5;
    vtx[3].v.ob[0] = arg5;
    func_80042330(arg0, arg1, arg2, arg3);
    gSPDisplayList(gDisplayListHead++, D_0D0078A0);
    gDPSetRenderMode(gDisplayListHead++, G_RM_CLD_SURF, G_RM_CLD_SURF2);
    gSPVertex(gDisplayListHead++, vtx, 4, 0);
    gSPDisplayList(gDisplayListHead++, common_rectangle_display);
}

s32 func_80043A54(s32 arg0) {
    s32 temp_a1;
    s32 phi_v0;
    s32 phi_v1 = 0;

    phi_v0 = arg0;
    do {
        phi_v1++;
        temp_a1 = phi_v0 / 2;
        phi_v0 = temp_a1;
    } while (temp_a1 != 1);
    return phi_v1;
}

void load_texture_block_rgba32_nomirror(u8* texture, s32 width, s32 height) {
    gDPLoadTextureBlock(gDisplayListHead++, texture, G_IM_FMT_RGBA, G_IM_SIZ_32b, width, height, 0,
                        G_TX_NOMIRROR | G_TX_CLAMP, G_TX_NOMIRROR | G_TX_CLAMP, G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD,
                        G_TX_NOLOD);
}

void load_texture_tile_rgba32_nomirror(u8* texture, s32 width, s32 height) {
    gDPLoadTextureTile(gDisplayListHead++, texture, G_IM_FMT_RGBA, G_IM_SIZ_32b, width, height, 0, 0, width - 1,
                       height - 1, 0, G_TX_NOMIRROR | G_TX_CLAMP, G_TX_NOMIRROR | G_TX_CLAMP, G_TX_NOMASK, G_TX_NOMASK,
                       G_TX_NOLOD, G_TX_NOLOD);
}

void load_texture_block_rgba16_mirror(u8* texture, s32 width, s32 height) {
    gDPLoadTextureBlock(gDisplayListHead++, texture, G_IM_FMT_RGBA, G_IM_SIZ_16b, width, height, 0,
                        G_TX_MIRROR | G_TX_CLAMP, G_TX_MIRROR | G_TX_CLAMP, G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD,
                        G_TX_NOLOD);
}

void load_texture_block_rgba16_nomirror(u8* texture, s32 width, s32 height, s32 someMask) {
    gDPLoadTextureBlock(gDisplayListHead++, texture, G_IM_FMT_RGBA, G_IM_SIZ_16b, width, height, 0,
                        G_TX_MIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_CLAMP, someMask, G_TX_NOMASK, G_TX_NOLOD,
                        G_TX_NOLOD);
}

void load_texture_tile_rgba16_nomirror(u8* texture, s32 width, s32 height) {
    gDPLoadTextureTile(gDisplayListHead++, texture, G_IM_FMT_RGBA, G_IM_SIZ_16b, width, height, 0, 0, width - 1,
                       height - 1, 0, G_TX_NOMIRROR | G_TX_CLAMP, G_TX_NOMIRROR | G_TX_CLAMP, G_TX_NOMASK, G_TX_NOMASK,
                       G_TX_NOLOD, G_TX_NOLOD);
}

void load_texture_block_ia16_nomirror(u8* texture, s32 width, s32 height) {
    gDPLoadTextureBlock(gDisplayListHead++, texture, G_IM_FMT_IA, G_IM_SIZ_16b, width, height, 0,
                        G_TX_NOMIRROR | G_TX_CLAMP, G_TX_NOMIRROR | G_TX_CLAMP, G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD,
                        G_TX_NOLOD);
}

void load_texture_tile_ia16_nomirror(u8* texture, s32 width, s32 height) {
    gDPLoadTextureTile(gDisplayListHead++, texture, G_IM_FMT_IA, G_IM_SIZ_16b, width, height, 0, 0, width - 1,
                       height - 1, 0, G_TX_NOMIRROR | G_TX_CLAMP, G_TX_NOMIRROR | G_TX_CLAMP, G_TX_NOMASK, G_TX_NOMASK,
                       G_TX_NOLOD, G_TX_NOLOD);
}

void load_texture_block_ia8_nomirror(u8* texture, s32 width, s32 height) {
    gDPLoadTextureBlock(gDisplayListHead++, texture, G_IM_FMT_IA, G_IM_SIZ_8b, width, height, 0,
                        G_TX_NOMIRROR | G_TX_CLAMP, G_TX_NOMIRROR | G_TX_CLAMP, G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD,
                        G_TX_NOLOD);
}

void load_texture_tile_ia8_nomirror(u8* texture, s32 width, s32 height) {
    gDPLoadTextureTile(gDisplayListHead++, texture, G_IM_FMT_IA, G_IM_SIZ_8b, width, height, 0, 0, width - 1,
                       height - 1, 0, G_TX_NOMIRROR | G_TX_CLAMP, G_TX_NOMIRROR | G_TX_CLAMP, G_TX_NOMASK, G_TX_NOMASK,
                       G_TX_NOLOD, G_TX_NOLOD);
}

void load_texture_block_i8_nomirror(u8* texture, s32 width, s32 height) {
    gDPLoadTextureBlock(gDisplayListHead++, texture, G_IM_FMT_I, G_IM_SIZ_8b, width, height, 0,
                        G_TX_NOMIRROR | G_TX_CLAMP, G_TX_NOMIRROR | G_TX_CLAMP, G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD,
                        G_TX_NOLOD);
}

void func_80044924(u8* texture, s32 width, s32 height) {
    // This macro ought to be equivalent to the block of macros below but it doesn't match
    // See comment above the `gDPLoadBlock` macro
    // gDPLoadTextureBlock_4b(gDisplayListHead++, texture, G_IM_FMT_I, width, height, 0, G_TX_NOMIRROR | G_TX_CLAMP,
    // G_TX_NOMIRROR | G_TX_CLAMP, G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);

    gDPSetTextureImage(gDisplayListHead++, G_IM_FMT_IA, G_IM_SIZ_16b, 1, texture);
    gDPSetTile(gDisplayListHead++, G_IM_FMT_IA, G_IM_SIZ_16b, 0, G_TX_RENDERTILE, G_TX_LOADTILE, 0,
               G_TX_NOMIRROR | G_TX_CLAMP, G_TX_NOMASK, G_TX_NOLOD, G_TX_NOMIRROR | G_TX_CLAMP, G_TX_NOMASK,
               G_TX_NOLOD);
    gDPLoadSync(gDisplayListHead++);
    // The last argument to this macro really should be `CALC_DXT_4b(width)` but that creates a massive diff
    gDPLoadBlock(gDisplayListHead++, G_TX_LOADTILE, 0, 0, (((width * height) + 3) >> 2) - 1,
                 ((width / 16) + 2047) / (width / 16));
    gDPPipeSync(gDisplayListHead++);
    gDPSetTile(gDisplayListHead++, G_IM_FMT_IA, G_IM_SIZ_4b, (((width >> 1) + 7) >> 3), G_TX_RENDERTILE,
               G_TX_RENDERTILE, 0, G_TX_NOMIRROR | G_TX_CLAMP, G_TX_NOMASK, G_TX_NOLOD, G_TX_NOMIRROR | G_TX_CLAMP,
               G_TX_NOMASK, G_TX_NOLOD);
    gDPSetTileSize(gDisplayListHead++, G_TX_RENDERTILE, 0, 0, (width - 1) << G_TEXTURE_IMAGE_FRAC,
                   (height - 1) << G_TEXTURE_IMAGE_FRAC);
}

UNUSED void func_80044AB8(u8* texture, s32 width, s32 height) {
    gDPLoadTextureTile_4b(gDisplayListHead++, texture, G_IM_FMT_IA, width, height, 0, 0, width - 1, height - 1, 0,
                          G_TX_NOMIRROR | G_TX_CLAMP, G_TX_NOMIRROR | G_TX_CLAMP, G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD,
                          G_TX_NOLOD);
}

void func_80044BF8(u8* texture, s32 width, s32 height) {
    gDPLoadTextureBlock(gDisplayListHead++, texture, G_IM_FMT_I, G_IM_SIZ_8b, width, height, 0,
                        G_TX_NOMIRROR | G_TX_CLAMP, G_TX_NOMIRROR | G_TX_CLAMP, G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD,
                        G_TX_NOLOD);
}

void func_80044DA0(u8* image, s32 width, s32 height) {
    // This macro ought to be equivalent to the block of macros below but it doesn't match
    // See comment above the `gDPLoadBlock` macro
    // gDPLoadTextureBlock_4b(gDisplayListHead++, image, G_IM_FMT_I, width, height, 0, G_TX_NOMIRROR | G_TX_CLAMP,
    // G_TX_NOMIRROR | G_TX_CLAMP, G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);

    gDPSetTextureImage(gDisplayListHead++, G_IM_FMT_I, G_IM_SIZ_16b, 1, image);
    gDPSetTile(gDisplayListHead++, G_IM_FMT_I, G_IM_SIZ_16b, 0, G_TX_RENDERTILE, G_TX_LOADTILE, 0,
               G_TX_NOMIRROR | G_TX_CLAMP, G_TX_NOMASK, G_TX_NOLOD, G_TX_NOMIRROR | G_TX_CLAMP, G_TX_NOMASK,
               G_TX_NOLOD);
    gDPLoadSync(gDisplayListHead++);
    // The last argument to this macro really should be `CALC_DXT_4b(width)` but that creates a massive diff
    gDPLoadBlock(gDisplayListHead++, G_TX_LOADTILE, 0, 0, (((width * height) + 3) >> 2) - 1,
                 ((width / 16) + 2047) / (width / 16));
    gDPPipeSync(gDisplayListHead++);
    gDPSetTile(gDisplayListHead++, G_IM_FMT_I, G_IM_SIZ_4b, (((width >> 1) + 7) >> 3), G_TX_RENDERTILE, G_TX_RENDERTILE,
               0, G_TX_NOMIRROR | G_TX_CLAMP, G_TX_NOMASK, G_TX_NOLOD, G_TX_NOMIRROR | G_TX_CLAMP, G_TX_NOMASK,
               G_TX_NOLOD);
    gDPSetTileSize(gDisplayListHead++, G_TX_RENDERTILE, 0, 0, (width - 1) << G_TEXTURE_IMAGE_FRAC,
                   (height - 1) << G_TEXTURE_IMAGE_FRAC);
}

// Appears to be a complete copy of `func_80044F34`?
void func_80044F34(u8* image, s32 width, s32 height) {
    // This macro ought to be equivalent to the block of macros below but it doesn't match
    // See comment above the `gDPLoadBlock` macro
    // gDPLoadTextureBlock_4b(gDisplayListHead++, image, G_IM_FMT_I, width, height, 0, G_TX_NOMIRROR | G_TX_CLAMP,
    // G_TX_NOMIRROR | G_TX_CLAMP, G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);

    gDPSetTextureImage(gDisplayListHead++, G_IM_FMT_I, G_IM_SIZ_16b, 1, image);
    gDPSetTile(gDisplayListHead++, G_IM_FMT_I, G_IM_SIZ_16b, 0, G_TX_RENDERTILE, G_TX_LOADTILE, 0,
               G_TX_NOMIRROR | G_TX_CLAMP, G_TX_NOMASK, G_TX_NOLOD, G_TX_NOMIRROR | G_TX_CLAMP, G_TX_NOMASK,
               G_TX_NOLOD);
    gDPLoadSync(gDisplayListHead++);
    // The last argument to this macro really should be `CALC_DXT_4b(width)` but that creates a massive diff
    gDPLoadBlock(gDisplayListHead++, G_TX_LOADTILE, 0, 0, (((width * height) + 3) >> 2) - 1,
                 ((width / 16) + 2047) / (width / 16));
    gDPPipeSync(gDisplayListHead++);
    gDPSetTile(gDisplayListHead++, G_IM_FMT_I, G_IM_SIZ_4b, (((width >> 1) + 7) >> 3), G_TX_RENDERTILE, G_TX_RENDERTILE,
               0, G_TX_NOMIRROR | G_TX_CLAMP, G_TX_NOMASK, G_TX_NOLOD, G_TX_NOMIRROR | G_TX_CLAMP, G_TX_NOMASK,
               G_TX_NOLOD);
    gDPSetTileSize(gDisplayListHead++, G_TX_RENDERTILE, 0, 0, (width - 1) << G_TEXTURE_IMAGE_FRAC,
                   (height - 1) << G_TEXTURE_IMAGE_FRAC);
}

void func_800450C8(u8* image, s32 width, s32 height) {
    // This macro ought to be equivalent to the block of macros below but it doesn't match
    // See comment above the `gDPLoadBlock` macro
    // gDPLoadTextureBlock_4b(gDisplayListHead++, image, G_IM_FMT_I, width, height, 0, G_TX_NOMIRROR | G_TX_CLAMP,
    // G_TX_MIRROR | G_TX_WRAP, G_TX_NOMASK, masks, G_TX_NOLOD, G_TX_NOLOD);
    s32 masks = func_80043A54(width);

    gDPSetTextureImage(gDisplayListHead++, G_IM_FMT_I, G_IM_SIZ_16b, 1, image);
    gDPSetTile(gDisplayListHead++, G_IM_FMT_I, G_IM_SIZ_16b, 0, G_TX_RENDERTILE, G_TX_LOADTILE, 0,
               G_TX_NOMIRROR | G_TX_CLAMP, G_TX_NOMASK, G_TX_NOLOD, G_TX_MIRROR | G_TX_WRAP, masks, G_TX_NOLOD);
    gDPLoadSync(gDisplayListHead++);
    // The last argument to this macro really should be `CALC_DXT_4b(width)` but that creates a massive diff
    gDPLoadBlock(gDisplayListHead++, G_TX_LOADTILE, 0, 0, (((width * height) + 3) >> 2) - 1,
                 ((width / 16) + 2047) / (width / 16));
    gDPPipeSync(gDisplayListHead++);
    gDPSetTile(gDisplayListHead++, G_IM_FMT_I, G_IM_SIZ_4b, (((width >> 1) + 7) >> 3), G_TX_RENDERTILE, G_TX_RENDERTILE,
               0, G_TX_NOMIRROR | G_TX_CLAMP, G_TX_NOMASK, G_TX_NOLOD, G_TX_MIRROR | G_TX_WRAP, masks, G_TX_NOLOD);
    gDPSetTileSize(gDisplayListHead++, G_TX_RENDERTILE, 0, 0, (width - 1) << G_TEXTURE_IMAGE_FRAC,
                   (height - 1) << G_TEXTURE_IMAGE_FRAC);
}

void rsp_load_texture(u8* texture, s32 width, s32 height) {
    gDPLoadTextureBlock(gDisplayListHead++, texture, G_IM_FMT_CI, G_IM_SIZ_8b, width, height, 0,
                        G_TX_NOMIRROR | G_TX_CLAMP, G_TX_NOMIRROR | G_TX_CLAMP, G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD,
                        G_TX_NOLOD);
}

void rsp_load_texture_mask(u8* texture, s32 width, s32 height, s32 someMask) {
    gDPLoadTextureBlock(gDisplayListHead++, texture, G_IM_FMT_CI, G_IM_SIZ_8b, width, height, 0,
                        G_TX_MIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_CLAMP, someMask, G_TX_NOMASK, G_TX_NOLOD,
                        G_TX_NOLOD);
}

UNUSED void func_80045614(u8* texture, s32 width, s32 height) {
    gDPLoadTextureTile(gDisplayListHead++, texture, G_IM_FMT_CI, G_IM_SIZ_8b, width, height, 0, 0, width - 1,
                       height - 1, 0, G_TX_NOMIRROR | G_TX_CLAMP, G_TX_NOMIRROR | G_TX_CLAMP, G_TX_NOMASK, G_TX_NOMASK,
                       G_TX_NOLOD, G_TX_NOLOD);
}

UNUSED void func_80045738(u8* image1, u8* image2, s32 width, s32 height) {
    gDPSetCombineLERP(gDisplayListHead++, 0, 0, 0, TEXEL0, 0, 0, 0, TEXEL1, 0, 0, 0, COMBINED, 0, 0, 0, COMBINED);

    gDPLoadMultiBlock(gDisplayListHead++, image2, 0x100, G_TX_RENDERTILE, G_IM_FMT_I, G_IM_SIZ_8b, width, height, 0,
                      G_TX_MIRROR | G_TX_CLAMP, G_TX_MIRROR | G_TX_CLAMP, G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD,
                      G_TX_NOLOD);
    gDPLoadTextureBlock(gDisplayListHead++, image1, G_IM_FMT_RGBA, G_IM_SIZ_16b, width, height, 0,
                        G_TX_MIRROR | G_TX_CLAMP, G_TX_MIRROR | G_TX_CLAMP, G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD,
                        G_TX_NOLOD);

    gDPSetTile(gDisplayListHead++, G_IM_FMT_I, G_IM_SIZ_8b, (width + 7) >> 3, 0x0100, 1, 0, G_TX_NOMIRROR | G_TX_WRAP,
               G_TX_NOMASK, G_TX_NOLOD, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK, G_TX_NOLOD);
    ;
    gDPSetTileSize(gDisplayListHead++, 1, 0, 0, (width - 1) << G_TEXTURE_IMAGE_FRAC,
                   (height - 1) << G_TEXTURE_IMAGE_FRAC);
}

void func_80045B2C(Vtx* arg0) {
    gSPVertex(gDisplayListHead++, arg0, 4, 0);
    gSPDisplayList(gDisplayListHead++, common_rectangle_display);
}

void func_80045B74(Vtx* arg0) {
    gSPVertex(gDisplayListHead++, arg0, 3, 0);
    gSPDisplayList(gDisplayListHead++, D_0D006930);
}

UNUSED void func_80045BBC(Vec3f arg0, Vec3su arg1, f32 arg2, Vtx* arg3) {
    rsp_set_matrix_transformation(arg0, arg1, arg2);
    gSPDisplayList(gDisplayListHead++, D_0D0078A0);
    gSPVertex(gDisplayListHead++, arg3, 4, 0);
    gSPDisplayList(gDisplayListHead++, common_rectangle_display);
}

UNUSED void func_80045C48(Vec3f arg0, Vec3su arg1, f32 arg2, Vtx* arg3) {
    rsp_set_matrix_transformation(arg0, arg1, arg2);
    gSPDisplayList(gDisplayListHead++, D_0D0078D0);
    gSPClearGeometryMode(gDisplayListHead++, G_CULL_BOTH);
    gSPVertex(gDisplayListHead++, arg3, 4, 0);
    gSPDisplayList(gDisplayListHead++, common_rectangle_display);
    gSPSetGeometryMode(gDisplayListHead++, G_CULL_BACK);
}

void func_80045D0C(u8* texture, Vtx* arg1, s32 width, s32 arg3, s32 height) {
    s32 heightIndex;
    s32 vertexIndex = 0;
    u8* img = texture;

    for (heightIndex = 0; heightIndex < arg3 / height; heightIndex++) {
        load_texture_block_rgba16_mirror(img, width, height);
        func_80045B2C(&arg1[vertexIndex]);
        img += width * height * 2;
        vertexIndex += 4;
    }
    gSPTexture(gDisplayListHead++, 1, 1, 0, G_TX_RENDERTILE, G_OFF);
}

void func_80045E10(u8* texture, Vtx* arg1, s32 width, s32 arg3, s32 height) {
    s32 heightIndex;
    s32 vertexIndex = 0;
    u8* img = texture;

    for (heightIndex = 0; heightIndex < arg3 / height; heightIndex++) {
        load_texture_block_rgba16_mirror(img, width, height);
        func_80045B2C(&arg1[vertexIndex]);
        img += width * (height - 1) * 2;
        vertexIndex += 4;
    }
    gSPTexture(gDisplayListHead++, 1, 1, 0, G_TX_RENDERTILE, G_OFF);
}

void func_80045F18(u8* texture, Vtx* arg1, s32 width, s32 arg3, s32 height, s32 someMask) {
    s32 heightIndex;
    s32 vertexIndex = 0;
    u8* img = texture;

    for (heightIndex = 0; heightIndex < arg3 / height; heightIndex++) {
        load_texture_block_rgba16_nomirror(img, width, height, someMask);
        func_80045B2C(&arg1[vertexIndex]);
        img += width * (height - 1) * 2;
        vertexIndex += 4;
    }
    gSPTexture(gDisplayListHead++, 1, 1, 0, G_TX_RENDERTILE, G_OFF);
}

//! @todo tlut/texture unconfirmed. This could be texture1 and texture2
UNUSED void func_80046030(u8* tlut, u8* texture, Vtx* arg2, s32 width, s32 arg4, s32 height) {
    s32 var_s0 = 0;
    u8* img1 = tlut;
    u8* img2 = texture;
    s32 temp_lo_2;
    s32 var;
    s32 i;

    gSPDisplayList(gDisplayListHead++, D_0D008138);

    for (i = 0; i < arg4 / height; i++) {
        func_80045738(img1, img2, width, height);
        func_80045B2C(&arg2[var_s0]);
        var = height - 1;
        temp_lo_2 = (width * var);
        img1 += temp_lo_2 * 2;
        img2 += temp_lo_2;
        var_s0 += 4;
    }
    gSPTexture(gDisplayListHead++, 0x0001, 0x0001, 0, G_TX_RENDERTILE, G_OFF);
    gSPDisplayList(gDisplayListHead++, D_0D008120);
}

void func_800461A4(u8* texture, Vtx* arg1, s32 width, s32 arg3, s32 height) {
    s32 heightIndex;
    s32 vertexIndex = 0;
    u8* img = texture;

    for (heightIndex = 0; heightIndex < arg3 / height; heightIndex++) {
        load_texture_block_rgba32_nomirror(img, width, height);
        func_80045B2C(&arg1[vertexIndex]);
        img += width * height * 4;
        vertexIndex += 4;
    }
    gSPTexture(gDisplayListHead++, 1, 1, 0, G_TX_RENDERTILE, G_OFF);
}

void func_800462A8(u8* texture, Vtx* arg1, s32 width, s32 arg3, s32 height) {
    s32 heightIndex;
    s32 vertexIndex = 0;
    u8* img = texture;

    for (heightIndex = 0; heightIndex < arg3 / height; heightIndex++) {
        load_texture_block_rgba32_nomirror(img, width, height);
        func_80045B2C(&arg1[vertexIndex]);
        img += width * (height - 1) * 4;
        vertexIndex += 4;
    }
    gSPTexture(gDisplayListHead++, 1, 1, 0, G_TX_RENDERTILE, G_OFF);
}

void func_800463B0(s32 arg0, s32 arg1, u16 arg2, f32 arg3, u8* texture, Vtx* arg5, s32 arg6, s32 arg7, UNUSED s32 arg8,
                   s32 arg9) {

    switch (gScreenModeSelection) {
        case SCREEN_MODE_1P:
        case SCREEN_MODE_2P_SPLITSCREEN_HORIZONTAL:
        case SCREEN_MODE_2P_SPLITSCREEN_VERTICAL:
            func_80042330(arg0, arg1, arg2, arg3);
            break;
        case SCREEN_MODE_3P_4P_SPLITSCREEN:
            func_80042330_unchanged(arg0, arg1, arg2, arg3);
            break;
    }

    gSPDisplayList(gDisplayListHead++, D_0D007928);
    func_80045D0C(texture, arg5, arg6, arg7, arg9);
}

void func_80046424(s32 arg0, s32 arg1, u16 arg2, f32 arg3, u8* texture, Vtx* arg5, s32 arg6, s32 arg7, UNUSED s32 arg8,
                   s32 arg9) {
    switch (gScreenModeSelection) {
        case SCREEN_MODE_1P:
        case SCREEN_MODE_2P_SPLITSCREEN_HORIZONTAL:
        case SCREEN_MODE_2P_SPLITSCREEN_VERTICAL:
            func_80042330(arg0, arg1, arg2, arg3);
            break;
        case SCREEN_MODE_3P_4P_SPLITSCREEN:
            func_80042330_unchanged(arg0, arg1, arg2, arg3);
            break;
    }

    gSPDisplayList(gDisplayListHead++, D_0D007968);
    func_8004B614(D_801656C0, D_801656D0, D_801656E0, 128, 128, 128, 255);
    func_80045D0C(texture, arg5, arg6, arg7, arg9);
}

UNUSED void func_800464D0(s32 arg0, s32 arg1, u16 arg2, f32 arg3, u8* texture, Vtx* arg5, s32 arg6, s32 arg7,
                          UNUSED s32 arg8, s32 arg9) {
    func_80042330(arg0, arg1, arg2, arg3);
    gSPDisplayList(gDisplayListHead++, D_0D007948);
    func_80045E10(texture, arg5, arg6, arg7, arg9);
}

UNUSED void func_80046544(s32 arg0, s32 arg1, u16 arg2, f32 arg3, u8* texture, Vtx* arg5, s32 arg6, s32 arg7,
                          UNUSED s32 arg8, s32 arg9) {
    func_80042330(arg0, arg1, arg2, arg3);
    gSPDisplayList(gDisplayListHead++, D_0D0079A8);
    func_80045E10(texture, arg5, arg6, arg7, arg9);
}

void func_800465B8(s32 arg0, s32 arg1, u16 arg2, f32 arg3, s32 arg4, u8* texture, Vtx* arg6, s32 arg7, s32 arg8,
                   UNUSED s32 arg9, s32 argA) {
    func_80042330(arg0, arg1, arg2, arg3);
    gSPDisplayList(gDisplayListHead++, D_0D0079E8);

    set_transparency(arg4);
    func_80045E10(texture, arg6, arg7, arg8, argA);
}

UNUSED void func_80046634(s32 arg0, s32 arg1, u16 arg2, f32 arg3, u8* tlut, u8* texture, Vtx* arg6, s32 arg7, s32 arg8,
                          UNUSED s32 arg9, s32 argA) {
    func_80042330(arg0, arg1, arg2, arg3);
    gSPDisplayList(gDisplayListHead++, D_0D007948);
    func_80046030(tlut, texture, arg6, arg7, arg8, argA);
}

void func_800466B0(s32 arg0, s32 arg1, u16 arg2, f32 arg3, u8* texture, Vtx* arg5, s32 arg6, s32 arg7) {
    func_80042330(arg0, arg1, arg2, arg3);
    gSPDisplayList(gDisplayListHead++, D_0D007948);
    load_texture_block_rgba16_mirror(texture, arg6, arg7);
    func_80045B74(arg5);
}

UNUSED void func_80046720(s32 arg0, s32 arg1, u16 arg2, f32 arg3, u8* texture, Vtx* arg5, s32 arg6, s32 arg7,
                          UNUSED s32 arg8, s32 arg9) {
    func_80042330(arg0, arg1, arg2, arg3);
    gSPDisplayList(gDisplayListHead++, D_0D007928);
    func_800461A4(texture, arg5, arg6, arg7, arg9);
}

UNUSED void func_80046794(s32 arg0, s32 arg1, u16 arg2, f32 arg3, u8* texture, Vtx* arg5, s32 arg6, s32 arg7,
                          UNUSED s32 arg8, s32 arg9) {
    func_80042330(arg0, arg1, arg2, arg3);
    gSPDisplayList(gDisplayListHead++, D_0D007948);
    func_800462A8(texture, arg5, arg6, arg7, arg9);
}

void func_80046808(Vec3f arg0, Vec3su arg1, f32 arg2, u8* texture, Vtx* arg4, s32 arg5, s32 arg6, UNUSED s32 arg7,
                   s32 arg8) {
    rsp_set_matrix_transformation(arg0, arg1, arg2);
    gSPDisplayList(gDisplayListHead++, D_0D007948);
    func_80045E10(texture, arg4, arg5, arg6, arg8);
}

UNUSED void func_80046874(Vec3f arg0, Vec3su arg1, f32 arg2, u8* texture, Vtx* arg4, s32 arg5, s32 arg6,
                          UNUSED s32 arg7, s32 arg8) {
    rsp_set_matrix_transformation(arg0, arg1, arg2);
    gSPDisplayList(gDisplayListHead++, D_0D0079C8);
    func_80045E10(texture, arg4, arg5, arg6, arg8);
}

void func_800468E0(Vec3f arg0, Vec3su arg1, f32 arg2, u8* texture, Vtx* arg4, s32 arg5, s32 arg6, UNUSED s32 arg7,
                   s32 arg8, s32 arg9) {
    rsp_set_matrix_transformation(arg0, arg1, arg2);
    gSPDisplayList(gDisplayListHead++, D_0D0079C8);
    func_80045F18(texture, arg4, arg5, arg6, arg8, arg9);
}

UNUSED void func_80046954(Vec3f arg0, Vec3su arg1, f32 arg2, u8* texture, Vtx* arg4, s32 arg5, s32 arg6,
                          UNUSED s32 arg7, s32 arg8) {
    rsp_set_matrix_transformation(arg0, arg1, arg2);
    gSPDisplayList(gDisplayListHead++, D_0D0079C8);
    gSPClearGeometryMode(gDisplayListHead++, G_CULL_BOTH);
    func_80045E10(texture, arg4, arg5, arg6, arg8);
    gSPSetGeometryMode(gDisplayListHead++, G_CULL_BACK);
}

void func_80046A00(Vec3f arg0, Vec3su arg1, f32 arg2, u8* texture, Vtx* arg4, s32 arg5, s32 arg6) {
    rsp_set_matrix_transformation(arg0, arg1, arg2);
    gSPDisplayList(gDisplayListHead++, D_0D007948);
    load_texture_block_rgba16_mirror(texture, arg5, arg6);
    func_80045B74(arg4);
}

UNUSED void func_80046A68(Vec3f arg0, Vec3su arg1, f32 arg2, u8* texture, Vtx* arg4, s32 arg5, s32 arg6,
                          UNUSED s32 arg7, s32 arg8) {
    rsp_set_matrix_transformation(arg0, arg1, arg2);
    gSPDisplayList(gDisplayListHead++, D_0D0079C8);
    func_800462A8(texture, arg4, arg5, arg6, arg8);
}

UNUSED void func_80046AD4(s32 arg0, s32 arg1, u16 arg2, f32 arg3, u8* texture) {
    func_800464D0(arg0, arg1, arg2, arg3, texture, common_vtx_player_minimap_icon, 8, 8, 8, 8);
}

UNUSED void func_80046B38(s32 arg0, s32 arg1, u16 arg2, f32 arg3, u8* texture) {
    func_800464D0(arg0, arg1, arg2, arg3, texture, common_vtx_rectangle, 16, 16, 16, 16);
}

UNUSED void func_80046B9C(Vec3f arg0, Vec3su arg1, f32 arg2, u8* texture) {
    func_80046808(arg0, arg1, arg2, texture, common_vtx_rectangle, 16, 16, 16, 16);
}

UNUSED void func_80046BEC(s32 arg0, s32 arg1, u16 arg2, f32 arg3, u8* texture, Vtx* arg5) {
    func_800466B0(arg0, arg1, arg2, arg3, texture, arg5, 16, 16);
}

UNUSED void func_80046C3C(Vec3f arg0, Vec3su arg1, f32 arg2, u8* texture, Vtx* arg4) {
    func_80046A00(arg0, arg1, arg2, texture, arg4, 16, 16);
}

UNUSED void func_80046C78(s32 arg0, s32 arg1, u16 arg2, f32 arg3, u8* texture) {
    func_800464D0(arg0, arg1, arg2, arg3, texture, D_0D005AE0, 32, 32, 32, 32);
}

UNUSED void func_80046CDC(s32 arg0, s32 arg1, u16 arg2, f32 arg3, u8* texture) {
    func_800464D0(arg0, arg1, arg2, arg3, texture, D_0D005FB0, 64, 32, 64, 32);
}

UNUSED void func_80046D40(Vec3f arg0, Vec3su arg1, f32 arg2, u8* texture) {
    func_80046808(arg0, arg1, arg2, texture, D_0D005FB0, 64, 32, 64, 32);
}

UNUSED void func_80046D90(s32 arg0, s32 arg1, u16 arg2, f32 arg3, u8* texture) {
    Vtx* vtx = (Vtx*) LOAD_ASSET(common_vtx_hedgehog);
    func_800464D0(arg0, arg1, arg2, arg3, texture, vtx, 64, 64, 64, 32);
}

UNUSED void func_80046DF4(s32 arg0, s32 arg1, u16 arg2, f32 arg3, s32 arg4, u8* texture) {
    Vtx* vtx = (Vtx*) LOAD_ASSET(common_vtx_hedgehog);
    func_800465B8(arg0, arg1, arg2, arg3, arg4, texture, vtx, 64, 64, 64, 32);
}

void load_texture_and_tlut(u8* tlut, u8* texture, s32 width, s32 height) {
    gSPDisplayList(gDisplayListHead++, D_0D007D78);
    gDPLoadTLUT_pal256(gDisplayListHead++, tlut);
    rsp_load_texture(texture, width, height);
}

void func_80047068(u8* tlut, u8* texture, Vtx* arg2, UNUSED s32 arg3, s32 arg4, s32 width, s32 height) {
    s32 heightIndex;
    s32 vertexIndex = 0;
    u8* img = texture;

    gDPLoadTLUT_pal256(gDisplayListHead++, tlut);
    for (heightIndex = 0; heightIndex < arg4 / height; heightIndex++) {
        rsp_load_texture(img, width, height);
        gSPVertex(gDisplayListHead++, &arg2[vertexIndex], 4, 0);
        gSPDisplayList(gDisplayListHead++, common_rectangle_display);
        img += width * height;
        vertexIndex += 4;
    }
    gSPTexture(gDisplayListHead++, 1, 1, 0, G_TX_RENDERTILE, G_OFF);
}

void draw_rectangle_texture_overlap(u8* tlut, u8* texture, Vtx* arg2, UNUSED s32 w, s32 height, s32 width,
                                    s32 heighthalf) {
    s32 heightIndex;
    s32 vertexIndex = 0;
    u8* img = texture;

    gDPLoadTLUT_pal256(gDisplayListHead++, tlut);
    for (heightIndex = 0; heightIndex < height / heighthalf; heightIndex++) {
        rsp_load_texture(img, width, heighthalf);
        gSPVertex(gDisplayListHead++, &arg2[vertexIndex], 4, 0);
        gSPDisplayList(gDisplayListHead++, common_rectangle_display);
        img += width * (heighthalf - 1);
        vertexIndex += 4;
    }
    gSPTexture(gDisplayListHead++, 1, 1, 0, G_TX_RENDERTILE, G_OFF);
}

void func_8004747C(u8* tlut, u8* texture, Vtx* arg2, UNUSED s32 arg3, s32 arg4, s32 width, s32 height, s32 someMask) {
    s32 heightIndex;
    s32 vertexIndex = 0;
    u8* img = texture;

    gDPLoadTLUT_pal256(gDisplayListHead++, tlut);
    for (heightIndex = 0; heightIndex < arg4 / height; heightIndex++) {
        rsp_load_texture_mask(img, width, height, someMask);
        gSPVertex(gDisplayListHead++, &arg2[vertexIndex], 4, 0);
        gSPDisplayList(gDisplayListHead++, common_rectangle_display);
        img += width * (height - 1);
        vertexIndex += 4;
    }
    gSPTexture(gDisplayListHead++, 1, 1, 0, G_TX_RENDERTILE, G_OFF);
}

void func_8004768C(u8* tlut, u8* texture, Vtx* arg2, s32 arg3, s32 width, s32 height) {
    s32 heightIndex;
    s32 vertexIndex = 0;
    u8* img = texture;

    gDPLoadTLUT_pal256(gDisplayListHead++, tlut);
    for (heightIndex = 0; heightIndex < arg3 / height; heightIndex++) {
        // Something seems off about arguments here, but if it matches it matches
        rsp_load_texture(img, height, width);
        gSPVertex(gDisplayListHead++, &arg2[vertexIndex], 4, 0);
        gSPDisplayList(gDisplayListHead++, common_rectangle_display);
        img += height * width;
        vertexIndex += 4;
    }
    gSPTexture(gDisplayListHead++, 1, 1, 0, G_TX_RENDERTILE, G_OFF);
}

void func_8004788C(s32 arg0, s32 arg1, u16 arg2, f32 arg3, u8* tlut, u8* texture, Vtx* arg6, s32 arg7, s32 arg8,
                   s32 arg9, s32 argA) {
    func_80042330(arg0, arg1, arg2, arg3);
    gSPDisplayList(gDisplayListHead++, D_0D007CB8);
    func_80047068(tlut, texture, arg6, arg7, arg8, arg9, argA);
}

void func_80047910(s32 x, s32 y, u16 angle, f32 size, u8* tlut, u8* texture, Vtx* arg6, s32 arg7, s32 arg8, s32 arg9,
                   s32 argA) {
    func_80042330(x, y, angle, size);
    gSPDisplayList(gDisplayListHead++, D_0D007CD8);
    draw_rectangle_texture_overlap(tlut, texture, arg6, arg7, arg8, arg9, argA);
}

void func_80047994(s32 arg0, s32 arg1, u16 arg2, f32 arg3, u8* tlut, u8* texture, Vtx* arg6, s32 arg7, s32 arg8,
                   s32 arg9, s32 argA) {
    func_80042330(arg0, arg1, arg2, arg3);
    gSPDisplayList(gDisplayListHead++, D_0D007CF8);
    draw_rectangle_texture_overlap(tlut, texture, arg6, arg7, arg8, arg9, argA);
}

void func_80047A18(s32 arg0, s32 arg1, u16 arg2, f32 arg3, u8* tlut, u8* texture, Vtx* arg6, s32 arg7, s32 arg8,
                   s32 arg9, s32 argA) {
    func_80042330(arg0, arg1, arg2, arg3);
    gSPDisplayList(gDisplayListHead++, D_0D007D18);
    func_80047068(tlut, texture, arg6, arg7, arg8, arg9, argA);
}

void func_80047A9C(s32 arg0, s32 arg1, u16 arg2, f32 arg3, u8* tlut, u8* texture, Vtx* arg6, s32 arg7, s32 arg8,
                   s32 arg9, s32 argA) {
    func_80042330(arg0, arg1, arg2, arg3);
    gSPDisplayList(gDisplayListHead++, D_0D007D38);
    draw_rectangle_texture_overlap(tlut, texture, arg6, arg7, arg8, arg9, argA);
}

UNUSED void func_80047B20(s32 arg0, s32 arg1, u16 arg2, f32 arg3, u8* tlut, u8* texture, Vtx* arg6, s32 arg7, s32 arg8,
                          s32 arg9) {
    func_80042330(arg0, arg1, arg2, arg3);
    gSPDisplayList(gDisplayListHead++, D_0D007D38);
    func_8004768C(tlut, texture, arg6, arg7, arg8, arg9);
}

void func_80047B9C(s32 arg0, s32 arg1, u16 arg2, f32 arg3, s32 arg4, u8* tlut, u8* texture, Vtx* arg7, s32 arg8,
                   s32 arg9, s32 argA, s32 argB) {
    func_80042330(arg0, arg1, arg2, arg3);
    gSPDisplayList(gDisplayListHead++, D_0D007DB8);
    set_transparency(arg4);
    func_80047068(tlut, texture, arg7, arg8, arg9, argA, argB);
}

UNUSED void func_80047C28(s32 arg0, s32 arg1, u16 arg2, f32 arg3, s32 arg4, u8* tlut, u8* texture, Vtx* arg7, s32 arg8,
                          s32 arg9, s32 argA, s32 argB) {
    func_80042330(arg0, arg1, arg2, arg3);
    gSPDisplayList(gDisplayListHead++, D_0D007DD8);
    set_transparency(arg4);
    draw_rectangle_texture_overlap(tlut, texture, arg7, arg8, arg9, argA, argB);
}

void func_80047CB4(s32 arg0, s32 arg1, u16 arg2, f32 arg3, s32 arg4, u8* tlut, u8* texture, Vtx* arg7, s32 arg8,
                   s32 arg9, s32 argA, s32 argB) {
    func_80042330(arg0, arg1, arg2, arg3);
    gSPDisplayList(gDisplayListHead++, D_0D007E38);
    set_transparency(arg4);
    func_80047068(tlut, texture, arg7, arg8, arg9, argA, argB);
}

UNUSED void func_80047D40(s32 arg0, s32 arg1, u16 arg2, f32 arg3, s32 arg4, u8* tlut, u8* texture, Vtx* arg7, s32 arg8,
                          s32 arg9, s32 argA, s32 argB) {
    func_80042330(arg0, arg1, arg2, arg3);
    gSPDisplayList(gDisplayListHead++, D_0D007E58);
    set_transparency(arg4);
    draw_rectangle_texture_overlap(tlut, texture, arg7, arg8, arg9, argA, argB);
}

UNUSED void func_80047DCC(Vec3f arg0, Vec3su arg1, f32 arg2, u8* tlut, u8* texture, Vtx* arg5, s32 arg6, s32 arg7,
                          s32 arg8, s32 arg9) {
    rsp_set_matrix_transformation(arg0, arg1, arg2);
    gSPDisplayList(gDisplayListHead++, D_0D007CB8);
    func_80047068(tlut, texture, arg5, arg6, arg7, arg8, arg9);
}

void func_80047E48(Vec3f arg0, Vec3su arg1, f32 arg2, u8* tlut, u8* texture, Vtx* arg5, s32 arg6, s32 arg7, s32 arg8,
                   s32 arg9) {
    rsp_set_matrix_transformation(arg0, arg1, arg2);
    gSPDisplayList(gDisplayListHead++, D_0D007CD8);
    draw_rectangle_texture_overlap(tlut, texture, arg5, arg6, arg7, arg8, arg9);
}

UNUSED void func_80047EC4(Vec3f arg0, Vec3su arg1, f32 arg2, u8* tlut, u8* texture, Vtx* arg5, s32 arg6, s32 arg7,
                          s32 arg8, s32 arg9) {
    rsp_set_matrix_transformation(arg0, arg1, arg2);
    gSPDisplayList(gDisplayListHead++, D_0D007D18);
    func_80047068(tlut, texture, arg5, arg6, arg7, arg8, arg9);
}

void func_80047F40(Vec3f arg0, Vec3su arg1, f32 arg2, u8* tlut, u8* texture, Vtx* arg5, s32 arg6, s32 arg7, s32 arg8,
                   s32 arg9) {
    rsp_set_matrix_transformation(arg0, arg1, arg2);
    gSPDisplayList(gDisplayListHead++, D_0D007D38);
    draw_rectangle_texture_overlap(tlut, texture, arg5, arg6, arg7, arg8, arg9);
}

UNUSED void func_80047FBC(Vec3f arg0, Vec3su arg1, f32 arg2, u8* tlut, u8* texture, Vtx* arg5, s32 arg6, s32 arg7,
                          s32 arg8, s32 arg9) {
    rsp_set_matrix_transformation(arg0, arg1, arg2);
    gSPDisplayList(gDisplayListHead++, D_0D007D58);
    draw_rectangle_texture_overlap(tlut, texture, arg5, arg6, arg7, arg8, arg9);
}

UNUSED void func_80048038(Vec3f arg0, Vec3su arg1, f32 arg2, u8* tlut, u8* texture, Vtx* arg5, s32 arg6, s32 arg7,
                          s32 arg8, s32 arg9) {
    rsp_set_matrix_transformation(arg0, arg1, arg2);
    gSPDisplayList(gDisplayListHead++, D_0D007D98);
    func_80047068(tlut, texture, arg5, arg6, arg7, arg8, arg9);
}

void draw_2d_texture_at(Vec3f arg0, Vec3su arg1, f32 arg2, u8* tlut, u8* texture, Vtx* arg5, s32 width, s32 height,
                        s32 width2, s32 height2) {
    rsp_set_matrix_transformation(arg0, arg1, arg2);
    gSPDisplayList(gDisplayListHead++, D_0D007D78);
    draw_rectangle_texture_overlap(tlut, texture, arg5, width, height, width2, height2);
}

void func_80048130(Vec3f arg0, Vec3su arg1, f32 arg2, u8* tlut, u8* texture, Vtx* arg5, s32 arg6, s32 arg7, s32 arg8,
                   s32 arg9, s32 argA) {
    rsp_set_matrix_transformation(arg0, arg1, arg2);
    gSPDisplayList(gDisplayListHead++, D_0D007D78);
    func_8004747C(tlut, texture, arg5, arg6, arg7, arg8, arg9, argA);
}

UNUSED void func_800481B4(Vec3f arg0, Vec3su arg1, f32 arg2, u8* tlut, u8* texture, Vtx* arg5, s32 arg6, s32 arg7,
                          s32 arg8) {
    rsp_set_matrix_transformation(arg0, arg1, arg2);
    gSPDisplayList(gDisplayListHead++, D_0D007D78);
    func_8004768C(tlut, texture, arg5, arg6, arg7, arg8);
}

UNUSED void func_80048228(Vec3f arg0, Vec3su arg1, f32 arg2, s32 arg3, u8* tlut, u8* texture, Vtx* arg6, s32 arg7,
                          s32 arg8, s32 arg9, s32 argA) {
    rsp_set_matrix_transformation(arg0, arg1, arg2);
    gSPDisplayList(gDisplayListHead++, D_0D007DB8);
    set_transparency(arg3);
    func_80047068(tlut, texture, arg6, arg7, arg8, arg9, argA);
}

void func_800482AC(Vec3f arg0, Vec3su arg1, f32 arg2, s32 arg3, u8* tlut, u8* texture, Vtx* arg6, s32 arg7, s32 arg8,
                   s32 arg9, s32 argA) {
    rsp_set_matrix_transformation(arg0, arg1, arg2);
    gSPDisplayList(gDisplayListHead++, D_0D007DD8);
    set_transparency(arg3);
    draw_rectangle_texture_overlap(tlut, texture, arg6, arg7, arg8, arg9, argA);
}

UNUSED void func_80048330(Vec3f arg0, Vec3su arg1, f32 arg2, s32 arg3, u8* tlut, u8* texture, Vtx* arg6, s32 arg7,
                          s32 arg8, s32 arg9, s32 argA) {
    rsp_set_matrix_transformation(arg0, arg1, arg2);
    gSPDisplayList(gDisplayListHead++, D_0D007E38);
    set_transparency(arg3);
    func_80047068(tlut, texture, arg6, arg7, arg8, arg9, argA);
}

void func_800483B4(Vec3f arg0, Vec3su arg1, f32 arg2, s32 arg3, u8* tlut, u8* texture, Vtx* arg6, s32 arg7, s32 arg8,
                   s32 arg9, s32 argA) {
    rsp_set_matrix_transformation(arg0, arg1, arg2);
    gSPDisplayList(gDisplayListHead++, D_0D007E58);
    set_transparency(arg3);
    draw_rectangle_texture_overlap(tlut, texture, arg6, arg7, arg8, arg9, argA);
}

void func_80048438(Vec3f arg0, Vec3su arg1, f32 arg2, s32 arg3, u8* tlut, u8* texture, Vtx* arg6, s32 arg7, s32 arg8,
                   s32 arg9, s32 argA) {
    rsp_set_matrix_transformation(arg0, arg1, arg2);
    gSPDisplayList(gDisplayListHead++, D_0D007DF8);
    set_transparency(arg3);
    func_80047068(tlut, texture, arg6, arg7, arg8, arg9, argA);
}

void func_800484BC(Vec3f arg0, Vec3su arg1, f32 arg2, s32 arg3, u8* tlut, u8* texture, Vtx* arg6, s32 arg7, s32 arg8,
                   s32 arg9, s32 argA) {
    rsp_set_matrix_transformation(arg0, arg1, arg2);
    gSPDisplayList(gDisplayListHead++, D_0D007E18);
    set_transparency(arg3);
    gDPLoadTLUT_pal256(gDisplayListHead++, tlut);
    rsp_load_texture(texture, arg9, arg8);
    gSPVertex(gDisplayListHead++, arg6, 4, 0);
    gSPDisplayList(gDisplayListHead++, common_rectangle_display);
    gSPTexture(gDisplayListHead++, 1, 1, 0, G_TX_RENDERTILE, G_OFF);
}

void func_80048540(Vec3f arg0, Vec3su arg1, f32 arg2, s32 arg3, u8* tlut, u8* texture, Vtx* arg6, s32 arg7, s32 arg8,
                   s32 arg9, s32 argA) {
    rsp_set_matrix_transformation(arg0, arg1, arg2);
    gSPDisplayList(gDisplayListHead++, D_0D007E98);
    set_transparency(arg3);
    draw_rectangle_texture_overlap(tlut, texture, arg6, arg7, arg8, arg9, argA);
}

void func_800485C4(Vec3f arg0, Vec3su arg1, f32 arg2, s32 arg3, u8* tlut, u8* texture, Vtx* arg6, s32 arg7, s32 arg8,
                   s32 arg9, s32 argA) {
    rsp_set_matrix_transformation(arg0, arg1, arg2);
    gSPDisplayList(gDisplayListHead++, D_0D007E98);
    gDPSetAlphaCompare(gDisplayListHead++, G_AC_DITHER);
    gDPSetRenderMode(gDisplayListHead++, G_RM_AA_ZB_XLU_SURF, G_RM_AA_ZB_XLU_SURF2);

    set_transparency(arg3);
    draw_rectangle_texture_overlap(tlut, texture, arg6, arg7, arg8, arg9, argA);

    gDPSetAlphaCompare(gDisplayListHead++, G_AC_NONE);
}

UNUSED void func_800486B0(s32 arg0, s32 arg1, u16 arg2, f32 arg3, u8* tlut, u8* texture, Vtx* arg6) {
    func_8004788C(arg0, arg1, arg2, arg3, tlut, texture, arg6, 24, 48, 24, 48);
}

UNUSED void func_80048718(s32 arg0, s32 arg1, u16 arg2, f32 arg3, u8* tlut, u8* texture, Vtx* arg6) {
    func_8004788C(arg0, arg1, arg2, arg3, tlut, texture, arg6, 32, 32, 32, 32);
}

UNUSED void func_80048780(Vec3f arg0, Vec3su arg1, f32 arg2, s32 arg3, u8* tlut, u8* texture, Vtx* arg6) {
    func_80048540(arg0, arg1, arg2, arg3, tlut, texture, arg6, 48, 48, 48, 40);
}

UNUSED void func_800487DC(s32 arg0, s32 arg1, u16 arg2, f32 arg3, u8* tlut, u8* texture, Vtx* arg6) {
    func_8004788C(arg0, arg1, arg2, arg3, tlut, texture, arg6, 48, 48, 48, 48);
}

UNUSED void func_80048844(s32 arg0, s32 arg1, u16 arg2, f32 arg3, u8* tlut, u8* texture, Vtx* arg6) {
    func_8004788C(arg0, arg1, arg2, arg3, tlut, texture, arg6, 64, 32, 64, 32);
}

UNUSED void func_800488AC(s32 arg0, s32 arg1, u16 arg2, f32 arg3, u8* tlut, u8* texture, Vtx* arg6) {
    func_8004788C(arg0, arg1, arg2, arg3, tlut, texture, arg6, 64, 64, 64, 32);
}

UNUSED void func_80048914(s32 arg0, s32 arg1, u16 arg2, f32 arg3, u8* tlut, u8* texture, Vtx* arg6) {
    func_80047910(arg0, arg1, arg2, arg3, tlut, texture, arg6, 64, 64, 64, 32);
}

UNUSED void func_8004897C(s32 arg0, s32 arg1, u16 arg2, f32 arg3, u8* tlut, u8* texture, Vtx* arg6) {
    func_80047994(arg0, arg1, arg2, arg3, tlut, texture, arg6, 64, 64, 64, 32);
}

UNUSED void func_800489E4(s32 arg0, s32 arg1, u16 arg2, f32 arg3, u8* tlut, u8* texture, Vtx* arg6) {
    func_80047A18(arg0, arg1, arg2, arg3, tlut, texture, arg6, 64, 64, 64, 32);
}

UNUSED void func_80048A4C(s32 arg0, s32 arg1, u16 arg2, f32 arg3, u8* tlut, u8* texture, Vtx* arg6) {
    func_80047A9C(arg0, arg1, arg2, arg3, tlut, texture, arg6, 64, 64, 64, 32);
}

UNUSED void func_80048AB4(s32 arg0, s32 arg1, u16 arg2, f32 arg3, s32 arg4, u8* tlut, u8* texture, Vtx* arg7) {
    func_80047B9C(arg0, arg1, arg2, arg3, arg4, tlut, texture, arg7, 64, 64, 64, 32);
}

UNUSED void func_80048B24(s32 arg0, s32 arg1, u16 arg2, f32 arg3, s32 arg4, u8* tlut, u8* texture, Vtx* arg7) {
    func_80047CB4(arg0, arg1, arg2, arg3, arg4, tlut, texture, arg7, 64, 64, 64, 32);
}

UNUSED void func_80048B94(Vec3f arg0, Vec3su arg1, f32 arg2, u8* tlut, u8* texture, Vtx* arg5) {
    func_80047E48(arg0, arg1, arg2, tlut, texture, arg5, 64, 64, 64, 32);
}

UNUSED void func_80048BE8(Vec3f arg0, Vec3su arg1, f32 arg2, u8* tlut, u8* texture, Vtx* arg5) {
    func_80047F40(arg0, arg1, arg2, tlut, texture, arg5, 64, 64, 64, 32);
}

UNUSED void func_80048C3C(Vec3f arg0, Vec3su arg1, f32 arg2, u8* tlut, u8* texture, Vtx* arg5) {
    draw_2d_texture_at(arg0, arg1, arg2, tlut, texture, arg5, 64, 64, 64, 32);
}

UNUSED void func_80048C90(Vec3f arg0, Vec3su arg1, f32 arg2, s32 arg3, u8* tlut, u8* texture, Vtx* arg6) {
    func_800482AC(arg0, arg1, arg2, arg3, tlut, texture, arg6, 64, 64, 64, 32);
}

UNUSED void func_80048CEC(Vec3f arg0, Vec3su arg1, f32 arg2, s32 arg3, u8* tlut, u8* texture, Vtx* arg6) {
    func_800483B4(arg0, arg1, arg2, arg3, tlut, texture, arg6, 64, 64, 64, 32);
}

UNUSED void func_80048D48(Vec3f arg0, Vec3su arg1, f32 arg2, s32 arg3, u8* tlut, u8* texture, Vtx* arg6) {
    func_800484BC(arg0, arg1, arg2, arg3, tlut, texture, arg6, 64, 64, 64, 32);
}

UNUSED void func_80048DA4(Vec3f arg0, Vec3su arg1, f32 arg2, s32 arg3, u8* tlut, u8* texture, Vtx* arg6) {
    func_80048540(arg0, arg1, arg2, arg3, tlut, texture, arg6, 64, 64, 64, 32);
}

UNUSED void func_80048E00(s32 arg0, s32 arg1, u16 arg2, f32 arg3, u8* tlut, u8* texture, Vtx* arg6) {
    func_80047910(arg0, arg1, arg2, arg3, tlut, texture, arg6, 72, 48, 72, 24);
}

UNUSED void func_80048E68(s32 arg0, s32 arg1, u16 arg2, f32 arg3, u8* tlut, u8* texture, Vtx* arg6) {
    func_80047A18(arg0, arg1, arg2, arg3, tlut, texture, arg6, 72, 48, 72, 24);
}

UNUSED void func_80048ED0(s32 arg0, s32 arg1, u16 arg2, f32 arg3, u8* tlut, u8* texture, Vtx* arg6) {
    func_80047A9C(arg0, arg1, arg2, arg3, tlut, texture, arg6, 72, 48, 72, 24);
}

UNUSED void func_80048F38(Vec3f arg0, Vec3su arg1, f32 arg2, u8* tlut, u8* texture, Vtx* arg5) {
    draw_2d_texture_at(arg0, arg1, arg2, tlut, texture, arg5, 72, 48, 72, 24);
}

void func_80048F8C(u8* texture, Vtx* arg1, s32 arg2, s32 arg3, s32 width, s32 height) {
    s32 heightIndex;
    s32 widthIndex;
    s32 vertexIndex = 0;
    u8* img = texture;

    for (heightIndex = 0; heightIndex < arg3 / height; heightIndex++) {
        for (widthIndex = 0; widthIndex < arg2 / width; widthIndex++) {
            load_texture_block_ia16_nomirror(img, width, height);
            gSPVertex(gDisplayListHead++, &arg1[vertexIndex], 4, 0);
            gSPDisplayList(gDisplayListHead++, common_rectangle_display);
            img += width * height * 2;
            vertexIndex += 4;
        }
    }
    gSPTexture(gDisplayListHead++, 1, 1, 0, G_TX_RENDERTILE, G_OFF);
}

void func_80049130(u8* texture, Vtx* arg1, s32 arg2, s32 arg3, s32 width, s32 height) {
    s32 heightIndex;
    s32 widthIndex;
    s32 vertexIndex = 0;
    u8* img = texture;

    for (heightIndex = 0; heightIndex < arg3 / height; heightIndex++) {
        for (widthIndex = 0; widthIndex < arg2 / width; widthIndex++) {
            load_texture_tile_ia16_nomirror(img, width, height);
            gSPVertex(gDisplayListHead++, &arg1[vertexIndex], 4, 0);
            gSPDisplayList(gDisplayListHead++, common_rectangle_display);
            img += width * height * 2;
            vertexIndex += 4;
        }
    }
    gSPTexture(gDisplayListHead++, 1, 1, 0, G_TX_RENDERTILE, G_OFF);
}

void func_800492D4(u8* texture, Vtx* arg1, s32 arg2, s32 arg3, s32 width, s32 height) {
    s32 heightIndex;
    s32 widthIndex;
    s32 vertexIndex = 0;
    u8* img = texture;

    for (heightIndex = 0; heightIndex < arg3 / height; heightIndex++) {
        for (widthIndex = 0; widthIndex < arg2 / width; widthIndex++) {
            load_texture_block_ia8_nomirror(img, width, height);
            gSPVertex(gDisplayListHead++, &arg1[vertexIndex], 4, 0);
            gSPDisplayList(gDisplayListHead++, common_rectangle_display);
            img += width * height;
            vertexIndex += 4;
        }
    }
    gSPTexture(gDisplayListHead++, 1, 1, 0, G_TX_RENDERTILE, G_OFF);
}

void func_80049478(u8* texture, Vtx* arg1, s32 arg2, s32 arg3, s32 width, s32 height) {
    s32 heightIndex;
    s32 widthIndex;
    s32 vertexIndex = 0;
    u8* img = texture;

    for (heightIndex = 0; heightIndex < arg3 / height; heightIndex++) {
        for (widthIndex = 0; widthIndex < arg2 / width; widthIndex++) {
            load_texture_tile_ia8_nomirror(img, width, height);
            gSPVertex(gDisplayListHead++, &arg1[vertexIndex], 4, 0);
            gSPDisplayList(gDisplayListHead++, common_rectangle_display);
            img += width * height;
            vertexIndex += 4;
        }
    }
    gSPTexture(gDisplayListHead++, 1, 1, 0, G_TX_RENDERTILE, G_OFF);
}

void func_8004961C(u8* texture, Vtx* arg1, s32 arg2, s32 arg3, s32 width, s32 height) {
    s32 i;
    s32 j;
    s32 var_s2 = 0;
    u8* img = texture;

    for (i = 0; i < arg3 / height; i++) {
        for (j = 0; j < arg2 / width; j++) {

            func_80044924(img, width, height);
            gSPVertex(gDisplayListHead++, &arg1[var_s2], 4, 0);
            gSPDisplayList(gDisplayListHead++, common_rectangle_display);
            img += (width * height) / 2;
            var_s2 += 4;
        }
    }

    gSPTexture(gDisplayListHead++, 0x0001, 0x0001, 0, G_TX_RENDERTILE, G_OFF);
}

void func_800497CC(u8* texture, Vtx* arg1, s32 arg2, s32 arg3, s32 width, s32 height) {
    s32 heightIndex;
    s32 widthIndex;
    s32 vertexIndex = 0;
    u8* img = texture;

    for (heightIndex = 0; heightIndex < arg3 / height; heightIndex++) {
        for (widthIndex = 0; widthIndex < arg2 / width; widthIndex++) {
            func_80044BF8(img, width, height);
            gSPVertex(gDisplayListHead++, &arg1[vertexIndex], 4, 0);
            gSPDisplayList(gDisplayListHead++, common_rectangle_display);
            img += width * height;
            vertexIndex += 4;
        }
    }
    gSPTexture(gDisplayListHead++, 1, 1, 0, G_TX_RENDERTILE, G_OFF);
}

void func_80049970(u8* texture, Vtx* arg1, s32 arg2, s32 arg3, s32 width, s32 height) {
    s32 i;
    s32 j;
    s32 var_s2 = 0;
    u8* img = texture;

    for (i = 0; i < arg3 / height; i++) {
        for (j = 0; j < arg2 / width; j++) {
            func_80044DA0(img, width, height);
            gSPVertex(gDisplayListHead++, &arg1[var_s2], 4, 0);
            gSPDisplayList(gDisplayListHead++, common_rectangle_display);
            img += (width * height) / 2;
            var_s2 += 4;
        }
    }
    gSPTexture(gDisplayListHead++, 0x0001, 0x0001, 0, G_TX_RENDERTILE, G_OFF);
}

void func_80049B20(s32 arg0, s32 arg1, u16 arg2, f32 arg3, u8* texture, Vtx* arg5, s32 arg6, s32 arg7, s32 arg8,
                   s32 arg9) {
    func_80042330(arg0, arg1, arg2, arg3);
    gSPDisplayList(gDisplayListHead++, D_0D007A40);
    func_80048F8C(texture, arg5, arg6, arg7, arg8, arg9);
}

UNUSED void func_80049B9C(s32 arg0, s32 arg1, u16 arg2, f32 arg3, u8* texture, Vtx* arg5, s32 arg6, s32 arg7, s32 arg8,
                          s32 arg9) {
    func_80042330(arg0, arg1, arg2, arg3);
    gSPDisplayList(gDisplayListHead++, D_0D007A40);
    func_80049130(texture, arg5, arg6, arg7, arg8, arg9);
}

void func_80049C18(s32 arg0, s32 arg1, u16 arg2, f32 arg3, u8* texture, Vtx* arg5, s32 arg6, s32 arg7, s32 arg8,
                   s32 arg9) {
    func_80042330(arg0, arg1, arg2, arg3);
    gSPDisplayList(gDisplayListHead++, D_0D007A40);
    func_800492D4(texture, arg5, arg6, arg7, arg8, arg9);
}

void func_80049C94(s32 arg0, s32 arg1, u16 arg2, f32 arg3, u8* texture, Vtx* arg5, s32 arg6, s32 arg7, s32 arg8,
                   s32 arg9) {
    func_80042330(arg0, arg1, arg2, arg3);
    gSPDisplayList(gDisplayListHead++, D_0D007A60);
    func_800492D4(texture, arg5, arg6, arg7, arg8, arg9);
}

void func_80049D10(s32 arg0, s32 arg1, u16 arg2, f32 arg3, u8* texture, Vtx* arg5, s32 arg6, s32 arg7, s32 arg8,
                   s32 arg9) {
    func_80042330(arg0, arg1, arg2, arg3);
    gSPDisplayList(gDisplayListHead++, D_0D007A80);
    func_800492D4(texture, arg5, arg6, arg7, arg8, arg9);
}

void func_80049D8C(s32 arg0, s32 arg1, u16 arg2, f32 arg3, u8* texture, Vtx* arg5, s32 arg6, s32 arg7, s32 arg8,
                   s32 arg9) {
    func_80042330(arg0, arg1, arg2, arg3);
    gSPDisplayList(gDisplayListHead++, D_0D007AA0);
    func_800492D4(texture, arg5, arg6, arg7, arg8, arg9);
}

UNUSED void func_80049E08(s32 arg0, s32 arg1, u16 arg2, f32 arg3, s32 red, s32 green, s32 blue, s32 alpha, u8* texture,
                          Vtx* arg9, s32 argA, s32 argB, s32 argC, s32 argD) {
    func_80042330(arg0, arg1, arg2, arg3);
    gSPDisplayList(gDisplayListHead++, D_0D007A40);
    func_8004B35C(red, green, blue, alpha);
    func_800492D4(texture, arg9, argA, argB, argC, argD);
}

UNUSED void func_80049E98(s32 arg0, s32 arg1, u16 arg2, f32 arg3, s32 red, s32 green, s32 blue, s32 alpha, u8* texture,
                          Vtx* arg9, s32 argA, s32 argB, s32 argC, s32 argD) {
    func_80042330(arg0, arg1, arg2, arg3);
    gSPDisplayList(gDisplayListHead++, D_0D007A60);
    func_8004B35C(red, green, blue, alpha);
    func_800492D4(texture, arg9, argA, argB, argC, argD);
}

UNUSED void func_80049F28(s32 arg0, s32 arg1, u16 arg2, f32 arg3, s32 red, s32 green, s32 blue, s32 alpha, u8* texture,
                          Vtx* arg9, s32 argA, s32 argB, s32 argC, s32 argD) {
    func_80042330(arg0, arg1, arg2, arg3);
    gSPDisplayList(gDisplayListHead++, D_0D007B00);
    func_8004B35C(red, green, blue, alpha);
    func_800492D4(texture, arg9, argA, argB, argC, argD);
}

UNUSED void func_80049FB8(s32 arg0, s32 arg1, u16 arg2, f32 arg3, u8* texture, Vtx* arg5, s32 arg6, s32 arg7, s32 arg8,
                          s32 arg9) {
    func_80042330(arg0, arg1, arg2, arg3);
    gSPDisplayList(gDisplayListHead++, D_0D007A40);
    func_80049478(texture, arg5, arg6, arg7, arg8, arg9);
}

UNUSED void func_8004A034(s32 arg0, s32 arg1, u16 arg2, f32 arg3, u8* texture, Vtx* arg5, s32 arg6, s32 arg7, s32 arg8,
                          s32 arg9) {
    func_80042330(arg0, arg1, arg2, arg3);
    gSPDisplayList(gDisplayListHead++, D_0D007A60);
    func_80049478(texture, arg5, arg6, arg7, arg8, arg9);
}

void func_8004A0B0(s32 arg0, s32 arg1, u16 arg2, f32 arg3, u8* texture, Vtx* arg5, s32 arg6, s32 arg7, s32 arg8,
                   s32 arg9) {
    func_80042330(arg0, arg1, arg2, arg3);
    gSPDisplayList(gDisplayListHead++, D_0D007A40);
    func_8004961C(texture, arg5, arg6, arg7, arg8, arg9);
}

UNUSED void func_8004A12C(s32 arg0, s32 arg1, u16 arg2, f32 arg3, s32 red, s32 green, s32 blue, s32 alpha, u8* texture,
                          Vtx* arg9, s32 argA, s32 argB, s32 argC, s32 argD) {
    func_80042330(arg0, arg1, arg2, arg3);
    gSPDisplayList(gDisplayListHead++, D_0D007A60);
    func_8004B35C(red, green, blue, alpha);
    func_8004961C(texture, arg9, argA, argB, argC, argD);
}
UNUSED void func_8004A1BC(s32 arg0, s32 arg1, u16 arg2, f32 arg3, u8* texture, Vtx* arg5, s32 arg6, s32 arg7, s32 arg8,
                          s32 arg9) {
    func_80042330(arg0, arg1, arg2, arg3);
    gSPDisplayList(gDisplayListHead++, D_0D007A40);
    gDPSetCombineLERP(gDisplayListHead++, 1, 0, SHADE, 0, 0, 0, 0, TEXEL0, 1, 0, SHADE, 0, 0, 0, 0, TEXEL0);
    func_80049970(texture, arg5, arg6, arg7, arg8, arg9);
}

void func_8004A258(s32 arg0, s32 arg1, u16 arg2, f32 arg3, u8* texture, Vtx* arg5, s32 arg6, s32 arg7, s32 arg8,
                   s32 arg9) {
    func_80042330(arg0, arg1, arg2, arg3);
    gSPDisplayList(gDisplayListHead++, D_0D007A60);
    gDPSetCombineLERP(gDisplayListHead++, 1, 0, SHADE, 0, 0, 0, 0, TEXEL0, 1, 0, SHADE, 0, 0, 0, 0, TEXEL0);
    func_80049970(texture, arg5, arg6, arg7, arg8, arg9);
}

void func_8004A2F4(s32 arg0, s32 arg1, u16 arg2, f32 arg3, s32 red, s32 green, s32 blue, s32 alpha, u8* texture,
                   Vtx* vtx, s32 argA, s32 argB, s32 width, s32 height) {
    func_80042330(arg0, arg1, arg2, arg3);
    gSPDisplayList(gDisplayListHead++, D_0D007A40);
    func_8004B414(red, green, blue, alpha);
    func_80044DA0(texture, argA, argB);
    gSPVertex(gDisplayListHead++, vtx, 4, 0);
    gSPDisplayList(gDisplayListHead++, common_rectangle_display);
    gSPTexture(gDisplayListHead++, 0x0001, 0x0001, 0, G_TX_RENDERTILE, G_OFF);
}

void func_8004A384(s32 arg0, s32 arg1, u16 arg2, f32 arg3, s32 red, s32 green, s32 blue, s32 alpha, u8* texture,
                   Vtx* arg9, s32 argA, s32 argB, s32 argC, s32 argD) {
    func_80042330_wide(arg0, arg1, arg2, arg3);
    gSPDisplayList(gDisplayListHead++, D_0D007A60);
    func_8004B414(red, green, blue, alpha);
    func_80049970(texture, arg9, argA, argB, argC, argD);
}

void func_8004A414(Vec3f arg0, Vec3su arg1, f32 arg2, u8* texture, Vtx* arg4, s32 arg5, s32 arg6, s32 arg7, s32 arg8) {
    rsp_set_matrix_transformation(arg0, arg1, arg2);
    gSPDisplayList(gDisplayListHead++, D_0D007A40);
    func_800492D4(texture, arg4, arg5, arg6, arg7, arg8);
}

UNUSED void func_8004A488(Vec3f arg0, Vec3su arg1, f32 arg2, u8* texture, Vtx* arg4, s32 arg5, s32 arg6, s32 arg7,
                          s32 arg8) {
    rsp_set_matrix_transformation(arg0, arg1, arg2);
    gSPDisplayList(gDisplayListHead++, D_0D007A60);
    func_800492D4(texture, arg4, arg5, arg6, arg7, arg8);
}

UNUSED void func_8004A4FC(Vec3f arg0, Vec3su arg1, f32 arg2, u8* texture, Vtx* arg4, s32 arg5, s32 arg6, s32 arg7,
                          s32 arg8) {
    rsp_set_matrix_transformation(arg0, arg1, arg2);
    gSPDisplayList(gDisplayListHead++, D_0D007AC0);
    func_800492D4(texture, arg4, arg5, arg6, arg7, arg8);
}

UNUSED void func_8004A570(Vec3f arg0, Vec3su arg1, f32 arg2, u8* texture, Vtx* arg4, s32 arg5, s32 arg6, s32 arg7,
                          s32 arg8) {
    rsp_set_matrix_transformation(arg0, arg1, arg2);
    gSPDisplayList(gDisplayListHead++, D_0D007AE0);
    func_800492D4(texture, arg4, arg5, arg6, arg7, arg8);
}

UNUSED void func_8004A5E4(Vec3f arg0, Vec3su arg1, f32 arg2, u8* texture, Vtx* arg4) {
    func_8004A414(arg0, arg1, arg2, texture, arg4, 16, 16, 16, 16);
}

void func_8004A630(Collision* arg0, Vec3f arg1, f32 arg2) {
    if (func_80041924(arg0, arg1) != 0) {
        D_80183E50[0] = arg1[0];
        D_80183E50[1] = calculate_surface_height(arg1[0], 0.0f, arg1[2], arg0->meshIndexZX) + 0.8;
        D_80183E50[2] = arg1[2];
        rsp_set_matrix_transl_rot_scale(D_80183E50, arg0->orientationVector, arg2);
        gSPDisplayList(gDisplayListHead++, D_0D007B98);
    }
}

void func_8004A6EC(s32 objectIndex, f32 scale) {
    Object* object;

    if ((is_obj_flag_status_active(objectIndex, 0x00000020) != 0) &&
        (is_obj_flag_status_active(objectIndex, 0x00800000) != 0)) {
        object = &gObjectList[objectIndex];
        D_80183E50[0] = object->pos[0];
        D_80183E50[1] = object->surfaceHeight + 0.8;
        D_80183E50[2] = object->pos[2];
        rsp_set_matrix_transformation(D_80183E50, object->unk_0B8, scale);
        gSPDisplayList(gDisplayListHead++, D_0D007B20);
    }
}

void func_8004A7AC(s32 objectIndex, f32 arg1) {
    Object* object;

    if (is_obj_flag_status_active(objectIndex, 0x00000020) != 0) {
        object = &gObjectList[objectIndex];
        D_80183E50[0] = object->pos[0];
        D_80183E50[1] = object->surfaceHeight + 0.8;
        D_80183E50[2] = object->pos[2];
        D_80183E98[0] = 0x4000;
        D_80183E98[1] = 0;
        D_80183E98[2] = 0;
        rsp_set_matrix_transformation(D_80183E50, D_80183E98, arg1);
        gSPDisplayList(gDisplayListHead++, D_0D007B20);
    }
}

void func_8004A9B8(f32 arg0) {
    rsp_set_matrix_transl_rot_scale(D_80183E50, D_80183E70, arg0);
    gSPDisplayList(gDisplayListHead++, D_0D007C10);
}

UNUSED void func_8004AA10(Vec3f arg0, Vec3su arg1, f32 arg2, u8* texture, Vtx* arg4, s32 arg5, s32 arg6, s32 arg7,
                          s32 arg8) {
    rsp_set_matrix_transformation(arg0, arg1, arg2);
    gSPDisplayList(gDisplayListHead++, D_0D007AE0);
    func_8004B6C4(D_80165860, D_8016586C, D_80165878);
    func_800497CC(texture, arg4, arg5, arg6, arg7, arg8);
}

UNUSED void func_8004AAA0(s32 arg0, s32 arg1, u16 arg2, f32 arg3, u8* texture, Vtx* arg5) {
    func_80049B20(arg0, arg1, arg2, arg3, texture, arg5, 16, 16, 16, 16);
}

UNUSED void func_8004AB00(s32 arg0, s32 arg1, u16 arg2, f32 arg3, u8* texture, Vtx* arg5) {
    func_80049C18(arg0, arg1, arg2, arg3, texture, arg5, 16, 16, 16, 16);
}

UNUSED void func_8004AB60(s32 arg0, s32 arg1, u16 arg2, f32 arg3, u8* arg4, Vtx* arg5) {
    func_8004A0B0(arg0, arg1, arg2, arg3, arg4, arg5, 16, 16, 16, 16);
}

UNUSED void func_8004ABC0(s32 arg0, s32 arg1, u16 arg2, f32 arg3, u8* texture, Vtx* arg5) {
    func_80049B20(arg0, arg1, arg2, arg3, texture, arg5, 32, 32, 32, 32);
}

UNUSED void func_8004AC20(s32 arg0, s32 arg1, u16 arg2, f32 arg3, u8* arg4, Vtx* arg5) {
    func_80049C18(arg0, arg1, arg2, arg3, arg4, arg5, 32, 32, 32, 32);
}

UNUSED void func_8004AC80(s32 arg0, s32 arg1, u16 arg2, f32 arg3, u8* arg4, Vtx* arg5) {
    func_8004A0B0(arg0, arg1, arg2, arg3, arg4, arg5, 32, 32, 32, 32);
}

UNUSED void func_8004ACE0(Vec3f arg0, Vec3su arg1, f32 arg2, u8* texture, Vtx* arg4) {
    func_8004A414(arg0, arg1, arg2, texture, arg4, 32, 32, 32, 32);
}

UNUSED void func_8004AD2C(s32 arg0, s32 arg1, u16 arg2, f32 arg3, u8* texture, Vtx* arg5) {
    func_80049B20(arg0, arg1, arg2, arg3, texture, arg5, 64, 32, 64, 32);
}

UNUSED void func_8004AD8C(s32 arg0, s32 arg1, u16 arg2, f32 arg3, u8* arg4, Vtx* arg5) {
    func_80049C18(arg0, arg1, arg2, arg3, arg4, arg5, 64, 32, 64, 32);
}

UNUSED void func_8004ADEC(s32 arg0, s32 arg1, u16 arg2, f32 arg3, u8* arg4, Vtx* arg5) {
    func_80049C94(arg0, arg1, arg2, arg3, arg4, arg5, 64, 32, 64, 32);
}

UNUSED void func_8004AE4C(s32 arg0, s32 arg1, u16 arg2, f32 arg3, u8* texture, Vtx* arg5) {
    func_80049D10(arg0, arg1, arg2, arg3, texture, arg5, 64, 32, 64, 32);
}

UNUSED void func_8004AEAC(s32 arg0, s32 arg1, u16 arg2, f32 arg3, u8* texture, Vtx* arg5) {
    func_80049D8C(arg0, arg1, arg2, arg3, texture, arg5, 64, 32, 64, 32);
}

UNUSED void func_8004AF0C(s32 arg0, s32 arg1, u16 arg2, f32 arg3, u8* texture, Vtx* arg5) {
    func_80049C18(arg0, arg1, arg2, arg3, texture, arg5, 64, 64, 64, 64);
}

UNUSED void func_8004AF6C(s32 arg0, s32 arg1, u16 arg2, f32 arg3, u8* texture, Vtx* arg5) {
    func_80049B20(arg0, arg1, arg2, arg3, texture, arg5, 64, 64, 64, 32);
}

UNUSED void func_8004AFCC(s32 arg0, s32 arg1, u16 arg2, f32 arg3, u8* texture, Vtx* arg5) {
    func_80049C18(arg0, arg1, arg2, arg3, texture, arg5, 64, 96, 64, 48);
}

UNUSED void func_8004B02C(void) {
    gDPSetRenderMode(gDisplayListHead++,
                     AA_EN | Z_CMP | Z_UPD | IM_RD | CVG_DST_WRAP | ZMODE_XLU | CVG_X_ALPHA | FORCE_BL |
                         GBL_c1(G_BL_CLR_IN, G_BL_A_IN, G_BL_CLR_MEM, G_BL_1MA),
                     AA_EN | Z_CMP | Z_UPD | IM_RD | CVG_DST_WRAP | ZMODE_XLU | CVG_X_ALPHA | FORCE_BL |
                         GBL_c2(G_BL_CLR_IN, G_BL_A_IN, G_BL_CLR_MEM, G_BL_1MA));
}

void func_8004B05C(u8* tlut) {
    gDPLoadTLUT_pal256(gDisplayListHead++, tlut);
    gDPLoadSync(gDisplayListHead++);
    gDPSetTexturePersp(gDisplayListHead++, G_TP_NONE);
}

void func_8004B138(s32 red, s32 green, s32 blue, s32 alpha) {
    gDPSetPrimColor(gDisplayListHead++, 0, 0, red, green, blue, alpha);
}

UNUSED void func_8004B180(s32 red, s32 green, s32 blue, s32 alpha) {
    gDPSetEnvColor(gDisplayListHead++, red, green, blue, alpha);
}

void set_color_render(s32 primRed, s32 primGreen, s32 primBlue, s32 envRed, s32 envGreen, s32 envBlue, s32 primAlpha) {
    gDPSetPrimColor(gDisplayListHead++, 0, 0, primRed, primGreen, primBlue, primAlpha);
    gDPSetEnvColor(gDisplayListHead++, envRed, envGreen, envBlue, 0xFF);
}

UNUSED void func_8004B254(s32 red, s32 green, s32 blue) {
    gDPSetCombineMode(gDisplayListHead++, G_CC_MODULATEIA_PRIM, G_CC_MODULATEIA_PRIM);
    gDPSetPrimColor(gDisplayListHead++, 0, 0, red, green, blue, 0xFF);
}

void set_transparency(s32 alpha) {
    gDPSetCombineMode(gDisplayListHead++, G_CC_MODULATEIA_PRIM, G_CC_MODULATEIA_PRIM);
    gDPSetPrimColor(gDisplayListHead++, 0, 0, 0xFF, 0xFF, 0xFF, alpha);
}

void func_8004B310(s32 alpha) {
    gDPSetCombineLERP(gDisplayListHead++, 0, 0, 0, TEXEL0, TEXEL0, 0, PRIMITIVE, 0, 0, 0, 0, TEXEL0, TEXEL0, 0,
                      PRIMITIVE, 0);
    gDPSetPrimColor(gDisplayListHead++, 0, 0, 0x00, 0x00, 0x00, alpha);
}

void func_8004B35C(s32 red, s32 green, s32 blue, s32 alpha) {
    gDPSetCombineMode(gDisplayListHead++, G_CC_MODULATEIA_PRIM, G_CC_MODULATEIA_PRIM);
    gDPSetPrimColor(gDisplayListHead++, 0, 0, red, green, blue, alpha);
}

void func_8004B3C8(s32 alpha) {
    gDPSetCombineLERP(gDisplayListHead++, 0, 0, 0, 1, TEXEL0, 0, PRIMITIVE, 0, 0, 0, 0, 1, TEXEL0, 0, PRIMITIVE, 0);
    gDPSetPrimColor(gDisplayListHead++, 0, 0, 0x00, 0x00, 0x00, alpha);
}

void func_8004B414(s32 red, s32 green, s32 blue, s32 alpha) {
    gDPSetCombineLERP(gDisplayListHead++, 0, 0, 0, PRIMITIVE, TEXEL0, 0, PRIMITIVE, 0, 0, 0, 0, PRIMITIVE, TEXEL0, 0,
                      PRIMITIVE, 0);
    gDPSetPrimColor(gDisplayListHead++, 0, 0, red, green, blue, alpha);
}

void func_8004B480(s32 red, s32 green, s32 blue) {
    gDPSetCombineLERP(gDisplayListHead++, 0, 0, 0, PRIMITIVE, 0, 0, 0, TEXEL0, 0, 0, 0, PRIMITIVE, 0, 0, 0, TEXEL0);
    gDPSetPrimColor(gDisplayListHead++, 0, 0, red, green, blue, 0xFF);
}

UNUSED void func_8004B4E8(s32 red, s32 green, s32 blue, s32 alpha) {
    gDPSetCombineLERP(gDisplayListHead++, 1, 0, SHADE, PRIMITIVE, 0, 0, 0, TEXEL0, 1, 0, SHADE, PRIMITIVE, 0, 0, 0,
                      TEXEL0);
    gDPSetPrimColor(gDisplayListHead++, 0, 0, red, green, blue, alpha);
}

UNUSED void func_8004B554(s32 alpha) {
    gDPSetCombineLERP(gDisplayListHead++, 0, 0, 0, SHADE, TEXEL0, 0, PRIMITIVE, 0, 0, 0, 0, SHADE, TEXEL0, 0, PRIMITIVE,
                      0);
    gDPSetPrimColor(gDisplayListHead++, 0, 0, 0xFF, 0xFF, 0xFF, alpha);
}

UNUSED void func_8004B5A8(s32 red, s32 green, s32 blue, s32 alpha) {
    gDPSetPrimColor(gDisplayListHead++, 0, 0, red, green, blue, alpha);
    gDPSetCombineLERP(gDisplayListHead++, 1, PRIMITIVE_ALPHA, TEXEL0, PRIMITIVE, 0, 0, 0, TEXEL0, 1, PRIMITIVE_ALPHA,
                      TEXEL0, PRIMITIVE, 0, 0, 0, TEXEL0);
}

void func_8004B614(s32 primRed, s32 primGreen, s32 primBlue, s32 envRed, s32 envGreen, s32 envBlue, s32 primAlpha) {
    gDPSetPrimColor(gDisplayListHead++, 0, 0, primRed, primGreen, primBlue, primAlpha);
    gDPSetEnvColor(gDisplayListHead++, envRed, envGreen, envBlue, 0xFF);
    gDPSetCombineLERP(gDisplayListHead++, 1, ENVIRONMENT, TEXEL0, PRIMITIVE, PRIMITIVE, 0, TEXEL0, 0, 1, ENVIRONMENT,
                      TEXEL0, PRIMITIVE, PRIMITIVE, 0, TEXEL0, 0);
}

void func_8004B6C4(s32 red, s32 green, s32 blue) {
    gDPSetCombineLERP(gDisplayListHead++, 0, 0, 0, PRIMITIVE, 0, 0, 0, TEXEL0, 0, 0, 0, PRIMITIVE, 0, 0, 0, TEXEL0);
    gDPSetPrimColor(gDisplayListHead++, 0, 0, red, green, blue, 0xFF);
}

void func_8004B72C(s32 primRed, s32 primGreen, s32 primBlue, s32 envRed, s32 envGreen, s32 envBlue, s32 primAlpha) {
    gDPSetPrimColor(gDisplayListHead++, 0, 0, primRed, primGreen, primBlue, primAlpha);
    gDPSetEnvColor(gDisplayListHead++, envRed, envGreen, envBlue, 0xFF);
    gDPSetCombineLERP(gDisplayListHead++, PRIMITIVE, ENVIRONMENT, TEXEL0, ENVIRONMENT, TEXEL0, 0, PRIMITIVE, 0,
                      PRIMITIVE, ENVIRONMENT, TEXEL0, ENVIRONMENT, TEXEL0, 0, PRIMITIVE, 0);
}

/**
 * Renders
 *
 * Menus: Mario Kart 64 Logo, Debug text
 *
 * 1P: Coloured square in third hud mode that players go around.
 *
 *
 */
void render_texture_rectangle(s32 x, s32 y, s32 width, s32 height, s32 s, s32 w, s32 mode) {

    s32 xh = (((x + width) - 1) << 2);
    s32 yh = (((y + height) - 1) << 2);
    s32 xl = ((x * 4));
    s32 yl = y * 4;

    s32 xh2 = (((x + width)) << 2);
    s32 yh2 = ((y + height) << 2);

    // If no cycle mode is set, render texture rectangle in copy mode
    if (mode == 0) {
        //! @todo Update to F3DEX. Uses OLD definition for gspTextureRectangle.
        gSPTextureRectangle(gDisplayListHead++, xl, yl, xh, yh, G_TX_RENDERTILE, s << 5, (w << 5), 4 << 10, 1 << 10);
        return;
    }
    // Render texture rectangle in default cycle mode (1 cycle or 2 cycle)
    gSPTextureRectangle(gDisplayListHead++, xl, yl, xh2, yh2, G_TX_RENDERTILE, s << 5, (w << 5), 1 << 10, 1 << 10);
}

/**
 * Renders
 *
 * For all game modes
 *
 * Minimap, CurrLap, Lap time
 *
 *
 */
void render_texture_rectangle_wide(s32 x, s32 y, s32 width, s32 height, s32 arg4, s32 arg5, s32 arg6) {

    s32 xh = (((x + width) - 1));
    s32 yh = (((y + height) - 1) << 2);
    s32 xl = ((x));
    s32 yl = y << 2;

    s32 xh2 = (((x + width)));
    s32 yh2 = ((y + height) << 2);

    // if center of image is to the left side of the screen then align left,
    // otherwise align right

    s32 coordX = 0;
    s32 coordX2 = 0;

    if (arg6 == 0) {
        switch (gScreenModeSelection) {
            case SCREEN_MODE_1P:
            case SCREEN_MODE_3P_4P_SPLITSCREEN:
            case SCREEN_MODE_2P_SPLITSCREEN_HORIZONTAL:
                if ((xl - (width / 2)) < (SCREEN_WIDTH / 2)) {
                    coordX = (s32) OTRGetDimensionFromLeftEdge(xl) << 2;
                    coordX2 = (s32) (xh) << 2;
                } else {
                    coordX = (s32) OTRGetDimensionFromRightEdge(xl) << 2;
                    coordX2 = (s32) OTRGetDimensionFromRightEdge(xh) << 2;
                }
                //! @todo Update to F3DEX. Uses OLD definition for gspTextureRectangle.
                gSPWideTextureRectangle(gDisplayListHead++, coordX, yl, coordX2, yh, G_TX_RENDERTILE, arg4 << 5,
                                        (arg5 << 5), 4 << 10, 1 << 10);
                break;
            case SCREEN_MODE_2P_SPLITSCREEN_VERTICAL:
                gSPTextureRectangle(gDisplayListHead++, xl << 2, yl, xh << 2, yh2, G_TX_RENDERTILE, arg4 << 5,
                                    (arg5 << 5), 1 << 10, 1 << 10);
                break;
        }
        // OTRGetDimensionFromLeftEdge
        // gSPTextureRectangle(gDisplayListHead++, xl, yl, xh, yh, G_TX_RENDERTILE, arg4 << 5, (arg5 << 5), 4 << 10,
        //                     1 << 10);
    } else { // minimap
        switch (gScreenModeSelection) {
            case SCREEN_MODE_3P_4P_SPLITSCREEN:
                if (gPlayerCount == 3) {
                    // Center item in area of screen
                    s32 center = (s32) ((OTRGetDimensionFromRightEdge(SCREEN_WIDTH) - SCREEN_WIDTH) / 2) +
                                 ((SCREEN_WIDTH / 4) + (SCREEN_WIDTH / 2));
                    s32 coordX = (s32) (center - (width / 2)) << 2;
                    s32 coordX2 = (s32) (center + (width / 2)) << 2;
                    gSPWideTextureRectangle(gDisplayListHead++, coordX, yl, coordX2, yh2, G_TX_RENDERTILE, arg4 << 5,
                                            (arg5 << 5), 1 << 10, 1 << 10);
                } else { // 4 players
                    s32 renderWidth = SCREEN_WIDTH;
                    s32 center = (renderWidth / 2);
                    coordX = (s32) (center - (width / 2)) << 2;
                    coordX2 = (s32) (center + (width / 2)) << 2;
                    gSPWideTextureRectangle(gDisplayListHead++, coordX, yl, coordX2, yh2, G_TX_RENDERTILE, arg4 << 5,
                                            (arg5 << 5), 1 << 10, 1 << 10);
                }
                break;
            default:
                coordX = (s32) OTRGetDimensionFromRightEdge(xl) << 2;
                coordX2 = (s32) OTRGetDimensionFromRightEdge(xh2) << 2;
                gSPWideTextureRectangle(gDisplayListHead++, coordX, yl, coordX2, yh2, G_TX_RENDERTILE, arg4 << 5,
                                        (arg5 << 5), 1 << 10, 1 << 10);
                break;
        }
    }
    // gSPTextureRectangle(gDisplayListHead++, xl, yl, xh2, yh2, G_TX_RENDERTILE, arg4 << 5, (arg5 << 5), 1 << 10,
    //                     1 << 10);
}

void render_texture_rectangle_wide_left(s32 x, s32 y, s32 width, s32 height, s32 arg4, s32 arg5, s32 arg6) {
    s32 xh = (((x + width) - 1));
    s32 yh = (((y + height) - 1) << 2);
    s32 xl = ((x));
    s32 yl = y << 2;

    s32 xh2 = (((x + width)));
    s32 yh2 = ((y + height) << 2);

    // if center of image is to the left side of the screen then align left,
    // otherwise align right

    s32 coordX = 0;
    s32 coordX2 = 0;

    switch (gScreenModeSelection) {
        case SCREEN_MODE_3P_4P_SPLITSCREEN:
            if (gPlayerCount == 3) {
                // Center item in area of screen
                s32 center = (s32) ((OTRGetDimensionFromLeftEdge(SCREEN_WIDTH) - SCREEN_WIDTH) / 2) +
                             ((SCREEN_WIDTH / 4) + (SCREEN_WIDTH / 2));
                s32 coordX = (s32) (center - (width / 2)) << 2;
                s32 coordX2 = (s32) (center + (width / 2)) << 2;
                gSPWideTextureRectangle(gDisplayListHead++, coordX, yl, coordX2, yh2, G_TX_RENDERTILE, arg4 << 5,
                                        (arg5 << 5), 1 << 10, 1 << 10);
            } else { // 4 players
                s32 renderWidth = SCREEN_WIDTH;
                s32 center = (renderWidth / 2);
                coordX = (s32) (center - (width / 2)) << 2;
                coordX2 = (s32) (center + (width / 2)) << 2;
                gSPWideTextureRectangle(gDisplayListHead++, coordX, yl, coordX2, yh2, G_TX_RENDERTILE, arg4 << 5,
                                        (arg5 << 5), 1 << 10, 1 << 10);
            }
            break;
        default:
            coordX = (s32) OTRGetDimensionFromLeftEdge(xl) << 2;
            coordX2 = (s32) OTRGetDimensionFromLeftEdge(xh2) << 2;
            gSPWideTextureRectangle(gDisplayListHead++, coordX, yl, coordX2, yh2, G_TX_RENDERTILE, arg4 << 5,
                                    (arg5 << 5), 1 << 10, 1 << 10);
            break;
    }

    // gSPTextureRectangle(gDisplayListHead++, xl, yl, xh2, yh2, G_TX_RENDERTILE, arg4 << 5, (arg5 << 5), 1 << 10,
    //                     1 << 10);
}

void render_texture_rectangle_wrap(s32 x, s32 y, s32 width, s32 height, s32 mode) {
    // (0, 0) means texture coordinates will be rendered from the top left corner
    render_texture_rectangle(x, y, width, height, 0, 0, mode);
}

// Positions item window, the Lap 1/2/3, TIME texture, and minimap on the screen.
void func_8004B97C(s32 arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4) {
    UNUSED s32 pad[2];
    s32 sp2C;
    s32 var_a1;
    s32 var_v0;
    s32 var_v1;

    if ((-arg2 < arg0) && (-arg3 < arg1)) {
        var_v0 = 0;
        var_v1 = 0;
        sp2C = arg0;
        var_a1 = arg1;
        if (arg0 < 0) {
            var_v1 = -arg0;
            sp2C = 0;
        }
        if (arg1 < 0) {
            var_v0 = -arg1;
            var_a1 = 0;
        }
        render_texture_rectangle(sp2C, var_a1, arg2 - var_v1, arg3 - var_v0, var_v1, var_v0, arg4);
    }
}

// Positions item window, the Lap 1/2/3, TIME texture, and minimap on the screen.
void func_8004B97C_wide(s32 arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4) {
    UNUSED s32 pad[2];
    s32 sp2C;
    s32 var_a1;
    s32 var_v0;
    s32 var_v1;

    if ((-arg2 < arg0) && (-arg3 < arg1)) {
        var_v0 = 0;
        var_v1 = 0;
        sp2C = arg0;
        var_a1 = arg1;
        if (arg0 < 0) {
            var_v1 = -arg0;
            sp2C = 0;
        }
        if (arg1 < 0) {
            var_v0 = -arg1;
            var_a1 = 0;
        }
        render_texture_rectangle_wide(sp2C, var_a1, arg2 - var_v1, arg3 - var_v0, var_v1, var_v0, arg4);
    }
}

void func_8004B97C_wide_left(s32 arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4) {
    UNUSED s32 pad[2];
    s32 sp2C;
    s32 var_a1;
    s32 var_v0;
    s32 var_v1;

    if ((-arg2 < arg0) && (-arg3 < arg1)) {
        var_v0 = 0;
        var_v1 = 0;
        sp2C = arg0;
        var_a1 = arg1;
        if (arg0 < 0) {
            var_v1 = -arg0;
            sp2C = 0;
        }
        if (arg1 < 0) {
            var_v0 = -arg1;
            var_a1 = 0;
        }
        render_texture_rectangle_wide_left(sp2C, var_a1, arg2 - var_v1, arg3 - var_v0, var_v1, var_v0, arg4);
    }
}

// extra mode minimap
void func_8004BA08(s32 arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4) {
    UNUSED s32 pad[2];
    s32 sp2C;
    s32 phi_a1;
    s32 phi_v1;
    s32 phi_v0;

    if ((-arg2 < arg0) && (-arg3 < arg1)) {
        phi_v0 = 0;
        phi_v1 = 0;
        sp2C = arg0;
        phi_a1 = arg1;
        if (arg0 < 0) {
            phi_v1 = -arg0;
            sp2C = 0;
        }
        if (arg1 < 0) {
            phi_v0 = -arg1;
            phi_a1 = 0;
        }
        render_texture_rectangle_wide(sp2C, phi_a1, arg2 - phi_v1, arg3 - phi_v0, phi_v1 + arg2, phi_v0, arg4);
    }
}

// Places the timer on the screen
void func_8004BA98(s32 arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4, s32 arg5, s32 arg6) {
    UNUSED s32 pad[2];
    s32 sp34;
    s32 sp30;
    s32 sp2C;
    s32 phi_a3;
    s32 phi_v0;
    s32 phi_v1;

    if ((-arg2 < arg0) && (-arg3 < arg1)) {
        sp34 = arg0;
        sp30 = arg1;
        phi_v0 = arg4;
        sp2C = arg2;
        phi_a3 = arg3;
        phi_v1 = arg5;
        if (arg0 < 0) {
            phi_v0 = arg4 - arg0;
            sp34 = 0;
            sp2C = arg2 + arg0;
        }
        if (arg1 < 0) {
            phi_v1 = arg5 - arg1;
            sp30 = 0;
            phi_a3 = arg3 + arg1;
        }
        render_texture_rectangle_wide(sp34, sp30, sp2C, phi_a3, phi_v0, phi_v1, arg6);
    }
}

// Display lap count (but not the texture that says lap)
void func_8004BA98_wide(s32 arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4, s32 arg5, s32 arg6) {
    UNUSED s32 pad[2];
    s32 sp34;
    s32 sp30;
    s32 sp2C;
    s32 phi_a3;
    s32 phi_v0;
    s32 phi_v1;

    if ((-arg2 < arg0) && (-arg3 < arg1)) {
        sp34 = arg0;
        sp30 = arg1;
        phi_v0 = arg4;
        sp2C = arg2;
        phi_a3 = arg3;
        phi_v1 = arg5;
        if (arg0 < 0) {
            phi_v0 = arg4 - arg0;
            sp34 = 0;
            sp2C = arg2 + arg0;
        }
        if (arg1 < 0) {
            phi_v1 = arg5 - arg1;
            sp30 = 0;
            phi_a3 = arg3 + arg1;
        }

        switch (gScreenModeSelection) {
            case SCREEN_MODE_1P:
            case SCREEN_MODE_2P_SPLITSCREEN_HORIZONTAL:
                render_texture_rectangle_wide(sp34, sp30, sp2C, phi_a3, phi_v0, phi_v1, arg6);
                break;
            default:
                render_texture_rectangle(sp34, sp30, sp2C, phi_a3, phi_v0, phi_v1, arg6);
                break;
        }
    }
}

UNUSED void func_8004BB34(void) {
}

void func_8004BB3C(s32 arg0, s32 arg1, s32 arg2, s32 arg3, f32 arg4) {
    s16 t;
    s16 s;
    s16 temp_t9;
    s32 var_t0;
    s32 var_t1;
    s32 xl;
    s32 yl;
    UNUSED s32 thing0;
    UNUSED s32 thing1;

    var_t0 = (arg2 * 4 * arg4) + 0.5;
    var_t1 = (arg3 * 4 * arg4) + 0.5;
    xl = (arg0 * 4) - (var_t0 / 2);
    yl = (arg1 * 4) - (var_t1 / 2);
    if (-var_t0 < xl) {
        t = 0;
        if (-var_t1 < yl) {
            s = 0;
            if (xl < 0) {
                var_t0 += xl;
                s = (-xl * 8) / arg4;
                xl = 0;
            }
            if (yl < 0) {
                var_t1 += yl;
                t = (-yl * 8) / arg4;
                yl = 0;
            }
            temp_t9 = (1024.0f / arg4) + 0.5;
            gSPTextureRectangle(gDisplayListHead++, xl, yl, xl + var_t0, yl + var_t1, 0, s, t, temp_t9, temp_t9);
        }
    }
}

UNUSED void func_8004BD14(s32 x, s32 y, u32 width, u32 height, s32 alpha, u8* texture1, u8* texture2) {
    gSPDisplayList(gDisplayListHead++, D_0D007F38);
    gSPDisplayList(gDisplayListHead++, D_0D008138);
    gDPSetTextureLOD(gDisplayListHead++, G_TL_TILE);
    gDPSetPrimColor(gDisplayListHead++, 0, 0, 0x00, 0x00, 0x00, alpha);
    gDPSetCombineLERP(gDisplayListHead++, TEXEL1, TEXEL0, PRIMITIVE_ALPHA, TEXEL0, TEXEL1, TEXEL0, PRIMITIVE, TEXEL0, 0,
                      0, 0, COMBINED, 0, 0, 0, COMBINED);
    gDPLoadMultiTile(gDisplayListHead++, texture1, 0, G_TX_RENDERTILE, G_IM_FMT_RGBA, G_IM_SIZ_16b, width, height, 0, 0,
                     width - 1, height - 1, 0, G_TX_NOMIRROR | G_TX_CLAMP, G_TX_NOMIRROR | G_TX_CLAMP, G_TX_NOMASK,
                     G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
    gDPLoadMultiTile(gDisplayListHead++, texture2, 256, G_TX_RENDERTILE + 1, G_IM_FMT_RGBA, G_IM_SIZ_16b, width, height,
                     0, 0, width - 1, height - 1, 0, G_TX_NOMIRROR | G_TX_CLAMP, G_TX_NOMIRROR | G_TX_CLAMP,
                     G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
    render_texture_rectangle_wrap(x, y, width, height, 2);
    gSPDisplayList(gDisplayListHead++, D_0D008120);
}

void func_8004C024(s16 arg0, s16 arg1, s16 arg2, u16 red, u16 green, u16 blue, u16 alpha) {
    gDPSetPrimColor(gDisplayListHead++, 0, 0, red, green, blue, alpha);
    gDPSetTextureLUT(gDisplayListHead++, G_TT_NONE);
    gDPSetTexturePersp(gDisplayListHead++, G_TP_NONE);
    gDPSetCombineMode(gDisplayListHead++, G_CC_PRIMITIVE, G_CC_PRIMITIVE);
    gDPSetRenderMode(gDisplayListHead++, G_RM_XLU_SURF, G_RM_XLU_SURF2);
    func_8004B97C(arg0, arg1, arg2, 1, 1);
}

void func_8004C148(s16 arg0, s16 arg1, s16 arg2, u16 red, u16 green, u16 blue, u16 alpha) {
    gDPSetPrimColor(gDisplayListHead++, 0, 0, red, green, blue, alpha);
    gDPSetTextureLUT(gDisplayListHead++, G_TT_NONE);
    gDPSetTexturePersp(gDisplayListHead++, G_TP_NONE);
    gDPSetCombineMode(gDisplayListHead++, G_CC_PRIMITIVE, G_CC_PRIMITIVE);
    gDPSetRenderMode(gDisplayListHead++, G_RM_XLU_SURF, G_RM_XLU_SURF2);
    func_8004B97C(arg0, arg1, 1, arg2, 1);
}

void func_8004C268(u32 arg0, u32 arg1, u8* texture, u32 width, u32 arg4, u32 height, s32 arg6) {
    s32 i;
    u8* img2;

    arg0 -= (width / 2);
    arg1 -= (arg4 / 2);
    img2 = texture;

    for (i = 0; (u32) i < (arg4 / height); i++) {
        load_texture_block_rgba16_mirror(img2, width, height);
        func_8004B97C_wide(arg0, arg1, width, height, arg6);
//! @todo fakematch?
#ifdef AVOID_UB
        img2 += (width * height) * 2;
#else
        img2 += (width * height) * 2 ^ ((arg4 / height) * 0);
#endif
        arg1 += height;
    }
}

void func_8004C268_wide(u32 arg0, u32 arg1, u8* texture, u32 width, u32 arg4, u32 height, s32 arg6) {
    s32 i;
    u8* img2;

    arg0 -= (width / 2);
    arg1 -= (arg4 / 2);
    img2 = texture;

    for (i = 0; (u32) i < (arg4 / height); i++) {
        load_texture_block_rgba16_mirror(img2, width, height);
        func_8004B97C_wide_left(arg0, arg1, width, height, arg6);
//! @todo fakematch?
#ifdef AVOID_UB
        img2 += (width * height) * 2;
#else
        img2 += (width * height) * 2 ^ ((arg4 / height) * 0);
#endif
        arg1 += height;
    }
}

UNUSED void func_8004C354() {
}

UNUSED void func_8004C35C() {
}

void draw_hud_2d_texture(s32 x, s32 y, u32 width, u32 height, u8* texture) {
    gSPDisplayList(gDisplayListHead++, D_0D008108);
    gSPDisplayList(gDisplayListHead++, D_0D007EF8);
    gDPSetAlphaCompare(gDisplayListHead++, G_AC_THRESHOLD);
    load_texture_block_rgba16_mirror(texture, width, height);
    func_8004B97C(x - (width >> 1), y - (height >> 1), width, height, 0);
    gSPDisplayList(gDisplayListHead++, D_0D007EB8);
}

void draw_hud_2d_texture_wide(s32 x, s32 y, u32 width, u32 height, u8* texture) {
    gSPDisplayList(gDisplayListHead++, D_0D008108);
    gSPDisplayList(gDisplayListHead++, D_0D007EF8);
    gDPSetTextureFilter(gDisplayListHead++, G_TF_POINT);
    gDPSetAlphaCompare(gDisplayListHead++, G_AC_THRESHOLD);
    load_texture_block_rgba16_mirror(texture, width, height);
    func_8004B97C_wide(x - (width >> 1), y - (height >> 1), width, height, 0);
    gSPDisplayList(gDisplayListHead++, D_0D007EB8);
    gDPSetTextureFilter(gDisplayListHead++, G_TF_BILERP);
}

static s32 get_arcadekart_scaled_size(f32 value) {
    if (value < 1.0f) {
        return 1;
    }
    return (s32) (value + 0.5f);
}

static s32 get_arcadekart_texture_delta(s32 sourceSize, s32 destSize, s32 copyMode) {
    if (destSize <= 0) {
        return 1 << 10;
    }
    return ((copyMode != 0 ? sourceSize * 4 : sourceSize) << 10) / destSize;
}

static void render_texture_rectangle_scaled(s32 x, s32 y, s32 destWidth, s32 destHeight, s32 sourceWidth,
                                            s32 sourceHeight, s32 s, s32 t) {
    s32 xh = (((x + destWidth) - 1) << 2);
    s32 yh = (((y + destHeight) - 1) << 2);
    s32 xl = x << 2;
    s32 yl = y << 2;

    gSPTextureRectangle(gDisplayListHead++, xl, yl, xh, yh, G_TX_RENDERTILE, s << 5, t << 5,
                        get_arcadekart_texture_delta(sourceWidth, destWidth, true),
                        get_arcadekart_texture_delta(sourceHeight, destHeight, true));
}

static void render_texture_rectangle_wide_scaled(s32 x, s32 y, s32 destWidth, s32 destHeight, s32 sourceWidth,
                                                 s32 sourceHeight, s32 s, s32 t, s32 copyMode, s32 minimapMode) {
    s32 xh = x + destWidth;
    s32 yh = (y + destHeight) << 2;
    s32 coordX;
    s32 coordX2;

    if (minimapMode != 0) {
        coordX = (s32) OTRGetDimensionFromRightEdge((f32) x) << 2;
        coordX2 = (s32) OTRGetDimensionFromRightEdge((f32) xh) << 2;
    } else if ((x - (destWidth / 2)) < (SCREEN_WIDTH / 2)) {
        coordX = (s32) OTRGetDimensionFromLeftEdge((f32) x) << 2;
        coordX2 = xh << 2;
    } else {
        coordX = (s32) OTRGetDimensionFromRightEdge((f32) x) << 2;
        coordX2 = (s32) OTRGetDimensionFromRightEdge((f32) xh) << 2;
    }

    gSPWideTextureRectangle(gDisplayListHead++, coordX, y << 2, coordX2, yh, G_TX_RENDERTILE, s << 5, t << 5,
                            get_arcadekart_texture_delta(sourceWidth, destWidth, copyMode),
                            get_arcadekart_texture_delta(sourceHeight, destHeight, copyMode));
}

static void draw_hud_2d_texture_scaled(s32 x, s32 y, u32 width, u32 height, f32 scale, u8* texture) {
    s32 destWidth = get_arcadekart_scaled_size((f32) width * scale);
    s32 destHeight = get_arcadekart_scaled_size((f32) height * scale);

    gSPDisplayList(gDisplayListHead++, D_0D008108);
    gSPDisplayList(gDisplayListHead++, D_0D007EF8);
    gDPSetAlphaCompare(gDisplayListHead++, G_AC_THRESHOLD);
    load_texture_block_rgba16_mirror(texture, width, height);
    render_texture_rectangle_scaled(x - (destWidth / 2), y - (destHeight / 2), destWidth, destHeight, width, height,
                                    0, 0);
    gSPDisplayList(gDisplayListHead++, D_0D007EB8);
}

static void draw_hud_2d_texture_wide_scaled(s32 x, s32 y, u32 width, u32 height, f32 scale, u8* texture,
                                            s32 minimapMode) {
    s32 destWidth = get_arcadekart_scaled_size((f32) width * scale);
    s32 destHeight = get_arcadekart_scaled_size((f32) height * scale);

    gSPDisplayList(gDisplayListHead++, D_0D008108);
    gSPDisplayList(gDisplayListHead++, D_0D007EF8);
    gDPSetTextureFilter(gDisplayListHead++, G_TF_POINT);
    gDPSetAlphaCompare(gDisplayListHead++, G_AC_THRESHOLD);
    load_texture_block_rgba16_mirror(texture, width, height);
    render_texture_rectangle_wide_scaled(x - (destWidth / 2), y - (destHeight / 2), destWidth, destHeight, width,
                                         height, 0, 0, true, minimapMode);
    gSPDisplayList(gDisplayListHead++, D_0D007EB8);
    gDPSetTextureFilter(gDisplayListHead++, G_TF_BILERP);
}

void func_8004C450(s32 arg0, s32 arg1, u32 arg2, u32 arg3, u8* texture) {

    gSPDisplayList(gDisplayListHead++, D_0D007F38);
    func_8004B614(D_801656C0, D_801656D0, D_801656E0, 0x80, 0x80, 0x80, 0xFF);
    load_texture_block_rgba16_mirror(texture, arg2, arg3);
    func_8004B97C_wide(arg0 - (arg2 >> 1), arg1 - (arg3 >> 1), arg2, arg3, 1);
    gSPDisplayList(gDisplayListHead++, D_0D007EB8);
}

static void func_8004C450_scaled(s32 arg0, s32 arg1, u32 arg2, u32 arg3, f32 scale, u8* texture) {
    s32 destWidth = get_arcadekart_scaled_size((f32) arg2 * scale);
    s32 destHeight = get_arcadekart_scaled_size((f32) arg3 * scale);

    gSPDisplayList(gDisplayListHead++, D_0D007F38);
    func_8004B614(D_801656C0, D_801656D0, D_801656E0, 0x80, 0x80, 0x80, 0xFF);
    load_texture_block_rgba16_mirror(texture, arg2, arg3);
    render_texture_rectangle_wide_scaled(arg0 - (destWidth / 2), arg1 - (destHeight / 2), destWidth, destHeight,
                                         arg2, arg3, 0, 0, false, 1);
    gSPDisplayList(gDisplayListHead++, D_0D007EB8);
}

UNUSED void func_8004C53C(s32 arg0, s32 arg1, u32 arg2, u32 arg3, u8* texture) {

    gSPDisplayList(gDisplayListHead++, D_0D008108);
    gSPDisplayList(gDisplayListHead++, D_0D007EF8);
    gDPSetAlphaCompare(gDisplayListHead++, G_AC_THRESHOLD);
    load_texture_tile_rgba16_nomirror(texture, arg2, arg3);
    func_8004B97C_wide(arg0 - (arg2 >> 1), arg1 - (arg3 >> 1), arg2, arg3, 0);
    gSPDisplayList(gDisplayListHead++, D_0D007EB8);
}

void func_8004C628(s32 arg0, s32 arg1, u32 arg2, u32 arg3, u8* texture) {

    gSPDisplayList(gDisplayListHead++, D_0D007EF8);
    gDPSetAlphaCompare(gDisplayListHead++, G_AC_THRESHOLD);
    load_texture_block_rgba32_nomirror(texture, arg2, arg3);
    func_8004B97C_wide(arg0 - (arg2 >> 1), arg1 - (arg3 >> 1), arg2, arg3, 1);
    gSPDisplayList(gDisplayListHead++, D_0D007EB8);
}

// non-matching
void render_texture_tile_rgba32_block(s16 x, s16 y, u8* texture, u32 width, u32 height) {
    s32 remainingSize;
    s32 currX;
    s32 currY;
    u32 size;
    s32 tileHeight;
    s32 numTiles;
    s32 numTilesDup;

    currX = x - (width / 2);
    currY = y - (height / 2);

    gSPDisplayList(gDisplayListHead++, D_0D007EF8);
    gDPSetRenderMode(gDisplayListHead++, G_RM_XLU_SURF, G_RM_XLU_SURF2);

    gDPLoadTextureTile(gDisplayListHead++, texture, G_IM_FMT_RGBA, G_IM_SIZ_32b, width, height, 0, 0, width - 1,
                       height - 1, 0, G_TX_NOMIRROR | G_TX_CLAMP, G_TX_NOMIRROR | G_TX_CLAMP, G_TX_NOMASK, G_TX_NOMASK,
                       G_TX_NOLOD, G_TX_NOLOD);
    gSPWideTextureRectangle(gDisplayListHead++, currX * 4, currY * 4, ((x + (width / 2)) << 2),
                            ((y + (height / 2)) << 2), G_TX_RENDERTILE, 0, 0, 1 << 10, 1 << 10);

    gSPDisplayList(gDisplayListHead++, D_0D007EB8);
}

void render_game_logo(s16 x, s16 y) {
    int32_t height = 128;
    int32_t width =
        ResourceGetTexWidthByName(logo_mario_kart_64) * height / ResourceGetTexHeightByName(logo_mario_kart_64);
    render_texture_tile_rgba32_block(x, y, logo_mario_kart_64, width, height);
}

UNUSED void func_8004C91C(s32 arg0, s32 arg1, u8* texture, s32 arg3, s32 arg4, s32 arg5) {
    gSPDisplayList(gDisplayListHead++, D_0D008108);
    gSPDisplayList(gDisplayListHead++, D_0D007EF8);
    gDPSetAlphaCompare(gDisplayListHead++, G_AC_THRESHOLD);
    func_8004C268(arg0, arg1, texture, arg3, arg4, arg5, 0);
    gSPDisplayList(gDisplayListHead++, D_0D007EB8);
}

void func_8004C9D8(s32 arg0, s32 arg1, s32 arg2, u8* texture, s32 arg4, s32 arg5, UNUSED s32 arg6, s32 arg7) {
    gSPDisplayList(gDisplayListHead++, D_0D007F38);
    set_transparency(arg2);
    func_8004C268(arg0, arg1, texture, arg4, arg5, arg7, 1);
}

void func_8004C9D8_wide(s32 arg0, s32 arg1, s32 arg2, u8* texture, s32 arg4, s32 arg5, UNUSED s32 arg6, s32 arg7) {
    gSPDisplayList(gDisplayListHead++, D_0D007F38);
    set_transparency(arg2);
    func_8004C268_wide(arg0, arg1, texture, arg4, arg5, arg7, 1);
}

void func_8004CA58(s32 arg0, s32 arg1, f32 arg2, u8* texture, s32 arg4, s32 arg5) {
    gSPDisplayList(gDisplayListHead++, D_0D007F78);
    load_texture_block_rgba16_mirror(texture, arg4, arg5);
    func_8004BB3C(arg0, arg1, arg4, arg5, arg2);
}

void draw_hud_2d_texture_8x8(s32 x, s32 y, u8* texture) {
    switch (gScreenModeSelection) {
        case SCREEN_MODE_1P:
        case SCREEN_MODE_2P_SPLITSCREEN_HORIZONTAL:
        case SCREEN_MODE_2P_SPLITSCREEN_VERTICAL:
            draw_hud_2d_texture_wide(x, y, 8, 8, texture);
            break;
        case SCREEN_MODE_3P_4P_SPLITSCREEN:
            draw_hud_2d_texture(x, y, 8, 8, texture);
            break;
    }
}

UNUSED void draw_hud_2d_texture_8x16(s32 x, s32 y, u8* texture) {
    draw_hud_2d_texture_wide(x, y, 8, 16, texture);
}

UNUSED void draw_hud_2d_texture_16x16(s32 x, s32 y, u8* texture) {
    draw_hud_2d_texture_wide(x, y, 16, 16, texture);
}

void draw_hud_2d_texture_32x8(s32 x, s32 y, u8* texture) {
    draw_hud_2d_texture_wide(x, y, 32, 8, texture);
}

void draw_hud_2d_texture_32x16(s32 x, s32 y, u8* texture) {
    draw_hud_2d_texture_wide(x, y, 32, 16, texture);
}

UNUSED void func_8004CBC0(s32 arg0, s32 arg1, f32 arg2, u8* texture) {
    func_8004CA58(arg0, arg1, arg2, texture, 32, 16);
}

UNUSED void draw_hud_2d_texture_32x32(s32 x, s32 y, u8* texture) {
    draw_hud_2d_texture_wide(x, y, 32, 32, texture);
}

UNUSED void func_8004CC24(s32 arg0, s32 arg1, u8* texture) {
    func_8004C628(arg0, arg1, 32, 32, texture);
}

UNUSED void draw_hud_2d_texture_40x32(s32 x, s32 y, u8* texture) {
    draw_hud_2d_texture_wide(x, y, 40, 32, texture);
}

UNUSED void func_8004CC84(s32 x, s32 y, u8* texture) {
    func_8004C91C(x, y, texture, 48, 48, 24);
}

UNUSED void func_8004CCB4(s32 x, s32 y, u8* texture) {
    draw_hud_2d_texture_wide(x, y, 64, 32, texture);
}

UNUSED void func_8004CCE4(s32 arg0, s32 arg1, f32 arg2, u8* texture) {
    func_8004CA58(arg0, arg1, arg2, texture, 64, 32);
}

UNUSED void func_8004CD18(s32 arg0, s32 arg1, u8* texture) {
    func_8004C91C(arg0, arg1, texture, 64, 64, 32);
}

UNUSED void func_8004CD48(s32 arg0, s32 arg1, UNUSED u8* texture, s32 width, s32 arg4, s32 height) {
    UNUSED s32 pad;
    s32 var_s0;
    s32 i;
    u8* img;

    var_s0 = arg1 - (arg4 / 2);
    gSPDisplayList(gDisplayListHead++, D_0D007FE0);

    for (i = 0; i < arg4 / height; i++) {
        load_texture_block_ia16_nomirror(img, width, height);
        func_8004B97C_wide(arg0 - (width / 2), var_s0, width, height, 1);
        img += width * height * 2;
        var_s0 += height;
    }
}

UNUSED void func_8004CE8C(s32 arg0, s32 arg1, u8* texture, s32 width, s32 arg4, s32 height) {
    s32 var_s0 = arg1 - (arg4 / 2);
    s32 i;
    u8* img = texture;

    for (i = 0; i < arg4 / height; i++) {
        load_texture_block_ia8_nomirror(img, width, height);
        func_8004B97C_wide(arg0 - (width / 2), var_s0, width, height, 1);
        img += width * height;
        var_s0 += height;
    }
}

UNUSED void func_8004CF9C(s32 arg0, s32 arg1, u8* texture, s32 arg3, s32 arg4, UNUSED s32 arg5, s32 arg6) {
    gSPDisplayList(gDisplayListHead++, D_0D007FE0);
    func_8004CE8C(arg0, arg1, texture, arg3, arg4, arg6);
}

UNUSED void func_8004CFF0(s32 arg0, s32 arg1, u8* texture, s32 arg3, s32 arg4, UNUSED s32 arg5, s32 arg6) {
    gSPDisplayList(gDisplayListHead++, D_0D008000);
    func_8004CE8C(arg0, arg1, texture, arg3, arg4, arg6);
}

UNUSED void func_8004D044(s32 arg0, s32 arg1, u8* texture, s32 red, s32 green, s32 blue, s32 alpha, s32 arg7, s32 arg8,
                          UNUSED s32 arg9, s32 argA) {
    gSPDisplayList(gDisplayListHead++, D_0D007FE0);
    func_8004B35C(red, green, blue, alpha);
    func_8004CE8C(arg0, arg1, texture, arg7, arg8, argA);
}

UNUSED void func_8004D0CC(void) {
}

UNUSED void func_8004D0D4(s32 arg0, s32 arg1, u8* texture, s32 width, s32 arg4, s32 height) {
    s32 var_s0;
    u8* img;
    s32 i;

    var_s0 = arg1 - (arg4 / 2);
    img = texture;
    gSPDisplayList(gDisplayListHead++, D_0D007FE0);

    for (i = 0; i < arg4 / height; i++) {
        func_80044924(img, width, height);
        func_8004B97C_wide(arg0 - (width / 2), var_s0, width, height, 1);
        img += width * height;
        var_s0 += height;
    }
}

void func_8004D210(s32 arg0, s32 arg1, u8* texture, s32 arg3, s32 arg4, s32 arg5, s32 arg6, s32 width, s32 arg8,
                   UNUSED s32 arg9, s32 height) {
    s32 var_s3;
    u8* img;
    s32 i;

    var_s3 = arg1 - (arg8 / 2);
    img = texture;
    gSPDisplayList(gDisplayListHead++, D_0D007FE0);
    func_8004B35C(arg3, arg4, arg5, arg6);

    for (i = 0; i < arg8 / height; i++) {
        func_80044924(img, width, height);
        func_8004B97C_wide(arg0 - (width / 2), var_s3, width, height, 1);
        img += (width * height) / 2;
        var_s3 += height;
    }
}

void func_8004D37C(s32 x, s32 y, u8* texture, s32 red, s32 green, s32 blue, s32 alpha, s32 width, s32 height,
                   UNUSED s32 width2, s32 height2) {
    s32 var_s3;
    u8* img;
    s32 i;

    img = texture;
    gSPDisplayList(gDisplayListHead++, D_0D007FE0);
    func_8004B414(red, green, blue, alpha);

    func_80044F34(img, width, height);
    func_8004B97C_wide(x - (width / 2), y - (height / 2), width, height, 1);
}

void func_8004D4E8(s32 arg0, s32 arg1, u8* texture, s32 red, s32 green, s32 blue, s32 alpha, s32 width, s32 height,
                   UNUSED s32 width2, s32 height2) {
    s32 var_s3;
    u8* img;
    s32 i;

    var_s3 = arg1 - (height / 2);
    img = texture;
    gSPDisplayList(gDisplayListHead++, D_0D007FE0);
    func_8004B414(red, green, blue, alpha);
    func_800450C8(img, width, height2);
    func_8004BA08(arg0 - (width / 2), var_s3, width, height2, 1);
    img += (width * height2) / 2;
    var_s3 += height2;
}

void func_8004D654(s32 arg0, s32 arg1, u8* texture, f32 arg3, s32 arg4, s32 arg5, s32 arg6, UNUSED s32 arg7, s32 width,
                   s32 arg9, UNUSED s32 argA, s32 height) {
    s32 i;
    s32 var_s3;
    u8* textureCopy;

    var_s3 = arg1 - (arg9 / 2);
    textureCopy = texture;
    gSPDisplayList(gDisplayListHead++, D_0D008000);
    func_8004B480(arg4, arg5, arg6);
    for (i = 0; i < (arg9 / height); i++) {
        func_80044F34(textureCopy, width, height);
        func_8004BB3C(arg0, arg1, width, arg9, arg3);
        textureCopy += (width * height) / 2;
        var_s3 += height;
    }
}

void func_8004D7B4(s32 arg0, s32 arg1, u8* texture, s32 arg3, s32 arg4) {
    s32 sp5C;
    f32 temp_f20;
    s16 temp_s7;
    s16 var_s1;
    u16 temp_s0;
    s32 temp_s5;
    s32 var_s3;
    u8* img;
    UNUSED s32 test[3];
    s32 i;

    D_801656B0 += D_80165710;
    temp_f20 = D_8018D00C;
    temp_s7 = D_80165708;
    var_s1 = D_801656B0;
    img = texture;
    var_s3 = arg1 - (arg4 / 2);
    gSPDisplayList(gDisplayListHead++, D_0D007FE0);

    sp5C = arg3 * 2;
    for (i = 0; i < arg4; i++) {
        temp_s0 = var_s1;
        temp_s5 = (s32) ((sins(temp_s0) * temp_f20) + (f32) (arg0 - (arg3 / 2)));
        sins(temp_s0);
        load_texture_block_ia16_nomirror(img, arg3, 1);
        func_8004B97C_wide(temp_s5, var_s3, arg3, 1, 1);

        var_s1 += temp_s7;
        var_s3 += 1;
        img += sp5C;
    }
}

void func_8004D93C(s32 arg0, s32 arg1, u8* texture, s32 arg3, s32 arg4) {
    f32 temp_f20;
    s16 temp_s7;
    s16 var_s1;
    u16 temp_s0;
    s32 temp_s6;
    s32 var_s4;
    u8* img;
    s32 i;
    s32 var;

    D_801656B0 += D_80165710;
    temp_f20 = D_8018D00C;
    temp_s7 = D_80165708;
    var_s1 = D_801656B0;
    img = texture;
    var = arg3 / 2;
    var_s4 = arg1 - (arg4 / 2);

    gSPDisplayList(gDisplayListHead++, D_0D007FE0);

    for (i = 0; i < arg4; i++) {
        temp_s0 = var_s1;
        temp_s6 = (s32) ((sins(temp_s0) * temp_f20) + (f32) (arg0 - (var)));
        sins(temp_s0);
        load_texture_block_ia8_nomirror(img, arg3, 1);
        func_8004B97C_wide(temp_s6, var_s4, arg3, 1, 1);
        var_s1 += temp_s7;
        img = &img[arg3];
        var_s4 += 1;
    }
}

UNUSED void func_8004DAB8(s32 arg0, s32 arg1, u8* texture, s32 arg3, s32 arg4) {
    f32 temp_f20;
    s16 temp_s7;
    s16 var_s1;
    u16 temp_s0;
    s32 temp_s6;
    u8* img;
    s32 var_s4;
    s32 var;
    s32 i;

    D_801656B0 += D_80165710;
    temp_f20 = D_8018D00C;
    temp_s7 = D_80165708;
    var_s1 = (s16) D_801656B0;
    img = texture;
    var = arg3 / 2;
    var_s4 = arg1 - (arg4 / 2);

    gSPDisplayList(gDisplayListHead++, D_0D007FE0);
    for (i = 0; i < arg4; i++) {
        temp_s0 = var_s1;
        temp_s6 = (s32) ((sins(temp_s0) * temp_f20) + (f32) (arg0 - (var)));
        sins(temp_s0);
        func_80044924(img, arg3, 1);
        func_8004B97C_wide(temp_s6, var_s4, arg3, 1, 1);
        var_s1 += temp_s7;
        img += arg3;
        var_s4 += 1;
    }
}

UNUSED void func_8004DC34(s32 arg0, s32 arg1, u8* texture) {
    func_8004CF9C(arg0, arg1, texture, 8, 160, 8, 160);
}

UNUSED void func_8004DC6C(s32 arg0, s32 arg1, u8* texture) {
    func_8004CF9C(arg0, arg1, texture, 12, 160, 12, 160);
}

UNUSED void func_8004DCA4(s32 arg0, s32 arg1, u8* texture) {
    func_8004CF9C(arg0, arg1, texture, 12, 192, 12, 192);
}

UNUSED void func_8004DCDC(s32 arg0, s32 arg1, u8* texture) {
    func_8004CD48(arg0, arg1, texture, 16, 16, 16);
}

UNUSED void func_8004DD0C(s32 arg0, s32 arg1, u8* texture) {
    func_8004CF9C(arg0, arg1, texture, 16, 160, 16, 160);
}

UNUSED void func_8004DD44(s32 arg0, s32 arg1, u8* texture) {
    func_8004CD48(arg0, arg1, texture, 32, 32, 32);
}

UNUSED void func_8004DD74(s32 arg0, s32 arg1, u8* texture) {
    func_8004CF9C(arg0, arg1, texture, 32, 32, 32, 32);
}

UNUSED void func_8004DDAC(s32 arg0, s32 arg1, u8* texture) {
    func_8004D0D4(arg0, arg1, texture, 32, 32, 32);
}

UNUSED void func_8004DDDC(s32 arg0, s32 arg1, u8* texture) {
    func_8004D7B4(arg0, arg1, texture, 32, 32);
}

UNUSED void func_8004DE04(s32 arg0, s32 arg1, u8* texture) {
    func_8004D93C(arg0, arg1, texture, 32, 32);
}

UNUSED void func_8004DE2C(s32 arg0, s32 arg1, u8* arg2) {
    func_8004DAB8(arg0, arg1, arg2, 32, 32);
}

UNUSED void func_8004DE54(s32 arg0, s32 arg1, u8* arg2) {
    func_8004CD48(arg0, arg1, arg2, 64, 32, 32);
}

UNUSED void func_8004DE84(s32 arg0, s32 arg1, u8* arg2) {
    func_8004CD48(arg0, arg1, arg2, 64, 64, 32);
}

UNUSED void func_8004DEB4(s32 arg0, s32 arg1, u8* texture) {
    func_8004CF9C(arg0, arg1, texture, 64, 96, 64, 48);
}

UNUSED void func_8004DEEC(s32 arg0, s32 arg1, u8* arg2) {
    func_8004CF9C(arg0, arg1, arg2, 112, 32, 112, 32);
}

UNUSED void func_8004DF24(s32 arg0, s32 arg1, u8* arg2) {
    func_8004CF9C(arg0, arg1, arg2, 128, 32, 128, 32);
}

// Positions the item box on screen
void func_8004DF5C(s32 arg0, s32 arg1, u8* texture, s32 width, s32 arg4, s32 height) {
    s32 var_s0 = var_s0 = arg1 - (arg4 / 2);
    u8* img = texture;
    s32 i;

    for (i = 0; i < arg4 / height; i++) {
        rsp_load_texture(img, width, height);
        func_8004B97C(arg0 - (width / 2), var_s0, width, height, 1);
        img += width * height;
        var_s0 += height;
    }
}

void func_8004E06C(s32 arg0, s32 arg1, u8* texture, s32 arg3, s32 arg4) {
    f32 temp_f20;
    s16 temp_s7;
    s16 var_s1;
    u16 temp_s0;
    s32 var_s4;
    u8* img;
    u32 temp_s6;
    s32 i;
    s32 var;

    D_801656B0 += D_80165710;
    temp_f20 = D_8018D00C;
    var_s1 = (s16) D_801656B0;
    temp_s7 = D_80165708;
    img = texture;
    var = arg3 / 2;
    var_s4 = arg1 - (arg4 / 2);

    for (i = 0; i < arg4; i++) {
        temp_s0 = var_s1;
        temp_s6 = (u32) ((sins(temp_s0) * temp_f20) + (f32) (arg0 - var));
        sins(temp_s0);
        rsp_load_texture(img, arg3, 1);
        func_8004B97C_wide(temp_s6, var_s4, arg3, 1, 1);
        var_s1 += temp_s7;
        img += arg3;
        var_s4 += 1;
    }
}

UNUSED void func_8004E238(void) {
}

void func_8004E240(s32 arg0, s32 arg1, u8* tlut, u8* texture, s32 arg4, s32 arg5, s32 arg6) {
    gSPDisplayList(gDisplayListHead++, D_0D007CB8);
    func_8004B05C(tlut);
    func_8004DF5C(arg0, arg1, texture, arg4, arg5, arg6);
}

void func_8004E2B8(s32 arg0, s32 arg1, s32 arg2, u8* tlut, u8* texture, s32 arg5, s32 arg6, s32 arg7) {
    gSPDisplayList(gDisplayListHead++, D_0D007DB8);
    set_transparency(arg2);
    func_8004B05C(tlut);
    func_8004DF5C(arg0, arg1, texture, arg5, arg6, arg7);
}

void func_8004E338(s32 arg0, s32 arg1, u8* tlut, u8* texture, s32 arg4, s32 arg5) {
    gSPDisplayList(gDisplayListHead++, D_0D007DB8);
    set_transparency(D_8016589C);
    func_8004B05C(tlut);
    func_8004E06C(arg0, arg1, texture, arg4, arg5);
}

UNUSED void func_8004E3B8(void) {
}

UNUSED void func_8004E3C0(s32 arg0, s32 arg1, u8* tlut, u8* texture, s32 arg4, s32 arg5, UNUSED s32 arg6, s32 arg7) {
    func_8004E240(arg0, arg1, tlut, texture, arg4, arg5, arg7);
}

UNUSED void func_8004E3F4(s32 arg0, s32 arg1, s32 arg2, u8* tlut, u8* texture, s32 arg5, s32 arg6, UNUSED s32 arg7,
                          s32 arg8) {
    func_8004E2B8(arg0, arg1, arg2, tlut, texture, arg5, arg6, arg8);
}

UNUSED void func_8004E430(s32 arg0, s32 arg1, u8* tlut, u8* texture) {
    func_8004E240(arg0, arg1, tlut, texture, 8, 128, 128);
}

UNUSED void func_8004E464(s32 arg0, s32 arg1, u8* tlut, u8* texture) {
    func_8004E240(arg0, arg1, tlut, texture, 32, 32, 32);
}

UNUSED void func_8004E498(s32 arg0, s32 arg1, u8* tlut, u8* texture) {
    func_8004E240(arg0, arg1, tlut, texture, 32, 64, 64);
}

void func_8004E4CC(s32 arg0, s32 arg1, u8* tlut, u8* texture) {
    func_8004E240(arg0, arg1, tlut, texture, 40, 32, 32);
}

UNUSED void func_8004E500(s32 arg0, s32 arg1, u8* tlut, u8* texture) {
    func_8004E240(arg0, arg1, tlut, texture, 48, 48, 24);
}

UNUSED void func_8004E534(s32 arg0, s32 arg1, u8* tlut, u8* texture) {
    func_8004E240(arg0, arg1, tlut, texture, 64, 32, 32);
}

UNUSED void func_8004E568(s32 arg0, s32 arg1, u8* tlut, u8* texture) {
    func_8004E240(arg0, arg1, tlut, texture, 64, 64, 32);
}

UNUSED void func_8004E59C(s32 arg0, s32 arg1, s32 arg2, u8* tlut, u8* texture) {
    func_8004E2B8(arg0, arg1, arg2, tlut, texture, 64, 64, 32);
}

UNUSED void func_8004E5D8(s32 arg0, s32 arg1, u8* tlut, u8* arg3) {
    func_8004E338(arg0, arg1, tlut, arg3, 64, 64);
}

UNUSED void func_8004E604(s32 arg0, s32 arg1, u8* tlut, u8* texture) {
    func_8004E240(arg0, arg1, tlut, texture, SCREEN_WIDTH, SCREEN_HEIGHT, 6);
}

#define ARCADEKART_ITEM_BOX_Y_OFFSET_DEFAULT 4.0f

void draw_item_window(s32 playerId) {
    s32 objectIndex;
    s32 itemBoxX;
    s32 itemBoxY;
    Object* object;
    hud_player* temp_v0;

    objectIndex = gItemWindowObjectByPlayerId[playerId];
    object = &gObjectList[objectIndex];
    if (object->state >= 2) {
        temp_v0 = &playerHUD[playerId];
        if ((gPlayerCountSelection1 == 1) && (playerId == PLAYER_ONE)) {
            temp_v0->itemBoxX = 0x00A0;
        }
        itemBoxX = temp_v0->slideItemBoxX + temp_v0->itemBoxX;
        itemBoxY = temp_v0->slideItemBoxY + temp_v0->itemBoxY;
        if ((gPlayerCountSelection1 == 1) && (playerId == PLAYER_ONE)) {
            itemBoxY += (s32) CVarGetFloat("gArcadeKart.Hud.ItemBoxYOffset", ARCADEKART_ITEM_BOX_Y_OFFSET_DEFAULT);
        }
        func_8004E4CC(itemBoxX, itemBoxY, (u8*) object->activeTLUT, object->activeTexture);
    }
}

void func_8004E6C4(s32 playerId) {
    s32 objectIndex;
    Object* object;
    hud_player* temp_v0;

    objectIndex = gItemWindowObjectByPlayerId[playerId];
    object = &gObjectList[objectIndex];
    if (object->state >= 2) {
        temp_v0 = &playerHUD[playerId];
        FrameInterpolation_RecordOpenChild("item_window_splitscreen", playerId);
        func_80047910(temp_v0->slideItemBoxX + temp_v0->itemBoxX, temp_v0->slideItemBoxY + temp_v0->itemBoxY, 0U,
                      temp_v0->unknownScaling, (u8*) object->activeTLUT, (u8*) object->activeTexture, (Vtx*)LOAD_ASSET(D_0D005C30),
                      0x00000028, 0x00000020, 0x00000028, 0x00000020);
        FrameInterpolation_RecordCloseChild();
    }
}

void draw_simplified_lap_count(s32 playerId) {
    draw_hud_2d_texture_32x8((s32) playerHUD[playerId].lapX, playerHUD[playerId].lapY + 3,
                             (u8*) common_texture_hud_lap);
    draw_hud_2d_texture_32x16(playerHUD[playerId].lapX + 0x1C, (s32) playerHUD[playerId].lapY,
                              (u8*) gHudLapTextures[playerHUD[playerId].alsoLapCount]);
}

#define ARCADEKART_HUD_REFERENCE_WIDTH 320.0f
#define ARCADEKART_HUD_REFERENCE_HEIGHT 240.0f
#define ARCADEKART_HUD_EDGE_MARGIN_DEFAULT 32.0f
#define ARCADEKART_HUD_SAFE_ZONE_X_DEFAULT 28.0f
#define ARCADEKART_HUD_SAFE_ZONE_TOP_DEFAULT 0.0f
#define ARCADEKART_HUD_SAFE_ZONE_BOTTOM_DEFAULT 10.0f

// Anchor padding is expressed as a ratio of the current player view so split-screen gets proportional margins.
static HudRect get_arcadekart_hud_player_view_rect(UNUSED s32 playerId) {
    HudRect rect;

    switch (gScreenModeSelection) {
        case SCREEN_MODE_2P_SPLITSCREEN_HORIZONTAL:
            rect = hud_layout_rect(0.0f, 0.0f, ARCADEKART_HUD_REFERENCE_WIDTH, ARCADEKART_HUD_REFERENCE_HEIGHT * 0.5f);
            break;
        case SCREEN_MODE_2P_SPLITSCREEN_VERTICAL:
        case SCREEN_MODE_3P_4P_SPLITSCREEN:
            rect = hud_layout_rect(0.0f, 0.0f, ARCADEKART_HUD_REFERENCE_WIDTH * 0.5f,
                                   ARCADEKART_HUD_REFERENCE_HEIGHT * 0.5f);
            break;
        case SCREEN_MODE_1P:
        default:
            rect = hud_layout_rect(0.0f, 0.0f, ARCADEKART_HUD_REFERENCE_WIDTH, ARCADEKART_HUD_REFERENCE_HEIGHT);
            break;
    }

    return rect;
}

static f32 get_arcadekart_hud_view_scale(HudRect viewRect) {
    f32 scaleX = viewRect.w / ARCADEKART_HUD_REFERENCE_WIDTH;
    f32 scaleY = viewRect.h / ARCADEKART_HUD_REFERENCE_HEIGHT;
    return (scaleX < scaleY) ? scaleX : scaleY;
}

static f32 get_arcadekart_hud_view_x(HudRect viewRect, f32 referenceUnits) {
    return viewRect.w * (referenceUnits / ARCADEKART_HUD_REFERENCE_WIDTH);
}

static f32 get_arcadekart_hud_view_y(HudRect viewRect, f32 referenceUnits) {
    return viewRect.h * (referenceUnits / ARCADEKART_HUD_REFERENCE_HEIGHT);
}

static f32 get_arcadekart_hud_cluster_scale(HudRect viewRect, const char* cvarName, f32 defaultValue) {
    f32 scale = CVarGetFloat(cvarName, defaultValue) * get_arcadekart_hud_view_scale(viewRect);

    if (scale < 0.25f) {
        scale = 0.25f;
    }
    if (scale > 2.0f) {
        scale = 2.0f;
    }
    return scale;
}

static f32 get_arcadekart_hud_safe_zone_x_units(void) {
    return CVarGetFloat("gArcadeKart.Hud.SafeZoneX",
                        CVarGetFloat("gArcadeKart.Hud.RpmMeterRightMargin", ARCADEKART_HUD_SAFE_ZONE_X_DEFAULT));
}

static f32 get_arcadekart_hud_safe_zone_bottom_units(void) {
    return CVarGetFloat("gArcadeKart.Hud.SafeZoneBottom",
                        CVarGetFloat("gArcadeKart.Hud.RpmMeterBottomMargin", ARCADEKART_HUD_SAFE_ZONE_BOTTOM_DEFAULT));
}

static HudPadding get_arcadekart_hud_safe_zone_padding(HudRect viewRect) {
    f32 safeX = get_arcadekart_hud_safe_zone_x_units();
    f32 safeTop = CVarGetFloat("gArcadeKart.Hud.SafeZoneTop", ARCADEKART_HUD_SAFE_ZONE_TOP_DEFAULT);
    f32 safeBottom = get_arcadekart_hud_safe_zone_bottom_units();

    return hud_layout_padding(get_arcadekart_hud_view_x(viewRect, safeX),
                              get_arcadekart_hud_view_y(viewRect, safeTop),
                              get_arcadekart_hud_view_x(viewRect, safeX),
                              get_arcadekart_hud_view_y(viewRect, safeBottom));
}

#define ARCADEKART_PLACE_NUMBER_WIDGET_WIDTH 128.0f
#define ARCADEKART_PLACE_NUMBER_WIDGET_HEIGHT 64.0f
#define ARCADEKART_PLACE_NUMBER_DRAW_X 29.0f
#define ARCADEKART_PLACE_NUMBER_DRAW_Y 12.0f
#define ARCADEKART_PLACE_NUMBER_SCREEN_PADDING 10.0f
#define ARCADEKART_PLACE_NUMBER_DEFAULT_LEFT_CORRECTION 16.0f
#define ARCADEKART_PLACE_NUMBER_DEFAULT_TOP_CORRECTION 14.0f

typedef struct ArcadeKartPlaceNumberLeaf {
    s32 playerId;
    s32 rankIndex;
    s32 fadeAlpha;
} ArcadeKartPlaceNumberLeaf;

static s32 clamp_arcadekart_place_rank_index(s32 rankIndex) {
    if (rankIndex < 0) {
        return 0;
    }
    if (rankIndex > 7) {
        return 7;
    }
    return rankIndex;
}

static void render_place_number_at(s32 playerId, s32 rankIndex, s32 fadeAlpha, f32 centerX, f32 centerY,
                                   f32 widgetScale) {
    rankIndex = clamp_arcadekart_place_rank_index(rankIndex);
    func_8004A384((s32) centerX, (s32) centerY, 0U, playerHUD[playerId].rankScaling * widgetScale, 0x000000FF,
                  fadeAlpha, 0, 0x000000FF, common_texture_hud_place[rankIndex], D_0D0068F0, 0x00000080,
                  0x00000040, 0x00000080, 0x00000040);
}

static void draw_arcadekart_place_number_leaf(UNUSED const HudLayoutContext* ctx, UNUSED HudWidgetId widgetId,
                                              HudRect rect, void* userData) {
    ArcadeKartPlaceNumberLeaf* leaf = (ArcadeKartPlaceNumberLeaf*) userData;
    f32 scaleX;
    f32 scaleY;
    f32 widgetScale;
    f32 contentWidth;
    f32 contentHeight;
    f32 contentX;
    f32 contentY;
    f32 centerX;
    f32 centerY;

    if ((leaf == NULL) || (rect.w <= 0.0f) || (rect.h <= 0.0f)) {
        return;
    }

    scaleX = rect.w / ARCADEKART_PLACE_NUMBER_WIDGET_WIDTH;
    scaleY = rect.h / ARCADEKART_PLACE_NUMBER_WIDGET_HEIGHT;
    widgetScale = (scaleX < scaleY) ? scaleX : scaleY;
    contentWidth = ARCADEKART_PLACE_NUMBER_WIDGET_WIDTH * widgetScale;
    contentHeight = ARCADEKART_PLACE_NUMBER_WIDGET_HEIGHT * widgetScale;
    contentX = rect.x + ((rect.w - contentWidth) * 0.5f);
    contentY = rect.y + ((rect.h - contentHeight) * 0.5f);
    centerX = contentX + (ARCADEKART_PLACE_NUMBER_DRAW_X * widgetScale);
    centerY = contentY + (ARCADEKART_PLACE_NUMBER_DRAW_Y * widgetScale);
    render_place_number_at(leaf->playerId, leaf->rankIndex, leaf->fadeAlpha, centerX, centerY, widgetScale);
}

static void render_arcadekart_place_number_layout(s32 playerId, s32 rankIndex, s32 fadeAlpha) {
    HudLayoutContext layout;
    ArcadeKartPlaceNumberLeaf leaf;
    HudWidgetId root;
    HudWidgetId scaleBox;
    HudWidgetId placeNumber;
    HudRect viewRect = get_arcadekart_hud_player_view_rect(playerId);
    f32 placeScale = get_arcadekart_hud_cluster_scale(viewRect, "gArcadeKart.Hud.PlaceNumberScale", 1.0f);
    f32 leftCorrection = CVarGetFloat("gArcadeKart.Hud.PlaceNumberLeftCorrection",
                                      ARCADEKART_PLACE_NUMBER_DEFAULT_LEFT_CORRECTION);
    f32 topCorrection = CVarGetFloat("gArcadeKart.Hud.PlaceNumberTopCorrection",
                                     ARCADEKART_PLACE_NUMBER_DEFAULT_TOP_CORRECTION);
    f32 edgePaddingX = get_arcadekart_hud_view_x(viewRect, ARCADEKART_PLACE_NUMBER_SCREEN_PADDING) +
                       ((64.0f - ARCADEKART_PLACE_NUMBER_DRAW_X - leftCorrection) * placeScale);
    f32 edgePaddingY = get_arcadekart_hud_view_y(viewRect, ARCADEKART_PLACE_NUMBER_SCREEN_PADDING);
    f32 visibleTopInset = (32.0f - ARCADEKART_PLACE_NUMBER_DRAW_Y - topCorrection) * placeScale;
    if (visibleTopInset < 0.0f) {
        visibleTopInset = 0.0f;
    }

    leaf.playerId = playerId;
    leaf.rankIndex = rankIndex;
    leaf.fadeAlpha = fadeAlpha;

    hud_layout_begin_safe_zone(&layout, viewRect, get_arcadekart_hud_safe_zone_padding(viewRect));
    root = hud_layout_root(&layout);
    scaleBox = hud_layout_scale_box(&layout, HUD_SCALE_STRETCH_USER, HUD_SCALE_DIRECTION_BOTH, placeScale);
    placeNumber =
        hud_layout_draw(&layout, hud_layout_vec2(ARCADEKART_PLACE_NUMBER_WIDGET_WIDTH,
                                                 ARCADEKART_PLACE_NUMBER_WIDGET_HEIGHT),
                        draw_arcadekart_place_number_leaf, &leaf);
    hud_layout_single_child_add(&layout, scaleBox, placeNumber,
                                hud_layout_single_child_slot(hud_layout_padding(0.0f, 0.0f, 0.0f, 0.0f),
                                                             hud_layout_vec2(0.5f, 0.5f), false, false));
    hud_layout_canvas_add(&layout, root, scaleBox,
                          hud_layout_canvas_safe_slot(hud_layout_anchor(0.0f, 0.0f, 0.0f, 0.0f),
                                                      hud_layout_padding(edgePaddingX, edgePaddingY + visibleTopInset,
                                                                         0.0f, 0.0f),
                                                      hud_layout_vec2(0.0f, 0.0f), true, 0));
    hud_layout_arrange(&layout);
    hud_layout_draw_tree(&layout);
}

void func_8004E800(s32 playerId) {
    FrameInterpolation_RecordOpenChild("Player place HUD", playerId);
    if (playerHUD[playerId].unk_81 != 0) {
        if ((gPlayerCountSelection1 == 1) && (playerId == PLAYER_ONE)) {
            if (playerHUD[playerId].lapCount != 3) {
                render_arcadekart_place_number_layout(playerId, D_8018CF98[playerId],
                                                      D_800E55F8[clamp_arcadekart_place_rank_index(
                                                          D_8018CF98[playerId])]);
            } else {
                render_arcadekart_place_number_layout(
                    playerId, gGPCurrentRaceRankByPlayerId[playerId],
                    D_800E55F8[clamp_arcadekart_place_rank_index(D_80165594)]);
            }
        } else if (playerHUD[playerId].lapCount != 3) {
            func_8004A384(playerHUD[playerId].rankX + playerHUD[playerId].slideRankX,
                          playerHUD[playerId].rankY + playerHUD[playerId].slideRankY, 0U,
                          playerHUD[playerId].rankScaling, 0x000000FF, D_800E55F8[D_8018CF98[playerId]], 0, 0x000000FF,
                          common_texture_hud_place[D_8018CF98[playerId]], D_0D0068F0, 0x00000080, 0x00000040,
                          0x00000080, 0x00000040);
        } else {
            func_8004A384(playerHUD[playerId].rankX + playerHUD[playerId].slideRankX,
                          playerHUD[playerId].rankY + playerHUD[playerId].slideRankY, 0U,
                          playerHUD[playerId].rankScaling, 0x000000FF, D_800E55F8[D_80165594], 0, 0x000000FF,
                          common_texture_hud_place[gGPCurrentRaceRankByPlayerId[playerId]], D_0D0068F0, 0x00000080,
                          0x00000040, 0x00000080, 0x00000040);
        }
    }
    FrameInterpolation_RecordCloseChild();
}

void func_8004E998(s32 playerId) {
    if (playerHUD[playerId].unk_81 != 0) {
        FrameInterpolation_RecordOpenChild("Player place HUD2", playerId);
        if (playerHUD[playerId].lapCount != 3) {
            func_8004A384(playerHUD[playerId].rankX + playerHUD[playerId].slideRankX,
                          playerHUD[playerId].rankY + playerHUD[playerId].slideRankY, 0U,
                          playerHUD[playerId].rankScaling, 0x000000FF,
                          D_800E5618[gGPCurrentRaceRankByPlayerId[playerId]], 0, 0x000000FF,
                          D_0D015258[gGPCurrentRaceRankByPlayerId[playerId]], D_0D006030, 0x00000040, 0x00000040,
                          0x00000040, 0x00000040);
        } else {
            func_8004A384(playerHUD[playerId].rankX + playerHUD[playerId].slideRankX,
                          playerHUD[playerId].rankY + playerHUD[playerId].slideRankY, 0U,
                          playerHUD[playerId].rankScaling, 0x000000FF, D_800E5618[D_80165598], 0, 0x000000FF,
                          D_0D015258[gGPCurrentRaceRankByPlayerId[playerId]], D_0D006030, 0x00000040, 0x00000040,
                          0x00000040, 0x00000040);
        }
        FrameInterpolation_RecordCloseChild();
    }
}

void func_8004EB30(UNUSED s32 arg0) {
}

void func_8004EB38(UNUSED s32 playerId) {
}

static f32 get_display_rpm(f32 actualRpm) {
    return actualRpm * CVarGetFloat("gArcadeKart.RpmDisplayMultiplier", KART_RPM_DISPLAY_MULTIPLIER_DEFAULT);
}

static f32 get_arcadekart_rpm_meter_motion_max(const Player* player) {
    f32 motionMax = (player != NULL) ? kart_transmission_get_shift_ideal_rpm_max(player) :
                                       KART_RPM_METER_MOTION_MAX_DEFAULT;
    f32 characterScale =
        CVarGetFloat("gArcadeKart.Hud.RpmMeterCharacterRangeScale", KART_RPM_METER_CHARACTER_RANGE_SCALE_DEFAULT);

    motionMax = CVarGetFloat("gArcadeKart.Hud.RpmMeterMotionMaxRpm", motionMax);
    if ((player != NULL) && (player->characterId >= 0) && (player->characterId < KART_CHARACTER_STATS_COUNT)) {
        f32 torqueMultiplier = kart_character_stats_get_torque_multiplier(player->characterId);
        motionMax *= 1.0f + ((torqueMultiplier - 1.0f) * characterScale);
    }

    if (motionMax < 1000.0f) {
        motionMax = 1000.0f;
    }
    return motionMax;
}

#define ARCADEKART_RPM_METER_WIDGET_WIDTH 96.0f
#define ARCADEKART_RPM_METER_WIDGET_HEIGHT 96.0f
#define ARCADEKART_RPM_METER_CENTER_X 48.0f
#define ARCADEKART_RPM_METER_CENTER_Y 48.0f
#define ARCADEKART_RPM_METER_TEXTURE_SCALE 0.78f
#define ARCADEKART_RPM_METER_NEEDLE_OFFSET_X 18.0f
#define ARCADEKART_RPM_METER_NEEDLE_OFFSET_Y 5.0f
#define ARCADEKART_RPM_METER_SCREEN_PADDING 2.0f
#define ARCADEKART_RPM_METER_RIGHT_MARGIN ARCADEKART_HUD_EDGE_MARGIN_DEFAULT
#define ARCADEKART_RPM_METER_RIGHT_INSET 4.0f
#define ARCADEKART_RPM_METER_BOTTOM_INSET 8.0f
#define ARCADEKART_RPM_METER_FACE_VISIBLE_LEFT_X -32.0f
#define ARCADEKART_RPM_METER_FACE_VISIBLE_RIGHT_X 31.0f
#define ARCADEKART_RPM_METER_FACE_VISIBLE_TOP_Y -47.0f
#define ARCADEKART_RPM_METER_FACE_VISIBLE_BOTTOM_Y 46.75f
#define ARCADEKART_RPM_METER_FACE_BOUNDS_LEFT \
    (ARCADEKART_RPM_METER_CENTER_X + (ARCADEKART_RPM_METER_FACE_VISIBLE_LEFT_X * ARCADEKART_RPM_METER_TEXTURE_SCALE))
#define ARCADEKART_RPM_METER_FACE_BOUNDS_RIGHT \
    (ARCADEKART_RPM_METER_CENTER_X + (ARCADEKART_RPM_METER_FACE_VISIBLE_RIGHT_X * ARCADEKART_RPM_METER_TEXTURE_SCALE))
#define ARCADEKART_RPM_METER_FACE_BOUNDS_TOP \
    (ARCADEKART_RPM_METER_CENTER_Y + (ARCADEKART_RPM_METER_FACE_VISIBLE_TOP_Y * ARCADEKART_RPM_METER_TEXTURE_SCALE))
#define ARCADEKART_RPM_METER_FACE_BOUNDS_BOTTOM \
    (ARCADEKART_RPM_METER_CENTER_Y + (ARCADEKART_RPM_METER_FACE_VISIBLE_BOTTOM_Y * ARCADEKART_RPM_METER_TEXTURE_SCALE))
#define ARCADEKART_RPM_METER_FACE_BOUNDS_WIDTH \
    (ARCADEKART_RPM_METER_FACE_BOUNDS_RIGHT - ARCADEKART_RPM_METER_FACE_BOUNDS_LEFT)
#define ARCADEKART_RPM_METER_FACE_BOUNDS_HEIGHT \
    (ARCADEKART_RPM_METER_FACE_BOUNDS_BOTTOM - ARCADEKART_RPM_METER_FACE_BOUNDS_TOP)
#define ARCADEKART_RPM_METER_NEEDLE_FACE_OFFSET_X \
    (ARCADEKART_RPM_METER_NEEDLE_OFFSET_X / ARCADEKART_RPM_METER_TEXTURE_SCALE)
#define ARCADEKART_RPM_METER_NEEDLE_FACE_OFFSET_Y \
    (ARCADEKART_RPM_METER_NEEDLE_OFFSET_Y / ARCADEKART_RPM_METER_TEXTURE_SCALE)
#define ARCADEKART_RPM_METER_GEAR_TEXT_SCALE 0.42f
#define ARCADEKART_RPM_METER_GEAR_PANEL_INSET_X 2.0f
#define ARCADEKART_RPM_METER_GEAR_PANEL_INSET_Y 2.0f

typedef enum ArcadeKartRpmMeterPass {
    ARCADEKART_RPM_METER_PASS_DIAL,
    ARCADEKART_RPM_METER_PASS_DIGITAL
} ArcadeKartRpmMeterPass;

typedef enum ArcadeKartRpmMeterElement {
    ARCADEKART_RPM_METER_ELEMENT_FACEPLATE,
    ARCADEKART_RPM_METER_ELEMENT_NEEDLE_GLOW,
    ARCADEKART_RPM_METER_ELEMENT_NEEDLE,
    ARCADEKART_RPM_METER_ELEMENT_GEAR,
    ARCADEKART_RPM_METER_ELEMENT_SHIFT_TEXT,
    ARCADEKART_RPM_METER_ELEMENT_DIGITAL
} ArcadeKartRpmMeterElement;

typedef struct ArcadeKartRpmMeterLeaf {
    s32 playerIdx;
    ArcadeKartRpmMeterElement element;
} ArcadeKartRpmMeterLeaf;

typedef struct ArcadeKartRpmMeterCanvas {
    f32 centerX;
    f32 centerY;
    f32 canvasScale;
    f32 faceScale;
} ArcadeKartRpmMeterCanvas;

static f32 get_arcadekart_text_width(const char* text, f32 scale);

static ArcadeKartRpmMeterCanvas get_arcadekart_rpm_meter_canvas(HudRect rect) {
    ArcadeKartRpmMeterCanvas canvas;
    f32 scaleX = rect.w / ARCADEKART_RPM_METER_WIDGET_WIDTH;
    f32 scaleY = rect.h / ARCADEKART_RPM_METER_WIDGET_HEIGHT;
    f32 contentWidth;
    f32 contentHeight;
    f32 contentX;
    f32 contentY;

    canvas.canvasScale = (scaleX < scaleY) ? scaleX : scaleY;
    contentWidth = ARCADEKART_RPM_METER_WIDGET_WIDTH * canvas.canvasScale;
    contentHeight = ARCADEKART_RPM_METER_WIDGET_HEIGHT * canvas.canvasScale;
    contentX = rect.x + ((rect.w - contentWidth) * 0.5f);
    contentY = rect.y + ((rect.h - contentHeight) * 0.5f);
    canvas.centerX = contentX + (ARCADEKART_RPM_METER_CENTER_X * canvas.canvasScale);
    canvas.centerY = contentY + (ARCADEKART_RPM_METER_CENTER_Y * canvas.canvasScale);
    canvas.faceScale = ARCADEKART_RPM_METER_TEXTURE_SCALE * canvas.canvasScale;
    return canvas;
}

static void arcadekart_rpm_meter_canvas_to_screen(const ArcadeKartRpmMeterCanvas* canvas, f32 localX, f32 localY,
                                                  f32* screenX, f32* screenY) {
    *screenX = canvas->centerX + (localX * canvas->canvasScale);
    *screenY = canvas->centerY + (localY * canvas->canvasScale);
}

static const char* get_arcadekart_gear_label(s32 playerIdx) {
    static char gearDigits[4];
    s32 gear = kart_transmission_get_gear(playerIdx);
    const char* gearLabel = "N";

    if (gear == KART_GEAR_REVERSE) {
        gearLabel = "R";
    } else if (gear > KART_GEAR_NEUTRAL) {
        snprintf(gearDigits, sizeof(gearDigits), "%d", gear);
        gearLabel = gearDigits;
    }

    return gearLabel;
}

static void render_digital_speedometer_at(s32 playerIdx, f32 centerX, f32 centerY, f32 widgetScale) {
    char str[20];
    f32 rpm = get_display_rpm(kart_transmission_get_engine_rpm(&gPlayers[playerIdx], playerIdx));
    const char* gearLabel = get_arcadekart_gear_label(playerIdx);
    set_text_color(TEXT_YELLOW);

    size_t len = (size_t) snprintf(str, sizeof(str), "%s %.0f", gearLabel, rpm);
    if (len >= sizeof(str)) {
        printf("[render_objects.c] [render_digital_speedometer] str buffer too small, characters were discarded!\n");
    }

    text_draw((s32) (centerX - (16.0f * widgetScale)), (s32) (centerY + (26.0f * widgetScale)), str, 0,
              0.38f * widgetScale, 0.38f * widgetScale);
}

Vtx speedometer_vtx[] = {
    { { { -32, -47, 0 }, 0, { 0, 0 }, { 255, 255, 255, 255 } } },
    { { { 31, -47, 0 }, 0, { 4032, 0 }, { 255, 255, 255, 255 } } },
    { { { 31, 47, 0 }, 0, { 4032, 6016 }, { 255, 255, 255, 255 } } },
    { { { -32, 47, 0 }, 0, { 0, 6016 }, { 255, 255, 255, 255 } } },
};

static void render_arcadekart_rpm_faceplate(s32 playerIdx, const ArcadeKartRpmMeterCanvas* canvas) {
    s32 meterRed = CM_GetProps()->Minimap.Colour.r;
    s32 meterGreen = CM_GetProps()->Minimap.Colour.g;
    s32 meterBlue = CM_GetProps()->Minimap.Colour.b;

    gSPClearGeometryMode(gDisplayListHead++, G_ZBUFFER);
    func_8004A2F4((s32) canvas->centerX, (s32) canvas->centerY, 0U, canvas->faceScale, meterRed, meterGreen,
                  meterBlue, 0xFF, common_texture_speedometer, speedometer_vtx, 64, 96, 64, 48);
}

static u16 get_arcadekart_rpm_needle_rotation(s32 playerIdx) {
    f32 rpm = kart_transmission_get_engine_rpm(&gPlayers[playerIdx], playerIdx);
    f32 motionMaxRpm = get_arcadekart_rpm_meter_motion_max(&gPlayers[playerIdx]);
    f32 rpmRatio = rpm / motionMaxRpm;

    if (rpmRatio < 0.0f) {
        rpmRatio = 0.0f;
    }
    if (rpmRatio > 1.0f) {
        rpmRatio = 1.0f;
    }
    return 0xDD00 + (u16) (rpmRatio * 0x1980);
}

static s32 get_arcadekart_rpm_shift_ready(s32 playerIdx) {
    f32 rpm = kart_transmission_get_engine_rpm(&gPlayers[playerIdx], playerIdx);
    return rpm >= kart_transmission_get_shift_ideal_rpm_max(&gPlayers[playerIdx]);
}

static s32 get_arcadekart_rpm_shift_over(s32 playerIdx) {
    f32 rpm = kart_transmission_get_engine_rpm(&gPlayers[playerIdx], playerIdx);
    return rpm >= kart_transmission_get_shift_over_rpm(&gPlayers[playerIdx]);
}

static void render_arcadekart_rpm_needle(s32 playerIdx, const ArcadeKartRpmMeterCanvas* canvas, s32 glow) {
    f32 needleX;
    f32 needleY;
    f32 needleScale = canvas->faceScale;
    u16 needleRotation = get_arcadekart_rpm_needle_rotation(playerIdx);

    if (glow) {
        if (!get_arcadekart_rpm_shift_ready(playerIdx)) {
            return;
        }
        needleScale = (get_arcadekart_rpm_shift_over(playerIdx) ? (((gGlobalTimer & 4) != 0) ? 0.92f : 0.86f)
                                                                : 0.84f) *
                      canvas->canvasScale;
    }

    arcadekart_rpm_meter_canvas_to_screen(canvas, ARCADEKART_RPM_METER_NEEDLE_OFFSET_X,
                                          ARCADEKART_RPM_METER_NEEDLE_OFFSET_Y, &needleX, &needleY);
    func_8004A258((s32) needleX, (s32) needleY, needleRotation, needleScale, common_texture_speedometer_needle,
                  D_0D005FF0, 0x40, 0x20, 0x40, 0x20);
}

static void render_arcadekart_rpm_gear_overlay(s32 playerIdx, const ArcadeKartRpmMeterCanvas* canvas) {
    const char* gearLabel = get_arcadekart_gear_label(playerIdx);
    f32 gearRightX;
    f32 gearBottomY;
    f32 textScale = ARCADEKART_RPM_METER_GEAR_TEXT_SCALE * canvas->canvasScale;
    f32 textHeight = 16.0f * textScale;
    f32 textWidth;
    s32 textLeftX;
    s32 textTopY;
    f32 gearInsetX =
        CVarGetFloat("gArcadeKart.Hud.RpmGearPanelInsetX", ARCADEKART_RPM_METER_GEAR_PANEL_INSET_X);
    f32 gearInsetY =
        CVarGetFloat("gArcadeKart.Hud.RpmGearPanelInsetY", ARCADEKART_RPM_METER_GEAR_PANEL_INSET_Y);

    arcadekart_rpm_meter_canvas_to_screen(canvas,
                                          ARCADEKART_RPM_METER_FACE_BOUNDS_RIGHT -
                                              ARCADEKART_RPM_METER_CENTER_X - gearInsetX,
                                          ARCADEKART_RPM_METER_FACE_BOUNDS_BOTTOM -
                                              ARCADEKART_RPM_METER_CENTER_Y - gearInsetY,
                                          &gearRightX, &gearBottomY);
    set_text_color(TEXT_YELLOW);
    textWidth = (f32) get_string_width((char*) gearLabel) * textScale;
    textLeftX = (s32) (gearRightX - textWidth);
    textTopY = (s32) (gearBottomY - textHeight);
    if (gearRightX >= (SCREEN_WIDTH / 2.0f)) {
        print_text_mode_2_wide_right(textLeftX, textTopY, (char*) gearLabel, 0, textScale, textScale);
    } else {
        text_draw(textLeftX, textTopY, (char*) gearLabel, 0, textScale, textScale);
    }
}

static void render_arcadekart_rpm_shift_text(s32 playerIdx, const ArcadeKartRpmMeterCanvas* canvas) {
    f32 shiftX;
    f32 shiftY;

    if (!get_arcadekart_rpm_shift_ready(playerIdx)) {
        return;
    }

    arcadekart_rpm_meter_canvas_to_screen(canvas, -17.0f, -31.0f, &shiftX, &shiftY);
    set_text_color(get_arcadekart_rpm_shift_over(playerIdx) ? (((gGlobalTimer & 4) != 0) ? TEXT_RED : TEXT_YELLOW)
                                                           : TEXT_RED);
    text_draw((s32) shiftX, (s32) shiftY, get_arcadekart_rpm_shift_over(playerIdx) ? "SHIFT!" : "SHIFT", 0,
              0.34f * canvas->faceScale, 0.34f * canvas->faceScale);
}

static void draw_arcadekart_rpm_meter_leaf(UNUSED const HudLayoutContext* ctx, UNUSED HudWidgetId widgetId, HudRect rect,
                                           void* userData) {
    ArcadeKartRpmMeterLeaf* leaf = (ArcadeKartRpmMeterLeaf*) userData;
    ArcadeKartRpmMeterCanvas canvas;

    if ((leaf == NULL) || (rect.w <= 0.0f) || (rect.h <= 0.0f)) {
        return;
    }

    canvas = get_arcadekart_rpm_meter_canvas(rect);
    switch (leaf->element) {
        case ARCADEKART_RPM_METER_ELEMENT_FACEPLATE:
            render_arcadekart_rpm_faceplate(leaf->playerIdx, &canvas);
            break;
        case ARCADEKART_RPM_METER_ELEMENT_NEEDLE_GLOW:
            render_arcadekart_rpm_needle(leaf->playerIdx, &canvas, true);
            break;
        case ARCADEKART_RPM_METER_ELEMENT_NEEDLE:
            render_arcadekart_rpm_needle(leaf->playerIdx, &canvas, false);
            break;
        case ARCADEKART_RPM_METER_ELEMENT_GEAR:
            render_arcadekart_rpm_gear_overlay(leaf->playerIdx, &canvas);
            break;
        case ARCADEKART_RPM_METER_ELEMENT_SHIFT_TEXT:
            render_arcadekart_rpm_shift_text(leaf->playerIdx, &canvas);
            break;
        case ARCADEKART_RPM_METER_ELEMENT_DIGITAL:
            render_digital_speedometer_at(leaf->playerIdx, canvas.centerX, canvas.centerY, canvas.canvasScale);
            break;
    }
}

static f32 get_arcadekart_minimap_right_edge(s32 playerIdx, HudRect viewRect, f32 fallbackPaddingX) {
    s32 minimapPlayerIdx = playerIdx;
    f32 rightEdge;
    f32 maxRightEdge;

    if (minimapPlayerIdx < 0) {
        minimapPlayerIdx = 0;
    }
    if (minimapPlayerIdx > 1) {
        minimapPlayerIdx = 0;
    }

    rightEdge =
        (f32) CM_GetProps()->Minimap.Pos[minimapPlayerIdx].X + ((f32) CM_GetProps()->Minimap.Width * 0.5f);
    maxRightEdge = viewRect.x + viewRect.w - fallbackPaddingX;
    if (rightEdge <= viewRect.x) {
        rightEdge = maxRightEdge;
    }
    if (rightEdge > maxRightEdge) {
        rightEdge = maxRightEdge;
    }
    return rightEdge;
}

static void render_arcadekart_rpm_meter_layout(s32 playerIdx, ArcadeKartRpmMeterPass pass) {
    HudLayoutContext layout;
    ArcadeKartRpmMeterLeaf leaves[5];
    HudWidgetId root;
    HudWidgetId scaleBox;
    HudWidgetId sizeBox;
    HudWidgetId meterCanvas;
    HudWidgetId meterPart;
    HudRect viewRect = get_arcadekart_hud_player_view_rect(playerIdx);
    f32 meterScale = get_arcadekart_hud_cluster_scale(viewRect, "gArcadeKart.Hud.RpmMeterScale", 0.9f);
    s32 leafCount = 0;

    hud_layout_begin_safe_zone(&layout, viewRect, get_arcadekart_hud_safe_zone_padding(viewRect));
    root = hud_layout_root(&layout);
    scaleBox = hud_layout_scale_box(&layout, HUD_SCALE_STRETCH_USER, HUD_SCALE_DIRECTION_BOTH, meterScale);
    sizeBox = hud_layout_size_box(&layout, ARCADEKART_RPM_METER_WIDGET_WIDTH, ARCADEKART_RPM_METER_WIDGET_HEIGHT,
                                  -1.0f, -1.0f, -1.0f, -1.0f,
                                  hud_layout_padding(0.0f, 0.0f, 0.0f, 0.0f), hud_layout_vec2(0.5f, 0.5f));
    meterCanvas = hud_layout_canvas(&layout);
    hud_layout_single_child_add(&layout, sizeBox, meterCanvas,
                                hud_layout_single_child_slot(hud_layout_padding(0.0f, 0.0f, 0.0f, 0.0f),
                                                             hud_layout_vec2(0.5f, 0.5f), true, true));
    hud_layout_single_child_add(&layout, scaleBox, sizeBox,
                                hud_layout_single_child_slot(hud_layout_padding(0.0f, 0.0f, 0.0f, 0.0f),
                                                             hud_layout_vec2(0.5f, 0.5f), false, false));

    if (pass == ARCADEKART_RPM_METER_PASS_DIGITAL) {
        leaves[leafCount].playerIdx = playerIdx;
        leaves[leafCount].element = ARCADEKART_RPM_METER_ELEMENT_DIGITAL;
        meterPart = hud_layout_draw(&layout, hud_layout_vec2(ARCADEKART_RPM_METER_WIDGET_WIDTH,
                                                             ARCADEKART_RPM_METER_WIDGET_HEIGHT),
                                    draw_arcadekart_rpm_meter_leaf, &leaves[leafCount++]);
        hud_layout_canvas_add(&layout, meterCanvas, meterPart,
                              hud_layout_canvas_slot(hud_layout_anchor(0.0f, 0.0f, 1.0f, 1.0f),
                                                     hud_layout_padding(0.0f, 0.0f, 0.0f, 0.0f),
                                                     hud_layout_vec2(0.0f, 0.0f), false, 0));
    } else {
        const ArcadeKartRpmMeterElement elements[] = {
            ARCADEKART_RPM_METER_ELEMENT_FACEPLATE, ARCADEKART_RPM_METER_ELEMENT_NEEDLE_GLOW,
            ARCADEKART_RPM_METER_ELEMENT_NEEDLE,    ARCADEKART_RPM_METER_ELEMENT_GEAR,
            ARCADEKART_RPM_METER_ELEMENT_SHIFT_TEXT,
        };
        s32 i;

        for (i = 0; i < (s32) (sizeof(elements) / sizeof(elements[0])); i++) {
            leaves[leafCount].playerIdx = playerIdx;
            leaves[leafCount].element = elements[i];
            meterPart = hud_layout_draw(&layout, hud_layout_vec2(ARCADEKART_RPM_METER_WIDGET_WIDTH,
                                                                 ARCADEKART_RPM_METER_WIDGET_HEIGHT),
                                        draw_arcadekart_rpm_meter_leaf, &leaves[leafCount]);
            hud_layout_canvas_add(&layout, meterCanvas, meterPart,
                                  hud_layout_canvas_slot(hud_layout_anchor(0.0f, 0.0f, 1.0f, 1.0f),
                                                         hud_layout_padding(0.0f, 0.0f, 0.0f, 0.0f),
                                                         hud_layout_vec2(0.0f, 0.0f), false, i));
            leafCount++;
        }
    }

    hud_layout_canvas_add(&layout, root, scaleBox,
                          hud_layout_canvas_safe_slot(hud_layout_anchor(1.0f, 1.0f, 1.0f, 1.0f),
                                                      hud_layout_padding(0.0f, 0.0f, 0.0f, 0.0f),
                                                      hud_layout_vec2(1.0f, 1.0f), true, 0));
    hud_layout_arrange(&layout);
    hud_layout_draw_tree(&layout);
}

void render_digital_speedometer(s32 playerIdx) {
    render_arcadekart_rpm_meter_layout(playerIdx, ARCADEKART_RPM_METER_PASS_DIGITAL);
}

// render the speedometer for the player
void render_speedometer(s32 playerIdx) {
    render_arcadekart_rpm_meter_layout(playerIdx, ARCADEKART_RPM_METER_PASS_DIAL);
}

static const char* const* get_arcadekart_feedback_glyph(char letter) {
    static const char* const glyphG[] = { "01111", "10000", "10000", "10111", "10001", "10001", "01111" };
    static const char* const glyphR[] = { "11110", "10001", "10001", "11110", "10100", "10010", "10001" };
    static const char* const glyphI[] = { "11111", "00100", "00100", "00100", "00100", "00100", "11111" };
    static const char* const glyphN[] = { "10001", "11001", "10101", "10011", "10001", "10001", "10001" };
    static const char* const glyphD[] = { "11110", "10001", "10001", "10001", "10001", "10001", "11110" };

    switch (letter) {
        case 'G':
            return glyphG;
        case 'R':
            return glyphR;
        case 'I':
            return glyphI;
        case 'N':
            return glyphN;
        case 'D':
            return glyphD;
        default:
            return glyphI;
    }
}

static void draw_arcadekart_feedback_glyph(s32 x, s32 y, char letter, s32 cellSize, s32 red, s32 green, s32 blue) {
    const char* const* glyph = get_arcadekart_feedback_glyph(letter);
    s32 row;
    s32 column;

    for (row = 0; row < 7; row++) {
        for (column = 0; column < 5; column++) {
            if (glyph[row][column] != '0') {
                s32 left = x + (column * cellSize);
                s32 top = y + (row * cellSize);
                gDisplayListHead = draw_box_fill(gDisplayListHead, left, top, left + cellSize - 1,
                                                 top + cellSize - 1, red, green, blue, 255);
            }
        }
    }
}

static void draw_arcadekart_grind_feedback(s16 timer) {
    const char* text = "GRIND";
    s32 cellSize = (timer > 30) ? 3 : 2;
    s32 letterWidth = 5 * cellSize;
    s32 letterSpacing = cellSize;
    s32 textWidth = (5 * letterWidth) + (4 * letterSpacing);
    s32 x = (SCREEN_WIDTH - textWidth) / 2;
    s32 y = 126;
    s32 i;

    for (i = 0; i < 5; i++) {
        s32 letterX = x + (i * (letterWidth + letterSpacing));
        draw_arcadekart_feedback_glyph(letterX + cellSize, y + cellSize, text[i], cellSize, 0, 0, 0);
    }
    for (i = 0; i < 5; i++) {
        s32 letterX = x + (i * (letterWidth + letterSpacing));
        draw_arcadekart_feedback_glyph(letterX, y, text[i], cellSize, 255, 255, 255);
    }
}

void render_shift_feedback_hud(s32 playerIdx) {
    KartShiftFeedback feedback = kart_transmission_get_shift_feedback(playerIdx);
    s16 timer = kart_transmission_get_shift_feedback_timer(playerIdx);
    f32 scale = 0.75f;

    if ((feedback == KART_SHIFT_FEEDBACK_NONE) || (timer <= 0)) {
        return;
    }

    if (timer > 30) {
        scale = 0.90f;
    }

    if (feedback == KART_SHIFT_FEEDBACK_GOOD) {
        set_text_color(TEXT_YELLOW);
        text_draw_wide(124, 132, "GOOD SHIFT", 0, scale, scale);
    } else if (feedback == KART_SHIFT_FEEDBACK_BAD) {
        draw_arcadekart_grind_feedback(timer);
    }
}

// player is only 0 or 1
static f32 get_arcadekart_minimap_cluster_scale(s32 playerId) {
    HudRect viewRect = get_arcadekart_hud_player_view_rect(playerId);
    return get_arcadekart_hud_cluster_scale(viewRect, "gArcadeKart.Hud.MinimapScale", 1.0f);
}

static f32 get_arcadekart_minimap_center_x(s32 playerId) {
    if (gPlayerCount == 3) {
        return ((OTRGetDimensionFromRightEdge(SCREEN_WIDTH) - SCREEN_WIDTH) / 2) +
               ((SCREEN_WIDTH / 4) + (SCREEN_WIDTH / 2));
    }
    return (f32) CM_GetProps()->Minimap.Pos[playerId].X;
}

static void scale_arcadekart_minimap_point(s32 playerId, f32* x, f32* y) {
    f32 scale = get_arcadekart_minimap_cluster_scale(playerId);
    f32 centerX = get_arcadekart_minimap_center_x(playerId);
    f32 centerY = (f32) CM_GetProps()->Minimap.Pos[playerId].Y;

    *x = centerX + ((*x - centerX) * scale);
    *y = centerY + ((*y - centerY) * scale);
}

void func_8004EE54(s32 playerId) {
    f32 scale = get_arcadekart_minimap_cluster_scale(playerId);
    s32 centerX = CM_GetProps()->Minimap.Pos[playerId].X;
    s32 centerY = CM_GetProps()->Minimap.Pos[playerId].Y;
    s32 destWidth = get_arcadekart_scaled_size((f32) CM_GetProps()->Minimap.Width * scale);
    s32 destHeight = get_arcadekart_scaled_size((f32) CM_GetProps()->Minimap.Height * scale);

    gSPDisplayList(gDisplayListHead++, D_0D007FE0);
    func_8004B414(CM_GetProps()->Minimap.Colour.r, CM_GetProps()->Minimap.Colour.g, CM_GetProps()->Minimap.Colour.b,
                  0xFF);
    if (gIsMirrorMode != 0) {
        func_800450C8((u8*) D_8018D240, CM_GetProps()->Minimap.Width, CM_GetProps()->Minimap.Height);
    } else {
        func_80044F34((u8*) D_8018D240, CM_GetProps()->Minimap.Width, CM_GetProps()->Minimap.Height);
    }
    render_texture_rectangle_wide_scaled(centerX - (destWidth / 2), centerY - (destHeight / 2), destWidth, destHeight,
                                         CM_GetProps()->Minimap.Width, CM_GetProps()->Minimap.Height, 0, 0, false, 1);
}

void func_8004EF9C(s32 arg0) {
    s16 height;
    s16 width;
    const char* minimap = TrackBrowser_GetMinimapTextureByIdx(arg0);

    if (NULL == minimap) {
        return;
    }

    width = ResourceGetTexWidthByName(minimap);
    height = ResourceGetTexHeightByName(minimap);
    func_8004D37C(0x00000104, 0x0000003C, minimap, 0x000000FF, 0x000000FF,
                  0x000000FF, 0x000000FF, width, height, width, height);
}

void set_minimap_finishline_position(s32 playerId) {
    f32 var_f0;
    f32 var_f2;
    s32 center = 0;

    //! @todo: Hardcode these x and y values. Because why not?

    if (gPlayerCount == 3) {
        center = ((OTRGetDimensionFromRightEdge(SCREEN_WIDTH) - SCREEN_WIDTH) / 2) +
                 ((SCREEN_WIDTH / 4) + (SCREEN_WIDTH / 2));
    } else {
        center = CM_GetProps()->Minimap.Pos[playerId].X;
    }

    // minimap center pos -  minimap left edge  +  offset
    var_f2 = (center - (CM_GetProps()->Minimap.Width / 2)) +
             CM_GetProps()->Minimap.PlayerX; // (center - (gMinimapWidth / 2)) + gMinimapPlayerX;
    var_f0 = (CM_GetProps()->Minimap.Pos[playerId].Y - (CM_GetProps()->Minimap.Height / 2)) +
             CM_GetProps()->Minimap.PlayerY; // (gMinimapY[arg0] - (gMinimapHeight / 2)) + gMinimapPlayerY

    var_f2 += CM_GetProps()->Minimap.FinishlineX;
    var_f0 += CM_GetProps()->Minimap.FinishlineY;
    scale_arcadekart_minimap_point(playerId, &var_f2, &var_f0);

    draw_hud_2d_texture_wide_scaled((s32) var_f2, (s32) var_f0, 8, 8, get_arcadekart_minimap_cluster_scale(playerId),
                                    (u8*) common_texture_minimap_finish_line, 1);
}

char* common_texture_minimap_progress[] = {
    common_texture_minimap_mario, common_texture_minimap_luigi,  common_texture_minimap_yoshi,
    common_texture_minimap_toad,  common_texture_minimap_dk,     common_texture_minimap_wario,
    common_texture_minimap_peach, common_texture_minimap_bowser,
};

typedef struct MinimapCharacterDotColor {
    u8 red;
    u8 green;
    u8 blue;
} MinimapCharacterDotColor;

static const MinimapCharacterDotColor sMinimapCharacterDotColors[] = {
    { 0xE8, 0x20, 0x18 }, // Mario
    { 0x20, 0xB8, 0x38 }, // Luigi
    { 0x30, 0xC8, 0x68 }, // Yoshi
    { 0x38, 0x90, 0xFF }, // Toad
    { 0x9C, 0x60, 0x20 }, // D.K.
    { 0xF0, 0xD8, 0x20 }, // Wario
    { 0xFF, 0x78, 0xB8 }, // Peach
    { 0xF0, 0x80, 0x20 }, // Bowser
};

static const MinimapCharacterDotColor* get_minimap_character_dot_color(s32 characterId) {
    if ((characterId < 0) ||
        (characterId >= (s32) (sizeof(sMinimapCharacterDotColors) / sizeof(sMinimapCharacterDotColors[0])))) {
        characterId = 0;
    }
    return &sMinimapCharacterDotColors[characterId];
}

static void draw_minimap_character_dot(s32 x, s32 y, s32 red, s32 green, s32 blue) {
    gSPDisplayList(gDisplayListHead++, D_0D007F38);
    func_8004B614(red, green, blue, 0x80, 0x80, 0x80, 0xFF);
    load_texture_block_rgba16_mirror((u8*) common_texture_minimap_progress_dot, 8, 8);
    func_8004B97C_wide(x - 4, y - 4, 8, 8, 1);
    gSPDisplayList(gDisplayListHead++, D_0D007EB8);
}

static void draw_minimap_character_dot_scaled(s32 x, s32 y, f32 scale, s32 red, s32 green, s32 blue) {
    s32 destWidth = get_arcadekart_scaled_size(8.0f * scale);
    s32 destHeight = get_arcadekart_scaled_size(8.0f * scale);

    gSPDisplayList(gDisplayListHead++, D_0D007F38);
    func_8004B614(red, green, blue, 0x80, 0x80, 0x80, 0xFF);
    load_texture_block_rgba16_mirror((u8*) common_texture_minimap_progress_dot, 8, 8);
    render_texture_rectangle_wide_scaled(x - (destWidth / 2), y - (destHeight / 2), destWidth, destHeight, 8, 8, 0, 0,
                                         false, 1);
    gSPDisplayList(gDisplayListHead++, D_0D007EB8);
}

#ifdef NON_MATCHING
// https://decomp.me/scratch/FxA1w
/**
 * characterId of 8 appears to be a type of null check or control flow alteration.
 */
void draw_minimap_character(s32 arg0, s32 playerId, s32 characterId) {
    f32 thing0;
    f32 thing1;
    s16 x;
    s16 y;
    s32 center = 0;
    Player* player = &gPlayerOne[playerId];
    s32 dotCharacterId;
    const MinimapCharacterDotColor* dotColor;

    if (player->type & (1 << 15)) {
        thing0 = player->pos[0] * CM_GetProps()->Minimap.PlayerScaleFactor; // gMinimapPlayerScale;
        thing1 = player->pos[2] * CM_GetProps()->Minimap.PlayerScaleFactor; // gMinimapPlayerScale;

        center = (s32) get_arcadekart_minimap_center_x(arg0);

        x = (center - (CM_GetProps()->Minimap.Width / 2)) + CM_GetProps()->Minimap.PlayerX + (s16) (thing0);
        y = (CM_GetProps()->Minimap.Pos[arg0].Y - (CM_GetProps()->Minimap.Height / 2)) +
            CM_GetProps()->Minimap.PlayerY + (s16) (thing1);
        {
            f32 scaledX = (f32) x;
            f32 scaledY = (f32) y;
            scale_arcadekart_minimap_point(arg0, &scaledX, &scaledY);
            x = (s16) scaledX;
            y = (s16) scaledY;
        }

        dotCharacterId = (characterId == 8) ? player->characterId : characterId;
        dotColor = get_minimap_character_dot_color(dotCharacterId);
        FrameInterpolation_RecordOpenChild("minimap_dots", TAG_MINIMAP_DOTS(((arg0 & 0x1) << 6) |
                                                                            ((playerId & 0x7) << 3) |
                                                                            (dotCharacterId & 0x7)));
        if ((gGPCurrentRaceRankByPlayerId[playerId] == 0) && (gModeSelection != BATTLE) &&
            (gModeSelection != TIME_TRIALS)) {
            func_8004C450_scaled(x, y, 8, 8, get_arcadekart_minimap_cluster_scale(arg0),
                                 (u8*) common_texture_minimap_progress_dot);
        } else {
            draw_minimap_character_dot_scaled(x, y, get_arcadekart_minimap_cluster_scale(arg0), dotColor->red,
                                              dotColor->green, dotColor->blue);
        }
        FrameInterpolation_RecordCloseChild();
    }
}
#else
GLOBAL_ASM("asm/non_matchings/render_objects/draw_minimap_character.s")
#endif

// WTF is up with the gPlayerOne access in this function?
void func_8004F3E4(s32 arg0) {
    UNUSED Player* player;
    s32 playerId;
    s32 idx;

    switch (gModeSelection) { /* irregular */
        case GRAND_PRIX:
            for (idx = D_8018D158 - 1; idx >= 0; idx--) {
                playerId = gGPCurrentRacePlayerIdByRank[idx];
                if ((gPlayerOne + playerId)->type & PLAYER_CPU) {
                    draw_minimap_character(arg0, playerId, 8);
                }
            }
            for (idx = D_8018D158 - 1; idx >= 0; idx--) {
                playerId = gGPCurrentRacePlayerIdByRank[idx];
                if (((gPlayerOne + playerId)->type & PLAYER_CPU) != PLAYER_CPU) {
                    draw_minimap_character(arg0, playerId, (gPlayerOne + playerId)->characterId);
                }
            }
            break;
        case TIME_TRIALS:
            for (idx = 0; idx < 8; idx++) {
                if (((gPlayerOne + idx)->type & PLAYER_INVISIBLE_OR_BOMB) == PLAYER_INVISIBLE_OR_BOMB) {
                    draw_minimap_character(arg0, idx, 8);
                }
            }
            draw_minimap_character(arg0, 0, gPlayerOne->characterId);
            break;
        case VERSUS:
            for (idx = gPlayerCountSelection1 - 1; idx >= 0; idx--) {
                playerId = gGPCurrentRacePlayerIdByRank[idx];
                draw_minimap_character(arg0, playerId, (gPlayerOne + playerId)->characterId);
            }
            break;
        case BATTLE:
            for (idx = 0; idx < gPlayerCountSelection1; idx++) {
                if (!((gPlayerOne + idx)->type & PLAYER_UNKNOWN_0x40)) {
                    draw_minimap_character(arg0, idx, (gPlayerOne + idx)->characterId);
                }
            }
            break;
    }
}

s32 func_8004F674(s32* arg0, s32 arg1) {
    s32 temp_v0;
    s32 ret;

    temp_v0 = *arg0;
    if (temp_v0 != 0) {
        ret = temp_v0 / arg1;
        *arg0 = temp_v0 % arg1;
    } else {
        *arg0 = 0;
        ret = 0;
    }
    return ret;
}

void func_8004F6D0(s32 arg0) {
    UNUSED s32 stackPadding0;
    UNUSED s32 stackPadding1;
    s32 sp24;

    sp24 = arg0;
    if (arg0 >= 599999) {
        sp24 = 599999;
    }
    D_801657D0[0] = func_8004F674(&sp24, 60000);
    D_801657D0[1] = func_8004F674(&sp24, 6000);
    D_801657D0[3] = func_8004F674(&sp24, 1000);
    D_801657D0[4] = func_8004F674(&sp24, 100);
    D_801657D0[6] = func_8004F674(&sp24, 10);
    D_801657D0[7] = sp24;
    D_801657D0[2] = 10;
    D_801657D0[5] = 11;
}

void func_8004F774(s32 arg0, s32 arg1) {
    s32 i;
    s32 phi_s1 = arg0;

    for (i = 0; i < 8; i++) {
        func_8004BA98(phi_s1, arg1, 8, 16, D_801657D0[i] * 8, 0, 0);
        phi_s1 += 8;
    }
}

void print_timer(s32 arg0, s32 arg1, s32 arg2) {
    gSPDisplayList(gDisplayListHead++, D_0D008108);
    gSPDisplayList(gDisplayListHead++, D_0D007EF8);
    gDPSetAlphaCompare(gDisplayListHead++, G_AC_THRESHOLD);
    load_texture_block_rgba16_mirror((u8*) common_texture_hud_normal_digit, 104, 16);
    func_8004F6D0(arg2);
    func_8004F774(arg0, arg1);
    gSPDisplayList(gDisplayListHead++, D_0D007EB8);
}

void func_8004F8CC(s32 arg0, s32 arg1) {
    s32 phi_s1 = arg0;
    s32 i;

    for (i = 0; i < 8; i++) {
        func_8004BA98(phi_s1, arg1, 8, 16, D_801657D0[i] * 8, 0, 1);
        phi_s1 += 8;
    }
}

void func_8004F950(s32 arg0, s32 arg1, s32 arg2, s32 arg3) {
    gSPDisplayList(gDisplayListHead++, D_0D007F38);
    set_transparency(arg2);
    load_texture_block_rgba16_mirror((u8*) common_texture_hud_normal_digit, 104, 16);
    func_8004F6D0(arg3);
    func_8004F8CC(arg0, arg1);
}

void print_timer_rainbow(s32 arg0, s32 arg1, s32 arg2) {
    gSPDisplayList(gDisplayListHead++, D_0D007F38);
    func_8004B614(D_801656C0, D_801656D0, D_801656E0, 128, 128, 128, 255);
    load_texture_block_rgba16_mirror((u8*) common_texture_hud_normal_digit, 104, 16);
    func_8004F6D0(arg2);
    func_8004F8CC(arg0, arg1);
}

static void format_arcadekart_timer(char* buffer, size_t bufferSize, s32 timerValue) {
    s32 minutes;
    s32 seconds;
    s32 centiseconds;

    if (timerValue < 0) {
        timerValue = 0;
    }

    minutes = timerValue / 6000;
    seconds = (timerValue / 100) % 60;
    centiseconds = timerValue % 100;
    snprintf(buffer, bufferSize, "%d'%02d\"%02d", minutes, seconds, centiseconds);
}

static f32 get_arcadekart_text_width(const char* text, f32 scale) {
    return (f32) strlen(text) * 12.0f * scale;
}

static HudVec2 get_arcadekart_text_desired_size(const char* text, f32 scale) {
    return hud_layout_vec2(get_arcadekart_text_width(text, scale), 16.0f * scale);
}

typedef struct ArcadeKartHudTextLeaf {
    const char* text;
    f32 scale;
    s32 color;
    s32 useWideText;
} ArcadeKartHudTextLeaf;

static f32 get_arcadekart_rect_scale(HudRect rect, f32 baseWidth, f32 baseHeight) {
    f32 scaleX = (baseWidth > 0.0f) ? (rect.w / baseWidth) : 1.0f;
    f32 scaleY = (baseHeight > 0.0f) ? (rect.h / baseHeight) : 1.0f;
    return (scaleX < scaleY) ? scaleX : scaleY;
}

static void draw_arcadekart_hud_text_leaf(UNUSED const HudLayoutContext* ctx, UNUSED HudWidgetId widgetId,
                                          HudRect rect, void* userData) {
    ArcadeKartHudTextLeaf* leaf = (ArcadeKartHudTextLeaf*) userData;

    if ((leaf == NULL) || (leaf->text == NULL)) {
        return;
    }

    set_text_color(leaf->color);
    if (leaf->useWideText != 0) {
        text_draw_wide((s32) rect.x, (s32) rect.y, (char*) leaf->text, 0, leaf->scale, leaf->scale);
    } else {
        text_draw((s32) rect.x, (s32) rect.y, (char*) leaf->text, 0, leaf->scale, leaf->scale);
    }
}

typedef struct ArcadeKartHudTextureLeaf {
    u8* texture;
    s32 width;
    s32 height;
} ArcadeKartHudTextureLeaf;

typedef struct ArcadeKartHudTimerGlyphLeaf {
    s32 glyphIndex;
} ArcadeKartHudTimerGlyphLeaf;

typedef struct ArcadeKartHudTimerValueLeaf {
    s32 timerValue;
} ArcadeKartHudTimerValueLeaf;

static void draw_arcadekart_hud_texture_leaf(UNUSED const HudLayoutContext* ctx, UNUSED HudWidgetId widgetId,
                                             HudRect rect, void* userData) {
    ArcadeKartHudTextureLeaf* leaf = (ArcadeKartHudTextureLeaf*) userData;
    f32 scale;

    if ((leaf == NULL) || (leaf->texture == NULL)) {
        return;
    }

    scale = get_arcadekart_rect_scale(rect, (f32) leaf->width, (f32) leaf->height);
    draw_hud_2d_texture_scaled((s32) (rect.x + (rect.w * 0.5f)), (s32) (rect.y + (rect.h * 0.5f)), leaf->width,
                               leaf->height, scale, leaf->texture);
}

static void draw_arcadekart_hud_timer_glyph_leaf(UNUSED const HudLayoutContext* ctx, UNUSED HudWidgetId widgetId,
                                                 HudRect rect, void* userData) {
    ArcadeKartHudTimerGlyphLeaf* leaf = (ArcadeKartHudTimerGlyphLeaf*) userData;
    f32 scale;
    s32 destWidth;
    s32 destHeight;

    if (leaf == NULL) {
        return;
    }

    scale = get_arcadekart_rect_scale(rect, 8.0f, 16.0f);
    destWidth = get_arcadekart_scaled_size(8.0f * scale);
    destHeight = get_arcadekart_scaled_size(16.0f * scale);
    gSPDisplayList(gDisplayListHead++, D_0D008108);
    gSPDisplayList(gDisplayListHead++, D_0D007EF8);
    gDPSetAlphaCompare(gDisplayListHead++, G_AC_THRESHOLD);
    load_texture_block_rgba16_mirror((u8*) common_texture_hud_normal_digit, 104, 16);
    render_texture_rectangle_scaled((s32) rect.x, (s32) rect.y, destWidth, destHeight, 8, 16, leaf->glyphIndex * 8,
                                    0);
    gSPDisplayList(gDisplayListHead++, D_0D007EB8);
}

static void draw_arcadekart_timer_value_scaled(s32 x, s32 y, s32 timerValue, f32 scale) {
    s32 destWidth = get_arcadekart_scaled_size(8.0f * scale);
    s32 destHeight = get_arcadekart_scaled_size(16.0f * scale);
    s32 cursorX = x;
    s32 digitIndex;

    gSPDisplayList(gDisplayListHead++, D_0D008108);
    gSPDisplayList(gDisplayListHead++, D_0D007EF8);
    gDPSetAlphaCompare(gDisplayListHead++, G_AC_THRESHOLD);
    load_texture_block_rgba16_mirror((u8*) common_texture_hud_normal_digit, 104, 16);
    func_8004F6D0(timerValue);
    for (digitIndex = 0; digitIndex < 8; digitIndex++) {
        render_texture_rectangle_scaled(cursorX, y, destWidth, destHeight, 8, 16, D_801657D0[digitIndex] * 8, 0);
        cursorX += destWidth;
    }
    gSPDisplayList(gDisplayListHead++, D_0D007EB8);
}

static void draw_arcadekart_hud_timer_value_leaf(UNUSED const HudLayoutContext* ctx, UNUSED HudWidgetId widgetId,
                                                 HudRect rect, void* userData) {
    ArcadeKartHudTimerValueLeaf* leaf = (ArcadeKartHudTimerValueLeaf*) userData;
    f32 scale;

    if (leaf == NULL) {
        return;
    }

    scale = get_arcadekart_rect_scale(rect, 64.0f, 16.0f);
    draw_arcadekart_timer_value_scaled((s32) rect.x, (s32) rect.y, leaf->timerValue, scale);
}

static void render_arcadekart_timer_strip(s32 playerId) {
    s32 timerValue = playerHUD[playerId].someTimer;
    ArcadeKartHudTextureLeaf timeLabelLeaf;
    ArcadeKartHudTextureLeaf lapLabelLeaf;
    ArcadeKartHudTimerGlyphLeaf timerGlyphLeaves[8];
    ArcadeKartHudTextureLeaf lapCounterLeaf;
    ArcadeKartHudTimerValueLeaf splitTimerLeaves[3];
    HudLayoutContext layout;
    HudWidgetId root;
    HudWidgetId topBox;
    HudWidgetId topScaleBox;
    HudWidgetId timerBox;
    HudWidgetId splitBox;
    HudWidgetId splitScaleBox;
    HudWidgetId widget;
    s32 lapIndex;
    s32 digitIndex;
    HudRect viewRect = get_arcadekart_hud_player_view_rect(playerId);
    f32 topScale = get_arcadekart_hud_cluster_scale(viewRect, "gArcadeKart.Hud.TimeLapScale", 0.95f);
    f32 splitScale = get_arcadekart_hud_cluster_scale(viewRect, "gArcadeKart.Hud.LapTimesScale", 0.9f);
    const f32 topX = CVarGetFloat("gArcadeKart.Hud.TopXOffset", 0.0f);
    const f32 topY = CVarGetFloat("gArcadeKart.Hud.TopY", 15.0f);
    const f32 splitTopPadding = 10.0f;

    if ((playerHUD[playerId].blinkTimer != 0) && (playerHUD[playerId].blinkState == 0)) {
        timerValue = playerHUD[playerId].someTimer1;
    }

    hud_layout_begin_safe_zone(&layout, viewRect, get_arcadekart_hud_safe_zone_padding(viewRect));
    root = hud_layout_root(&layout);

    timeLabelLeaf.texture = (u8*) common_texture_hud_time;
    timeLabelLeaf.width = 32;
    timeLabelLeaf.height = 16;
    lapLabelLeaf.texture = (u8*) common_texture_hud_lap;
    lapLabelLeaf.width = 32;
    lapLabelLeaf.height = 8;
    lapCounterLeaf.texture = (u8*) gHudLapTextures[playerHUD[playerId].alsoLapCount];
    lapCounterLeaf.width = 32;
    lapCounterLeaf.height = 16;
    func_8004F6D0(timerValue);
    for (digitIndex = 0; digitIndex < 8; digitIndex++) {
        timerGlyphLeaves[digitIndex].glyphIndex = D_801657D0[digitIndex];
    }

    topBox = hud_layout_horizontal_box(&layout, 3.0f * topScale, hud_layout_padding(0.0f, 0.0f, 0.0f, 0.0f));
    topScaleBox = hud_layout_scale_box(&layout, HUD_SCALE_STRETCH_USER, HUD_SCALE_DIRECTION_BOTH, 1.0f);
    widget = hud_layout_draw(&layout, hud_layout_vec2(32.0f * topScale, 16.0f * topScale), draw_arcadekart_hud_texture_leaf,
                             &timeLabelLeaf);
    hud_layout_box_add(&layout, topBox, widget,
                       hud_layout_auto_slot(hud_layout_padding(0.0f, 0.0f, 0.0f, 0.0f),
                                            hud_layout_vec2(0.0f, 0.5f), false));

    timerBox = hud_layout_horizontal_box(&layout, 0.0f, hud_layout_padding(0.0f, 0.0f, 0.0f, 0.0f));
    for (digitIndex = 0; digitIndex < 8; digitIndex++) {
        widget = hud_layout_draw(&layout, hud_layout_vec2(8.0f * topScale, 16.0f * topScale), draw_arcadekart_hud_timer_glyph_leaf,
                                 &timerGlyphLeaves[digitIndex]);
        hud_layout_box_add(&layout, timerBox, widget,
                           hud_layout_auto_slot(hud_layout_padding(0.0f, 0.0f, 0.0f, 0.0f),
                                                hud_layout_vec2(0.0f, 0.5f), false));
    }
    hud_layout_box_add(&layout, topBox, timerBox,
                       hud_layout_auto_slot(hud_layout_padding(0.0f, 0.0f, 8.0f * topScale, 0.0f),
                                            hud_layout_vec2(0.0f, 0.5f), false));
    widget = hud_layout_draw(&layout, hud_layout_vec2(32.0f * topScale, 8.0f * topScale), draw_arcadekart_hud_texture_leaf, &lapLabelLeaf);
    hud_layout_box_add(&layout, topBox, widget,
                       hud_layout_auto_slot(hud_layout_padding(0.0f, 0.0f, 0.0f, 0.0f),
                                            hud_layout_vec2(0.0f, 0.5f), false));
    widget = hud_layout_draw(&layout, hud_layout_vec2(32.0f * topScale, 16.0f * topScale), draw_arcadekart_hud_texture_leaf,
                             &lapCounterLeaf);
    hud_layout_box_add(&layout, topBox, widget,
                       hud_layout_auto_slot(hud_layout_padding(-2.0f * topScale, 0.0f, 0.0f, 0.0f),
                                            hud_layout_vec2(0.0f, 0.5f), false));

    hud_layout_single_child_add(&layout, topScaleBox, topBox,
                                hud_layout_single_child_slot(hud_layout_padding(0.0f, 0.0f, 0.0f, 0.0f),
                                                             hud_layout_vec2(0.5f, 0.0f), false, false));
    hud_layout_canvas_add(&layout, root, topScaleBox,
                          hud_layout_canvas_safe_slot(hud_layout_anchor(0.5f, 0.0f, 0.5f, 0.0f),
                                                      hud_layout_padding(get_arcadekart_hud_view_x(viewRect, topX),
                                                                         get_arcadekart_hud_view_y(viewRect, topY),
                                                                         0.0f, 0.0f),
                                                      hud_layout_vec2(0.5f, 0.0f), true, 0));

    splitBox = hud_layout_vertical_box(&layout, 0.0f, hud_layout_padding(0.0f, 0.0f, 0.0f, 0.0f));
    splitScaleBox = hud_layout_scale_box(&layout, HUD_SCALE_STRETCH_USER, HUD_SCALE_DIRECTION_BOTH, 1.0f);
    for (lapIndex = 0; lapIndex < 3; lapIndex++) {
        s32 lapDuration = playerHUD[playerId].lapDurations[lapIndex];

        if (lapDuration < 0) {
            lapDuration = 0;
        }

        splitTimerLeaves[lapIndex].timerValue = lapDuration;
        widget = hud_layout_draw(&layout, hud_layout_vec2(64.0f * splitScale, 16.0f * splitScale), draw_arcadekart_hud_timer_value_leaf,
                                 &splitTimerLeaves[lapIndex]);
        hud_layout_box_add(&layout, splitBox, widget,
                           hud_layout_auto_slot(hud_layout_padding(0.0f, 0.0f, 0.0f, 0.0f),
                                                hud_layout_vec2(1.0f, 0.0f), false));
    }

    hud_layout_single_child_add(&layout, splitScaleBox, splitBox,
                                hud_layout_single_child_slot(hud_layout_padding(0.0f, 0.0f, 0.0f, 0.0f),
                                                             hud_layout_vec2(1.0f, 0.0f), false, false));
    hud_layout_canvas_add(&layout, root, splitScaleBox,
                          hud_layout_canvas_safe_slot(hud_layout_anchor(1.0f, 0.0f, 1.0f, 0.0f),
                                                      hud_layout_padding(
                                                          0.0f, get_arcadekart_hud_view_y(viewRect, splitTopPadding),
                                                          0.0f, 0.0f),
                                                      hud_layout_vec2(1.0f, 0.0f), true, 1));

    hud_layout_arrange(&layout);
    hud_layout_draw_tree(&layout);
}

void render_hud_timer(s32 playerId) {
    s32 var_s0;

    if ((gModeSelection != 2) && (gModeSelection != 3)) {
        if ((gPlayerCountSelection1 == 1) && (playerId == PLAYER_ONE)) {
            render_arcadekart_timer_strip(playerId);
            return;
        }

        if (D_8018D320 == playerHUD[playerId].lapCount) {
            if (D_8015F890 == 0) {
                for (var_s0 = 0; var_s0 < 3; var_s0++) {
                    if (D_80165658[var_s0] == 0) {
                        print_timer(playerHUD[playerId].lapCompletionTimeXs[var_s0],
                                    playerHUD[playerId].timerY + (var_s0 * 0x10),
                                    playerHUD[playerId].lapDurations[var_s0]);
                    } else {
                        print_timer_rainbow(playerHUD[playerId].lapCompletionTimeXs[var_s0],
                                            playerHUD[playerId].timerY + (var_s0 * 0x10),
                                            playerHUD[playerId].lapDurations[var_s0]);
                    }
                }
                draw_hud_2d_texture_32x16(playerHUD[playerId].totalTimeX - 0x13, playerHUD[playerId].timerY + 0x38,
                                          (u8*) common_texture_hud_total_time);
                if (D_801657E5 != 0) {
                    print_timer_rainbow(playerHUD[playerId].totalTimeX, playerHUD[playerId].timerY + 0x30,
                                        playerHUD[playerId].someTimer);
                } else {
                    print_timer(playerHUD[playerId].totalTimeX, playerHUD[playerId].timerY + 0x30,
                                playerHUD[playerId].someTimer);
                }
            }
        } else {
            if (playerHUD[playerId].blinkTimer == 0) {
                draw_hud_2d_texture_32x16(playerHUD[playerId].timerX - 0x13, playerHUD[playerId].timerY + 8,
                                          (u8*) common_texture_hud_time);
                print_timer(playerHUD[playerId].timerX, playerHUD[playerId].timerY, playerHUD[playerId].someTimer);
            } else {
                draw_hud_2d_texture_32x16(playerHUD[playerId].timerX - 0x13, playerHUD[playerId].timerY + 8,
                                          (u8*) common_texture_hud_lap_time);
                if (D_801657E3 != 0) {
                    print_timer_rainbow(playerHUD[playerId].timerX, playerHUD[playerId].timerY,
                                        playerHUD[playerId].someTimer1);
                } else if (playerHUD[playerId].blinkState == 0) {
                    print_timer(playerHUD[playerId].timerX, playerHUD[playerId].timerY, playerHUD[playerId].someTimer1);
                }
            }
        }
    }
}

void draw_lap_count(s16 lapX, s16 lapY, s8 lap) {
    gSPDisplayList(gDisplayListHead++, D_0D008108);
    gSPDisplayList(gDisplayListHead++, D_0D007EF8);
    gDPSetAlphaCompare(gDisplayListHead++, G_AC_THRESHOLD);
    load_texture_block_rgba16_mirror((u8*) common_texture_hud_123, 32, 8);
    // Display current lap. Ex. 1/3

    func_8004BA98_wide(lapX, lapY, 8, 8, lap * 8, 0, 0); // display the digit
    func_8004BA98_wide(lapX + 8, lapY, 8, 8, 24, 0, 0);  // display the /
    func_8004BA98_wide(lapX + 16, lapY, 8, 8, 16, 0, 0); // display the 3
    gSPDisplayList(gDisplayListHead++, D_0D007EB8);
}

static f32 sArcadeKartPortraitDrawScale = 1.0f;

void func_8004FDB4(f32 arg0, f32 arg1, s16 arg2, s16 arg3, s16 characterId, s32 arg5, s32 arg6, s32 arg7, s32 arg8) {
    if ((IsYoshiValley()) && (arg3 < 3) && (arg8 == 0)) {
        func_80042330((s32) arg0, (s32) arg1, 0U, sArcadeKartPortraitDrawScale);
        gSPDisplayList(gDisplayListHead++, D_0D007DB8);
        func_8004B35C(0x000000FF, 0x000000FF, 0x000000FF, D_8018D3E0);
        gDPLoadTLUT_pal256(gDisplayListHead++, common_tlut_portrait_bomb_kart_and_question_mark);
        rsp_load_texture(common_texture_portrait_question_mark, 0x00000020, 0x00000020);
        gSPDisplayList(gDisplayListHead++, D_0D0069E0);
    } else {
        f32 rankXOffset = 9.0f * sArcadeKartPortraitDrawScale;
        f32 rankYOffset = 7.0f * sArcadeKartPortraitDrawScale;

        func_80042330_portrait(arg0, arg1, 0U, sArcadeKartPortraitDrawScale, arg3);
        gSPDisplayList(gDisplayListHead++, D_0D007DB8);
        func_8004B35C(0x000000FF, 0x000000FF, 0x000000FF, arg5);
        gDPLoadTLUT_pal256(gDisplayListHead++, gPortraitTLUTs[characterId]);
        rsp_load_texture(gPortraitTextures[characterId], 0x00000020, 0x00000020);
        if (arg7 != 0) {
            gSPDisplayList(gDisplayListHead++, D_0D0069F8);
        } else {
            gSPDisplayList(gDisplayListHead++, D_0D0069E0);
        }
        if (arg6 != 0) {
            func_80042330_portrait(arg0, arg1, 0U, sArcadeKartPortraitDrawScale, arg3);
            gSPDisplayList(gDisplayListHead++, D_0D007A60);
            func_8004B35C(D_8018D3E4, D_8018D3E8, D_8018D3EC, 0x000000FF);
            func_80044924(common_texture_character_portrait_border, 0x20, 0x20);
            gSPDisplayList(gDisplayListHead++, D_0D0069E0);
        }
        gSPDisplayList(gDisplayListHead++, D_0D007DB8);
        func_8004B35C(0x000000FF, 0x000000FF, 0x000000FF, arg5);
        gSPDisplayList(gDisplayListHead++, D_0D007CB8);
        gDPLoadTLUT_pal256(gDisplayListHead++, common_tlut_hud_type_C_rank_font);
        rsp_load_texture(common_texture_hud_type_C_rank_font[arg2], 0x00000010, 0x00000010);
        if (arg7 != 0) {
            func_80042330_portrait((s32) (arg0 + rankXOffset), (s32) (arg1 + rankYOffset), 0U,
                                    sArcadeKartPortraitDrawScale, arg3);
        } else {
            func_80042330_portrait((s32) (arg0 - rankXOffset), (s32) (arg1 + rankYOffset), 0U,
                                    sArcadeKartPortraitDrawScale, arg3);
        }
        gSPDisplayList(gDisplayListHead++, D_0D006980);
    }
}

#define ARCADEKART_RANKING_PORTRAIT_SIZE 32.0f
#define ARCADEKART_RANKING_PORTRAIT_SPACING 10.0f
#define ARCADEKART_RANKING_PORTRAIT_EDGE_OFFSET 26.0f
#define ARCADEKART_RANKING_PORTRAIT_SPACING_MIN 0.0f
#define ARCADEKART_RANKING_PORTRAIT_SPACING_MAX 24.0f

typedef struct ArcadeKartRankingPortraitLeaf {
    s32 rankIndex;
    s32 lapCount;
    s16 characterId;
    s32 fadeAlpha;
    s32 isPlayer;
    s32 rankSide;
} ArcadeKartRankingPortraitLeaf;

static void draw_arcadekart_ranking_portrait_leaf(UNUSED const HudLayoutContext* ctx, UNUSED HudWidgetId widgetId,
                                                  HudRect rect, void* userData) {
    ArcadeKartRankingPortraitLeaf* leaf = (ArcadeKartRankingPortraitLeaf*) userData;
    f32 centerX;
    f32 centerY;

    if (leaf == NULL) {
        return;
    }

    centerX = rect.x + (rect.w * 0.5f);
    centerY = rect.y + (rect.h * 0.5f);
    FrameInterpolation_RecordOpenChild("ranking_portraits", (leaf->rankSide << 4) | leaf->rankIndex);
    sArcadeKartPortraitDrawScale = get_arcadekart_rect_scale(rect, ARCADEKART_RANKING_PORTRAIT_SIZE,
                                                             ARCADEKART_RANKING_PORTRAIT_SIZE);
    func_8004FDB4(centerX, centerY, leaf->rankIndex, leaf->lapCount, leaf->characterId, leaf->fadeAlpha,
                  leaf->isPlayer, leaf->rankSide, 0);
    sArcadeKartPortraitDrawScale = 1.0f;
    FrameInterpolation_RecordCloseChild();
}

static void render_arcadekart_ranking_portrait_stack(void) {
    HudLayoutContext layout;
    HudWidgetId root;
    HudWidgetId stack;
    HudWidgetId widget;
    ArcadeKartRankingPortraitLeaf leaves[4];
    HudRect viewRect = get_arcadekart_hud_player_view_rect(PLAYER_ONE);
    f32 portraitScale = get_arcadekart_hud_cluster_scale(viewRect, "gArcadeKart.Hud.PortraitStripScale", 1.0f);
    f32 sidePadding = get_arcadekart_hud_view_x(viewRect, CVarGetFloat("gArcadeKart.Hud.PortraitStripSidePad", ARCADEKART_RANKING_PORTRAIT_EDGE_OFFSET));
    f32 spacing = CVarGetFloat("gArcadeKart.Hud.PortraitStripSpacing", ARCADEKART_RANKING_PORTRAIT_SPACING);
    f32 centerYOffset = get_arcadekart_hud_view_y(viewRect, CVarGetFloat("gArcadeKart.Hud.PortraitStripCenterYOffset", 0.0f));

    if (spacing < ARCADEKART_RANKING_PORTRAIT_SPACING_MIN) {
        spacing = ARCADEKART_RANKING_PORTRAIT_SPACING_MIN;
    } else if (spacing > ARCADEKART_RANKING_PORTRAIT_SPACING_MAX) {
        spacing = ARCADEKART_RANKING_PORTRAIT_SPACING_MAX;
    }
    spacing *= portraitScale;
    HudVec2 portraitSize = hud_layout_vec2(ARCADEKART_RANKING_PORTRAIT_SIZE * portraitScale,
                                           ARCADEKART_RANKING_PORTRAIT_SIZE * portraitScale);
    const s32 portraitAlpha = 0x000000FF;
    s32 leafCount = 0;
    s32 i;

    hud_layout_begin_safe_zone(&layout, viewRect, get_arcadekart_hud_safe_zone_padding(viewRect));
    root = hud_layout_root(&layout);
    stack = hud_layout_vertical_box(&layout, spacing, hud_layout_padding(0.0f, 0.0f, 0.0f, 0.0f));

    for (i = 0; i < 4; i++) {
        s16 playerId;
        s16 characterId;
        s32 lapCount;
        s32 rankSide = 0;

        if (D_8018D050[i] < 0.0f) {
            continue;
        }
        if (D_8018D078[i] < 0.0f) {
            rankSide = 1;
        }

        playerId = gGPCurrentRacePlayerIdByRank[i];
        characterId = gGPCurrentRaceCharacterIdByRank[i];
        lapCount = gLapCountByPlayerId[playerId];
        leaves[leafCount].rankIndex = i;
        leaves[leafCount].lapCount = lapCount;
        leaves[leafCount].characterId = characterId;
        leaves[leafCount].fadeAlpha = portraitAlpha;
        leaves[leafCount].isPlayer = (characterId == gPlayerOne->characterId) ? 1 : 0;
        leaves[leafCount].rankSide = rankSide;

        widget = hud_layout_draw(&layout, portraitSize, draw_arcadekart_ranking_portrait_leaf, &leaves[leafCount]);
        hud_layout_box_add(&layout, stack, widget,
                           hud_layout_auto_slot(hud_layout_padding(0.0f, 0.0f, 0.0f, 0.0f),
                                                hud_layout_vec2(0.5f, 0.5f), false));
        leafCount++;
    }

    if (leafCount == 0) {
        return;
    }

    hud_layout_canvas_add(&layout, root, stack,
                          hud_layout_canvas_slot(hud_layout_anchor(0.0f, 0.5f, 0.0f, 0.5f),
                                                hud_layout_padding(sidePadding, centerYOffset, 0.0f, 0.0f),
                                                hud_layout_vec2(0.0f, 0.5f), true, 0));
    hud_layout_arrange(&layout);
    hud_layout_draw_tree(&layout);
}

void func_80050320(void) {
    s16 temp_v0;
    s16 characterId;
    s32 i;
    s32 lapCount;
    s32 var_a0;

    if (D_801657E2 == 0) {
        if (gPlayerCountSelection1 == 1) {
            render_arcadekart_ranking_portrait_stack();
        } else {
            for (i = 0; i < 4; i++) {
                var_a0 = 0;
                if (D_8018D050[i] >= 0.0f) {
                    if (D_8018D078[i] < 0.0) {
                        var_a0 = 1;
                    }

                    FrameInterpolation_RecordOpenChild("ranking_portraits", (var_a0 << 4) | i);

                    temp_v0 = gGPCurrentRacePlayerIdByRank[i];
                    characterId = gGPCurrentRaceCharacterIdByRank[i];
                    lapCount = gLapCountByPlayerId[temp_v0];
                    if (characterId == gPlayerOne->characterId) {
                        func_8004FDB4(D_8018D028[i], D_8018D050[i], i, lapCount, characterId, 0x000000FF, 1, var_a0,
                                      0);
                    } else {
                        func_8004FDB4(D_8018D028[i], D_8018D050[i], i, lapCount, characterId, D_8018D3E0, 0, var_a0,
                                      0);
                    }

                    FrameInterpolation_RecordCloseChild();
                }
            }
        }
    } else {
        for (i = 0; i < 8; i++) {
            var_a0 = 0;
            if (D_8018D050[i] >= 0.0f) {
                if (D_8018D078[i] <= 0.0) {
                    var_a0 = 1;
                }

                FrameInterpolation_RecordOpenChild("ranking_portraits2", (var_a0 << 4) | i);

                temp_v0 = gGPCurrentRacePlayerIdByRank[i];
                // ????
                characterId = (gPlayerOne + temp_v0)->characterId;
                lapCount = gLapCountByPlayerId[temp_v0];
                if (temp_v0 == 0) {
                    func_8004FDB4(D_8018D028[i], D_8018D050[i], i, lapCount, characterId, 0x000000FF, 1, var_a0, 1);
                } else {
                    func_8004FDB4(D_8018D028[i], D_8018D050[i], i, lapCount, characterId, 0x000000FF, 0, var_a0, 1);
                }

                FrameInterpolation_RecordCloseChild();
            }
        }
    }
    gSPTexture(gDisplayListHead++, 0x0001, 0x0001, 0, G_TX_RENDERTILE, G_OFF);
}

s32 func_80050644(u16 arg0, s32* arg1, s32* arg2) {
    s32 var_v0 = 0;
    s32 thing = 0;
    s32 test = gLapCountByPlayerId[arg0];

    if (test < 3) {
        if (gPlayerCountSelection1 == 1) {
            if (test >= 0) {
                thing = (s32) (gLapCompletionPercentByPlayerId[arg0] * 928);
            }
            if (thing < 0x104) {
                *arg1 = thing;
                *arg2 = 0;
                var_v0 = 1;
            } else if (thing < 0x1D0) {
                *arg1 = 0x00000104;
                *arg2 = thing - 0x104;
                var_v0 = 2;
            } else if (thing < 0x2D4) {
                *arg1 = 0x2D4 - thing;
                *arg2 = 0x000000CC;
                var_v0 = 3;
            } else {
                *arg1 = 0;
                *arg2 = 0x3A0 - thing;
                var_v0 = 4;
            }
        } else {
            if (test >= 0) {
                thing = (s32) (gLapCompletionPercentByPlayerId[arg0] * 260);
            }
            *arg1 = thing;
            *arg2 = 0;
        }
    } else if (gPlayerCountSelection1 == 1) {
        *arg1 = 0x00000020;
        *arg2 = (gGPCurrentRaceRankByPlayerId[arg0] * 0x14) + 0x20;
    } else {
        thing = (s32) (gLapCompletionPercentByPlayerId[arg0] * 260);
        *arg1 = thing;
        *arg2 = 0;
    }
    return var_v0;
}

void func_800507D8(u16 bombIndex, s32* arg1, s32* arg2) {
    s32 temp_v0 = gBombKarts[bombIndex].waypointIndex;
    s32 var_v1 = 0;

    if (temp_v0 != 0) {
        var_v1 = (s32) (temp_v0 * 0x3A0) / (s32) gSelectedPathCount;
    }
    if (var_v1 < 0x104) {
        *arg1 = var_v1;
        *arg2 = 0;
    } else if (var_v1 < 0x1D0) {
        *arg1 = 0x00000104;
        *arg2 = var_v1 - 0x104;
    } else if (var_v1 < 0x2D4) {
        *arg1 = 0x2D4 - var_v1;
        *arg2 = 0x000000CC;
    } else {
        *arg1 = 0;
        *arg2 = 0x3A0 - var_v1;
    }
}

void func_800508C0(void) {
    s32 sp54;
    s32 sp50;
    s32 sp4C;
    s32 temp_v1;
    s16 var_s0;
    UNUSED s16 stackPadding;
    u16 var_s0_2;
    u16 var_s1;
    u16 var_s2;

    if (gModeSelection == TIME_TRIALS) {
        var_s0 = D_80164378[0];
    } else {
        var_s0 = gGPCurrentRacePlayerIdByRank[0];
    }
    sp4C = func_80050644(var_s0, &sp54, &sp50);
    temp_v1 = gLapCountByPlayerId[var_s0];
    if (temp_v1 > 0) {
        if (temp_v1 == 1) {
            var_s0_2 = 0;
            var_s1 = 0;
            var_s2 = 0x000000FF;
        } else {
            if (temp_v1 == 2) {
                var_s0_2 = 0x00FF;
                var_s1 = 0x000000FF;
                var_s2 = 0;
            } else {
                var_s0_2 = 0x00FF;
                var_s1 = 0;
                var_s2 = 0;
            }
        }
        func_8004C024(0x0020, 0x0012, 0x0104, var_s0_2, var_s1, var_s2, 0x000000FF);
        func_8004C148(0x0124, 0x0012, 0x00CC, var_s0_2, var_s1, var_s2, 0x000000FF);
        func_8004C024(0x0020, 0x00DE, 0x0104, var_s0_2, var_s1, var_s2, 0x000000FF);
        func_8004C148(0x0020, 0x0012, 0x00CC, var_s0_2, var_s1, var_s2, 0x000000FF);
    }
    if ((temp_v1 < 0) || (temp_v1 >= 3)) {
        return;
    }
    switch (temp_v1) {
        case 0:
            var_s0_2 = 0;
            var_s1 = 0;
            var_s2 = 0x00FF;
            break;
        case 1:
            var_s0_2 = 0x00FF;
            var_s1 = 0x00FF;
            var_s2 = 0;
            break;
        case 2:
            var_s0_2 = 0x00FF;
            var_s1 = 0;
            var_s2 = 0;
            break;
        default:
            break;
    }
    switch (sp4C) {
        case 1:
            func_8004C024(0x0020, 0x0012, sp54, var_s0_2, var_s1, var_s2, 0x000000FF);
            break;
        case 2:
            func_8004C024(0x0020, 0x0012, 0x0104, var_s0_2, var_s1, var_s2, 0x000000FF);
            func_8004C148(0x0124, 0x0012, sp50, var_s0_2, var_s1, var_s2, 0x000000FF);
            break;
        case 3:
            func_8004C024(0x0020, 0x0012, 0x0104, var_s0_2, var_s1, var_s2, 0x000000FF);
            func_8004C148(0x0124, 0x0012, 0x00CC, var_s0_2, var_s1, var_s2, 0x000000FF);
            func_8004C024(sp54 + 0x20, 0x00DE, 0x104 - sp54, var_s0_2, var_s1, var_s2, 0x000000FF);
            break;
        case 4:
            func_8004C024(0x0020, 0x0012, 0x0104, var_s0_2, var_s1, var_s2, 0x000000FF);
            func_8004C148(0x0124, 0x0012, 0x00CC, var_s0_2, var_s1, var_s2, 0x000000FF);
            func_8004C024(0x0020, 0x00DE, 0x0104, var_s0_2, var_s1, var_s2, 0x000000FF);
            func_8004C148(0x0020, sp50 + 0x12, 0xCC - sp50, var_s0_2, var_s1, var_s2, 0x000000FF);
            break;
        case 0:
        default:
            break;
    }
}

void func_80050C68(void) {
    UNUSED s32 stackPadding0;
    s32 sp88;
    s32 sp84;
    UNUSED s32 stackPadding1;
    s32 var_s1;

    for (var_s1 = 0; var_s1 < NUM_BOMB_KARTS_VERSUS; var_s1++) {
        if ((gBombKarts[var_s1].state != BOMB_STATE_EXPLODED) && (gBombKarts[var_s1].state != BOMB_STATE_INACTIVE)) {
            func_800507D8(var_s1, &sp88, &sp84);
            gSPDisplayList(gDisplayListHead++, D_0D007DB8);
            gDPLoadTLUT_pal256(gDisplayListHead++, common_tlut_portrait_bomb_kart_and_question_mark);
            rsp_load_texture(common_texture_portrait_bomb_kart, 0x00000020, 0x00000020);
            func_80042330(sp88 + 0x20, sp84 + 0x12, 0U, 0.6f);
            gSPDisplayList(gDisplayListHead++, D_0D0069E0);
        }
    }
}

void func_80050E34(s32 playerId, s32 arg1) {
    s32 objectIndex;
    s32 spD0;
    s32 spCC;
    s32 spC4;
    s32 lapCount;
    s32 characterId;
    s32 spB8;
    s32 result;
    Object* object;
    Player *player = &gPlayerOne[playerId];

    lapCount = gLapCountByPlayerId[playerId];
    characterId = player->characterId;
    objectIndex = D_8018CE10[playerId].objectIndex;

    if (gPlayerCountSelection1 == 1) {
        spC4 = 0x00000012;
    } else {
        spC4 = 0x00000078;
    }

    result = func_80050644(playerId, &spD0, &spCC);
    if ((result == 2) || (result == 3)) {
        spB8 = 1;
    } else {
        spB8 = 0;
    }

    FrameInterpolation_RecordOpenChild("progress_portraits", TAG_PORTRAITS( ((playerId & 0x7) << 8) |  ((characterId & 0x7) << 5) | (objectIndex & 0x1F) ));
    if ((IsYoshiValley()) && (lapCount < 3)) {
        gSPDisplayList(gDisplayListHead++, D_0D007DB8);
        gDPLoadTLUT_pal256(gDisplayListHead++, common_tlut_portrait_bomb_kart_and_question_mark);
        rsp_load_texture(common_texture_portrait_question_mark, 0x00000020, 0x00000020);
        object = &gObjectList[objectIndex];
        object->pos[0] = object->offset[0] + ((f32) (spD0 + 0x20));
        object->pos[1] = object->offset[1] + ((f32) (spC4 + spCC));
        object->pos[2] = object->offset[2];
        rsp_set_matrix_transformation(object->pos, object->direction_angle, object->sizeScaling);
        gSPDisplayList(gDisplayListHead++, D_0D0069E0);
    } else {
        gDPLoadTLUT_pal256(gDisplayListHead++, gPortraitTLUTs[characterId]);
        gSPDisplayList(gDisplayListHead++, D_0D007DB8);
        if (player->effects & STAR_EFFECT) {
            func_8004B614((s32) D_801656C0, (s32) D_801656D0, (s32) D_801656E0, 0x00000080, 0x00000080, 0x00000080,
                          (s32) gObjectList[objectIndex].primAlpha);
        } else {
            set_transparency((s32) gObjectList[objectIndex].primAlpha);
        }
        rsp_load_texture(gPortraitTextures[characterId], 0x00000020, 0x00000020);
        object = &gObjectList[objectIndex];
        object->pos[0] = object->offset[0] + ((f32) (spD0 + 0x20));
        object->pos[1] = object->offset[1] + ((f32) (spC4 + spCC));
        object->pos[2] = object->offset[2];
        rsp_set_matrix_transformation(object->pos, object->direction_angle, object->sizeScaling);
        if (spB8 != 0) {
            gSPDisplayList(gDisplayListHead++, D_0D0069F8);
        } else {
            gSPDisplayList(gDisplayListHead++, D_0D0069E0);
        }
        gDPLoadTLUT_pal256(gDisplayListHead++, common_tlut_hud_type_C_rank_tiny_font);
        rsp_load_texture(common_texture_hud_type_C_rank_tiny_font[arg1 + 1], 8, 8);
        if (spB8 != 0) {
            func_80042330_unchanged(spD0 + 0x26, (spC4 + spCC) + 4, 0U, 1.0f);
        } else {
            func_80042330_unchanged(spD0 + 0x1B, (spC4 + spCC) + 4, 0U, 1.0f);
        }
        gSPDisplayList(gDisplayListHead++, D_0D006950);
        if ((player == gPlayerOne) && (gScreenModeSelection == SCREEN_MODE_1P)) {
            gSPDisplayList(gDisplayListHead++, D_0D007A40);
            func_8004B35C(D_8018D3E4, D_8018D3E8, D_8018D3EC, 0x000000FF);
            func_80044924(common_texture_character_portrait_border, 0x00000020, 0x00000020);
            rsp_set_matrix_transformation(object->pos, object->direction_angle, object->sizeScaling);
            gSPDisplayList(gDisplayListHead++, D_0D0069E0);
        }
    }
    FrameInterpolation_RecordCloseChild();
}

void func_800514BC(void) {
    s32 temp_a0;
    s32 var_s0;
    s32 var_s1;
    s32 var_s3;
    Player* player;

    if (gScreenModeSelection == 0) {
        func_800508C0();
    }
    var_s3 = 8;
    if ((gPlayerCountSelection1 == 2) && (gActiveScreenMode == 2)) {
        var_s3 = 0;
    }
    for (var_s0 = var_s3 - 1, var_s1 = 0; var_s1 < var_s3; var_s1++, var_s0--) {
        temp_a0 = gGPCurrentRacePlayerIdByRank[var_s0];
        player = &gPlayerOne[temp_a0];
        if ((player->type & 0x8000) && ((temp_a0 != 0) || (gPlayerCountSelection1 != 1))) {
            func_80050E34(temp_a0, var_s0);
        }
    }
    if (gModeSelection == 1) {
        func_80050E34(0, gGPCurrentRaceRankByPlayerIdDup[0]);
    } else if (gPlayerCountSelection1 == 1) {
        func_80050E34(0, gGPCurrentRaceRankByPlayerId[0]);
    }
    gSPTexture(gDisplayListHead++, 1, 1, 0, G_TX_RENDERTILE, G_OFF);
}

void render_object_leaf_particle(UNUSED s32 cameraId) {
    size_t i;
    s32 leafIndex;
    Object* object;

    gSPDisplayList(gDisplayListHead++, D_0D0079C8);
    gSPClearGeometryMode(gDisplayListHead++, G_CULL_BOTH);
    load_texture_block_rgba16_mirror((u8*) common_texture_particle_leaf, 0x00000020, 0x00000010);
    for (i = 0; i < gLeafParticle_SIZE; i++) {
        leafIndex = gLeafParticle[i];

        FrameInterpolation_RecordOpenChild("leaves", (leafIndex << 4) | cameraId);

        if (leafIndex != -1) {
            object = &gObjectList[leafIndex];
            if ((object->state >= 2) && (object->unk_0D5 == 7) && (gMatrixHudCount <= MTX_HUD_POOL_SIZE_MAX)) {
                rsp_set_matrix_gObjectList(leafIndex);
                gSPDisplayList(gDisplayListHead++, D_0D0069C8);
            }
        }

        FrameInterpolation_RecordCloseChild();
    }
    gSPSetGeometryMode(gDisplayListHead++, G_CULL_BACK);
    gSPTexture(gDisplayListHead++, 1, 1, 0, G_TX_RENDERTILE, G_OFF);
}

void render_object_snowflakes_particles(s32 cameraId) {
    size_t i;
    s32 snowflakeIndex;

    gSPDisplayList(gDisplayListHead++, D_0D007AE0);
    gDPSetCombineLERP(gDisplayListHead++, 1, 0, SHADE, 0, 0, 0, 0, TEXEL0, 1, 0, SHADE, 0, 0, 0, 0, TEXEL0);
    func_80044F34(D_0D0293D8, 0x10, 0x10);
    for (i = 0; i < NUM_SNOWFLAKES; i++) {
        snowflakeIndex = gObjectParticle1[i];

        FrameInterpolation_RecordOpenChild("snowFlakes", (snowflakeIndex << 4) | cameraId);

        if (gObjectList[snowflakeIndex].state >= 2) {
            rsp_set_matrix_gObjectList(snowflakeIndex);
            gSPDisplayList(gDisplayListHead++, D_0D006980);
        }

        FrameInterpolation_RecordCloseChild();
    }
    gSPTexture(gDisplayListHead++, 1, 1, 0, G_TX_RENDERTILE, G_OFF);
}

// Render clouds
void func_80051ABC(ScreenContext* screen, s16 arg0, s32 arg1) {
    s32 var_s0;
    s32 objectIndex;
    Object* object;

    D_8018D228 = 0xFF;
    gSPDisplayList(gDisplayListHead++, D_0D007A60);
    DrawSkyActors(screen, arg0);
}

void func_80051C60(ScreenContext* screen, s16 arg0, s32 arg1) {
    s16 var_s5;
    s32 var_s0;
    s32 objectIndex;
    Object* object;

    if (D_801658FE == 0) {
        if (IsKoopaTroopaBeach()) {
            var_s5 = arg0;
        } else if (IsMooMooFarm()) {
            var_s5 = arg0 - 16;
        } else if (IsYoshiValley()) {
            var_s5 = arg0 - 16;
        } else {
            var_s5 = arg0 + 16;
        }
    } else if (IsKoopaTroopaBeach()) {
        var_s5 = arg0 * 2;
    } else {
        var_s5 = arg0 + 32;
    }

    D_8018D228 = 0xFF;
    gSPDisplayList(gDisplayListHead++, D_0D007A60);
    DrawSkyActors(screen, var_s5);
}

void func_80051EBC(ScreenContext* screen) {
    func_80051ABC(screen, 240 - screen->cameraHeight, 0); // 28
}

void func_80051EF8(ScreenContext* screen) {
    s16 temp_a0;

    temp_a0 = 0xF0 - screen->cameraHeight;
    if (IsKoopaTroopaBeach()) {
        temp_a0 = temp_a0 - 0x30;
    } else if (IsMooMooFarm()) {
        temp_a0 = temp_a0 - 0x40;
    } else if (IsYoshiValley()) {
        temp_a0 = temp_a0 - 0x40;
    } else {
        temp_a0 = temp_a0 - 0x30;
    }
    func_80051ABC(screen, temp_a0, 0);
}

void func_80051F9C(ScreenContext* screen) {
    s16 temp_a0;

    temp_a0 = 0xF0 - screen->cameraHeight;
    if (IsKoopaTroopaBeach()) {
        temp_a0 = temp_a0 - 0x30;
    } else if (IsMooMooFarm()) {
        temp_a0 = temp_a0 - 0x40;
    } else if (IsYoshiValley()) {
        temp_a0 = temp_a0 - 0x40;
    } else {
        temp_a0 = temp_a0 - 0x30;
    }
    func_80051ABC(screen, temp_a0, D_8018D1F0);
}

void func_80052044(ScreenContext* screen) {
    func_80051C60(screen, 240 - screen->cameraHeight, 0);
}

void func_80052080(ScreenContext* screen) {
    func_80051C60(screen, 240 - screen->cameraHeight, D_8018D1F0);
}

void func_800520C0(s32 arg0) {
    // Lights1 *D_800E45C0 = LOAD_ASSET(D_800E45C0);
    if (gObjectList[arg0].unk_0D5 == 0) {
        D_800E45C0[0].l[0].l.dir[0] = D_800E45C0[1].l[0].l.dir[0] = D_800E45C0[2].l[0].l.dir[0] =
            D_800E45C0[3].l[0].l.dir[0] = 0;
        D_800E45C0[0].l[0].l.dir[1] = D_800E45C0[1].l[0].l.dir[1] = D_800E45C0[2].l[0].l.dir[1] =
            D_800E45C0[3].l[0].l.dir[1] = -0x78;
        D_800E45C0[0].l[0].l.dir[2] = D_800E45C0[1].l[0].l.dir[2] = D_800E45C0[2].l[0].l.dir[2] =
            D_800E45C0[3].l[0].l.dir[2] = 0;
    } else {
        D_800E45C0[0].l[0].l.dir[0] = D_800E45C0[1].l[0].l.dir[0] = D_800E45C0[2].l[0].l.dir[0] =
            D_800E45C0[3].l[0].l.dir[0] = 0x63;
        D_800E45C0[0].l[0].l.dir[1] = D_800E45C0[1].l[0].l.dir[1] = D_800E45C0[2].l[0].l.dir[1] =
            D_800E45C0[3].l[0].l.dir[1] = 0x42;
        D_800E45C0[0].l[0].l.dir[2] = D_800E45C0[1].l[0].l.dir[2] = D_800E45C0[2].l[0].l.dir[2] =
            D_800E45C0[3].l[0].l.dir[2] = 0;
    }
}

void func_8005285C(s32 cameraId, s32 playerId) {
    Player* temp_v0;

    temp_v0 = &gPlayerOne[playerId];
    D_80183E40[0] = temp_v0->pos[0];
    D_80183E40[1] = temp_v0->pos[1];
    D_80183E40[2] = temp_v0->pos[2];
    D_80183E80[0] = 0;
    D_80183E80[1] = 0;
    D_80183E80[2] = 0;
    FrameInterpolation_RecordOpenChild("ice_block", (playerId << 4) | cameraId);
    func_80043500(D_80183E40, D_80183E80, 0.02f, d_course_sherbet_land_dl_ice_block);
    FrameInterpolation_RecordCloseChild();
}

void func_800528EC(s32 cameraId) {
    s32 var_s3;
    s32 objectIndex;
    Object* object;
    // Lights1 D_800E4620l = *(Lights1 *) LOAD_ASSET(D_800E4620);

    D_80183E80[0] = D_8016582C[0];
    D_80183E80[1] = D_8016582C[1];
    D_80183E80[2] = D_8016582C[2];
    gSPDisplayList(gDisplayListHead++, D_0D007B00);
    gSPNumLights(gDisplayListHead++, 1);
    gSPLight(gDisplayListHead++, &D_800E4620.l[0], LIGHT_1);
    gSPLight(gDisplayListHead++, &D_800E4620.a, LIGHT_2);
    gDPSetCombineMode(gDisplayListHead++, G_CC_MODULATEIA, G_CC_MODULATEIA);
    gSPClearGeometryMode(gDisplayListHead++, G_CULL_BOTH);
    gSPSetGeometryMode(gDisplayListHead++, G_SHADE | G_LIGHTING | G_SHADING_SMOOTH);
    load_texture_block_ia16_nomirror(d_course_sherbet_land_ice, 32, 32);
    if (gPlayerCountSelection1 < 3) {
        for (var_s3 = 0; var_s3 < gObjectParticle2_SIZE; var_s3++) {
            objectIndex = gObjectParticle2[var_s3];
            if (objectIndex != NULL_OBJECT_ID) {
                object = &gObjectList[objectIndex];
                if (object->state > 0) {
                    FrameInterpolation_RecordOpenChild("ice_block2", (var_s3 << 4) | cameraId);
                    rsp_set_matrix_transformation(object->pos, D_80183E80, object->sizeScaling);
                    gSPVertex(gDisplayListHead++, D_0D005BD0, 3, 0);
                    gSPDisplayList(gDisplayListHead++, D_0D006930);
                    FrameInterpolation_RecordCloseChild();
                }
            }
        }
    } else {
        for (var_s3 = 0; var_s3 < gObjectParticle2_SIZE; var_s3++) {
            objectIndex = gObjectParticle2[var_s3];
            if (objectIndex != NULL_OBJECT_ID) {
                object = &gObjectList[objectIndex];
                if ((object->state > 0) && (cameraId == object->unk_084[7]) &&
                    (gMatrixHudCount <= MTX_HUD_POOL_SIZE_MAX)) {
                    FrameInterpolation_RecordOpenChild("ice_block3", (var_s3 << 4) | cameraId);
                    rsp_set_matrix_transformation(object->pos, D_80183E80, object->sizeScaling);
                    gSPVertex(gDisplayListHead++, D_0D005BD0, 3, 0);
                    gSPDisplayList(gDisplayListHead++, D_0D006930);
                    FrameInterpolation_RecordCloseChild();
                }
            }
        }
    }
    gSPSetGeometryMode(gDisplayListHead++, G_CULL_BACK);
    gSPClearGeometryMode(gDisplayListHead++, G_LIGHTING);
    gSPTexture(gDisplayListHead++, 0x0001, 0x0001, 0, G_TX_RENDERTILE, G_OFF);
}

void render_ice_block(s32 cameraId) {
    s32 objectIndex;
    // Lights1 D_800E4620l = *(Lights1 *) LOAD_ASSET(D_800E4620);
    D_800E4620.l[0].l.dir[0] = D_80165840[0];
    D_800E4620.l[0].l.dir[1] = D_80165840[1];
    D_800E4620.l[0].l.dir[2] = D_80165840[2];
    gSPLight(gDisplayListHead++, &D_800E4620.l[0], LIGHT_1);
    gSPLight(gDisplayListHead++, &D_800E4620.a, LIGHT_2);
    for (size_t i = 0; i < gPlayerCountSelection1; i++) {
        objectIndex = gIndexLakituList[i];
        if (objectIndex) {}
        if (func_80072320(objectIndex, 4) != false) {
            func_8005285C(cameraId, i);
        }
        func_80072320(objectIndex, 0x00000010);
    }
    func_800528EC(cameraId);
}

void func_80052D70(s32 cameraId, s32 playerId) {
    s32 test;
    Player* temp_v1;

    temp_v1 = &gPlayerOne[playerId];
    test = gIndexLakituList[playerId];
    if (func_80072320(test, 8) != 0) {
        D_80183E40[0] = temp_v1->pos[0];
        D_80183E40[1] = temp_v1->unk_074 - 6.5;
        D_80183E40[2] = temp_v1->pos[2];
        FrameInterpolation_RecordOpenChild("some_snow_thing", (playerId << 4) | cameraId);
        func_800435A0(D_80183E40, (u16*) D_80183E80, 0.02f, d_course_sherbet_land_dl_ice_block, 0x000000FF);
        FrameInterpolation_RecordCloseChild();
    }
}

void func_80052E30(s32 cameraId) {
    s32 var_s0;
    D_800E4620.l[0].l.dir[0] = D_80165840[0];
    D_800E4620.l[0].l.dir[1] = D_80165840[1];
    D_800E4620.l[0].l.dir[2] = D_80165840[2];
    gSPLight(gDisplayListHead++, &D_800E4620.l[0], LIGHT_1);
    gSPLight(gDisplayListHead++, &D_800E4620.a, LIGHT_2);
    D_80183E80[0] = 0;
    D_80183E80[1] = 0;
    D_80183E80[2] = 0;
    if (gPlayerCount == 1) {
        for (var_s0 = 0; var_s0 < gPlayerCountSelection1; var_s0++) {
            func_80052D70(cameraId, var_s0);
        }
    }
}

void render_object_train_smoke_particle(s32 objectIndex, s32 cameraId) {
    Camera* camera;

    camera = &camera1[cameraId];
    if (objectIndex != NULL_OBJECT_ID) {
        if ((gObjectList[objectIndex].state >= 2) && (gObjectList[objectIndex].unk_0D5 == 1) &&
            (gMatrixHudCount <= MTX_HUD_POOL_SIZE_MAX)) {
            set_color_render((s32) gObjectList[objectIndex].type, (s32) gObjectList[objectIndex].type,
                             (s32) gObjectList[objectIndex].type, 0, 0, 0, (s32) gObjectList[objectIndex].primAlpha);
            D_80183E80[1] =
                func_800418AC(gObjectList[objectIndex].pos[0], gObjectList[objectIndex].pos[2], camera->pos);
            func_800431B0(gObjectList[objectIndex].pos, D_80183E80, gObjectList[objectIndex].sizeScaling, D_0D005AE0);
        }
    }
}

// Trains smoke particles.
void render_object_trains_smoke_particles(s32 cameraId) {
    UNUSED s32 pad;
    UNUSED s32 j;
    Camera* camera;
    s32 i;

    camera = &camera1[cameraId];
    gSPDisplayList(gDisplayListHead++, D_0D007AE0);
    load_texture_block_i8_nomirror(D_0D029458, 32, 32);
    func_8004B72C(255, 255, 255, 255, 255, 255, 255);
    D_80183E80[0] = 0;
    D_80183E80[2] = 0x8000;

// Render smoke for any number of trains. Don't know enough about these variables yet.
#ifdef AVOID_UB_WIP
    for (j = 0; j < NUM_TRAINS; j++) {
        if ((gTrainList[j].someFlags != 0) &&
            (is_particle_on_screen(&gTrainList[j].locomotive.position, camera, 0x4000U) != 0)) {

            for (i = 0; i < 128; i++) {
                // Need to make a way to increase this array for each train.
                render_object_train_smoke_particle(gObjectParticle2[i], cameraId);
            }
        }
    }
#else

    if ((gTrainList[0].someFlags != 0) &&
        (is_particle_on_screen(gTrainList[0].locomotive.position, camera, 0x4000U) != 0)) {

        for (i = 0; i < gObjectParticle2_SIZE; i++) {
            render_object_train_smoke_particle(gObjectParticle2[i], cameraId);
        }
    }
    if ((gTrainList[1].someFlags != 0) &&
        (is_particle_on_screen(gTrainList[1].locomotive.position, camera, 0x4000U) != 0)) {
        for (i = 0; i < gObjectParticle3_SIZE; i++) {
            render_object_train_smoke_particle(gObjectParticle3[i], cameraId);
        }
    }
#endif
}

void render_object_paddle_boat_smoke_particle(s32 objectIndex, s32 cameraId) {
    Camera* camera;

    camera = &camera1[cameraId];
    if (objectIndex != NULL_OBJECT_ID) {
        if ((gObjectList[objectIndex].state >= 2) && (gObjectList[objectIndex].unk_0D5 == 6) &&
            (gMatrixHudCount <= MTX_HUD_POOL_SIZE_MAX)) {
            set_color_render((s32) gObjectList[objectIndex].type, (s32) gObjectList[objectIndex].type,
                             (s32) gObjectList[objectIndex].type, gObjectList[objectIndex].unk_0A2,
                             gObjectList[objectIndex].unk_0A2, gObjectList[objectIndex].unk_0A2,
                             (s32) gObjectList[objectIndex].primAlpha);
            D_80183E80[1] =
                func_800418AC(gObjectList[objectIndex].pos[0], gObjectList[objectIndex].pos[2], camera->pos);
            func_800431B0(gObjectList[objectIndex].pos, D_80183E80, gObjectList[objectIndex].sizeScaling, D_0D005AE0);
        }
    }
}

// Likely smoke related.
void render_object_paddle_boat_smoke_particles(s32 cameraId) {
    UNUSED s32 pad[2];
    Camera* camera;
    s32 i;

    camera = &camera1[cameraId];
    gSPDisplayList(gDisplayListHead++, D_0D007AE0);

    load_texture_block_i8_nomirror(D_0D029458, 32, 32);
    func_8004B72C(255, 255, 255, 255, 255, 255, 255);
    D_80183E80[0] = 0;
    D_80183E80[2] = 0x8000;
    if ((gPaddleBoats[0].someFlags != 0) && (is_particle_on_screen(gPaddleBoats[0].position, camera, 0x4000U) != 0)) {
        for (i = 0; i < gObjectParticle2_SIZE; i++) {
            render_object_paddle_boat_smoke_particle(gObjectParticle2[i], cameraId);
        }
    }
    if ((gPaddleBoats[1].someFlags != 0) && (is_particle_on_screen(gPaddleBoats[1].position, camera, 0x4000U) != 0)) {
        for (i = 0; i < gObjectParticle3_SIZE; i++) {
            render_object_paddle_boat_smoke_particle(gObjectParticle3[i], cameraId);
        }
    }
}

void render_object_bowser_flame_particle(s32 objectIndex, s32 cameraId) {
    Camera* camera;
    Object* object;

    camera = &camera1[cameraId];
    if (gMatrixHudCount <= MTX_HUD_POOL_SIZE_MAX) {
        object = &gObjectList[objectIndex];

        FrameInterpolation_RecordOpenChild("bowser_statue_flame", TAG_ITEM_ADDR((objectIndex << 4) | cameraId));
        if (object->unk_0D5 == 9) {
            func_8004B72C(0xFF, (s32) object->type, 0, (s32) object->unk_0A2, 0, 0, (s32) object->primAlpha);
        } else {
            func_8004B138(0xFF, (s32) object->type, 0, (s32) object->primAlpha);
        }
        D_80183E80[1] = func_800418AC(object->pos[0], object->pos[2], camera->pos);
        func_800431B0(object->pos, D_80183E80, object->sizeScaling, D_0D005AE0);

        FrameInterpolation_RecordCloseChild();
    }
}

void render_object_bowser_flame(s32 cameraId) {
    s32 var_s0;
    s32 objectIndex;

    gSPDisplayList(gDisplayListHead++, D_0D007AE0);
    load_texture_block_i8_nomirror(common_texture_particle_smoke[D_80165598], 0x00000020, 0x00000020);
    func_8004B414(0, 0, 0, 0x000000FF);
    D_80183E80[0] = 0;
    D_80183E80[2] = 0x8000;
    for (var_s0 = 0; var_s0 < gObjectParticle1_SIZE; var_s0++) {
        objectIndex = gObjectParticle1[var_s0];
        if ((objectIndex != NULL_OBJECT_ID) && (gObjectList[objectIndex].state >= 3)) {
            render_object_bowser_flame_particle(objectIndex, cameraId);
        }
    }
}

void func_8005477C(s32 objectIndex, u8 arg1, Vec3f arg2) {
    if (gMatrixHudCount <= MTX_HUD_POOL_SIZE_MAX) {
        switch (arg1) { /* irregular */
            case 0:
                set_color_render(0xE6, 0xFF, 0xFF, 0x00, 0x00, 0xFF, (s32) gObjectList[objectIndex].primAlpha);
                break;
            case 1:
                set_color_render(0xFF, 0xFF, 0x96, 0xFF, 0x00, 0x00, (s32) gObjectList[objectIndex].primAlpha);
                break;
            case 2:
                set_color_render(0xFF, 0xE6, 0xFF, 0xFF, 0x00, 0x96, (s32) gObjectList[objectIndex].primAlpha);
                break;
            case 3:
                set_color_render(0xFF, 0xFF, 0x1E, 0xFF, 0x00, 0x00, (s32) gObjectList[objectIndex].primAlpha);
                break;
            default:
                break;
        }
        D_80183E80[1] = func_800418AC(gObjectList[objectIndex].pos[0], gObjectList[objectIndex].pos[2], arg2);
        func_800431B0(gObjectList[objectIndex].pos, D_80183E80, gObjectList[objectIndex].sizeScaling, D_0D005AE0);
    }
}

void render_object_smoke_particles(s32 cameraId) {
    UNUSED s32 stackPadding[2];
    Camera* sp54;
    s32 i;
    s32 objectIndex;
    Object* object;

    sp54 = &camera1[cameraId];

    gSPDisplayList(gDisplayListHead++, D_0D007AE0);
    load_texture_block_i8_nomirror(common_texture_particle_smoke[D_80165598], 32, 32);
    func_8004B72C(255, 255, 255, 255, 255, 255, 255);
    D_80183E80[0] = 0;
    D_80183E80[2] = 0x8000;
    for (i = 0; i < gObjectParticle4_SIZE; i++) {
        objectIndex = gObjectParticle4[i];
        if (objectIndex != NULL_OBJECT_ID) {
            object = &gObjectList[objectIndex];

            FrameInterpolation_RecordOpenChild("smokes_particles", (uintptr_t) (objectIndex << 4) | cameraId);
            if (object->state >= 2) {
                if (object->unk_0D8 == 3) {
                    func_8008A364(objectIndex, cameraId, 0x4000U, 0x00000514);
                } else {
                    func_8008A364(objectIndex, cameraId, 0x4000U, 0x000001F4);
                }
                if (is_obj_flag_status_active(objectIndex, VISIBLE) != 0) {
                    func_8005477C(objectIndex, object->unk_0D8, sp54->pos);
                }
            }
            FrameInterpolation_RecordCloseChild();
        }
    }
}

UNUSED void func_800557AC() {
}

void func_800557B4(s32 objectIndex, s32 cameraId, u32 arg1, u32 arg2) {
    Vec3f sp34;
    Object* object;

    object = &gObjectList[objectIndex];

    FrameInterpolation_RecordOpenChild("penguin", (objectIndex << 4) | cameraId);
    if (object->state >= 2) {
        if (is_obj_flag_status_active(objectIndex, 0x00000020) != 0) {
            if (func_80072320(objectIndex, 4) != 0) {
                if (arg2 >= arg1) {
                    sp34[0] = object->pos[0];
                    sp34[1] = object->pos[1] - 1.0;
                    sp34[2] = object->pos[2];
                    rsp_set_matrix_transformation_inverted_x_y_orientation(sp34, object->orientation,
                                                                           object->sizeScaling);
                    gSPDisplayList(gDisplayListHead++, D_0D0077D0);
                    render_animated_model((Armature*) object->model, (Animation**) object->vertex,
                                          (s16) object->unk_0D8, (s16) object->textureListIndex);
                }
            } else if (arg1 < 0x15F91U) {
                func_8004A7AC(objectIndex, 1.5f);
            }
        }
        rsp_set_matrix_transformation(object->pos, object->orientation, object->sizeScaling);
        gSPDisplayList(gDisplayListHead++, D_0D0077D0);
        render_animated_model((Armature*) object->model, (Animation**) object->vertex, (s16) object->unk_0D8,
                              (s16) object->textureListIndex);
    }

    FrameInterpolation_RecordCloseChild();
}

void func_80055EF4(s32 objectIndex, UNUSED s32 arg1) {
    Object* object;

    object = &gObjectList[objectIndex];
    if (object->state >= 2) {
        func_80043220(object->pos, object->direction_angle, object->sizeScaling, object->model);
    }
}

Vtx common_vtx_neon[] = {
    { { { -32, -31, 0 }, 0, { 0, 0 }, { 255, 255, 255, 255 } } },
    { { { 31, -31, 0 }, 0, { 4032, 0 }, { 255, 255, 255, 255 } } },
    { { { 31, 31, 0 }, 0, { 4032, 3968 }, { 255, 255, 255, 255 } } },
    { { { -32, 31, 0 }, 0, { 0, 3968 }, { 255, 255, 255, 255 } } },

};

void render_object_neon(s32 cameraId) {
    Camera* camera;
    s32 objectIndex;
    Object* object;

    camera = &camera1[cameraId];
    for (size_t i = 0; i < 10; i++) {
        objectIndex = indexObjectList1[i];
        if (D_8018E838[cameraId] == 0) {
            object = &gObjectList[objectIndex];
            FrameInterpolation_RecordOpenChild("neon_sign", TAG_OBJECT((objectIndex << 8) | (cameraId << 4) | i));
            if ((object->state >= 2) && (is_obj_index_flag_status_inactive(objectIndex, 0x00080000) != 0) &&
                (is_object_visible_on_camera(objectIndex, camera, 0x2AABU) != 0)) {
                object->orientation[1] = angle_between_object_camera(objectIndex, camera);
                rsp_set_matrix_transformation(object->pos, object->orientation, object->sizeScaling);
                gSPDisplayList(gDisplayListHead++, D_0D007D78);
                gDPLoadTLUT_pal256(gDisplayListHead++, object->activeTLUT);
                rsp_load_texture(object->activeTexture, 64, 64);
                gSPVertex(gDisplayListHead++, common_vtx_neon, 4, 0);
                gSPDisplayList(gDisplayListHead++, common_rectangle_display);
                gSPTexture(gDisplayListHead++, 1, 1, 0, G_TX_RENDERTILE, G_OFF);
            }
            FrameInterpolation_RecordCloseChild();
        }
    }
}

u8 D_800E471CA[] = {
    0, 1, 2, 3, 2, 1, 0,
};

void func_800569F4(s32 playerIndex) {
    CM_DisplayBattleBombKart(playerIndex, 0);
}

void func_80056A40(s32 playerIndex, s32 arg1) {
    CM_DisplayBattleBombKart(playerIndex, arg1);
}

void func_80056A94(s32 playerIndex) {
    // func_80072428(gIndexObjectBombKart[playerIndex]);
    CM_DisplayBattleBombKart(playerIndex, 0);
}

UNUSED void func_80057330(void) {
}

UNUSED void func_80057338(void) {

    gSPDisplayList(gDisplayListHead++, D_0D0079C8);
    gSPClearGeometryMode(gDisplayListHead++, G_CULL_BOTH);
    gSPDisplayList(gDisplayListHead++, D_0D007AE0);
    gSPTexture(gDisplayListHead++, 1, 1, 0, G_TX_RENDERTILE, G_OFF);
}

UNUSED void func_800573BC(void) {
}

UNUSED void func_800573C4(void) {
}

UNUSED void func_800573CC(void) {
}
UNUSED void func_800573D4(void) {
}

UNUSED void func_800573DC(void) {
}

void func_800573E4(s32 x, s32 y, s8 str) {
    render_texture_rectangle(x, y, 8, 8, (((str % 16) * 8) << 16) >> 16, (((unsigned short) (str / 16)) << 19) >> 16,
                             0);
}

void debug_wrap_text(s32* x, s32* y) {
    *x += 8;
    if (*x >= 296) {
        *x = 20;
        *y += 8;
    }
}

void debug_print_string(s32* x, s32* y, char* arg2) {
    *x += 20;
    *y += 20;

    while (*arg2 != '\0') {
        if (D_800E5628[(s32) *arg2] >= 0) {
            func_800573E4(*x, *y, D_800E5628[(s32) *arg2]);
        }
        debug_wrap_text(x, y);
        arg2++;
    }
}

void debug_print_number(s32* x, s32* y, s32 number, u32 numDigits) {
    s32 n;
    s8* ptr;
    s8 remainder;

    debug_wrap_text(x, y);
    n = number;
    if (n < 0) {
        func_800573E4(*x, *y, D_800E5628[0x2D]);
        debug_wrap_text(x, y);
        n = -number;
    }

    *D_801657B8 = -1;
    ptr = D_801657B8;
    if (n != 0) {
        while (n != 0) {
            remainder = n % numDigits;
            *++ptr = remainder;
            n = n / numDigits;
        }
    } else {
        *++ptr = 0;
    }

    do {
        func_800573E4(*x, *y, *ptr--);
        debug_wrap_text(x, y);
    } while (*ptr != -1);
}

/**
 * 801657B8[] does nothing? 0xFF a mask?
 * Index zero is a null/0xFF flag.
 * The other indexes increment 0-9
 * The final index (10) increments the tenth digit.
 */
void func_8005762C(s32* x, s32* y, s32 pathCount, u32 numDigits) {
    s8* ptr;
    s32 count;
    s8 remainder;

    debug_wrap_text(x, y);
    *D_801657B8 = -1;
    ptr = D_801657B8;
    count = pathCount;
    if (count != 0) {
        while (count != 0) {
            // Retrives ones digit (31 outputs 1).
            remainder = count % numDigits;
            *++ptr = remainder;
            // Retrieves tens digit (31 outputs 3).
            count = count / numDigits;
        }
    } else {
        *++ptr = 0;
    }

    do {
        func_800573E4(*x, *y, *ptr--);
        debug_wrap_text(x, y);
    } while (*ptr != -1);
}

UNUSED void func_80057708() {
}

void load_debug_font(void) {
    gSPDisplayList(gDisplayListHead++, D_0D008108);
    gSPDisplayList(gDisplayListHead++, D_0D008080);
    gDPSetAlphaCompare(gDisplayListHead++, G_AC_THRESHOLD);
}

void func_80057778(void) {
    gSPDisplayList(gDisplayListHead++, D_0D007EB8);
}

void debug_print_str2(s32 xPos, s32 yPos, char* str) {
    debug_print_string(&xPos, &yPos, str);
}

void print_str_num(s32 arg0, s32 arg1, char* arg2, s32 arg3) {
    debug_print_string(&arg0, &arg1, arg2);
    debug_print_number(&arg0, &arg1, arg3, 10);
}

UNUSED void func_80057814(s32 arg0, s32 arg1, char* arg2, u32 arg3) {
    debug_print_string(&arg0, &arg1, arg2);
    func_8005762C(&arg0, &arg1, arg3, 10);
}

UNUSED void func_80057858(s32 arg0, s32 arg1, char* arg2, u32 arg3) {
    debug_print_string(&arg0, &arg1, arg2);
    debug_print_number(&arg0, &arg1, arg3, 16);
    func_800573E4(arg0, arg1, D_800E5628[0x48]);
}

UNUSED void func_800578B0(s32 arg0, s32 arg1, char* arg2, u32 arg3) {
    debug_print_string(&arg0, &arg1, arg2);
    func_8005762C(&arg0, &arg1, arg3, 16);
    func_800573E4(arg0, arg1, D_800E5628[0x48]);
}

UNUSED void func_80057908(s32 arg0, s32 arg1, char* arg2, u32 arg3) {
    debug_print_string(&arg0, &arg1, arg2);
    debug_print_number(&arg0, &arg1, arg3, 2);
    func_800573E4(arg0, arg1, D_800E5628[0x42]);
}

UNUSED void func_80057960(s32 arg0, s32 arg1, char* arg2, u32 arg3) {
    debug_print_string(&arg0, &arg1, arg2);
    func_8005762C(&arg0, &arg1, arg3, 2);
    func_800573E4(arg0, arg1, D_800E5628[0x42]);
}

UNUSED void func_800579B8(s32 arg0, s32 arg1, char* arg2) {
    load_debug_font();
    debug_print_string(&arg0, &arg1, arg2);
    func_80057778();
}

void func_800579F8(s32 arg0, s32 arg1, char* arg2, u32 arg3) {
    load_debug_font();
    debug_print_string(&arg0, &arg1, arg2);
    debug_print_number(&arg0, &arg1, arg3, 10);
    func_80057778();
}

void func_80057A50(s32 x, s32 y, char* str, u32 arg3) {
    load_debug_font();
    debug_print_string(&x, &y, str);
    func_8005762C(&x, &y, arg3, 10);
    func_80057778();
}

UNUSED void func_80057AA8(s32 arg0, s32 arg1, char* arg2, u32 arg3) {
    load_debug_font();
    debug_print_string(&arg0, &arg1, arg2);
    debug_print_number(&arg0, &arg1, arg3, 16);
    func_800573E4(arg0, arg1, D_800E5628[0x48]);
    func_80057778();
}

UNUSED void func_80057B14(s32 arg0, s32 arg1, char* arg2, u32 arg3) {
    load_debug_font();
    debug_print_string(&arg0, &arg1, arg2);
    func_8005762C(&arg0, &arg1, arg3, 16);
    func_800573E4(arg0, arg1, D_800E5628[0x48]);
    func_80057778();
}

UNUSED void func_80057B80(s32 arg0, s32 arg1, char* arg2, u32 arg3) {
    load_debug_font();
    debug_print_string(&arg0, &arg1, arg2);
    debug_print_number(&arg0, &arg1, arg3, 2);
    func_800573E4(arg0, arg1, D_800E5628[0x42]);
    func_80057778();
}

UNUSED void func_80057BEC(s32 arg0, s32 arg1, char* arg2, u32 arg3) {
    load_debug_font();
    debug_print_string(&arg0, &arg1, arg2);
    func_8005762C(&arg0, &arg1, arg3, 2);
    func_800573E4(arg0, arg1, D_800E5628[0x42]);
    func_80057778();
}
