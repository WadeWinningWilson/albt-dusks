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
    if (status->getWalletSize() == ALBW_COLOSSAL_WALLET) {
        *static_cast<u16*>(retval) = (u16)ALBW_COLOSSAL_WALLET_MAX;
    }
}

}  // namespace

// fork f_ap_game.cpp:886, verbatim in behaviour: checked every frame but only
// upgrades once - the < COLOSSAL_WALLET guard makes it a no-op afterwards.
// Retroactive for saves that already cleared the Cave of Ordeals.
void albw_colossal_wallet_tick() {
    if (albw_game::is_event_bit(dSv_event_flag_c::saveBitLabels[0x1F9]) &&
        dComIfGs_getWalletSize() < ALBW_COLOSSAL_WALLET) {
        dComIfGs_setWalletSize((u8)ALBW_COLOSSAL_WALLET);
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
