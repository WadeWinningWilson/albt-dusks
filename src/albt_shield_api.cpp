// ============================================
// NEW CODE - ALBT shield/guard compatibility service (provider side)
//
// Publishes dev.albt.albw.shield so another mod can coexist with ALBT's
// parry/bash combat instead of racing it. See src/albt_shield_api.h for the
// interface and docs/COMPAT-LAZYTWEAKS.md for the integration note.
//
// The concrete problem this solves: Lazy Tweaks hooks the same daAlink_c guard
// path we do (procGuardAttackInit, checkGuardActionChange, setBStatus,
// procGuardSlipInit, setShieldGuard, procCutNormalInit). With both mods
// installed the guard chord is consumed before ALBT's bash entry is evaluated,
// so the bash silently stops firing even on a full charge bar. Hook priority
// alone would only reorder the race; this makes the handoff explicit.
//
// Everything here is read-only against ALBT state except request_input_yield,
// which sets a frame countdown the bash entry checks. No save data is touched.
// ============================================

#include "global.h"

#include "albt_shield_api.h"
#include "albw_common.h"
#include "albw_game.h"
#include "shield.h"
#include "shield_adapt.h"

#include "mods/service.hpp"

#if TARGET_PC

namespace {

// Frames remaining in a cooperating mod's input claim. Decremented once per
// frame by dAlbtShieldApi_tick(); while non-zero the bash entry stands down.
u8 s_inputYieldFrames = 0;

bool svc_is_parry_combat_enabled(ModContext*) {
    return dShield_isParryCombatEnabled();
}

bool svc_owns_guard_input(ModContext*) {
    if (!dShield_isParryCombatEnabled()) {
        return false;
    }
    // Deliberately the same predicate the bash entry uses, so a consumer that
    // honours this can never be told "not mine" on a frame we then act on.
    return albw_manual_shield_button(albw_link_actor());
}

bool svc_bash_available(ModContext*) {
    if (!dShield_isParryCombatEnabled()) {
        return false;
    }
    const u8 max = dShield_getMaxBashCharges();
    return max != 0 && dShield_getBashCharges() >= dShield_getBashThreshold();
}

unsigned char svc_get_bash_charges(ModContext*) {
    return dShield_getBashCharges();
}

unsigned char svc_get_max_bash_charges(ModContext*) {
    return dShield_getMaxBashCharges();
}

void svc_request_input_yield(ModContext*, unsigned char frames) {
    s_inputYieldFrames = frames;
}

}  // namespace

constexpr AlbtShieldService g_albt_shield_service{
    SERVICE_HEADER(AlbtShieldService, ALBT_SHIELD_SERVICE_MAJOR, ALBT_SHIELD_SERVICE_MINOR),
    svc_is_parry_combat_enabled,
    svc_owns_guard_input,
    svc_bash_available,
    svc_get_bash_charges,
    svc_get_max_bash_charges,
    svc_request_input_yield,
};
EXPORT_SERVICE_AS(g_albt_shield_service, ALBT_SHIELD_SERVICE_ID);

bool dAlbtShieldApi_inputYielded() {
    return s_inputYieldFrames != 0;
}

void dAlbtShieldApi_tick() {
    if (s_inputYieldFrames != 0) {
        s_inputYieldFrames--;
    }
}

#else

bool dAlbtShieldApi_inputYielded() {
    return false;
}
void dAlbtShieldApi_tick() {}

#endif  // TARGET_PC
