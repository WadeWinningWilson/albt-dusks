#pragma once

#include "config_vars.h"

#include "mods/api.h"

// Hook dComIfGs_getMaxLifeGauge so bought bonus hearts raise real max HP.
ModResult albw_mq_hearts_init(ModError* error);
// Ports the fork's dMeter2_c::moveLife MQ branches so bought hearts grow the
// vanilla heart meter (containers) and heal into the bonus capacity.
ModResult albw_mq_heart_meter_init(ModError* error);
// getMaxLifeGauge rounded up to whole containers * 5 (fork getDisplayMaxLifeInternal).
unsigned short albw_mq_display_max_internal();

bool albw_mq_is_enabled();
bool albw_mq_can_purchase_heart_shop();
bool albw_mq_try_purchase_heart_shop();
int albw_mq_heart_shop_price();
const char* albw_mq_heart_shop_name();
const char* albw_mq_heart_shop_desc();

int albw_mq_get_meter_shop_tiers();
int albw_mq_meter_bonus_units();
bool albw_mq_can_purchase_meter_shop();
bool albw_mq_try_purchase_meter_shop();
int albw_mq_meter_shop_price();
const char* albw_mq_meter_shop_name();
const char* albw_mq_meter_shop_desc();
