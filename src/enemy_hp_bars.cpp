// ============================================
// NEW CODE - Per-enemy floating HP bars (optional; mod-original).
//
// An optional setting that reuses the boss health-bar rendering, but places a
// small bar in world at each regular enemy's head — at eyePos, where the Z-lock
// attention marker sits. Above the bar it prints "n/t" (current / current-max
// health) using the rupee-counter digit textures (albw_rupee_popup_draw_uint), so
// the region-HP multipliers are actually visible. Bosses are skipped (they own the
// dedicated boss bar). Solo feature: LazyTweaks ships no health-bar mod to defer to.
//
// World->screen via mDoLib_project (dst.x/y = framebuffer screen coords; dst.z > 0
// means behind the camera). Drawing rides the meter-draw-post 2D seam inside the
// same ortho the boss bar sets up.
// ============================================

#include "enemy_hp_bars.h"

#include "albw_common.h"
#include "boss_hp_hud.h"
#include "config_vars.h"
#include "modules.h"

#include "d/d_com_inf_game.h"
#include "f_op/f_op_actor.h"
#include "f_op/f_op_actor_iter.h"
#include "f_pc/f_pc_name.h"
#include "m_Do/m_Do_graphic.h"
#include "m_Do/m_Do_lib.h"

#include "JSystem/J2DGraph/J2DGrafContext.h"
#include "JSystem/J2DGraph/J2DOrthoGraph.h"
#include "JSystem/J2DGraph/J2DPicture.h"
#include "JSystem/JKernel/JKRArchive.h"
#include "JSystem/JKernel/JKRHeap.h"
#include "JSystem/JUtility/TColor.h"


#if TARGET_PC

namespace {

// Bar geometry as fractions of screen; numbers sit just above the bar.
constexpr f32 kBarWidthFrac = 0.05f;
constexpr f32 kBarHeightFrac = 0.008f;
constexpr f32 kBarBorder = 1.0f;
constexpr f32 kDigitHeightFrac = 0.024f;
constexpr f32 kMaxDrawDist = 6000.0f;  // skip far enemies to cut clutter

const char* kBlockBti = "tt_block8x8.bti";

// Boss-bar palette (boss_hp_hud.cpp) reused verbatim for a consistent look.
constexpr u8 kBorder[4] = {32, 32, 36, 255};
constexpr u8 kBg[4] = {18, 18, 22, 200};
constexpr u8 kFill[4] = {176, 8, 8, 255};
constexpr u8 kSlash[4] = {230, 230, 235, 255};

J2DPicture* sBlock = nullptr;

f32 sMinX = 0.0f;
f32 sMinY = 0.0f;
f32 sScrW = 0.0f;
f32 sScrH = 0.0f;

bool ensureResources() {
    if (sBlock == nullptr) {
        JKRArchive* arc = g_dComIfG_gameInfo.play.getMsgArchive(5);
        if (arc == nullptr) {
            return false;
        }
        ResTIMG* timg = static_cast<ResTIMG*>(arc->getResource('TIMG', kBlockBti));
        if (timg == nullptr) {
            return false;
        }
        sBlock = JKR_NEW J2DPicture(timg);
    }
    return sBlock != nullptr;
}

void drawRect(f32 x, f32 y, f32 w, f32 h, const u8 c[4]) {
    if (w <= 0.0f || h <= 0.0f) {
        return;
    }
    sBlock->setBlackWhite(JUtility::TColor(0, 0, 0, 0), JUtility::TColor(c[0], c[1], c[2], c[3]));
    sBlock->setAlpha(c[3]);
    sBlock->draw(x, y, w, h, false, false, false);
}

// A pixel-stair "/" between the two numbers (no slash glyph in the digit set).
void drawSlash(f32 leftX, f32 cy, f32 digitH) {
    const f32 s = digitH * 0.26f;
    const f32 spanX = digitH * 0.55f - s;
    const f32 spanY = digitH - s;
    for (int k = 0; k < 4; k++) {
        const f32 t = static_cast<f32>(k) / 3.0f;
        const f32 x = leftX + spanX * t;
        const f32 y = (cy + (digitH * 0.5f - s)) - spanY * t;
        drawRect(x, y, s, s, kSlash);
    }
}

void* judgeEnemy(void* i_actor, void* i_data) {
    fopAc_ac_c* actor = static_cast<fopAc_ac_c*>(i_actor);
    if (actor == NULL || !fopAcM_IsActor(actor)) {
        return NULL;
    }
    if (fopAcM_GetGroup(actor) != fopAc_ENEMY_e || actor->health <= 0) {
        return NULL;
    }
    if (albw_boss_hp_is_boss_profile(fopAcM_GetName(actor))) {
        return NULL;  // bosses use the dedicated bar
    }

    fopAc_ac_c* link = dComIfGp_getPlayer(0);
    if (link != NULL && actor->current.pos.abs(link->current.pos) > kMaxDrawDist) {
        return NULL;
    }

    // Max HP = field_0x560 (the fork's own max source; scaled alongside health by
    // dAlbwHP_tryApplyTrueMaxHp), falling back to current health. Matches
    // dAlbwHP_getLockonDisplayHp's non-boss path.
    const s16 peak = actor->field_0x560 > 0 ? actor->field_0x560 : actor->health;
    if (peak <= 0) {
        return NULL;
    }

    // Anchor at eyePos (the Z-lock attention point); fall back to a head offset.
    cXyz anchor = actor->eyePos;
    if (anchor.x == 0.0f && anchor.y == 0.0f && anchor.z == 0.0f) {
        anchor = actor->current.pos;
        anchor.y += 90.0f;
    }
    cXyz proj;
    mDoLib_project(&anchor, &proj);
    if (proj.z > 0.0f) {
        return NULL;  // behind camera
    }
    if (proj.x < sMinX || proj.x > sMinX + sScrW || proj.y < sMinY || proj.y > sMinY + sScrH) {
        return NULL;  // off-screen
    }

    // ---- bar (boss-bar style) centered on the anchor ----
    const f32 barW = sScrW * kBarWidthFrac;
    const f32 barH = sScrH * kBarHeightFrac;
    const f32 barX = proj.x - barW * 0.5f;
    const f32 barY = proj.y;

    f32 fill = static_cast<f32>(actor->health) / static_cast<f32>(peak);
    if (fill < 0.0f) {
        fill = 0.0f;
    } else if (fill > 1.0f) {
        fill = 1.0f;
    }

    drawRect(barX - kBarBorder, barY - kBarBorder, barW + kBarBorder * 2.0f,
             barH + kBarBorder * 2.0f, kBorder);
    drawRect(barX, barY, barW, barH, kBg);
    drawRect(barX, barY, barW * fill, barH, kFill);

    // ---- "n/t" numbers above the bar (rupee-counter digit style) ----
    const u32 cur = static_cast<u32>(actor->health);
    const u32 mx = static_cast<u32>(peak);
    const f32 digitH = sScrH * kDigitHeightFrac;
    const f32 gap = digitH * 0.14f;
    const f32 slashW = digitH * 0.55f;
    const f32 wN = albw_rupee_popup_uint_width(cur, digitH);
    const f32 wT = albw_rupee_popup_uint_width(mx, digitH);
    const f32 total = wN + gap + slashW + gap + wT;
    const f32 startX = proj.x - total * 0.5f;
    const f32 numCy = barY - gap - digitH * 0.5f;

    f32 x = startX;
    x += albw_rupee_popup_draw_uint(cur, x, numCy, digitH, 255);
    x += gap;
    drawSlash(x, numCy, digitH);
    x += slashW + gap;
    albw_rupee_popup_draw_uint(mx, x, numCy, digitH, 255);
    return NULL;
}

}  // namespace

void albw_enemy_hp_bars_draw() {
    if (!albw_cfg_bool(g_enemy_hp_bars, false)) {
        return;
    }
    if (!ensureResources()) {
        return;
    }
    J2DGrafContext* gfx = g_dComIfG_gameInfo.play.getCurrentGrafPort();
    if (gfx == nullptr) {
        return;
    }

    sMinX = mDoGph_gInf_c::getMinXF();
    sMinY = mDoGph_gInf_c::getMinYF();
    sScrW = mDoGph_gInf_c::getWidthF();
    sScrH = mDoGph_gInf_c::getHeightF();

    J2DOrthoGraph* ortho = static_cast<J2DOrthoGraph*>(gfx);
    ortho->setOrtho(sMinX, sMinY, sScrW, sScrH, -1.0f, 1.0f);
    ortho->setup2D();
    ortho->setPort();

    fopAcIt_Judge(judgeEnemy, NULL);
}

#endif  // TARGET_PC
