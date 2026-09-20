// === AUTO-EXTRACTED by port_tool.py — do not hand-edit the bodies ===
// diag defines forced 0 so scaffolding compiles out:

// ---- pulled fork-local dependencies (verbatim) ----
static const int kAlbwHowlAoeHitPower  = 100;

static const int kAlbwMidnaArmHitPower = 100;

u16 albw_wolf_cc_pl_cut_bit_get() {
    daPy_py_c* player_p = (daPy_py_c*)dComIfGp_getPlayer(0);

    u16 bit = 0;
    if (player_p->getCutType() == daPy_py_c::CUT_TYPE_NM_VERTICAL) {
        bit = 0x1;
    } else if (player_p->getCutType() == daPy_py_c::CUT_TYPE_NM_STAB) {
        bit = 0x2;
    } else if (player_p->getCutType() == daPy_py_c::CUT_TYPE_COMBO_STAB) {
        bit = 0x200;
    } else if (player_p->getCutType() == daPy_py_c::CUT_TYPE_NM_RIGHT) {
        bit = 0x4;
    } else if (player_p->getCutType() == daPy_py_c::CUT_TYPE_NM_LEFT) {
        bit = 0x8;
    } else if (player_p->getCutType() == daPy_py_c::CUT_TYPE_FINISH_LEFT) {
        bit = 0x20;
    } else if (player_p->getCutType() == daPy_py_c::CUT_TYPE_FINISH_VERTICAL) {
        bit = 0x40;
    } else if (player_p->getCutType() == daPy_py_c::CUT_TYPE_TURN_RIGHT ||
               player_p->getCutType() == daPy_py_c::CUT_TYPE_UNK_9 || player_p->getCutType() == daPy_py_c::CUT_TYPE_HORSE_TURN ||
               player_p->getCutType() == daPy_py_c::CUT_TYPE_LARGE_TURN_RIGHT)
    {
        bit = 0x80;
    } else if (player_p->getCutType() == daPy_py_c::CUT_TYPE_TURN_LEFT ||
               player_p->getCutType() == daPy_py_c::CUT_TYPE_LARGE_TURN_LEFT)
    {
        bit = 0x800;
    } else if (player_p->getCutType() == daPy_py_c::CUT_TYPE_JUMP) {
        bit = 0x100;
    } else if (player_p->getCutType() == daPy_py_c::CUT_TYPE_UNK_28 || player_p->getCutType() == daPy_py_c::CUT_TYPE_GUARD_ATTACK) {
        bit = 0x400;
    }

    return bit;
}

static int getMapInfo(s8 param_0) {
    int map_info = 30;
    if (param_0 == 1) {
        map_info = 31;
    } else if (param_0 == 2) {
        map_info = 32;
    }

    return map_info;
}

static u32 getHitId(cCcD_Obj* i_ccObj, int i_useReboundSe) {
    dCcD_GObjInf* dObj = static_cast<dCcD_GObjInf*>(i_ccObj);
    return dObj->getHitSeID(dObj->GetAtSe(), i_useReboundSe);
}

static u8 albw_wolf_at_power_get(dCcU_AtInfo* i_AtInfo) {
    u8 power = i_AtInfo->mpCollider->GetAtAtp();

    if (i_AtInfo->mPowerType == 0) {
        if (i_AtInfo->mpCollider->ChkAtType(AT_TYPE_10000000)) {
            power = 0;
        } else if (power == 1) {
            power = 1;
        } else if (power == 2) {
            power = 10;
        } else if (power == 3) {
            power *= 10;
        } else if (power == 6) {
            power = 80;
        } else if (power >= 4) {
            power = 200;
        }
    } else if (i_AtInfo->mPowerType == 6) {
        if (i_AtInfo->mpCollider->ChkAtType(AT_TYPE_10000000)) {
            power = 0;
        } else if (power == 1) {
            power = 1;
        } else if (power == 2) {
            power = 10;
        } else if (power == 6) {
            power = 80;
        } else if (power >= 3) {
            power *= 10;
        }
    } else if (i_AtInfo->mPowerType == 4) {
        if (i_AtInfo->mpCollider->ChkAtType(AT_TYPE_IRON_BALL)) {
            power = 200;
        } else if (i_AtInfo->mpCollider->ChkAtType(AT_TYPE_10000000)) {
            power = 0;
        } else if (power == 1) {
            power = 1;
        } else if (power == 2) {
            power = 10;
        } else if (power >= 3) {
            power *= 10;
        } else if (power == 6) {
            power = 80;
        } else if (power >= 4) {
            power = 200;
        }
    } else if (i_AtInfo->mPowerType == 5) {
        if (i_AtInfo->mpCollider->ChkAtType(AT_TYPE_ARROW)) {
            power = 0;
        } else if (power == 1) {
            power = 1;
        } else if (power == 2) {
            power = 3;
        } else if (power >= 3) {
            power = 4;
        }
    } else if (i_AtInfo->mPowerType == 2) {
        if (i_AtInfo->mpCollider->ChkAtType(AT_TYPE_IRON_BALL)) {
            power = 200;
        } else if (power == 1) {
            power = 1;
        } else if (power == 2) {
            power = 10;
        } else if (power == 3) {
            power = 20;
        } else if (power == 6) {
            power = 80;
        } else if (power >= 4) {
            power = 200;
        }
    } else if (i_AtInfo->mPowerType == 1) {
        if (i_AtInfo->mpCollider->ChkAtType(AT_TYPE_IRON_BALL) || power >= 4) {
            power = 200;
        } else {
            power *= 10;
        }
    } else if (i_AtInfo->mPowerType == 3) {
        if (i_AtInfo->mpCollider->ChkAtType(AT_TYPE_IRON_BALL) || power >= 4) {
            power = 200;
        } else if (power == 1) {
            power = 10;
        } else if (power == 2) {
            power = 20;
        } else if (power >= 3) {
            power = 40;
        }
    } else if (i_AtInfo->mPowerType == 7) {
        if (i_AtInfo->mpCollider->ChkAtType(AT_TYPE_THROW_OBJ | AT_TYPE_20000 | AT_TYPE_IRON_BALL)) {
            power = 50;
        } else if (i_AtInfo->mpCollider->ChkAtType(AT_TYPE_ARROW)) {
            power = 27;
        } else if (power == 1) {
            power = 1;
        } else if (power == 2) {
            power = 10;
        } else if (power == 3) {
            power *= 10;
        } else if (power == 6) {
            power = 80;
        } else if (power >= 4) {
            power = 200;
        }
    } else if (i_AtInfo->mPowerType == 8) {
        if (power == 1) {
            power = 3;
        } else if (power == 2) {
            power = 10;
        } else if (power == 6) {
            power = 80;
        } else if (power >= 3) {
            power *= 10;
        }
    } else if (i_AtInfo->mPowerType == 9 || i_AtInfo->mPowerType == 10) {
        if (i_AtInfo->mpCollider->ChkAtType(AT_TYPE_HOOKSHOT)) {
            power = 0;
        }
        if (i_AtInfo->mpCollider->ChkAtType(AT_TYPE_IRON_BALL)) {
            power = 19;
        }

        if (power == 1) {
            power = 4;
        } else if (power == 2) {
            power = 8;
        } else if (power == 3) {
            power *= 10;
        } else if (power == 6) {
            power = 80;
        } else if (power >= 4) {
            power = 200;
        }
    } else if (i_AtInfo->mPowerType == 11) {
        if (i_AtInfo->mpCollider->ChkAtType(AT_TYPE_HOOKSHOT)) {
            power = 0;
        }

        if (power == 1) {
            power = 10;
        } else if (power == 2) {
            power = 20;
        } else if (power == 3) {
            power = 30;
        } else if (power >= 4) {
            power = 100;
        }
    } else if (i_AtInfo->mPowerType == 12) {
        if (power == 1) {
            power = 10;
        } else if (power == 2) {
            power = 20;
        } else if (power == 3) {
            power = 30;
        } else if (power >= 4) {
            power = 100;
        }
    }

    return power;
}

fopAc_ac_c* albw_wolf_at_power_check(dCcU_AtInfo* i_AtInfo) {
    if (i_AtInfo->mpCollider == NULL) {
        return NULL;
    }

    i_AtInfo->mpActor = i_AtInfo->mpCollider->GetAc();
    i_AtInfo->mHitType = 12;
    i_AtInfo->mAttackPower = 0;
    i_AtInfo->mHitBit = 0;

    if (i_AtInfo->mpActor != NULL) {
        i_AtInfo->mAttackPower = albw_wolf_at_power_get(i_AtInfo);

        s16 ac_name = fopAcM_GetName(i_AtInfo->mpActor);
        if (ac_name == fpcNm_ALINK_e || ac_name == fpcNm_ALINK_e) {
            if (i_AtInfo->mpCollider->ChkAtType(AT_TYPE_8000) ||
                i_AtInfo->mpCollider->ChkAtType(AT_TYPE_IRON_BALL))
            {
                i_AtInfo->mHitType = HIT_TYPE_LINK_HEAVY_ATTACK;
            } else {
                i_AtInfo->mHitType = HIT_TYPE_LINK_NORMAL_ATTACK;
                i_AtInfo->mHitBit = albw_wolf_cc_pl_cut_bit_get();
            }
        } else if (ac_name == fpcNm_NBOMB_e) {
            i_AtInfo->mHitType = HIT_TYPE_BOMB;
            i_AtInfo->mHitBit = 0x10000000;
        } else if (ac_name == fpcNm_BOOMERANG_e) {
            i_AtInfo->mHitType = HIT_TYPE_BOOMERANG;
            i_AtInfo->mHitBit = 0x40000000;
        } else if (ac_name == fpcNm_ARROW_e) {
            i_AtInfo->mHitType = HIT_TYPE_ARROW;
            i_AtInfo->mHitBit = 0x80000000;
        }
    }

    i_AtInfo->mHitStatus = 0;
    if (i_AtInfo->mpCollider->ChkAtType(AT_TYPE_40) ||
        i_AtInfo->mpCollider->ChkAtType(AT_TYPE_BOOMERANG) ||
        i_AtInfo->mpCollider->ChkAtType(AT_TYPE_SLINGSHOT) ||
        i_AtInfo->mpCollider->ChkAtType(AT_TYPE_SHIELD_ATTACK))
    {
        i_AtInfo->mHitType = HIT_TYPE_STUN;
    } else if (i_AtInfo->mpCollider->ChkAtType(AT_TYPE_HOOKSHOT)) {
        i_AtInfo->mHitBit = 0x1000;
    } else {
        s16 ac_name = fopAcM_GetName(i_AtInfo->mpActor);
        if (ac_name == fpcNm_ALINK_e || ac_name == fpcNm_ALINK_e) {
            if (dCcD_GetGObjInf(i_AtInfo->mpCollider)->GetAtSpl() == 1) {
                i_AtInfo->mHitStatus = 1;
            }
        } else if (i_AtInfo->mAttackPower >= 3) {
            i_AtInfo->mHitStatus = 1;
        }
    }

    return i_AtInfo->mpActor;
}

// ---- NEW helpers (verbatim) ----
// ---- WHOLE fork replacements (verbatim, transformed): hook the stock
//      symbol with HOOK_SKIP_ORIGINAL and call these ----
// [WHOLE replacement] cc_at_check
fopAc_ac_c* albw_wolf_cc_at_check(fopAc_ac_c* i_enemy, dCcU_AtInfo* i_AtInfo) {
    daPy_py_c* player_p = (daPy_py_c*)dComIfGp_getPlayer(0);

    // [PORT-EXCLUDED] lockout slingshot tag: owned by lockout.cpp's cc_at_check
    // PRE hook (albw_lockout_init registers before albw_wolf_combat_init, so it
    // has already run before this replacement body executes).

    i_AtInfo->mpActor = albw_wolf_at_power_check(i_AtInfo);

    f32 x_diff;
    f32 z_diff;
    if (i_AtInfo->mpActor != NULL) {
        cXyz tmp = i_AtInfo->mpActor->speed;
        tmp.y = 0.0f;
        if (tmp.abs() > 100.0f) {
            f32 x = i_AtInfo->mpActor->speed.x;
            f32 z = i_AtInfo->mpActor->speed.z;
            i_AtInfo->mHitDirection.y = cM_atan2s(-x, -z) + (s16)cM_rndFX(4000.0f);
        } else {
            if (fopAcM_GetName(i_AtInfo->mpActor) == fpcNm_BOOMERANG_e) {
                x_diff = i_enemy->current.pos.x - player_p->current.pos.x;
                z_diff = i_enemy->current.pos.z - player_p->current.pos.z;
                i_AtInfo->mHitDirection.y = cM_atan2s(-x_diff, -z_diff) + (s16)cM_rndFX(10000.0f);
            } else {
                x_diff = i_enemy->current.pos.x - i_AtInfo->mpActor->current.pos.x;
                z_diff = i_enemy->current.pos.z - i_AtInfo->mpActor->current.pos.z;
                i_AtInfo->mHitDirection.y = cM_atan2s(-x_diff, -z_diff);
            }
        }

        if (i_AtInfo->mHitType == HIT_TYPE_LINK_NORMAL_ATTACK &&
            player_p->getCutType() == daPy_py_c::CUT_TYPE_HEAD_JUMP)
        {
            i_AtInfo->mHitDirection.y = player_p->shape_angle.y;
        }

        if (i_AtInfo->mpCollider->ChkAtType(AT_TYPE_HOOKSHOT) &&
            fopAcM_CheckStatus(i_enemy, 0x380000))
        {
            i_AtInfo->mAttackPower = 0;
        }

        if (static_cast<dCcD_GObjInf*>(i_AtInfo->mpCollider)->GetAtMtrl() == dCcD_MTRL_LIGHT) {
            if (fopAcM_GetName(i_enemy) == fpcNm_B_GND_e) {
                i_AtInfo->mAttackPower = 0;
            } else if (fopAcM_GetName(i_enemy) != fpcNm_B_ZANT_e) {
                i_AtInfo->mAttackPower = 100;
            }
        }

        if (i_AtInfo->mHitType == HIT_TYPE_LINK_NORMAL_ATTACK) {
            if (!daPy_py_c::checkNowWolf()) {
                if (player_p->checkMasterSwordEquip()) {
                    i_AtInfo->mAttackPower *= 2;
                }

                if (daPy_py_c::checkWoodSwordEquip()) {
                    i_AtInfo->mAttackPower /= 2;
                }
            }

            if (player_p->getSwordAtUpTime()) {
                i_AtInfo->mAttackPower *= 2;
                i_AtInfo->mHitStatus = 1;
            }

            // [PORT-EXCLUDED] FA hurricane/melee resolve: owned by focused_arts.cpp
            // cc_at_check POST hook (fires after a skipped original too).

            // [PORT-EXCLUDED] Ending Blow -> Great Spin AOE: owned by focused_arts.cpp
            // cc_at_check POST hook.

            // [PORT-EXCLUDED] outfit outgoing damage mult: owned by focused_arts.cpp
            // cc_at_check POST hook.
            ;
        }

#if TARGET_PC
        // ============================================
        // NEW CODE — ALBW Port
        // D-pad wolf-art damage overrides. Pinned AFTER the vanilla
        // modifiers (so the 200-tier table value is replaced outright)
        // but BEFORE the global sliders below (invincibleEnemies /
        // dAlbwHP_applyMult / bash +5%) so those still stack. Running
        // inside albw_wolf_cc_at_check also covers frozen-enemy bridge hits.
        //  - Combat Howl AOE: ALINK-owned WOLF_CUT_TURN collider while
        //    mWolfCombatHowlActive — same disambiguation as the
        //    guard-opener classifier (Link cannot wolf-spin mid-howl).
        //  - Midna arm: its own actor, so its name is sufficient.
        // ============================================
        if (dAlbwWolfCombat_isEnabled() && i_AtInfo->mAttackPower > 0 &&
            fopAcM_GetGroup(i_enemy) == fopAc_ENEMY_e) {
            if (i_AtInfo->mpActor != NULL &&
                fopAcM_GetName(i_AtInfo->mpActor) == 0x031A /* fpcNm_ALBW_MIDNA_ARM_e (fork f_pc_name.h; stock table has no slot) */) {
                i_AtInfo->mAttackPower = kAlbwMidnaArmHitPower;
            } else if (i_AtInfo->mHitType == HIT_TYPE_LINK_NORMAL_ATTACK &&
                       i_AtInfo->mpCollider->ChkAtType(AT_TYPE_WOLF_CUT_TURN)) {
                // [PORT-TRANSLATED] fork daAlink_c::mWolfCombatHowlActive (fork-only
                // field) -> the DUSK's own howl-active accessor.
                if (albw_wolf_combat_howl_active()) {
                    i_AtInfo->mAttackPower = kAlbwHowlAoeHitPower;
                }
            }
        }
        // ============================================
        // NEW CODE ENDS HERE
        // ============================================
        // [PORT-DEVIATION] stock/fork invincibleEnemies zero-out: dusk::getSettings()
        // is not exported to mods (no host-settings service). While this replacement
        // body is active (wolf combat ON + wolf form) the host cheat toggle cannot
        // zero damage here; every other path still runs the stock original.
        // ============================================
        // NEW CODE — ALBW Port
        // Link damage decrease intercept. Runs after all attack-power
        // modifiers and after the invincibleEnemies zero-out above.
        // Divides mAttackPower by linkDamageDecreaseMult (global, default 1×).
        // Independent of per-category true max-HP sliders.
        // ============================================
        if (i_AtInfo->mAttackPower > 0 && fopAcM_GetGroup(i_enemy) == fopAc_ENEMY_e) {
            i_AtInfo->mAttackPower =
                dAlbwHP_applyMult(fopAcM_GetName(i_enemy), i_AtInfo->mAttackPower);
        }
        // [PORT-EXCLUDED] lockout / focused-arts item damage boosts: owned by
        // lockout.cpp and focused_arts.cpp cc_at_check POST hooks.
        // Wolf field attack damage modifier.
        // Runs after all other attack-power adjustments (including Link
        // damage decrease above) so the split is applied to the
        // fully-resolved value.
        //
        // Link damage decrease already divided mAttackPower by rawMult.
        // To express charge damage as  base × rawMult × fraction  we
        // multiply by rawMult² here, which cancels the division and then
        // scales forward:  (base / rawMult) × rawMult² × fraction
        //                = base × rawMult × fraction.
        //
        // Twilight enemies (shadow/dusk forms):  70 % of scaled damage,
        //   no stun; these enemies feel weaker against wolf form.
        // Non-twilight enemies:                  25 % of scaled damage,
        //   compensated by the 300-frame stun dispatched after deduction.
        //
        // Zant excluded — his fight's phase scripts check AT_TYPE_WOLF_*
        // for hit-timing hooks and must not have damage or stun modified.
        // ============================================
        if (dAlbwWolfCombat_isEnabled() &&
            i_AtInfo->mAttackPower > 0 &&
            i_AtInfo->mpCollider->ChkAtType(AT_TYPE_MIDNA_LOCK) &&
            daPy_py_c::checkNowWolf() &&
            fopAcM_GetGroup(i_enemy) == fopAc_ENEMY_e &&
            fopAcM_GetName(i_enemy) != fpcNm_B_ZANT_e)
        {
            // [WOLFHIT] probe (STRIP before release): power entering the fork split.
            ALBW_WOLFHIT_LOG("[WOLFHIT] split-in name=%04x pow=%d rawMult=%d twilight=%d",
                             (int)fopAcM_GetName(i_enemy), (int)i_AtInfo->mAttackPower,
                             dAlbwHP_getRawMult(fopAcM_GetName(i_enemy)),
                             (int)dAlbwWolfStun_isTwilightEnemy(fopAcM_GetName(i_enemy)));
            const int rawMult = dAlbwHP_getRawMult(fopAcM_GetName(i_enemy));
            if (dAlbwWolfStun_isTwilightEnemy(fopAcM_GetName(i_enemy))) {
                // Twilight: base × rawMult × 0.70, no stun
                i_AtInfo->mAttackPower =
                    (i_AtInfo->mAttackPower * rawMult * rawMult * 7) / 10;
                if (i_AtInfo->mAttackPower < 1) {
                    i_AtInfo->mAttackPower = 1;
                }
            } else {
                // Non-twilight: base × rawMult × 0.25, stun applied below
                i_AtInfo->mAttackPower =
                    (i_AtInfo->mAttackPower * rawMult * rawMult * 25) / 100;
                if (i_AtInfo->mAttackPower < 1) {
                    i_AtInfo->mAttackPower = 1;
                }
            }
        }
        // Boss Refinement: Armogohma core arrow hits chip 4% in dAlbwBoss_armogohmaOnBowCoreHit().
        if (dAlbwBoss_armogohmaShouldSuppressVanillaArrowDamage(i_enemy, i_AtInfo)) {
            i_AtInfo->mAttackPower = 0;
        }
        // ============================================
        // NEW CODE ENDS HERE
        // ============================================
#endif

#if TARGET_PC
        // Successful shield bash arms +5% on the next damaging hit (any source).
        if (i_AtInfo->mAttackPower > 0 && fopAcM_GetGroup(i_enemy) == fopAc_ENEMY_e) {
            dShield_tryConsumeBashNextHitBoost(i_AtInfo->mAttackPower);
        }

        // [PORT-EXCLUDED] lockout double-claw finisher pin: owned by lockout.cpp's
        // cc_at_check POST hook.
        {
        }
#endif

        if (i_AtInfo->mAttackPower != 0) {
            i_enemy->health -= i_AtInfo->mAttackPower;
#if TARGET_PC
            // [PORT-EXCLUDED] parry-master onDealtDamage: owned by parry_hooks.cpp's
            // cc_at_check POST hook (reads the retval our pre hook fills in).
            // [PORT-EXCLUDED] enemy-rupee kill grant + magic-jar drop + confuse
            // friendly-fire + FA item fill: owned by enemy_rupees.cpp, lockout.cpp and
            // focused_arts.cpp cc_at_check POST hooks.
#endif
        }

        s8 pause_time = 0;
        if (i_AtInfo->mAttackPower != 0 && i_enemy->health <= 0) {
            i_AtInfo->mHitStatus = 2;
            i_enemy->health = 0;
#if TARGET_PC
            // [PORT-DEVIATION] host achievements (enemy_killed / rollstab_kill):
            // dusk::AchievementSystem is not exported to mods, so kills that resolve
            // through this replacement body (wolf form + wolf combat ON) do not signal
            // achievements. All other kills run the stock original and still do.
            // [PORT-EXCLUDED] fork magic-jar bonus drop: owned by enemy_rupees.cpp's
            // cc_at_check POST hook (tryDropMagicJar).
#endif
        }

#if TARGET_PC
        // ============================================
        // NEW CODE — ALBW Port
        // Wolf bite charge accumulation and field attack stun dispatch.
        //
        // Bite tracking: 5 normal wolf bites (AT not MIDNA_LOCK) grant
        // +1 charge (cap 2) and heal 1/4 heart when Link is at or below
        // 50 % max HP.  Bites that deal 0 damage are ignored.
        //
        // Field attack stun: non-twilight enemies that survived the hit
        // (health > 0 after deduction) are frozen via fpcM_PauseEnable
        // for WOLF_STUN_FRAMES (300 frames).  Twilight enemies and Zant
        // are excluded.
        // ============================================
        if (dAlbwWolfCombat_isEnabled() &&
            fopAcM_GetGroup(i_enemy) == fopAc_ENEMY_e &&
            fopAcM_GetName(i_enemy) != fpcNm_B_ZANT_e)
        {
            auto* link = static_cast<daAlink_c*>(daPy_getPlayerActorClass());
            if (link != NULL && daPy_py_c::checkNowWolf()) {

                // --- Wolf normal bite: charge accumulation ---
                // Special D-pad ARTS never build charges: the combat-howl AOE rides Link's own
                // collider (AT_TYPE_WOLF_CUT_TURN, owner = ALINK) so it would otherwise pass this
                // guard — exclude it via mWolfCombatHowlActive (Link can't bite mid-howl, so the
                // flag exactly identifies howl-AOE hits; the vanilla roll attack, same AT type,
                // still counts).  The Midna-arm art is a separate non-ALINK actor and is excluded
                // automatically (mHitType stays generic).
                if (i_AtInfo->mHitType == HIT_TYPE_LINK_NORMAL_ATTACK &&
                    !i_AtInfo->mpCollider->ChkAtType(AT_TYPE_MIDNA_LOCK) &&
                    !albw_wolf_combat_howl_active() &&  // [PORT-TRANSLATED] fork mWolfCombatHowlActive
                    i_AtInfo->mAttackPower > 0)
                {
                    // Tally lives in dAlbwWolfCombat_onBiteConnect (alpha cleanup:
                    // extracted so hang-bite/chest-mash paths that bypass
                    // albw_wolf_cc_at_check can award the same economy).
                    dAlbwWolfCombat_onBiteConnect();
                }

                // --- Wolf field attack: stun non-twilight survivors ---
                if (i_AtInfo->mpCollider->ChkAtType(AT_TYPE_MIDNA_LOCK) &&
                    i_AtInfo->mAttackPower > 0)
                {
                    if (dFocusedArts_isMdForcedWolfActive()) {
                        dComIfGp_setItemLifeCount(4.0f, 0);
                    }
                    if (i_enemy->health > 0 &&
                        !dAlbwWolfStun_isTwilightEnemy(fopAcM_GetName(i_enemy)))
                    {
                        dAlbwWolfStun_apply(i_enemy);
                    }
                }
            }
        }

        // ============================================
        // NEW CODE ENDS HERE
        // ============================================
#endif

        int uvar8;
        if (i_AtInfo->mpCollider->ChkAtType(AT_TYPE_HOOKSHOT) &&
            !fopAcM_CheckStatus(i_enemy, 0x280000))
        {
            uvar8 = 1;
        } else {
            uvar8 = 0;
        }

        if (i_AtInfo->mpSound != NULL) {
            if (i_AtInfo->field_0x18 != 0) {
                i_AtInfo->mpSound->startCollisionSE(getHitId(i_AtInfo->mpCollider, uvar8),
                                                    i_AtInfo->field_0x18);
            } else {
                i_AtInfo->mpSound->startCollisionSE(getHitId(i_AtInfo->mpCollider, uvar8),
                                                    getMapInfo(i_AtInfo->mHitStatus));
            }
        }

        if (i_AtInfo->mHitStatus != 0) {
            pause_time = 5;
        } else {
            if (i_AtInfo->mAttackPower > 1) {
                pause_time = 2;
            }
        }

        if (i_AtInfo->mpCollider->ChkAtType(AT_TYPE_MIDNA_LOCK) ||
            ((daPy_py_c*)dComIfGp_getPlayer(0))->checkHorseRide())
        {
            // actor is Bulblin or Horseback Ganon
            if ((fopAcM_GetName(i_enemy) == fpcNm_E_RD_e && ((s8*)i_enemy)[0x129a] != 0) ||
                fopAcM_GetName(i_enemy) == fpcNm_B_GND_e)
            {
                pause_time = 3;
            } else {
                pause_time = 0;
            }
        }

        s16 ac_name = fopAcM_GetName(i_enemy);
        // actor is Stalkin, Chu, Keese, Shadow Keese, Shadow Vermin, Baby Gohma, or Rat
        if (ac_name == fpcNm_E_BS_e || ac_name == fpcNm_E_SM2_e || ac_name == fpcNm_E_BA_e ||
            ac_name == fpcNm_E_YK_e || ac_name == fpcNm_E_YG_e || ac_name == fpcNm_E_GM_e ||
            ac_name == fpcNm_E_MS_e)
        {
            pause_time = 0;
        }

        if ((i_AtInfo->mHitType == HIT_TYPE_LINK_NORMAL_ATTACK ||
             i_AtInfo->mpCollider->ChkAtType(AT_TYPE_THROW_OBJ)) &&
            !player_p->checkCutJumpCancelTurn())
        {
            if (i_AtInfo->mpCollider->ChkAtType(AT_TYPE_THROW_OBJ)) {
                pause_time = 4;
            }
            dScnPly_c::setPauseTimer(pause_time);
        }
    }

    return i_AtInfo->mpActor;
}

// ---- MODIFIED stock fns: ADDED hunks only (place at hook seams) ----
