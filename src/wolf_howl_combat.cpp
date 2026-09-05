// ============================================
// NEW CODE - ALBW Port (Wolf Art: combat Howl - the MOVE)
//
// The mod already had the burst trigger (wolf_arts.cpp) and the shop unlock;
// what it triggered was a PLAIN howl, because the fork's combat-howl machinery
// in d_a_alink_wolf.inc / d_a_alink.cpp was never ported. This TU carries it:
//
//   procWolfHowlInit combat branch   wolf.inc:3521-3543  (duet pose + AOE arm)
//   procWolfHowl dispatch            wolf.inc:3567       (combat lifecycle)
//   setWolfHowlSpinEffect            wolf.inc:3627       (KAITENGIRIL ring)
//   procWolfHowlCombat               wolf.inc:3682       (song-driven duration)
//   setWolfAtCollision AOE case      d_a_alink.cpp:6958  (Link-centered radial)
//   commonProcInit cleanup           d_a_alink.cpp:16403 (interruption chokepoint)
//
// Bodies are verbatim; the fork's five added daAlink_c members (mWolfHowl* +
// mWolfCombatHowlActive) live module-side here, the same translation the rest
// of the wolf module uses. The fork's VFX tuner reads host settings
// (game.wolfHowlVfxOverride + sliders) that stock does not have - the BAKED
// defaults ship, which the fork's own comment calls "the finalized shipping
// look"; the tuner was a dev tool.
// ============================================

#include "global.h"
#include <os.h>

#include "SSystem/SComponent/c_math.h"
#include "d/d_com_inf_game.h"
#include "d/d_particle_name.h"
#define private public
#include "d/actor/d_a_alink.h"
#undef private
#include "Z2AudioLib/Z2Instances.h"

#include "albw_common.h"
#include "albw_game.h"
#include "wolf_combat.h"
#include "mods/hook.hpp"

#if TARGET_PC

namespace {

DEFINE_HOOK(&daAlink_c::procWolfHowlInit, ProcWolfHowlInit);
DEFINE_HOOK(&daAlink_c::procWolfHowl, ProcWolfHowl);
DEFINE_HOOK(&daAlink_c::setWolfAtCollision, SetWolfAtCollision);
DEFINE_HOOK(&daAlink_c::commonProcInit, CommonProcInit);

// fork daAlink_c added members (d_a_alink.h:4661-4666), module-side.
bool s_combatHowlActive = false;
bool s_howlWantCombat   = false;
bool s_howlEnding       = false;
u16  s_howlElapsed      = 0;
u16  s_howlFramesLeft   = 0;

// fork wolf.inc baked VFX defaults ("FINAL LOOK, user-tuned 2026-07-13").
const int l_wolfHowlVfxPeriod    = 1;
const f32 l_wolfHowlVfxYOffset   = 30.0f;
const s16 l_wolfHowlVfxSweepRate = 300;
const int l_wolfHowlVfxWidthPct  = 90;
const int l_wolfHowlVfxHeightPct = 90;

// fork daAlink_c::setWolfHowlSpinEffect (wolf.inc:3627), baked-default path.
void setWolfHowlSpinEffect(daAlink_c* link) {
    const s16 sweep = (s16)(link->shape_angle.y + (s16)(s_howlElapsed * l_wolfHowlVfxSweepRate));

    cXyz  fxPos(link->current.pos.x, link->current.pos.y + l_wolfHowlVfxYOffset,
                link->current.pos.z);
    csXyz fxRot(0, sweep, 0);

    cXyz scaleVec((f32)l_wolfHowlVfxWidthPct * 0.01f, (f32)l_wolfHowlVfxHeightPct * 0.01f,
                  (f32)l_wolfHowlVfxWidthPct * 0.01f);

    // Base KAITENGIRIL (A-D), fire-and-forget with NULL tevStr (4-arg overload
    // keeps the effect's own colors + particles).
    static const u16 effNames[4] = {
        ID_ZI_J_KAITENGIRIL_A, ID_ZI_J_KAITENGIRIL_B, ID_ZI_J_KAITENGIRIL_C, ID_ZI_J_KAITENGIRIL_D,
    };
    for (int i = 0; i < 4; i++) {
        // fork uses the 4-arg overload = NULL tevStr (keeps the effect's own colors).
        albw_game::particle_set(effNames[i], &fxPos, nullptr, &fxRot, &scaleVec);
    }
}

// fork daAlink_c::procWolfHowlCombat (wolf.inc:3682) - verbatim over the
// module-side fields.
int procWolfHowlCombat(daAlink_c* link) {
    // Exit phase: the WANM_HOWL_END anim is playing -> return to gameplay when it finishes.
    if (s_howlEnding) {
        if (link->checkAnmEnd(link->mUnderFrameCtrl)) {
            s_combatHowlActive = false;
            s_howlEnding       = false;
            link->checkNextActionWolf(0);
        }
        return 1;
    }

    if (s_howlElapsed < 0xFFFF) {
        s_howlElapsed++;
    }

    const bool songPlaying = Z2GetSeqMgr()->isItemGetDemo();
    const bool songEnded   = s_howlElapsed > 15 && !songPlaying;
    const bool hardCap     = s_howlElapsed > 1800;  // 60 s failsafe (30 Hz sim tick)

    if (songEnded || hardCap) {
        if (s_howlFramesLeft > 0) {
            s_howlFramesLeft--;  // 1 s buffer after the song, for a clean return to gameplay
        } else {
            Z2GetSeqMgr()->stopWolfHowlSong();  // safety (already ended); field BGM fades back
            link->setSingleAnimeWolfBase(daAlink_c::WANM_HOWL_END);
            s_howlEnding = true;
        }
    } else {
        s_howlFramesLeft = 30;  // still singing -> keep the 1 s (30 Hz) buffer fresh
        if ((s_howlElapsed - 1) % l_wolfHowlVfxPeriod == 0) {
            setWolfHowlSpinEffect(link);
        }
    }

    // Keep the Link-centered AOE collider armed while singing.
    link->onResetFlg0(daPy_py_c::RFLG0_UNK_2);
    return 1;
}

// fork procWolfHowlInit combat branch (wolf.inc:3507-3543). When the one-shot
// combat request is armed, this pre-hook runs the fork's whole init (guard +
// commonProcInit + combat body) and skips vanilla; a vanilla demo howl leaves
// the request false and falls straight through to the stock body.
HookAction on_proc_wolf_howl_init_pre(ModContext*, void* args, void* retval, void*) {
    auto* link = mods::arg<daAlink_c*>(args, 0);
    if (link == nullptr || retval == nullptr) {
        return HOOK_CONTINUE;
    }
    // Consume the request exactly once, whichever path runs.
    const bool wantCombat = s_howlWantCombat;
    s_howlWantCombat = false;
    if (!wantCombat) {
        s_combatHowlActive = false;
        return HOOK_CONTINUE;
    }

    if (link->checkEventRun() && link->mProcID == daAlink_c::PROC_WOLF_HOWL) {
        *static_cast<int*>(retval) = 0;
        return HOOK_SKIP_ORIGINAL;
    }
    link->commonProcInit(daAlink_c::PROC_WOLF_HOWL);
    s_combatHowlActive = true;

    // The DUET "singing with the Golden Wolf" pose (WANM_HOWL_SUCCESS / WL_HOWLC), looped.
    link->setSingleAnimeWolfBase(daAlink_c::WANM_HOWL_SUCCESS);
    link->mUnderFrameCtrl[0].setLoop(27);
    link->mUnderFrameCtrl[0].setAttribute(2);
    link->setFaceBasicTexture(daAlink_c::FTANM_WL_HOWLC);
    link->mProcVar3.field_0x300e = 0;
    link->mProcVar2.field_0x300c = 0;
    link->mNormalSpeed = 0.0f;
    // Link-centered radial AOE collider (roll-attack pattern; placed by
    // setWolfAtCollision's howl case, armed each singing frame).
    link->setCylAtParam(AT_TYPE_WOLF_CUT_TURN, dCcG_At_Spl_UNK_1, 3, dCcD_SE_WOLF_BITE, 8, 300.0f,
                        155.0f);
    *static_cast<int*>(retval) = 1;
    return HOOK_SKIP_ORIGINAL;
}

// fork procWolfHowl dispatch (wolf.inc:3567).
HookAction on_proc_wolf_howl_pre(ModContext*, void* args, void* retval, void*) {
    auto* link = mods::arg<daAlink_c*>(args, 0);
    if (link == nullptr || retval == nullptr || !s_combatHowlActive) {
        return HOOK_CONTINUE;
    }
    *static_cast<int*>(retval) = procWolfHowlCombat(link);
    return HOOK_SKIP_ORIGINAL;
}

// fork setWolfAtCollision howl case (d_a_alink.cpp:6958): the combat howl uses
// the same Link-centered radial collider as PROC_WOLF_ROLL_ATTACK. That case's
// body, verbatim (vibration line + SetC + Ccsp Set/SetMass), then skip vanilla
// (whose else-branch would mis-place the cylinder ahead of the wolf).
HookAction on_set_wolf_at_collision_pre(ModContext*, void* args, void*, void*) {
    auto* link = mods::arg<daAlink_c*>(args, 0);
    if (link == nullptr || !s_combatHowlActive ||
        link->mProcID != daAlink_c::PROC_WOLF_HOWL)
    {
        return HOOK_CONTINUE;
    }
    if (!link->checkResetFlg0(daPy_py_c::RFLG0_UNK_2)) {
        return HOOK_SKIP_ORIGINAL;  // stock does nothing without the arm flag
    }
    if (!link->setSwordHitVibration(&link->mAtCyl) && !link->setSwordHitVibration(&link->mTgCyls[0]) &&
        !link->setSwordHitVibration(&link->mTgCyls[1]))
    {
        link->setSwordHitVibration(&link->mTgCyls[2]);
    }
    link->mAtCyl.SetC(link->current.pos);
    dComIfG_Ccsp()->Set(&link->mAtCyl);
    dComIfG_Ccsp()->SetMass(&link->mAtCyl, 1);
    return HOOK_SKIP_ORIGINAL;
}

// fork commonProcInit cleanup (d_a_alink.cpp:16403): any proc other than the
// howl taking over clears the combat flags - without this an interrupted howl
// left them stuck TRUE, which (among other things) permanently blocked the
// wolf-charge counter via the earn guard.
HookAction on_common_proc_init_pre(ModContext*, void* args, void*, void*) {
    auto procID = mods::arg<daAlink_c::daAlink_PROC>(args, 1);
    if (s_combatHowlActive && procID != daAlink_c::PROC_WOLF_HOWL) {
        s_combatHowlActive = false;
        s_howlEnding       = false;
    }
    return HOOK_CONTINUE;
}

bool install(ModError* error, const char* name, ModResult r) {
    if (r != MOD_OK) {
        if (svc_log != nullptr) svc_log->error(mod_ctx, name);
        mods::set_error(error, MOD_ERROR, name);
        return false;
    }
    return true;
}

}  // namespace

// fork handleWolfHowlBurst's trigger side-effects (d_a_alink_dusk.cpp:157-160):
// arm the one-shot combat request; the pre-hook on procWolfHowlInit consumes it.
void albw_wolf_howl_arm_combat_request() {
    s_howlWantCombat = true;
    s_howlEnding     = false;
    s_howlElapsed    = 0;
    s_howlFramesLeft = 30;  // 1 s post-song buffer at the 30 Hz sim tick
}

// fork daAlink_c::mWolfCombatHowlActive, for the earn guard (d_cc_uty.cpp:738)
// and the howl-AOE power boost (d_cc_uty.cpp:512).
bool albw_wolf_combat_howl_active() {
    return s_combatHowlActive;
}

ModResult albw_wolf_howl_combat_init(ModError* error) {
    if (!install(error, "ProcWolfHowlInitCombat",
                 mods::hook_add_pre<ProcWolfHowlInit>(svc_hook, on_proc_wolf_howl_init_pre)) ||
        !install(error, "ProcWolfHowlCombat",
                 mods::hook_add_pre<ProcWolfHowl>(svc_hook, on_proc_wolf_howl_pre)) ||
        !install(error, "SetWolfAtCollisionHowlAoe",
                 mods::hook_add_pre<SetWolfAtCollision>(svc_hook, on_set_wolf_at_collision_pre)) ||
        !install(error, "CommonProcInitHowlCleanup",
                 mods::hook_add_pre<CommonProcInit>(svc_hook, on_common_proc_init_pre)))
    {
        return MOD_ERROR;
    }
    svc_log->info(mod_ctx, "albw wolf combat-howl move ready");
    return MOD_OK;
}

#endif  // TARGET_PC

// ============================================
// NEW CODE ENDS HERE
// ============================================
