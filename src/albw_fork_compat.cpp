// ============================================
// NEW CODE - ALBT multiplatform (fork-added game-code compat)
// See albw_fork_compat.h for the per-item provenance notes.
// ============================================

#include "albw_fork_compat.h"
#include "albw_game.h"
#include "d/actor/d_a_alink.h"

#if TARGET_PC

// ---- 3. Ghost-rat cling state ------------------------------------------------
// Resolve stock's own `static u8 data_8072C454[4]` (d_a_e_nz.cpp:136) by name.
// File-scope statics are not name-mangled on any of the eight targets, so one
// literal works everywhere - no ALBT_SYM split needed here.
namespace {
u8*  s_ratStickBits    = nullptr;
bool s_ratResolveTried = false;
bool s_ratResolveWarned = false;

bool ensure_rat_bits() {
    if (s_ratStickBits != nullptr) return true;
    if (s_ratResolveTried) return false;
    s_ratResolveTried = true;
    if (svc_hook == nullptr) return false;
    void* addr = nullptr;
    if (svc_hook->resolve(mod_ctx, "data_8072C454", &addr, nullptr) != MOD_OK || addr == nullptr) {
        // Do not silently answer "no rat attached" - that would make the outfit
        // swap-block predicate quietly weaker than the fork's forever.
        if (!s_ratResolveWarned && svc_log != nullptr) {
            s_ratResolveWarned = true;
            svc_log->warn(mod_ctx,
                          "albw: could not resolve d_a_e_nz data_8072C454 - the ghost-rat "
                          "cling check is unavailable, outfit swap will not be blocked by it");
        }
        return false;
    }
    s_ratStickBits = static_cast<u8*>(addr);
    return true;
}
}  // namespace

bool dE_NZ_isRatStuckOnPlayer() {
    if (!ensure_rat_bits()) return false;
    return s_ratStickBits[0] != 0;  // fork d_a_e_nz.cpp:148
}

// ---- 4. ALBW meter movement-exhaustion --------------------------------------
namespace albw_meter_impl {
extern bool g_exhausted;  // meter.cpp:87
}
bool dMeter2_isALBWMovementExhausted() { return albw_meter_impl::g_exhausted; }

// ---- 5. Rental ownership -----------------------------------------------------
bool albw_rental_player_owns_item(u8 itemNo);  // rental_eligibility.cpp
bool dMeter2_playerOwnsRentalItem(u8 itemNo) { return albw_rental_player_owns_item(itemNo); }

// ---- 6. Metamorphose-proc predicate -----------------------------------------
// fork d_a_alink.h:3442, re-expressed as a free function (see header note).
bool albw_checkMetamorphoseProcActive(const daAlink_c* link) {
    if (link == nullptr) return false;
    const u16 proc = link->mProcID;
    return proc == daAlink_c::PROC_METAMORPHOSE || proc == daAlink_c::PROC_METAMORPHOSE_ONLY;
}

// ---- 7-13. Fork meter/shield surface ----------------------------------------
bool albw_shield_is_item(u8 itemNo);        // quick_swap.cpp
bool albw_shield_is_owned(u8 itemNo);       // quick_swap.cpp
bool albw_shield_equip_owned(u8 itemNo);    // quick_swap.cpp
void albw_shield_apply_equipped(u8 itemNo); // quick_swap.cpp
namespace albw_meter_impl {
int albw_meter_normal_recovery_rate();   // meter.cpp (inside this namespace)
int albw_meter_lockout_recovery_rate();  // meter.cpp (inside this namespace)
}

bool dMeter2_isShieldItem(u8 itemNo) { return albw_shield_is_item(itemNo); }
bool dMeter2_shieldIsOwned(u8 itemNo) { return albw_shield_is_owned(itemNo); }
bool dMeter2_equipOwnedShield(u8 itemNo) { return albw_shield_equip_owned(itemNo); }
void dMeter2_applyEquippedShield(u8 itemNo) { albw_shield_apply_equipped(itemNo); }

namespace albw_meter_impl {
extern bool g_armor_depleted;  // meter.cpp:88
}
bool dMeter2_isALBWArmorDepleted() { return albw_meter_impl::g_armor_depleted; }
int  dMeter2_getALBWNormalRecoveryRate() { return albw_meter_impl::albw_meter_normal_recovery_rate(); }
int  dMeter2_getALBWLockoutRecoveryRate() { return albw_meter_impl::albw_meter_lockout_recovery_rate(); }

// ---- 14. stricmp (non-MSVC only; MSVC's CRT already has it) ------------------
#ifndef _MSC_VER
#include <strings.h>
int stricmp(const char* str1, const char* str2) { return strcasecmp(str1, str2); }
int strnicmp(const char* str1, const char* str2, int n) {
    return strncasecmp(str1, str2, static_cast<size_t>(n));
}
#endif

#endif  // TARGET_PC

// ============================================
// NEW CODE ENDS HERE
// ============================================
