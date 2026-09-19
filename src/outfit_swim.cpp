// ============================================
// NEW CODE — ALBW Port (Outfit Stats — swim mechanics) — part of dev.albt.albw.
// See outfit_swim.h for the design note: re-gate the out-of-line getZoraSwim() so the
// stock swim machine (dive entry + Zora dive controls + execute()/posMove() buoyancy)
// treats an outfit-wearing human as a Zora swimmer, giving underwater diving natively.
// Then re-apply the two effects the re-gate would otherwise lose: the outfit swim-speed
// scale, and the non-Zora submerged stroke cap. Oxygen still drains natively
// (checkOxygenTimer gates on checkZoraWearAbility(), not getZoraSwim()).
// ============================================

#include "outfit_swim.h"

#include "global.h"
#include "albw_common.h"
#include "config_vars.h"
#include "outfit_stats.h"
#include "modules.h"
#include "mods/svc/hook.hpp"

#define private public
#include "d/actor/d_a_alink.h"
#undef private

#if TARGET_PC

namespace {

// Self-contained (must NOT call getZoraSwim, which we hook — else recursion): an
// outfit-wearing human, in water, who is not a real Zora. allowsSubmergedSwim already
// folds in the outfit-stats enable, human form, and the no-heavy-boots gate.
bool outfit_zora_swim(const daAlink_c* link) {
    if (link == nullptr) {
        return false;
    }
    if (!dAlbwOutfitStats_allowsSubmergedSwim(link)) {
        return false;
    }
    if (const_cast<daAlink_c*>(link)->checkZoraWearAbility()) {
        return false;  // a real Zora — the stock path already handles diving
    }
    return const_cast<daAlink_c*>(link)->checkModeFlg(daAlink_c::MODE_SWIMMING);
}

// getZoraSwim() POST: OR the outfit-human-swim condition into the engine's predicate, so
// every stock site (dive entry, dive controls, execute()/posMove() buoyancy) treats the
// human as a Zora swimmer. getZoraSwim is const -> args[0] is const daAlink_c*.
DEFINE_HOOK(&daAlink_c::getZoraSwim, GetZoraSwim);
void on_get_zora_swim_post(ModContext*, void* args, void* retval, void*) {
    if (retval == nullptr) {
        return;
    }
    const auto* link = mods::arg<const daAlink_c*>(args, 0);
    if (outfit_zora_swim(link)) {
        *static_cast<bool*>(retval) = true;
    }
}

// getSwimFrontMaxSpeed() POST: re-apply the outfit swim-speed scale + the non-Zora
// submerged stroke cap (fork d_a_alink_swim.inc:77 / setSpeedAndAngleSwim). Both self-gate
// on the outfit, so this is inert unless the relevant outfit is worn.
DEFINE_HOOK(&daAlink_c::getSwimFrontMaxSpeed, SwimFrontMaxSpeed);
void on_swim_front_max_speed_post(ModContext*, void* args, void* retval, void*) {
    if (retval == nullptr) {
        return;
    }
    const auto* link = mods::arg<const daAlink_c*>(args, 0);
    if (link == nullptr) {
        return;
    }
    *static_cast<f32*>(retval) *= dAlbwOutfitStats_getSwimSpeedMult(link);
    *static_cast<f32*>(retval) *= dAlbwOutfitStats_getSubmergedHumanSwimSpeedMult(link);
}

}  // namespace

ModResult albw_outfit_swim_init(ModError* error) {
    if (mods::hook::add_post<GetZoraSwim>(on_get_zora_swim_post) != MOD_OK) {
        if (svc_log != nullptr) svc_log->error(mod_ctx, "failed to hook daAlink_c::getZoraSwim");
        mods::set_error(error, MOD_ERROR, "outfit swim getZoraSwim");
        return MOD_ERROR;
    }
    if (mods::hook::add_post<SwimFrontMaxSpeed>(on_swim_front_max_speed_post) != MOD_OK) {
        if (svc_log != nullptr) svc_log->error(mod_ctx, "failed to hook getSwimFrontMaxSpeed");
        mods::set_error(error, MOD_ERROR, "outfit swim getSwimFrontMaxSpeed");
        return MOD_ERROR;
    }
    if (svc_log != nullptr) svc_log->info(mod_ctx, "albw outfit swim (native Zora-swim re-gate) ready");
    return MOD_OK;
}

#endif  // TARGET_PC
