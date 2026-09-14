// === AUTO-EXTRACTED by port_tool.py — do not hand-edit the bodies ===
// diag defines forced 0 so scaffolding compiles out:

// ---- pulled fork-local dependencies (verbatim) ----
static std::unordered_set<fpc_ProcID> s_trueHpApplied;

static bool inList(const s16* list, std::size_t count, s16 name) {
    for (std::size_t i = 0; i < count; ++i) {
        if (list[i] == name) {
            return true;
        }
    }
    return false;
}

static void markTrueHpProcessed(fpc_ProcID procId) {
    s_trueHpApplied.insert(procId);
}

static bool isTrueHpProcessed(fpc_ProcID procId) {
    return s_trueHpApplied.find(procId) != s_trueHpApplied.end();
}

static s16 scaleHpField(s16 value, int mult) {
    if (value <= 1 || mult <= 1) {
        return value;
    }
    const int scaled = static_cast<int>(value) * mult;
    return static_cast<s16>(std::min(scaled, 32767));
}

static int getCategoryMult(dAlbwHP_Category cat) {
    if (cat == dAlbwHP_EXCLUDED) {
        return 1;
    }

    
    switch (cat) {
    case dAlbwHP_NORMAL:
        return albw_cfg_int(g_hp_normal, 1);
    case dAlbwHP_MID_BOSS:
        return albw_cfg_int(g_hp_midboss, 1);
    case dAlbwHP_BOSS:
        return albw_cfg_int(g_hp_boss, 1);
    case dAlbwHP_FINAL:
        return albw_cfg_int(g_hp_final, 1);
    default:
        return 1;
    }
}

static int getLinkDamageDecreaseMult() {
    const int mult = albw_cfg_int(g_link_damage_decrease, 1);
    return mult > 1 ? mult : 1;
}

// ---- NEW helpers (verbatim) ----
// [NEW helper] dAlbwHP_getCategory
dAlbwHP_Category dAlbwHP_getCategory(s16 profName) {
    if (IN(sFinalBoss, profName)) {
        return dAlbwHP_FINAL;
    }
    if (IN(sBoss, profName)) {
        return dAlbwHP_BOSS;
    }
    if (IN(sMidBoss, profName)) {
        return dAlbwHP_MID_BOSS;
    }
    return dAlbwHP_NORMAL;
}

// [NEW helper] dAlbwHP_getTrueHpMult
int dAlbwHP_getTrueHpMult(s16 profName) {
    const int mult = getCategoryMult(dAlbwHP_getCategory(profName));
    return mult > 1 ? mult : 1;
}

// [NEW helper] dAlbwHP_scaleHpValue
s16 dAlbwHP_scaleHpValue(s16 profName, s16 hp) {
    return dAlbwRegionMult_scaleHp(scaleHpField(hp, dAlbwHP_getTrueHpMult(profName)));
}

// [NEW helper] dAlbwHP_tryApplyTrueMaxHp
void dAlbwHP_tryApplyTrueMaxHp(fopAc_ac_c* actor) {
    if (actor == NULL || !fopAcM_IsActor(actor)) {
        return;
    }

    const fpc_ProcID procId = fpcM_GetID(actor);
    if (procId == fpcM_ERROR_PROCESS_ID_e || !fpcM_IsExecuting(procId)) {
        return;
    }
    if (isTrueHpProcessed(procId)) {
        return;
    }

    if (fopAcM_GetGroup(actor) != fopAc_ENEMY_e) {
        markTrueHpProcessed(procId);
        return;
    }

    const s16 profName = fopAcM_GetName(actor);
    if (dAlbwHP_getCategory(profName) == dAlbwHP_EXCLUDED) {
        markTrueHpProcessed(procId);
        return;
    }

    if (actor->health <= 0) {
        return;
    }

    if (actor->health <= 1) {
        markTrueHpProcessed(procId);
        return;
    }

    const int mult = dAlbwHP_getTrueHpMult(profName);
    if (mult > 1) {
        actor->health = scaleHpField(actor->health, mult);
        if (actor->field_0x560 > 1) {
            actor->field_0x560 = scaleHpField(actor->field_0x560, mult);
        } else if (actor->field_0x560 > 0) {
            actor->field_0x560 = actor->health;
        }
    }

    actor->health = dAlbwRegionMult_scaleHp(actor->health);
    if (actor->field_0x560 > 1) {
        actor->field_0x560 = dAlbwRegionMult_scaleHp(actor->field_0x560);
    } else if (actor->field_0x560 > 0) {
        actor->field_0x560 = actor->health;
    }

    markTrueHpProcessed(procId);
}

// [NEW helper] dAlbwHP_onActorDelete
void dAlbwHP_onActorDelete(fpc_ProcID procId) {
    s_trueHpApplied.erase(procId);
}

// [NEW helper] dAlbwHP_applyMult
int dAlbwHP_applyMult(s16 profName, int attackPower) {
    (void)profName;

    if (attackPower <= 0) {
        return attackPower;
    }

    const int mult = getLinkDamageDecreaseMult();
    if (mult <= 1) {
        return attackPower;
    }
    return std::max(1, attackPower / mult);
}

// [NEW helper] dAlbwHP_getRawMult
int dAlbwHP_getRawMult(s16 profName) {
    (void)profName;
    return getLinkDamageDecreaseMult();
}

// ---- MODIFIED stock fns: ADDED hunks only (place at hook seams) ----