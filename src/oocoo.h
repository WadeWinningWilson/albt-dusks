#pragma once

void albw_oocoo_on_death_context(const char* lastStageName, bool diedInDungeon);
void albw_oocoo_on_warp_choice(int choice);
bool albw_oocoo_can_show_in_shop();
bool albw_oocoo_try_purchase();
void albw_oocoo_execute_pending_warp();
const char* albw_oocoo_service_name();
const char* albw_oocoo_service_desc();
int albw_oocoo_service_price();
