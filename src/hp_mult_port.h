// Copied verbatim from fork include/d/d_albw_hp_mult.h — enemy HP scaling + Link
// damage-decrease API (enums/structs/decls). Implementation ported into
// region_port.cpp (build/port/hp_mult.inc). Do not hand-edit; re-copy from fork.
#pragma once

#if TARGET_PC

#include "f_op/f_op_actor.h"
#include "f_pc/f_pc_base.h"

enum dAlbwHP_Category {
    dAlbwHP_NORMAL   = 0,
    dAlbwHP_MID_BOSS = 1,
    dAlbwHP_BOSS     = 2,
    dAlbwHP_FINAL    = 3,
    dAlbwHP_EXCLUDED = 4,
};

dAlbwHP_Category dAlbwHP_getCategory(s16 profName);

int dAlbwHP_getTrueHpMult(s16 profName);

s16 dAlbwHP_scaleHpValue(s16 profName, s16 hp);

enum dAlbwHP_DarknutPhase {
    dAlbwHP_Darknut_NONE = 0,
    dAlbwHP_Darknut_ARMORED,
    dAlbwHP_Darknut_TRANSITION,
    dAlbwHP_Darknut_UNARMORED,
};

struct dAlbwHP_LockonDisplay {
    s16 current;
    s16 max;
    bool customMeter;
    dAlbwHP_DarknutPhase darknutPhase;
    s16 actorHealth;
    s16 actorHealthMax;
};

// (getLockonDisplayHp is intentionally NOT ported here — it pulls in Darknut /
//  Armogohma boss internals; the per-enemy bar reads field_0x560 directly.)

void dAlbwHP_tryApplyTrueMaxHp(fopAc_ac_c* actor);

void dAlbwHP_onActorDelete(fpc_ProcID procId);

int dAlbwHP_applyMult(s16 profName, int attackPower);

int dAlbwHP_getRawMult(s16 profName);

#endif  // TARGET_PC
