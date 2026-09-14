// ============================================
// Manual bridges for the whole-function armogohma port (armogohma_port.inc).
// These are the pieces the port tool cannot emit for the Execute-chain closure:
//   - the reveal eye/mouth anchor constants + helper (only nodeCallBack uses them,
//     and nodeCallBack is a joint callback, not reached by the Execute chain, so
//     the tool's dep-closure never pulls them),
//   - nodeCallBack itself (fork foot-IK + reveal eye/lid anchoring, verbatim),
//   - albw_armo_build_reveal_morf (the mod's host-resource reveal-model shim that
//     stands in for the fork's useHeapInit custom-asset load).
// Placed AFTER armogohma_port.inc so they see its s_gm* statics.
// ============================================

static const s16 kAlbwArmoRevealMouthRotZ = -14294;
static const f32 kAlbwArmoEyeRetract      = 200.0f;

// [reveal anchor] fork b_gm_revealAnchorMouthJoint (verbatim).
static void b_gm_revealAnchorMouthJoint(J3DModel* model, f32 tx, f32 ty, f32 tz) {
    Mtx local;
    cMtx_ZrotS(local, kAlbwArmoRevealMouthRotZ);         // local = RotZ(-78.5)
    local[0][3] = tx;                                    // + baked mouth translation ->
    local[1][3] = ty;                                    //   local = Trans * RotZ (joint-local)
    local[2][3] = tz;
    cMtx_concat(model->getAnmMtx(6), local, *calc_mtx);  // *calc_mtx = body-world * mouth-local
}

// [joint callback] fork nodeCallBack (foot IK + phase-3 reveal mouth/eye anchoring),
// verbatim. Bound to every reveal-morf joint so the baked eye tracks the mouth and blinks.
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

// [reveal shim] fork useHeapInit reveal-morf build, host-resource variant
// (per-fight; NULL -> vanilla model, graceful).
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
