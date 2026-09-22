#include "sim_time_scale.h"

namespace albw {

// ============================================
// Storage for the hot-path global (see sim_time_scale.h). This replaces the
// former file-static s_simTimeScale; the clamp, the range and every existing
// caller behave exactly as before - only the linkage changed, so the hook in
// sim_time_scale_hooks.cpp can read it with a single load.
//
// Fork counterpart: src/dusk/sim_time_scale.cpp:1-22 (g_simTimeScale plus the
// mirrored extern "C" dusk_world_sim_time_scale).
// ============================================
float g_world_sim_time_scale = 1.0f;

namespace {
int   s_animBoostDepth = 0;
float s_animBoost = 1.0f;
}

void anim_boost_begin(float boost) {
    s_animBoost = boost;
    ++s_animBoostDepth;
}

void anim_boost_end() {
    if (s_animBoostDepth > 0) {
        --s_animBoostDepth;
    }
    if (s_animBoostDepth == 0) {
        s_animBoost = 1.0f;
    }
}

float anim_boost_current() {
    return s_animBoostDepth > 0 ? s_animBoost : 1.0f;
}

float get_sim_time_scale() {
    return g_world_sim_time_scale;
}

void set_sim_time_scale(float scale) {
    if (scale < 0.01f) {
        scale = 0.01f;
    }
    if (scale > 1.0f) {
        scale = 1.0f;
    }
    g_world_sim_time_scale = scale;
}

}  // namespace albw
