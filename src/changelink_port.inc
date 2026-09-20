// === AUTO-EXTRACTED from fork d_a_alink_wolf.inc:334-948 (changeLink) by hand-run extractor ===
// Verbatim fork body; only the method qualifier is renamed daAlink_c:: -> AlbwChangeLink_c::.
// D_ALBW_ARC_LIFECYCLE_DEBUG is forced 0 in changelink.cpp so the fork debug probes compile out.

void AlbwChangeLink_c::changeLink(int param_0) {
    JKRHeap* current_heap = mAnmHeap3.setAnimeHeap();
#if TARGET_PC
    if (dAlbwOutfit_isSumoWorn() && dAlbwSumoTest_prepareChangeLink()) {
        onNoResetFlg2(FLG2_UNK_200000);
    }
#endif
    offNoResetFlg2(daPy_FLG2(FLG2_UNK_100000 | FLG2_UNK_80000));
    mMagicArmorBodyBrk = NULL;
    mMagicArmorHeadBrk = NULL;

    J3DModelData* modelData;

    if (checkNoResetFlg2(FLG2_UNK_200000)) {
        offNoResetFlg2(FLG2_UNK_200000);
        onNoResetFlg2(FLG2_UNK_80000);

        modelData = static_cast<J3DModelData*>(dAlbwSumoTest_sumoRes(0x31));  // 3b: private slot, global fallback
        mpLinkModel = mDoExt_J3DModel__create(modelData, 0x80000, 0x11000084);
#if TARGET_PC
        // Sumo Outfit "Link Hat": load Link's cap (Kmdl al_head.bmd) instead of
        // the sumo headpiece (alSumou 0x33).  On a re-apply while already sumo,
        // Kmdl may not be resident (the clothes-change skips the arc reload when
        // the sumo flag is set), so getObjectRes can return NULL; fall back to
        // the sumo headpiece (kept resident by the sumo module) so we never build
        // a model from a null J3DModelData and crash.
        // WIP: the cap loads but does not render on the BLS body (material/draw
        // issue, not transform — position is correct via body head joint 4).
        if (dAlbwSumoTest_wantLinkCap()) {
            // Cap Wear: the cap model + arc come from game.capWear -- green al_head/Kmdl,
            // red ml_head/Mmdl, blue zl_head/Zmdl.  The sumo module keeps the chosen arc resident
            // on any base (resourcesReady cap donor); a NULL here = not-yet-resident -> topknot.
            const char* capArc  = dAlbwSumoTest_capArcName();
            const char* capName = dAlbwSumoTest_capModelName();
            // 3b: prefer the PRIVATE independent cap (gen-swapped -> tracks the Custom Models
            // toggle).  Fall back to the global donor arc (prior behavior) until it is resident.
            J3DModelData* capData = dAlbwSumoTest_independentCapData();
            if (capData == NULL) {
                capData = static_cast<J3DModelData*>(dComIfG_getObjectRes(capArc, capName));
            }
            if (capData == NULL) {
                capData = static_cast<J3DModelData*>(dAlbwSumoTest_sumoRes(0x33));  // 3b topknot fallback
            }
            mpLinkHatModel = initModel(capData, 0);
        } else
#endif
        {
            mpLinkHatModel =
                initModel(static_cast<J3DModelData*>(dAlbwSumoTest_sumoRes(0x33)), 0);  // 3b topknot
        }
        mpLinkHandModel = initModel(static_cast<J3DModelData*>(dAlbwSumoTest_sumoRes(0x32)), 0);  // 3b hand
#if TARGET_PC
        // Link Hat: shape visibility lives on the shared al_head.bmd J3DModelData,
        // and a prior use of the model may have left shapes hidden.  Force every
        // cap-model shape visible so the cap renders.
        if (dAlbwSumoTest_wantLinkCap() && mpLinkHatModel != NULL &&
            mpLinkHatModel->getModelData()->getJointNum() > 6) {
            J3DModelData* capMd = mpLinkHatModel->getModelData();
            for (u16 caps = 0; caps < capMd->getShapeNum(); caps++) {
                capMd->getShapeNodePointer(caps)->show();
            }
        }
#endif
    } else if (checkCasualWearFlg()) {
        onNoResetFlg2(FLG2_UNK_100000);

        mpLinkModel = initModel(static_cast<J3DModelData*>(dComIfG_getObjectRes(l_bArcName, "bl.bmd")), 0);
        mpLinkHatModel =
            initModel(static_cast<J3DModelData*>(dComIfG_getObjectRes(l_bArcName, "bl_head.bmd")), 0);
        mpLinkHandModel =
            initModel(static_cast<J3DModelData*>(dComIfG_getObjectRes(l_bArcName, "bl_hands.bmd")), 0);
    } else if (checkZoraWearFlg()) {
        mpLinkModel = initModel(static_cast<J3DModelData*>(dComIfG_getObjectRes(l_zArcName, "zl.bmd")), 0);
        mpLinkHatModel =
            initModel(static_cast<J3DModelData*>(dComIfG_getObjectRes(l_zArcName, "zl_head.bmd")), 0);
        mpLinkHandModel =
            initModel(static_cast<J3DModelData*>(dComIfG_getObjectRes(l_zArcName, "al_hands.bmd")), 0);
    } else if (checkMagicArmorWearFlg()) {
        J3DModelData* bodyData =
            static_cast<J3DModelData*>(dComIfG_getObjectRes(l_mArcName, "ml.bmd"));
        J3DModelData* headData =
            static_cast<J3DModelData*>(dComIfG_getObjectRes(l_mArcName, "ml_head.bmd"));
        J3DModelData* handData =
            static_cast<J3DModelData*>(dComIfG_getObjectRes(l_mArcName, "al_hands.bmd"));
        bool useMmdlFlags = bodyData != NULL && headData != NULL && handData != NULL;
        // Tell draw() whether this is the REAL Magic model.  On the Kmdl fallback below,
        // the body is Link's Hero's body but the wear flag stays Magic; draw() must NOT
        // run its Magic-only material/Brk ops on it (it reads off the end and crashes).
        s_albwMagicModelReady = useMmdlFlags;
        if (!useMmdlFlags) {
            // Log the Mmdl residency state so the ROOT (why ml.bmd isn't resolvable on a
            // shop-buy under churn) is visible: a registered-but-null archive == freed /
            // mid-read (aliasing class); not-registered == never loaded.
            dRes_info_c* mmdlInfo = g_dComIfG_gameInfo.mResControl.getObjectResInfo(l_mArcName);
            DuskLog.debug("ALBW-LIFE changeLink Magic FALLBACK: missing Mmdl part in {} "
                          "(body={} head={} hand={}) resInfo={} archive={}",
                          l_mArcName, (void*)bodyData, (void*)headData, (void*)handData,
                          (void*)mmdlInfo, (void*)(mmdlInfo != NULL ? mmdlInfo->getArchive() : NULL));
            bodyData = static_cast<J3DModelData*>(dComIfG_getObjectRes(l_kArcName, "al.bmd"));
            headData = static_cast<J3DModelData*>(dComIfG_getObjectRes(l_kArcName, "al_head.bmd"));
            handData = static_cast<J3DModelData*>(dComIfG_getObjectRes(l_kArcName, "al_hands.bmd"));
        }
        if (bodyData != NULL) {
            mpLinkModel = initModel(bodyData, useMmdlFlags ? 0x1000000 : 0);
        }
        if (headData != NULL) {
            mpLinkHatModel = initModel(headData, useMmdlFlags ? 0x1000000 : 0);
        }
        if (handData != NULL) {
            mpLinkHandModel = initModel(handData, 0);
        }

#if TARGET_PC
        if (dusk::getSettings().game.armorRupeeDrain.getValue() == dusk::MagicArmorMode::ALBW) {
            // MODIFIED - ALBW Magic Armor exposure batch (user-arbitrated divergence):
            // repower threshold 500 -> 1. All five threshold sites move together
            // (here; clothes_pipeline.cpp P2a arm x2; meter.cpp armor gate x2).
            // Costs STAY -500 (clamped at 0) and deity 5000/2500 is untouched, so
            // a 1-499-rupee block empties the wallet - intended.
            if (!dMeter2_isALBWArmorDepleted() && dComIfGs_getRupee() >= 1) {
                if (setMagicArmorBrk(1)) {
                    mMagicArmorBodyBrk->setFrame(mMagicArmorBodyBrk->getFrameMax());
                    mMagicArmorHeadBrk->setFrame(mMagicArmorHeadBrk->getFrameMax());
                }
            } else if (setMagicArmorBrk(0)) {
                mMagicArmorBodyBrk->setFrame(mMagicArmorBodyBrk->getFrameMax());
                mMagicArmorHeadBrk->setFrame(mMagicArmorHeadBrk->getFrameMax());
            }
        } else if (dComIfGs_getRupee() != 0 ||
                   dusk::getSettings().game.armorRupeeDrain.getValue() != dusk::MagicArmorMode::NORMAL)
        {
            if (setMagicArmorBrk(1)) {
                mMagicArmorBodyBrk->setFrame(mMagicArmorBodyBrk->getFrameMax());
                mMagicArmorHeadBrk->setFrame(mMagicArmorHeadBrk->getFrameMax());
            }
        } else if (setMagicArmorBrk(0)) {
            mMagicArmorBodyBrk->setFrame(mMagicArmorBodyBrk->getFrameMax());
            mMagicArmorHeadBrk->setFrame(mMagicArmorHeadBrk->getFrameMax());
        }
#else
        if (dComIfGs_getRupee() != 0) {
            if (setMagicArmorBrk(1)) {
                mMagicArmorBodyBrk->setFrame(mMagicArmorBodyBrk->getFrameMax());
                mMagicArmorHeadBrk->setFrame(mMagicArmorHeadBrk->getFrameMax());
            }
        } else if (setMagicArmorBrk(0)) {
            mMagicArmorBodyBrk->setFrame(mMagicArmorBodyBrk->getFrameMax());
            mMagicArmorHeadBrk->setFrame(mMagicArmorHeadBrk->getFrameMax());
        }
#endif
    } else {
        mpLinkModel = initModel(static_cast<J3DModelData*>(dComIfG_getObjectRes(l_kArcName, "al.bmd")), 0);
        mpLinkHatModel =
            initModel(static_cast<J3DModelData*>(dComIfG_getObjectRes(l_kArcName, "al_head.bmd")), 0);
        mpLinkHandModel =
            initModel(static_cast<J3DModelData*>(dComIfG_getObjectRes(l_kArcName, "al_hands.bmd")), 0);
    }

#if TARGET_PC
    // ============================================
    // NEW CODE — ALBW Port (GLOBAL Cap Wear — non-sumo override)
    // The native cascade above built this outfit's OWN hat (mpLinkHatModel).  Apply the global
    // Cap Wear setting (game.capWear) so the chosen cap/topknot shows on EVERY native outfit, not
    // just the sumo body (the sumo branch near the top owns its own cap).  Skip the sumo body
    // (FLG2_UNK_80000 -- handled there) and wolf.  The chosen arc is held resident off its owning
    // base by the sumo module's non-sumo cap donor (nativeCapResourcesReady); a NULL here means
    // not-yet-resident, so we LEAVE the native hat and let the outfit module self-heal
    // (dAlbwAlink_nativeCapResolved) re-fire one rebuild once the donor lands.  We never build
    // mpLinkHatModel from a NULL J3DModelData.  Risk #1 (a foreign cap's materials read off the
    // end in setWaterDropColor) is bounded by ALBW_HAT_TEV / getMaterialNum (commit 55c30fb496).
    // ============================================
    s_albwNativeCapResolved = true;  // default: Off / sumo / wolf -> nothing to override
    if (dusk::getSettings().game.capWear.getValue() != dusk::CapWearMode::Off &&
        !checkNoResetFlg2(FLG2_UNK_80000) && !checkWolf()) {
        const bool wantTopknot = dAlbwSumoTest_wantTopknot();
        const char* capArc  = wantTopknot ? "alSumou" : dAlbwSumoTest_capArcName();
        const char* capName = wantTopknot ? "topknot(0x33)" : dAlbwSumoTest_capModelName();
        (void)capArc;
        (void)capName;
        // None uses the resident alSumou topknot; Green/Red/Blue come from the model-agnostic
        // INDEPENDENT loader (private array+heap, base-independent, no aliasing).  NULL while it
        // loads / on failure -> keep the native hat (self-heal re-fires once it's ready).
        J3DModelData* capData =
            wantTopknot ? static_cast<J3DModelData*>(dAlbwSumoTest_sumoRes(0x33))  // 3b: private, tracks toggle
                        : dAlbwSumoTest_independentCapData();

        if (capData != NULL) {
            // On a Magic base the cascade bound mMagicArmorHeadBrk to the ml_head we are
            // replacing; drop the head Brk so draw()'s Magic head shimmer does not animate the
            // orphaned model (simpleAnmPlay null-guards).  The body Brk stays (the body is intact).
            mMagicArmorHeadBrk = NULL;
            mpLinkHatModel = initModel(capData, 0);
            // Shape visibility lives on the shared J3DModelData; a prior use may have hidden
            // shapes.  Force the cap's shapes visible (mirrors the sumo branch).
            J3DModelData* capMd = mpLinkHatModel->getModelData();
            for (u16 cs = 0; cs < capMd->getShapeNum(); cs++) {
                capMd->getShapeNodePointer(cs)->show();
            }
        } else {
            s_albwNativeCapResolved = false;  // not resident yet -> keep native hat; self-heal later
        }
    }
#endif

    // ============================================
    // NEW CODE — ALBW Port (Sumo Outfit equipment attach)
    // The alSumou "BLS" skeleton places the hand / weapon locators at different
    // joint indices than Link's normal body: HANDL=9, HANDR=14, WEAPONL=10,
    // WEAPONR=15.  Without this remap the sword/shield/held items attach to the
    // wrong BLS joints and read as invisible on the sumo body.  Override ONLY for
    // the sumo body — for normal clothes the game sets these joints dynamically
    // (per equipped item), so we must not clobber them or every outfit swap puts
    // the sword at the legs.  On revert the game restores them itself.
    // ============================================
    if (checkNoResetFlg2(FLG2_UNK_80000)) {
        mLeftHandJntNo  = 9;   // BLS_JNT_HANDL
        mRightHandJntNo = 14;  // BLS_JNT_HANDR
        mLeftItemJntNo  = 10;  // BLS_JNT_WEAPONL
        mRightItemJntNo = 15;  // BLS_JNT_WEAPONR
    }
    // ============================================
    // NEW CODE ENDS HERE
    // ============================================

    J3DModelData* faceData = NULL;
#if TARGET_PC
    // ============================================
    // NEW CODE — ALBW Port (Sumo Outfit face)
    // The sumo body has no face of its own (alSumou 0x33 is only the topknot), so it
    // borrows Link's standard al_face.bmd.  Source it from the RESIDENT BASE clothes
    // arc (mArcName): Bmdl (Ordon), Kmdl (Hero's) and Mmdl (Magic) all ship al_face,
    // so over those bases the sumo face is correct with no extra residency.  ONLY the
    // Zora base (Zmdl) lacks al_face — it has just zl_face.bmd, whose chin/neck
    // geometry reads as the "chin strap" on the sumo body — so there alone we fall
    // back to Kmdl's al_face (the sumo module keeps Kmdl resident for the Zora base
    // only; see d_albw_sumo_test.cpp resourcesReady()/releaseFaceDonor()).
    //
    // Why gate the Kmdl borrow to the Zora base: the resource manager is refcounted by
    // arc NAME, but Link's clothes pipeline frees its arc heap with mpArcHeap->freeAll()
    // (bypassing the refcount).  Over a Hero's base the pipeline's Kmdl lives in that
    // heap, so a second sumo-side refcount on "Kmdl" dangles when the heap is freed —
    // the dual-Kmdl crash / chin-strap regression.  Over the Zora base the pipeline
    // holds Zmdl instead, so our Kmdl is an independent entry and never aliases it.
    // ============================================
    if (checkNoResetFlg2(FLG2_UNK_80000)) {
        // 3b: prefer the private Kmdl al_face (tracks the Custom Models toggle, base-independent).
        faceData = static_cast<J3DModelData*>(dAlbwSumoTest_sumoFaceData());
        if (faceData == NULL) {
            // Fallback (private Kmdl not resident yet / feature off): the base-arc al_face.
            if (checkZoraWearFlg()) {
                faceData = static_cast<J3DModelData*>(dComIfG_getObjectRes(l_kArcName, "al_face.bmd"));
            } else {
                faceData = static_cast<J3DModelData*>(dComIfG_getObjectRes(mArcName, "al_face.bmd"));
            }
        }
    }
#endif
    if (faceData == NULL) {
        if (checkNoResetFlg2(FLG2_UNK_200000) || !checkZoraWearFlg()) {
            faceData = static_cast<J3DModelData*>(dComIfG_getObjectRes(mArcName, "al_face.bmd"));
        } else {
            faceData = static_cast<J3DModelData*>(dComIfG_getObjectRes(mArcName, "zl_face.bmd"));
        }
    }
    mpLinkFaceModel = initModel(faceData, 0x20200);

#ifdef TARGET_PC
    // Update Adult Link's eye maxLOD to prevent the eyes from disappearing
    {
        J3DTexture* tex = mpLinkFaceModel->getModelData()->getTexture();
        JUTNameTab* nametable = mpLinkFaceModel->getModelData()->getTextureName();
        if (tex != nullptr && nametable != nullptr) {
            for (u16 i = 0; i < tex->getNum(); i++) {
                const char* tex_name = nametable->getName(i);
                if (tex_name != nullptr &&
                    (strcmp(tex_name, "al_eyeball") == 0 || strcmp(tex_name, "highlight02") == 0 ||
                     strcmp(tex_name, "eye_kage01") == 0))
                {
                    ResTIMG* timg = tex->getResTIMG(i);
                    timg->maxLOD = 0;
                }
            }
        }
    }
#endif

    u16 i;
#if TARGET_PC
    // ============================================
    // NEW CODE — ALBW Port (WW iron-boots skin via Custom Model API — Layer B)
    // Try a loose per-arc WW boot override "<arc>_<al_bootsH index>.bmd" from an enabled Custom
    // Models folder (e.g. "Wind Waker Skins"). al_bootsH sits at index 0xD in Kmdl/Mmdl and 0xC
    // in Bmdl/Zmdl. try_load returns NULL (folder disabled / file absent) -> vanilla al_bootsH.
    // The re-rigged BMD carries al_bootsH's exact 4-joint skeleton, so the vanilla per-foot rig
    // below drives it with zero special-casing (standard initModel + normal clothes draw path).
    // When the WW boot loads, s_albwWwBootsSkinned flips true so modelDraw() renders it with the
    // get-item cel lighting recipe (struct-0 + ambient-only) instead of MAJI, which crushes the
    // WW cel materials to black (the get-item / held-bow saga).
    // ============================================
    s_albwWwBootsSkinned = false;
    const int bootsHIdx = (mArcName == l_bArcName || mArcName == l_zArcName) ? 0xC : 0xD;
    modelData = dusk::custom_assets::try_load(mArcName, bootsHIdx);
    if (modelData != NULL) {
        s_albwWwBootsSkinned = true;
    }
    if (modelData == NULL)
#endif
    {
        modelData = static_cast<J3DModelData*>(dComIfG_getObjectRes(mArcName, "al_bootsH.bmd"));
    }

    for (i = 0; i < 2; i++) {
        mpLinkBootModels[i] = initModel(modelData, 0);
    }

    mpKanteraModel = initModelEnv(static_cast<J3DModelData*>(dComIfG_getObjectRes(mArcName, "al_kantera.bmd")), 0);

    modelData = static_cast<J3DModelData*>(dComIfG_getObjectRes(mArcName, "ef_ktGlow.bmd"));
    mpKanteraGlowModel = initModel(modelData, 0x200);

    mpKanteraGlowBtk = static_cast<J3DAnmTextureSRTKey*>(dComIfG_getObjectRes(mArcName, "ef_ktGlow.btk"));
    mpKanteraGlowBtk->setFrame(0.0f);
    mpKanteraGlowBtk->searchUpdateMaterialID(modelData);
    modelData->entryTexMtxAnimator(mpKanteraGlowBtk);

    mWoodSwordModel = initModel(static_cast<J3DModelData*>(dComIfG_getObjectRes(mArcName, "al_SWB.bmd")), 0);

#if TARGET_PC
    // Clothes models just (re)built from the current arc heap — stamp the live epoch and the
    // wear/sumo state they were built for so draw()'s consistency guard treats them as valid.
    s_albwClothesModelEpoch = s_albwArcEpoch;
    // changeLink always builds human Link — stamp wolf=false even during metamorphose
    // (FLG1_IS_WOLF may still be set until changeCommon() below).
    s_albwBuiltModelState = albwModelStateToken(false,
        checkNoResetFlg2(FLG2_UNK_80000), checkCasualWearFlg(), checkZoraWearFlg(),
        checkMagicArmorWearFlg());

    // Build-time corruption DETECTOR (see albwFirstCorruptMat): scan every clothes model the
    // instant it is built.  Logs full context on a corrupt build and flags it so draw() skips
    // Link until the next clean rebuild (no crash, session keeps running to collect samples).
    {
        struct { const char* name; J3DModel* m; } l_mdls[] = {
            {"body", mpLinkModel},   {"face", mpLinkFaceModel}, {"hat", mpLinkHatModel},
            {"hand", mpLinkHandModel}, {"boot0", mpLinkBootModels[0]},
            {"boot1", mpLinkBootModels[1]}, {"kantera", mpKanteraModel},
            {"wsword", mWoodSwordModel},
        };
        s_albwBuildIntegrityOk = true;
        for (int l_i = 0; l_i < (int)(sizeof(l_mdls) / sizeof(l_mdls[0])); l_i++) {
            int l_bad = albwFirstCorruptMat(l_mdls[l_i].m);
            if (l_bad != -1) {
                s_albwBuildIntegrityOk = false;
#if D_ALBW_ARC_LIFECYCLE_DEBUG
                J3DModelData* l_bmd = l_mdls[l_i].m ? l_mdls[l_i].m->getModelData() : NULL;
                // Bad material's PEBlock VALUE + its offset from md: a small offset (e.g. a few
                // KB, or near 0) means an UNRELOCATED offset -> the arc wasn't fully loaded/
                // patched when changeLink built (load-completion race); a wild ~GB-off pointer
                // means it points at freed/aliased data (stale).  Decides which fix.
                void* l_peb = NULL;
                intptr_t l_pebOff = 0;
                if (l_bmd != NULL && l_bad >= 0 && (u16)l_bad < l_bmd->getMaterialNum() &&
                    l_bmd->getMaterialNodePointer((u16)l_bad) != NULL) {
                    l_peb = (void*)l_bmd->getMaterialNodePointer((u16)l_bad)->getPEBlock();
                    l_pebOff = (intptr_t)l_peb - (intptr_t)l_bmd;
                }
                DuskLog.debug("ALBW-LIFE *** BUILD-CORRUPT *** model={} badMat={} matNum={} md={} "
                              "peb={} pebOff={} mArcName={} prevArc={} flg80000={} arc={} kmdl={} "
                              "heap={} build#={}",
                              l_mdls[l_i].name, l_bad, (l_bmd ? l_bmd->getMaterialNum() : 0),
                              (void*)l_bmd, l_peb, (long long)l_pebOff,
                              (mArcName ? mArcName : "(null)"),
                              (s_albwPrevArcName ? s_albwPrevArcName : "(null)"),
                              checkNoResetFlg2(FLG2_UNK_80000) ? 1 : 0, albwArcArchive(mArcName),
                              albwArcArchive(l_kArcName), (void*)mpArcHeap, s_albwBuildCounter);
#endif
            }
        }
        s_albwPrevArcName = mArcName;
        s_albwBuildCounter++;
    }
#endif

#if TARGET_PC && D_ALBW_ARC_LIFECYCLE_DEBUG
    // TEMP: build snapshot — ties each rebuilt Link model to the live archive pointers of
    // the arcs it could have been built from (mArcName / Kmdl / alSumou).  A later FREEALL
    // or releaseFaceDonor line freeing one of those archives, followed by the modelDraw
    // "skip corrupt" line naming the same model pointer, pinpoints the exact stale
    // free->build->draw handoff.  STRIP before push (see D_ALBW_ARC_LIFECYCLE_DEBUG).
    {
        J3DModelData* bMd = (mpLinkModel     != NULL) ? mpLinkModel->getModelData()     : NULL;
        J3DModelData* fMd = (mpLinkFaceModel != NULL) ? mpLinkFaceModel->getModelData() : NULL;
        J3DModelData* hMd = (mpLinkHatModel  != NULL) ? mpLinkHatModel->getModelData()  : NULL;
        J3DModelData* nMd = (mpLinkHandModel != NULL) ? mpLinkHandModel->getModelData() : NULL;
        // The crash is always the HAND in getFog (mPEBlock->getFog) — log the hand's matNum
        // and material(0)'s PEBlock AT BUILD.  If the PEBlock is already wild here, the model
        // is corrupt at build (load/relocate race); if it's sane here but the matching
        // handDraw line below shows it wild, something stomps it after build.
        u16 nMat = (nMd != NULL) ? nMd->getMaterialNum() : 0xFFFF;
        void* nPEB = (nMd != NULL && nMat > 0 && nMat <= 256)
                         ? (void*)nMd->getMaterialNodePointer(0)->getPEBlock() : NULL;
        DuskLog.debug("ALBW-LIFE changeLink BUILD flg80000={} mArcName={} arc[m={} K={} sumo={}] "
                      "body={}/{} face={}/{} hat={}/{} hand={}/{} handMat={} handPEB={}",
                      checkNoResetFlg2(FLG2_UNK_80000) ? 1 : 0, (mArcName ? mArcName : "(null)"),
                      albwArcArchive(mArcName), albwArcArchive(l_kArcName), albwArcArchive("alSumou"),
                      (void*)mpLinkModel, (void*)bMd, (void*)mpLinkFaceModel, (void*)fMd,
                      (void*)mpLinkHatModel, (void*)hMd, (void*)mpLinkHandModel, (void*)nMd,
                      nMat, nPEB);
    }
    // Cutscene-anim crash probe: dump the body anm-pack slots at the rebuild.  setDemoBodyBck
    // binds a demo-arc BCK into a pack slot; the blend calc walks all 3.  resetDemoBck only
    // re-sets slot 0 (via ANM_WAIT), so a demo BCK left in under1/upper1/upper2 survives the
    // cutscene -> freed when the demo arc frees -> J3DMtxCalcAnimation null crash on rebuild.
    // Compare these to the "setDemoBodyBck" pointers to spot the lingering freed slot.  STRIP.
    DuskLog.debug("ALBW-LIFE ANMPACK under0={} under1={} under2={} upper0={} upper1={} upper2={}",
                  (void*)getNowAnmPackUnder(UNDER_0), (void*)getNowAnmPackUnder(UNDER_1),
                  (void*)getNowAnmPackUnder(UNDER_2), (void*)getNowAnmPackUpper(UPPER_0),
                  (void*)getNowAnmPackUpper(UPPER_1), (void*)getNowAnmPackUpper(UPPER_2));
#endif

    mDoExt_setCurrentHeap(current_heap);

    dComIfGs_setTransformStatus(0);
    mpLinkModel->setUserArea((uintptr_t)this);
    mpLinkHatModel->setUserArea((uintptr_t)this);
    changeModelDataDirect(param_0);
#if TARGET_PC
    // ============================================
    // NEW CODE — ALBW Port (root-motion foot-tracking reset on clothes/transform rebuild)
    // field_0x33a0 is the per-frame foot-displacement "speed" (root motion): it measures how
    // far the foot joint moved vs the stored previous foot position (field_0x37b0).  A model
    // rebuild makes the foot joint discontinuous vs the stored previous -> the "foot moved"
    // distance comes out absurd (hundreds-to-thousands of units) -> speedF explodes -> Link
    // teleports (the 0.7 smoothing latches it; only while the stick drives movement).
    // Arm the foot-reseed window so setFootSpeed zeroes the spike and re-anchors field_0x37b0
    // to the new model's foot in LOCAL space over the next few frames.  We do NOT write
    // current.pos into field_0x37b0 here (the earlier fix did): current.pos is WORLD space but
    // field_0x37b0/sp18 are LOCAL (mInvMtx * getAnmMtx(foot)), so that just swapped one garbage
    // delta for another and did not fix the teleport.
    // ============================================
    s_albwFootReseedFrames = 3;
    field_0x33a0 = 0.0f;
#endif
    field_0x06e4 = NULL;

    u8 var_r27;
    if (checkNoResetFlg2(FLG2_UNK_80000)) {
        field_0x06d8 = field_0x064C->getMaterialNodePointer(4)->getShape();
        field_0x06dc = field_0x064C->getMaterialNodePointer(5)->getShape();
        field_0x06e0 = NULL;
        field_0x06e8 = field_0x064C->getMaterialNodePointer(3)->getShape();
        field_0x06ec = NULL;
        field_0x06f0 = field_0x064C->getMaterialNodePointer(2)->getShape();
        var_r27 = 2;
    } else if (checkCasualWearFlg()) {
        field_0x06d8 = field_0x064C->getMaterialNodePointer(3)->getShape();
        field_0x06dc = field_0x064C->getMaterialNodePointer(4)->getShape();
        field_0x06e0 = NULL;
        field_0x06e8 = field_0x064C->getMaterialNodePointer(2)->getShape();
        field_0x06ec = field_0x064C->getMaterialNodePointer(0)->getShape();
        field_0x06f0 = field_0x064C->getMaterialNodePointer(1)->getShape();
        var_r27 = 2;
    } else if (checkZoraWearFlg()) {
        field_0x06d8 = field_0x064C->getMaterialNodePointer(4)->getShape();
        field_0x06dc = field_0x064C->getMaterialNodePointer(5)->getShape();
        field_0x06e0 = field_0x064C->getMaterialNodePointer(8)->getShape();
        field_0x06e4 = field_0x064C->getMaterialNodePointer(9)->getShape();
        field_0x06e4->hide();
        field_0x06e8 = NULL;
        field_0x06ec = field_0x064C->getMaterialNodePointer(2)->getShape();
        field_0x06f0 = field_0x064C->getMaterialNodePointer(11)->getShape();
        var_r27 = 3;
    } else if (checkMagicArmorWearFlg()) {
        field_0x06d8 = field_0x064C->getMaterialNodePointer(4)->getShape();
        field_0x06dc = field_0x064C->getMaterialNodePointer(5)->getShape();
        field_0x06e0 = field_0x064C->getMaterialNodePointer(10)->getShape();
        field_0x06e8 = field_0x064C->getMaterialNodePointer(3)->getShape();
        field_0x06ec = field_0x064C->getMaterialNodePointer(1)->getShape();
        field_0x06f0 = field_0x064C->getMaterialNodePointer(2)->getShape();

#if TARGET_PC
        if (dComIfGs_getRupee() != 0 || dusk::getSettings().game.armorRupeeDrain.getValue() != dusk::MagicArmorMode::NORMAL) {
#else
        if (dComIfGs_getRupee() != 0) {
#endif
            var_r27 = 4;
        } else {
            var_r27 = 5;
        }
    } else {
        field_0x064C->getMaterialNodePointer(16)->getShape()->hide();
        field_0x06d8 = field_0x064C->getMaterialNodePointer(11)->getShape();
        field_0x06dc = field_0x064C->getMaterialNodePointer(12)->getShape();
        field_0x06e0 = field_0x064C->getMaterialNodePointer(6)->getShape();
        field_0x06e8 = field_0x064C->getMaterialNodePointer(8)->getShape();
        field_0x06ec = field_0x064C->getMaterialNodePointer(4)->getShape();
        field_0x06f0 = field_0x064C->getMaterialNodePointer(7)->getShape();
        var_r27 = 0;
    }

    if (field_0x06e0 != NULL) {
        if (checkEquipHeavyBoots()) {
            field_0x06e0->hide();
        } else {
            field_0x06e0->show();

            if (checkZoraWearMaskDraw()) {
                field_0x06e4->show();
            }
        }
    }

    field_0x06d0 = field_0x06d8;
    field_0x06d4 = field_0x06dc;

    modelData = mpLinkHandModel->getModelData();
    for (i = 0; i < 11; i++) {
        modelData->getMaterialNodePointer(i)->getShape()->hide();
    }

    modelData = mpLinkFaceModel->getModelData();
    modelData->getMaterialNodePointer(2)->setMaterialAnm(field_0x2180[0]);
    modelData->getMaterialNodePointer(3)->setMaterialAnm(field_0x2180[1]);
    mEyeHL1.entry(modelData, "highlight02");

    mpKanteraModel->setUserArea((uintptr_t)this);
    mpKanteraModel->getModelData()->getJointNodePointer(1)->setCallBack(daAlink_kandelaarModelCallBack);
    mZ2Link.setLinkState(var_r27);

#if VERSION != VERSION_SHIELD_DEBUG
    if (checkHookshotReadyMaterialOffMode()) {
        setHookshotReadyMaterial();
    }
#endif

    if (param_0) {
        fopAcM_SetMtx(this, mpLinkModel->getBaseTRMtx());
        model = mpLinkModel;
        field_0x2060->offOldFrameFlg();
    } else {
        changeCommon();
        offNoResetFlg1(FLG1_IS_WOLF);
        field_0x2e44.init(this, l_jntColData, mpLinkModel, 18);
        field_0x3088 = 0;
        field_0x302c[7] = -0x3800;
        field_0x3060 = 0;
        field_0x3062 = shape_angle.y;

        mHeight = 180.0f;
        mLinkAcch.SetRoofCrrHeight(mHeight);
        mMaxSpeed = mpHIO->mMove.m.mMaxSpeed;

        l_autoUpHeight = mpHIO->mWallHang.m.auto_walk_height + 0.01f;
        l_autoDownHeight = -l_autoUpHeight;
        field_0x3470 = cM_scos(cM_deg2s(mpHIO->mSlide.m.mSlideAngle));
        field_0x3122 = cM_deg2s(mpHIO->mSlide.m.mClimbAngle);
        field_0x3458 = -120.0f;
        field_0x345c = -200.0f;

        field_0x2e54.changeWaterOffset(mpHIO->mBasic.m.mWaterSurfaceEffectHeight);
        field_0x2e54.changeRippleOffset(mHeight);
        offWolfEyeUp();

        field_0x3420 = 1.0f;
        field_0x30c6 = 35;
        field_0x30a8 = 1;
        field_0x30aa = 16;
        field_0x30ae = 0x2000;
        field_0x3424 = 4.0f;
        field_0x3428 = 35.0f;
        field_0x2f50 = &l_crawlSideOffset;
        field_0x2f54 = &l_crawlTopUpOffset;

        resetWolfAtCollision();
        resetWolfBallGrab();
        deleteEquipItem(FALSE, FALSE);
        field_0x2fb0 = 0;

        for (int i = 0; i < 10; i++) {
            mWolfLockAcKeep[i].clearData();
        }

        mWolfLockNum = 0;
        mLeftHandJntNo = 9;
        mRightHandJntNo = 14;
        mLeftItemJntNo = 10;
        mRightItemJntNo = 15;
        field_0x30c4 = 3;
        field_0x30b4 = 4;
        field_0x30bc = 21;
        field_0x30be = 26;
        field_0x30b6 = 5;
        field_0x32c4[0] = 2;
        field_0x32c4[1] = 16;
        attention_info.field_0xa = 10;

        dCcD_Cyl* cyl_p = mTgCyls;
        for (int i = 0; i < 3; i++, cyl_p++) {
            cyl_p->OnTgShieldFrontRange();
            cyl_p->OnTgSmallShield();
        }

        mMagneBootsTopVec = cXyz::BaseY;

        mAtSph.OnAtSetBit();
        mAtSph.OffTgSetBit();
        mAtSph.OffCoSetBit();

        mpWlMidnaModel = NULL;
        mpWlMidnaMaskModel = NULL;
        mpWlMidnaHandModel = NULL;
        mpWlMidnaHairModel = NULL;
    }

    if (param_0) {
        daMidna_c* midna = daPy_py_c::getMidnaActor();
        if (midna != NULL) {
            // BOUNDARY: fork daMidna_c::resetDemoBck is a fork-only actor method absent
            // from stock; the DUSK already ports its body as the free function
            // albw_midna_reset_demo_bck (clothes_pipeline.cpp / .h). Call that.
            albw_midna_reset_demo_bck(midna);
        }
    }
}
