// ============================================
// NEW CODE - ALBW Port (Darknut B_TN shield-bash parry / guard-open) -
// part of dev.albt.albw.
//
// Port of the fork's daB_TN_c guard-break system, fork
// src/d/actor/d_a_b_tn.cpp. Bodies are auto-extracted, BYTE-VERBATIM, by
// tools/port/port_tool.py (tools/port/btn.json -> src/btn_port.inc). Not one
// character of a fork body is retyped here; the only transform applied to a
// body is the `daB_TN_c::` -> `AlbwBtn_c::` rename on definition headers, plus
// the ONE declared substitution recorded in btn.json (m_attack_tn, below).
//
// THE MECHANISM (fork d_a_b_tn.cpp:1337-1475)
// -------------------------------------------
// A shield bash - or one of the three "guard opener" attacks - drops the
// Darknut's guard for a window measured in frames on the previously-unused
// layout byte field_0xaa2 (stock d_a_b_tn.h:191; stock declares it and never
// reads or writes it anywhere in the tree). Phase 1 (armoured) gets 90 frames
// and a GUARDH break, phase 2 (unarmoured) 75 frames and an ACT_YOROKE
// stagger, an opener hit 40 (kAlbwGuardOpenerWindowFrames). While the window
// is open the actor cannot attack, cannot re-raise field_0xa91, and holds the
// helm head-lock so Link's cut code can target it.
//
// THE SUBCLASS (why the bodies can stay verbatim)
// -----------------------------------------------
// A method body full of unqualified member access (`mSphC`, `setActionMode`,
// `field_0xaa2`) only compiles inside a daB_TN_c. We cannot redefine the stock
// exported methods, so - exactly as flurry_proc.cpp does for the rush proc -
// the ported bodies live on a layout-compatible subclass, AlbwBtn_c. Sibling
// calls inside the bodies resolve to the EXPORTED stock methods through
// inheritance; the members AlbwBtn_c redeclares (damage_check, setActionMode,
// setShieldEffect, the five executeXxx) deliberately HIDE the base versions, so
// a ported body calling setActionMode() gets the fork's H02 version - which is
// exactly what the fork's own body does. daB_TN_c has no virtual members of its
// own (stock d_a_b_tn.h), and AlbwBtn_c adds none, so sizeof and the vtable are
// untouched; every dispatch here is static. Calls out of the hooks are still
// written qualified (`self->AlbwBtn_c::damage_check()`) so a future virtual on
// the base could not silently turn a call into recursion back through its own
// hook.
//
// `#define private public` is the house idiom for reaching the actor's data
// block (enemy_lockout.cpp:68-70); the fork moves these fields behind `private`
// but does not move them, so this is an access question only, never layout.
//
// WHY damage_check IS REPLACED WHOLE, AND WHY IT IS GATED TO mType == 0
// ---------------------------------------------------------------------
// Five of its parry hunks are FALL-THROUGH GATE modifications: they prepend
// `field_0xaa2 == 0 &&` to a stock condition so that, while the window is open,
// control SKIPS a stock block and CONTINUES into later stock code. No pre-,
// post- or skip-hook can express "skip this interior branch but keep going" -
// only reproducing the body can.
//
// That collides with the file-static m_attack_tn (stock :1061, non-exported, so
// unaddressable from a plugin), which the stock body writes at three sites. The
// collision is ROUTED AROUND rather than crossed: for the Temple of Time boss
// those writes are provably dead.
//   1. m_attack_tn is READ in exactly one place - checkNormalAttackAble
//      :1072/:1077/:1082 - and that whole body sits inside `if (mType == 1)`
//      (:1066). For mType == 0 the function falls through to `return 1;`.
//   2. action() :4440 clears it unconditionally for mType == 0, every frame,
//      regardless of what damage_check wrote earlier in the same frame.
//   3. mType comes from fopAcM_GetParamBit(this, 8, 8) (:5034) and is forced to
//      0 at :5037; mType == 1 is the zako variant.
// So the hook refuses mType != 0 and hands those Darknuts straight back to
// stock. Cave-of-Ordeals zako Darknuts are therefore OUT OF SCOPE by
// construction, not by accident. Lifting the gate would make the writes live
// again and would need a mod-side m_attack_tn mirror plus a checkNormalAttackAble
// hook - NOT proposed, and it needs the user's go (see enemy_lockout.cpp:39-52).
//
// THE l_HIO WALL (found during implementation; not in the scoping doc)
// --------------------------------------------------------------------
// executeChaseH reads l_HIO.mTimer3NormalType0 / mTimer3NormalType1 (stock
// :2167-2169) - another non-exported file static. Rather than invent two
// constants, the donor's OWN system is ported: daB_TN_HIO_c and its constructor
// are reproduced verbatim (class declaration below - port_tool's documented
// KNOWN LIMITATION keys a class and its out-of-line constructor on the same
// name, so the declaration is the one thing it cannot pull - and the ctor plus
// the `static daB_TN_HIO_c l_HIO;` definition come out of btn_port.inc,
// verbatim). This is the flurry GAP-2 precedent (flurry_proc.cpp:94-102): the
// same file-static datum exists in both trees and simply never reached a
// header. l_HIO is mutated only by the HIO debug panel, which no shipping build
// instantiates, so our copy reads the constructor defaults - the same values
// the host's copy holds.
// ============================================

#include "btn_parry.h"

#include "global.h"
#include "albw_common.h"
#include "albw_dusk_log.h"
#include "btn_probe.h"
#include "devil_trigger.h"
#include "devil_trigger_probe.h"

// d_a_b_tn.h does not include headers for two of its own member types -
// request_of_phase_process_class (c_phase.h) and mDoExt_McaMorfSO
// (m_Do_ext.h). It relies on the includer already having them. Pulled in
// BEFORE the private/public redefinition so they parse normally and only
// d_a_b_tn.h sees the macro.
#include "SSystem/SComponent/c_phase.h"
#include "m_Do/m_Do_ext.h"

#define private public
#include "d/actor/d_a_b_tn.h"
#undef private

#include "d/actor/d_a_player.h"
#include "d/d_cc_d.h"
#include "d/d_cc_uty.h"  // def_se_set
#include "d/d_com_inf_game.h"
#include "d/d_save.h"
#include "f_op/f_op_actor.h"
#include "f_op/f_op_actor_enemy.h"
#include "f_op/f_op_actor_mng.h"
#include "f_pc/f_pc_name.h"
#include "m_Do/m_Do_mtx.h"
#include "SSystem/SComponent/c_cc_d.h"
#include "SSystem/SComponent/c_counter.h"
#include "SSystem/SComponent/c_lib.h"
#include "SSystem/SComponent/c_math.h"
#include "Z2AudioLib/Z2Instances.h"
#include "Z2AudioLib/Z2SeMgr.h"  // Z2SE_EN_TN_*

#include "albw_combat.h"    // dAlbwCombat_isGuardOpenerHit, kAlbwGuardOpenerWindowFrames
#include "hp_mult_port.h"   // dAlbwHP_scaleHpValue (the H-HP hunk, fork d_a_b_tn.cpp:5046)
#include "lockout_port.h"   // dAlbwLockout_* (the L hunks of the shared damage_check)
#include "shield.h"         // dShield_*
#include "mods/hook.hpp"

#include <cstring>

#if TARGET_PC

// ============================================
// Bridge dep. dMeter2_isALBWLocked() is the mod's own meter bridge, defined
// with external linkage at lockout.cpp:56, but it never reached a header (the
// lockout port includes its .inc into that same TU). Declared here rather than
// stubbed: this is the real function, not a stand-in.
// ============================================
bool dMeter2_isALBWLocked();

// ============================================
// PORTED VERBATIM - stock/fork src/d/actor/d_a_b_tn.cpp:18-34, the file-local
// HIO class. Reproduced because executeChaseH reads two of its fields through
// the file static l_HIO (see the header comment). The class DECLARATION is
// here; the CONSTRUCTOR and the `static daB_TN_HIO_c l_HIO;` definition are
// extracted into btn_port.inc, unmodified, so the defaults are the donor's own
// data and not numbers written out by hand.
//
// The stock STATIC_ASSERT(sizeof(daB_TN_HIO_c) == 0x30) is deliberately NOT
// reproduced: it asserts the GameCube layout of an object we construct
// ourselves and never hand to the host, so it would test our compiler, not the
// port. Nothing crosses the module boundary with this type.
// ============================================
class daB_TN_HIO_c {
public:
    daB_TN_HIO_c();
    virtual ~daB_TN_HIO_c() {};

    /* 0x04 */ s8 mUnk1;
    /* 0x08 */ f32 mScale;
    /* 0x0C */ f32 mKColorA;
    /* 0x10 */ f32 mTimer3Wolf;
    /* 0x14 */ f32 mTimer3HumanType0;
    /* 0x18 */ f32 mTimer3HumanType1;
    /* 0x1C */ f32 mTimer3NormalType0;
    /* 0x20 */ f32 mTimer3NormalType1;
    /* 0x24 */ f32 field_0x24;
    /* 0x28 */ f32 mTimer1Action1;
    /* 0x2C */ f32 mTimer1Action2;
};

// ============================================
// PORTED VERBATIM - stock/fork src/d/actor/d_a_b_tn.cpp:38-85 (byte-identical
// in both trees; the stock->fork diff has no hunk in this range, and that was
// checked, not assumed).
//
// B_TN_RES_FILE_ID is a FILE-SCOPE enum in the actor's .cpp - it never reached
// include/d/actor/d_a_b_tn.h - so the ported executeXxx bodies, which call
// setBck(BCK_TNA_GUARD, ...) and twelve siblings, cannot name it. Same shape as
// flurry_proc.cpp's GAP 1 (daAlink_CutNmParamType) and reproduced the same way:
// WHOLE, all members, original order. A partial enum is a different type, and
// these are resource indices - one dropped enumerator silently renumbers the
// rest and the Darknut plays the wrong animation.
//
// Only 13 of these are referenced by this port (BCK_TNA_ATACK_A/_B, _GUARD,
// _GUARD_DAMAGE, _WAIT_B_1/_2, _WALK, BCK_TNB_DOWN, _DOWN_SIPPU, _GUARD_A/_B,
// _JUMP_B_1/_2); the rest are carried so the numbering is the donor's.
// ============================================
enum B_TN_RES_FILE_ID {
    /* BCK */
    /* 0x04 */ BCK_TN2B_DIE = 4,
    /* 0x05 */ BCK_TNA_ATACK_A,
    /* 0x06 */ BCK_TNA_ATACK_B,
    /* 0x07 */ BCK_TNA_ATACK_OP,
    /* 0x08 */ BCK_TNA_ATACK_SHIELD,
    /* 0x09 */ BCK_TNA_DAMAGE_L,
    /* 0x0A */ BCK_TNA_DAMAGE_LAST,
    /* 0x0B */ BCK_TNA_DAMAGE_R,
    /* 0x0C */ BCK_TNA_GUARD,
    /* 0x0D */ BCK_TNA_GUARD_DAMAGE,
    /* 0x0E */ BCK_TNA_TURN_OP,
    /* 0x0F */ BCK_TNA_WAIT,
    /* 0x10 */ BCK_TNA_WAIT_B_1,
    /* 0x11 */ BCK_TNA_WAIT_B_2,
    /* 0x12 */ BCK_TNA_WAIT_OP,
    /* 0x13 */ BCK_TNA_WALK,
    /* 0x14 */ BCK_TNB_ATACK_A,
    /* 0x15 */ BCK_TNB_ATACK_B,
    /* 0x16 */ BCK_TNB_ATACK_SHIELD,
    /* 0x17 */ BCK_TNB_DAMAGE_L,
    /* 0x18 */ BCK_TNB_DAMAGE_R,
    /* 0x19 */ BCK_TNB_DIE,
    /* 0x1A */ BCK_TNB_DOWN,
    /* 0x1B */ BCK_TNB_DOWN_SIPPU,
    /* 0x1C */ BCK_TNB_GUARD_A,
    /* 0x1D */ BCK_TNB_GUARD_B,
    /* 0x1E */ BCK_TNB_JUMP_B_1,
    /* 0x1F */ BCK_TNB_JUMP_B_2,
    /* 0x20 */ BCK_TNB_JUMP_F_1,
    /* 0x21 */ BCK_TNB_JUMP_F_2,
    /* 0x22 */ BCK_TNB_JUMP_L_1,
    /* 0x23 */ BCK_TNB_JUMP_L_2,
    /* 0x24 */ BCK_TNB_JUMP_R_1,
    /* 0x25 */ BCK_TNB_JUMP_R_2,
    /* 0x26 */ BCK_TNB_PULL,
    /* 0x27 */ BCK_TNB_SWORD_B_PULL_A,
    /* 0x28 */ BCK_TNB_SWORD_B_PULL_B,
    /* 0x29 */ BCK_TNB_THROW,
    /* 0x2A */ BCK_TNB_WAIT,
    /* 0x2B */ BCK_TNB_WAIT_B_1,
    /* 0x2C */ BCK_TNB_WAIT_B_2,
    /* 0X2D */ BCK_TNB_WALK,

    /* BMDR */
    /* 0x30 */ BMDR_TN_EFFECT = 0x30,
};

// ============================================
// The port surface. Every member below is defined ONLY in src/btn_port.inc,
// verbatim from the fork, except the two marked ADAPTER / SEAM.
//
// Declarations are the fork's own, fork include/d/actor/d_a_b_tn.h:131-161 and
// the stock declarations of the nine modified members. The fork's two O-class
// HUD accessors (albwArmorBroken / albwArmorRemaining) and the four unused
// header inlines are NOT part of this port and are not declared.
// ============================================
class AlbwBtn_c : public daB_TN_c {
public:
    // ---- fork-ADDED members (fork hdr :143-160) ----
    // Reproduced verbatim from the fork header; the only one of the fork's
    // trivial inlines this port actually needs.
    bool albwIsUnarmoredPhase() const { return mNextBreakPart >= 11; }  // fork hdr :151

    bool albwApplyPhase1BashGuardBreak();                                // fork .cpp:1390
    bool albwApplyPhase2BashGuardBreak();                                // fork .cpp:1399
    int  albwTryApplyBashGuardBreakFromHit();                            // fork .cpp:1408
    bool albwHandleParryCombatBashShieldHit(dCcD_Sph* i_sph, cCcD_Obj* i_atObj);  // fork .cpp:1449
    void albwBeginGuardOpenWindow(u8 i_frames);                          // fork .cpp:1337
    void albwFinishBashGuardBreakFromHit(cCcD_Obj* i_atObj, int i_result);  // fork .cpp:1345
    void albwApplyPendingPhase1GuardBreak();                             // fork .cpp:1362
    bool albwIsPhase1AttackVulnerable() const;                           // fork .cpp:1373
    bool albwIsPhase1BashTargetState() const;                            // fork .cpp:1378

    // ADAPTER - the fork's albwDebugLogEvent (fork .cpp:1295) keeps its name and
    // signature so all 20 fork call sites stay byte-verbatim, but the BODY is
    // ours: the donor's is an unconditional fopen into %USERPROFILE%. See
    // btn_probe.h for the full argument.
    void albwDebugLogEvent(const char* event) const;

    // ---- fork-MODIFIED stock members, reproduced WHOLE ----
    void damage_check();                       // fork .cpp:1475  (H05c,H06-H12,H16 + L hunks)
    void setActionMode(int i_mode1, int i_mode2);  // fork .cpp:927   (H02)
    void setShieldEffect(dCcD_Sph* i_sph);     // fork .cpp:1250  (H05a)
    void executeChaseH();                      // (H17)
    void executeAttackH();                     // (H18,H19)
    void executeGuardH();                      // (H20,H21,H22)
    void executeChaseL();                      // (H25)
    void executeGuardL();                      // (H26,H27)
    void executeYoroke();                      // (H28,H29)

    // SEAM carrier - H33, the window countdown. Defined below, not in the .inc:
    // it is an ADDED BLOCK inside stock execute(), not a whole function, so
    // port_tool emits it as a diff fragment (tools/port/btn_seams.json ->
    // btn_seams.inc.ref) and the lines are transplanted here unchanged.
    void albwTickGuardOpenWindow();
};

#include "btn_port.inc"  // daB_TN_HIO_c::daB_TN_HIO_c, l_HIO, AlbwBtn_c::* - verbatim fork bodies

// ============================================
// ADAPTER BODY - albwDebugLogEvent.
//
// Defined AFTER the .inc so it can use albwDarknutAction1Name, the fork's own
// action-name table (fork .cpp:1256), which the .inc carries verbatim.
//
// Two deliberate differences from the donor body, both instrumentation-only:
//   * the sink is DuskLog, never a file (btn_probe.h);
//   * consecutive identical events are coalesced. The donor logs every call,
//     and two of its call sites are per-frame while their state holds
//     ("attack_a1_stop_blocked", "attack_a2_stop_blocked", fork .cpp:2963/3018),
//     which is the per-frame hazard this port is required to remove.
// Nothing here reads or writes actor state the donor does not already read.
// ============================================
void AlbwBtn_c::albwDebugLogEvent(const char* event) const {
#if ALBW_DARKNUT_PROBE
    static const char* sLastEvent = nullptr;
    static u32 sLastFrame = 0;
    if (event == nullptr) {
        return;
    }
    if (sLastEvent != nullptr && std::strcmp(sLastEvent, event) == 0 &&
        (g_Counter.mCounter0 - sLastFrame) < 30) {
        return;  // same event still repeating - do not spam the frame log
    }
    sLastEvent = event;
    sLastFrame = g_Counter.mCounter0;

    const f32 animFrame = mpModelMorf2 != NULL ? mpModelMorf2->getFrame() : -1.0f;
    const int animStop = mpModelMorf2 != NULL && mpModelMorf2->isStop() ? 1 : 0;

    // `headLock` used to name field_0xaa8 here, which is the "this bash already
    // counted" latch and NOT fopEn_flag_HeadLock. Reading the first run I nearly
    // called the head-lock risk closed on the wrong field. aa8 now says what it
    // is, and hlock is the real checkHeadLockFlg() the scope flagged: our port
    // calls onHeadLockFlg() and H02 removes setActionMode's unconditional
    // offHeadLockFlg() rescue, so a stranded lock is the actual release risk.
    DuskLog.info("[b_tn] f={} evt={} act={}/{} a2={} shield={} aa8={} hlock={} window={} flags={} "
                 "break={} anim={} stop={} bash={}/{} pending={} inFlight={} mType={}",
                 g_Counter.mCounter0, event, albwDarknutAction1Name(mActionMode1), mActionMode1,
                 mActionMode2, field_0xa91 ? 1 : 0, field_0xaa8 ? 1 : 0,
                 checkHeadLockFlg() ? 1 : 0, (int)field_0xaa2,
                 (int)field_0xa9d, mNextBreakPart, animFrame, animStop, dShield_getBashCharges(),
                 dShield_getMaxBashCharges(), dShield_hasFullBarPunishPending() ? 1 : 0,
                 dShield_hasFullBarBashInFlight() ? 1 : 0, (int)mType);
#else
    (void)event;
#endif
    // albwDarknutAction1Name is the donor's table and is carried verbatim even
    // when the probe is off; keep it referenced so an unused-static diagnostic
    // cannot turn a probe-off release build red.
    (void)&albwDarknutAction1Name;
}

// ============================================
// SEAM BODY - H33, the guard-open window countdown.
//
// The four statements below are the fork's added block, fork
// src/d/actor/d_a_b_tn.cpp:5481-5496, transplanted unchanged (see
// btn_seams.inc.ref, the port_tool extract they were taken from).
//
// NOT gated on the parry toggle - and that is the donor's own choice, not an
// oversight. The fork does not wrap this block in dShield_isParryCombatEnabled()
// either. It does not need to be: field_0xaa2 is written ONLY by ported code,
// so with the feature off it is 0, `if (field_0xaa2 != 0)` is false and the
// whole block is a no-op. Gating it would be strictly WORSE - it would strand a
// live window (and with it the head-lock, see the H02 note) the moment the user
// flipped the toggle off mid-fight. As written, a mid-fight toggle-off drains
// the window over at most 90 frames and fires the donor's own expiry cleanup.
// ============================================
void AlbwBtn_c::albwTickGuardOpenWindow() {
    if (field_0xaa2 != 0) {
        if (field_0xaa2 == 1) {
#if TARGET_PC
            albwDebugLogEvent("punish_expired");
            dShield_clearFullBarPunishPending();
            field_0xaa8 = false;
            offHeadLockFlg();
#endif
        }
        field_0xaa2--;
    }

#if TARGET_PC
    if (field_0xaa2 != 0 && field_0xaa8) {
        onHeadLockFlg();
    }
#endif
}

namespace {

// ============================================
// ALL-OR-NOTHING INSTALL.
//
// Same doctrine as flurry_proc.cpp:501-510, and here it is not a stylistic
// choice - it is the mitigation for this port's top risk.
//
// H02 removes stock setActionMode's UNCONDITIONAL offHeadLockFlg() while the
// window is open, so the release of the helm head-lock moves into H33's expiry.
// If BtnSetActionMode installed and BtnAction did not, the rescue would be gone
// AND nothing would ever expire the window: the lock would stick for the rest
// of the session and Link's cut code (d_a_alink_cut.inc:796 reads
// checkHeadLockFlg) would stay pinned to the Darknut's head. Likewise
// BtnDamageCheck without BtnAction would OPEN a window nothing can close.
//
// So a partial install is downgraded to NO install: every callback tests this
// flag first and the feature is provably stock for the run.
// ============================================
bool s_featureReady = false;

DEFINE_HOOK(&daB_TN_c::damage_check, BtnDamageCheck);
DEFINE_HOOK(&daB_TN_c::action, BtnAction);
DEFINE_HOOK(&daB_TN_c::checkAttackAble, BtnCheckAttackAble);
DEFINE_HOOK(&daB_TN_c::setActionMode, BtnSetActionMode);
DEFINE_HOOK(&daB_TN_c::setShieldEffect, BtnSetShieldEffect);
DEFINE_HOOK(&daB_TN_c::executeChaseH, BtnExecuteChaseH);
DEFINE_HOOK(&daB_TN_c::executeAttackH, BtnExecuteAttackH);
DEFINE_HOOK(&daB_TN_c::executeGuardH, BtnExecuteGuardH);
DEFINE_HOOK(&daB_TN_c::executeChaseL, BtnExecuteChaseL);
DEFINE_HOOK(&daB_TN_c::executeGuardL, BtnExecuteGuardL);
DEFINE_HOOK(&daB_TN_c::executeYoroke, BtnExecuteYoroke);
DEFINE_HOOK(&daB_TN_c::execute, BtnExecute);  // Devil Trigger sub-step


AlbwBtn_c* self_of(void* args) {
    return static_cast<AlbwBtn_c*>(mods::arg<daB_TN_c*>(args, 0));
}

// ============================================
// damage_check - H32-P (pre-arm) + the WHOLE replacement.
//
// PLACEMENT, part 1 (H32-P, albwApplyPendingPhase1GuardBreak). The fork adds
// that one statement inside action(), and in the fork it sits IMMEDIATELY
// before the call to damage_check() with ZERO statements in between (fork
// :5040-5041). A PRE hook on damage_check is therefore not merely "equivalent
// to" the donor's position - it IS the donor's position. No argument from
// similarity is needed, and action()'s own three file statics (m_attack_timer,
// m_attack_tn, l_HIO) never come into it because action() is not touched.
//
// It runs BEFORE the toggle test, matching the fork, which does not gate it
// either. Provably inert with the feature off: field_0xa9d bit 0x80 is never
// set by stock (verified - stock uses only 0x01/0x02/0x04 of that byte) and is
// set only by ported code, so the function returns on its first line. Gating it
// would strand the bit if the toggle flipped off mid-swing, which is precisely
// the stranding risk the scoping doc's risk register flags; running it
// unconditionally is both donor-faithful and the safer of the two.
//
// It is also NOT mType-gated, again matching the fork. For a zako the bit can
// never be set (nothing but the mType==0 replacement below sets it), so the
// call is a no-op there.
//
// PLACEMENT, part 2 (the body). PRE + HOOK_SKIP_ORIGINAL on a whole function is
// the same position as the function itself: the stock body does not run at all,
// ours does. This is the pattern already shipping at enemy_lockout.cpp:110-178
// for daE_WW_c::damage_check.
// ============================================
HookAction on_btn_damage_check_pre(ModContext*, void* args, void*, void*) {
    if (!s_featureReady) {
        return HOOK_CONTINUE;
    }
    AlbwBtn_c* self = self_of(args);
    if (self == nullptr) {
        return HOOK_CONTINUE;
    }

    self->AlbwBtn_c::albwApplyPendingPhase1GuardBreak();  // H32-P, donor position

    if (!dShield_isParryCombatEnabled()) {
        return HOOK_CONTINUE;  // toggle off == stock, m_attack_tn writes included
    }
    if (self->mType != 0) {
        return HOOK_CONTINUE;  // zako Darknuts stay stock - see the header comment
    }

    self->AlbwBtn_c::damage_check();
    return HOOK_SKIP_ORIGINAL;
}

// ============================================
// action - H33, the window countdown.
//
// PLACEMENT - the trap in this port, and the reason this is NOT a post hook on
// execute(). The fork puts the countdown inside execute(), between mTimer12--
// (stock :4879) and mTimer13-- (stock :4883). execute() then calls action() at
// stock :4887, AFTER every timer decrement. A POST hook on execute() would
// therefore run after action() -> damage_check() -> every executeXxx() had
// already read field_0xaa2 for that frame: the window would be a frame late and
// the expiry cleanup (dShield_clearFullBarPunishPending / field_0xaa8 = false /
// offHeadLockFlg) would land after a frame that still saw the stale window.
//
// A PRE hook on action() is the correct position, and the argument is exact
// rather than "close enough":
//   * VERIFIED: stock execute() (:4831) opens with the timer block and has NO
//     early return, no branch and no call of any kind before action(). So the
//     hook fires on exactly the frames the donor's insertion point does.
//   * The only stock statement between the donor's insertion point and
//     action()'s entry is `if (mTimer13 != 0) mTimer13--;` (:4883-4885). The
//     moved block reads and writes only field_0xaa2, field_0xaa8, the head-lock
//     flag and mod-side shield state - it shares no state whatsoever with
//     mTimer13, and nothing between the two points reads field_0xaa2. The
//     reorder is unobservable.
//   * The property the donor's placement exists to guarantee - the countdown
//     runs before action() and everything downstream of it - is preserved, as
//     is the donor's relative order across the two hooks:
//     action PRE (countdown) -> damage_check PRE (apply pending) -> body.
//
// HOOK_CONTINUE: stock action() still runs. We are adding a statement, not
// replacing the function (action() is not replaceable - m_attack_timer,
// m_attack_tn and l_HIO are all file-static, and the l_HIO read there belongs
// to the HP lane's hunk anyway).
// ============================================
HookAction on_btn_action_pre(ModContext*, void* args, void*, void*) {
    if (!s_featureReady) {
        return HOOK_CONTINUE;
    }
    AlbwBtn_c* self = self_of(args);
    if (self != nullptr) {
        self->AlbwBtn_c::albwTickGuardOpenWindow();
        // ============================================
        // Generalized DT parry-opening (DT §7): when a parry set the DT window on
        // this enraged Darknut, make it REACT AS IF BASHED. This is the native
        // reaction from albwTryApplyBashGuardBreakFromHit — head-lock + a
        // phase-appropriate stagger (ACT_YOROKE unarmored / ACT_GUARDH armored) +
        // the guard-open window — but WITHOUT its bash-credit gate
        // (dShield_tryGrantHelmPunishCredit needs a recent bash SPEND, which a
        // parry has not made). A parry earns the opening directly, so the gate is
        // the one omission. Fires ~once per parry (gated on field_0xaa2 == 0, so it
        // re-arms only after the previous open decays). setActionMode via the
        // AlbwBtn_c:: qualified call — the same path the bash reaction uses.
        // ============================================
        // Fire the bashed-reaction exactly ONCE per parry — consumeOpenReaction
        // is true only on the frame a parry set the window (fixes the prior
        // field_0xaa2==0 gate re-firing it 2-3 times as the window decayed).
        if (dAlbwDevil_consumeOpenReaction(self) &&
            self->mActionMode1 != daB_TN_c::ACT_CHANGEDEMO &&
            self->mActionMode1 != daB_TN_c::ACT_ENDING)
        {
            self->setSwordAtBit(0);
            self->onHeadLockFlg();
            if (self->albwIsUnarmoredPhase()) {
                self->AlbwBtn_c::albwBeginGuardOpenWindow(75);
                self->AlbwBtn_c::setActionMode(daB_TN_c::ACT_YOROKE, daB_TN_c::ACTION2_0_e);
            } else {
                self->AlbwBtn_c::albwBeginGuardOpenWindow(90);
                self->AlbwBtn_c::setActionMode(daB_TN_c::ACT_GUARDH, daB_TN_c::ACTION2_0_e);
            }
#if ALBW_DEVIL_PROBE
            self->albwDebugLogEvent("dt_parry_react");
#endif
        }
    }
    return HOOK_CONTINUE;
}

// ============================================
// action - H-HP, the true-max-HP hunk. Port of fork d_a_b_tn.cpp:5045-5047.
//
// WHY THIS EXISTS. dAlbwHP_tryApplyTrueMaxHp scales health and field_0x560,
// which for this actor are not the durability pool. daB_TN_c::setDamage opens
// `health = 100;` and then calls cc_at_check (stock :1147-1148), so health is a
// SCRATCH register holding one hit's damage - the same reading btnHealthFraction
// below already documents. The pool is
//     field_0x6fc += 100 - health;                  // accumulated  (stock :1213)
//     if (field_0x6fc >= field_0x700) -> ACT_ENDING // death        (stock :1216)
// and field_0x700 is re-seeded from l_HIO.field_0x24 (360.0f, stock :272) on
// EVERY frame at stock :4449. So the once-per-actor init scaler cannot reach it,
// and "Mid-boss HP xN" left a Darknut exactly as durable as vanilla.
//
// PLACEMENT - POST, and the argument is exact, not "close enough". The fork puts
// its line immediately after the stock `field_0x700 = (int)l_HIO.field_0x24;`
// inside action(), which is unreachable for us: action() is NOT replaceable
// (m_attack_timer, m_attack_tn and l_HIO are all file-static - see the H33
// comment above, which already flags this hunk as owed to the HP lane).
//   * VERIFIED: field_0x700 is WRITTEN in exactly one place in the whole stock
//     file (action() :4449) and READ in exactly three (setDamage :1205, :1210,
//     :1216), all reachable only from damage_check(), one statement earlier.
//   * VERIFIED: nothing in the mActionMode1 switch, in any executeXxx, or in
//     execute() reads or writes field_0x700.
//   * The donor's placement guarantees one property: field_0x700 is scaled from
//     the stock re-seed onward, and therefore scaled when the NEXT frame's
//     damage_check() -> setDamage reads it. A POST hook on action() reproduces
//     that cadence exactly, and leaves the value scaled for the HUD accessor too.
//   * A PRE hook would be WRONG: the stock re-seed then wipes the scale for the
//     rest of the frame, so btnHealthFraction would read the unscaled 360.
//   * A POST on damage_check would be WRONG: it lands BEFORE the stock re-seed,
//     which overwrites it on the very next statement.
//
// NOT gated on s_featureReady - that latch is the parry set's all-or-nothing
// install guard, and an unrelated parry hook failing must not switch HP scaling
// off. NOT mType-gated: the fork's line runs for every daB_TN_c, zako included.
//
// Toggle off == provably stock: scaleHpField returns its input when mult <= 1
// (fork d_albw_hp_mult.cpp:105-111) and dAlbwRegionMult_scaleHp returns its input
// when mult <= 1.0f (fork d_albw_region_mult.cpp:242-249), so with both sliders
// at 1 this writes back the identical 360.
// ============================================
void on_btn_action_post(ModContext*, void* args, void*, void*) {
    daB_TN_c* self = mods::arg<daB_TN_c*>(args, 0);
    if (self == nullptr) {
        return;
    }
    // fork d_a_b_tn.cpp:5046, verbatim
    self->field_0x700 = dAlbwHP_scaleHpValue(fpcNm_B_TN_e, (s16)self->field_0x700);
}

// ============================================
// checkAttackAble - H23. The Darknut cannot attack while the window is open.
//
// PLACEMENT: the fork's change is the LITERAL FIRST STATEMENT of the function
// (fork :3714, ahead of the distance/angle test at stock :3196). A pre-hook is
// the same position by construction, and skipping with `false` reproduces the
// donor's `return false` exactly.
// ============================================
HookAction on_btn_check_attack_able_pre(ModContext*, void* args, void* retval, void*) {
    if (!s_featureReady || !dShield_isParryCombatEnabled()) {
        return HOOK_CONTINUE;
    }
    AlbwBtn_c* self = self_of(args);
    if (self == nullptr || retval == nullptr) {
        return HOOK_CONTINUE;
    }
    if (self->field_0xaa2 != 0) {
        *static_cast<bool*>(retval) = false;
        return HOOK_SKIP_ORIGINAL;
    }
    return HOOK_CONTINUE;
}

// ============================================
// setActionMode - H02, head-lock preservation. WHOLE replacement.
//
// PLACEMENT: not hookable as a pair. The stock body is three statements
// (:913-917) and the fork's change INTERLEAVES with them - it must decide
// keepHeadLock BEFORE offHeadLockFlg() runs, then re-assert AFTER the two
// assignments. A PRE hook cannot suppress the stock offHeadLockFlg(); a POST
// hook cannot know what the flag was beforehand. Replacing the five-line
// function is the only faithful expression. It is exported and has no
// file-scope dependencies.
//
// ⚠ LOAD-BEARING. This removes stock's unconditional offHeadLockFlg() rescue
// while the window is open, which is what makes H33's expiry the sole release
// path. See s_featureReady above for why the two cannot ship apart, and the
// MANIFEST for the four release paths that were walked.
// ============================================
HookAction on_btn_set_action_mode_pre(ModContext*, void* args, void*, void*) {
    if (!s_featureReady || !dShield_isParryCombatEnabled()) {
        return HOOK_CONTINUE;
    }
    AlbwBtn_c* self = self_of(args);
    if (self == nullptr) {
        return HOOK_CONTINUE;
    }

    // ============================================
    // DEVIL TRIGGER - knockback immunity.
    //
    // Refuse the STAGGER state transitions only. It is deliberately not a
    // damage change: field_0x6fc still accumulates in setDamage, and ACT_ENDING
    // is set from there too (stock d_a_b_tn.cpp:1213-1217), so an enraged
    // Darknut still takes damage and still dies on schedule - it just does not
    // flinch. Gating the damage path instead would make low-HP enemies
    // unkillable, which is the failure this feature is most able to cause and
    // the one that would look like it working until nothing died.
    // ============================================
    const int mode = mods::arg<int>(args, 1);
    if (dAlbwDevil_isArmed(self) && !dAlbwDevil_isGuardOpen(self) &&
        (mode == daB_TN_c::ACT_YOROKE || mode == daB_TN_c::ACT_DAMAGEH ||
         mode == daB_TN_c::ACT_DAMAGEL))
    {
#if ALBW_DEVIL_PROBE
        DuskLog.info("[devil] no-flinch: refused mode={} (armed)", mode);
#endif
        return HOOK_SKIP_ORIGINAL;
    }

    self->AlbwBtn_c::setActionMode(mode, mods::arg<int>(args, 2));
    return HOOK_SKIP_ORIGINAL;
}

// ============================================
// setShieldEffect - H05a. WHOLE replacement (two statements).
//
// PLACEMENT: the fork changes ARGUMENT 2 of the first of the body's two calls
// (`mAtInfo.mpCollider` -> `i_sph->GetTgHitObj()`, so the SE is sourced from the
// sphere actually hit rather than a stale mAtInfo). No pre- or post-position can
// express an argument change; replace.
//
// ⚠ This is a behaviour change inside a stock function that MANY non-parry
// paths call (stock :1297, :1352, :1367, :1374, :1395, :1411, :1479, :1488...).
// It is kept strictly inside the toggle for that reason.
// ============================================
HookAction on_btn_set_shield_effect_pre(ModContext*, void* args, void*, void*) {
    if (!s_featureReady || !dShield_isParryCombatEnabled()) {
        return HOOK_CONTINUE;
    }
    AlbwBtn_c* self = self_of(args);
    if (self == nullptr) {
        return HOOK_CONTINUE;
    }
    self->AlbwBtn_c::setShieldEffect(mods::arg<dCcD_Sph*>(args, 1));
    return HOOK_SKIP_ORIGINAL;
}

// ============================================
// The five executeXxx replacements - H17, H18/H19, H20/H21/H22, H25, H26/H27,
// H28/H29.
//
// PLACEMENT, all six: every fork change is MID-FUNCTION - inside a `case` body
// of the mActionMode2 switch, or wrapping an existing `if` body in a new
// condition. None is a prologue, an epilogue or an argument, so none has a
// pre/post equivalent; each is a whole-body replacement with SKIP_ORIGINAL.
// Each was checked for file-scope dependencies rather than assumed from
// hookability: executeChaseH is the only one with any (l_HIO, handled above),
// and none of the six touches m_attack_tn or m_attack_timer.
//
// NOT mType-gated - the fork applies them to every Darknut and they have no
// m_attack_tn dependency, so the gate that damage_check needs would be a
// divergence here, not a safety measure.
// ============================================
#define ALBW_BTN_REPLACE(fn, cb)                                          \
    HookAction cb(ModContext*, void* args, void*, void*) {                \
        if (!s_featureReady || !dShield_isParryCombatEnabled()) {         \
            return HOOK_CONTINUE;                                         \
        }                                                                 \
        AlbwBtn_c* self = self_of(args);                                  \
        if (self == nullptr) {                                            \
            return HOOK_CONTINUE;                                         \
        }                                                                 \
        self->AlbwBtn_c::fn();                                            \
        return HOOK_SKIP_ORIGINAL;                                        \
    }

ALBW_BTN_REPLACE(executeChaseH, on_btn_execute_chase_h_pre)
ALBW_BTN_REPLACE(executeAttackH, on_btn_execute_attack_h_pre)
ALBW_BTN_REPLACE(executeGuardH, on_btn_execute_guard_h_pre)
ALBW_BTN_REPLACE(executeChaseL, on_btn_execute_chase_l_pre)
ALBW_BTN_REPLACE(executeGuardL, on_btn_execute_guard_l_pre)
ALBW_BTN_REPLACE(executeYoroke, on_btn_execute_yoroke_pre)

#undef ALBW_BTN_REPLACE

// ============================================
// A hook that fails to resolve must NOT abort mod_initialize. Same doctrine as
// enemy_lockout.cpp:180-204 - the miss is LOUD and SCOPED, never silent.
// ============================================
bool report(const char* name, ModResult r) {
    if (r != MOD_OK) {
        if (svc_log != nullptr) {
            svc_log->error(mod_ctx, name);
            svc_log->error(mod_ctx, "hook above did NOT install - the Darknut parry/guard-open "
                                    "port is inactive this run");
        }
        return false;
    }
    return true;
}

// ============================================
// DEVIL TRIGGER - Darknut wiring.
//
// WHAT "25% HEALTH" MEANS FOR THIS ENEMY, which is not obvious and not the
// generic reading. daB_TN_c::setDamage opens with `health = 100;` and then
// calls cc_at_check (stock d_a_b_tn.cpp:1147), so `health` is a SCRATCH
// register holding one hit's damage - 100 minus whatever the hit took. It is
// never a pool and never trends downward, so the generic
// health / field_0x560 reading is meaningless here (and, separately, that is
// why the enemy HP bar cannot be right for Darknuts either - noted, not fixed
// here).
//
// The real pool is two ints further down the same function:
//     field_0x6fc += 100 - health;                 // accumulated damage
//     if (field_0x6fc >= field_0x700) -> ACT_ENDING  // death (stock :1213-1217)
// with field_0x700 seeded from the HIO at stock :4449. So
//     remaining = 1 - field_0x6fc / field_0x700
// and that is a single pool spanning BOTH phases - armour breaking and
// post-armour damage accumulate into the same counter - so the threshold
// means the same thing either side of ACT_CHANGEDEMO.
// ============================================
bool btnHealthFraction(fopAc_ac_c* actor, float* outFraction) {
    auto* tn = static_cast<daB_TN_c*>(actor);
    if (tn == nullptr || tn->field_0x700 <= 0) {
        return false;  // no threshold seeded yet - fall back, do not guess
    }
    const float taken = static_cast<f32>(tn->field_0x6fc);
    const float pool  = static_cast<f32>(tn->field_0x700);
    float frac = 1.0f - (taken / pool);
    if (frac < 0.0f) {
        frac = 0.0f;
    }
    *outFraction = frac;
    return true;
}

// ============================================
// The speed-up. State-gated sub-step, per docs/DEVIL-TRIGGER-SCOPE.md 3b:
// run the actor a second time ONLY while no attack collider is live, so
// movement, locomotion animation, timers and the state machine all advance
// together while every swing keeps stock timing and a stock hitbox.
//
// POST, not PRE: the extra step must come after the real one, and calling the
// original through g_orig would re-enter our own hook.
//
// s_inSubStep is not decoration. execute() reaches damage_check, which this
// module also hooks; without the latch a sub-step would recurse.
// ============================================
bool s_inSubStep = false;

void on_btn_execute_post(ModContext*, void* args, void*, void*) {
    if (!s_featureReady || s_inSubStep) {
        return;
    }
    auto* self = self_of(args);
    if (self == nullptr || self->mType != 0) {
        return;  // boss Darknut only, matching the parry port's own gate
    }

    dAlbwDevil_tickActor(self);

#if ALBW_DEVIL_PROBE
    // ============================================
    // HEARTBEAT. The first bring-up run produced ZERO devil lines and I could
    // not tell why, because the only trace was on ARM: "the hook never fired",
    // "it fired but never crossed 25%" and "it crossed but the policy refused"
    // all look identical from silence. That is the same blind spot that cost a
    // run earlier today, so the probe now reports the state it is deciding on.
    //
    // Throttled on CHANGE plus a slow floor, so a long fight does not flood
    // the log but a still one still proves the hook is alive.
    // ============================================
    {
        static int sLastPct = -1;
        static u32 sLastFrame = 0;
        const float frac = dAlbwDevil_healthFraction(self);
        const int pct = frac >= 0.0f ? static_cast<int>(frac * 100.0f) : -1;
        if (pct != sLastPct || (g_Counter.mCounter0 - sLastFrame) > 300) {
            sLastPct = pct;
            sLastFrame = g_Counter.mCounter0;
            DuskLog.info("[devil] hb f={} pct={} taken={}/{} armed={} atLive={} act={}",
                         g_Counter.mCounter0, pct, self->field_0x6fc, self->field_0x700,
                         dAlbwDevil_isArmed(self) ? 1 : 0,
                         dAlbwDevil_isAttackLive(self) ? 1 : 0, self->mActionMode1);
        }
    }
#endif

    // Computed here rather than after the armed gate: it is a MEASUREMENT as
    // well as a decision, and gating it on armed meant the first run produced
    // no signal data at all.
    const bool attacking = dAlbwDevil_isAttackLive(self);

#if ALBW_DEVIL_PROBE
    // The measurement the whole design rests on: does the generic
    // AT-registry answer agree with the Darknut action mode we already know?
    // ACT_ATTACKH/ATTACKSHIELDH/ATTACKL/ATTACKSHIELDL are the ground truth.
    const int m = self->mActionMode1;
    const bool truth = (m == daB_TN_c::ACT_ATTACKH || m == daB_TN_c::ACT_ATTACKSHIELDH ||
                        m == daB_TN_c::ACT_ATTACKL || m == daB_TN_c::ACT_ATTACKSHIELDL);
    if (truth != attacking) {
        DuskLog.warn("[devil] signal MISMATCH act={} atRegistry={} truth={}", m,
                     attacking ? 1 : 0, truth ? 1 : 0);
    }
#endif

    if (!dAlbwDevil_isArmed(self)) {
        return;
    }

    // Death sequence: stop enraging. Re-running execute() through ACT_ENDING
    // let the Darknut survive a killing blow ("persist for one more hit") -
    // the extra frame re-touches the death path at the threshold boundary.
    // Forget the actor so nothing here fires again, and let the real death
    // play out at stock speed.
    if (self->mActionMode1 == daB_TN_c::ACT_ENDING) {
        dAlbwDevil_forget(self);
        return;
    }

    if (attacking) {
        return;  // a swing is live - stock timing, stock hitbox, no sub-step
    }

    s_inSubStep = true;
    self->AlbwBtn_c::execute();
    s_inSubStep = false;
}

}  // namespace

ModResult albw_btn_parry_init(ModError*) {
    bool ok = true;
    ok &= report("BtnDamageCheck",
                 mods::hook_add_pre<BtnDamageCheck>(svc_hook, on_btn_damage_check_pre));
    ok &= report("BtnAction", mods::hook_add_pre<BtnAction>(svc_hook, on_btn_action_pre));

    // H-HP - the Darknut true-max-HP hunk (fork d_a_b_tn.cpp:5046). Deliberately
    // NOT folded into `ok`: this is the HP lane, not the parry lane. If the parry
    // set half-installs, HP scaling must still work; if this one misses, the parry
    // port must still ship. Reported loudly either way - never a silent fallback.
    if (!report("BtnActionTrueHp",
                mods::hook_add_post<BtnAction>(svc_hook, on_btn_action_post))) {
        if (svc_log != nullptr) {
            svc_log->error(mod_ctx, "Darknut true-max-HP scaling is INACTIVE this run - "
                                    "Mid-boss HP x will have no effect on Darknuts");
        }
    }

    ok &= report("BtnCheckAttackAble",
                 mods::hook_add_pre<BtnCheckAttackAble>(svc_hook, on_btn_check_attack_able_pre));
    ok &= report("BtnSetActionMode",
                 mods::hook_add_pre<BtnSetActionMode>(svc_hook, on_btn_set_action_mode_pre));
    ok &= report("BtnSetShieldEffect",
                 mods::hook_add_pre<BtnSetShieldEffect>(svc_hook, on_btn_set_shield_effect_pre));
    ok &= report("BtnExecuteChaseH",
                 mods::hook_add_pre<BtnExecuteChaseH>(svc_hook, on_btn_execute_chase_h_pre));
    ok &= report("BtnExecuteAttackH",
                 mods::hook_add_pre<BtnExecuteAttackH>(svc_hook, on_btn_execute_attack_h_pre));
    ok &= report("BtnExecuteGuardH",
                 mods::hook_add_pre<BtnExecuteGuardH>(svc_hook, on_btn_execute_guard_h_pre));
    ok &= report("BtnExecuteChaseL",
                 mods::hook_add_pre<BtnExecuteChaseL>(svc_hook, on_btn_execute_chase_l_pre));
    ok &= report("BtnExecuteGuardL",
                 mods::hook_add_pre<BtnExecuteGuardL>(svc_hook, on_btn_execute_guard_l_pre));
    ok &= report("BtnExecuteYoroke",
                 mods::hook_add_pre<BtnExecuteYoroke>(svc_hook, on_btn_execute_yoroke_pre));


    ok &= report("BtnExecuteDevil",
                 mods::hook_add_post<BtnExecute>(svc_hook, on_btn_execute_post));

    // The Darknut reads its health differently from every other enemy - see
    // btnHealthFraction. Registered even when Devil Trigger is toggled off:
    // the override is a READING, not a behaviour, and the policy module gates
    // on the toggle itself.
    dAlbwDevil_registerHealthOverride(fpcNm_B_TN_e, btnHealthFraction);

    s_featureReady = ok;
    if (!s_featureReady) {
        if (svc_log != nullptr) {
            svc_log->error(mod_ctx, "Darknut parry/guard-open DISABLED: the window timer "
                                    "(daB_TN_c::action) and the head-lock preservation "
                                    "(daB_TN_c::setActionMode) must install together with the "
                                    "rest or none of them takes effect - a half-installed set "
                                    "can leave the helm head-lock permanently on");
        }
    } else {
        svc_log->info(mod_ctx, "albw darknut parry/guard-open ready");
    }
    return MOD_OK;
}

#endif  // TARGET_PC
