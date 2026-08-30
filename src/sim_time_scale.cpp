#include "sim_time_scale.h"

namespace albw {

static float s_simTimeScale = 1.0f;

float get_sim_time_scale() {
    return s_simTimeScale;
}

void set_sim_time_scale(float scale) {
    if (scale < 0.01f) {
        scale = 0.01f;
    }
    if (scale > 1.0f) {
        scale = 1.0f;
    }
    s_simTimeScale = scale;
}

}  // namespace albw
