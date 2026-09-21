#pragma once

// Internal side of the dev.albt.albw.shield service (src/albt_shield_api.cpp).
// The shareable interface is src/albt_shield_api.h; this header is ALBT-only.

// True while a cooperating mod holds an input yield. The shield bash entry
// must not begin a bash while this is set.
bool dAlbtShieldApi_inputYielded();

// Decrements the yield countdown. Called once per frame from mod_update.
void dAlbtShieldApi_tick();
