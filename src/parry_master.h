#pragma once

class daAlink_c;

bool dParryMaster_isEnabled();
void dParryMaster_resetSession();
void dParryMaster_clearQueue();
void dParryMaster_update();
int dParryMaster_getRecoverablePieces();

void dParryMaster_onFailedBlock(daAlink_c* link, int atp);
void dParryMaster_onPerfectParry();
void dParryMaster_onDealtDamage();
void dParryMaster_onHpLoss(int pieces);
void dParryMaster_onHeal(int pieces);
