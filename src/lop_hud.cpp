// LoP HUD relayout — part of dev.albt.albw. Port of the fork's game.lopHud.
// See lop_hud.h for the fork-site -> hook-target map.

#include "lop_hud.h"

#include "albw_common.h"
#include "config_vars.h"
#include "albw_game.h"
#include "modules.h"

#include "d/actor/d_a_player.h"
#include "d/d_com_inf_game.h"
#include "d/d_pane_class.h"
#include "d/d_meter_HIO.h"
#include "d/d_save.h"
#include "parry_master.h"
#include "lop_item_belt.h"
#include "focused_arts.h"
#include "d/d_item_data.h"
#include "JSystem/JKernel/JKRArchive.h"
#include "JSystem/J2DGraph/J2DPane.h"
#include "d/d_meter2_draw.h"
#include "d/d_meter2.h"
#include "mods/svc/hook.hpp"

#include <algorithm>

namespace albw_lop {

namespace {

// ============================================
// NEW CODE - ALBW Port (Lies of Link HUD)
// Fork tuning constants, copied verbatim from d_meter2_draw.cpp:123-138.
// ============================================
constexpr f32 kLopSpurAnchorOffX = 36.0f;   // first shield centre, right of Midna
constexpr f32 kLopSpurAnchorOffY = -16.0f;  // lift to sit even with the Midna icon row
constexpr f32 kLopLifeFallbackOffY = 360.0f;
constexpr f32 kLopHealthBarOffX = 0.0f;         // X vs the ALBW meter
constexpr f32 kLopHealthBarAbovePx = 24.0f;     // lift above the ALBW meter
constexpr f32 kLopHealthBarWidthScale = 1.40f;  // bar span vs the meter init width
constexpr f32 kLopSwordSlotOffX = 44.0f;
constexpr f32 kLopSwordSlotOffY = 0.0f;
constexpr f32 kLopButtonRaiseY = 64.0f;
constexpr f32 kFaSegGap = 3.0f;
constexpr f32 kFaSegWidthScale = 0.5f;

bool s_active = false;
bool s_healthBar = false;
bool s_anchorValid = false;
Vec s_anchor = {0.0f, 0.0f, 0.0f};
bool s_lifeHidden = false;
// exec() carries the ring-vs-main status; draw() does not, so keep the last one.
u32 s_lastExecStatus = 0;
bool s_lastExecStatusValid = false;
// Vanilla rupee position, captured before we first move it, so turning the
// layout off can put it back without waiting for a rupee-count change.
bool s_rupeeHomeValid = false;
f32 s_rupeeHomeX = 0.0f;
f32 s_rupeeHomeY = 0.0f;

void draw_item_belt(dMeter2Draw_c* d);  // defined below, used by on_draw_post
void draw_fa_meter(dMeter2Draw_c* d);   // defined below, used by on_draw_post
void apply_lop_button_ring(dMeter2Draw_c* d, u32 i_status);  // defined below

void hide_pane(CPaneMgr* p) {
    if (p != nullptr && p->getPanePtr() != nullptr) {
        p->hide();
    }
}

void show_pane(CPaneMgr* p) {
    if (p != nullptr && p->getPanePtr() != nullptr) {
        p->show();
    }
}

// Fork draw():867-869 - refresh the two state flags from the setting each frame.
void refresh_mode() {
    const Mode m = mode();
    s_active = m != Mode::Off;
    s_healthBar = m == Mode::HealthBar;
}

// Fork draw():870-890 - cache the shield row anchor. Midna is the real anchor;
// cross and life are fallbacks only, in that order.
void compute_anchor(dMeter2Draw_c* d) {
    s_anchorValid = false;
    if (!s_active || d == nullptr) {
        return;
    }
    if (d->mpButtonMidona != nullptr && d->mpButtonMidona->getPanePtr() != nullptr) {
        s_anchor = d->mpButtonMidona->getGlobalVtxCenter(false, 0);
        s_anchor.x += kLopSpurAnchorOffX;
        s_anchor.y += kLopSpurAnchorOffY;
        s_anchorValid = true;
    } else if (d->mpButtonCrossParent != nullptr &&
               d->mpButtonCrossParent->getPanePtr() != nullptr) {
        s_anchor = d->mpButtonCrossParent->getGlobalVtxCenter(false, 0);
        s_anchor.x += kLopSpurAnchorOffX;
        s_anchor.y += kLopSpurAnchorOffY;
        s_anchorValid = true;
    } else if (d->mpLifeParent != nullptr && d->mpLifeParent->getPanePtr() != nullptr) {
        s_anchor = d->mpLifeParent->getGlobalVtxCenter(false, 0);
        s_anchor.x += kLopSpurAnchorOffX;
        s_anchor.y += kLopLifeFallbackOffY;
        s_anchorValid = true;
    }
}

// Fork draw():907-943 - strip the action ring to the B-sword box (human) or to
// Midna alone (wolf), then re-hide the A/X/Y/Z circles, labels and ring item
// copies. Vanilla's per-frame button draw makes these visible again in the move
// phase, so this must run every frame right before the screen rasterizes.
void strip_button_ring(dMeter2Draw_c* d) {
    const bool wolfForm = albw_game::is_wolf_form();

    if (d->mpButtonParent != nullptr && d->mpButtonParent->getPanePtr() != nullptr) {
        for (J2DPane* c = d->mpButtonParent->getPanePtr()->getFirstChildPane(); c != nullptr;
             c = c->getNextChildPane())
        {
            const u64 tag = c->mInfoTag;
            // Human form keeps the B box + sword and Midna; wolf keeps only Midna.
            const bool keep =
                tag == MULTI_CHAR('midona_n') ||
                (!wolfForm && (tag == MULTI_CHAR('bbtn_n') || tag == MULTI_CHAR('b_text_b')));
            if (!keep) {
                c->hide();
            }
        }
    }

    hide_pane(d->mpButtonA);
    for (int i = 0; i < 3; i++) {
        hide_pane(d->mpButtonXY[i]);
        hide_pane(d->mpBTextXY[i]);
        hide_pane(d->mpTextXY[i]);
    }
    hide_pane(d->mpBTextA);
    hide_pane(d->mpTextA);
    hide_pane(d->mpTextB);
    hide_pane(d->mpItemXY[0]);
    hide_pane(d->mpItemXY[1]);
    for (int i = 0; i < 5; i++) {
        hide_pane(d->mpAText[i]);
    }
}

// Fork draw():991-999 - Health Bar mode replaces the heart row, so hide the life
// parent. Track it so heart visibility is never touched in Off / Vanilla Hearts.
void apply_health_bar_life(dMeter2Draw_c* d) {
    if (s_healthBar) {
        hide_pane(d->mpLifeParent);
        s_lifeHidden = true;
    } else if (s_lifeHidden) {
        show_pane(d->mpLifeParent);
        s_lifeHidden = false;
    }
}


// ============================================
// Fork d_meter2_draw.cpp helpers the health bar needs. Both are fork-only
// methods on dMeter2Draw_c; every member they touch is public in stock, so they
// port as free functions taking the draw object.
// ============================================
void apply_magic_meter_layout_transient(dMeter2Draw_c* d, s16 i_max, s16 i_fill, f32 i_posX,
                                        f32 i_posY, f32 i_widthScale, u8 i_alphaSlot) {
    const f32 frameL = d->mpMagicFrameL->getInitPosX();
    const f32 frameSpan = d->mpMagicFrameR->getInitPosX() - frameL;

    f32 meterW = (f32)i_fill * d->mpMagicMeter->getInitSizeX() / 32.0f;
    const f32 meterH = d->mpMagicMeter->getInitSizeY();
    f32 frameR = frameSpan * ((f32)i_max / 32.0f) + frameL;
    const f32 frameY = d->mpMagicFrameL->getInitPosY();
    f32 baseW = (f32)i_max * d->mpMagicBase->getInitSizeX() / 32.0f;
    const f32 baseH = d->mpMagicBase->getInitSizeY();
    const f32 scale = g_drawHIO.mMagicMeterScale;

    if (i_widthScale != 1.0f) {
        baseW *= i_widthScale;
        meterW *= i_widthScale;
        frameR = frameL + (frameR - frameL) * i_widthScale;
    }

    d->mpMagicMeter->resize(meterW, meterH);
    d->mpMagicFrameR->move(frameR, frameY);
    d->mpMagicBase->resize(baseW, baseH);
    d->mpMagicParent->scale(scale, scale);
    d->mpMagicParent->paneTrans(i_posX, i_posY);
    d->mpMagicParent->setAlphaRate(d->mMeterAlphaRate[i_alphaSlot]);
}

void apply_magic_meter_slot(dMeter2Draw_c* d, u8 i_slot) {
    d->mpMagicMeter->resize(d->field_0x584[i_slot], d->field_0x590[i_slot]);
    d->mpMagicFrameR->move(d->field_0x59c[i_slot], d->field_0x5a8[i_slot]);
    d->mpMagicBase->resize(d->field_0x5b4[i_slot], d->field_0x5c0[i_slot]);
    d->mpMagicParent->scale(d->field_0x5cc[i_slot], d->field_0x5d8[i_slot]);
    d->mpMagicParent->paneTrans(d->field_0x5e4[i_slot], d->field_0x5f0[i_slot]);
}

// Fork drawLopHealthBar() - the LoP crimson life bar that replaces the heart row
// in Health Bar mode, reusing the magic-meter widget. Two passes: dull reclaim
// (Parry Master) underneath, live crimson on top, then slot 0 is restored so the
// shared panes are left exactly as the green ALBW meter drew them.
void draw_lop_health_bar(dMeter2Draw_c* d) {
    if (!s_healthBar) {
        return;
    }
    // Human form gates on ALBW meter alpha. Wolf pins mMeterAlphaRate[0] to 0 on
    // purpose (stamina hidden), so use the life/parent HUD alpha there instead.
    const bool wolfForm = albw_game::is_wolf_form();
    f32 barAlpha = d->mMeterAlphaRate[0];
    if (wolfForm) {
        barAlpha = (d->mpLifeParent != nullptr) ? d->mpLifeParent->getAlphaRate()
                                                : g_drawHIO.mParentAlpha;
    }
    if (barAlpha <= 0.0f) {
        return;
    }
    if (d->mpKanteraScreen == nullptr || d->mpMagicBase == nullptr ||
        d->mpMagicParent == nullptr || d->mpMagicMeter == nullptr ||
        d->mpMagicFrameL == nullptr || d->mpMagicFrameR == nullptr)
    {
        return;
    }
    J2DGrafContext* graf_ctx = albw_game::current_graf_port();
    if (graf_ctx == nullptr) {
        return;
    }

    // Fill against the TRUE full-life basis: drawLife() treats a full heart row as
    // (maxLife/5)*4 life units, so that value fills the bar to exactly 32/32.
    const s16 maxHearts = (s16)(albw_game::max_life_gauge() / 5);
    const s16 fullLife = (s16)(maxHearts * 4);
    s16 life = (s16)albw_game::life();
    if (life < 0) {
        life = 0;
    }
    if (fullLife > 0 && life > fullLife) {
        life = fullLife;
    }
    const s16 fill32 = (fullLife > 0) ? (s16)((s32)life * 32 / fullLife) : 0;

    // Parry Master reclaim: dull-red extension beyond live.
    s16 reclaim = 0;
    if (dParryMaster_isEnabled()) {
        reclaim = (s16)dParryMaster_getRecoverablePieces();
    }
    s16 combined = (s16)(life + reclaim);
    if (fullLife > 0 && combined > fullLife) {
        combined = fullLife;
    }
    const s16 fillCombined32 = (fullLife > 0) ? (s16)((s32)combined * 32 / fullLife) : 0;

    // Position in the ALBW meter local space: same X, lifted above it.
    const f32 barX = d->field_0x5e4[0] + kLopHealthBarOffX;
    const f32 barY = d->field_0x5f0[0] - kLopHealthBarAbovePx;

    const JUtility::TColor liveBlack(90, 18, 18, 255);
    const JUtility::TColor liveWhite(215, 40, 40, 255);
    // Hybrid vs live: ~0.85x luminance + ~0.95 alpha - readable on TP BGs, hard cut.
    const JUtility::TColor dullBlack(77, 15, 15, 255);
    const JUtility::TColor dullWhite(183, 34, 34, 255);
    const f32 dullAlpha = barAlpha * 0.95f;

    if (reclaim > 0 && fillCombined32 > fill32) {
        apply_magic_meter_layout_transient(d, 32, fillCombined32, barX, barY,
                                           kLopHealthBarWidthScale, 0);
        d->mpMagicMeter->setBlackWhite(dullBlack, dullWhite);
        d->mpMagicParent->setAlphaRate(dullAlpha);
        d->setAlphaMagicChange(true);
        d->mpKanteraScreen->draw(0.0f, 0.0f, graf_ctx);
    }

    apply_magic_meter_layout_transient(d, 32, fill32, barX, barY, kLopHealthBarWidthScale, 0);
    d->mpMagicMeter->setBlackWhite(liveBlack, liveWhite);
    d->mpMagicParent->setAlphaRate(barAlpha);
    d->setAlphaMagicChange(true);
    d->mpKanteraScreen->draw(0.0f, 0.0f, graf_ctx);

    // Restore slot 0 (green ALBW meter) so the shared panes are left as drawn.
    JUtility::TColor black = d->mpMagicMeter->getInitBlack();
    black.a = 255;
    d->mpMagicMeter->setBlackWhite(black, d->mpMagicMeter->getInitWhite());
    apply_magic_meter_slot(d, 0);
    d->mpMagicParent->setAlphaRate(d->mMeterAlphaRate[0]);
    d->setAlphaMagicChange(true);
}

DEFINE_HOOK(&dMeter2Draw_c::draw, LopMeterDraw);

// ============================================
// Fork drawRupee():3381 - LoP moves the wallet to the TOP-right corner (the ring
// vacated it) and lifts it onto the heart row. The fork edits rupeeKeyPosY and
// the anchor corner before the final paneTrans; drawRupee ends in that paneTrans,
// so re-applying it from a post-hook is equivalent. The Y delta is measured once
// from the life-vs-rupee gap and held so it cannot oscillate frame to frame.
// ============================================
bool s_rupeeYCached = false;
f32 s_rupeeYOffset = 0.0f;

void apply_lop_rupee(dMeter2Draw_c* d) {
    if (d->mpRupeeKeyParent == nullptr || d->mpRupeeKeyParent->getPanePtr() == nullptr) {
        return;
    }
    if (!s_rupeeHomeValid) {
        // Vanilla's own drawRupee ends in this paneTrans; snapshot it once so the
        // Off path can restore exactly, rather than recomputing vanilla's math.
        s_rupeeHomeX = g_drawHIO.mRupeeKeyPosX;
        s_rupeeHomeY = g_drawHIO.mRupeeKeyPosY;
        s_rupeeHomeValid = true;
    }
    if (!s_active) {
        f32 hx = s_rupeeHomeX;
        f32 hy = s_rupeeHomeY;
        anchor_hud_scale(d->mpRupeeKeyParent, Corner::BottomRight, &hx, &hy);
        d->mpRupeeKeyParent->paneTrans(hx, hy);
        return;
    }
    if (!s_rupeeYCached && d->mpLifeParent != nullptr &&
        d->mpLifeParent->getPanePtr() != nullptr)
    {
        const Vec lifeC = d->mpLifeParent->getGlobalVtxCenter(false, 0);
        const Vec rupeeC = d->mpRupeeKeyParent->getGlobalVtxCenter(false, 0);
        const f32 scl = std::max(d->mpRupeeKeyParent->getScaleY(), 0.001f);
        s_rupeeYOffset = (lifeC.y - rupeeC.y) / scl;
        s_rupeeYCached = true;
    }
    f32 posX = g_drawHIO.mRupeeKeyPosX;
    f32 posY = g_drawHIO.mRupeeKeyPosY + s_rupeeYOffset;
    anchor_hud_scale(d->mpRupeeKeyParent, Corner::TopRight, &posX, &posY);
    d->mpRupeeKeyParent->paneTrans(posX, posY);
}

DEFINE_HOOK(&dMeter2Draw_c::drawButtonCross, LopButtonCross);

HookAction on_draw_pre(ModContext*, void* args, void*, void*) {
    auto* d = mods::arg<dMeter2Draw_c*>(args, 0);
    if (d == nullptr) {
        return HOOK_CONTINUE;
    }
    refresh_mode();
    // ============================================
    // Positioning runs EVERY FRAME from here, not from the drawRupee / exec
    // post-hooks alone. The fork forces a redraw on toggle by setting
    // draw_rupee / draw_cross / the mDoStatus family, but those are locals and
    // private members a mod cannot reach - so a hook that only fires on the
    // game's own redraw cadence would leave the wallet and button ring parked
    // in their vanilla spots until an unrelated change happened to trigger one.
    // Re-applying paneTrans per frame is idempotent and cadence-independent.
    // ============================================
    if (s_lastExecStatusValid) {
        apply_lop_button_ring(d, s_lastExecStatus);
    }
    apply_lop_rupee(d);
    compute_anchor(d);
    apply_health_bar_life(d);
    // Fork gates this on dusk settings game.enableTouchControls (touch builds keep
    // the ring). That is a HOST setting and unreachable from a mod, same as
    // game.hudScale - see the parity note in lop_hud.h. Assuming touch controls
    // off matches every desktop build; a touch player would lose the ring.
    if (s_active) {
        strip_button_ring(d);
    }
    return HOOK_CONTINUE;
}

// Fork draw():1026 calls drawLopHealthBar() mid-draw. A mod cannot splice into
// the middle, so it runs from a post-hook: the function issues its own
// mpKanteraScreen->draw(), so it composites on top of the finished HUD.
void on_draw_post(ModContext*, void* args, void*, void*) {
    auto* d = mods::arg<dMeter2Draw_c*>(args, 0);
    if (d != nullptr) {
        draw_item_belt(d);
        draw_lop_health_bar(d);
        draw_fa_meter(d);
    }
}


// ============================================
// Fork d_meter2_draw.cpp helpers that feed the item belt.
// ============================================

// Fork makeLopBeltIcon() - snapshot an X/Y slot into a belt icon: base texture
// plus tint, the item-data scale, and the optional second composite layer
// (bottle glass etc.) only when that overlay pane is actually visible.
LopBeltIcon make_belt_icon(u8 i_itemNo, CPaneMgr* i_basePane, J2DPicture* i_overlayPane,
                           ResTIMG* i_tex0, ResTIMG* i_tex1) {
    LopBeltIcon icon = {};
    icon.scalePct = 100;
    if (i_itemNo == dItemNo_NONE_e || i_basePane == nullptr ||
        i_basePane->getPanePtr() == nullptr)
    {
        return icon;
    }
    J2DPicture* basePic = static_cast<J2DPicture*>(i_basePane->getPanePtr());
    icon.tex0 = i_tex0;
    icon.black0 = basePic->getBlack();
    icon.white0 = basePic->getWhite();
    icon.scalePct = dItem_data::getTexScale(i_itemNo);
    if (i_overlayPane != nullptr && i_overlayPane->isVisible()) {
        icon.tex1 = i_tex1;
        icon.black1 = i_overlayPane->getBlack();
        icon.white1 = i_overlayPane->getWhite();
    }
    return icon;
}

// Fork lopSwordIcon() - resolve the blade icon through the item-data texture
// index, the same source vanilla uses for the B button. Index-based so every
// blade works (Wooden/Ordon/Master/Light) and no hardcoded switch can drop one.
ResTIMG* sword_icon(u8 i_swordNo) {
    if (i_swordNo == dItemNo_NONE_e) {
        return nullptr;
    }
    JKRArchive* arc = albw_game::item_icon_archive();
    if (arc == nullptr) {
        return nullptr;
    }
    return static_cast<ResTIMG*>(arc->getIdxResource(dItem_data::getTexture(i_swordNo)));
}

// Fork lopFaMeterActive() - does the sword panel extend for the FA strip?
// The fork's wood-stick branch defers to dAlbwOutfitStats_allowsWoodHiddenSkills(),
// which is just dAlbwOutfitStats_isSumoOffensiveKitActive(). Outfit stats are not
// ported to this mod, so that kit can never be active and the branch is false.
bool fa_meter_active() {
    if (!dFocusedArts_isEnabled()) {
        return false;
    }
    daPy_py_c* player = albw_game::link_player();
    if (player == nullptr || player->checkWolf()) {
        return false;
    }
    if (albw_game::select_equip_sword() == dItemNo_WOOD_STICK_e) {
        return false;  // sumo offensive kit absent in this build
    }
    return true;
}

// Fork draw():951-978 - the bottom-left belt: carved slot frames stacked above
// Midna with the live X/Y assignments composited inside, plus the sword panel
// right of the B box. Gated by the button-ring fade and pause so it hides with
// the rest of the HUD, and by wolf form - Wolf Link shows only Midna, charges
// and tears of light, never the human belt.
void draw_item_belt(dMeter2Draw_c* d) {
    if (!s_active || albw_game::is_pause_flag() || d->mpButtonParent == nullptr ||
        d->mpButtonParent->getAlphaRate() <= 0.0f ||
        albw_game::is_wolf_form())
    {
        return;
    }
    J2DGrafContext* graf_ctx = albw_game::current_graf_port();
    if (graf_ctx == nullptr) {
        return;
    }

    if (d->mpButtonMidona != nullptr && d->mpButtonMidona->getPanePtr() != nullptr) {
        const Vec midna = d->mpButtonMidona->getGlobalVtxCenter(false, 0);
        // Query the live assignment: the cached mpItemXYTex is not reset when the
        // ALBW mod strips an item, so a stripped slot must read as empty here.
        const u8 xItem = dComIfGp_getSelectItem(0);
        const u8 yItem = dComIfGp_getSelectItem(1);
        const LopBeltIcon xi =
            make_belt_icon(xItem, d->mpItemXY[0], d->mpItemXYPane[0],
                           d->mpItemXYTex[0][d->field_0x76c[0]][0],
                           d->mpItemXYTex[0][d->field_0x76c[0]][1]);
        const LopBeltIcon yi =
            make_belt_icon(yItem, d->mpItemXY[1], d->mpItemXYPane[1],
                           d->mpItemXYTex[1][d->field_0x76c[1]][0],
                           d->mpItemXYTex[1][d->field_0x76c[1]][1]);
        dAlbwLopItemBelt_draw(graf_ctx, midna.x, midna.y, xi, yi);
    }
    // Sword slot: re-queried every frame so it follows d-pad sword swaps.
    if (d->mpButtonB != nullptr && d->mpButtonB->getPanePtr() != nullptr) {
        const Vec bc = d->mpButtonB->getGlobalVtxCenter(false, 0);
        ResTIMG* swordTex = sword_icon(albw_game::select_equip_sword());
        dAlbwLopItemBelt_drawSword(graf_ctx, bc.x + kLopSwordSlotOffX, bc.y + kLopSwordSlotOffY,
                                   swordTex, fa_meter_active());
    }
}


// ============================================
// Fork exec():778-826 - LoP drops the action button ring from the top-right (now
// the wallet corner) down to the bottom-right, level with the d-pad cross row.
// The lift is measured ONCE from the ring-vs-cross gap and held; the divisor is
// the PARENT scale, because the ring carries its own ~0.55 scale which would
// over-lift, then pulled back up by kLopButtonRaiseY so the B box is not clipped.
//
// Vanilla exec() already positioned the ring at TopRight and wrote mButtonsPosX/Y
// before this post-hook runs, so recompute for BottomRight and re-apply. The
// cached mButtonsPosX/Y guard is honoured so a no-op frame stays a no-op.
// ============================================
bool s_btnLiftCached = false;
f32 s_btnLift = 0.0f;

void apply_lop_button_ring(dMeter2Draw_c* d, u32 i_status) {
    if (!s_active || d->mpButtonParent == nullptr ||
        d->mpButtonParent->getPanePtr() == nullptr)
    {
        return;
    }
    if (!s_btnLiftCached && d->mpButtonCrossParent != nullptr &&
        d->mpButtonCrossParent->getPanePtr() != nullptr)
    {
        const Vec btnC = d->mpButtonParent->getGlobalVtxCenter(false, 0);
        const Vec crossC = d->mpButtonCrossParent->getGlobalVtxCenter(false, 0);
        const f32 parentSc = std::max(g_drawHIO.mParentScale, 0.001f);
        s_btnLift = (crossC.y - btnC.y) / parentSc;
        s_btnLiftCached = true;
    }
    const f32 liftY = s_btnLift - kLopButtonRaiseY;

    f32 posX;
    f32 posY;
    if (i_status & 0x1000000) {
        posX = g_drawHIO.mRingHUDButtonsPosX;
        posY = g_drawHIO.mRingHUDButtonsPosY + liftY;
    } else {
        posX = g_drawHIO.mMainHUDButtonsPosX;
        posY = g_drawHIO.mMainHUDButtonsPosY + liftY;
    }
    anchor_hud_scale(d->mpButtonParent, Corner::BottomRight, &posX, &posY);
    if (d->mButtonsPosX != posX || d->mButtonsPosY != posY) {
        d->mButtonsPosX = posX;
        d->mButtonsPosY = posY;
        d->mpButtonParent->paneTrans(posX, posY);
    }
}


// ============================================
// Fork lopRestoreButtonRing() - undo the LoP sweep when the layout is switched
// off. Vanilla's button redraw does NOT manage the ring vine and decorations, so
// without this they stay hidden until a scene reload.
// ============================================
void restore_button_ring(dMeter2Draw_c* d) {
    if (d->mpButtonParent != nullptr && d->mpButtonParent->getPanePtr() != nullptr) {
        for (J2DPane* c = d->mpButtonParent->getPanePtr()->getFirstChildPane(); c != nullptr;
             c = c->getNextChildPane())
        {
            if (c->mInfoTag != MULTI_CHAR('ju_ring5')) {
                c->show();
            }
        }
    }
    show_pane(d->mpButtonA);
    for (int i = 0; i < 3; i++) {
        show_pane(d->mpButtonXY[i]);
        show_pane(d->mpBTextXY[i]);
        show_pane(d->mpTextXY[i]);
    }
    show_pane(d->mpBTextA);
    show_pane(d->mpTextA);
    show_pane(d->mpTextB);
    // Only re-show an X/Y item pane that actually holds an item - an empty slot has
    // no texture and would flash as a white cube until the next vanilla redraw.
    if (dComIfGp_getSelectItem(0) != dItemNo_NONE_e) {
        show_pane(d->mpItemXY[0]);
    }
    if (dComIfGp_getSelectItem(1) != dItemNo_NONE_e) {
        show_pane(d->mpItemXY[1]);
    }
    for (int i = 0; i < 5; i++) {
        show_pane(d->mpAText[i]);
    }
}

// ============================================
// Fork d_meter2.cpp:1537 / :2874 / :3922 - on a LoP toggle, force the affected
// HUD groups to redraw so they re-apply correct visibility. Without these,
// flipping the setting mid-session leaves orphaned panes behind.
// ============================================
DEFINE_HOOK(&dMeter2_c::_execute, LopMeterExecute);
DEFINE_HOOK(&dMeter2_c::moveRupee, LopMoveRupee);
DEFINE_HOOK(&dMeter2_c::moveButtonCross, LopMoveButtonCross);

bool s_buttonsWasOn = false;
bool s_rupeeWasOn = false;
bool s_crossWasOn = false;

HookAction on_meter_execute_pre(ModContext*, void* args, void*, void*) {
    auto* m = mods::arg<dMeter2_c*>(args, 0);
    if (m == nullptr) {
        return HOOK_CONTINUE;
    }
    const bool lopOn = active();
    if (s_buttonsWasOn != lopOn) {
        s_buttonsWasOn = lopOn;
        // Fork also pokes mDoStatus/mAStatus/mRStatus/mZStatus/mItemStatus[] to 0xFF
        // here, forcing vanilla to redraw each button group. Those are PRIVATE on
        // dMeter2_c and only reachable by hardcoded offset, which would break
        // silently on any host layout change - so they are deliberately not poked.
        // Consequence: after toggling the layout OFF, per-button visibility is
        // restored by restore_button_ring() but not recomputed, so a slot may stay
        // shown until vanilla's next status change corrects it. Visual only.
        dMeter2Draw_c* d = m->getMeterDrawPtr();
        if (d != nullptr) {
            restore_button_ring(d);
        }
    }
    return HOOK_CONTINUE;
}

HookAction on_move_rupee_pre(ModContext*, void*, void*, void*) {
    const bool lopOn = active();
    if (s_rupeeWasOn != lopOn) {
        s_rupeeWasOn = lopOn;
        s_rupeeYCached = false;  // re-measure the offset for the new layout
    }
    return HOOK_CONTINUE;
}

HookAction on_move_button_cross_pre(ModContext*, void*, void*, void*) {
    const bool lopOn = active();
    if (s_crossWasOn != lopOn) {
        s_crossWasOn = lopOn;
        s_btnLiftCached = false;  // re-measure the ring lift for the new layout
    }
    return HOOK_CONTINUE;
}


// ============================================
// Fork getKanteraRootOffset() - the kantera screen local->global root offset
// (corner0 minus the slot-0 paneTrans), measured once. global = local + offset,
// so local = global - offset. Fork caches it in members; here it is a static.
// ============================================
bool s_kanteraOffsetValid = false;
f32 s_kanteraOffsetX = 0.0f;
f32 s_kanteraOffsetY = 0.0f;

void kantera_root_offset(dMeter2Draw_c* d, f32* o_x, f32* o_y) {
    if (!s_kanteraOffsetValid && d->mpMagicParent != nullptr &&
        d->mpMagicParent->getPanePtr() != nullptr)
    {
        Mtx m;
        J2DPane* mp = d->mpMagicParent->getPanePtr();
        const Vec c0 = d->mpMagicParent->getGlobalVtx(mp, &m, 0, false, 0);
        s_kanteraOffsetX = c0.x - d->field_0x5e4[0];
        s_kanteraOffsetY = c0.y - d->field_0x5f0[0];
        s_kanteraOffsetValid = true;
    }
    if (o_x != nullptr) {
        *o_x = s_kanteraOffsetX;
    }
    if (o_y != nullptr) {
        *o_y = s_kanteraOffsetY;
    }
}

// Fork drawFocusedArtsMeterKantera() - the lilac Focused Arts segment strip, drawn
// with the same kantera/magic-meter reuse as durability. Banked segments fill
// opaque, the active one fills partially at half alpha, the rest read empty.
void draw_fa_meter_kantera(dMeter2Draw_c* d, f32 i_localX, f32 i_localY, f32 i_totalW) {
    if (d->mpKanteraScreen == nullptr || d->mpMagicBase == nullptr ||
        d->mpMagicParent == nullptr || d->mpMagicMeter == nullptr ||
        d->mpMagicFrameL == nullptr || d->mpMagicFrameR == nullptr ||
        d->mMeterAlphaRate[0] <= 0.0f)
    {
        return;
    }
    J2DGrafContext* graf_ctx = albw_game::current_graf_port();
    if (graf_ctx == nullptr) {
        return;
    }

    const int maxBank = dFocusedArts_getMaxBank();
    if (maxBank <= 0) {
        return;
    }
    const int bank = dFocusedArts_getBankCount();
    const int den = dFocusedArts_getFillDenominator() > 0 ? dFocusedArts_getFillDenominator() : 1;
    const int num = dFocusedArts_getFillNumerator();

    const f32 baseW = d->mpMagicBase->getInitSizeX();
    const f32 segW = (i_totalW - kFaSegGap * (f32)(maxBank - 1)) / (f32)maxBank;
    if (segW <= 0.0f || baseW <= 0.0f) {
        return;
    }
    const f32 segScale = segW / baseW;

    const JUtility::TColor lilacTrack(30, 22, 56, 255);    // dark purple empty track
    const JUtility::TColor lilacFill(208, 184, 248, 255);  // bright lavender fill
    const JUtility::TColor clear(0, 0, 0, 0);

    for (int i = 0; i < maxBank; i++) {
        const f32 segX = i_localX + (f32)i * (segW + kFaSegGap);

        s16 fill = 0;
        f32 alpha = 1.0f;
        if (i < bank) {
            fill = 32;
        } else if (i == bank && num > 0) {
            int f = num * 32 / den;
            if (f > 32) {
                f = 32;
            }
            fill = (s16)f;
            alpha = 0.5f;
        }

        d->mpMagicBase->setBlackWhite(clear, lilacTrack);
        d->mpMagicMeter->setBlackWhite(clear, lilacFill);
        apply_magic_meter_layout_transient(d, 32, fill, segX, i_localY, segScale, 0);
        // The fill pane (mm_00) is shorter than the tinted track (mm_base), so stretch
        // it to the track height or the dark base shows through below the lilac.
        const f32 fillW = (f32)fill * d->mpMagicMeter->getInitSizeX() / 32.0f * segScale;
        d->mpMagicMeter->resize(fillW, d->mpMagicBase->getInitSizeY());
        d->mpMagicParent->setAlphaRate(d->mMeterAlphaRate[0] * alpha);
        d->setAlphaMagicChange(true);
        d->mpKanteraScreen->draw(0.0f, 0.0f, graf_ctx);
    }

    // Restore slot 0 tints + layout so durability / the next frame are clean.
    JUtility::TColor mblack = d->mpMagicMeter->getInitBlack();
    mblack.a = 255;
    d->mpMagicMeter->setBlackWhite(mblack, d->mpMagicMeter->getInitWhite());
    d->mpMagicBase->setBlackWhite(d->mpMagicBase->getInitBlack(),
                                  d->mpMagicBase->getInitWhite());
    apply_magic_meter_slot(d, 0);
    d->mpMagicParent->setAlphaRate(d->mMeterAlphaRate[0]);
    d->setAlphaMagicChange(true);
}

// Fork drawFocusedArtsMeter() - routes the FA strip: LoP puts it in the sword
// panel bottom strip (global coords, converted to kantera local); vanilla puts it
// under the ALBW meter. No-op when FA is off, so durability keeps its placement.
void draw_fa_meter(dMeter2Draw_c* d) {
    if (!fa_meter_active() || d->mpMagicBase == nullptr) {
        return;
    }
    const int maxBank = dFocusedArts_getMaxBank();
    if (maxBank <= 0) {
        return;
    }
    const f32 segW = d->mpMagicBase->getInitSizeX() * kFaSegWidthScale;
    const f32 totalW = segW * (f32)maxBank + kFaSegGap * (f32)(maxBank - 1);

    if (s_active) {
        if (d->mpButtonB == nullptr || d->mpButtonB->getPanePtr() == nullptr) {
            return;
        }
        const Vec bc = d->mpButtonB->getGlobalVtxCenter(false, 0);
        f32 sLeft, sTop, sW, sH;
        dAlbwLopItemBelt_getSwordFaStrip(bc.x + kLopSwordSlotOffX, bc.y + kLopSwordSlotOffY,
                                         &sLeft, &sTop, &sW, &sH);
        // Shrink the whole group uniformly if it would overflow the sword panel -
        // never stretch a segment past its Ordon-durability cell size.
        f32 useW = totalW;
        if (useW > sW) {
            useW = sW;
        }
        f32 offX, offY;
        kantera_root_offset(d, &offX, &offY);
        draw_fa_meter_kantera(d, sLeft - offX, sTop - offY, useW);
    } else {
        const f32 albwX = d->field_0x5e4[0];
        const f32 albwY = d->field_0x5f0[0];
        const f32 rowLocalY = albwY + d->mpMagicBase->getInitSizeY() * d->field_0x5d8[0] + 8.0f;
        draw_fa_meter_kantera(d, albwX, rowLocalY, totalW);
    }
}

DEFINE_HOOK(&dMeter2Draw_c::exec, LopMeterExec);

void on_exec_post(ModContext*, void* args, void*, void*) {
    auto* d = mods::arg<dMeter2Draw_c*>(args, 0);
    if (d == nullptr) {
        return;
    }
    // exec() runs before draw(), so refresh state here too - otherwise the very
    // first frame after a toggle would position the ring from stale state.
    refresh_mode();
    s_lastExecStatus = mods::arg<u32>(args, 1);
    s_lastExecStatusValid = true;
    apply_lop_button_ring(d, s_lastExecStatus);
}

DEFINE_HOOK(&dMeter2Draw_c::drawRupee, LopDrawRupee);
// drawPikari is overloaded; select the CPaneMgr* form explicitly so the compiler
// mangles it per target and no hardcoded symbol string is needed.
DEFINE_HOOK(static_cast<void (dMeter2Draw_c::*)(CPaneMgr*, f32*, f32, JUtility::TColor,
                                                JUtility::TColor, JUtility::TColor,
                                                JUtility::TColor, f32, u8)>(
                &dMeter2Draw_c::drawPikari),
            LopDrawPikari);

void on_draw_rupee_post(ModContext*, void* args, void*, void*) {
    auto* d = mods::arg<dMeter2Draw_c*>(args, 0);
    if (d != nullptr) {
        apply_lop_rupee(d);
    }
}

// Fork draw():1072 and :1087 - the A and X/Y "pikari" flashes are drawn AFTER the
// screen, bypassing the pane hide, so they would flash where the hidden buttons
// used to be (e.g. on a context A-action like roll). The fork guards each call
// site; a mod cannot, so suppress the call itself for exactly those panes.
// The B flash at :1078 is deliberately NOT suppressed - the B box stays visible.
HookAction on_draw_pikari_pre(ModContext*, void* args, void*, void*) {
    if (!s_active) {
        return HOOK_CONTINUE;
    }
    auto* d = mods::arg<dMeter2Draw_c*>(args, 0);
    auto* pane = mods::arg<CPaneMgr*>(args, 1);
    if (d == nullptr || pane == nullptr) {
        return HOOK_CONTINUE;
    }
    if (pane == d->mpBTextA || pane == d->mpBTextXY[0] || pane == d->mpBTextXY[1]) {
        return HOOK_SKIP_ORIGINAL;
    }
    return HOOK_CONTINUE;
}

// Fork drawButtonCross():4104 - hide the d-pad cross group for a clean
// bottom-left. Midna is a separate screen pane (midona_n) so she survives.
void on_button_cross_post(ModContext*, void* args, void*, void*) {
    auto* d = mods::arg<dMeter2Draw_c*>(args, 0);
    if (d == nullptr || d->mpButtonCrossParent == nullptr ||
        d->mpButtonCrossParent->getPanePtr() == nullptr)
    {
        return;
    }
    if (s_active) {
        d->mpButtonCrossParent->hide();
    } else {
        d->mpButtonCrossParent->show();
    }
}

}  // namespace

Mode mode() {
    const int raw = albw_cfg_int(g_lop_hud_mode, 0);
    return static_cast<Mode>(std::clamp(raw, 0, 2));
}

bool active() {
    return mode() != Mode::Off;
}

bool health_bar() {
    return mode() == Mode::HealthBar;
}

// See the parity note in lop_hud.h: the host's game.hudScale is unreachable from
// a mod, and the anchor correction is a no-op at the default 1.0.
f32 hud_scale() {
    return 1.0f;
}

void anchor_hud_scale(CPaneMgr* pane, Corner corner, f32* io_x, f32* io_y, f32 pull) {
    if (pane == nullptr || io_x == nullptr || io_y == nullptr) {
        return;
    }
    const f32 half = (1.0f - hud_scale()) * 0.5f;
    const f32 dirX = (corner == Corner::TopRight || corner == Corner::BottomRight) ? 1.0f : -1.0f;
    const f32 dirY = (corner == Corner::BottomLeft || corner == Corner::BottomRight) ? 1.0f : -1.0f;
    *io_x += dirX * pane->getInitSizeX() * half * pull;
    *io_y += dirY * pane->getInitSizeY() * half;
}

bool shield_anchor(Vec* o_center) {
    if (!s_active || !s_anchorValid || o_center == nullptr) {
        return false;
    }
    *o_center = s_anchor;
    return true;
}

}  // namespace albw_lop

ModResult albw_lop_hud_init(ModError*) {
    if (mods::hook::add_pre<albw_lop::LopMeterDraw>(albw_lop::on_draw_pre) != MOD_OK) {
        svc_log->error(mod_ctx, "failed to hook dMeter2Draw_c::draw (lop hud)");
        return MOD_ERROR;
    }
    if (mods::hook::add_post<albw_lop::LopMeterDraw>(albw_lop::on_draw_post) != MOD_OK) {
        svc_log->error(mod_ctx, "failed to post-hook dMeter2Draw_c::draw (lop health bar)");
        return MOD_ERROR;
    }
    if (mods::hook::add_pre<albw_lop::LopMeterExecute>(albw_lop::on_meter_execute_pre) != MOD_OK ||
        mods::hook::add_pre<albw_lop::LopMoveRupee>(albw_lop::on_move_rupee_pre) != MOD_OK ||
        mods::hook::add_pre<albw_lop::LopMoveButtonCross>(albw_lop::on_move_button_cross_pre) !=
            MOD_OK)
    {
        svc_log->error(mod_ctx, "failed to hook dMeter2_c toggle transitions (lop hud)");
        return MOD_ERROR;
    }
    if (mods::hook::add_post<albw_lop::LopMeterExec>(albw_lop::on_exec_post) != MOD_OK) {
        svc_log->error(mod_ctx, "failed to hook dMeter2Draw_c::exec (lop hud)");
        return MOD_ERROR;
    }
    if (mods::hook::add_post<albw_lop::LopDrawRupee>(albw_lop::on_draw_rupee_post) != MOD_OK) {
        svc_log->error(mod_ctx, "failed to hook dMeter2Draw_c::drawRupee (lop hud)");
        return MOD_ERROR;
    }
    if (mods::hook::add_pre<albw_lop::LopDrawPikari>(albw_lop::on_draw_pikari_pre) != MOD_OK) {
        svc_log->error(mod_ctx, "failed to hook dMeter2Draw_c::drawPikari (lop hud)");
        return MOD_ERROR;
    }
    if (mods::hook::add_post<albw_lop::LopButtonCross>(albw_lop::on_button_cross_post) != MOD_OK) {
        svc_log->error(mod_ctx, "failed to hook dMeter2Draw_c::drawButtonCross");
        return MOD_ERROR;
    }
    svc_log->info(mod_ctx, "albw LoP HUD hooks ready");
    return MOD_OK;
}

ModResult albw_lop_hud_shutdown(ModError*) {
    dAlbwLopItemBelt_cleanup();
    albw_lop::s_active = false;
    albw_lop::s_healthBar = false;
    albw_lop::s_anchorValid = false;
    albw_lop::s_lifeHidden = false;
    albw_lop::s_rupeeYCached = false;
    albw_lop::s_btnLiftCached = false;
    albw_lop::s_buttonsWasOn = false;
    albw_lop::s_rupeeWasOn = false;
    albw_lop::s_crossWasOn = false;
    albw_lop::s_kanteraOffsetValid = false;
    return MOD_OK;
}
