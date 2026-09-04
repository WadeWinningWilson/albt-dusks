#include "boss_hp_hud.h"

#include "albw_common.h"
#include "albw_game.h"
#include "boss_refinement.h"
#include "config_vars.h"

#include "d/d_attention.h"
#include "d/d_com_inf_game.h"
#include "f_op/f_op_actor.h"
#include "f_pc/f_pc_name.h"
#include "m_Do/m_Do_graphic.h"
#include "JSystem/J2DGraph/J2DGrafContext.h"
#include "JSystem/J2DGraph/J2DOrthoGraph.h"
#include "JSystem/J2DGraph/J2DPicture.h"
#include "JSystem/J2DGraph/J2DTextBox.h"
#include "JSystem/JKernel/JKRArchive.h"
#include "JSystem/JKernel/JKRHeap.h"
#include "JSystem/JUtility/TColor.h"

namespace {

static constexpr f32 kBarWidthFrac = 0.58f;
static constexpr f32 kBarHeightFrac = 0.016f;
static constexpr f32 kBarTopFrac = 0.908f;
static constexpr f32 kBarBorder = 1.0f;
static constexpr f32 kNameFontFrac = 0.028f;
static constexpr f32 kNameGap = 3.0f;
static constexpr f32 kNameOutline = 1.0f;
static const char* kBlockBti = "tt_block8x8.bti";

// ============================================
// NEW CODE - ALBW Port (fork intro-card name styling)
// fork d_albw_boss_hp_hud.cpp D_ALBW_BOSS_BAR_INTRO_STYLE 1: the bar name uses
// the boss-intro title font (mDoExt_getRubyFont, same as zelda_boss_name.blo)
// in the intro-card cream/gold gradient. The previous version here used the
// message font in plain white - the reported wrong-font bar.
// ============================================
static const u32 kNameColorTop = 0xF6E8B0FF;  // cream - intro-card gold, glyph top
static const u32 kNameColorBot = 0xE0B84AFF;  // gold  - intro-card gradient, glyph bottom

static J2DPicture* sBlock = nullptr;
static J2DTextBox* sName = nullptr;

const s16 kBossNames[] = {
    fpcNm_B_BQ_e,  fpcNm_B_GM_e,  fpcNm_E_GM_e,  fpcNm_NPC_KN_e, fpcNm_B_ZANT_e, fpcNm_E_FM_e,
    fpcNm_B_DS_e,  fpcNm_B_DR_e,  fpcNm_B_OB_e,  fpcNm_B_YO_e,   fpcNm_B_GND_e,
    fpcNm_B_MGN_e, fpcNm_E_HZELDA_e,
};

bool isBossProfile(s16 profName) {
    for (s16 name : kBossNames) {
        if (name == profName) {
            return true;
        }
    }
    return false;
}

const char* bossDisplayName(s16 profName) {
    switch (profName) {
    case fpcNm_B_BQ_e:
        return "Twilit Parasite DIABABA";
    case fpcNm_B_GM_e:
    case fpcNm_E_GM_e:
        return "Twilit Arachnid Armogohma";
    case fpcNm_NPC_KN_e:
        return "Hero of Time";
    case fpcNm_B_ZANT_e:
        return "Usurper King Zant";
    case fpcNm_E_FM_e:
        return "Twilit Igniter FYRUS";
    case fpcNm_B_DS_e:
        return "Twilit Fossil Stallord";
    case fpcNm_B_DR_e:
        return "Twilit Dragon Argorok";
    case fpcNm_B_OB_e:
        return "Twilit Aquatic Morpheel";
    case fpcNm_B_YO_e:
        return "Twilit Ice Mass Blizzeta";
    case fpcNm_B_GND_e:
        return "Dark Lord Ganondorf";
    case fpcNm_B_MGN_e:
        return "Dark Beast Ganon";
    case fpcNm_E_HZELDA_e:
        return "Possessed Zelda";
    default:
        return nullptr;
    }
}

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
        if (sBlock == nullptr) {
            return false;
        }
    }
    if (sName == nullptr) {
        sName = JKR_NEW J2DTextBox();
        if (sName == nullptr) {
            return false;
        }
        sName->setFont(mDoExt_getRubyFont());  // fork: boss-intro title font
    }
    return true;
}

void drawRect(f32 x, f32 y, f32 w, f32 h, u8 r, u8 g, u8 b, u8 a = 255) {
    if (w <= 0.0f || h <= 0.0f) {
        return;
    }
    sBlock->setBlackWhite(JUtility::TColor(0, 0, 0, 0), JUtility::TColor(r, g, b, a));
    sBlock->setAlpha(a);
    sBlock->draw(x, y, w, h, false, false, false);
}

fopAc_ac_c* lockOnBossTarget() {
    dAttention_c* attn = albw_game::attention();
    if (attn == nullptr) {
        return nullptr;
    }
    fopAc_ac_c* lock = attn->LockonTarget(0);
    if (lock == nullptr || !fopAcM_IsActor(lock)) {
        return nullptr;
    }
    if (!isBossProfile(fopAcM_GetName(lock))) {
        return nullptr;
    }
    if (lock->health <= 0) {
        return nullptr;
    }
    return lock;
}

}  // namespace

void albw_boss_hp_hud_draw() {
    if (!albw_cfg_bool(g_boss_hp_bars, false)) {
        return;
    }

    // ============================================
    // NEW CODE - ALBW Port (fork dAlbwBossHpHud_draw)
    // Pick the active boss by POLLING each query in priority order, exactly as
    // the fork does. It never consults dAttention_c: arenas never coexist, so
    // whichever query reports a live pool owns the bar.
    //
    // The previous version started from attn->LockonTarget(0) and returned early
    // without a lock, so the bar only appeared while Z-targeting - which is why
    // Fyrus showed nothing. That was a mod-side shortcut, not fork behaviour.
    //
    // Fill is a normalised fillRatio, not raw {current,max}: Armogohma's query
    // composites both phases into fillRatio itself, and the early-out is
    // fillRatio <= 0 so a half-full composite bar (phase 1 empty at 0.5) is
    // never hidden.
    // ============================================
    f32 fillRatio = 0.0f;
    const char* name = nullptr;

    dAlbwBoss_ArmogohmaBarState bar{};
    if (dAlbwBoss_armogohmaQueryHealthBar(&bar) && bar.visible && bar.fillRatio > 0.0f) {
        fillRatio = bar.fillRatio;
        name = bossDisplayName(fpcNm_B_GM_e);
    } else {
        int sCur = 0;
        int sMax = 0;
        if (dAlbwBoss_diababaQueryHealthBar(&sCur, &sMax) && sMax > 0 && sCur > 0) {
            fillRatio = static_cast<f32>(sCur) / static_cast<f32>(sMax);
            name = bossDisplayName(fpcNm_B_BQ_e);
        } else if (dAlbwBoss_zantQueryHealthBar(&sCur, &sMax) && sMax > 0 && sCur > 0) {
            fillRatio = static_cast<f32>(sCur) / static_cast<f32>(sMax);
            name = bossDisplayName(fpcNm_B_ZANT_e);
        } else if (dAlbwBoss_fyrusQueryHealthBar(&sCur, &sMax) && sMax > 0 && sCur > 0) {
            fillRatio = static_cast<f32>(sCur) / static_cast<f32>(sMax);
            name = bossDisplayName(fpcNm_E_FM_e);
        }
        // Fork also polls dShadeBoss_queryHealthBar between Diababa and Zant;
        // the shade arc is not ported to this mod, so that link is absent.
    }

    if (name == nullptr) {
        return;
    }
    // ============================================
    // NEW CODE ENDS HERE
    // ============================================

    if (fillRatio <= 0.0f || !ensureResources()) {
        return;
    }

    J2DGrafContext* gfx = g_dComIfG_gameInfo.play.getCurrentGrafPort();
    if (gfx == nullptr) {
        return;
    }

    const f32 minX = mDoGph_gInf_c::getMinXF();
    const f32 minY = mDoGph_gInf_c::getMinYF();
    const f32 scrW = mDoGph_gInf_c::getWidthF();
    const f32 scrH = mDoGph_gInf_c::getHeightF();

    J2DOrthoGraph* ortho = static_cast<J2DOrthoGraph*>(gfx);
    ortho->setOrtho(minX, minY, scrW, scrH, -1.0f, 1.0f);
    ortho->setup2D();
    ortho->setPort();

    const f32 barW = scrW * kBarWidthFrac;
    const f32 barH = scrH * kBarHeightFrac;
    const f32 barX = minX + (scrW - barW) * 0.5f;
    const f32 barY = minY + scrH * kBarTopFrac;
    const f32 fontSz = scrH * kNameFontFrac;
    const f32 nameY = barY - kNameGap - fontSz;

    f32 fill = fillRatio;
    if (fill > 1.0f) {
        fill = 1.0f;
    }
    const f32 fillW = barW * fill;

    drawRect(barX - kBarBorder, barY - kBarBorder, barW + kBarBorder * 2.0f,
             barH + kBarBorder * 2.0f, 32, 32, 36, 255);
    drawRect(barX, barY, barW, barH, 18, 18, 22, 168);
    drawRect(barX, barY, fillW, barH, 176, 8, 8, 255);

    sName->setFontSize(fontSz, fontSz);
    sName->setString(name);
    // fork: black outline (8-neighbor, RGBA 0x000000FF - the old value here was
    // byte-swapped to 0xFF000000, i.e. RED with alpha 0, an invisible outline),
    // then the intro-card cream/gold fill.
    sName->setCharColor(0x000000FF);
    sName->setGradColor(0x000000FF);
    for (int ox = -1; ox <= 1; ++ox) {
        for (int oy = -1; oy <= 1; ++oy) {
            if (ox == 0 && oy == 0) {
                continue;
            }
            sName->draw(barX + ox * kNameOutline, nameY + oy * kNameOutline, barW,
                        HBIND_CENTER);
        }
    }
    sName->setCharColor(kNameColorTop);
    sName->setGradColor(kNameColorBot);
    sName->draw(barX, nameY, barW, HBIND_CENTER);
}
