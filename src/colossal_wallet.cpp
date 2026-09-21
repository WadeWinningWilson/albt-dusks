// ============================================
// NEW CODE - ALBW Port (Colossal Wallet, 4th wallet tier)
// See colossal_wallet.h for the fork provenance.
// ============================================

#include "global.h"
#include <os.h>

#include "colossal_wallet.h"
#include "albw_common.h"
#include "albw_game.h"
#include "d/d_com_inf_game.h"
#include "d/d_save.h"
#include "mods/hook.hpp"

#if TARGET_PC

namespace {

// fork d_save.cpp:152. Stock's getRupeeMax() switch has no case for wallet size
// 3, so it falls out and returns whatever the < 3 guard yields. Rather than
// reimplement the whole switch, let vanilla run and correct ONLY the new tier -
// the additive-with-suppression shape used elsewhere in this mod. Tiers 0-2 are
// left exactly as stock computes them (the fork's biggerWallets setting is a
// host setting and is deliberately not applied here).
DEFINE_HOOK(&dSv_player_status_a_c::getRupeeMax, GetRupeeMax);

void on_get_rupee_max_post(ModContext*, void* args, void* retval, void*) {
    auto* status = mods::arg<dSv_player_status_a_c*>(args, 0);
    if (status == nullptr || retval == nullptr) {
        return;
    }
    if (albw_colossal_wallet_owned()) {
        *static_cast<u16*>(retval) = (u16)ALBW_COLOSSAL_WALLET_MAX;
    }
}

}  // namespace

// ============================================
// OWNERSHIP IS DERIVED, NOT STORED. This replaces a save write that destroyed
// player data on uninstall.
//
// THE BUG THIS FIXES. The previous version wrote wallet size 3 into the save
// (dComIfGs_setWalletSize) and relied on the getRupeeMax POST hook above to
// give tier 3 a meaningful cap. Stock has no tier 3: getRupeeMax
// (dusklight-main/src/d/d_save.cpp:119-143) returns 0 for any size >= 3. So
// with the mod REMOVED the hook is gone, getRupeeMax returns 0, and
// dMeter2_c::moveRupee (dusklight-main/src/d/d_meter2.cpp:1082-1089) does
//     r29 = getRupee() + itemRupeeCount;
//     if (r29 > temp_r5) r29 = temp_r5;   // temp_r5 == 0
//     dComIfGs_setRupee(r29);             // -> 0
// - the first rupee the player picks up zeroes their wallet, permanently. A
// mod must not be able to damage a save it is no longer installed in.
//
// THE FIX. Nothing is stored. The eligibility bit was ALREADY being read one
// line above the write, so the derivation costs nothing: F_0505 (index 0x1F9,
// Cave of Ordeals cleared) is a stock flag the mod only reads, and it is the
// exact condition the fork grants the tier on (fork f_ap_game.cpp:886). The
// POST hook now keys on that instead of on a stored tier, so the save keeps a
// legal stock wallet size and behaves correctly with or without us.
//
// WHY A PREDICATE AND NOT AN INLINE CHECK: rental_shop.cpp:188 also gated the
// Deity Armor row on getWalletSize() == 3. Deriving in only one place would
// have silently removed that row. Every consumer asks this function.
// ============================================
bool albw_colossal_wallet_owned() {
    return albw_game::is_event_bit(dSv_event_flag_c::saveBitLabels[0x1F9]);
}

// ============================================
// REPAIR - the one deliberate save write in this file, and it exists solely to
// undo the damage described above.
//
// Saves touched by an earlier build already carry wallet size 3. Deriving from
// here on does not help them: the illegal value is already written, and it is
// what poisons getRupeeMax the moment the mod is uninstalled. So when we see
// it, we put the save back to a legal stock tier. GIANT_WALLET (2) is the
// correct landing value - it is stock's maximum, and the player's actual
// colossal capacity now comes from the derivation, not from the stored tier.
//
// This is a WRITE-DOWN to a value stock defines, never a write-up, and the
// guard makes it a one-shot: after the repair getWalletSize() is 2 and the
// condition is false forever.
// ============================================
void albw_colossal_wallet_tick() {
    if (dComIfGs_getWalletSize() > ALBW_STOCK_MAX_WALLET) {
        dComIfGs_setWalletSize((u8)ALBW_STOCK_MAX_WALLET);
        if (svc_log != nullptr) {
            svc_log->warn(mod_ctx, "albw: repaired an out-of-range wallet size written by an "
                                   "earlier build (3 -> 2); colossal capacity is now derived "
                                   "from the Cave of Ordeals flag and nothing is stored");
        }
    }
}

ModResult albw_colossal_wallet_init(ModError* error) {
    const ModResult r = mods::hook_add_post<GetRupeeMax>(svc_hook, on_get_rupee_max_post);
    if (r != MOD_OK) {
        if (svc_log != nullptr) {
            svc_log->error(mod_ctx, "albw: failed to hook getRupeeMax - the Colossal Wallet "
                                    "would cap at the Giant Wallet's limit");
        }
        mods::set_error(error, MOD_ERROR, "GetRupeeMax");
        return r;
    }
    return MOD_OK;
}

#endif  // TARGET_PC

// ============================================
// NEW CODE ENDS HERE
// ============================================
