// ============================================
// NEW CODE - ALBW Port (enemy-death rupee "+n" HUD popup)
//
// Full port of the fork's d_albw_rupee_popup.cpp. The mod previously hand-wrote
// this inside enemy_rupees.cpp and it never rendered: the '+' texture's filename
// contains a full-width 'x' whose byte encoding does not match the archive string
// table on PC, so the by-name lookup returns NULL, ensurePics() failed every
// frame, and the popup silently never drew (the counter still worked because the
// GRANT path is separate). The fork documents this exact trap and falls back to
// the archive file INDEX (dRes_INDEX_MAIN2D_BTI_IM_PLUS_METAL_24X24_00_e), which
// is encoding-proof. Reproduced here.
//
// The fork renders through dMeter2Draw_c helper methods it added
// (getRupeeAnchorCenter / getRupeeDigitMetrics / getRupeeHudAlphaRate /
// getRupeeHudReferenceSize). Those are not in stock dMeter2Draw_c, so they are
// reproduced here as free helpers reading the stock rupee-HUD panes directly
// (#define private public). The fork's inserted 10000s digit pane
// (mpRupeeTenThousand) is a fork-only HUD addition absent from stock, so that
// clamp is dropped — the mod's counter tops out at the vanilla digit set.
// ============================================

#include "global.h"
#include <cmath>

#include "albw_common.h"
#include "config_vars.h"
#include "modules.h"

#include "d/d_com_inf_game.h"
#include "d/d_meter2.h"
#include "d/d_meter2_info.h"
#include "d/d_meter_HIO.h"
#include "JSystem/J2DGraph/J2DGrafContext.h"
#include "JSystem/J2DGraph/J2DPicture.h"
#include "JSystem/JKernel/JKRArchive.h"
#include "JSystem/JKernel/JKRHeap.h"
#include "mods/svc/hook.hpp"
#include "res/Layout/main2D.h"

#define private public
#include "d/d_meter2_draw.h"
#include "d/d_pane_class.h"
#undef private

#if TARGET_PC

namespace {

constexpr s16 POPUP_FRAMES       = 120;
constexpr s16 POPUP_FADE_FRAMES  = 15;
constexpr f32 POPUP_COUNTER_GAP  = 1.6f;
constexpr f32 POPUP_PLUS_RATIO   = 0.8f;
constexpr int POPUP_MAX_DIGITS   = 4;

J2DPicture* sPlusPic = NULL;
J2DPicture* sDigitPic[10] = {NULL};
u16 sAmount = 0;
s16 sFramesLeft = 0;

DEFINE_HOOK(&dMeter2Draw_c::draw, RupeePopupMeterDraw);

bool enabled() {
    return albw_cfg_bool(g_kill_rupees, true);
}

bool ensurePics() {
    if (sPlusPic != NULL) {
        return true;
    }
    JKRArchive* arc = g_dComIfG_gameInfo.play.getMain2DArchive();
    if (arc == NULL) {
        return false;
    }
    for (int i = 0; i < 10; i++) {
        if (sDigitPic[i] != NULL) {
            continue;
        }
        ResTIMG* timg =
            static_cast<ResTIMG*>(arc->getResource('TIMG', dMeter2Info_getNumberTextureName(i)));
        if (timg == NULL) {
            return false;
        }
        sDigitPic[i] = JKR_NEW J2DPicture(timg);
        if (sDigitPic[i] == NULL) {
            return false;
        }
    }
    // The '+' filename's full-width 'x' fails the by-name lookup on PC; fall back
    // to the archive file index (encoding-proof), exactly as the fork does.
    ResTIMG* plusTimg =
        static_cast<ResTIMG*>(arc->getResource('TIMG', dMeter2Info_getPlusTextureName()));
    if (plusTimg == NULL) {
        plusTimg = static_cast<ResTIMG*>(
            arc->getIdxResource(dRes_INDEX_MAIN2D_BTI_IM_PLUS_METAL_24X24_00_e));
    }
    if (plusTimg == NULL) {
        return false;
    }
    sPlusPic = JKR_NEW J2DPicture(plusTimg);
    return sPlusPic != NULL;
}

// --- fork dMeter2Draw_c::getRupeeAnchorCenter, reproduced ---------------------
bool getRupeeAnchorCenter(dMeter2Draw_c* d, Vec* o_center) {
    if (o_center == NULL || d->mpRupeeParent[0] == NULL ||
        d->mpRupeeParent[0]->getPanePtr() == NULL) {
        return false;
    }
    *o_center = d->mpRupeeParent[0]->getGlobalVtxCenter(false, 0);
    return true;
}

// --- fork getRupeeHudAlphaRate, reproduced -----------------------------------
f32 getRupeeHudAlphaRate(dMeter2Draw_c* d) {
    if (d->mpRupeeParent[0] == NULL) {
        return 0.0f;
    }
    return d->mpRupeeParent[0]->getAlphaRate();
}

// --- fork getRupeeHudReferenceSize, reproduced -------------------------------
f32 getRupeeHudReferenceSize(dMeter2Draw_c* d) {
    if (d->mpRupeeParent[0] == NULL || d->mpRupeeParent[0]->getPanePtr() == NULL) {
        return 0.0f;
    }
    Mtx m;
    J2DPane* pane = d->mpRupeeParent[0]->getPanePtr();
    const Vec v0 = d->mpRupeeParent[0]->getGlobalVtx(pane, &m, 0, false, 0);
    const Vec v3 = d->mpRupeeParent[0]->getGlobalVtx(pane, &m, 3, false, 0);
    const f32 w = std::fabs(v3.x - v0.x);
    const f32 h = std::fabs(v3.y - v0.y);
    if (h < 1.0f) {
        return 0.0f;
    }
    if (w > h * 2.5f) {
        return h;
    }
    return (w < h) ? w : h;
}

// --- fork getRupeeDigitMetrics, reproduced (minus the fork-only 10000s pane) --
bool getRupeeDigitMetrics(dMeter2Draw_c* d, f32* o_width, f32* o_height, f32* o_advance,
                          f32* o_leftCenterX, f32* o_centerY) {
    if (d->mpRupeeTexture[0][1] == NULL || d->mpRupeeTexture[0][1]->getPanePtr() == NULL ||
        d->mpRupeeTexture[1][1] == NULL || d->mpRupeeTexture[1][1]->getPanePtr() == NULL) {
        return false;
    }
    Mtx m;
    J2DPane* pane0 = d->mpRupeeTexture[0][1]->getPanePtr();
    const Vec a0 = d->mpRupeeTexture[0][1]->getGlobalVtx(pane0, &m, 0, false, 0);
    const Vec a3 = d->mpRupeeTexture[0][1]->getGlobalVtx(pane0, &m, 3, false, 0);
    const Vec c0 = d->mpRupeeTexture[0][1]->getGlobalVtxCenter(false, 0);
    const Vec c1 = d->mpRupeeTexture[1][1]->getGlobalVtxCenter(false, 0);

    *o_width   = std::fabs(a3.x - a0.x);
    *o_height  = std::fabs(a3.y - a0.y);
    *o_advance = std::fabs(c1.x - c0.x);

    f32 leftX = c0.x;
    f32 centerY = c0.y;
    for (int i = 1; i < 4; i++) {
        if (d->mpRupeeTexture[i][1] == NULL || d->mpRupeeTexture[i][1]->getPanePtr() == NULL) {
            continue;
        }
        const Vec c = d->mpRupeeTexture[i][1]->getGlobalVtxCenter(false, 0);
        if (c.x < leftX) {
            leftX = c.x;
        }
    }
    *o_leftCenterX = leftX;
    *o_centerY = centerY;
    return *o_width > 0.5f && *o_height > 0.5f && *o_advance > 0.5f;
}

// --- fork dAlbwRupeePopup_draw, reproduced -----------------------------------
void draw() {
    if (sFramesLeft <= 0) {
        return;
    }
    if (!enabled()) {
        sFramesLeft = 0;
        return;
    }
    if (!ensurePics()) {
        return;
    }

    dMeter2_c* meter = g_meter2_info.getMeterClass();
    dMeter2Draw_c* meterDraw = (meter != NULL) ? meter->getMeterDrawPtr() : NULL;
    if (meterDraw == NULL) {
        return;
    }

    // Anchor: the green rupee icon (HIO fallback).
    Vec iconCenter;
    f32 iconSize;
    if (getRupeeAnchorCenter(meterDraw, &iconCenter)) {
        iconSize = getRupeeHudReferenceSize(meterDraw);
    } else {
        iconCenter.x = g_drawHIO.mRupeePosX;
        iconCenter.y = g_drawHIO.mRupeePosY;
        iconCenter.z = 0.0f;
        iconSize = 0.0f;
    }
    if (iconSize <= 0.0f) {
        iconSize = g_drawHIO.mRupeeScale * 24.0f;
    }

    // Glyph metrics: exact size/advance of the live counter digits.
    f32 digitW, digitH, advance, digitLeftX, digitRowY;
    if (!getRupeeDigitMetrics(meterDraw, &digitW, &digitH, &advance, &digitLeftX, &digitRowY)) {
        digitW = digitH = iconSize * 0.8f;
        advance = digitW * 0.6f;
        digitLeftX = iconCenter.x - advance * 5.0f;
        digitRowY = iconCenter.y;
    }

    // Follow the rupee counter's own fade — hold (don't burn) the window while
    // the HUD is hidden (cutscenes).
    f32 hudAlphaRate = getRupeeHudAlphaRate(meterDraw);
    if (hudAlphaRate <= 0.0f) {
        return;
    }
    if (hudAlphaRate > 1.0f) {
        hudAlphaRate = 1.0f;
    }

    sFramesLeft--;

    f32 fade = 1.0f;
    if (sFramesLeft < POPUP_FADE_FRAMES) {
        fade = (f32)sFramesLeft / (f32)POPUP_FADE_FRAMES;
    }
    const u8 alpha = (u8)(255.0f * fade * hudAlphaRate);
    if (alpha == 0) {
        return;
    }

    J2DGrafContext* grafCtx = g_dComIfG_gameInfo.play.getCurrentGrafPort();
    if (grafCtx == NULL) {
        return;
    }
    grafCtx->setup2D();

    int digits[POPUP_MAX_DIGITS];
    int digitCount = 0;
    {
        u16 value = sAmount;
        do {
            digits[digitCount++] = value % 10;
            value /= 10;
        } while (value != 0 && digitCount < POPUP_MAX_DIGITS);
    }

    const int glyphCount = 1 + digitCount;
    f32 posX = digitLeftX - advance * (POPUP_COUNTER_GAP + (f32)(glyphCount - 1));
    const f32 centerY = digitRowY;
    const f32 plusSize = digitH * POPUP_PLUS_RATIO;

    sPlusPic->setAlpha(alpha);
    sPlusPic->draw(posX - plusSize * 0.5f, centerY - plusSize * 0.5f, plusSize, plusSize, false,
                   false, false);
    posX += advance;

    for (int i = digitCount - 1; i >= 0; i--) {
        J2DPicture* pic = sDigitPic[digits[i]];
        pic->setAlpha(alpha);
        pic->draw(posX - digitW * 0.5f, centerY - digitH * 0.5f, digitW, digitH, false, false,
                  false);
        posX += advance;
    }
}

void on_meter_draw_post(ModContext*, void*, void*, void*) {
    draw();
}

}  // namespace

// fork dAlbwEnemyRupeesHud_onGrant — arm/replace the popup.
void albw_rupee_popup_on_grant(u16 amount) {
    if (amount == 0) {
        return;
    }
    sAmount = amount;
    sFramesLeft = POPUP_FRAMES;
}

ModResult albw_rupee_popup_init(ModError*) {
    if (mods::hook::add_post<RupeePopupMeterDraw>(on_meter_draw_post) != MOD_OK) {
        svc_log->error(mod_ctx, "failed to hook dMeter2Draw_c::draw (rupee popup)");
        return MOD_ERROR;
    }
    return MOD_OK;
}

// ---- Shared digit rendering (reused by the per-enemy HP bar's "n/t" label) ----
// Digit textures are square (im_font_number_32_32); advance 0.6*height packs them
// like the live counter. Width extent = height + advance*(digits-1).
namespace {
int uintDigitCount(u32 value) {
    int n = 0;
    u32 v = value;
    do {
        n++;
        v /= 10;
    } while (v != 0 && n < 10);
    return n;
}
}  // namespace

float albw_rupee_popup_uint_width(unsigned value, float digitH) {
    const float advance = digitH * 0.6f;
    return digitH + advance * static_cast<float>(uintDigitCount(value) - 1);
}

float albw_rupee_popup_draw_uint(unsigned value, float leftX, float cy, float digitH,
                                 unsigned char alpha) {
    if (digitH <= 0.0f || !ensurePics()) {
        return 0.0f;
    }
    const float digitW = digitH;
    const float advance = digitH * 0.6f;

    int digits[10];
    int count = 0;
    u32 v = value;
    do {
        digits[count++] = static_cast<int>(v % 10);
        v /= 10;
    } while (v != 0 && count < 10);

    float posX = leftX + digitW * 0.5f;
    for (int i = count - 1; i >= 0; i--) {
        J2DPicture* pic = sDigitPic[digits[i]];
        if (pic != NULL) {
            pic->setAlpha(alpha);
            pic->draw(posX - digitW * 0.5f, cy - digitH * 0.5f, digitW, digitH, false, false, false);
        }
        posX += advance;
    }
    return digitW + advance * static_cast<float>(count - 1);
}

#endif  // TARGET_PC

// ============================================
// NEW CODE ENDS HERE
// ============================================
