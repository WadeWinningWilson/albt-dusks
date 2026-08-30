#pragma once

#include "global.h"
#include <os.h>

bool albw_rental_postman_unlocked();
void albw_rental_on_eligible(u8 itemNo);
bool albw_rental_is_eligible(u8 itemNo);
bool albw_rental_is_shield_eligible(u8 itemNo);
void albw_rental_on_shield_eligible(u8 itemNo);
void albw_rental_strip_all_on_death();
