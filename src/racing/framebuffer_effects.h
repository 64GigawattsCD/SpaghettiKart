#ifndef FRAMEBUFFER_EFFECTS_H
#define FRAMEBUFFER_EFFECTS_H

#include <libultraship.h>

extern s32 gReusableFrameBuffer;
extern s32 gN64ResFrameBuffer;
extern s32 gArcadeKartPostFxSceneFrameBuffer;
extern s32 gArcadeKartPostFxHudFrameBuffer;

void FB_CreateFramebuffers(void);
void FB_CopyToFramebuffer(Gfx** gfxP, s32 fb_src, s32 fb_dest, u8 oncePerFrame, u8* hasCopied);
void FB_WriteFramebufferSliceToCPU(Gfx** gfxP, void* buffer, u8 byteSwap);
void FB_DrawFromFramebuffer(Gfx** gfxP, s32 fb, u8 alpha);
void FB_DrawFromFramebufferScaled(Gfx** gfxP, s32 fb, u8 alpha, float scaleX, float scaleY);
s32 FB_ArcadeKartPostFxShouldLayerHud(void);
void FB_ArcadeKartPostFxBeginScene(Gfx** gfxP);
void FB_ArcadeKartPostFxEndScene(Gfx** gfxP);
void FB_ArcadeKartPostFxMarkSceneReady(void);
void FB_ArcadeKartPostFxBeginHud(Gfx** gfxP);
void FB_ArcadeKartPostFxEndHud(Gfx** gfxP);

#endif // FRAMEBUFFER_EFFECTS_H
