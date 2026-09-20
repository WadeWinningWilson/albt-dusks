#include "extra_item_slot_hooks.h"
#include "albw_symbols.h"

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

// ============================================
// Z-ITEM KEEP FIX: the reproduced checkItemChangeFromButton tail compares
// getRunEventName() against "ANGER"/"ANGER2" (fork d_a_alink.cpp:13470-13471).
// ============================================
#include <cstring>

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
DEFINE_HOOK_SYMBOL(ALBT_SYM_SET_SELECT_ITEM, void(int), SetSelectItem);
DEFINE_HOOK(&dMenu_Ring_c::setActiveCursor, RingSetActiveCursor);

// ============================================
// SOFTLOCK FIX (item-ring back-out): stock dMenu_Ring_c::isMoveEnd closes the
// ring ONLY on dMw_UP/DOWN/B triggers (stock d_menu_ring.cpp:821-826), and the
// collect screens navigate with LEFT/RIGHT. These field-only D-pad reservations
// used to gate on !isPauseFlag() alone, which does not hold on every menu frame
// - the reservation then ate the ring's own back-out input and the menu could
// never close (assign works, exit hangs). Bail whenever the menu system is
// engaged (pause, message, menu heap lock - the same signals can_use_quick_swap
// trusts), so the reservations apply only to true field navigation. Donor
// intent: the fork suppresses FIELD D-pad, never subscreen input.
// ============================================
bool menu_system_engaged() {
    if (g_dComIfG_gameInfo.play.isPauseFlag()) {
        return true;
    }
    const int heapLock = g_dComIfG_gameInfo.play.isHeapLockFlag();
    if (heapLock != 0 && heapLock != 5) {
        return true;
    }
    return g_dComIfG_gameInfo.play.getMesgStatus() != 0;
}

bool quick_swap_suppresses_dpad() {
    return albw_is_dpad_quick_swap_enabled() && !menu_system_engaged();
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

// ============================================
// Z-ITEM KEEP FIX (shared helper): Z-aware checkItemSetButton with FORK
// sentinel semantics — X/Y like stock daAlink_c::checkItemSetButton
// (stock d_a_alink.cpp:14422-14430), then the Z slot, and 3 = not on any
// assign button (fork dusk_assignableItemButtonCount()==3 /
// dusk_itemNotOnAnyAssignButton() -> btn >= 3, fork d_a_alink.cpp:10443-10452).
// Returns 0 / 1 / SELECT_ITEM_DOWN / 3. File-local on purpose: the reproduced
// checkItemChangeFromButton body below must NOT re-enter the hooked member
// checkItemSetButton (the post-hook item-keyed translation would poison the
// tail's >= 3 test for KANTERA/HVY_BOOTS/SPINNER).
// ============================================
int z_aware_item_set_button(daAlink_c* link, int itemNo) {
    for (int i = 0; i < 2; i++) {
        if (link->checkGroupItem(itemNo, g_dComIfG_gameInfo.play.getSelectItem(i))) {
            return i;
        }
    }

    if (link->checkGroupItem(itemNo, g_dComIfG_gameInfo.play.getSelectItem(SELECT_ITEM_DOWN))) {
        return SELECT_ITEM_DOWN;
    }

    return 3;
}

// ============================================
// FIX B (item-keyed sentinel translation): stock callers test
// checkItemSetButton(...) == 2 / != 2 where 2 means "not assigned" — but
// SELECT_ITEM_DOWN is ALSO 2 (stock d_save.h:18,125), so a Z-only item reads
// as unassigned. For exactly these items the stock ==2/!=2 caller must see
// "assigned" for a Z-only item, so return 3 (never a valid button index to
// those callers) instead of SELECT_ITEM_DOWN:
//   dItemNo_KANTERA_e   — stock d_a_alink.cpp:17855 (offKandelaarModel: lit-
//                         lantern model survives), 14629/14698 (oil pour
//                         allowed; 0x48 == dItemNo_KANTERA_e, d_item_data.h:172)
//   dItemNo_HVY_BOOTS_e — stock d_a_alink.cpp:18244 (boots stay on; the else
//                         branch's dMeter2Info_onDirectUseItem(3) bit is never
//                         read — cosmetic only)
//   dItemNo_SPINNER_e   — stock d_a_alink_spinner.inc:214 (==2 dismount test).
//                         LT-sourced, user-approved usage: LAZY TWEAKS
//                         d_a_alink_spinner.inc:214 tests == SELECT_ITEM_NUM
//                         (3 in LT d_save.h:18) — ride continues while the
//                         spinner sits on Z. The ALBT fork itself left spinner
//                         UNSWEPT (fork d_a_alink_spinner.inc:260 still ==2),
//                         so LT is the donor for this caller's semantics.
// All other items keep the current 2/3 convention (on-Z -> SELECT_ITEM_DOWN,
// unassigned -> 3): stock rod-group callers 14674/14677, canoe.inc:1716/1721,
// demo.inc:4157 are unaffected.
// ============================================
bool z_only_uses_unassigned_sentinel(int itemNo) {
    switch (itemNo) {
    case dItemNo_KANTERA_e:
    case dItemNo_HVY_BOOTS_e:
    case dItemNo_SPINNER_e:
        return true;
    default:
        return false;
    }
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

    // Stock already proved X/Y miss (*out == 2), so the helper can only yield
    // SELECT_ITEM_DOWN (found on Z) or 3 (not on any assign button).
    const int zAware = z_aware_item_set_button(link, itemNo);
    if (zAware == SELECT_ITEM_DOWN && z_only_uses_unassigned_sentinel(itemNo)) {
        *out = 3;  // FIX B: Z-only item must not read as "unassigned" at ==2 callers.
        return;
    }

    *out = zAware;
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

// ============================================
// FIX A (Z-ITEM KEEP): pre + HOOK_SKIP_ORIGINAL reproducing the FORK's
// checkItemChangeFromButton WHOLE (fork d_a_alink.cpp:13401-13486; stock
// baseline d_a_alink.cpp:12139-12213). The old post-hook here only re-ran the
// X/Y-style trigger for Z AFTER stock returned FALSE — but stock's tail
// (stock :12204) had already tested checkItemSetButton(mEquipItem) == 2 and,
// with the Z slot's SELECT_ITEM_DOWN ALSO being 2, called allUnequip(1) every
// frame for a Z-only equipped item ("use once then auto-holster"). The fork
// swept that caller to dusk_itemNotOnAnyAssignButton(...) i.e. >= 3 (fork
// :13472-13479, helper :10447-10452); reproducing the whole body is the only
// way to give the tail fork semantics from a hook.
//
// Deviations from a byte-literal fork transcription, each deliberate:
//  - GATE: feature off -> HOOK_CONTINUE, stock body runs byte-identical.
//  - The fork's PLATFORM_GCN dComIfGs_getSelectEquipSword() guard
//    (fork :13410-13412) is compiled OUT of the PC binary this mod targets,
//    so it is omitted here (matches stock PC :12147-12150).
//  - dusk_assignableItemButtonCount() (fork :13434) is 3 iff the extra slot
//    is enabled — the gate above guarantees enabled, so the loop is a
//    constant 3.
//  - The lantern auto-equip loop is KEPT 2-WIDE (X/Y only) — donor-faithful:
//    the fork itself keeps 2 there (fork :13459-13463).
//  - The tail calls file-local z_aware_item_set_button(), NOT the hooked
//    member checkItemSetButton: calling the member would re-enter the hook
//    pipeline and Fix B's item-keyed translation would return 3 for a
//    Z-assigned KANTERA/HVY_BOOTS/SPINNER, making the >= 3 test unequip the
//    very item this fix keeps out.
//  - checkKandelaarSwingAnime() is tested twice — donor quirk reproduced
//    verbatim (fork :13406-13407, stock :12144-12145).
// ============================================
HookAction on_check_item_change_from_button_pre(ModContext*, void* args, void* retval, void*) {
    if (!albw_is_extra_item_slot_enabled()) {
        return HOOK_CONTINUE;  // feature OFF: stock path, byte-identical.
    }

    auto* link = mods::arg<daAlink_c*>(args, 0);
    if (link == nullptr || retval == nullptr) {
        return HOOK_CONTINUE;
    }

    BOOL* out = static_cast<BOOL*>(retval);
    *out = 0;

    // Entry guards — fork d_a_alink.cpp:13402-13407, verbatim.
    if (link->checkModeFlg(4)
        && !link->checkEquipAnime()
        && !link->checkBoomerangThrowAnime()
        && !link->checkCopyRodThrowAnime()
        && !link->checkKandelaarSwingAnime()
        && !link->checkKandelaarSwingAnime())
    {
        // Sword branch — fork :13409-13423 (PC: no PLATFORM_GCN sword guard).
        if (!daAlink_c::checkNotBattleStage()
            && !link->checkCanoeRide()
            && (!link->checkModeFlg(0x40000) || link->checkEquipHeavyBoots())
            && link->mEquipItem != 0x103
            && link->swordTrigger())
        {
            if (link->checkEndResetFlg1(daPy_py_c::ERFLG1_SWORD_TRIGGER_NON)) {
                return HOOK_SKIP_ORIGINAL;  // fork :13419-13421 (return 0).
            }

            link->swordEquip(TRUE);
        // Canoe wood-sword branch — fork :13424-13430.
        } else if (link->checkCanoeRide()
                    && !daAlink_c::checkStageName("F_SP103")
                    && !link->checkCanoeSlider()
                    && !link->checkFisingRodLure()
                    && link->swordTrigger())
        {
            link->itemEquip(0x105);
        } else {
            u8 i;
            // 3-wide trigger loop — fork :13434-13444; buttonCount is the
            // fork's dusk_assignableItemButtonCount()==3 (gate == enabled).
            for (i = 0; i < 3; i++) {
                int proc_type = link->checkNewItemChange(i);
                if (proc_type != 0 && link->itemTriggerCheck(1 << i)) {
                    *out = link->changeItemTriggerKeepProc(i, proc_type);
                    return HOOK_SKIP_ORIGINAL;
                }
            }

            // Put-away branch — fork :13446-13455.
            if (link->doTrigger() && dComIfGp_getDoStatus() == BUTTON_STATUS_PUT_AWAY) {
                if (link->mEquipItem != dItemNo_KANTERA_e &&
                    link->checkNoResetFlg2(daPy_py_c::FLG2_UNK_1))
                {
                    link->offKandelaarModel();
                } else if (link->mSwordFlourishTimer != 0 && link->mEquipItem == 0x103 &&
                           !daPy_py_c::checkWoodSwordEquip() && !link->checkModeFlg(0x402))
                {
                    *out = link->procSwordUnequipSpInit();
                    return HOOK_SKIP_ORIGINAL;
                } else {
                    link->allUnequip(TRUE);
                }
            // Lantern auto-equip branch — fork :13456-13466, KEPT 2-WIDE
            // (X/Y only) exactly like the fork.
            } else if (link->mEquipItem == dItemNo_NONE_e &&
                       link->mThrowBoomerangAcKeep.getActor() == NULL &&
                       !link->checkCanoeRide() && link->checkNoUpperAnime() &&
                       link->checkNoResetFlg2(daPy_py_c::FLG2_UNK_1))
            {
                for (i = 0; i < 2; i++) {
                    if (dComIfGp_getSelectItem(i) == dItemNo_KANTERA_e) {
                        link->mSelectItemId = i;
                    }
                }

                link->itemEquip(dItemNo_KANTERA_e);
                link->onNoResetFlg1(daPy_py_c::FLG1_UNK_40);
            // Auto-put-away tail — fork :13467-13481: allUnequip(1) ONLY when
            // the item sits on NO assign button (Z-aware >= 3), never when it
            // is Z-only (SELECT_ITEM_DOWN). THE fix for use-once-then-holster.
            } else if (link->mEquipItem != 0x103 && link->mEquipItem != dItemNo_NONE_e &&
                       link->mEquipItem != 0x10B && link->mEquipItem != 0x102 &&
                       (!link->checkCanoeRide() || !link->checkFisingRodLure()))
            {
                if (!link->checkEventRun() ||
                    strcmp(dComIfGp_getEventManager().getRunEventName(), "ANGER") != 0)
                {
                    if (strcmp(dComIfGp_getEventManager().getRunEventName(), "ANGER2") != 0 &&
                        z_aware_item_set_button(link, link->mEquipItem) >= 3)
                    {
                        link->allUnequip(1);
                    }
                }
            }
        }
    }

    return HOOK_SKIP_ORIGINAL;
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
    // SOFTLOCK FIX: same menu-aware gate as the quick-swap reservations — this
    // one is armed by Extra Item Slot ALONE (any mode), which is why the hang
    // also reproduced without Quick Swap.
    if (retval == nullptr || !extra_slot_reserves_left_dpad() || menu_system_engaged()) {
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
        // ============================================
        // FIX A: pre + skip (was post) — see on_check_item_change_from_button_pre.
        // ============================================
        !install(error, "CheckItemChangeFromButton",
                 mods::hook_add_pre<CheckItemChangeFromButton>(
                     svc_hook, on_check_item_change_from_button_pre)) ||
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
