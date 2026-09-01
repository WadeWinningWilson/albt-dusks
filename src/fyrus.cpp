// Fyrus Boss Refinement wiring — part of dev.albt.albw.
//
// ============================================
// NEW CODE - ALBW Boss Refinement (Fyrus, Pass 2)
// Fork source: d_a_e_fm.cpp. Method:
// docs/state/boss-refinement-mod-port-method.md in the fork repo.
//
// The refinement BRAIN is already ported (boss_refinement.cpp carries every
// dAlbwBoss_fyrus* helper). This file is wiring only.
//
// The fork's sites in vanilla functions LOOK like control-flow guards:
//
//     if (!dAlbwBoss_fyrusStayHollow()) { ...vanilla body... }
//
// but every guarded body is a STATE WRITE, so they port additively: record the
// field in a pre-hook, let vanilla run untouched, then correct in post. No
// vanilla code is copied. (Reading the bodies matters - judging these by their
// if-shape alone led to a 2,670-line copy estimate that was simply wrong.)
//
// Reachability: daE_FM_Create, daE_FM_Execute, e_fm_down, e_fm_normal,
// e_fm_fight_run, e_fm_stop, e_fm_damage_run are uniquely named and resolve
// through the symgen manifest. damage_check (53 actors) and action (95) are
// duplicate names; their sites are handled by consuming the core Tg hit from
// the Execute pre-hook before vanilla's damage_check ever reads it.
// ============================================

#include "fyrus.h"

#include "albw_common.h"
#include "albw_game.h"
#include "boss_refinement.h"
#include "modules.h"

#include "d/actor/d_a_e_fm.h"
#include "d/d_com_inf_game.h"
#include "f_op/f_op_actor.h"
#include "mods/svc/hook.hpp"

namespace {

// ============================================
// Mirrors of the TU-local enums in d_a_e_fm.cpp. Both are declaration-order
// enums with no explicit values except ACTION_DOWN, so the indices below are
// read straight off the fork's declarations (d_a_e_fm.cpp:158 and :178) - NOT
// inferred. mPlayTexAnmNo indexes mpFmBrk[]/mpFmBtk[], so a wrong value here is
// an out-of-slot animation, which is what crashed the first cut of this file.
//
//   enum daE_FM_ACTION { NORMAL, FIGHT_RUN, N_FIGHT, F_FIGHT, DAMAGE_RUN,
//                        ANIMAL, FIRE, STOP, ACTION_DOWN = 9, A_DOWN,
//                        START, END, ... }
//   enum { TEXANM_FM, TEXANM_ATTACK, TEXANM_PUTOUT, TEXANM_PUTOUT_WAIT,
//          TEXANM_ANIMAL, TEXANM_OP_DEMO, TEXANM_HANG_WAIT, ... }
// ============================================
constexpr s16 kActionNormal = 0;

constexpr u8 kTexAnmFm = 0;
constexpr u8 kTexAnmPutOutWait = 3;
constexpr u8 kTexAnmAnimal = 4;

DEFINE_HOOK_SYMBOL("daE_FM_Create", int(fopAc_ac_c*), FmCreate);
DEFINE_HOOK_SYMBOL("daE_FM_Execute", int(e_fm_class*), FmExecute);
DEFINE_HOOK_SYMBOL("e_fm_down", signed char(e_fm_class*), FmDown);
DEFINE_HOOK_SYMBOL("e_fm_normal", void(e_fm_class*), FmNormal);
DEFINE_HOOK_SYMBOL("e_fm_fight_run", void(e_fm_class*), FmFightRun);
DEFINE_HOOK_SYMBOL("e_fm_stop", void(e_fm_class*), FmStop);
DEFINE_HOOK_SYMBOL("e_fm_damage_run", void(e_fm_class*), FmDamageRun);

// ---- shared record state ----------------------------------------------------
struct Recorded {
    s16 action;
    s16 mode;
    s16 health;
    u8 texAnm;
    u8 f792;
    s16 f7c0;
    bool armed;
};
Recorded s_down{};
Recorded s_normal{};
Recorded s_fightRun{};
Recorded s_damageRun{};
Recorded s_stop{};

void record(Recorded& r, e_fm_class* fm) {
    r.action = fm->mAction;
    r.mode = fm->mMode;
    r.health = fm->health;
    r.texAnm = fm->mPlayTexAnmNo;
    r.f792 = fm->field_0x792;
    r.f7c0 = fm->field_0x7c0;
    r.armed = true;
}

bool begin(Recorded& r, void* args, e_fm_class** out) {
    if (!dAlbwBossRefinement_isEnabled()) {
        r.armed = false;
        return false;
    }
    auto* fm = mods::arg<e_fm_class*>(args, 0);
    if (fm == nullptr) {
        r.armed = false;
        return false;
    }
    record(r, fm);
    *out = fm;
    return true;
}

e_fm_class* finish(Recorded& r, void* args) {
    if (!r.armed) {
        return nullptr;
    }
    r.armed = false;
    return mods::arg<e_fm_class*>(args, 0);
}

// ---- e_fm_down --------------------------------------------------------------
// Fork guards three vanilla blocks with !fyrusStayHollow():
//   :2388  mPlayTexAnmNo = TEXANM_ANIMAL (+ brk/btk frame reset)
//   :2405  field_0x792 = 1; changeBgmStatus(4)
//   :2428  TEXANM_FM  ... else TEXANM_PUTOUT_WAIT
// Hollow means "stay burnt out": undo the texture/flag transitions vanilla made,
// and apply the else-branch texture the fork selects instead.
HookAction on_down_pre(ModContext*, void* args, void*, void*) {
    e_fm_class* fm = nullptr;
    begin(s_down, args, &fm);
    return HOOK_CONTINUE;
}

void on_down_post(ModContext*, void* args, void*, void*) {
    e_fm_class* fm = finish(s_down, args);
    if (fm == nullptr || !dAlbwBoss_fyrusStayHollow()) {
        return;
    }
    // The fork guards three blocks with !fyrusStayHollow(), each in a distinct
    // mMode arm of e_fm_down's switch. Branch on the mMode recorded BEFORE
    // vanilla ran, so the same arm is identified rather than inferred from the
    // resulting field values.
    switch (s_down.mode) {
    case 3:
        // fork :2388 - hollow does not light the animal texture.
        if (fm->mPlayTexAnmNo == kTexAnmAnimal && s_down.texAnm != kTexAnmAnimal) {
            fm->mPlayTexAnmNo = s_down.texAnm;
        }
        break;
    case 4:
        // fork :2405 - hollow keeps the fire-out flag clear and leaves BGM alone.
        if (fm->field_0x792 == 1 && s_down.f792 == 0) {
            fm->field_0x792 = s_down.f792;
        }
        // fork :2428 - the else-branch selects PUTOUT_WAIT instead of FM.
        if (fm->mPlayTexAnmNo == kTexAnmFm && s_down.texAnm != kTexAnmFm) {
            fm->mPlayTexAnmNo = kTexAnmPutOutWait;
            if (fm->mpFmBrk[kTexAnmPutOutWait] != nullptr) {
                fm->mpFmBrk[kTexAnmPutOutWait]->setFrame(0.0f);
            }
            if (fm->mpFmBtk[kTexAnmPutOutWait] != nullptr) {
                fm->mpFmBtk[kTexAnmPutOutWait]->setFrame(0.0f);
            }
        }
        break;
    default:
        break;
    }
}

// ---- e_fm_normal ------------------------------------------------------------
// :953  if (!fyrusGolemKidsLoose()) { ... mAction = ACTION_FIGHT_RUN ... }
// :971  field_0x7c0 = fyrusGolemKidsLoose() ? 0 : 1
// While the golem kids are loose Fyrus must not re-enter the fight run.
HookAction on_normal_pre(ModContext*, void* args, void*, void*) {
    e_fm_class* fm = nullptr;
    begin(s_normal, args, &fm);
    return HOOK_CONTINUE;
}

void on_normal_post(ModContext*, void* args, void*, void*) {
    e_fm_class* fm = finish(s_normal, args);
    if (fm == nullptr || !dAlbwBoss_fyrusGolemKidsLoose()) {
        return;
    }
    if (fm->mAction != s_normal.action) {
        fm->mAction = s_normal.action;
        fm->mMode = s_normal.mode;
    }
    fm->field_0x7c0 = 0;
}

// ---- e_fm_fight_run ---------------------------------------------------------
// :1051 kids loose -> ACTION_NORMAL, mMode 0, speedF 0, early return
// :1117 !ablazePhase() guards the chain-yank handoff to ACTION_STOP
HookAction on_fight_run_pre(ModContext*, void* args, void*, void*) {
    e_fm_class* fm = nullptr;
    begin(s_fightRun, args, &fm);
    return HOOK_CONTINUE;
}

void on_fight_run_post(ModContext*, void* args, void*, void*) {
    e_fm_class* fm = finish(s_fightRun, args);
    if (fm == nullptr) {
        return;
    }
    if (dAlbwBoss_fyrusGolemKidsLoose()) {
        fm->mAction = kActionNormal;
        fm->mMode = 0;
        fm->speedF = 0.0f;
        return;
    }
    // Ablaze holds the chain yank: undo vanilla's transition into ACTION_STOP.
    if (dAlbwBoss_fyrusAblazePhase() && fm->mAction != s_fightRun.action) {
        fm->mAction = s_fightRun.action;
        fm->mMode = s_fightRun.mode;
    }
}

// ---- e_fm_damage_run --------------------------------------------------------
// :1593 same !ablazePhase() guard on the chain-yank handoff.
HookAction on_damage_run_pre(ModContext*, void* args, void*, void*) {
    e_fm_class* fm = nullptr;
    begin(s_damageRun, args, &fm);
    return HOOK_CONTINUE;
}

void on_damage_run_post(ModContext*, void* args, void*, void*) {
    e_fm_class* fm = finish(s_damageRun, args);
    if (fm == nullptr || !dAlbwBoss_fyrusAblazePhase()) {
        return;
    }
    if (fm->mAction != s_damageRun.action) {
        fm->mAction = s_damageRun.action;
        fm->mMode = s_damageRun.mode;
    }
}

// ---- e_fm_stop --------------------------------------------------------------
// :1482 refinement keeps ONE pool - do not snap health back to 50 (or 200).
HookAction on_stop_pre(ModContext*, void* args, void*, void*) {
    e_fm_class* fm = nullptr;
    begin(s_stop, args, &fm);
    return HOOK_CONTINUE;
}

void on_stop_post(ModContext*, void* args, void*, void*) {
    e_fm_class* fm = finish(s_stop, args);
    if (fm == nullptr) {
        return;
    }
    if (fm->health == 50 && s_stop.health != 50) {
        fm->health = s_stop.health;
    }
}

// ---- daE_FM_Create ----------------------------------------------------------
// :4469 seed the refinement HP pool (50 vanilla / 200 refinement).
void on_create_post(ModContext*, void* args, void*, void*) {
    if (!dAlbwBossRefinement_isEnabled()) {
        return;
    }
    auto* actor = mods::arg<fopAc_ac_c*>(args, 0);
    if (actor == nullptr) {
        return;
    }
    dAlbwBoss_fyrusResetFightState();
    actor->health = dAlbwBoss_fyrusCreateHp();
    actor->field_0x560 = actor->health;
}

// ---- daE_FM_Execute ---------------------------------------------------------
// Per-frame golem window update, plus the damage_check suppression: consume the
// core Tg hit here so vanilla's (duplicate-named, unhookable) damage_check never
// sees it, and run the refinement damage path in its place.
//
// Timing is safe: Ccsp()->Move() runs in dScnPly_Draw, so the Tg flag set at the
// end of frame N-1 is live throughout frame N's Execute. Only damage_check and
// e_fm_tryAlbwAnmPreview read mCoreSph's Tg hit, so consuming it is contained.
HookAction on_execute_pre(ModContext*, void* args, void*, void*) {
    if (!dAlbwBossRefinement_isEnabled()) {
        return HOOK_CONTINUE;
    }
    auto* fm = mods::arg<e_fm_class*>(args, 0);
    if (fm == nullptr) {
        return HOOK_CONTINUE;
    }

    dAlbwBoss_fyrusUpdateGolemWindow(fm);

    if (!fm->mCoreSph.ChkTgHit()) {
        return HOOK_CONTINUE;
    }
    fm->mAtInfo.mpCollider = fm->mCoreSph.GetTgHitObj();

    if (dAlbwBoss_fyrusAblazePhase() && dAlbwBoss_fyrusAblazeVulnOpen()) {
        cc_at_check(fm, &fm->mAtInfo);
        dAlbwBoss_fyrusOnAblazeVulnDamaged();
        dAlbwBoss_fyrusSyncFireVulnState(fm);
    } else if (dAlbwBoss_fyrusShouldChipAblazeDamage()) {
        const int hpBefore = fm->health;
        cc_at_check(fm, &fm->mAtInfo);
        dAlbwBoss_fyrusApplyChipDamage(fm, hpBefore);
    } else {
        return HOOK_CONTINUE;  // no refinement claim on this hit - leave it to vanilla
    }

    fm->mCoreSph.ClrTgHit();
    return HOOK_CONTINUE;
}

// A boss hook that fails to resolve must not abort mod_initialize.
void try_install(const char* what, ModResult result) {
    if (result != MOD_OK) {
        svc_log->error(mod_ctx, what);
        svc_log->error(mod_ctx, "fyrus: hook above did NOT install - that "
                                "refinement site is inactive this run");
    }
}

}  // namespace

ModResult albw_fyrus_init(ModError*) {
    try_install("daE_FM_Create", mods::hook::add_post<FmCreate>(on_create_post));
    try_install("daE_FM_Execute", mods::hook::add_pre<FmExecute>(on_execute_pre));
    try_install("e_fm_down:pre", mods::hook::add_pre<FmDown>(on_down_pre));
    try_install("e_fm_down:post", mods::hook::add_post<FmDown>(on_down_post));
    try_install("e_fm_normal:pre", mods::hook::add_pre<FmNormal>(on_normal_pre));
    try_install("e_fm_normal:post", mods::hook::add_post<FmNormal>(on_normal_post));
    try_install("e_fm_fight_run:pre", mods::hook::add_pre<FmFightRun>(on_fight_run_pre));
    try_install("e_fm_fight_run:post", mods::hook::add_post<FmFightRun>(on_fight_run_post));
    try_install("e_fm_damage_run:pre", mods::hook::add_pre<FmDamageRun>(on_damage_run_pre));
    try_install("e_fm_damage_run:post", mods::hook::add_post<FmDamageRun>(on_damage_run_post));
    try_install("e_fm_stop:pre", mods::hook::add_pre<FmStop>(on_stop_pre));
    try_install("e_fm_stop:post", mods::hook::add_post<FmStop>(on_stop_post));
    svc_log->info(mod_ctx, "albw fyrus refinement hooks ready");
    return MOD_OK;
}

ModResult albw_fyrus_shutdown(ModError*) {
    s_down.armed = s_normal.armed = s_fightRun.armed = s_damageRun.armed = s_stop.armed = false;
    return MOD_OK;
}
