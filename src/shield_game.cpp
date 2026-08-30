#include "global.h"

#include "d/d_meter2.h"
#include "shield_game.h"

namespace albw_shield_game {

namespace {

dMeter2_c* g_meter = nullptr;

}  // namespace

void set_meter_class(dMeter2_c* meter) {
    g_meter = meter;
}

dMeter2_c* get_meter_class() {
    return g_meter;
}

}  // namespace albw_shield_game

// Header-instantiated daPy helpers in mod TUs reference this export; provide a local definition.
u8 dComIfGs_getSelectEquipShield() {
    return albw_shield_game::get_select_equip_shield();
}
