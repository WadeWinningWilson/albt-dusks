#pragma once

// Shared meter state for lockout.cpp (same translation unit as meter.cpp exports).
bool albw_meter_is_enabled();
bool albw_meter_is_locked();
void albw_meter_drain_to_lockout();
void albw_meter_add_base_fraction(int numerator, int denominator);
// Fork dMeter2_addALBWFraction (d_meter2.cpp:741-745) — CURRENT-ceiling scaled,
// unlike add_base_fraction which scales off the base pool. Used by the magic
// pickups (fork item_func_S_MAGIC 1/5, item_func_L_MAGIC 1/3).
void albw_meter_add_fraction(int numerator, int denominator);
void albw_meter_sub_base_fraction(int numerator, int denominator);
void albw_meter_drain_amount(int amount);
void albw_meter_fill_on_death();
// Fork dMeter2_restoreALBWMeterToFull — fill pool, clear lockout, reset lockout perks.
void albw_meter_restore_to_full();
int albw_meter_get_value();
int albw_meter_get_max();
// Deku Leaf glide (fork dMeter2_*ALBWDekuLeaf + the bomb-drop's can/onALBWBomb).
bool albw_meter_can_deku_leaf();
void albw_meter_on_deku_leaf();
void albw_meter_on_deku_leaf_start();
bool albw_meter_can_bomb();
void albw_meter_on_bomb();
