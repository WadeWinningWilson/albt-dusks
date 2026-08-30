#pragma once

#include "config_vars.h"

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
