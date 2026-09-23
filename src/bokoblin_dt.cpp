// ============================================
// NEW CODE — ALBW Port — Devil Trigger: Bokoblin (E_OC) compatibility profile.
//
// The SECOND DT profile after the Darknut (see the profile abstraction in
// devil_trigger + DEVIL-TRIGGER-METHODS). E_OC uses the standard `health` pool
// (no scratch-HP override — the Darknut needed one, this doesn't) and defaults to
// the NORMAL category, so it is DT-eligible out of the box.
//
// Front door: a POST hook on daE_OC_c::execute that latches and calls the generic
// dAlbwDevil_execProfile. The profile supplies only E_OC-specific pieces (death
// predicate + the redispatch). A bring-up AUDIT probe compares state across the
// second execute so any double-execute anomaly is caught by data, not by eye —
// that measurement is the whole point of onboarding this enemy.
// ============================================

#include "global.h"

#include "mods/hook.hpp"
// d_a_e_oc.h relies on the includer to predefine its member types (mDoExt_McaMorfSO,
// Z2CreatureEnemy, request_of_phase_process_class, fopEn_enemy_c base) — same
// prelude e_s1_hooks.cpp uses before its enemy-actor header.
#include "SSystem/SComponent/c_phase.h"
#include "SSystem/SComponent/c_counter.h"
#include "Z2AudioLib/Z2Instances.h"
#include "f_op/f_op_actor_enemy.h"
#include "f_op/f_op_actor_mng.h"
#include "m_Do/m_Do_ext.h"
#include "d/d_cc_d.h"
#include "d/d_cc_uty.h"

#define private public
#include "d/actor/d_a_e_oc.h"
#undef private

#include "f_pc/f_pc_name.h"
#include "albw_common.h"
#include "albw_dusk_log.h"
#include "devil_trigger.h"

// Bring-up audit probe. MUST be 0 before release.
#define ALBW_BOKO_DT_PROBE 0

namespace {

DEFINE_HOOK(&daE_OC_c::execute, OcExecute);

bool s_inSubStep = false;

// Death predicate: E_OC's own terminal check — never redispatch a dying Bokoblin.
bool ocDtIsDead(fopAc_ac_c* a) {
    return static_cast<daE_OC_c*>(a)->checkBeforeDeath();
}

// Redispatch: THE second execute. Qualified to reach the real body — an
// unqualified execute() would re-enter this POST hook.
void ocDtRedispatch(fopAc_ac_c* a) {
    static_cast<daE_OC_c*>(a)->daE_OC_c::execute();
}

// Bokoblin profile: standard health (generic reader, no override), redispatch-
// safe (clean sub-step). No compat pre/post pass until the audit proves one is
// needed — measuring that is step 2.
const DTProfile s_ocDtProfile = {
    fpcNm_E_OC_e,      // profName
    true,              // redispatchSafe
    ocDtIsDead,        // isDead
    nullptr,           // preRedispatch
    ocDtRedispatch,    // redispatch
    nullptr,           // postRedispatch
    nullptr,           // fallbackTick
};

void on_oc_execute_post(ModContext*, void* args, void*, void*) {
    if (s_inSubStep) {
        return;  // the second execute re-enters here — absorb it
    }
    auto* self = mods::arg<daE_OC_c*>(args, 0);
    if (self == nullptr) {
        return;
    }

#if ALBW_BOKO_DT_PROBE
    const float fracBefore = dAlbwDevil_healthFraction(self);
    const int actBefore = self->getActionMode();
    const cXyz posBefore = self->current.pos;
#endif

    // Latch around the whole call: the profile's redispatch re-enters this hook.
    s_inSubStep = true;
    const bool redispatched = dAlbwDevil_execProfile(self);
    s_inSubStep = false;

#if ALBW_BOKO_DT_PROBE
    // Compare AFTER-first vs AFTER-second execute: any timer/HP/action/position
    // value that jumped from the extra execute is a double-processing candidate
    // and a per-actor compat-patch target. Throttled.
    if (redispatched) {
        static u16 sP = 0;
        if ((++sP % 8) == 1) {
            const f32 dpos = (self->current.pos - posBefore).abs();
            DuskLog.info("[bokoDT] redispatch frac {}->{} act {}->{} dpos={} f={}",
                         fracBefore, dAlbwDevil_healthFraction(self), actBefore,
                         self->getActionMode(), dpos, g_Counter.mCounter0);
        }
    }
#endif
}

bool install(ModError*, const char* name, ModResult r) {
    if (r != MOD_OK && svc_log != nullptr) {
        svc_log->error(mod_ctx, name);
        svc_log->error(mod_ctx, "bokoblin-DT hook did NOT install - feature inactive this run");
    }
    return true;
}

}  // namespace

ModResult albw_bokoblin_dt_init(ModError* error) {
    dAlbwDevil_registerProfile(&s_ocDtProfile);
    install(error, "OcExecuteDevil",
            mods::hook_add_post<OcExecute>(svc_hook, on_oc_execute_post));
    if (svc_log != nullptr) {
        svc_log->info(mod_ctx, "albw bokoblin (E_OC) Devil Trigger profile ready");
    }
    return MOD_OK;
}
