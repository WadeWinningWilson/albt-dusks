// ============================================
// Armogohma modified-function seams (extracted by tools/port_tool.py from the
// fork's useHeapInit / damage_check / nodeCallBack, placed here so the file-
// static originals — unhookable by symbol — are reproduced at the daB_GM_Create
// / daB_GM_Execute hook seams). Bodies verbatim; the reveal `#if` is made
// unconditional (this TU only exists when the feature is wanted).
// ============================================

static const f32 kAlbwArmoEyeRetract = 200.0f;
static const int kAlbwArmoP3VulnHitCount = 5;   // fork d_a_b_gm.cpp:121 (hits -> stagger)
static const int kAlbwArmoP3LaserHitCount = 3;  // fork d_a_b_gm.cpp:122 (>this -> laser)

// fork nodeCallBack (foot IK + phase-3 reveal mouth/eye anchoring), verbatim.
// Bound to every reveal-morf joint so the baked eye tracks the mouth and blinks.
static int nodeCallBack(J3DJoint* i_joint, int param_1) {
    if (param_1 == 0) {
        int jnt_no = i_joint->getJntNo();
        J3DModel* model = j3dSys.getModel();

        b_gm_class* a_this = (b_gm_class*)model->getUserArea();
        if (a_this != NULL) {
            MTXCopy(model->getAnmMtx(jnt_no), *calc_mtx);

            if (jnt_no >= 0x19 && jnt_no <= 0x1B) {
                cMtx_ZrotM(*calc_mtx, a_this->mFoot[0].field_0x8[jnt_no - 0x19]);
            } else if (jnt_no >= 0x1D && jnt_no <= 0x1F) {
                cMtx_ZrotM(*calc_mtx, a_this->mFoot[1].field_0x8[jnt_no - 0x1D]);
            } else if (jnt_no >= 0x21 && jnt_no <= 0x23) {
                cMtx_ZrotM(*calc_mtx, a_this->mFoot[2].field_0x8[jnt_no - 0x21]);
            } else if (jnt_no >= 0x25 && jnt_no <= 0x27) {
                cMtx_ZrotM(*calc_mtx, a_this->mFoot[3].field_0x8[jnt_no - 0x25]);
            } else if (jnt_no >= 0x29 && jnt_no <= 0x2B) {
                cMtx_ZrotM(*calc_mtx, a_this->mFoot[4].field_0x8[jnt_no - 0x29]);
            } else if (jnt_no >= 0x2D && jnt_no <= 0x2F) {
                cMtx_ZrotM(*calc_mtx, a_this->mFoot[5].field_0x8[jnt_no - 0x2D]);
            } else if (jnt_no >= 0x31 && jnt_no <= 0x33) {
                cMtx_ZrotM(*calc_mtx, a_this->mFoot[6].field_0x8[jnt_no - 0x31]);
            } else if (jnt_no >= 0x35 && jnt_no <= 0x37) {
                cMtx_ZrotM(*calc_mtx, a_this->mFoot[7].field_0x8[jnt_no - 0x35]);
            } else if (jnt_no >= 0x39 && jnt_no <= 0x3B) {
                cMtx_ZrotM(*calc_mtx, a_this->mFoot[8].field_0x8[jnt_no - 0x39]);
            } else if (jnt_no >= 0x3E && jnt_no <= 0x40) {
                cMtx_ZrotM(*calc_mtx, a_this->mFoot[8].field_0x8[jnt_no - 0x3E]);
            } else if (jnt_no == 0x16) {
                if (s_gmRevealActive && s_gmRevealMorf != NULL && model == s_gmRevealMorf->getModel()) {
                    b_gm_revealAnchorMouthJoint(model, 438.2799f, -48.1853f, -7.1048f);  // eyelidT
                }
                cMtx_ZrotM(*calc_mtx, a_this->field_0x1ad8);
            } else if (jnt_no == 0x17) {
                if (s_gmRevealActive && s_gmRevealMorf != NULL && model == s_gmRevealMorf->getModel()) {
                    b_gm_revealAnchorMouthJoint(model, 437.6509f, -74.1303f, -5.1240f);  // eyelidU
                }
                cMtx_ZrotM(*calc_mtx, -a_this->field_0x1ad8);
            } else if (jnt_no == 0x15) {
                if (s_gmRevealActive && s_gmRevealMorf != NULL && model == s_gmRevealMorf->getModel()) {
                    f32 retract = (6000.0f - (f32)a_this->field_0x1ad8) * (1.0f / 14000.0f);
                    if (retract < 0.0f) retract = 0.0f;
                    else if (retract > 1.0f) retract = 1.0f;
                    b_gm_revealAnchorMouthJoint(model, 455.5993f - kAlbwArmoEyeRetract * retract,
                                                -62.0347f, -7.1048f);  // eye
                }
                cMtx_YrotM(*calc_mtx, a_this->field_0x1ada);
                cMtx_ZrotM(*calc_mtx, a_this->field_0x1adc);
            }

            model->setAnmMtx(jnt_no, *calc_mtx);
            MTXCopy(*calc_mtx, J3DSys::mCurrentMtx);
        }
    }
    return 1;
}

// fork useHeapInit reveal-morf build (per-fight; NULL -> vanilla model, graceful).
static void albw_armo_build_reveal_morf(b_gm_class* a_this) {
    s_gmRevealMorf = NULL;
    if (!dAlbwBossRefinement_isEnabled()) {
        return;
    }
    J3DModelData* revealData = albw_armo_load_reveal();
    if (revealData == NULL) {
        return;
    }
    mDoExt_McaMorfSO* rm = JKR_NEW mDoExt_McaMorfSO(
        revealData, NULL, NULL, (J3DAnmTransform*)dComIfG_getObjectRes("B_gm", 30), 2, 0.6f, 0, -1,
        &a_this->mCreatureSound, 0, 0x11000284);
    if (rm != NULL && rm->getModel() != NULL) {
        J3DModel* rmodel = rm->getModel();
        rmodel->setUserArea((uintptr_t)a_this);
        for (u16 j = 0; j < rmodel->getModelData()->getJointNum(); j++) {
            rmodel->getModelData()->getJointNodePointer(j)->setCallBack(nodeCallBack);
        }
        s_gmRevealMorf = rm;
    }
}

// fork damage_check ALBW routing (the phase-3 core drain + the reveal statue
// trigger that calls beginPhase3). Reproduced from the daB_GM_Execute pre-hook
// BEFORE vanilla damage_check runs; consumed hits are ClrTgHit'd so vanilla
// skips them (no double cc_at_check). Returns true if it consumed this frame.
static bool albw_armo_damage_route(b_gm_class* i_this) {
    if (!dAlbwBossRefinement_isEnabled() || i_this->mInvincibilityTimer != 0) {
        return false;
    }
    fopAc_ac_c* a_this = (fopAc_ac_c*)i_this;

    // --- core drain: the eye only registers while open (field_0x1ad6 gates it),
    //     so a core hit here IS a valid open-eye hit during phase 3.
    if ((i_this->mAction != ACTION_DROP || (i_this->mMode != 2 && i_this->mMode != 3)) &&
        i_this->mCoreSph.ChkTgHit() && s_gmPhase3Active)
    {
        i_this->mAtInfo.mpCollider = i_this->mCoreSph.GetTgHitObj();
        cc_at_check(i_this, &i_this->mAtInfo);
        i_this->mInvincibilityTimer = 8;
        bool p3handoff =
            dAlbwBoss_armogohmaPhase3Damage(a_this, i_this->mAtInfo.mAttackPower);
        if (p3handoff) {
            b_gm_beginPhase3Handoff(i_this);
        } else if (i_this->mMode == P3_DASH) {
            s_gmPhase3HitCount++;
            if (s_gmPhase3HitCount >= kAlbwArmoP3VulnHitCount) {
                b_gm_phase3EnterVuln(i_this);
            } else if (s_gmPhase3HitCount > kAlbwArmoP3LaserHitCount) {
                b_gm_phase3EnterLaser(i_this);
            }
        }
        cXyz p3_pos;
        MtxP p3_mtx = mDoMtx_stack_c::get();
        MTXCopy(i_this->mpModelMorf->getModel()->getAnmMtx(0x15), p3_mtx);
        mDoMtx_stack_c::multVecZero(&p3_pos);
        for (int p3 = 0; p3 < 3; p3++) {
            static u16 p3name[] = {0x8B8C, 0x8B8D, 0x8B8E};
            dComIfGp_particle_set(p3name[p3], &p3_pos, NULL, NULL);
        }
        i_this->mCoreSph.ClrTgHit();
        return true;
    }

    // --- reveal statue trigger: 2 statue hits (vs 3) -> beginPhase3.
    if (i_this->mAction == ACTION_DROP && (i_this->mMode == 2 || i_this->mMode == 3) &&
        s_gmRevealMorf != NULL)
    {
        for (int i = 0; i < 2; i++) {
            if (!i_this->mBodySph[i].ChkTgHit()) {
                continue;
            }
            cCcD_Obj* obj = i_this->mBodySph[i].GetTgHitObj();
            if (obj == NULL || !obj->ChkAtType(AT_TYPE_CSTATUE_BOSS_SWING)) {
                continue;
            }
            i_this->mHitCount++;
            dComIfGs_onOneZoneSwitch(4, -1);
            Z2GetAudioMgr()->changeBgmStatus(2);
            const int rodMax = 2;  // reveal fight: 2 statue hits
            if (i_this->mHitCount >= rodMax) {
                dAlbwBoss_armogohmaOnRodHit(a_this, i_this->mHitCount);
                b_gm_beginPhase3(i_this);
            } else {
                dAlbwBoss_armogohmaOnRodHit(a_this, i_this->mHitCount);
                i_this->mMode = 10;
                i_this->field_0x1ad5 = 2;
                i_this->mDemoMode = 20;
            }
            dComIfGp_getVibration().StartShock(VIBMODE_S_POWER8, 0x4F, cXyz(0.0f, 1.0f, 0.0f));
            for (int j = 0; j < 4; j++) {
                static u16 name[] = {0x8AF6, 0x8AF7, 0x8AF8, 0x8AF9};
                dComIfGp_particle_set(name[j], &a_this->current.pos, NULL, NULL);
            }
            i_this->mBodySph[i].ClrTgHit();
            return true;
        }
    }
    return false;
}

// ============================================
// fork daB_GM_Execute reveal additions (the D_ALBW_ARMO_REVEAL block), reproduced
// as a POST-Execute body. Three fork behaviors the stock Execute lacks:
//   (1) phase-3 eye (mCoreSph): widen + accept ANY player weapon (vanilla Tg is a
//       single narrow rod/bomb bit, so sword/arrows never registered on the eye);
//       while the DASH anim plays, the open eye also DAMAGES Link (contact hurt),
//       the VULN stagger drops the At bit so up-close hits there are free.
//   (2) phase-3 legs (mFootSph): widen + DAMAGE Link in the floor chase.
//   (3) reveal lid drive (field_0x1ad8): the stretched reveal lids need a deeper
//       close (-8000) to cover the relocated eye, and the laser keeps the eye
//       visually open. nodeCallBack reads field_0x1ad8 at draw, so driving it here
//       (post-Execute) is in time.
// The member spheres were registered to Ccsp by pointer during stock Execute this
// frame; their At/Tg/radius are read at end-of-frame CrossAllTg, so setting them
// here still lands. All gated on s_gmPhase3Active -> the vanilla ceiling/wall fight
// is untouched. Values verbatim from the fork.
// ============================================
static void albw_armo_apply_reveal_execute(b_gm_class* i_this) {
    if (i_this == NULL || !dAlbwBossRefinement_isEnabled() || !s_gmPhase3Active) {
        return;
    }
    const f32 sph_base_size = l_HIO.check_size * l_HIO.model_size;

    // (1) Eye (core) sphere.
    i_this->mCoreSph.SetR((JREG_F(11) + 200.0f) * sph_base_size);
    i_this->mCoreSph.SetTgType(0xd8fbfdff);  // any player damage, not just rod/bomb
    if (i_this->mAnmID == ANM_GOMA_DASH) {
        i_this->mCoreSph.SetAtType(AT_TYPE_CSTATUE_SWING);
        i_this->mCoreSph.SetAtAtp(16);  // ~4 hearts; eye hurts Link while dashing
        i_this->mCoreSph.OnAtSetBit();
    } else {
        i_this->mCoreSph.OffAtSetBit();  // VULN stagger = free hits
    }

    // (2) Legs (foot spheres): widen + damage Link (~2 hearts) in the floor chase.
    for (int i = 0; i < 8; i++) {
        i_this->mFootSph[i].SetR((TREG_F(13) + 90.0f) * sph_base_size);
        i_this->mFootSph[i].SetAtAtp(8);
    }

    // (3) Reveal lid drive (reveal-deep close + laser open exception).
    bool lidOpen = s_gmPhase3EyeOpen || (i_this->mMode == P3_LASER);
    if (lidOpen) {
        cLib_addCalcAngleS2(&i_this->field_0x1ad8, 6000, 4, 800);
    } else {
        s16 shutTarget = s_gmRevealActive ? (s16)(BREG_S(7) - 8000) : (s16)(BREG_S(7) - 3900);
        cLib_addCalcAngleS2(&i_this->field_0x1ad8, shutTarget, 1, 800);
    }
}
