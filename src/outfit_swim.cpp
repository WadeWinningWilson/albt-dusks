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

// ============================================
// NEW CODE — ALBW Port (whole swim-proc replacements, Dusklight 2.0)
// The fork's dive behavior lives in daAlink_c swim METHODS (dive entry, aim-down
// pitch, stay-submerged, reduced oxygen) that gate on checkZoraWearAbility(),
// which the getZoraSwim re-gate below cannot reach. We can't redefine the stock
// (exported) methods, so the ported fork bodies live on a layout-compatible
// subclass AlbwSwim_c; each stock proc is hooked and, for outfit swimmers,
// dispatched to the subclass copy (qualified call = static dispatch). Sibling
// calls inside the bodies resolve to the exported stock methods via inheritance.
// Bodies are auto-extracted (tools/port/outfit_swim.json -> outfit_swim_port.inc).
// ============================================
#define IF_DUSK(x)  // fork dusk-settings inline (invert-axis) — dropped in the port

namespace {
// Fork diagnostic logger the ported bodies call — no-op in the shipped mod.
inline void albwSwimSurfLog(const char*, const daAlink_c*, const char*) {}
}  // namespace

class AlbwSwim_c : public daAlink_c {
public:
    f32  getSwimFrontMaxSpeed() const;
    f32  getSwimMaxFallSpeed() const;
    void checkOxygenTimer();
    void offOxygenTimer();
    void setSpeedAndAngleSwim();
    int  checkNextActionSwim();
    int  checkSwimAction(int param_0);
    int  checkSwimUpAction();
    void setSwimMoveAnime();
    int  procSwimUpInit();
    int  procSwimMove();
};

#include "outfit_swim_port.inc"  // AlbwSwim_c::* ported fork bodies

namespace {

// Run the ported (fork) swim proc for outfit swimmers; stock for everyone else.
bool outfit_swimmer(const daAlink_c* link) {
    return link != nullptr && dAlbwOutfitStats_allowsSubmergedSwim(link);
}

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

// ---- whole swim-proc replacements: run the fork body for outfit swimmers ----
#define ALBW_SWIM_HOOK_VOID(Method, Tag)                                                    \
    DEFINE_HOOK(&daAlink_c::Method, Tag);                                                    \
    HookAction on_##Tag(ModContext*, void* args, void*, void*) {                             \
        daAlink_c* link = mods::arg<daAlink_c*>(args, 0);                                    \
        if (!outfit_swimmer(link)) return HOOK_CONTINUE;                                     \
        static_cast<AlbwSwim_c*>(link)->AlbwSwim_c::Method();                                \
        return HOOK_SKIP_ORIGINAL;                                                           \
    }

#define ALBW_SWIM_HOOK_INT(Method, Tag)                                                     \
    DEFINE_HOOK(&daAlink_c::Method, Tag);                                                    \
    HookAction on_##Tag(ModContext*, void* args, void* retval, void*) {                      \
        daAlink_c* link = mods::arg<daAlink_c*>(args, 0);                                    \
        if (!outfit_swimmer(link)) return HOOK_CONTINUE;                                     \
        const int r = static_cast<AlbwSwim_c*>(link)->AlbwSwim_c::Method();                  \
        if (retval != nullptr) *static_cast<int*>(retval) = r;                               \
        return HOOK_SKIP_ORIGINAL;                                                           \
    }

ALBW_SWIM_HOOK_VOID(setSpeedAndAngleSwim, SetSpeedAndAngleSwim)
ALBW_SWIM_HOOK_VOID(setSwimMoveAnime, SetSwimMoveAnime)
ALBW_SWIM_HOOK_VOID(checkOxygenTimer, CheckOxygenTimer)
ALBW_SWIM_HOOK_VOID(offOxygenTimer, OffOxygenTimer)
ALBW_SWIM_HOOK_INT(checkSwimUpAction, CheckSwimUpAction)
ALBW_SWIM_HOOK_INT(procSwimUpInit, ProcSwimUpInit)
ALBW_SWIM_HOOK_INT(checkNextActionSwim, CheckNextActionSwim)
ALBW_SWIM_HOOK_INT(procSwimMove, ProcSwimMove)

// checkSwimAction takes an int param (the water-entry mode); forward it.
DEFINE_HOOK(&daAlink_c::checkSwimAction, CheckSwimAction);
HookAction on_CheckSwimAction(ModContext*, void* args, void* retval, void*) {
    daAlink_c* link = mods::arg<daAlink_c*>(args, 0);
    if (!outfit_swimmer(link)) return HOOK_CONTINUE;
    const int param = mods::arg<int>(args, 1);
    const int r = static_cast<AlbwSwim_c*>(link)->AlbwSwim_c::checkSwimAction(param);
    if (retval != nullptr) *static_cast<int*>(retval) = r;
    return HOOK_SKIP_ORIGINAL;
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

    // Whole swim-proc replacements (fork dive behavior for outfit swimmers).
    const bool swimProcs =
        mods::hook::add_pre<SetSpeedAndAngleSwim>(on_SetSpeedAndAngleSwim) == MOD_OK &&
        mods::hook::add_pre<SetSwimMoveAnime>(on_SetSwimMoveAnime) == MOD_OK &&
        mods::hook::add_pre<CheckOxygenTimer>(on_CheckOxygenTimer) == MOD_OK &&
        mods::hook::add_pre<OffOxygenTimer>(on_OffOxygenTimer) == MOD_OK &&
        mods::hook::add_pre<CheckSwimUpAction>(on_CheckSwimUpAction) == MOD_OK &&
        mods::hook::add_pre<ProcSwimUpInit>(on_ProcSwimUpInit) == MOD_OK &&
        mods::hook::add_pre<CheckNextActionSwim>(on_CheckNextActionSwim) == MOD_OK &&
        mods::hook::add_pre<ProcSwimMove>(on_ProcSwimMove) == MOD_OK &&
        mods::hook::add_pre<CheckSwimAction>(on_CheckSwimAction) == MOD_OK;
    if (!swimProcs) {
        if (svc_log != nullptr) svc_log->error(mod_ctx, "failed to hook a swim proc replacement");
        mods::set_error(error, MOD_ERROR, "outfit swim proc replacements");
        return MOD_ERROR;
    }
    if (svc_log != nullptr) svc_log->info(mod_ctx, "albw outfit swim (native Zora-swim re-gate) ready");
    return MOD_OK;
}

#endif  // TARGET_PC
