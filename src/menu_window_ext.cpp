// ============================================
// NEW CODE - ALBW Port (Ext Status wiring into the pause menu)
//
// Ports the d_menu_window.cpp side of the two-page start menu from the fork.
// Three things had to move rather than copy, each noted at its site:
//
//  1. dMw_c::mpMenuExtStatus is a fork-added MEMBER. It lives in
//     menu_ext_members.cpp, keyed by instance.
//  2. EXT_STATUS_OPEN/MOVE/CLOSE are enum values 0x23-0x25, past the end of
//     stock's 35-entry init_proc/move_proc tables (stock's enum stops at 0x22).
//     _execute is therefore replaced with the fork's own body plus a hybrid
//     dispatch: stock's exported tables for 0x00-0x22, these free functions for
//     the three new procs. Vanilla could not be left to run even for one frame,
//     because the frame that ENTERS ext-status would index init_proc[0x23] out
//     of bounds before any post-hook could intervene.
//  3. The fork's ext_status_* methods are members; here they are free functions
//     over dMw_c*, the same treatment already used for the alink/midna helpers.
//
// NOT ported: collect_move_proc and collect_close_proc also gained Level Editor
// deny branches (dusk::g_levelEditorSession). That is a different fork feature
// and is not in this mod, so only the Ext Status branch is taken.
// ============================================

#include "global.h"
#include <os.h>

#include "d/d_com_inf_game.h"
#include "d/d_meter2_info.h"
#include "d/d_lib.h"
#include "JSystem/JKernel/JKRExpHeap.h"
#include "m_Do/m_Do_ext.h"
#include "m_Do/m_Do_controller_pad.h"
#include "Z2AudioLib/Z2Instances.h"
#define private public
#include "d/d_menu_window.h"
#include "d/d_menu_collect.h"
#include "d/d_menu_ring.h"  // dMenu_Ring_c::advanceSelectItem (frozen-wheel fix)
#undef private

#include "menu_window_ext.h"
#include "menu_ext_status.h"
#include "menu_ext_members.h"
#include "albw_common.h"
#include "albw_game.h"
#include "config_vars.h"
#include "mods/hook.hpp"

#if TARGET_PC

// Stock's dispatch tables. Declared DUSK_GAME_DATA (dllimport) in stock's own
// d_menu_window.cpp, so the game exports them and this resolves at link.
typedef void (dMw_c::*procFunc)();
typedef void (dMw_c::*initFunc)(u8);
DUSK_GAME_DATA extern initFunc init_proc[];
DUSK_GAME_DATA extern procFunc move_proc[];

namespace {

// stock d_menu_window.cpp:218 - a file-static there, so not linkable from a mod.
// Three lines, reproduced exactly rather than resolved by symbol.
BOOL albw_mw_is_menu_ring() {
    dMw_c* menu_window = dMeter2Info_getMenuWindowClass();
    if (menu_window != NULL) {
        return menu_window->isShowFlag();
    }
    return false;
}

// fork d_menu_window.h:83-85
constexpr u8 kExtStatusOpen  = 0x23;
constexpr u8 kExtStatusMove  = 0x24;
constexpr u8 kExtStatusClose = 0x25;
constexpr u8 kStockProcMax   = 0x22;  // stock's last enum: INSECT_AGITHA_CLOSE

DEFINE_HOOK(&dMw_c::_execute, MwExecute);
DEFINE_HOOK(&dMw_c::collect_move_proc, MwCollectMove);
DEFINE_HOOK(&dMw_c::_delete, MwDelete);

// ---- the fork's ext_status_* procs, as free functions (fork :1275-1334) -----

void ext_status_create(dMw_c* mw) {
    if (albw_mw_ext_status(mw) != nullptr) {
        return;
    }
    dMenu_ExtStatus_c* page = JKR_NEW dMenu_ExtStatus_c(mw->mpStick, mw->mpCStick);
    albw_mw_set_ext_status(mw, page);
    if (page != nullptr) {
        page->_create();
    }
}

bool ext_status_delete(dMw_c* mw) {
    dMenu_ExtStatus_c* page = albw_mw_ext_status(mw);
    if (page != nullptr) {
        page->_delete();
        JKR_DELETE(page);
        albw_mw_set_ext_status(mw, nullptr);
    }
    return true;
}

void ext_status_open_init(dMw_c* mw, u8) {
    dMeter2Info_setWindowStatus(11);
    ext_status_create(mw);
}

void ext_status_open_proc(dMw_c* mw) { mw->mMenuProc = kExtStatusMove; }

void ext_status_move_proc(dMw_c* mw) {
    dMenu_ExtStatus_c* page = albw_mw_ext_status(mw);
    if (page == nullptr) {
        mw->mMenuProc = dMw_c::COLLECT_MOVE;
        dMeter2Info_setWindowStatus(3);
        return;
    }
    page->_move();
    if (page->wantsClose()) {
        mw->mMenuProc = kExtStatusClose;
        return;
    }
    const s8 handoff = page->getCollectHandoff();
    if (handoff >= 0) {
        page->clearCollectHandoff();
        ext_status_delete(mw);
        dMeter2Info_setWindowStatus(3);
        mw->mMenuProc = dMw_c::COLLECT_MOVE;
        Z2GetAudioMgr()->seStart(Z2SE_SY_CURSOR_OK, NULL, 0, 0, 1.0f, 1.0f, -1.0f, -1.0f, 0);
    }
}

void ext_status_close_proc(dMw_c* mw) {
    ext_status_delete(mw);
    // Close entire pause (Collect was kept alive under Ext Status).
    mw->mMenuProc = dMw_c::COLLECT_CLOSE;
}

}  // namespace

namespace {

// ---- _execute replacement (fork body + hybrid dispatch) ---------------------
// Ported from stock d_menu_window.cpp:1595 verbatim except the dispatch pair,
// which routes 0x23-0x25 to the free functions above instead of indexing past
// the end of stock's tables.
HookAction on_mw_execute_pre(ModContext*, void* args, void* retval, void*) {
    auto* mw = mods::arg<dMw_c*>(args, 0);
    if (mw == nullptr || retval == nullptr) {
        return HOOK_CONTINUE;
    }

    // ============================================
    // FROZEN-WHEEL FIX: stock _execute's FIRST act (d_menu_window.cpp:1598-1602)
    // is the per-frame ring stepper - it advances the staged select-item counters
    // (dMenu_Ring_c::field_0x674[i], armed by setItem on an assignment) and
    // commits at 10 via setSelectItemForce. This replacement omitted it, so on
    // the stock binary every X/Y assignment froze at 674[i]==1: the item never
    // finished assigning and isClose (which waits on 674==0) never let the menu
    // shut. Restored verbatim.
    // ============================================
    if (mw->mpMenuRing != NULL) {
        mw->mpMenuRing->advanceSelectItem();
    }

    if (mw->field_0x151 != 0) {
        mw->field_0x151--;
    }

    JKRHeap* prev_heap = mDoExt_setCurrentHeap(static_cast<JKRHeap*>(mw->mpHeap));
    const u8 prev_proc = mw->mMenuProc;
    mw->mpStick->checkTrigger();

    if (albw_mw_is_menu_ring()) {
        mw->mpCStick->checkTrigger();
        mw->checkCStickTrigger();
    }

    if (dComIfGp_event_runCheck()) {
        mw->field_0x148 = 5;
    } else if (mw->field_0x148 > 0) {
        mw->field_0x148--;
    } else {
        mw->field_0x148 = 0;
    }

    // ---- hybrid dispatch -----------------------------------------------
    if (mw->mMenuProc == kExtStatusOpen)       ext_status_open_proc(mw);
    else if (mw->mMenuProc == kExtStatusMove)  ext_status_move_proc(mw);
    else if (mw->mMenuProc == kExtStatusClose) ext_status_close_proc(mw);
    else if (mw->mMenuProc <= kStockProcMax)   (mw->*move_proc[mw->mMenuProc])();

    if (mw->mMenuProc != prev_proc) {
        if (mw->mMenuProc == kExtStatusOpen)       ext_status_open_init(mw, prev_proc);
        else if (mw->mMenuProc == kExtStatusMove)  { /* fork: ext_status_move_init is empty */ }
        else if (mw->mMenuProc == kExtStatusClose) { /* fork: ext_status_close_init is empty */ }
        else if (mw->mMenuProc <= kStockProcMax)   (mw->*init_proc[mw->mMenuProc])(prev_proc);
    }

    if (!mDoCPd_c::getHoldLockL(PAD_1) && mw->dMw_isButtonBit(1)) {
        mw->dMw_offButtonBit(1);
    }
    if (!mDoCPd_c::getHoldLockR(PAD_1) && mw->dMw_isButtonBit(2)) {
        mw->dMw_offButtonBit(2);
    }

    mDoExt_setCurrentHeap(prev_heap);
    *static_cast<int*>(retval) = 1;
    return HOOK_SKIP_ORIGINAL;
}

// ---- the Ext Status entry point (fork collect_move_proc:902-908) ------------
// Only the Ext Status branch is ported; the fork's other addition here is Level
// Editor deny, which belongs to a feature this mod does not carry.
HookAction on_mw_collect_move_pre(ModContext*, void* args, void*, void*) {
    auto* mw = mods::arg<dMw_c*>(args, 0);
    if (mw == nullptr || mw->mpMenuCollect == nullptr) {
        return HOOK_CONTINUE;
    }
    if (!albw_cfg_bool(g_ext_status_page, false)) {
        return HOOK_CONTINUE;
    }
    // Sibling Ext Status (Tools/Quest/Atlas) - L/R while Collect has no submenu.
    if (mw->mpMenuCollect->getSubWindowOpenCheck() == 0 && !mw->mpMenuCollect->isKeyCheck() &&
        (dMw_LEFT_TRIGGER() || dMw_RIGHT_TRIGGER()))
    {
        mw->mMenuProc = kExtStatusOpen;
        return HOOK_SKIP_ORIGINAL;
    }
    return HOOK_CONTINUE;
}

// dMw_c is created and destroyed per pause session (stock dMw_Create /
// dMw_Delete), so its address changes between opens. Without this the side-table
// slot stayed claimed by a dead instance and the table filled after a few pauses.
void on_mw_delete_post(ModContext*, void* args, void*, void*) {
    auto* mw = mods::arg<dMw_c*>(args, 0);
    if (mw == nullptr) {
        return;
    }
    ext_status_delete(mw);   // page is owned by us; never leak it with the window
    albw_mw_release(mw);
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

ModResult albw_menu_window_ext_init(ModError* error) {
    if (!install(error, "MwExecuteExtStatus",
                 mods::hook_add_pre<MwExecute>(svc_hook, on_mw_execute_pre)) ||
        !install(error, "MwCollectMoveExtStatus",
                 mods::hook_add_pre<MwCollectMove>(svc_hook, on_mw_collect_move_pre)) ||
        !install(error, "MwDeleteReleaseExtStatus",
                 mods::hook_add_post<MwDelete>(svc_hook, on_mw_delete_post)))
    {
        return MOD_ERROR;
    }
    return MOD_OK;
}

#endif  // TARGET_PC

// ============================================
// NEW CODE ENDS HERE
// ============================================
