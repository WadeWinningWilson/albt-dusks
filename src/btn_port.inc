// === AUTO-EXTRACTED by port_tool.py — do not hand-edit the bodies ===
// diag defines forced 0 so scaffolding compiles out:

// ---- pulled fork-local dependencies (verbatim) ----
static daB_TN_HIO_c l_HIO;

daB_TN_HIO_c::daB_TN_HIO_c() {
    mUnk1 = -1;
    mScale = 1.3f;
    mKColorA = 40.0f;
    mTimer3NormalType0 = 150.0f;
    mTimer3NormalType1 = 250.0f;
    mTimer3HumanType0 = 210.0f;
    mTimer3HumanType1 = 350.0f;
    mTimer3Wolf = 120.0f;
    field_0x24 = 360.0f;
    mTimer1Action1 = 0.0f;
    mTimer1Action2 = 0.0f;
}

// ---- NEW helpers (verbatim) ----
// [NEW helper] albwDarknutAction1Name
static const char* albwDarknutAction1Name(int i_mode) {
    switch (i_mode) {
    case AlbwBtn_c::ACT_ROOMDEMO:
        return "ROOMDEMO";
    case AlbwBtn_c::ACT_OPENING:
        return "OPENING";
    case AlbwBtn_c::ACT_WAITH:
        return "WAITH";
    case AlbwBtn_c::ACT_CHASEH:
        return "CHASEH";
    case AlbwBtn_c::ACT_ATTACKH:
        return "ATTACKH";
    case AlbwBtn_c::ACT_ATTACKSHIELDH:
        return "ATKSHIELDH";
    case AlbwBtn_c::ACT_GUARDH:
        return "GUARDH";
    case AlbwBtn_c::ACT_DAMAGEH:
        return "DAMAGEH";
    case AlbwBtn_c::ACT_CHANGEDEMO:
        return "CHANGEDEMO";
    case AlbwBtn_c::ACT_CHASEL:
        return "CHASEL";
    case AlbwBtn_c::ACT_ATTACKL:
        return "ATTACKL";
    case AlbwBtn_c::ACT_ATTACKSHIELDL:
        return "ATKSHIELDL";
    case AlbwBtn_c::ACT_GUARDL:
        return "GUARDL";
    case AlbwBtn_c::ACT_DAMAGEL:
        return "DAMAGEL";
    case AlbwBtn_c::ACT_ENDING:
        return "ENDING";
    case AlbwBtn_c::ACT_YOROKE:
        return "YOROKE";
    default:
        return "?";
    }
}

// [NEW helper] albwBeginGuardOpenWindow
void AlbwBtn_c::albwBeginGuardOpenWindow(u8 i_frames) {
    field_0xaa2 = i_frames;
    if (mTimer5 < 60) {
        mTimer5 = 60;
    }
    albwDebugLogEvent("open_window");
}

// [NEW helper] albwFinishBashGuardBreakFromHit
void AlbwBtn_c::albwFinishBashGuardBreakFromHit(cCcD_Obj* i_atObj, int i_result) {
    if (i_result == 1) {
        if (mActionMode1 == ACT_GUARDH) {
            setActionMode(ACT_GUARDH, ACTION2_0_e);
            albwDebugLogEvent("finish_p1_immediate");
        } else {
            field_0xa9d |= 0x80;
            field_0xa9d |= 0x40;
            albwDebugLogEvent("finish_p1_deferred");
        }
    } else if (i_result == 2) {
        albwDebugLogEvent("finish_p2_yoroke");
    }

    def_se_set(&mSound, i_atObj, 42, this);
}

// [NEW helper] albwApplyPendingPhase1GuardBreak
void AlbwBtn_c::albwApplyPendingPhase1GuardBreak() {
    if ((field_0xa9d & 0x80) == 0) {
        return;
    }

    field_0xa9d &= (u8)~0x80;
    setSwordAtBit(0);
    setActionMode(ACT_GUARDH, ACTION2_0_e);
    albwDebugLogEvent("apply_pending");
}

// [NEW helper] albwIsPhase1AttackVulnerable
bool AlbwBtn_c::albwIsPhase1AttackVulnerable() const {
    return mNextBreakPart < 11 && !field_0xa91 &&
           (mActionMode1 == ACT_ATTACKH || mActionMode1 == ACT_ATTACKSHIELDH);
}

// [NEW helper] albwIsPhase1BashTargetState
bool AlbwBtn_c::albwIsPhase1BashTargetState() const {
    switch (mActionMode1) {
    case ACT_GUARDH:
    case ACT_CHASEH:
    case ACT_ATTACKH:
    case ACT_ATTACKSHIELDH:
        return true;
    default:
        return false;
    }
}

// [NEW helper] albwApplyPhase1BashGuardBreak
bool AlbwBtn_c::albwApplyPhase1BashGuardBreak() {
    if (!dShield_isParryCombatEnabled() || !dShield_tryGrantHelmPunishCredit(this)) {
        return false;
    }

    albwBeginGuardOpenWindow(90);
    return true;
}

// [NEW helper] albwApplyPhase2BashGuardBreak
bool AlbwBtn_c::albwApplyPhase2BashGuardBreak() {
    if (!dShield_isParryCombatEnabled() || !dShield_tryGrantHelmPunishCredit(this)) {
        return false;
    }

    albwBeginGuardOpenWindow(75);
    return true;
}

// [NEW helper] albwTryApplyBashGuardBreakFromHit
int AlbwBtn_c::albwTryApplyBashGuardBreakFromHit() {
    if (!dShield_isParryCombatEnabled()) {
        return 0;
    }

    if (mActionMode1 == ACT_CHANGEDEMO || mActionMode1 == ACT_ENDING) {
        return 0;
    }

    if (!albwIsUnarmoredPhase() && albwIsPhase1BashTargetState()) {
        if (!albwApplyPhase1BashGuardBreak()) {
            albwDebugLogEvent("try_p1_fail");
            return 0;
        }

        field_0xaa8 = true;
        onHeadLockFlg();
        albwDebugLogEvent("try_p1_ok");
        return 1;
    }

    if (albwIsUnarmoredPhase() && mActionMode1 >= ACT_CHASEL && mActionMode1 != ACT_CHANGEDEMO &&
        mActionMode1 != ACT_ENDING)
    {
        if (!albwApplyPhase2BashGuardBreak()) {
            albwDebugLogEvent("try_p2_fail");
            return 0;
        }

        field_0xaa8 = true;
        onHeadLockFlg();
        setSwordAtBit(0);
        setActionMode(ACT_YOROKE, ACTION2_0_e);
        albwDebugLogEvent("try_p2_ok");
        return 2;
    }

    albwDebugLogEvent("try_no_match");
    return 0;
}

// [NEW helper] albwHandleParryCombatBashShieldHit
bool AlbwBtn_c::albwHandleParryCombatBashShieldHit(dCcD_Sph* i_sph, cCcD_Obj* i_atObj) {
    if (!dShield_isParryCombatEnabled()) {
        return false;
    }

    albwDebugLogEvent("shield_hit");
    const int guardBreakResult = albwTryApplyBashGuardBreakFromHit();
    if (guardBreakResult != 0) {
        albwFinishBashGuardBreakFromHit(i_atObj, guardBreakResult);
        return true;
    }

    if (field_0xaa2 == 0) {
        field_0xaa8 = false;
    }
    setShieldEffect(i_sph);
    mTimer9 = 15;
    albwDebugLogEvent("shield_clang");
    return true;
}

// [NEW helper] albwDarknutAtHitIsShieldBash
static bool albwDarknutAtHitIsShieldBash(cCcD_Obj* i_atHitObj) {
    return i_atHitObj != NULL && i_atHitObj->ChkAtType(AT_TYPE_SHIELD_ATTACK);
}

// ---- WHOLE fork replacements (verbatim, transformed): hook the stock
//      symbol with HOOK_SKIP_ORIGINAL and call these ----
// [WHOLE replacement] setActionMode
void AlbwBtn_c::setActionMode(int i_mode1, int i_mode2) {
#if TARGET_PC
    const bool keepHeadLock = field_0xaa2 != 0 && field_0xaa8;
    if (!keepHeadLock) {
        offHeadLockFlg();
    }
#else
    offHeadLockFlg();
#endif
    mActionMode1 = i_mode1;
    mActionMode2 = i_mode2;
#if TARGET_PC
    if (keepHeadLock) {
        onHeadLockFlg();
    }
#endif
}

// [WHOLE replacement] setShieldEffect
void AlbwBtn_c::setShieldEffect(dCcD_Sph* i_sph) {
    def_se_set(&mSound, i_sph->GetTgHitObj(), 42, this);
    dComIfGp_setHitMark(2, this, i_sph->GetTgHitPosP(), NULL, NULL, 0);
}

// [WHOLE replacement] damage_check
void AlbwBtn_c::damage_check() {
    mStts1.Move();

    for (int i = 0; i < 4; i++) {
        if (mSwordSphs[i].ChkAtHit()) {
            field_0xa9a = true;
#if TARGET_PC
            cCcD_Obj* atHitObj = mSwordSphs[i].GetAtHitObj();
            if (albwDarknutAtHitIsShieldBash(atHitObj)) {
                albwDebugLogEvent("dmg_sword");
                const int guardBreakResult = albwTryApplyBashGuardBreakFromHit();
                if (guardBreakResult != 0) {
                    albwFinishBashGuardBreakFromHit(atHitObj, guardBreakResult);
                    return;
                }
            }
#endif
            if (mSwordSphs[i].ChkAtShieldHit() && mSwordSphs[i].GetAtSpl() == 10) {
                setSwordAtBit(0);
                /* [PORT-DEAD @ mType==0] fork/stock write the file-static m_attack_tn here (stock d_a_b_tn.cpp:1061). It is non-exported, so a plugin cannot address it - and under this port's mType==0 hook gate the write is provably unobservable: the only READ is inside checkNormalAttackAble's `if (mType == 1)` block (stock :1066-1072), and action() clears it unconditionally for mType == 0 (stock :4440). */
                break;
            }
        }
    }

    if (mCps.ChkAtHit()) {
        field_0xa9a = true;
#if TARGET_PC
        cCcD_Obj* atHitObj = mCps.GetAtHitObj();
        if (albwDarknutAtHitIsShieldBash(atHitObj)) {
            albwDebugLogEvent("dmg_cps");
            const int guardBreakResult = albwTryApplyBashGuardBreakFromHit();
            if (guardBreakResult != 0) {
                setSwordAtBit(0);
                albwFinishBashGuardBreakFromHit(atHitObj, guardBreakResult);
                return;
            }
        }
#endif
        if (mCps.ChkAtShieldHit() && mCps.GetAtSpl() == 10) {
            setSwordAtBit(0);
            /* [PORT-DEAD @ mType==0] fork/stock write the file-static m_attack_tn here (stock d_a_b_tn.cpp:1061). It is non-exported, so a plugin cannot address it - and under this port's mType==0 hook gate the write is provably unobservable: the only READ is inside checkNormalAttackAble's `if (mType == 1)` block (stock :1066-1072), and action() clears it unconditionally for mType == 0 (stock :4440). */
        }
    }

    if (mSphC.ChkAtHit() && !mSphC.ChkAtShieldHit()) {
        field_0xa9a = true;
#if TARGET_PC
        cCcD_Obj* atHitObj = mSphC.GetAtHitObj();
        if (albwDarknutAtHitIsShieldBash(atHitObj)) {
            albwDebugLogEvent("dmg_sphc");
            const int guardBreakResult = albwTryApplyBashGuardBreakFromHit();
            if (guardBreakResult != 0) {
                mSphC.ClrAtHit();
                albwFinishBashGuardBreakFromHit(atHitObj, guardBreakResult);
                return;
            }
        }
#endif
        mSound.startCreatureSound(Z2SE_EN_TN_ATK_NO_DMG, 0, -1);
        mSphC.ClrAtHit();
        /* [PORT-DEAD @ mType==0] fork/stock write the file-static m_attack_tn here (stock d_a_b_tn.cpp:1061). It is non-exported, so a plugin cannot address it - and under this port's mType==0 hook gate the write is provably unobservable: the only READ is inside checkNormalAttackAble's `if (mType == 1)` block (stock :1066-1072), and action() clears it unconditionally for mType == 0 (stock :4440). */
    }

    if (mInvincibilityTimer != 0) {
        return;
    }

    if (mActionMode1 == ACT_CHANGEDEMO) {
        return;
    }

    if (mActionMode1 == ACT_ENDING) {
        return;
    }

    dCcD_Sph dStack_160;

    if (mActionMode1 <= 1) {
        mAtInfo.mpCollider = NULL;
        if (mSphC.ChkTgHit()) {
            mAtInfo.mpCollider = mSphC.GetTgHitObj();
            dStack_160 = mSphC;
        }

        for (int i = 0; i < 3; i++) {
            if (mSphA[i].ChkTgHit()) {
                mAtInfo.mpCollider = mSphA[i].GetTgHitObj();
                dStack_160 = mSphA[i];
                break;
            }
        }

        for (int i = 0; i < 3; i++) {
            if (mSphB[i].ChkTgHit()) {
                mAtInfo.mpCollider = mSphB[i].GetTgHitObj();
                dStack_160 = mSphB[i];
                break;
            }
        }

        if (mAtInfo.mpCollider != NULL) {
            setShieldEffect(&dStack_160);
            setActionMode(ACT_OPENING, ACTION2_2_e);

            if (mAtInfo.mpCollider->ChkAtType(AT_TYPE_BOOMERANG) ||
                mAtInfo.mpCollider->ChkAtType(AT_TYPE_40) ||
                mAtInfo.mpCollider->ChkAtType(AT_TYPE_HOOKSHOT))
            {
                mTimer9 = 15;
            }

            if (mAtInfo.mpCollider->ChkAtType(AT_TYPE_SLINGSHOT)) {
#if TARGET_PC
                if (dMeter2_isALBWLocked()) {
                    dAlbwLockout_onSlingshotHit(this);
                } else
#endif
                {
                    mTimer9 = dAlbwLockout_getSlingshotStunFrames(15);
                    dAlbwLockout_onSlingshotHitNative(this);
                }
            }
        }

        return;
    }

    int cut_type = daPy_getPlayerActorClass()->getCutType();
#if TARGET_PC
    // ============================================
    // Guard-opener rework: an active open window (field_0xaa2, armed by a
    // shield bash OR an opener hit below) bypasses the central-shield fast
    // path too, so window-period hits actually land in the armored phase.
    // ============================================
    if (field_0xaa2 == 0 && mActionMode1 < 8 && mSphC.ChkTgHit() &&
        mSphC.GetTgHitObj()->ChkAtType(18) &&
        cut_type != daPy_py_c::CUT_TYPE_HEAD_JUMP &&
        cut_type != daPy_py_c::CUT_TYPE_MORTAL_DRAW_B &&
        cut_type != daPy_py_c::CUT_TYPE_MORTAL_DRAW_A)
#else
    if (mActionMode1 < 8 && mSphC.ChkTgHit() && mSphC.GetTgHitObj()->ChkAtType(18) &&
        cut_type != daPy_py_c::CUT_TYPE_HEAD_JUMP &&
        cut_type != daPy_py_c::CUT_TYPE_MORTAL_DRAW_B &&
        cut_type != daPy_py_c::CUT_TYPE_MORTAL_DRAW_A)
#endif
    {
#if TARGET_PC
        // Opener hit (Hurricane / Combat Howl / Midna arm) on the central
        // shield: clank this hit but open the guard window so follow-up
        // hits (Hurricane revolutions, arm re-strikes) connect.
        if (dAlbwCombat_isGuardOpenerHit(mSphC.GetTgHitObj())) {
            albwBeginGuardOpenWindow(kAlbwGuardOpenerWindowFrames);
            setShieldEffect(&mSphC);
            return;
        }
#endif
        def_se_set(&mSound, mSphC.GetTgHitObj(), 42, this);
        if (mSphC.GetTgHitObj()->ChkAtType(AT_TYPE_SHIELD_ATTACK)) {
#if TARGET_PC
            if (dShield_isParryCombatEnabled()) {
                if (albwHandleParryCombatBashShieldHit(&mSphC, mSphC.GetTgHitObj())) {
                    return;
                }
                if (albwIsPhase1AttackVulnerable()) {
                    albwDebugLogEvent("dmg_sphc_attack_clang");
                    setShieldEffect(&mSphC);
                    mTimer9 = 15;
                    return;
                }
                return;
            }
#endif
            if (mActionMode1 != ACT_GUARDH) {
                return;
            }

            if (field_0xaa8) {
                return;
            }

            field_0xaa8 = true;
        } else {
            field_0xaa8 = false;
        }

        setActionMode(ACT_GUARDH, ACTION2_0_e);
        return;
    }

    mAtInfo.mpCollider = NULL;
    for (int i = 0; i < 3; i++) {
        if (mSphA[i].ChkTgHit()) {
            mAtInfo.mpCollider = mSphA[i].GetTgHitObj();
            dStack_160 = mSphA[i];
            break;
        }
    }

    if (mAtInfo.mpCollider != NULL) {
        if (cut_type == daPy_py_c::CUT_TYPE_LARGE_JUMP_FINISH) {
            return;
        } else if (mActionMode1 < 8) {
            if (cut_type == daPy_py_c::CUT_TYPE_DASH_LEFT ||
                cut_type == daPy_py_c::CUT_TYPE_DASH_RIGHT ||
                mAtInfo.mpCollider->ChkAtType(AT_TYPE_UNK))
            {
                setShieldEffect(&dStack_160);
                mTimer9 = 15;
                return;
            }

#if TARGET_PC
            // ============================================
            // Guard-opener rework: the armored-phase block now honors the
            // ALBW guard-open window (field_0xaa2), mirroring the unarmored
            // block below — previously only phase 2 honored it, which is
            // why bash/opener windows never helped in the armored fight.
            // ============================================
            if (field_0xaa2 == 0 && dStack_160.ChkTgShield()) {
#else
            if (dStack_160.ChkTgShield()) {
#endif
                if (mAtInfo.mpCollider->ChkAtType(AT_TYPE_SHIELD_ATTACK)) {
                    if (mActionMode1 == ACT_GUARDH && field_0xaa8) {
                        return;
                    }

#if TARGET_PC
                    if (albwHandleParryCombatBashShieldHit(&dStack_160, dStack_160.GetTgHitObj())) {
                        return;
                    }
#endif
                    field_0xaa8 = true;
                    def_se_set(&mSound, dStack_160.GetTgHitObj(), 42, this);
                } else {
#if TARGET_PC
                    // Opener hit on a raised arm shield: clank + open window.
                    if (dAlbwCombat_isGuardOpenerHit(mAtInfo.mpCollider)) {
                        albwBeginGuardOpenWindow(kAlbwGuardOpenerWindowFrames);
                        setShieldEffect(&dStack_160);
                        return;
                    }
#endif
                    field_0xaa8 = false;
                    setShieldEffect(&dStack_160);
                }

                setActionMode(ACT_GUARDH, ACTION2_0_e);
                return;

            } else if (mAtInfo.mpCollider->ChkAtType(AT_TYPE_SHIELD_ATTACK)) {
#if TARGET_PC
                if (dShield_isParryCombatEnabled()) {
                    if (albwHandleParryCombatBashShieldHit(&dStack_160, dStack_160.GetTgHitObj())) {
                        return;
                    }
                }
#endif
                setShieldEffect(&dStack_160);
                mTimer9 = 0x14;
                return;
            }

            if (cut_type == daPy_py_c::CUT_TYPE_HEAD_JUMP ||
                cut_type == daPy_py_c::CUT_TYPE_MORTAL_DRAW_B ||
                cut_type == daPy_py_c::CUT_TYPE_MORTAL_DRAW_A)
            {
                mTimer12 = 60;
            }
        } else {
#if TARGET_PC
            if (field_0xaa2 == 0 && dStack_160.ChkTgShield()) {
#else
            if (dStack_160.ChkTgShield()) {
#endif
                if (mAtInfo.mpCollider->ChkAtType(AT_TYPE_SHIELD_ATTACK)) {
#if TARGET_PC
                    if (albwHandleParryCombatBashShieldHit(&dStack_160, dStack_160.GetTgHitObj())) {
                        return;
                    }
#endif
                    field_0xaa8 = true;
                    def_se_set(&mSound, dStack_160.GetTgHitObj(), 42, this);
                } else {
#if TARGET_PC
                    // Opener hit on a raised arm shield (unarmored phase —
                    // arms stay up vs wolf via checkNowWolf): clank + open.
                    if (dAlbwCombat_isGuardOpenerHit(mAtInfo.mpCollider)) {
                        albwBeginGuardOpenWindow(kAlbwGuardOpenerWindowFrames);
                        setShieldEffect(&dStack_160);
                        return;
                    }
#endif
                    field_0xaa8 = false;
                    setShieldEffect(&dStack_160);
                }

                setActionMode(ACT_GUARDL, ACTION2_0_e);
                return;
            }

            if (mAtInfo.mpCollider->ChkAtType(AT_TYPE_SHIELD_ATTACK)) {
#if TARGET_PC
                if (albwHandleParryCombatBashShieldHit(&dStack_160, dStack_160.GetTgHitObj())) {
                    return;
                }
#endif
                field_0xaa8 = true;
                def_se_set(&mSound, dStack_160.GetTgHitObj(), 42, this);
                setActionMode(ACT_GUARDL, ACTION2_0_e);
                return;
            }

#if TARGET_PC
            if (field_0xaa2 == 0 && field_0xa91) {
#else
            if (field_0xa91) {
#endif
                if (mCutFlag || (getCutType() & 4) != 0) {
#if TARGET_PC
                    // Opener hit routed to the facing guard-stagger branch
                    // (no shield sphere involved): clank + open window so
                    // the follow-up revolutions/strikes land.
                    if (dAlbwCombat_isGuardOpenerHit(mAtInfo.mpCollider)) {
                        albwBeginGuardOpenWindow(kAlbwGuardOpenerWindowFrames);
                        setShieldEffect(&dStack_160);
                        return;
                    }
#endif
                    setShieldEffect(&dStack_160);
                    if (mCutFlag) {
                        setActionMode(ACT_GUARDL, ACTION2_11_e);
                    } else {
                        setActionMode(ACT_YOROKE, ACTION2_0_e);
                    }

                    return;
                }

                if (mTimer10 != 0 ||
                    abs((s16)(fopAcM_searchPlayerAngleY(this) - shape_angle.y)) < 0x3000 ||
                    cut_type == daPy_py_c::CUT_TYPE_DASH_LEFT ||
                    cut_type == daPy_py_c::CUT_TYPE_DASH_RIGHT)
                {
                    field_0xaa8 = false;
                    setShieldEffect(&dStack_160);
                    setActionMode(ACT_GUARDL, ACTION2_0_e);
                    return;
                }
            }
        }
        setDamage(&dStack_160, 1);
        return;
    }

    mAtInfo.mpCollider = NULL;
    for (int i = 0; i < 3; i++) {
        if (mSphB[i].ChkTgHit()) {
            mAtInfo.mpCollider = mSphB[i].GetTgHitObj();
            dStack_160 = mSphB[i];
            break;
        }
    }

    int bVar1 = false;
    if (abs((s16)(fopAcM_searchPlayerAngleY(this) - shape_angle.y)) > 0x3000) {
        bVar1 = true;
    };

    if (mActionMode1 < 8) {
        if (!dStack_160.ChkTgShield() || mNextBreakPart >= 11) {
            bVar1 = true;
        }
    } else if (!field_0xa91) {
        bVar1 = true;
    }

    if (mActionMode1 < 8 && mSphC.ChkTgHit()) {
        mAtInfo.mpCollider = mSphC.GetTgHitObj();
        dStack_160 = mSphC;
        bVar1 = false;
    }

    if (mAtInfo.mpCollider) {
#if TARGET_PC
        if (field_0xaa2 == 0) {
            field_0xaa8 = false;
        }
#else
        field_0xaa8 = false;
#endif
        if (mAtInfo.mpCollider->ChkAtType(AT_TYPE_BOOMERANG) ||
            mAtInfo.mpCollider->ChkAtType(AT_TYPE_40))
        {
            if (mActionMode1 < 8) {
                mTimer9 = 15;
            } else {
                setActionMode(ACT_GUARDL, ACTION2_10_e);
            }
        } else if (mAtInfo.mpCollider->ChkAtType(AT_TYPE_SLINGSHOT)) {
#if TARGET_PC
            if (dMeter2_isALBWLocked()) {
                dAlbwLockout_onSlingshotHit(this);
                if (mActionMode1 < 8) {
                    mTimer9 = dAlbwLockout_getSlingshotStunFrames(15);
                } else {
                    setActionMode(ACT_GUARDL, ACTION2_10_e);
                }
                return;
            }
#endif
            dAlbwLockout_onSlingshotHitNative(this);
            if (mActionMode1 < 8) {
                mTimer9 = dAlbwLockout_getSlingshotStunFrames(15);
            } else {
                setActionMode(ACT_GUARDL, ACTION2_10_e);
            }
        } else if (mAtInfo.mpCollider->ChkAtType(AT_TYPE_BOMB)) {
#if TARGET_PC
            if (dAlbwLockout_isRangedOpened(this)) {
                setDamage(&dStack_160, 2);
            } else
#endif
            if (mActionMode1 < 8) {
                if (mNextBreakPart >= 11 ||
                    abs((s16)(cLib_targetAngleY(
                                  &current.pos,
                                  &dCc_GetAc(mAtInfo.mpCollider->GetAc())->current.pos) -
                              shape_angle.y)) > 0x4000)
                {
                    setDamage(&dStack_160, 2);
                } else {
                    setActionMode(ACT_GUARDH, ACTION2_0_e);
                }
            } else if (abs((s16)(cLib_targetAngleY(
                                     &current.pos,
                                     &dCc_GetAc(mAtInfo.mpCollider->GetAc())->current.pos) -
                                 shape_angle.y)) > 0x4000)
            {
                setDamage(&dStack_160, 2);
            }
        } else if (mAtInfo.mpCollider->ChkAtType(AT_TYPE_SPINNER)) {
            setShieldEffect(&dStack_160);
            if (mActionMode1 < 8) {
                setActionMode(ACT_GUARDH, ACTION2_0_e);
            } else {
                setActionMode(ACT_GUARDL, ACTION2_10_e);
            }
        } else if (mAtInfo.mpCollider->ChkAtType(AT_TYPE_HOOKSHOT)) {
            if (mActionMode1 < 8) {
                mTimer9 = 15;
                setShieldEffect(&dStack_160);
            } else if (!bVar1 || mTimer10 != 0) {
                setShieldEffect(&dStack_160);
                setActionMode(ACT_GUARDL, ACTION2_0_e);
            } else {
                health = 100;
                cc_at_check(this, &mAtInfo);
                mNextActionMode2 = ACTION2_0_e;
                if (cM_rnd() < 0.5f) {
                    mNextActionMode2 = ACTION2_1_e;
                }

                if (mActionMode1 != ACT_DAMAGEL && mTimer5 == 0) {
                    mTimer5 = 90;
                }

                setActionMode(ACT_DAMAGEL, mNextActionMode2);
            }
        } else if (mAtInfo.mpCollider->ChkAtType(AT_TYPE_IRON_BALL)) {
#if TARGET_PC
            if (dAlbwLockout_isRangedOpened(this)) {
                setDamage(&dStack_160, 2);
            } else
#endif
            if (!bVar1) {
                setShieldEffect(&dStack_160);
                if (mActionMode1 < 8) {
                    setActionMode(ACT_GUARDH, ACTION2_0_e);
                } else {
                    setActionMode(ACT_GUARDL, ACTION2_10_e);
                }
            } else {
                setDamage(&dStack_160, 2);
            }
        } else if (mAtInfo.mpCollider->ChkAtType(AT_TYPE_ARROW)) {
#if TARGET_PC
            if (dAlbwLockout_isRangedOpened(this)) {
                setDamage(&dStack_160, 1);
            } else
#endif
            if (mActionMode1 < 8) {
                setShieldEffect(&dStack_160);
            } else if (bVar1 == 0 || mTimer10 != 0) {
                setShieldEffect(&dStack_160);
                setActionMode(ACT_GUARDL, ACTION2_0_e);
            } else {
                setDamage(&dStack_160, 1);
            }
#if TARGET_PC
        } else if (mAtInfo.mpCollider->ChkAtType(AT_TYPE_SHIELD_ATTACK)) {
            if (dShield_isParryCombatEnabled()) {
                if (albwHandleParryCombatBashShieldHit(&dStack_160, mAtInfo.mpCollider)) {
                    return;
                }
            }

            setShieldEffect(&dStack_160);
            if (mActionMode1 < 8) {
                mTimer9 = 0x14;
            }
            return;
#endif
        }
    }
}

// [WHOLE replacement] executeChaseH
void AlbwBtn_c::executeChaseH() {
    f32 mPlayerDistance = fopAcM_searchPlayerDistance(this);
    s16 sVar4 = fopAcM_searchPlayerAngleY(this);
    s16 sVar5 = fopAcM_searchPlayerAngleY(this) - cM_atan2s(-current.pos.x, -current.pos.z);
    int mMoveArea = checkMoveArea();

    switch (mActionMode2) {
    case ACTION2_0_e:
    case ACTION2_76_e:
        if (mMoveArea == 3) {
            if (checkMoveAngle()) {
                mActionMode2 = ACTION2_100_e;
            } else {
                mActionMode2 = ACTION2_103_e;
            }
        } else {
            if (mMoveArea != 0 && mPlayerDistance > 800.0f) {
                mPlayerDistance = 400.0f;
            }

            if (mPlayerDistance < 200.0f) {
                mActionMode2 = ACTION2_3_e;
            } else if (mPlayerDistance >= 800.0f) {
                if (mActionMode2 == ACTION2_0_e) {
                    mActionMode2 = ACTION2_10_e;
                } else {
                    mActionMode2 = ACTION2_8_e;
                }
            } else if (mPlayerDistance >= 400.0f) {
                mActionMode2 = ACTION2_1_e;
            } else {
                mActionMode2 = ACTION2_5_e;
            }
        }

        setSwordAtBit(0);
#if TARGET_PC
        if (field_0xaa2 == 0) {
            field_0xa91 = true;
        } else {
            field_0xa91 = false;
        }
#else
        field_0xa91 = true;
#endif
        break;

    case ACTION2_8_e:
    case ACTION2_10_e:
        if (mActionMode2 == ACTION2_8_e) {
            setBck(BCK_TNA_WALK, 2, 3.0f, 1.0f);
        } else {
            setBck(BCK_TNA_WALK, 2, 10.0f, 1.0f);
        }

        mActionMode2 = ACTION2_9_e;
        speedF = 4.3f;
        // [[fallthrough]]

    case ACTION2_9_e:
        if (mpModelMorf2->checkFrame(23.0f)) {
            mSound.startCreatureSound(Z2SE_EN_TN_FOOT_M1, 0, -1);
        }

        if (mpModelMorf2->checkFrame(0.0f)) {
            mSound.startCreatureSound(Z2SE_EN_TN_FOOT_M2, 0, -1);
        }

        cLib_addCalcAngleS(&shape_angle.y, sVar4, 4, 0x400, 16);
        current.angle.y = shape_angle.y;

        if (mPlayerDistance < 700.0f) {
            mActionMode2 = ACTION2_11_e;
        }
        break;

    case ACTION2_1_e:
    case ACTION2_11_e:
        if (mActionMode2 == ACTION2_1_e) {
            setBck(BCK_TNA_WAIT_B_1, 2, 3.0f, 1.0f);
        } else {
            setBck(BCK_TNA_WAIT_B_1, 2, 10.0f, 1.0f);
        }

        mActionMode2 = ACTION2_2_e;
        speedF = 2.3f;
        current.angle.y = shape_angle.y;
        // [[fallthrough]]

    case ACTION2_2_e:
        setAwaitSound();
        cLib_addCalcAngleS(&shape_angle.y, sVar4, 4, 0x400, 16);
        current.angle.y = shape_angle.y;

        if (mPlayerDistance > 800.0f) {
            if (mMoveArea == 0) {
                mActionMode2 = ACTION2_10_e;
            }
        } else if (mPlayerDistance < 300.0f) {
            speedF = 0.0f;
            mActionMode2 = ACTION2_5_e;
        }
        break;

    case ACTION2_3_e:
        setBck(BCK_TNA_WAIT_B_1, 2, 3.0f, -1.0f);
        mActionMode2 = ACTION2_4_e;
        current.angle.y = shape_angle.y + 0x8000;
        mWalkDir = 0;
        // [[fallthrough]]

    case ACTION2_4_e:
        setAwaitSound();
        speedF = 2.3f;
        cLib_addCalcAngleS(&shape_angle.y, sVar4, 4, 0x400, 16);
        current.angle.y = shape_angle.y + 0x8000;

        if (mPlayerDistance < 200.0f && mTimer3 == 0) {
            mWalkDir++;
            if (mWalkDir > 60) {
                if (mType == 1) {
                    mTimer3 = l_HIO.mTimer3NormalType0;
                } else {
                    mTimer3 = l_HIO.mTimer3NormalType1;
                }

                setActionMode(ACT_ATTACKH, ACTION2_0_e);
            }
        } else {
            mWalkDir = 0;
        }

        if (mPlayerDistance > 300.0f) {
            speedF = 0.0f;
            mActionMode2 = ACTION2_5_e;
        }
        break;

    case ACTION2_5_e:
        if (cM_rnd() < 0.5f) {
            mActionMode2 = ACTION2_6_e;
            setBck(BCK_TNA_WAIT_B_2, 2, 3.0f, 1.0f);
            current.angle.y = shape_angle.y + 0x4000;
        } else {
            mActionMode2 = ACTION2_7_e;
            setBck(BCK_TNA_WAIT_B_2, 2, 3.0f, -1.0f);
            current.angle.y = shape_angle.y + -0x4000;
        }

        mTimer1 = cM_rndFX(20.0f) + 60.0f;
        speedF = 2.3f;
        break;

    case ACTION2_6_e:
        setAwaitSound();
        cLib_addCalcAngleS(&shape_angle.y, sVar4, 4, 0x400, 16);
        current.angle.y = shape_angle.y + 0x4000;

        if (mTimer1 == 0) {
            if (cM_rnd() < 0.5f) {
                setBck(BCK_TNA_WAIT_B_2, 2, 3.0f, -1.0f);
                current.angle.y = shape_angle.y + -0x4000;
                mActionMode2 = ACTION2_7_e;
            }

            mTimer1 = cM_rndFX(20.0f) + 60.0f;
        }

        if (mPlayerDistance < 200.0f) {
            mActionMode2 = ACTION2_3_e;
        }

        if (mPlayerDistance > 400.0f) {
            mActionMode2 = ACTION2_1_e;
        }
        break;

    case ACTION2_7_e:
        setAwaitSound();
        cLib_addCalcAngleS(&shape_angle.y, sVar4, 4, 0x400, 16);
        current.angle.y = shape_angle.y + -0x4000;

        if (mTimer1 == 0) {
            if (cM_rnd() < 0.5f) {
                setBck(BCK_TNA_WAIT_B_2, 2, 3.0f, 1.0f);
                current.angle.y = shape_angle.y + 0x4000;
                mActionMode2 = ACTION2_6_e;
            }
            mTimer1 = cM_rndFX(20.0f) + 90.0f;
        }

        if (mPlayerDistance < 200.0f) {
            mActionMode2 = ACTION2_3_e;
        }

        if (mPlayerDistance > 400.0f) {
            mActionMode2 = ACTION2_1_e;
        }
        break;

    case ACTION2_100_e:
        setBck(BCK_TNA_WAIT_B_1, 2, 10.0f, 1.0f);
        mActionMode2 = ACTION2_102_e;
        speedF = 2.3f;
        current.angle.y = shape_angle.y;
        // [[fallthrough]]

    case ACTION2_102_e:
        setAwaitSound();
        cLib_addCalcAngleS(&shape_angle.y, sVar4, 4, 0x400, 16);
        cLib_addCalcAngleS(&current.angle.y, cM_atan2s(-current.pos.x, -current.pos.z), 4, 0x400,
                           16);

        if (!checkMoveAngle()) {
            mActionMode2 = ACTION2_103_e;
        }

        if (mMoveArea <= 1) {
            mActionMode2 = ACTION2_76_e;
        }
        break;

    case ACTION2_103_e:
        mPlayerDistance = 3.0f;

        if ((u8)checkBck(BCK_TNA_WALK)) {
            mPlayerDistance = 10.0f;
        }

        if (abs(sVar5) < 0x2000) {
            if (cM_rnd() < 0.5f) {
                mActionMode2 = ACTION2_106_e;
                setBck(BCK_TNA_WAIT_B_2, 2, mPlayerDistance, 1.0f);
            } else {
                mActionMode2 = ACTION2_107_e;
                setBck(BCK_TNA_WAIT_B_2, 2, mPlayerDistance, -1.0f);
            }
        } else if (sVar5 < 0) {
            mActionMode2 = ACTION2_106_e;
            setBck(BCK_TNA_WAIT_B_2, 2, mPlayerDistance, 1.0f);
        } else {
            mActionMode2 = ACTION2_107_e;
            setBck(BCK_TNA_WAIT_B_2, 2, mPlayerDistance, -1.0f);
        }

        mTimer1 = cM_rndFX(20.0f) + 60.0f;
        // [[fallthrough]]

    case ACTION2_106_e:
    case ACTION2_107_e:
        setAwaitSound();
        speedF = 2.3f;
        cLib_addCalcAngleS(&shape_angle.y, sVar4, 4, 0x400, 16);

        if (mActionMode2 == ACTION2_106_e) {
            if (mMoveArea == 3) {
                current.angle.y = shape_angle.y + 0x6000;
            } else if (mMoveArea == 2) {
                current.angle.y = shape_angle.y + 0x4000;
            } else {
                current.angle.y = shape_angle.y + 0x3000;
            }
        } else if (mMoveArea == 3) {
            current.angle.y = shape_angle.y + -0x6000;
        } else if (mMoveArea == 2) {
            current.angle.y = shape_angle.y + -0x4000;
        } else {
            current.angle.y = shape_angle.y + -0x3000;
        }

        if (mTimer1 == 0 && mpModelMorf2->checkFrame(0.0f)) {
            if (abs(sVar5) > 0x7800) {
                mTimer1 = cM_rndFX(20.0f) + 90.0f;
                if (cM_rnd() < 0.5f) {
                    if (mActionMode2 == ACTION2_106_e) {
                        setBck(BCK_TNA_WAIT_B_2, 2, 3.0f, -1.0f);
                        mActionMode2 = ACTION2_107_e;
                    } else {
                        setBck(BCK_TNA_WAIT_B_2, 2, 3.0f, 1.0f);
                        mActionMode2 = ACTION2_106_e;
                    }
                }
            } else if (abs(sVar5) < 0x7000) {
                if (mActionMode2 == ACTION2_106_e) {
                    if (sVar5 > 0) {
                        setBck(BCK_TNA_WAIT_B_2, 2, 3.0f, -1.0f);
                        mActionMode2 = ACTION2_107_e;
                    }
                } else if (sVar5 < 0) {
                    setBck(BCK_TNA_WAIT_B_2, 2, 3.0f, 1.0f);
                    mActionMode2 = ACTION2_106_e;
                }
            }
        }

        if (checkMoveAngle()) {
            mActionMode2 = ACTION2_100_e;
        }

        if (mMoveArea < 1) {
            mActionMode2 = ACTION2_76_e;
        }
        break;
    }

    if (mActionMode2 < 100) {
        checkStartAttackH();
        if (mMoveArea == 3) {
            if (checkMoveAngle()) {
                mActionMode2 = ACTION2_100_e;
            } else {
                mActionMode2 = ACTION2_103_e;
            }
        }
    }
}

// [WHOLE replacement] executeAttackH
void AlbwBtn_c::executeAttackH() {
    f32 frame;
    s16 playerAngleY = fopAcM_searchPlayerAngleY(this);
    f32 playerDistance = fopAcM_searchPlayerDistance(this);

    switch (mActionMode2) {
    case ACTION2_0_e:
        speedF = 0.0f;

        if (playerDistance < 250.0f) {
            mActionMode2 = ACTION2_1_e;
            setBck(BCK_TNA_ATACK_A, 0, 3.0f, 1.0f);
        } else if (playerDistance < 350.0f) {
            mActionMode2 = ACTION2_2_e;
            setBck(BCK_TNA_ATACK_B, 0, 3.0f, 1.0f);
        } else if (cM_rnd() < 0.5f) {
            mActionMode2 = ACTION2_1_e;
            setBck(BCK_TNA_ATACK_A, 0, 3.0f, 1.0f);
        } else {
            mActionMode2 = ACTION2_2_e;
            setBck(BCK_TNA_ATACK_B, 0, 3.0f, 1.0f);
            setAttackBlurEffect(1);
        }

        field_0xa91 = false;
        break;

    case ACTION2_1_e:
        if (mpModelMorf2->getFrame() >= 20.0f && mpModelMorf2->getFrame() < 25.0f) {
            mUpdateModelTimer = 3;
            mDoMtx_stack_c::copy(mpModelMorf2->getModel()->getAnmMtx(15));
            mDoMtx_stack_c::transM(40.0f, 0.0f, 70.0f);
            mDoMtx_stack_c::YrotM(-0x8000);
            MTXCopy(mDoMtx_stack_c::get(), mNewTRMtx);
        }

        if (mpModelMorf2->checkFrame(18.0f)) {
            mSound.startCreatureSound(Z2SE_EN_TN_SWD_L, 0, -1);
            mSound.startCreatureVoice(Z2SE_EN_TN_V_KIAI, -1);
        }

        if (mpModelMorf2->checkFrame(23.0f)) {
            mSound.startCreatureSound(Z2SE_EN_TN_FOOT_L, 0, -1);
        }

        if (mpModelMorf2->checkFrame(72.0f)) {
            mSound.startCreatureSound(Z2SE_EN_TN_FOOT_M1, 0, -1);
        }

        if (mpModelMorf2->getFrame() <= 30.0f) {
            cLib_addCalcAngleS(&shape_angle.y, playerAngleY, 8, 0x400, 16);
        }

        frame = mpModelMorf2->getFrame();
        if (frame >= 18.0f && frame < 29.0f) {
            mSphCSmallFlag = true;
        }

        if (mpModelMorf2->checkFrame(18.0f)) {
            setSwordAtBit(1);
        }

        if (mpModelMorf2->checkFrame(29.0f)) {
            setSwordAtBit(0);
        }

        if (mpModelMorf2->isStop()) {
#if TARGET_PC
            if ((field_0xa9d & 0x80) == 0)
#endif
            {
                setActionMode(ACT_CHASEH, ACTION2_0_e);
                field_0xa91 = true;
#if TARGET_PC
                albwDebugLogEvent("attack_a1->chase");
#endif
            }
#if TARGET_PC
            else {
                albwDebugLogEvent("attack_a1_stop_blocked");
            }
#endif
        }
        break;

    case ACTION2_2_e:
        setAttackBlurEffect(1);
        if (mpModelMorf2->checkFrame(11.0f)) {
            mSound.startCreatureSound(Z2SE_EN_TN_FOOT_M1, 0, -1);
        } else if (mpModelMorf2->checkFrame(19.0f)) {
            mSound.startCreatureVoice(Z2SE_EN_TN_V_KIAI_AGE, -1);
        } else if (mpModelMorf2->checkFrame(22.0f)) {
            mSound.startCreatureSound(Z2SE_EN_TN_SWD_L, 0, -1);
        } else if (mpModelMorf2->checkFrame(23.0f)) {
            mSound.startCreatureSound(Z2SE_EN_TN_FOOT_L, 0, -1);
        } else if (mpModelMorf2->checkFrame(66.0f)) {
            mSound.startCreatureSound(Z2SE_EN_TN_FOOT_M2, 0, -1);
        }

        if (mpModelMorf2->getFrame() <= 22.0f) {
            cLib_addCalcAngleS(&shape_angle.y, playerAngleY, 8, 0x400, 16);
        }

        if (mpModelMorf2->getFrame() >= 22.0f && mpModelMorf2->getFrame() < 30.0f) {
            mChkCoHitOK = true;
            mSphCSmallFlag = true;
        }

        if (mpModelMorf2->checkFrame(22.0f)) {
            setSwordAtBit(1);
        }

        if (mpModelMorf2->checkFrame(30.0f)) {
            setSwordAtBit(0);
        }

        if (mpModelMorf2->checkFrame(26.0f)) {
            mSound.startCreatureSound(Z2SE_EN_TN_SWD_IMPCT, 0, -1);
            dComIfGp_getVibration().StartShock(6, 31, cXyz(0.0f, 1.0f, 0.0f));
        }

        if (mpModelMorf2->isStop()) {
#if TARGET_PC
            if ((field_0xa9d & 0x80) == 0)
#endif
            {
                setActionMode(ACT_CHASEH, ACTION2_0_e);
                field_0xa91 = true;
#if TARGET_PC
                albwDebugLogEvent("attack_a2->chase");
#endif
            }
#if TARGET_PC
            else {
                albwDebugLogEvent("attack_a2_stop_blocked");
            }
#endif
        }
    }
}

// [WHOLE replacement] executeGuardH
void AlbwBtn_c::executeGuardH() {
    cXyz sp18;
    switch (mActionMode2) {
    case ACTION2_0_e: {
        setSwordAtBit(0);
        mSphC.OffAtSetBit();
        field_0xa91 = mNextBreakPart < 11;

#if TARGET_PC
        const bool skipGuardSnap = (field_0xa9d & 0x40) != 0;
        if (skipGuardSnap) {
            field_0xa9d &= (u8)~0x40;
            albwDebugLogEvent("guardH_init_skip_snap");
        } else
#endif
        {
            mDoMtx_stack_c::copy(mpModelMorf2->getModel()->getAnmMtx(18));
            mDoMtx_stack_c::multVecZero(&sp18);
            current.pos.set(sp18.x, current.pos.y, sp18.z);
        }

        mActionMode2 = ACTION2_1_e;
        speedF = 0.0f;

        if (field_0xaa8) {
            shape_angle.y = fopAcM_searchPlayerAngleY(this);
            setBck(BCK_TNA_GUARD_DAMAGE, 0, 0.0f, 1.0f);
        } else {
            cLib_chaseAngleS(&shape_angle.y, fopAcM_searchPlayerAngleY(this), 0x2000);
            setBck(BCK_TNA_GUARD, 0, 0.0f, 1.0f);
        }
        break;
    }

    case ACTION2_1_e:
#if TARGET_PC
        if (field_0xaa2 != 0) {
            field_0xa91 = false;
        }
#endif
        if (field_0xaa8) {
            onHeadLockFlg();
        } else {
            // Obtained 4 secret techinques
            if (dComIfGs_isEventBit(dSv_event_flag_c::saveBitLabels[341])) {
                attention_info.flags |= fopAc_AttnFlag_UNK_0x200000;
            }
        }

        if (mpModelMorf2->isStop() &&
            daPy_getPlayerActorClass()->getCutType() != daPy_py_c::CUT_TYPE_HEAD_JUMP)
        {
            setActionMode(ACT_CHASEH, ACTION2_0_e);
#if TARGET_PC
            if (field_0xaa2 != 0) {
                field_0xa91 = false;
            }
            albwDebugLogEvent("guardH_done->chase");
#endif
        }
    }
}

// [WHOLE replacement] executeChaseL
void AlbwBtn_c::executeChaseL() {
    f32 mPlayerDistance = fopAcM_searchPlayerDistance(this);
    s16 mPlayerAngleY = fopAcM_searchPlayerAngleY(this);
    daPy_py_c* player = daPy_getPlayerActorClass();

    if (player->getCutCount() <= 1) {
        field_0xa9c++;
    } else {
        field_0xa9c = 0;
    }

    switch (mActionMode2) {
    case ACTION2_0_e:
        field_0xa9d = 0;
        // [[fallthrough]]

    case ACTION2_1_e:
        speedF = 0.0f;
        if (checkMoveArea()) {
            if (cM_rnd() < 0.5f) {
                initChaseL(4);
            } else {
                initChaseL(5);
            }
        } else if (mPlayerDistance > 800.0f) {
            initChaseL(3);
        } else if (cM_rnd() < 0.5f) {
            initChaseL(4);
        } else {
            initChaseL(5);
        }

        setSwordAtBit(0);
#if TARGET_PC
        if (field_0xaa2 == 0) {
            field_0xa91 = true;
        } else {
            field_0xa91 = false;
        }
#else
        field_0xa91 = true;
#endif
        break;

    case ACTION2_3_e:
        if (checkNextMove() == 0) {
            if (mpModelMorf2->checkFrame(16.0f)) {
                mSound.startCreatureSound(Z2SE_EN_TN_FOOT_M1, 0, -1);
            }

            if (mpModelMorf2->checkFrame(33.0f)) {
                mSound.startCreatureSound(Z2SE_EN_TN_FOOT_M2, 0, -1);
            }

            cLib_addCalcAngleS(&shape_angle.y, mPlayerAngleY, 4, 0x400, 16);
            current.angle.y = shape_angle.y;

            if (mPlayerDistance < 700.0f) {
                initChaseL(1);
            }

            if (checkMoveArea()) {
                if (cM_rnd() < 0.5f) {
                    initChaseL(4);
                } else {
                    initChaseL(5);
                }
            }
        }
        break;

    case ACTION2_16_e:
        if (mpModelMorf2->checkFrame(16.0f)) {
            mSound.startCreatureSound(Z2SE_EN_TN_FOOT_M1, 0, -1);
        }

        if (mpModelMorf2->checkFrame(33.0f)) {
            mSound.startCreatureSound(Z2SE_EN_TN_FOOT_M2, 0, -1);
        }

        cLib_addCalcAngleS(&shape_angle.y, mPlayerAngleY, 4, 0x400, 16);
        current.angle.y = shape_angle.y;

        if (mTimer1 == 0) {
            initChaseL(17);
        }
        break;

    case ACTION2_2_e:
        if (mTimer1 == 0) {
            initChaseL(1);
        }
        break;

    case ACTION2_4_e:
        if (checkNextMove() == 0) {
            if (mpModelMorf2->checkFrame(13.0f)) {
                mSound.startCreatureSound(Z2SE_EN_TN_FOOT_M1, 0, -1);
            }

            if (mpModelMorf2->checkFrame(27.0f)) {
                mSound.startCreatureSound(Z2SE_EN_TN_FOOT_M2, 0, -1);
            }

            mUpdateWaistAngle = true;
            setWalkDir();
            if (mWalkDir == 0) {
                cLib_addCalcAngleS(&shape_angle.y, mPlayerAngleY + -0x2000, 4, 0x400, 16);
            } else if (mWalkDir == 1) {
                cLib_addCalcAngleS(&shape_angle.y, mPlayerAngleY, 4, 0x400, 16);
            } else {
                cLib_addCalcAngleS(&shape_angle.y, mPlayerAngleY + 0x2000, 4, 0x400, 16);
            }

            current.angle.y = shape_angle.y + 0x4000;

            if (mTimer1 == 0) {
                initChaseL(1);
            }
        }
        break;

    case ACTION2_5_e:
        if (!checkNextMove()) {
            if (mpModelMorf2->checkFrame(13.0f)) {
                mSound.startCreatureSound(Z2SE_EN_TN_FOOT_M1, 0, -1);
            }

            if (mpModelMorf2->checkFrame(27.0f)) {
                mSound.startCreatureSound(Z2SE_EN_TN_FOOT_M2, 0, -1);
            }

            mUpdateWaistAngle = true;
            setWalkDir();
            if (mWalkDir == 0) {
                cLib_addCalcAngleS(&shape_angle.y, mPlayerAngleY + 0x2000, 4, 0x400, 16);
            } else if (mWalkDir == 1) {
                cLib_addCalcAngleS(&shape_angle.y, mPlayerAngleY, 4, 0x400, 16);
            } else {
                cLib_addCalcAngleS(&shape_angle.y, mPlayerAngleY + -0x2000, 4, 0x400, 16);
            }

            current.angle.y = shape_angle.y + -0x4000;

            if (mTimer1 == 0) {
                initChaseL(1);
            }
        }
        break;

    case ACTION2_6_e:
        cLib_addCalcAngleS(&shape_angle.y, mPlayerAngleY, 4, 0x400, 16);
        current.angle.y = shape_angle.y + 0x8000;

        if ((u8)checkBck(BCK_TNB_JUMP_B_1)) {
            if (mpModelMorf2->checkFrame(4.0f)) {
                speedF = 50.0f;
                speed.y = 19.0f;
                gravity = -4.0f;
            }

            if (mpModelMorf2->checkFrame(6.0f)) {
                mSound.startCreatureVoice(Z2SE_EN_TN_V_JUMP, -1);
            }

            if (mpModelMorf2->isStop()) {
                setBck(BCK_TNB_JUMP_B_2, 0, 5.0f, 0.0f);
            }
        } else if (mAcch.ChkGroundHit()) {
            initChaseL(7);
        }
        break;

    case ACTION2_7_e:
        cLib_chaseF(&speedF, 0.0f, 6.0f);

        if (mpModelMorf2->checkFrame(4.0f)) {
            mSound.startCreatureSound(Z2SE_EN_TN_FOOT_M2, 0, -1);
        }

        if (mpModelMorf2->isStop()) {
            if (!checkAttackAble() || mTimer13 != 0 || !checkNormalAttackAble()) {
                initChaseL(1);
            } else if (mPlayerDistance < 200.0f) {
                setActionMode(ACT_ATTACKSHIELDL, ACTION2_0_e);
            } else {
                setActionMode(ACT_ATTACKL, ACTION2_10_e);
            }
        }
        break;

    case ACTION2_8_e:
    case ACTION2_17_e:
        cLib_addCalcAngleS(&shape_angle.y, mPlayerAngleY, 4, 0x400, 16);
        current.angle.y = shape_angle.y;

        if (mpModelMorf2->checkFrame(4.0f)) {
            if (mActionMode2 == ACTION2_8_e) {
                speedF = 35.0f;
                speed.y = 4.0f;
            } else {
                speedF = 45.0f;
                speed.y = 5.0f;
            }

            gravity = -1.0f;
        }

        if (mpModelMorf2->checkFrame(6.0f)) {
            mSound.startCreatureVoice(Z2SE_EN_TN_V_JUMP, -1);
        }

        if (mpModelMorf2->isStop() && mAcch.ChkGroundHit()) {
            if (!checkAttackAble() || !checkNormalAttackAble()) {
                initChaseL(9);
            } else {
                mSound.startCreatureSound(Z2SE_EN_TN_FOOT_M1, 0, -1);

                if (mPlayerDistance < 200.0f) {
                    setActionMode(ACT_ATTACKSHIELDL, ACTION2_0_e);
                } else if (cM_rnd() < 0.5f) {
                    setActionMode(ACT_ATTACKL, ACTION2_10_e);
                } else {
                    setActionMode(ACT_ATTACKL, ACTION2_0_e);
                }
            }
        }
        break;

    case ACTION2_9_e:
        cLib_chaseF(&speedF, 0.0f, 7.0f);

        if (mpModelMorf2->checkFrame(4.0f)) {
            mSound.startCreatureSound(Z2SE_EN_TN_FOOT_M2, 0, -1);
        }

        if (mpModelMorf2->isStop()) {
            initChaseL(1);
        }
        break;

    case ACTION2_10_e:
    case ACTION2_11_e:
    case ACTION2_13_e:
    case ACTION2_14_e:
        cLib_addCalcAngleS(&shape_angle.y, mPlayerAngleY, 4, 0x400, 16);

        if (mActionMode2 == ACTION2_11_e || mActionMode2 == ACTION2_10_e) {
            current.angle.y = shape_angle.y + -0x4000;
        } else {
            current.angle.y = shape_angle.y + 0x4000;
        }

        if (mpModelMorf2->checkFrame(7.0f)) {
            speedF = 33.0f;
        } else if (mpModelMorf2->checkFrame(6.0f)) {
            mSound.startCreatureVoice(Z2SE_EN_TN_V_JUMP, -1);
            speedF = 35.0f;
        } else if (mpModelMorf2->checkFrame(5.0f)) {
            speedF = 40.0f;
        } else if (mpModelMorf2->checkFrame(4.0f)) {
            speedF = 45.0f;
            speed.y = 5.0f;
            gravity = -1.0f;
        }

        if (mpModelMorf2->isStop() && mAcch.ChkGroundHit()) {
            if (mActionMode2 == ACTION2_11_e || mActionMode2 == ACTION2_10_e) {
                initChaseL(12);
            } else {
                initChaseL(15);
            }
        }
        break;

    case ACTION2_12_e:
    case ACTION2_15_e:
        if (mpModelMorf2->checkFrame(5.0f)) {
            mSound.startCreatureSound(Z2SE_EN_TN_FOOT_M2, 0, -1);
        }

        cLib_chaseF(&speedF, 0.0f, 7.0f);

        if (mpModelMorf2->isStop()) {
            if (!checkAttackAble() || !checkNormalAttackAble()) {
                initChaseL(1);
            } else if (mPlayerDistance < 200.0f) {
                setActionMode(ACT_ATTACKSHIELDL, ACTION2_0_e);
            } else {
                setActionMode(ACT_ATTACKL, ACTION2_10_e);
            }
        }
    }
}

// [WHOLE replacement] executeGuardL
void AlbwBtn_c::executeGuardL() {
    daPy_py_c* player = daPy_getPlayerActorClass();
    s16 mPlayerAngleY = fopAcM_searchPlayerAngleY(this) - shape_angle.y;
    f32 mPlayerDistance = fopAcM_searchPlayerDistance(this);

    switch (mActionMode2) {
    case ACTION2_0_e:
    case ACTION2_11_e:
    case ACTION2_10_e:
        setSwordAtBit(0);
        setSwordAtBreak(1);
        mSphC.OffAtSetBit();
        speedF = 0.0f;

        if (mActionMode2 == ACTION2_0_e) {
#if TARGET_PC
            if (field_0xaa2 == 0) {
                field_0xa91 = true;
            }
#else
            field_0xa91 = true;
#endif

            u32 mCutType = getCutType();
            if ((mCutType & 16) != 0) {
                setBck(BCK_TNB_GUARD_A, 0, 3.0f, 1.0f);
            } else if ((mCutType & 32) != 0) {
                setBck(BCK_TNB_GUARD_B, 0, 3.0f, 1.0f);
            } else if (cM_rnd() < 0.5f) {
                setBck(BCK_TNB_GUARD_A, 0, 3.0f, 1.0f);
            } else {
                setBck(BCK_TNB_GUARD_B, 0, 3.0f, 1.0f);
            }

            speedF = 0.0f;
            mActionMode2 = ACTION2_1_e;
        } else {
            if (mActionMode2 == ACTION2_11_e) {
                mActionMode2 = ACTION2_3_e;
            } else {
                mActionMode2 = ACTION2_2_e;
            }

            field_0xa91 = false;
            setBck(BCK_TNB_DOWN, 0, 3.0f, 1.0f);
            speedF = 17.0f;
        }

        if (mTimer10 != 0) {
            shape_angle.y = fopAcM_searchPlayerAngleY(this);
        } else {
            cLib_chaseAngleS(&shape_angle.y, fopAcM_searchPlayerAngleY(this), 0x2000);
        }

        current.angle.y = shape_angle.y + 0x8000;
        break;

    case ACTION2_1_e:
        field_0xa91 = false;

#if TARGET_PC
        if (field_0xaa2 == 0 &&
#else
        if (
#endif
            player->getCutType() != daPy_py_c::CUT_TYPE_HEAD_JUMP && abs(mPlayerAngleY) < 0x3000)
        {
            field_0xa91 = true;
        }

        cLib_chaseF(&speedF, 0.0f, 1.0f);

        if (!mpModelMorf2->isStop()) {
            break;
        }

        if (player->getCutType() == daPy_py_c::CUT_TYPE_HEAD_JUMP) {
            break;
        }

        setActionMode(ACT_CHASEL, ACTION2_0_e);
        break;

    case ACTION2_2_e:
    case ACTION2_3_e:
        cLib_chaseF(&speedF, 0.0f, 1.0f);
        if (abs(mPlayerAngleY) < 0x3000) {
            field_0xa91 = true;
        } else {
            field_0xa91 = false;
        }

        if (mpModelMorf2->checkFrame(4.0f)) {
            mSound.startCreatureVoice(Z2SE_EN_TN_V_PULL_OUT, -1);
        }

        if (mpModelMorf2->checkFrame(37.0f)) {
            mSound.startCreatureSound(Z2SE_EN_TN_FOOT_M1, 0, -1);
        }

        if (mActionMode2 == ACTION2_3_e) {
            if (!mpModelMorf2->checkFrame(15.0f)) {
                break;
            }

            field_0xa91 = true;
            if (checkAttackAble() && mPlayerDistance < 200.0f) {
                setActionMode(ACT_ATTACKSHIELDL, ACTION2_0_e);
                break;
            }

            setActionMode(ACT_ATTACKL, ACTION2_11_e);
            break;
        }

        if (!mpModelMorf2->isStop()) {
            break;
        }

        setActionMode(ACT_CHASEL, ACTION2_0_e);
        break;
    }
}

// [WHOLE replacement] executeYoroke
void AlbwBtn_c::executeYoroke() {
    daPy_py_c* player = daPy_getPlayerActorClass();

    switch (mActionMode2) {
    case ACTION2_0_e:
    case ACTION2_1_e:
        if (mActionMode2 == ACTION2_0_e) {
            setBck(BCK_TNB_DOWN_SIPPU, 0, 3.0f, 1.0f);
        } else {
            setBck(BCK_TNB_DOWN_SIPPU, 0, 3.0f, 1.0f);
        }

        mActionMode2 = ACTION2_10_e;
        setSwordAtBit(0);
        setSwordAtBreak(1);
        mSphC.OffAtSetBit();
        field_0xa91 = false;
        speedF = 0.0f;
        mCutJumpStatus = 0;

        if (player->getCutType() == daPy_py_c::CUT_TYPE_JUMP) {
            mCutJumpStatus = 1;
        }
        // [[fallthrough]]

    case ACTION2_10_e:
        if (mCutJumpStatus == 1) {
            if (!player->checkCutJumpMode()) {
                mCutJumpStatus = 2;
            }
        } else if (mCutJumpStatus == 2 && player->checkCutJumpMode() &&
                   !player->checkCutJumpCancelTurn() && fopAcM_searchPlayerDistance(this) < 350.0f)
        {
#if TARGET_PC
            if (mNextBreakPart < 11) {
                setActionMode(ACT_CHASEH, ACTION2_0_e);
            } else {
                setActionMode(ACT_CHASEL, ACTION2_0_e);
            }
#else
            setActionMode(ACT_CHASEL, ACTION2_0_e);
#endif
            if (player->speedF < 28.0f) {
                initChaseL(6);
                break;
            }

            if ((s16)(fopAcM_searchPlayerAngleY(this) - shape_angle.y) < 0) {
                initChaseL(14);
            } else {
                initChaseL(11);
            }

            mTimer7 = 10;
            break;
        }

        if (mpModelMorf2->isStop()) {
#if TARGET_PC
            if (mNextBreakPart < 11) {
                setActionMode(ACT_CHASEH, ACTION2_0_e);
            } else {
                setActionMode(ACT_CHASEL, ACTION2_0_e);
            }

            if (field_0xaa2 == 0) {
                field_0xa91 = true;
            } else {
                field_0xa91 = false;
            }
#else
            setActionMode(ACT_CHASEL, ACTION2_0_e);
            field_0xa91 = true;
#endif
        }
    }
}

// ---- MODIFIED stock fns: ADDED hunks only (place at hook seams) ----