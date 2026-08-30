#include "extra_item_slot_hooks.h"

#include "extra_item_slot.h"
#include "quick_equip.h"
#include "albw_common.h"
#include "albw_l1_input.h"
#include "config_vars.h"
#include "albw_game.h"
#include "z_item_hud.h"

#include "Z2AudioLib/Z2AudioMgr.h"
#include "Z2AudioLib/Z2SeMgr.h"
#include "d/actor/d_a_player.h"
#include "d/d_camera.h"
#include "d/d_com_inf_game.h"
#include "d/d_meter2.h"
#include "d/d_meter2_draw.h"
#include "d/d_meter2_info.h"
#include "d/d_msg_object.h"
#include "d/d_save.h"
#include "d/d_stage.h"
#include "f_op/f_op_overlap_mng.h"
#include "m_Do/m_Do_controller_pad.h"
#include "m_Do/m_Do_graphic.h"

#define private public
#include "d/actor/d_a_alink.h"
#include "d/d_menu_item_explain.h"
#include "d/d_menu_ring.h"
#include "d/d_menu_window.h"
#undef private

#include "mods/hook.hpp"

namespace {

bool item_wheel_trig() {
    // When Quick Equip owns L1 tap/hold, suppress the edge→UP fake here.
    if (albw_quick_equip_enabled()) {
        return false;
    }
    // Physical L1/LB only — PAD_TRIGGER_L is polluted by L2 via emulateTriggers.
    return albw_l1_trig(PAD_1);
}

DEFINE_HOOK(&daAlink_c::midnaTalkTrigger, MidnaTalkTrigger);
DEFINE_HOOK(&daAlink_c::checkItemSetButton, CheckItemSetButton);
DEFINE_HOOK(&daAlink_c::checkSetItemTrigger, CheckSetItemTrigger);
DEFINE_HOOK(&daAlink_c::checkItemChangeFromButton, CheckItemChangeFromButton);
DEFINE_HOOK(&daAlink_c::checkItemButtonChange, CheckItemButtonChange);
DEFINE_HOOK(&dMeter2_c::_create, Meter2Create);
DEFINE_HOOK(&dMeter2_c::_execute, Meter2Execute);
DEFINE_HOOK(&dMeter2_c::moveButtonZ, MoveButtonZ);
DEFINE_HOOK(dMw_UP_TRIGGER, MwUpTrigger);
DEFINE_HOOK(dMw_DOWN_TRIGGER, MwDownTrigger);
DEFINE_HOOK(dMw_LEFT_TRIGGER, MwLeftTrigger);
DEFINE_HOOK(dMw_RIGHT_TRIGGER, MwRightTrigger);
DEFINE_HOOK_SYMBOL("?dComIfGp_setSelectItem@@YAXH@Z", void(int), SetSelectItem);
DEFINE_HOOK(&dMenu_Ring_c::setActiveCursor, RingSetActiveCursor);

bool quick_swap_suppresses_dpad() {
    return albw_is_dpad_quick_swap_enabled() && !g_dComIfG_gameInfo.play.isPauseFlag();
}

bool extra_slot_reserves_left_dpad() {
    return albw_is_extra_item_slot_enabled();
}

HookAction on_midna_talk_trigger_pre(ModContext*, void* args, void* retval, void*) {
    if (!albw_is_extra_item_slot_enabled() || retval == nullptr) {
        return HOOK_CONTINUE;
    }

    (void)args;
    *static_cast<BOOL*>(retval) = mDoCPd_c::getTrigLeft(PAD_1) != 0;
    return HOOK_SKIP_ORIGINAL;
}

void on_check_item_set_button_post(ModContext*, void* args, void* retval, void*) {
    if (!albw_is_extra_item_slot_enabled() || retval == nullptr) {
        return;
    }

    const int itemNo = mods::arg<int>(args, 1);
    auto* link = mods::arg<daAlink_c*>(args, 0);
    if (link == nullptr) {
        return;
    }

    int* out = static_cast<int*>(retval);
    if (*out != 2) {
        return;
    }

    if (link->checkGroupItem(itemNo, g_dComIfG_gameInfo.play.getSelectItem(SELECT_ITEM_DOWN))) {
        *out = SELECT_ITEM_DOWN;
        return;
    }

    *out = 3;
}

void on_check_set_item_trigger_post(ModContext*, void* args, void* retval, void*) {
    if (!albw_is_extra_item_slot_enabled() || retval == nullptr) {
        return;
    }

    if (*static_cast<int*>(retval) != 0) {
        return;
    }

    auto* link = mods::arg<daAlink_c*>(args, 0);
    const int itemNo = mods::arg<int>(args, 1);
    if (link == nullptr) {
        return;
    }

    if (link->checkGroupItem(itemNo, g_dComIfG_gameInfo.play.getSelectItem(SELECT_ITEM_DOWN)) &&
        link->itemTriggerCheck(daAlink_c::BTN_Z))
    {
        if (itemNo != dItemNo_HVY_BOOTS_e) {
            link->mSelectItemId = SELECT_ITEM_DOWN;
        }
        *static_cast<int*>(retval) = 1;
    }
}

// Stock only loops X/Y. Fork uses dusk_assignableItemButtonCount()==3 so Z
// reaches checkNewItemChange + changeItemTriggerKeepProc.
void on_check_item_change_from_button_post(ModContext*, void* args, void* retval, void*) {
    if (!albw_is_extra_item_slot_enabled() || retval == nullptr) {
        return;
    }
    if (*static_cast<BOOL*>(retval) != FALSE) {
        return;
    }

    auto* link = mods::arg<daAlink_c*>(args, 0);
    if (link == nullptr) {
        return;
    }
    // Same outer gate as stock checkItemChangeFromButton before the X/Y loop.
    if (!link->checkModeFlg(4) || link->checkEquipAnime() || link->checkBoomerangThrowAnime() ||
        link->checkCopyRodThrowAnime() || link->checkKandelaarSwingAnime())
    {
        return;
    }

    const int procType = link->checkNewItemChange(SELECT_ITEM_DOWN);
    if (procType != 0 && link->itemTriggerCheck(daAlink_c::BTN_Z)) {
        *static_cast<BOOL*>(retval) =
            link->changeItemTriggerKeepProc(SELECT_ITEM_DOWN, procType) != 0 ? TRUE : FALSE;
    }
}

// Keep mSelectItemId on Z when the equipped item is only assigned there.
void on_check_item_button_change_post(ModContext*, void* args, void*, void*) {
    if (!albw_is_extra_item_slot_enabled()) {
        return;
    }
    auto* link = mods::arg<daAlink_c*>(args, 0);
    if (link == nullptr || link->mEquipItem == dItemNo_NONE_e || link->checkEquipAnime()) {
        return;
    }
    if (link->mEquipItem == g_dComIfG_gameInfo.play.getSelectItem(SELECT_ITEM_DOWN)) {
        link->mSelectItemId = SELECT_ITEM_DOWN;
    }
}

void on_meter2_create_post(ModContext*, void*, void* retval, void*) {
    if (!albw_is_extra_item_slot_enabled() || retval == nullptr ||
        *static_cast<s32*>(retval) != cPhs_COMPLEATE_e)
    {
        return;
    }

    dComIfGp_setSelectItem(SELECT_ITEM_DOWN);
}

void on_meter2_execute_post(ModContext*, void*, void*, void*) {
    if (!albw_is_extra_item_slot_enabled()) {
        return;
    }

    // Stock _execute clears METER2_USEBUTTON_Z (0x800) every frame. Fork re-arms it
    // when the Z slot holds an item so use + icon alpha work.
    daPy_py_c* player = albw_game::link_player();
    if (player == nullptr || player->checkWolf()) {
        return;
    }

    dComIfGp_setSelectItem(SELECT_ITEM_DOWN);
    const u8 zItem = g_dComIfG_gameInfo.play.getSelectItem(SELECT_ITEM_DOWN);
    if (zItem != dItemNo_NONE_e && zItem != 0) {
        g_meter2_info.onUseButton(METER2_USEBUTTON_Z);
    }
}

void on_move_button_z_post(ModContext*, void* args, void*, void*) {
    if (!albw_is_extra_item_slot_enabled()) {
        return;
    }

    auto* meter = mods::arg<dMeter2_c*>(args, 0);
    if (meter == nullptr || meter->getMeterDrawPtr() == nullptr) {
        return;
    }
    daPy_py_c* player = albw_game::link_player();
    if (player != nullptr && player->checkWolf()) {
        return;
    }

    // Stock only redraws Z when status changes; fork forces every frame so the
    // item icon stays on after drawButtonR hides shared panes.
    meter->getMeterDrawPtr()->drawButtonZ(g_dComIfG_gameInfo.play.getZStatus());
}

HookAction on_set_select_item_pre(ModContext*, void* args, void*, void*) {
    if (!albw_is_extra_item_slot_enabled()) {
        return HOOK_CONTINUE;
    }

    const int idx = mods::arg<int>(args, 0);
    if (idx != SELECT_ITEM_DOWN) {
        return HOOK_CONTINUE;
    }

    auto& status = g_dComIfG_gameInfo.info.getPlayer().getPlayerStatusA();
    auto& item = g_dComIfG_gameInfo.info.getPlayer().getItem();

    if (status.getSelectItemIndex(idx) != 0xFF) {
        const u8 resolved = item.getItem(status.getSelectItemIndex(idx), false);
        g_dComIfG_gameInfo.play.setSelectItem(idx, resolved);
        if (resolved == dItemNo_NONE_e) {
            status.setSelectItemIndex(idx, 0xFF);
        }
    } else {
        g_dComIfG_gameInfo.play.setSelectItem(idx, dItemNo_NONE_e);
    }

    return HOOK_SKIP_ORIGINAL;
}

void on_up_trigger_post(ModContext*, void*, void* retval, void*) {
    if (retval == nullptr) {
        return;
    }

    // Quick Equip open path: one-shot TRUE so stock opens the ring.
    if (albw_quick_equip_consume_force_up()) {
        *static_cast<BOOL*>(retval) = TRUE;
        return;
    }

    // Without QE: Extra+QuickSwap maps L1 edge → item wheel (fake UP).
    if (albw_is_dpad_quick_swap_enabled() && item_wheel_trig()) {
        *static_cast<BOOL*>(retval) = TRUE;
        return;
    }

    // Quick Swap: D-pad Up is sword cycle — never open the stock item wheel in field.
    if (quick_swap_suppresses_dpad()) {
        *static_cast<BOOL*>(retval) = FALSE;
    }
}

void on_down_trigger_post(ModContext*, void*, void* retval, void*) {
    if (retval == nullptr || !quick_swap_suppresses_dpad()) {
        return;
    }
    *static_cast<BOOL*>(retval) = FALSE;
}

void on_right_trigger_post(ModContext*, void*, void* retval, void*) {
    if (retval == nullptr || !quick_swap_suppresses_dpad()) {
        return;
    }
    *static_cast<BOOL*>(retval) = FALSE;
}

void on_left_trigger_post(ModContext*, void*, void* retval, void*) {
    if (retval == nullptr || !extra_slot_reserves_left_dpad() ||
        g_dComIfG_gameInfo.play.isPauseFlag())
    {
        return;
    }
    *static_cast<BOOL*>(retval) = FALSE;
}

// Port of fork dMenu_Ring_c::setActiveCursor Z face-assign (getTrigZ → SELECT_ITEM_DOWN).
// Stock setItem() only handles field_0x6b3 0/1, so we assign Z directly.
void on_ring_set_active_cursor_post(ModContext*, void* args, void*, void*) {
    if (!albw_is_extra_item_slot_enabled() || args == nullptr) {
        return;
    }

    // Hold-L1 Quick Equip session: X/Y already redirect to Z via setSelectItemIndex.
    if (albw_quick_equip_session_active()) {
        return;
    }

    auto* ring = mods::arg<dMenu_Ring_c*>(args, 0);
    if (ring == nullptr || ring->mpItemExplain == nullptr) {
        return;
    }

    if (ring->mStatus != dMenu_Ring_c::STATUS_WAIT ||
        ring->mOldStatus == dMenu_Ring_c::STATUS_EXPLAIN_FORCE ||
        ring->mOldStatus == dMenu_Ring_c::STATUS_EXPLAIN ||
        ring->mpItemExplain->getStatus() != 0)
    {
        return;
    }

    if (!mDoCPd_c::getTrigZ(PAD_1)) {
        return;
    }

    if (ring->mPlayerIsWolf || ring->isMixItemOn() || ring->isMixItemOff()) {
        return;
    }

    const u8 invSlot = ring->mItemSlots[ring->mCurrentSlot];
    const u8 item =
        g_dComIfG_gameInfo.info.getPlayer().getItem().getItem(invSlot, false);
    if (item == dItemNo_NONE_e || invSlot == 0xFF) {
        Z2GetAudioMgr()->seStart(Z2SE_SYS_ERROR, NULL, 0, 0, 1.0f, 1.0f, -1.0f, -1.0f, 0);
        return;
    }

    for (int i = 0; i < MAX_SELECT_ITEM; i++) {
        ring->setSelectItemForce(i);
    }

    auto& status = g_dComIfG_gameInfo.info.getPlayer().getPlayerStatusA();

    // Fork setItem field_0x6b3==2: clear X/Y if they held the same inventory slot.
    if (status.getSelectItemIndex(SELECT_ITEM_X) == invSlot) {
        status.setSelectItemIndex(SELECT_ITEM_X, 0xFF);
        status.setMixItemIndex(SELECT_ITEM_X, 0xFF);
        ring->mXButtonSlot = 0xFF;
        dComIfGp_setSelectItem(SELECT_ITEM_X);
    }
    if (status.getSelectItemIndex(SELECT_ITEM_Y) == invSlot) {
        status.setSelectItemIndex(SELECT_ITEM_Y, 0xFF);
        status.setMixItemIndex(SELECT_ITEM_Y, 0xFF);
        ring->mYButtonSlot = 0xFF;
        dComIfGp_setSelectItem(SELECT_ITEM_Y);
    }

    status.setSelectItemIndex(SELECT_ITEM_DOWN, invSlot);
    ring->field_0x6ac = ring->mCurrentSlot;
    dComIfGp_setSelectItem(SELECT_ITEM_DOWN);

    dMeter2Info_set2DVibrationM();
    Z2GetAudioMgr()->seStart(Z2SE_SY_ITEM_SET_X, NULL, 0, 0, 1.0f, 1.0f, -1.0f, -1.0f, 0);

    if (ring->mpItemExplain->getStatus() == 0) {
        ring->setStatus(dMenu_Ring_c::STATUS_WAIT);
        ring->stick_wait_init();
    }
}

bool install(ModError* error, const char* name, ModResult r) {
    if (r != MOD_OK) {
        svc_log->error(mod_ctx, name);
        mods::set_error(error, MOD_ERROR, name);
        return false;
    }
    return true;
}

}  // namespace

ModResult albw_extra_item_slot_hooks_init(ModError* error) {
    if (!install(error, "MidnaTalkTrigger",
                 mods::hook_add_pre<MidnaTalkTrigger>(svc_hook, on_midna_talk_trigger_pre)) ||
        !install(error, "CheckItemSetButton",
                 mods::hook_add_post<CheckItemSetButton>(svc_hook, on_check_item_set_button_post)) ||
        !install(error, "CheckSetItemTrigger",
                 mods::hook_add_post<CheckSetItemTrigger>(svc_hook,
                                                        on_check_set_item_trigger_post)) ||
        !install(error, "CheckItemChangeFromButton",
                 mods::hook_add_post<CheckItemChangeFromButton>(
                     svc_hook, on_check_item_change_from_button_post)) ||
        !install(error, "CheckItemButtonChange",
                 mods::hook_add_post<CheckItemButtonChange>(svc_hook,
                                                           on_check_item_button_change_post)) ||
        !install(error, "Meter2CreateExtraSlot",
                 mods::hook_add_post<Meter2Create>(svc_hook, on_meter2_create_post)) ||
        !install(error, "Meter2ExecuteZUse",
                 mods::hook_add_post<Meter2Execute>(svc_hook, on_meter2_execute_post)) ||
        !install(error, "MoveButtonZForceDraw",
                 mods::hook_add_post<MoveButtonZ>(svc_hook, on_move_button_z_post)) ||
        !install(error, "SetSelectItemZSlot",
                 mods::hook_add_pre<SetSelectItem>(svc_hook, on_set_select_item_pre)) ||
        !install(error, "MwUpTrigger",
                 mods::hook_add_post<MwUpTrigger>(svc_hook, on_up_trigger_post)) ||
        !install(error, "MwDownTrigger",
                 mods::hook_add_post<MwDownTrigger>(svc_hook, on_down_trigger_post)) ||
        !install(error, "MwRightTrigger",
                 mods::hook_add_post<MwRightTrigger>(svc_hook, on_right_trigger_post)) ||
        !install(error, "MwLeftTrigger",
                 mods::hook_add_post<MwLeftTrigger>(svc_hook, on_left_trigger_post)) ||
        !install(error, "RingSetActiveCursorZ",
                 mods::hook_add_post<RingSetActiveCursor>(svc_hook,
                                                        on_ring_set_active_cursor_post)))
    {
        return MOD_ERROR;
    }

    svc_log->info(mod_ctx, "extra item slot + d-pad reservation hooks ready");
    return MOD_OK;
}

ModResult albw_extra_item_slot_hooks_shutdown(ModError*) {
    albw_z_item_hud_restore_stock();
    mods::hook_uninstall<MidnaTalkTrigger>(svc_hook);
    mods::hook_uninstall<CheckItemSetButton>(svc_hook);
    mods::hook_uninstall<CheckSetItemTrigger>(svc_hook);
    mods::hook_uninstall<CheckItemChangeFromButton>(svc_hook);
    mods::hook_uninstall<CheckItemButtonChange>(svc_hook);
    mods::hook_uninstall<Meter2Create>(svc_hook);
    mods::hook_uninstall<Meter2Execute>(svc_hook);
    mods::hook_uninstall<MoveButtonZ>(svc_hook);
    mods::hook_uninstall<SetSelectItem>(svc_hook);
    mods::hook_uninstall<MwUpTrigger>(svc_hook);
    mods::hook_uninstall<MwDownTrigger>(svc_hook);
    mods::hook_uninstall<MwRightTrigger>(svc_hook);
    mods::hook_uninstall<MwLeftTrigger>(svc_hook);
    mods::hook_uninstall<RingSetActiveCursor>(svc_hook);
    return MOD_OK;
}
