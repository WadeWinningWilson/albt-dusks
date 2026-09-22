// ============================================
// World slow-motion - the engine half of Flurry Rush.
//
// PORTED FROM FORK. The fork slows the world by scaling every J3D animation
// frame controller and exempting Link:
//
//   fork libs/JSystem/src/J3DGraphAnimator/J3DAnimation.cpp:140-146
//       void J3DFrameCtrl::update() {
//       #if TARGET_PC
//           updateWithRateScale(dusk_world_sim_time_scale);
//       #else
//           updateWithRateScale(1.0f);
//       #endif
//       }
//
//   fork libs/JSystem/src/J3DGraphAnimator/J3DAnimation.cpp:149-220
//       void J3DFrameCtrl::updateWithRateScale(f32 rateScale)   <- the real body
//
//   fork src/d/actor/d_a_player.cpp:31-38
//       void daPy_frameCtrl_c::updateFrame() {
//       #if TARGET_PC
//           // Flurry Rush slows the world, not Link.
//           updateWithRateScale(1.0f);
//       #else
//           update();
//       #endif
//           offNowSetFlg();
//       }
//
// Stock has neither function and no host-level time-scale service exists, so
// the fork's own body is ported here and driven from two typed hooks. Both
// targets are header-declared (stock J3DAnimation.h:980, d_a_player.h:15) and
// both are exported (?update@J3DFrameCtrl@@QEAAXXZ /
// ?updateFrame@daPy_frameCtrl_c@@QEAAXXZ), so no DEFINE_HOOK_SYMBOL is needed
// and tools/check_hooks.py's file-local-static baseline is untouched.
// ============================================

#include "sim_time_scale.h"

#include "albw_common.h"
#include "albw_dusk_log.h"
#include "flurry_probe.h"  // ALBW_FLURRY_PROBE (shared with flurry_proc.cpp)

#include "d/actor/d_a_player.h"                        // daPy_frameCtrl_c (stock :15)
#include "JSystem/J3DGraphAnimator/J3DAnimation.h"     // J3DFrameCtrl (stock :980)

#include "mods/svc/hook.hpp"

// Slow-motion probe. Logs ON CHANGE only (never per frame) and must be 0
// before release (docs/RELEASE-PROCEDURE.md step 1). With it on, one run tells
// "working" from "hooked but inert" without guessing: the scale in effect,
// that Link is exempt, and how many controllers were actually scaled.
//
// ALBW_FLURRY_PROBE itself now lives in flurry_probe.h (included above). It
// moved there when the attack proc landed so the two halves of Flurry Rush -
// this world slow-motion and flurry_proc.cpp - share ONE switch to turn off
// before a release, rather than each carrying its own.

namespace {

// ============================================
// PORTED VERBATIM FROM FORK
//   libs/JSystem/src/J3DGraphAnimator/J3DAnimation.cpp:149-220
//   void J3DFrameCtrl::updateWithRateScale(f32 rateScale)
//
// Diffed against STOCK J3DFrameCtrl::update()
// (dusklight-main J3DAnimation.cpp:141-210) before porting. The whole delta is
// three lines:
//
//   -void J3DFrameCtrl::update() {
//   -    IF_DUSK(dusk::interp::material::Update materialUpdate(*this));
//   +void J3DFrameCtrl::updateWithRateScale(f32 rateScale) {
//        mState = 0;
//   -    mFrame += mRate;
//   +    mFrame += mRate * rateScale;
//
// Everything from `switch (mAttribute)` down is byte-identical in both trees.
//
// Only mechanical changes were made, all forced by the free-function form the
// mod must use (it cannot add a member to J3DFrameCtrl):
//   * `this->` spelled explicitly as `self->` on every member access;
//   * the enumerators qualified `J3DFrameCtrl::EMode_*` (unqualified inside a
//     member function, not visible at namespace scope).
// No statement was added, removed, reordered or rewritten.
//
// The `IF_DUSK(dusk::interp::material::Update ...)` line is NOT reproduced -
// see the fidelity note in MANIFEST.md. It cannot be: dusk::interp lives in
// the host's src/dusk/interp/material.h, ships in no SDK header, and exports
// zero symbols (`interp@dusk` count in dusklight_exports.def is 0).
// ============================================
void J3DFrameCtrl_updateWithRateScale(J3DFrameCtrl* self, f32 rateScale) {
    self->mState = 0;
    self->mFrame += self->mRate * rateScale;

    switch (self->mAttribute) {
    case J3DFrameCtrl::EMode_NONE:
        if (self->mFrame < self->mStart) {
            self->mFrame = self->mStart;
            self->mRate = 0.0f;
            self->mState |= (u8)1;
        }
        if (self->mFrame >= self->mEnd) {
            self->mFrame = self->mEnd - 0.001f;
            self->mRate = 0.0f;
            self->mState |= (u8)1;
        }
        break;
    case J3DFrameCtrl::EMode_RESET:
        if (self->mFrame < self->mStart) {
            self->mFrame = self->mStart;
            self->mRate = 0.0f;
            self->mState |= (u8)1;
        }
        if (self->mFrame >= self->mEnd) {
            self->mFrame = self->mStart;
            self->mRate = 0.0f;
            self->mState |= (u8)1;
        }
        break;
    case J3DFrameCtrl::EMode_LOOP:
        while (self->mFrame < self->mStart) {
            self->mState |= (u8)2;
            if (self->mLoop - self->mStart <= 0.0f) {
                break;
            }
            self->mFrame += self->mLoop - self->mStart;
        }
        while (self->mFrame >= self->mEnd) {
            self->mState |= (u8)2;
            if (self->mEnd - self->mLoop <= 0.0f) {
                break;
            }
            self->mFrame -= self->mEnd - self->mLoop;
        }
        break;
    case J3DFrameCtrl::EMode_REVERSE:
        if (self->mFrame >= self->mEnd) {
            self->mFrame = self->mEnd - (self->mFrame - self->mEnd);
            self->mRate = -self->mRate;
        }
        if (self->mFrame < self->mStart) {
            self->mFrame = self->mStart - (self->mFrame - self->mStart);
            self->mRate = 0.0f;
            self->mState |= (u8)1;
        }
        break;
    case J3DFrameCtrl::EMode_LOOP_REVERSE:
        if (self->mFrame >= self->mEnd - 1.0f) {
            self->mFrame = (self->mEnd - 1.0f) - (self->mFrame - (self->mEnd - 1.0f));
            self->mRate = -self->mRate;
        }
        if (self->mFrame < self->mStart) {
            self->mFrame = self->mStart - (self->mFrame - self->mStart);
            self->mRate = -self->mRate;
            self->mState |= (u8)2;
        }
        break;
    }
}

// ============================================
// The Link exemption, and why a PRE hook on updateFrame is the same POSITION
// as the fork's edit.
//
// The fork does NOT exempt an OBJECT, it exempts a CALL SITE: it changed the
// one call inside daPy_frameCtrl_c::updateFrame from update() to
// updateWithRateScale(1.0f). Nothing about the daPy_frameCtrl_c instance is
// marked; a daPy_frameCtrl_c reached any other way is still scaled in the
// fork.
//
// Stock's body is exactly:
//     void daPy_frameCtrl_c::updateFrame() { update(); offNowSetFlg(); }
// so a PRE hook opens a window that (a) begins BEFORE the update() call -
// there is no statement before it to miss - and (b) closes in POST, after
// offNowSetFlg(), which touches no frame controller. The window therefore
// covers exactly one update() call and nothing else that the scale can reach:
// same call site, same semantics as the donor's edit.
//
// Reentrancy: update()'s body (ported above) calls nothing, so no other
// controller can be updated inside the window and be wrongly exempted. The
// counter rather than a bool is belt-and-braces against a future nested call.
//
// Deliberately NOT early-outed on the scale. Incrementing and decrementing an
// int is cheaper than the float compare, and an early-out here would be a
// correctness bug rather than a saving: if the scale changed between PRE and
// POST the counter would leak and Link would be exempt (or not) forever.
// daPy_frameCtrl_c::updateFrame is a handful of calls per frame - Link's own
// controllers - not the whole-game surface that update() is.
// ============================================
int s_linkExemptDepth = 0;

// ============================================
// Both hooks or neither. A partial install - update() hooked, updateFrame not
// - would slow LINK as well as the world, which is worse than no feature. If
// the pair does not come up, the world scale is refused here as well as
// uninstalled (uninstall is an optional service entry and can report
// MOD_UNAVAILABLE). Short-circuit order keeps this free when the feature is
// off: the scale compare fails first and this is never loaded.
// ============================================
bool s_slowmoInstalled = false;

#if ALBW_FLURRY_PROBE
// ============================================
// Probe state. All of it is behind ALBW_FLURRY_PROBE and all logging happens
// on a scale TRANSITION, never per frame.
//
// A slow-mo window reports, when it ends:
//   scaled=N        controllers whose frame advance we actually scaled
//   linkExempt=N    updateFrame calls that ran stock (Link at 1.0x)
// scaled == 0 over a whole window is the "hooked but inert" signature (target
// resolved but never reached - e.g. every caller got an inlined copy of
// update()), and it is logged LOUD through error(), never swallowed.
// ============================================
float s_probeLastScale = 1.0f;
unsigned s_probeScaledCalls = 0;
unsigned s_probeLinkExemptCalls = 0;
bool s_probeFirstScaledLogged = false;
bool s_probeFirstExemptLogged = false;

// Called from the updateFrame POST hook: once per Link controller per frame,
// off the hot path, and it still runs when the scale is 1.0 so the window's
// END is noticed (the update() hook early-outs and would never see it).
void probe_note_scale(float scale) {
    if (scale == s_probeLastScale) {
        return;
    }

    const float previous = s_probeLastScale;
    s_probeLastScale = scale;

    if (scale < 0.999f) {
        DuskLog.info("[slowmo] scale {} -> {} : world slow-mo ON (Link exempt)", previous, scale);
        s_probeScaledCalls = 0;
        s_probeLinkExemptCalls = 0;
        s_probeFirstScaledLogged = false;
        s_probeFirstExemptLogged = false;
        return;
    }

    DuskLog.info("[slowmo] scale {} -> {} : window ended, scaled={} linkExempt={}", previous,
                 scale, s_probeScaledCalls, s_probeLinkExemptCalls);
    if (s_probeScaledCalls == 0) {
        DuskLog.error("[slowmo] INERT: a whole {}x window scaled ZERO controllers - the "
                      "J3DFrameCtrl::update hook is installed but never reached",
                      previous);
    }
    if (s_probeLinkExemptCalls == 0) {
        DuskLog.error("[slowmo] INERT: zero daPy_frameCtrl_c::updateFrame calls during a {}x "
                      "window - Link's exemption cannot be proven, Link may be slowed too",
                      previous);
    }
}
#endif  // ALBW_FLURRY_PROBE

DEFINE_HOOK(&J3DFrameCtrl::update, J3DFrameCtrlUpdate);
DEFINE_HOOK(&daPy_frameCtrl_c::updateFrame, DaPyFrameCtrlUpdateFrame);

// ============================================
// THE HOT PATH. This runs for every animation in the game - actors, UI,
// cutscenes, material animation - many times per frame.
//
// The scale compare is the FIRST STATEMENT, before any other work: no
// argument decode, no null check, no probe counter, no call into another TU
// (that is why world_sim_time_scale() is an inline read of a global rather
// than sim_time_scale.cpp's get_sim_time_scale()). With the feature off the
// callback is one load, one compare, one branch, and then the stock function
// runs completely untouched - so "toggle off == provably stock" holds by
// construction, not by review.
// ============================================
HookAction on_j3d_frame_ctrl_update_pre(ModContext*, void* args, void*, void*) {
    if (!s_slowmoInstalled) {
        return HOOK_CONTINUE;
    }
    // World slow (flurry, <=1) and per-actor boost (Devil Trigger, >=1) compose
    // by multiplication: a boosted enemy inside a flurry window is still slowed
    // by it. Off, both are 1.0 and this is a load-compare-branch no-op - the
    // hot-path contract the flurry port depends on is preserved.
    const float scale = albw::world_sim_time_scale() * albw::anim_boost_current();
    if (scale >= 0.999f && scale <= 1.001f) {
        return HOOK_CONTINUE;
    }

    // Link: the fork runs updateWithRateScale(1.0f) here, which is stock's
    // update() minus the material-interpolation guard. Letting the ORIGINAL
    // run is that same arithmetic and keeps the guard - strictly closer to
    // stock than reproducing the fork's 1.0f call would be.
    if (s_linkExemptDepth > 0) {
        return HOOK_CONTINUE;
    }

    auto* self = mods::arg<J3DFrameCtrl*>(args, 0);
    if (self == nullptr) {
        return HOOK_CONTINUE;
    }

    J3DFrameCtrl_updateWithRateScale(self, scale);

#if ALBW_FLURRY_PROBE
    s_probeScaledCalls++;
    if (!s_probeFirstScaledLogged) {
        s_probeFirstScaledLogged = true;
        DuskLog.info("[slowmo] first scaled controller: ctrl={} attr={} rate={} frame={} "
                     "scale={}",
                     (void*)self, (int)self->mAttribute, self->mRate, self->mFrame, scale);
    }
#endif

    return HOOK_SKIP_ORIGINAL;
}

// fork d_a_player.cpp:31-38 - opens the "this controller is Link's" window.
HookAction on_da_py_frame_ctrl_update_frame_pre(ModContext*, void*, void*, void*) {
    s_linkExemptDepth++;

#if ALBW_FLURRY_PROBE
    if (albw::world_sim_time_scale() < 0.999f) {
        s_probeLinkExemptCalls++;
        if (!s_probeFirstExemptLogged) {
            s_probeFirstExemptLogged = true;
            DuskLog.info("[slowmo] Link exempt: daPy_frameCtrl_c::updateFrame running at 1.0x "
                         "while the world is at {}x",
                         albw::world_sim_time_scale());
        }
    }
#endif

    return HOOK_CONTINUE;
}

void on_da_py_frame_ctrl_update_frame_post(ModContext*, void*, void*, void*) {
    if (s_linkExemptDepth > 0) {
        s_linkExemptDepth--;
    }

#if ALBW_FLURRY_PROBE
    probe_note_scale(albw::world_sim_time_scale());
#endif
}

// ============================================
// A hook that fails to resolve must NOT abort mod_initialize. Same doctrine as
// flurry_hooks.cpp: the miss is LOUD and SCOPED - slow-motion is inactive for
// the run and says so by name - and every other feature still loads.
//
// This matters more here than elsewhere: if only ONE of the two installs, the
// result is not "no slow-mo". Losing the updateFrame hook while update() is
// hooked would slow LINK as well as the world. So a partial install is
// downgraded to no install at all.
// ============================================
bool report(const char* name, ModResult r) {
    if (r != MOD_OK) {
        if (svc_log != nullptr) {
            svc_log->error(mod_ctx, name);
            svc_log->error(mod_ctx,
                           "hook above did NOT install - world slow-motion is inactive this run");
        }
        return false;
    }
    return true;
}

}  // namespace

ModResult albw_sim_time_scale_hooks_init(ModError*) {
    const bool updateOk =
        report("SlowMoJ3DFrameCtrlUpdatePre",
               mods::hook::add_pre<J3DFrameCtrlUpdate>(svc_hook, on_j3d_frame_ctrl_update_pre));
    const bool framePreOk = report("SlowMoDaPyUpdateFramePre",
                                   mods::hook::add_pre<DaPyFrameCtrlUpdateFrame>(
                                       svc_hook, on_da_py_frame_ctrl_update_frame_pre));
    const bool framePostOk = report("SlowMoDaPyUpdateFramePost",
                                    mods::hook::add_post<DaPyFrameCtrlUpdateFrame>(
                                        svc_hook, on_da_py_frame_ctrl_update_frame_post));

    s_slowmoInstalled = updateOk && framePreOk && framePostOk;
    if (!s_slowmoInstalled) {
        albw::set_sim_time_scale(1.0f);
        if (updateOk) {
            // Take the detour back off the hottest function in the game rather
            // than leave a callback that can only do harm. The flag above
            // already refuses the work if the service cannot uninstall.
            mods::hook::uninstall<J3DFrameCtrlUpdate>(svc_hook);
        }
        if (svc_log != nullptr) {
            svc_log->error(mod_ctx, "world slow-motion DISABLED: the Link exemption and the world "
                                    "scale must install together or neither takes effect");
        }
    }
    return MOD_OK;
}

ModResult albw_sim_time_scale_hooks_shutdown(ModError*) {
    albw::set_sim_time_scale(1.0f);
    s_linkExemptDepth = 0;
    return MOD_OK;
}
