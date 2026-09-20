#include "z_item_hud.h"

#include "albw_common.h"
#include "extra_item_slot.h"
#include "albw_game.h"

#include "JSystem/J2DGraph/J2DPicture.h"
#include "JSystem/J2DGraph/J2DPane.h"
#include "d/d_com_inf_game.h"
#include "d/d_meter_HIO.h"
#include "d/d_item_data.h"
#include "d/d_meter2_draw.h"
#include "d/d_meter2.h"
#include "d/d_meter2_info.h"
#include "d/d_pane_class.h"
#include "d/d_save.h"

#include <algorithm>

#include "mods/hook.hpp"

namespace {

static constexpr int kMidnaCrossLeftJujiIndex = 1;
static constexpr u64 kMidnaCrossLeftMapTextTag = MULTI_CHAR('cont_ju6');
static constexpr f32 kMidnaCrossLeftExtraX = -46.0f;
static constexpr f32 kMidnaCrossLeftExtraY = 0.0f;
static constexpr f32 kMidnaCrossScaleFactor = 0.82f;

// Fork/stock meter draw allocates 0xC00 ResTIMG scratch per slot (heap->alloc).
// readItemTexture no-ops when the buffer pointer is null — then width/height deref
// faults at +0x2 (seen on Ordon load with Z=kantera).
alignas(32) u8 sItemZTexBuf[2][2][0xC00] = {};
ResTIMG* sItemZTex[2][2] = {
    {reinterpret_cast<ResTIMG*>(sItemZTexBuf[0][0]), reinterpret_cast<ResTIMG*>(sItemZTexBuf[0][1])},
    {reinterpret_cast<ResTIMG*>(sItemZTexBuf[1][0]), reinterpret_cast<ResTIMG*>(sItemZTexBuf[1][1])},
};
u8 sButtonZItem = dItemNo_NONE_e;
u8 sZTexFlip = 0;

bool z_draw_ready(dMeter2Draw_c* draw) {
    return draw != nullptr && draw->mpItemR != nullptr && draw->mpItemR->getPanePtr() != nullptr &&
           draw->mpItemXYPane[2] != nullptr && draw->mpButtonXY[2] != nullptr &&
           draw->mpLightXY[2] != nullptr && draw->mpButtonMidona != nullptr &&
           draw->mpTextXY[2] != nullptr && draw->mpScreen != nullptr;
}

void positionMidnaIconOnCrossLeft(CPaneMgr* midona, CPaneMgr* crossAnchor, CPaneMgr* buttonParent) {
    const f32 buttonScale = std::max(buttonParent->getScaleX(), 0.001f);
    const f32 iconScale = g_drawHIO.mMidnaIconScale * kMidnaCrossScaleFactor;
    midona->scale(iconScale, iconScale);
    midona->paneTrans(g_drawHIO.mMidnaIconPosX, g_drawHIO.mMidnaIconPosY);
    const Vec zSlotCenter = midona->getGlobalVtxCenter(false, 0);
    const Vec crossCenter = crossAnchor->getGlobalVtxCenter(false, 0);
    midona->paneTrans(g_drawHIO.mMidnaIconPosX + (crossCenter.x - zSlotCenter.x) / buttonScale +
                         kMidnaCrossLeftExtraX,
                     g_drawHIO.mMidnaIconPosY + (crossCenter.y - zSlotCenter.y) / buttonScale +
                         kMidnaCrossLeftExtraY);
}

void changeTextureItemZ(dMeter2Draw_c* draw, u8 itemNo) {
    if (itemNo == dItemNo_LIGHT_ARROW_e) {
        itemNo = dItemNo_BOW_e;
    }

    // Local flip index — stock field_0x76e is shared UI state; keep ours independent.
    sZTexFlip = sZTexFlip == 0 ? 1 : 0;
    const u8 flip = sZTexFlip;

    draw->setItemParamZ(itemNo);
    if (g_meter2_info.readItemTexture(
            itemNo, sItemZTex[flip][0], static_cast<J2DPicture*>(draw->mpItemR->getPanePtr()),
            sItemZTex[flip][1], draw->mpItemXYPane[2], NULL, NULL, NULL, NULL, -1) <= 1)
    {
        draw->mpItemXYPane[2]->hide();
    } else {
        draw->mpItemXYPane[2]->show();
    }

    ResTIMG* tex = sItemZTex[flip][0];
    if (tex == nullptr || tex->width == 0 || tex->height == 0) {
        return;
    }

    f32 texScale;
    if (g_drawHIO.mItemScaleAdjustON == true) {
        texScale = g_drawHIO.mItemScalePercent / 100.0f;
    } else {
        texScale = dItem_data::getTexScale(itemNo) / 100.0f;
    }

    draw->field_0x6c4[2] =
        texScale * ((tex->width * draw->mpItemR->getInitSizeX()) / 48.0f);
    draw->field_0x6d0[2] =
        texScale * ((tex->height * draw->mpItemR->getInitSizeY()) / 48.0f);
    draw->field_0x6ac[2] = (draw->mpItemR->getInitSizeX() - draw->field_0x6c4[2]) * 0.5f;
    draw->field_0x6b8[2] = (draw->mpItemR->getInitSizeY() - draw->field_0x6d0[2]) * 0.5f;
    draw->mpItemR->resize(draw->field_0x6c4[2], draw->field_0x6d0[2]);
    draw->mpItemR->paneTrans(draw->mItemParams[dMeter2Draw_c::SELECT_Z_e].pos_x + draw->field_0x6ac[2],
                             draw->mItemParams[dMeter2Draw_c::SELECT_Z_e].pos_y + draw->field_0x6b8[2]);
    draw->mpItemXYPane[2]->resize(draw->field_0x6c4[2], draw->field_0x6d0[2]);
}

void hideButtonZItem(dMeter2Draw_c* draw) {
    sButtonZItem = dItemNo_NONE_e;
    if (draw == nullptr) {
        return;
    }
    if (draw->mpItemR != nullptr) {
        draw->mpItemR->hide();
    }
    if (draw->mpLightXY[2] != nullptr) {
        draw->mpLightXY[2]->hide();
    }
}

void drawButtonZItem(dMeter2Draw_c* draw, u8 itemNo) {
    draw->mpButtonMidona->hide();
    draw->mpTextXY[2]->hide();

    draw->mpButtonXY[2]->scale(g_drawHIO.mButtonZScale, g_drawHIO.mButtonZScale);
    draw->mpButtonXY[2]->paneTrans(g_drawHIO.mButtonZPosX, g_drawHIO.mButtonZPosY);

    if (sButtonZItem != itemNo) {
        sButtonZItem = itemNo;
        changeTextureItemZ(draw, itemNo);
    }

    const f32 itemScale =
        draw->mItemParams[dMeter2Draw_c::SELECT_Z_e].scale * g_drawHIO.mButtonZItemScale;
    draw->mpItemR->scale(itemScale, itemScale);
    draw->mpItemR->paneTrans(g_drawHIO.mButtonZItemPosX + draw->field_0x6ac[2],
                             g_drawHIO.mButtonZItemPosY + draw->field_0x6b8[2]);
    draw->mpItemR->getPanePtr()->rotate(draw->mpItemR->getSizeX() * 0.5f,
                                       draw->mpItemR->getSizeY() * 0.5f, ROTATE_Z,
                                       draw->mItemParams[dMeter2Draw_c::SELECT_Z_e].rotation);
    draw->mpItemR->show();
    draw->mpScreen->search(MULTI_CHAR('item_r_n'))->show();

    draw->mpLightXY[2]->scale(g_drawHIO.mButtonZItemBaseScale, g_drawHIO.mButtonZItemBaseScale);
    draw->mpLightXY[2]->paneTrans(g_drawHIO.mButtonZItemBasePosX, g_drawHIO.mButtonZItemBasePosY);
    draw->mpLightXY[2]->show();
}

void drawExtraSlotButtonZ(dMeter2Draw_c* draw) {
    const u8 zItem = g_dComIfG_gameInfo.play.getSelectItem(SELECT_ITEM_DOWN);
    if (zItem != dItemNo_NONE_e && zItem != 0) {
        drawButtonZItem(draw, zItem);
        return;
    }

    hideButtonZItem(draw);
    draw->mpButtonMidona->hide();
    draw->mpTextXY[2]->hide();
    draw->mpButtonXY[2]->scale(g_drawHIO.mButtonZScale, g_drawHIO.mButtonZScale);
    draw->mpButtonXY[2]->paneTrans(g_drawHIO.mButtonZPosX, g_drawHIO.mButtonZPosY);
}

DEFINE_HOOK(&dMeter2Draw_c::drawButtonZ, DrawButtonZ);
DEFINE_HOOK(&dMeter2Draw_c::drawButtonR, DrawButtonR);
DEFINE_HOOK(&dMeter2Draw_c::setButtonIconMidonaAlpha, SetButtonIconMidonaAlpha);

void on_midona_alpha_post(ModContext*, void* args, void*, void*) {
    if (!albw_is_extra_item_slot_enabled()) {
        return;
    }

    auto* draw = mods::arg<dMeter2Draw_c*>(args, 0);
    if (draw == nullptr || draw->mpButtonMidona == nullptr ||
        draw->mpJujiM[kMidnaCrossLeftJujiIndex] == nullptr || draw->mpButtonParent == nullptr)
    {
        return;
    }

    positionMidnaIconOnCrossLeft(draw->mpButtonMidona, draw->mpJujiM[kMidnaCrossLeftJujiIndex],
                                 draw->mpButtonParent);
    draw->mpJujiM[kMidnaCrossLeftJujiIndex]->hide();
    if (draw->mpJujiI[kMidnaCrossLeftJujiIndex] != nullptr) {
        draw->mpJujiI[kMidnaCrossLeftJujiIndex]->hide();
    }
    if (draw->mpScreen != nullptr) {
        J2DPane* mapLabel = draw->mpScreen->search(kMidnaCrossLeftMapTextTag);
        if (mapLabel != nullptr) {
            mapLabel->hide();
        }
    }
    if (!draw->mpButtonMidona->isVisible()) {
        draw->mpButtonMidona->show();
    }
}

HookAction on_draw_button_z_pre(ModContext*, void* args, void*, void*) {
    auto* draw = mods::arg<dMeter2Draw_c*>(args, 0);

    if (!albw_is_extra_item_slot_enabled()) {
        // Feature toggled Extra Slot off mid-session — clear Z-item panes before stock draws.
        if (sButtonZItem != dItemNo_NONE_e && z_draw_ready(draw)) {
            hideButtonZItem(draw);
        }
        return HOOK_CONTINUE;
    }

    if (!z_draw_ready(draw)) {
        return HOOK_CONTINUE;
    }

    if (albw_game::is_wolf_form()) {
        if (sButtonZItem != dItemNo_NONE_e) {
            hideButtonZItem(draw);
        }
        return HOOK_CONTINUE;
    }

    drawExtraSlotButtonZ(draw);
    return HOOK_SKIP_ORIGINAL;
}

void on_draw_button_r_post(ModContext*, void* args, void*, void*) {
    if (!albw_is_extra_item_slot_enabled() || sButtonZItem == dItemNo_NONE_e) {
        return;
    }

    auto* draw = mods::arg<dMeter2Draw_c*>(args, 0);
    if (draw == nullptr || draw->mpScreen == nullptr) {
        return;
    }

    J2DPane* itemR = draw->mpScreen->search(MULTI_CHAR('item_r_n'));
    if (itemR != nullptr) {
        itemR->show();
    }
}

// ============================================
// A hook that fails to resolve must NOT abort mod_initialize.
//
// This helper used to call mods::set_error(..., MOD_ERROR, ...) and return
// false, which made mod_initialize return MOD_ERROR - so ONE unresolved symbol
// unloaded the ENTIRE mod. That is how a single missing hook target reached
// players as "Failed - Reason: <hook name>" with nothing loaded at all, on a
// build where every other feature was fine. It is the same doctrine fyrus.cpp
// already states for the boss hooks.
//
// Now the miss is LOUD and SCOPED: the feature that needed the hook is
// inactive for the run and says so by name in the log, and everything else
// still loads. Never make this silent - a quiet miss turns "never bound" into
// "plausibly wrong forever".
// ============================================
bool install(ModError*, const char* name, ModResult r) {
    if (r != MOD_OK) {
        if (svc_log != nullptr) {
            svc_log->error(mod_ctx, name);
            svc_log->error(mod_ctx,
                           "hook above did NOT install - that feature is inactive this run");
        }
    }
    return true;
}

}  // namespace

void albw_z_item_hud_restore_stock() {
    dMeter2Draw_c* draw = nullptr;
    dMeter2_c* meter = g_meter2_info.getMeterClass();
    if (meter != nullptr) {
        draw = meter->getMeterDrawPtr();
    }
    hideButtonZItem(draw);
    sZTexFlip = 0;
}

ModResult albw_z_item_hud_hooks_init(ModError* error) {
    if (!install(error, "DrawButtonZExtraSlot",
                 mods::hook_add_pre<DrawButtonZ>(svc_hook, on_draw_button_z_pre)) ||
        !install(error, "DrawButtonRKeepZItem",
                 mods::hook_add_post<DrawButtonR>(svc_hook, on_draw_button_r_post)) ||
        !install(error, "MidnaIconCrossLeft",
                 mods::hook_add_post<SetButtonIconMidonaAlpha>(svc_hook, on_midona_alpha_post)))
    {
        return MOD_ERROR;
    }
    return MOD_OK;
}

ModResult albw_z_item_hud_hooks_shutdown(ModError*) {
    albw_z_item_hud_restore_stock();
    mods::hook_uninstall<DrawButtonZ>(svc_hook);
    mods::hook_uninstall<DrawButtonR>(svc_hook);
    mods::hook_uninstall<SetButtonIconMidonaAlpha>(svc_hook);
    return MOD_OK;
}
