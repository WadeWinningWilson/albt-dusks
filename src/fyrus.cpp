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

#include "SSystem/SComponent/c_cc_d.h"
#include "d/actor/d_a_e_fm.h"
#include "d/d_cc_uty.h"
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
constexpr s16 kActionStop = 7;  // daE_FM_ACTION ACTION_STOP (the chain-yank handoff)

// ============================================
// NEW CODE - ALBW Port (E_FM look-pass) - the remaining daE_FM_ACTION mirrors.
// Same declaration-order read as above; the fork's three added values sit past
// stock's ACTION_END=12 (fork d_a_e_fm.cpp:158-176), which is exactly how the
// fork parks the boss in states vanilla's switch cannot dispatch.
// ============================================
constexpr s16 kActionDown = 9;        // ACTION_DOWN
constexpr s16 kActionADown = 10;      // ACTION_A_DOWN
constexpr s16 kActionStart = 11;      // ACTION_START
constexpr s16 kActionEnd = 12;        // ACTION_END
constexpr s16 kActionGolemHold = 14;  // ACTION_ALBW_GOLEM_HOLD
constexpr s16 kActionAblazeStun = 15; // ACTION_ALBW_ABLAZE_STUN

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
    // ============================================
    // Ablaze holds the chain yank: undo vanilla's transition into ACTION_STOP.
    //
    // BUGFIX (Fyrus never attacked): this used to revert on
    // `fm->mAction != s_fightRun.action`, i.e. ANY action change fight_run
    // made. Stock fight_run's whole job is to leave for ACTION_F_FIGHT /
    // ACTION_N_FIGHT once Link is in range (stock d_a_e_fm.cpp:723-731), so the
    // broad condition snapped every attack transition straight back to
    // FIGHT_RUN in the same frame - Fyrus charged Link forever and the
    // f_fight/fire gates were never even entered.
    //
    // The fork's guard is surgical: it wraps ONLY the ACTION_STOP assignment
    // inside the chain-yank branch (fork d_a_e_fm.cpp:1116-1125), leaving every
    // other transition alone. Match that - revert only when vanilla chose
    // ACTION_STOP.
    // ============================================
    if (dAlbwBoss_fyrusAblazePhase() && fm->mAction == kActionStop &&
        s_fightRun.action != kActionStop)
    {
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
    // Same BUGFIX as on_fight_run_post: the fork's ablaze guard here wraps ONLY
    // the chain-yank ACTION_STOP assignment (fork d_a_e_fm.cpp:1608-1618), not
    // every transition. Reverting broadly would pin Fyrus in DAMAGE_RUN after
    // any ablaze-phase hit, the same way the fight_run version pinned him in
    // FIGHT_RUN and stopped him ever attacking.
    if (fm->mAction == kActionStop && s_damageRun.action != kActionStop) {
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

// ============================================
// NEW CODE - ALBW Port (E_FM look-pass: body is shootable for chip damage)
//
// Fork cluster d_a_e_fm.cpp:356-475 + :649-741 + call sites :2685, :3828-3835,
// :3873-3876. Bodies below are the fork's, verbatim, except that TU-local enum
// values become the declaration-order mirrors above (the same way this file
// already mirrors ACTION_STOP / TEXANM_*).
// Fork comments are carried across unchanged - several encode hard-won facts.
//
// mFEffAtSph[8] is STOCK (d_a_e_fm.h offset 0x01990) and stock Create already
// builds all eight from f_eff_at_sph_src with SetStts(&field_0xa24), AtType
// 0x100 and MTRL_FIRE (stock d_a_e_fm.cpp:3826-3831). Stock simply never Sets
// them into Ccsp. No ABI work, no Create-side work.
// ============================================

// NOT PORTED - the fork's ACTION_ANM_PREVIEW half of this cluster
// (s_albwFmPreviewAnmId, e_fm_anm_preview, e_fm_albwLookPassAnmFromHit and the
// BCK mirrors they need). The fork labels it "TEMP: orphan BCK look-pass
// (revert after pick)" (d_a_e_fm.cpp:172, :357): it is a dev tool for
// auditioning stun animations, and its RESULT is already ported - the anim it
// was used to pick is baked into e_fm_albwPickGolemStunAnm
// (fyrus_phases.cpp:112-123). Shipping it would mean that in golem phase 2/3
// any body projectile throws Fyrus into an orphan BCK for 20 invulnerable
// frames and deals no damage. Excised deliberately; see fyrus_phases.cpp:33-36
// for the earlier decision this is consistent with.

// Body mCcSph stay At-only (latent burn). Look-pass Tg uses mFEffAtSph (donor
// prep spheres, never wired in vanilla) so burn At→player is not blocked by
// OnAtNoTgHitInfSet on the same collider.
const u32 kAlbwFmLookPassTg =
    AT_TYPE_ARROW | AT_TYPE_HOOKSHOT | AT_TYPE_BOOMERANG | AT_TYPE_SLINGSHOT;

const cXyz kAlbwFmLookPassParkPos(20000.0f, -23000.0f, 40000.0f);

// Tg SPrm must be 0x3 (Set + grp bit). OnTgSetBit alone leaves GetGrp()==0
// so projectile At never matches — first look-pass pass was dead on arrival.
void e_fm_albwLookPassOpenBodyTgSph(dCcD_Sph* i_sph) {
    i_sph->OffAtSetBit();
    i_sph->SetTgType(kAlbwFmLookPassTg);
    i_sph->SetTgSPrm(0x3);
    i_sph->OnTgNoHitMark();
}

void e_fm_albwLookPassParkBodyTgSph(dCcD_Sph* i_sph) {
    i_sph->SetC(kAlbwFmLookPassParkPos);
    i_sph->OffAtSetBit();
    i_sph->SetTgType(0);
    i_sph->SetTgSPrm(0);
    i_sph->OffTgSetBit();
}

bool e_fm_albwLookPassWantTg(e_fm_class* i_this) {
    // Fork-verbatim minus its `mAction != ACTION_ANM_PREVIEW` term, which cannot
    // be true here - that action is never entered (see the excision note above).
    return dAlbwBossRefinement_isEnabled() && i_this->mAction != kActionGolemHold &&
           i_this->mAction != kActionStart && i_this->mAction != kActionEnd;
}

// Mirror body At sphere layout onto Tg-only shadow spheres for look-pass.
void e_fm_albwLookPassSyncBodySph(e_fm_class* i_this) {
    const bool want = e_fm_albwLookPassWantTg(i_this);
    for (int i = 0; i < 8; i++) {
        if (want) {
            e_fm_albwLookPassOpenBodyTgSph(&i_this->mFEffAtSph[i]);
            i_this->mFEffAtSph[i].SetC(i_this->mCcSph[i].GetC());
            i_this->mFEffAtSph[i].SetR(i_this->mCcSph[i].GetR());
        } else {
            e_fm_albwLookPassParkBodyTgSph(&i_this->mFEffAtSph[i]);
        }
        dComIfG_Ccsp()->Set(&i_this->mFEffAtSph[i]);
    }
}

// Fork name kept (donor-derived symbols keep donor names) even though only the
// chip-damage half survives here - see the excision note above. Everything from
// the fork's `const int anm = e_fm_albwLookPassAnmFromHit(hit);` to its end was
// the ANM_PREVIEW dev tool and is replaced by `return false;`.
//
// Not gated on mDamageInvulnerabilityTimer — body Tg grp / fire At were the blockers.
// Core Tg is the real weak-spot path (cc_at_check). Body projectile sph overlap
// the chest, so a core hit this frame must not become CHANCE-with-no-bar.
bool e_fm_tryAlbwAnmPreview(e_fm_class* i_this) {
    if (!dAlbwBossRefinement_isEnabled()) {
        return false;
    }
    if (i_this->mAction == kActionGolemHold || i_this->mAction == kActionAblazeStun ||
        i_this->mAction == kActionStart || i_this->mAction == kActionEnd ||
        i_this->mAction == kActionDown || i_this->mAction == kActionADown)
    {
        return false;
    }

    if (i_this->mCoreSph.ChkTgHit()) {
        for (int i = 0; i < 8; i++) {
            i_this->mFEffAtSph[i].ClrTgHit();
        }
        return false;
    }

    cCcD_Obj* hit = NULL;
    for (int i = 0; i < 8; i++) {
        if (i_this->mFEffAtSph[i].ChkTgHit()) {
            hit = i_this->mFEffAtSph[i].GetTgHitObj();
            break;
        }
    }
    if (hit == NULL) {
        return false;
    }

    if (dAlbwBoss_fyrusShouldChipAblazeDamage()) {
        const int hpBefore = i_this->health;
        i_this->mAtInfo.mpCollider = hit;
        cc_at_check(i_this, &i_this->mAtInfo);
        dAlbwBoss_fyrusApplyChipDamage(i_this, hpBefore);
        for (int i = 0; i < 8; i++) {
            i_this->mFEffAtSph[i].ClrTgHit();
        }
        i_this->mCoreSph.ClrTgHit();
        return false;
    }

    // Chip not wanted this phase (ShouldChipAblazeDamage = ablaze && !vulnOpen,
    // so phase 1 only). The hit still has to be consumed: in the fork this fell
    // through to the ANM_PREVIEW tail, which cleared it. With that tail excised
    // the clear has to happen here, or a set Tg flag would persist into the next
    // frame and land a phantom chip the moment the phase opens.
    for (int i = 0; i < 8; i++) {
        i_this->mFEffAtSph[i].ClrTgHit();
    }
    return false;
}
// ============================================
// LOOK-PASS CLUSTER (bodies) ENDS HERE
// ============================================

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
    // Core-hit claims moved WHOLE to fyrus_phases.cpp's pre-hook on this same
    // seam: the fork's damage_check tail also enters the ablaze STUN and the
    // hollow DAMAGE_RUN, which the claim here lacked - two partial claimants
    // on one hit would shadow each other.

    // ============================================
    // NEW CODE - ALBW Port (look-pass consume, fork damage_check:2682-2688)
    //
    //     if (e_fm_tryAlbwAnmPreview(i_this)) { return; }
    //
    // sits at the TOP of the fork's damage_check, above the invuln gate and
    // above the refinement core-hit tail. This pre-hook is the earliest point
    // in the mod's frame: fyrus.cpp's hooks register before fyrus_phases.cpp's
    // (mod.cpp:215/217) and the hook service dispatches equal-priority pre
    // hooks in registration order (hook.cpp sort_hooks - stable, by order), so
    // this runs BEFORE fyrus_phases.cpp's core-hit tail, which is what the fork
    // ordering requires: the tail CLEARS mCoreSph's Tg hit, and
    // e_fm_tryAlbwAnmPreview must still be able to see it to yield to the core
    // (its "a core hit this frame must not become CHANCE-with-no-bar" branch).
    //
    // The fork's early `return` out of damage_check cannot be reproduced (the
    // remainder of damage_check is vanilla and runs anyway), but it does not
    // need to be: on every path where tryAlbwAnmPreview returns true it has
    // already cleared mCoreSph's and all eight body Tg hits, so vanilla's
    // damage_check and fyrus_phases.cpp's tail both find nothing to act on -
    // the same observable result as the fork's return.
    // ============================================
    // Called for its side effects only: it now either applies chip damage or
    // consumes the hit. The fork's ACTION_ANM_PREVIEW dispatch that used to
    // follow is excised (see the note on the cluster above), so nothing here
    // ever writes mAction.
    e_fm_tryAlbwAnmPreview(fm);
    return HOOK_CONTINUE;
}

// ============================================
// NEW CODE - ALBW Port (look-pass per-frame collider wiring)
// Fork call sites, both inside daE_FM_Execute:
//   :3828-3835  widen mCoreSph's Tg type while the look-pass wants Tg (and
//               restore stock's 0x2002 otherwise - stock core_sph_src is
//               {0x2002, 0x3}, stock d_a_e_fm.cpp:3681, so the else branch is
//               a byte-exact restore, not a change),
//   :3873-3876  e_fm_albwLookPassSyncBodySph.
// Both run from the Execute POST hook - see MANIFEST.md "call-site ordering".
// ============================================
void on_execute_lookpass_post(ModContext*, void* args, void*, void*) {
    auto* i_this = mods::arg<e_fm_class*>(args, 0);
    if (i_this == nullptr) {
        return;
    }
    // Toggle-off gate. The fork gets this from #if TARGET_PC plus the
    // refinement check inside e_fm_albwLookPassWantTg; in the receiver the
    // WantTg check alone is not enough to be provably stock, because the
    // park branch would still Set eight extra objects into the Ccsp list that
    // stock never registers. Gate the whole hook instead: with the toggle off
    // nothing here executes and the frame is byte-for-byte stock.
    if (!dAlbwBossRefinement_isEnabled()) {
        return;
    }

    // Core already has Tg SPrm 0x3; widen type so aimed shots at the eye count.
    if (e_fm_albwLookPassWantTg(i_this)) {
        i_this->mCoreSph.SetTgType(0x2002 | kAlbwFmLookPassTg);
    } else {
        i_this->mCoreSph.SetTgType(0x2002);
    }

    // Tg-only shadow spheres track body burn At layout (see kAlbwFmLookPassTg).
    e_fm_albwLookPassSyncBodySph(i_this);
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


// ============================================
// NEW CODE - ALBW Port (E_FM parryable ATs - fork e_fm_albwApplyParryableAt)
// Fork d_a_e_fm.cpp:616 stamps AtSpl 1 + SPrm bit 12 + VsPlayer on Fyrus's
// at/chain/effect spheres at their arming sites (effect_set:3547, Execute:3875
// and :3955); ChkAtNoGuard rejects spl >= 12, which is what makes the swings
// PARRYABLE. This was never carried - the reported "phase 2 attacks aren't
// parry-able". Re-stamped once per frame after Execute (any vanilla re-Set of
// a sphere this frame lands before this), gated exactly as the fork gates it.
// ============================================
static void e_fm_albwApplyParryableAt(dCcD_Sph* i_sph) {
    if (i_sph == NULL) {
        return;
    }
    i_sph->SetAtSpl((dCcG_At_Spl)1);
    i_sph->OnAtSPrmBit(12);
    i_sph->OnAtVsPlayerBit();
}

static void on_execute_parry_post(ModContext*, void* args, void*, void*) {
    auto* i_this = mods::arg<e_fm_class*>(args, 0);
    if (i_this == nullptr) {
        return;
    }
    // fork e_fm_albwWantParryableAt
    if (!dAlbwBossRefinement_isEnabled() || dAlbwBoss_fyrusGolemWindowIsLive()) {
        return;
    }
    e_fm_albwApplyParryableAt(&i_this->mAtSph);
    e_fm_albwApplyParryableAt(&i_this->mEffAtSph);
    for (int j = 0; j < (int)(sizeof(i_this->mChainAtSph) / sizeof(i_this->mChainAtSph[0])); j++) {
        e_fm_albwApplyParryableAt(&i_this->mChainAtSph[j]);
    }
}
ModResult albw_fyrus_init(ModError*) {
    try_install("daE_FM_Create", mods::hook::add_post<FmCreate>(on_create_post));
    try_install("daE_FM_Execute", mods::hook::add_pre<FmExecute>(on_execute_pre));
    try_install("daE_FM_Execute post", mods::hook::add_post<FmExecute>(on_execute_parry_post));
    try_install("daE_FM_Execute lookpass post",
                mods::hook::add_post<FmExecute>(on_execute_lookpass_post));
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
