// ============================================
// NEW CODE - Devil Trigger policy (INVENTED, no donor).
// See devil_trigger.h and docs/DEVIL-TRIGGER-SCOPE.md.
// ============================================

#include "devil_trigger.h"

#include "albw_common.h"
#include "albw_dusk_log.h"
#include "config_vars.h"
#include "devil_trigger_probe.h"
#include "hp_mult_port.h"

#include "d/d_com_inf_game.h"
#include "f_op/f_op_actor.h"
#include "f_op/f_op_actor_mng.h"
#include "SSystem/SComponent/c_cc_s.h"
#include "SSystem/SComponent/c_cc_d.h"

#if TARGET_PC

namespace {

// Fraction of health remaining at which an enemy enrages.
constexpr float kArmThreshold = 0.50f;  // user-set: enrage at half health

// Small fixed table rather than a map: the armed set is bounded by how many
// enemies can be alive and low at once, and a fixed array cannot allocate
// mid-fight.
constexpr int kMaxArmed = 32;
fpc_ProcID sArmed[kMaxArmed] = {};
int        sArmedCount = 0;

constexpr int kMaxOverrides = 8;
struct HealthOverride {
    short profName;
    dAlbwDevilHealthFn fn;
};
HealthOverride sOverrides[kMaxOverrides] = {};
int            sOverrideCount = 0;

bool enabled() {
    return albw_cfg_bool(g_devil_trigger, false);
}

int armedIndex(fpc_ProcID id) {
    for (int i = 0; i < sArmedCount; ++i) {
        if (sArmed[i] == id) {
            return i;
        }
    }
    return -1;
}

}  // namespace

// ============================================
// Eligibility. Reuses the mod's existing enemy tier table rather than
// introducing a second classification - dAlbwHP_getCategory already backs the
// HP-multiplier settings, so "common" and "mid-boss" mean exactly what they
// mean everywhere else in the mod.
// ============================================
bool dAlbwDevil_isEligible(fopAc_ac_c* actor) {
    if (!enabled() || actor == NULL) {
        return false;
    }
    if (fopAcM_GetGroup(actor) != fopAc_ENEMY_e) {
        return false;
    }
    const dAlbwHP_Category cat = dAlbwHP_getCategory(fopAcM_GetName(actor));
    return cat == dAlbwHP_NORMAL || cat == dAlbwHP_MID_BOSS;
}

void dAlbwDevil_registerHealthOverride(short profName, dAlbwDevilHealthFn fn) {
    if (fn == NULL || sOverrideCount >= kMaxOverrides) {
        return;
    }
    sOverrides[sOverrideCount].profName = profName;
    sOverrides[sOverrideCount].fn = fn;
    ++sOverrideCount;
}

// ============================================
// Health fraction.
//
// The generic reading (health / field_0x560) is what enemy_hp_bars.cpp uses,
// and it is right for most actors. It is WRONG wherever `health` is not a
// pool - see the Darknut override in btn_parry.cpp, where `health` is slammed
// to 100 before every damage evaluation and the real pool lives elsewhere.
// Overrides take precedence; anything without one uses the generic reading.
// ============================================
float dAlbwDevil_healthFraction(fopAc_ac_c* actor) {
    if (actor == NULL) {
        return -1.0f;
    }

    const short name = fopAcM_GetName(actor);
    for (int i = 0; i < sOverrideCount; ++i) {
        if (sOverrides[i].profName == name) {
            float frac = -1.0f;
            if (sOverrides[i].fn(actor, &frac)) {
                return frac;
            }
            return -1.0f;
        }
    }

    const short peak = actor->field_0x560 > 0 ? actor->field_0x560 : actor->health;
    if (peak <= 0) {
        return -1.0f;
    }
    return static_cast<float>(actor->health) / static_cast<float>(peak);
}

// ============================================
// Arming. LATCHED on purpose: health crossing the threshold repeatedly (a heal,
// a damage-then-regen, or simply integer noise at the boundary) must not
// toggle the mode. Once armed, an actor stays armed until it is forgotten.
// ============================================
bool dAlbwDevil_isArmed(fopAc_ac_c* actor) {
    if (!enabled() || actor == NULL) {
        return false;
    }
    return armedIndex(fopAcM_GetID(actor)) >= 0;
}

void dAlbwDevil_tickActor(fopAc_ac_c* actor) {
    if (!enabled() || actor == NULL) {
        return;
    }
    if (armedIndex(fopAcM_GetID(actor)) >= 0) {
        return;  // already armed - latched
    }
    if (!dAlbwDevil_isEligible(actor)) {
        return;
    }

    const float frac = dAlbwDevil_healthFraction(actor);
    if (frac < 0.0f || frac > kArmThreshold) {
        return;
    }
    if (sArmedCount >= kMaxArmed) {
        return;  // full: fail closed, never overwrite another actor's slot
    }

    sArmed[sArmedCount++] = fopAcM_GetID(actor);
#if ALBW_DEVIL_PROBE
    DuskLog.info("[devil] ARM name={} frac={} armed={}/{}", (int)fopAcM_GetName(actor), frac,
                 sArmedCount, kMaxArmed);
#endif
}

void dAlbwDevil_forget(fopAc_ac_c* actor) {
    if (actor == NULL) {
        return;
    }
    const int i = armedIndex(fopAcM_GetID(actor));
    if (i < 0) {
        return;
    }
    sArmed[i] = sArmed[sArmedCount - 1];
    --sArmedCount;
#if ALBW_DEVIL_PROBE
    DuskLog.info("[devil] FORGET name={} armed={}", (int)fopAcM_GetName(actor), sArmedCount);
#endif
}

// ============================================
// Is an attack live? The generic signal from the scope: the collision system
// holds every AT collider registered this pass, and each knows its owner.
//
// Caveats, both real and both in the scope: the list is filled DURING execute,
// so a caller inside execute reads the previous frame; and an actor that keeps
// an AT collider permanently set will always look like it is attacking, so it
// simply never speeds up. That is a safe failure, not a dangerous one.
// ============================================
bool dAlbwDevil_isAttackLive(fopAc_ac_c* actor) {
    if (actor == NULL) {
        return false;
    }
    dCcS* ccs = dComIfG_Ccsp();
    if (ccs == NULL) {
        return true;  // cannot tell -> assume attacking -> do not speed up
    }
    const int n = static_cast<int>(ccs->mObjAtCount);
    for (int i = 0; i < n; ++i) {
        cCcD_Obj* obj = ccs->mpObjAt[i];
        if (obj != NULL && obj->GetAc() == actor) {
            return true;
        }
    }
    return false;
}

void albw_devil_trigger_reset() {
    sArmedCount = 0;
}

ModResult albw_devil_trigger_init(ModError*) {
    // Reset the ARMED set only.
    //
    // sOverrideCount is deliberately NOT cleared here. Health overrides are
    // registered by the actor modules during their own init, and this init
    // runs AFTER btn_parry_init in the chain (mod.cpp:209-210) - so zeroing
    // the table here silently discarded the Darknut override and the policy
    // fell back to the generic health/field_0x560 reading. That is precisely
    // the reading which is meaningless for a Darknut, so the probe showed a
    // health fraction frozen at 31% while the real pool ran 0 -> 360.
    //
    // Making this order-independent is the fix, not reordering the chain: the
    // table is statically zero-initialised, registration is one-time, and no
    // init should depend on being called before its own clients.
    sArmedCount = 0;
    return MOD_OK;
}

#endif  // TARGET_PC

// ============================================
// NEW CODE ENDS HERE
// ============================================
