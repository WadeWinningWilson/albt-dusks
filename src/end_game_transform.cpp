// ============================================
// End-Game Transform - RUNTIME-ONLY free wolf transform (ZERO save writes).
// Part of dev.albt.albw.
//
// SAVE-SAFETY (Code Red rewrite): the previous implementation ported the fork
// EDITOR toggle by WRITING story save bits every frame (M_077, M_067, M_011,
// F_0250 = Midna revived / Hyrule Castle barrier, + transform LV0-3). Those are
// story-progression state: forcing them onto a mid-game save changes how events
// unfold and corrupts saves. This version persists NOTHING.
//
// Mechanism - the dusk's own proven native path, identical to Focused Arts'
// Mortal Draw forced-wolf window (focused_arts_core.inc kickForce*Transform):
//   human -> wolf: player->onForceWolfChange();            link->checkGroundSpecialMode();
//   wolf -> human: player->onEndResetFlg0(ERFLG0_UNK_1);   link->checkWolfGroundSpecialMode();
// Both flags live in mEndResetFlg0, a runtime u32 cleared every frame - never
// saved. The native dispatchers run procCoMetamorphoseInit() with no story gate
// and no Midna dereference on this path (Midna is spawned unconditionally by
// Link at create; the wolf arc streams from disc via loadModelDVD's meta branch),
// and the wolf form works at transform LV0.
//
// Input: D-pad Down toggles the form. Precedence mirrors the fork
// (dpad_quick_swap.cpp:45-47 unbinds quick-transform from Down in favor of the
// D-pad Quick Swap): when Quick Swap is enabled it owns Down in HUMAN form
// (outfit cycle), so the transform fires only from wolf form; with Quick Swap
// off, Down transforms in both forms.
//
// D_MN08 (Hyrule Castle) is excluded: the native forced-wolf path sets event bit
// F_0776 there (procCoMetamorphoseInit, d_a_alink.cpp:19513) - the ONLY save
// write this mechanism could ever make - so the kick is skipped in that stage.
// ============================================

#include "end_game_transform.h"

#include "albw_common.h"
#include "albw_fork_compat.h"  // albw_checkMetamorphoseProcActive
#include "albw_game.h"         // albw_game::link_player
#include "config_vars.h"

#include "m_Do/m_Do_controller_pad.h"
#include "d/d_com_inf_game.h"
#include "d/actor/d_a_player.h"

#define private public
#include "d/actor/d_a_alink.h"
#undef private

#include <cstring>

void albw_end_game_transform_tick() {
    if (!albw_cfg_bool(g_end_game_transform, false)) {
        return;
    }

    daPy_py_c* player = albw_game::link_player();
    if (player == nullptr) {
        return;
    }
    daAlink_c* link = static_cast<daAlink_c*>(player);

    // Never re-kick mid-metamorphose.
    if (albw_checkMetamorphoseProcActive(link)) {
        return;
    }

    // Same frame guards Quick Swap uses: no heap lock, pause, message, or
    // stage transition (the kick can start a model load).
    const int heapLock = g_dComIfG_gameInfo.play.isHeapLockFlag();
    if (heapLock != 0 && heapLock != 5) {
        return;
    }
    if (g_dComIfG_gameInfo.play.isPauseFlag() ||
        g_dComIfG_gameInfo.play.getMesgStatus() != 0 ||
        g_dComIfG_gameInfo.play.isEnableNextStage())
    {
        return;
    }

    if (mDoCPd_c::getTrigDown(PAD_1) == 0) {
        return;
    }

    // Hyrule Castle: the native forced path writes F_0776 there - stay save-clean.
    const char* stage = g_dComIfG_gameInfo.play.getStartStageName();
    if (stage != nullptr && std::strcmp(stage, "D_MN08") == 0) {
        return;
    }

    const bool wolf = daPy_py_c::checkNowWolf() != 0;

    // Fork precedence: D-pad Quick Swap owns Down in human form (outfit cycle).
    if (!wolf && albw_cfg_bool(g_dpad_quick_swap, false)) {
        return;
    }

    if (wolf) {
        player->onEndResetFlg0(daPy_py_c::ERFLG0_UNK_1);
        link->checkWolfGroundSpecialMode();
    } else {
        player->onForceWolfChange();
        link->checkGroundSpecialMode();
    }
}
