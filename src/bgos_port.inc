// === AUTO-EXTRACTED by port_tool.py — do not hand-edit the bodies ===
// diag defines forced 0 so scaffolding compiles out:

// ---- pulled fork-local dependencies (verbatim) ----
static b_go_class* boss;

static daB_GOS_HIO_c l_HIO;

static constexpr int kGraPunchBck = 11;

static constexpr int kGraHeadBackRBck = 12;

static constexpr int kGraHeadBackLBck = 13;

static constexpr int kGraTotterBck = 14;

static constexpr f32 kKidSearchR = 500.0f;

static constexpr f32 kKidPunchR = 230.0f;

static constexpr f32 kKidVertMax = 300.0f;

static constexpr f32 kKidPunchAngDeg = 15.0f;

static constexpr f32 kKidHandR = 45.0f;

static constexpr f32 kKidHeadBackAnimSpeed = 1.0f / 1.15f;

daB_GOS_HIO_c::daB_GOS_HIO_c() {
    field_0x4 = -1;
    mSize = 1.0f;
    mNormalSpeed = 10.0f;
}

static void anm_init(b_gos_class* i_this, int i_anmID, f32 i_morf, u8 i_attribute, f32 i_speed) {
    J3DAnmTransform* bck = (J3DAnmTransform*)dComIfG_getObjectRes("B_gos", i_anmID);
    i_this->mpMorf->setAnm(bck, i_attribute, i_morf, i_speed, 0.0f, -1.0f);
    i_this->mAnmID = i_anmID;
}

static void wait(b_gos_class* i_this) {
    i_this->speedF = 0.0f;

    switch (i_this->mMode) {
    case 0:
        anm_init(i_this, BCK_GRA_WAIT_AGRA_WAIT_A, 5.0f, J3DFrameCtrl::EMode_LOOP, 1.0f);
        i_this->mMode = 1;
        i_this->mTimers[0] = cM_rndF(30) + 30.0f;
        break;
    case 1:
        if (i_this->mTimers[0] == 0) {
            i_this->mAction = ACTION_WALK;
            i_this->mMode = 0;
        }
        break;
    }
}

static void walk(b_gos_class* i_this) {
    f32 speed = 0.0f;

    switch (i_this->mMode) {
    case 0:
        anm_init(i_this, BCK_GRA_WAIT_AGRA_RUN_A, 5.0f, J3DFrameCtrl::EMode_LOOP, 1.0f);
        i_this->mMode = 1;
        i_this->mTimers[0] = cM_rndF(60) + 60.0f;
        i_this->mWalkDirection = cM_rndF(0x10000);
        break;
    case 1:
        speed = l_HIO.mNormalSpeed;
        if (i_this->mTimers[0] == 0) {
            i_this->mAction = ACTION_WAIT;
            i_this->mMode = 0;
        }
        break;
    }

    cLib_addCalc2(&i_this->speedF, speed, 1.0f, 1.0f);
    cLib_addCalcAngleS2(&i_this->current.angle.y, i_this->mWalkDirection, 1, 0x800);
}

static void ball(b_gos_class* i_this) {
    switch (i_this->mMode) {
    case 0:
        i_this->mTimers[0] = cM_rndF(20);
        i_this->mMode = 1;
        // fallthrough
    case 1:
        if (i_this->mTimers[0] == 0) {
            anm_init(i_this, BCK_GRA_WAIT_AGRA_TO_STONE_NORMAL, 5.0f, J3DFrameCtrl::EMode_NONE, 1.0f);
            i_this->mMode = 2;
            i_this->mTimers[0] = cM_rndF(30) + 60.0f;
        }
        break;
    case 2:
        if (i_this->mTimers[0] == 0) {
            i_this->mAction = ACTION_STICK;
            i_this->mMode = 0;
        }
        break;
    }

    cLib_addCalc0(&i_this->speedF, 1.0f, 1.0f);
}

static void stick(b_gos_class* i_this) {
    cXyz offset;
    cXyz move_pos;

    if (boss == NULL) {
        return;
    }

    switch (i_this->mMode) {
    case 0:
        i_this->mMode = 1;
        i_this->speedF = 0.0f;
        i_this->mAngleOffset = cM_rndF(0x10000);
        break;
    case 1:
        break;
    }

    cLib_addCalc2(&i_this->speedF, 200, 1, 1);

    J3DModel* boss_model = boss->mpMorf->getModel();
    MTXCopy(boss_model->getAnmMtx(j_info[i_this->mJointIndex].joint_no & 0xFF), *calc_mtx);

    offset.x = 0.0f;
    offset.y = 0.0f;
    offset.z = 0.0f;
    MtxPosition(&offset, &move_pos);

    if (j_info[i_this->mJointIndex].field_0x4 < 1.0f) {
        cXyz sp60;
        MTXCopy(boss_model->getAnmMtx((j_info[i_this->mJointIndex].joint_no >> 8) & 0xFF),
                *calc_mtx);

        offset.x = 0.0f;
        offset.y = 0.0f;
        offset.z = 0.0f;
        MtxPosition(&offset, &sp60);

        move_pos += (sp60 - move_pos) * j_info[i_this->mJointIndex].field_0x4;
    }

    cLib_addCalc2(&i_this->current.pos.x, move_pos.x, 1.0f, i_this->speedF);
    cLib_addCalc2(&i_this->current.pos.y, move_pos.y, 1.0f, i_this->speedF);
    cLib_addCalc2(&i_this->current.pos.z, move_pos.z, 1.0f, i_this->speedF);
    cLib_addCalcAngleS2(&i_this->current.angle.y, boss->shape_angle.y + i_this->mAngleOffset, 1,
                        0x400);
}

static bool b_gos_albwHitIsShieldBash(cCcD_Obj* i_hit) {
    return i_hit != NULL && i_hit->ChkAtType(AT_TYPE_SHIELD_ATTACK);
}

static void b_gos_albwOffPunchAt(b_gos_class* i_this) {
    i_this->field_0x8f8.OffAtSetBit();
    i_this->field_0xa30.OffAtSetBit();
}

static void b_gos_albwStartHeadBackFromBash(b_gos_class* i_this) {
    const s16 diff = i_this->mAngleToPlayer - i_this->shape_angle.y;
    i_this->mTimers[1] = (diff > 0) ? 0 : 1;
    b_gos_albwOffPunchAt(i_this);
    i_this->mAction = ACTION_HEAD_BACK;
    i_this->mMode = 0;
    i_this->speedF = 0.0f;
}

static f32 b_gos_albwDistToPlayerXZ(b_gos_class* i_this) {
    return fopAcM_searchPlayerDistanceXZ(i_this);
}

static bool b_gos_albwPlayerInAggroRange(b_gos_class* i_this) {
    fopAc_ac_c* player = daPy_getPlayerActorClass();
    if (player == NULL) {
        return false;
    }
    const f32 yDelta = std::abs(player->current.pos.y - i_this->current.pos.y);
    return b_gos_albwDistToPlayerXZ(i_this) <= kKidSearchR && yDelta < kKidVertMax;
}

static void b_gos_albwResumeAfterReact(b_gos_class* i_this) {
    b_gos_albwOffPunchAt(i_this);
    if (b_gos_albwPlayerInAggroRange(i_this)) {
        i_this->mAction = ACTION_FIGHT;
        i_this->mMode = 0;
    } else {
        i_this->mAction = ACTION_WAIT;
        i_this->mMode = 0;
        i_this->speedF = 0.0f;
    }
}

static void b_gos_albwTryDeaggro(b_gos_class* i_this) {
    if (!b_gos_albwPlayerInAggroRange(i_this)) {
        b_gos_albwOffPunchAt(i_this);
        i_this->mAction = ACTION_WAIT;
        i_this->mMode = 0;
        i_this->speedF = 0.0f;
    }
}

static bool b_gos_albwPlayGraAnm(b_gos_class* i_this, int grAIdx, u8 attribute, f32 startFrame) {
    J3DAnmTransform* bck = (J3DAnmTransform*)dComIfG_getObjectRes("grA", grAIdx);
    if (bck == NULL) {
        return false;
    }

    i_this->mpMorf->setAnm(bck, attribute, 5.0f, 1.0f, startFrame, -1.0f);
    i_this->mAnmID = grAIdx;
    return true;
}

static void b_gos_albwEnsurePunchCc(b_gos_class* i_this) {
    if (i_this->field_0x8f8.GetStts() != NULL) {
        return;
    }

    static dCcD_SrcSph punch_sph_src = {
        {
            {0x0, {{AT_TYPE_800, 0x1, 0x1d}, {0x0, 0x0}, 0x0}},
            {dCcD_SE_HARD_BODY, 0x0, 0x1, 0x0, 0x0},
            {dCcD_SE_NONE, 0x0, 0x0, 0x0, 0x0},
            {0x0},
        },
        {
            {{0.0f, 0.0f, 0.0f}, 45.0f},
        },
    };

    i_this->field_0x8f8.Set(punch_sph_src);
    i_this->field_0x8f8.SetStts(&i_this->mCcStts);
    i_this->field_0x8f8.SetAtSe(dCcD_SE_HARD_BODY);
    i_this->field_0x8f8.SetAtSpl((dCcG_At_Spl)1);
    i_this->field_0x8f8.SetAtType(AT_TYPE_800);
    i_this->field_0x8f8.OnAtSPrmBit(12);
    i_this->field_0x8f8.SetAtAtp(1);
    i_this->field_0x8f8.OffTgSetBit();
    i_this->field_0x8f8.OffCoSetBit();
    i_this->field_0x8f8.OffAtSetBit();

    i_this->field_0xa30.Set(punch_sph_src);
    i_this->field_0xa30.SetStts(&i_this->mCcStts);
    i_this->field_0xa30.SetAtSe(dCcD_SE_HARD_BODY);
    i_this->field_0xa30.SetAtSpl((dCcG_At_Spl)1);
    i_this->field_0xa30.SetAtType(AT_TYPE_800);
    i_this->field_0xa30.OnAtSPrmBit(12);
    i_this->field_0xa30.SetAtAtp(1);
    i_this->field_0xa30.OffTgSetBit();
    i_this->field_0xa30.OffCoSetBit();
    i_this->field_0xa30.OffAtSetBit();
}

static void b_gos_albwPlayPunchAnm(b_gos_class* i_this) {
    J3DAnmTransform* bck = (J3DAnmTransform*)dComIfG_getObjectRes("grA", kGraPunchBck);
    if (bck != NULL) {
        i_this->mpMorf->setAnm(bck, J3DFrameCtrl::EMode_NONE, 5.0f, 1.0f, 0.0f, -1.0f);
        i_this->mAnmID = kGraPunchBck;
    } else {
        anm_init(i_this, BCK_GRA_WAIT_AGRA_RUN_A, 5.0f, J3DFrameCtrl::EMode_NONE, 1.0f);
    }
}

static void fight(b_gos_class* i_this) {
    b_gos_albwTryDeaggro(i_this);
    if (i_this->mAction != ACTION_FIGHT) {
        return;
    }

    switch (i_this->mMode) {
    case 0:
        anm_init(i_this, BCK_GRA_WAIT_AGRA_RUN_A, 5.0f, J3DFrameCtrl::EMode_LOOP, 1.0f);
        i_this->mMode = 1;
        break;
    case 1:
        cLib_addCalc2(&i_this->speedF, l_HIO.mNormalSpeed, 0.25f, 3.5f);
        cLib_addCalcAngleS2(&i_this->current.angle.y, i_this->mAngleToPlayer, 1, 0x800);
        if (i_this->mDistToPlayer < kKidPunchR) {
            const s16 diff = i_this->mAngleToPlayer - i_this->shape_angle.y;
            if (abs(static_cast<int>(diff)) < cM_deg2s(kKidPunchAngDeg)) {
                i_this->mAction = ACTION_PUNCH;
                i_this->mMode = 0;
            }
        }
        break;
    }
}

static void head_back(b_gos_class* i_this) {
    b_gos_albwTryDeaggro(i_this);
    if (i_this->mAction != ACTION_HEAD_BACK) {
        return;
    }

    i_this->speedF = 0.0f;

    switch (i_this->mMode) {
    case 0: {
        const int bck =
            (i_this->mTimers[1] != 0) ? kGraHeadBackLBck : kGraHeadBackRBck;
        if (!b_gos_albwPlayGraAnm(i_this, bck, J3DFrameCtrl::EMode_NONE, 0.0f)) {
            b_gos_albwResumeAfterReact(i_this);
            return;
        }
        i_this->mpMorf->setPlaySpeed(kKidHeadBackAnimSpeed);
        i_this->mSound.startCreatureVoice(Z2SE_GORON_V_HEAD_BACK, -1);
        i_this->mMode = 1;
        break;
    }
    case 1:
        if (i_this->mpMorf->isStop()) {
            b_gos_albwResumeAfterReact(i_this);
        }
        break;
    }
}

static void totter(b_gos_class* i_this) {
    b_gos_albwTryDeaggro(i_this);
    if (i_this->mAction != ACTION_TOTTER) {
        return;
    }

    i_this->speedF = 0.0f;

    switch (i_this->mMode) {
    case 0:
        if (!b_gos_albwPlayGraAnm(i_this, kGraTotterBck, J3DFrameCtrl::EMode_NONE, 6.0f)) {
            b_gos_albwResumeAfterReact(i_this);
            return;
        }
        i_this->mSound.startCreatureVoice(Z2SE_GORON_V_TOTTER, -1);
        i_this->mMode = 1;
        break;
    case 1:
        if (i_this->mpMorf->isStop()) {
            b_gos_albwResumeAfterReact(i_this);
        }
        break;
    }
}

static void punch(b_gos_class* i_this) {
    b_gos_albwTryDeaggro(i_this);
    if (i_this->mAction != ACTION_PUNCH) {
        return;
    }

    const f32 frame = i_this->mpMorf->getFrame();

    switch (i_this->mMode) {
    case 0:
        b_gos_albwPlayPunchAnm(i_this);
        b_gos_albwEnsurePunchCc(i_this);
        b_gos_albwOffPunchAt(i_this);
        i_this->mMode = 1;
        break;
    case 1:
        cLib_addCalc2(&i_this->speedF, l_HIO.mNormalSpeed / 3.0f, 0.25f, 3.5f);
        cLib_addCalcAngleS2(&i_this->current.angle.y, i_this->mAngleToPlayer, 4, 0x200);

        if (frame == 9.0f) {
            i_this->field_0xa30.OnAtSetBit();
        } else if (frame == 15.0f) {
            i_this->field_0xa30.OffAtSetBit();
        } else if (frame == 20.0f) {
            i_this->field_0x8f8.OnAtSetBit();
        } else if (frame >= 24.0f) {
            i_this->field_0x8f8.OffAtSetBit();
        }

        if (frame == 4.0f || (frame >= 18.5f && frame <= 19.5f)) {
            i_this->mSound.startCreatureVoice(Z2SE_GORON_V_PUNCH, -1);
        }

        if (i_this->mpMorf->isStop()) {
            b_gos_albwOffPunchAt(i_this);
            i_this->mAction = ACTION_FIGHT;
            i_this->mMode = 0;
        }
        break;
    }
}

static void b_gos_albwPunchHitReact(b_gos_class* i_this) {
    if (i_this->mAction != ACTION_PUNCH || i_this->mMode != 1) {
        return;
    }

    if (i_this->field_0xa30.ChkAtHit() && !i_this->field_0xa30.ChkAtShieldHit()) {
        i_this->field_0xa30.ClrAtHit();
        b_gos_albwOffPunchAt(i_this);
        i_this->mAction = ACTION_TOTTER;
        i_this->mMode = 0;
        return;
    }

    if (i_this->field_0x8f8.ChkAtHit() && !i_this->field_0x8f8.ChkAtShieldHit()) {
        i_this->field_0x8f8.ClrAtHit();
        b_gos_albwOffPunchAt(i_this);
        i_this->mAction = ACTION_TOTTER;
        i_this->mMode = 0;
    }
}

static void b_gos_albwTryNotifyLastKidCleared(b_gos_class* i_this) {
    if (!dAlbwBoss_fyrusGolemKidsLoose()) {
        return;
    }

    b_go_class* parent = (b_go_class*)fopAcM_SearchByID(i_this->parentActorID);
    if (parent == NULL) {
        return;
    }

    const fpc_ProcID selfId = fopAcM_GetID(i_this);
    for (int i = 0; i < GORON_CHILD_MAX; i++) {
        const fpc_ProcID id = parent->mGoronChildIDs[i];
        if (id == fpcM_ERROR_PROCESS_ID_e || id == selfId) {
            continue;
        }
        if (fopAcM_SearchByID(id) != NULL) {
            return;
        }
    }

    dAlbwBoss_fyrusOnGolemKidsCleared();
}

static void stone_die(b_gos_class* i_this) {
    i_this->speedF = 0.0f;
    i_this->mCyl.OffTgSetBit();
    i_this->mCyl.OffAtSetBit();
    i_this->field_0x8f8.OffAtSetBit();
    i_this->field_0xa30.OffAtSetBit();
    fopAcM_OffStatus(i_this, 0);
    i_this->attention_info.flags = 0;

    switch (i_this->mMode) {
    case 0:
        anm_init(i_this, BCK_GRA_WAIT_AGRA_TO_STONE_NORMAL, 5.0f, J3DFrameCtrl::EMode_NONE, 1.0f);
        i_this->mMode = 1;
        break;
    case 1:
        if (i_this->mpMorf->isStop()) {
            b_gos_albwTryNotifyLastKidCleared(i_this);
            fopAcM_delete(i_this);
        }
        break;
    }
}

static void b_gos_albwSetAttnPos(b_gos_class* i_this) {
    if (!dAlbwBoss_fyrusGolemKidsLoose() || i_this->mAction == ACTION_STONE_DIE ||
        i_this->mAction == ACTION_STICK || i_this->mAction == ACTION_BALL)
    {
        return;
    }

    J3DModel* model = i_this->mpMorf->getModel();
    if (model == NULL) {
        return;
    }

    static DUSK_CONSTEXPR cXyz kEyeOfst(15.0f, 25.0f, 0.0f);
    static DUSK_CONSTEXPR f32 kAttnYOffset = 85.0f;

    cXyz attnBase;
    mDoMtx_stack_c::copy(model->getAnmMtx(4));
    mDoMtx_stack_c::multVec(&kEyeOfst, &i_this->eyePos);
    mDoMtx_stack_c::multVecZero(&attnBase);
    i_this->attention_info.position.set(attnBase.x, attnBase.y + kAttnYOffset, attnBase.z);
    fopAcM_OnStatus(i_this, 0);
    i_this->attention_info.flags = fopAc_AttnFlag_BATTLE_e;
}

static void b_gos_albwRegisterPunchAt(b_gos_class* i_this) {
    if (i_this->mAction != ACTION_PUNCH) {
        return;
    }
    b_gos_albwEnsurePunchCc(i_this);
    J3DModel* model = i_this->mpMorf->getModel();
    if (model == NULL) {
        return;
    }

    cXyz pos;
    cXyz offset(0.0f, 0.0f, 0.0f);
    MTXCopy(model->getAnmMtx(17), *calc_mtx);
    MtxPosition(&offset, &pos);
    i_this->field_0x8f8.SetC(pos);
    i_this->field_0x8f8.SetR(kKidHandR);
    dComIfG_Ccsp()->Set(&i_this->field_0x8f8);

    MTXCopy(model->getAnmMtx(11), *calc_mtx);
    MtxPosition(&offset, &pos);
    i_this->field_0xa30.SetC(pos);
    i_this->field_0xa30.SetR(kKidHandR);
    dComIfG_Ccsp()->Set(&i_this->field_0xa30);
}

static void b_gos_albwSetDamageCyl(b_gos_class* i_this) {
    static DUSK_CONSTEXPR cXyz kGraBodyOfst(0.0f, 0.0f, 20.0f);
    static DUSK_CONSTEXPR f32 kGraBodyH = 280.0f;
    static DUSK_CONSTEXPR f32 kGraBodyR = 100.0f;

    cXyz center;
    mDoMtx_stack_c::transS(i_this->current.pos);
    mDoMtx_stack_c::YrotM(i_this->shape_angle.y);
    mDoMtx_stack_c::multVec(&kGraBodyOfst, &center);
    i_this->mCyl.SetC(center);
    i_this->mCyl.SetH(kGraBodyH);
    i_this->mCyl.SetR(kGraBodyR);
}

// [port_tool substitution] b_gos_class::b_gos_class() is NOT redefined in the mod
// TU. The host owns it (??0b_gos_class@@QEAA@XZ is exported) and emitting it here
// would also emit a local b_gos_class vtable whose inherited fopAc_ac_c /
// fopEn_enemy_c virtuals are NOT in dusklight_exports.def. Nothing in the ported
// chain constructs a b_gos_class; only the host's daB_GOS_Create does.

// ---- NEW helpers (verbatim) ----
// ---- WHOLE fork replacements (verbatim, transformed): hook the stock
//      symbol with HOOK_SKIP_ORIGINAL and call these ----
// [WHOLE replacement] damage_check
static void damage_check(b_gos_class* i_this) {
#if TARGET_PC
    i_this->mCcStts.Move();
    if (!dAlbwBoss_fyrusGolemKidsLoose() || i_this->mAction == ACTION_STONE_DIE) {
        i_this->mCyl.ClrTgHit();
        return;
    }
    if (i_this->field_0x694 != 0) {
        i_this->mCyl.ClrTgHit();
        return;
    }
    if (!i_this->mCyl.ChkTgHit()) {
        return;
    }

    cCcD_Obj* tgHit = i_this->mCyl.GetTgHitObj();
    if (b_gos_albwHitIsShieldBash(tgHit)) {
        i_this->mCyl.ClrTgHit();
        b_gos_albwStartHeadBackFromBash(i_this);
        return;
    }

    i_this->mAtInfo.mpCollider = tgHit;
    cc_at_check(i_this, &i_this->mAtInfo);
    i_this->mCyl.ClrTgHit();
    i_this->field_0x694 = 6;
    if (i_this->health <= 0) {
        i_this->mAction = ACTION_STONE_DIE;
        i_this->mMode = 0;
        i_this->speedF = 0.0f;
    }
#endif
}

// [WHOLE replacement] action
static void action(b_gos_class* i_this) {
    u8 var_r29;
    bool on_cyl_co;
    bool update_pos;

    cXyz speed_offset;
    cXyz move_speed;

    i_this->mAngleToPlayer = fopAcM_searchPlayerAngleY(i_this);
#if TARGET_PC
    if (dAlbwBoss_fyrusGolemKidsLoose()) {
        i_this->mDistToPlayer = b_gos_albwDistToPlayerXZ(i_this);
    } else
#endif
    {
        i_this->mDistToPlayer = fopAcM_searchPlayerDistance(i_this);
    }

    fopAcM_OffStatus(i_this, 0);
    i_this->attention_info.flags = 0;

    on_cyl_co = true;
    update_pos = true;
    var_r29 = 0;

    switch (i_this->mAction) {
    case ACTION_WAIT:
        wait(i_this);
        break;
    case ACTION_WALK:
        walk(i_this);
        break;
    case ACTION_BALL:
        ball(i_this);
        var_r29 = 1;
        break;
    case ACTION_STICK:
        stick(i_this);
        on_cyl_co = false;
        update_pos = false;
        var_r29 = 2;
        break;
#if TARGET_PC
    case ACTION_FIGHT:
        fight(i_this);
        break;
    case ACTION_PUNCH:
        punch(i_this);
        break;
    case ACTION_HEAD_BACK:
        head_back(i_this);
        break;
    case ACTION_TOTTER:
        totter(i_this);
        break;
    case ACTION_STONE_DIE:
        stone_die(i_this);
        on_cyl_co = false;
        break;
#endif
    }

    if (on_cyl_co) {
        i_this->mCyl.OnCoSetBit();
        cXyz* cc_move = i_this->mCcStts.GetCCMoveP();

        if (cc_move != NULL) {
            i_this->current.pos += *cc_move * 0.5f;
        }
    } else {
        i_this->mCyl.OffCoSetBit();
#if TARGET_PC
        if (dAlbwBoss_fyrusGolemWindowIsLive()) {
            i_this->mCyl.OffTgSetBit();
        }
#endif
    }

#if TARGET_PC
    if (dAlbwBoss_fyrusGolemKidsLoose() && i_this->mAction != ACTION_STONE_DIE &&
        i_this->mAction != ACTION_STICK && i_this->mAction != ACTION_BALL)
    {
        i_this->mCyl.OnTgSetBit();
    }
#endif

    cLib_addCalcAngleS2(&i_this->shape_angle.y, i_this->current.angle.y, 2, 0x2000);

    if (update_pos == true) {
        cMtx_YrotS(*calc_mtx, i_this->current.angle.y);

        speed_offset.x = 0.0f;
        speed_offset.y = 0.0f;
        speed_offset.z = i_this->speedF * l_HIO.mSize;

        MtxPosition(&speed_offset, &move_speed);
        i_this->speed.x = move_speed.x;
        i_this->speed.z = move_speed.z;

        i_this->current.pos += i_this->speed;
        i_this->speed.y += i_this->gravity;

        i_this->mAcch.CrrPos(dComIfG_Bgsp());
    }

    if (var_r29 != 0) {
        if (var_r29 == 2) {
            cLib_addCalc2(&i_this->field_0x698, -60, 1, 2);
            cLib_addCalc2(&i_this->field_0x69c, -30, 1, 1);
        }

        if (boss != NULL && boss->field_0x692 == 1) {
            i_this->mAction = ACTION_WAIT;
            i_this->mMode = 0;
            i_this->speedF = cM_rndF(10);
            i_this->speed.y = cM_rndF(10);
#if TARGET_PC
            if (dAlbwBoss_fyrusGolemKidsLoose()) {
                i_this->mTimers[2] = (s16)(cM_rndF(50.0f) + 90.0f);
            }
#endif
        }
    } else {
        cLib_addCalc0(&i_this->field_0x698, 1.0f, 5.0f);
        cLib_addCalc0(&i_this->field_0x69c, 1.0f, 2.5f);

        if (boss != NULL && boss->field_0x692 == 2) {
#if TARGET_PC
            if (!dAlbwBoss_fyrusGolemKidsLoose())
#endif
            {
            i_this->mAction = ACTION_BALL;
            i_this->mMode = 0;
            i_this->speedF = 0.0f;
            }
        }
    }

#if TARGET_PC
    if (dAlbwBoss_fyrusGolemKidsLoose() && i_this->mTimers[2] == 0 &&
        (i_this->mAction == ACTION_WAIT || i_this->mAction == ACTION_WALK))
    {
        if (b_gos_albwPlayerInAggroRange(i_this)) {
            i_this->mAction = ACTION_FIGHT;
            i_this->mMode = 0;
        }
    }
#endif
}

// [WHOLE replacement] daB_GOS_Execute
static int albw_bgos_Execute(b_gos_class* i_this) {
    i_this->field_0x66c++;

    boss = (b_go_class*)fopAcM_SearchByID(i_this->parentActorID);

    for (int i = 0; i < 4; i++) {
        if (i_this->mTimers[i] != 0) {
            i_this->mTimers[i]--;
        }
    }

    if (i_this->field_0x694 != 0) {
        i_this->field_0x694--;
    }

    action(i_this);
    damage_check(i_this);

    mDoMtx_stack_c::transS(i_this->current.pos.x, i_this->current.pos.y, i_this->current.pos.z);
    mDoMtx_stack_c::YrotM(i_this->shape_angle.y);
    mDoMtx_stack_c::XrotM(i_this->shape_angle.x);
    mDoMtx_stack_c::scaleM(l_HIO.mSize, l_HIO.mSize, l_HIO.mSize);
    mDoMtx_stack_c::transM(0.0f, i_this->field_0x698, i_this->field_0x69c);
    i_this->mpMorf->getModel()->setBaseTRMtx(mDoMtx_stack_c::get());

    i_this->mpMorf->play(0, dComIfGp_getReverb(fopAcM_GetRoomNo(i_this)));
    i_this->mpMorf->modelCalc();

#if TARGET_PC
    b_gos_albwSetAttnPos(i_this);
    b_gos_albwRegisterPunchAt(i_this);
    if (dAlbwBoss_fyrusGolemKidsLoose()) {
        b_gos_albwSetDamageCyl(i_this);
    } else {
        i_this->mCyl.SetR(60.0f);
        i_this->mCyl.SetC(i_this->current.pos);
    }
#else
    i_this->mCyl.SetR(60.0f);
    i_this->mCyl.SetC(i_this->current.pos);
#endif
    dComIfG_Ccsp()->Set(&i_this->mCyl);
#if TARGET_PC
    b_gos_albwPunchHitReact(i_this);
#endif
    return 1;
}

// ---- MODIFIED stock fns: ADDED hunks only (place at hook seams) ----