// === AUTO-EXTRACTED by port_tool.py — do not hand-edit the bodies ===
// diag defines forced 0 so scaffolding compiles out:

// ---- pulled fork-local dependencies (verbatim) ----
// ---- NEW helpers (verbatim) ----
// ---- WHOLE fork replacements (verbatim, transformed): hook the stock
//      symbol with HOOK_SKIP_ORIGINAL and call these ----
// [WHOLE replacement] commonBottleDrink
int AlbwPotionLink_c::commonBottleDrink(BOOL param_0) {
    simpleAnmPlay(field_0x0718);
    simpleAnmPlay(field_0x071c);

    daPy_frameCtrl_c* frameCtrl_p;
    u16 current_anm;
    if (param_0 || mUpperAnmHeap[2].getIdx() == dRes_ID_ALANM_BCK_TAKEL_e) {
        frameCtrl_p = &mUpperFrameCtrl[2];
        current_anm = mUpperAnmHeap[2].getIdx();
    } else {
        frameCtrl_p = &mUnderFrameCtrl[0];
        current_anm = mUnderAnmHeap[0].getIdx();
    }

    if (current_anm == getMainBckData(ANM_BOTTLE_DRINK_START)->m_underID) {
        if (checkAnmEnd(frameCtrl_p)) {
            if (param_0) {
                setUpperAnimeBaseMorf(getMainBckData(ANM_BOTTLE_DRINK)->m_upperID, 0.0f);
                setFacePriAnime(ANM_BOTTLE_DRINK);
            } else {
                setSingleAnimeBaseMorf(ANM_BOTTLE_DRINK, 0.0f);
            }

            mItemVar0.mBottleItemHealAmount = 0;

            if (mEquipItem != dItemNo_UGLY_SOUP_e && mEquipItem != dItemNo_CHUCHU_BLACK_e &&
                mEquipItem != dItemNo_CHUCHU_PURPLE_e)
            {
                if (mEquipItem == dItemNo_LV1_SOUP_e) {
                    dComIfGp_setItemLifeCount(8, 0);
                } else if (mEquipItem == dItemNo_LV2_SOUP_e) {
                    dComIfGp_setItemLifeCount(16, 0);
                } else if (mEquipItem == dItemNo_LV3_SOUP_e) {
                    dComIfGp_setItemLifeCount(32, 0);
                } else if (mEquipItem == dItemNo_BEE_CHILD_e) {
                    field_0x2fd3 = 3;
                    dComIfGp_setItemLifeCount(dComIfGp_getSelectItemNum(mSelectItemId), 0);
                } else if (mEquipItem == dItemNo_RED_BOTTLE_e || mEquipItem == dItemNo_CHUCHU_RED_e ||
                           mEquipItem == dItemNo_RED_BOTTLE_2_e)
                {
#if TARGET_PC
                    dComIfGp_setItemLifeCount(
                        dAlbwPotion_getHealQuarters(mSelectItemId, mEquipItem), 0);
#else
                    dComIfGp_setItemLifeCount(32, 0);
#endif
                } else if (mEquipItem == dItemNo_MILK_BOTTLE_e || mEquipItem == dItemNo_HALF_MILK_BOTTLE_e) {
                    dComIfGp_setItemLifeCount(12, 0);
                } else if (mEquipItem == dItemNo_BLUE_BOTTLE_e || mEquipItem == dItemNo_CHUCHU_BLUE_e ||
                           mEquipItem == dItemNo_HOT_SPRING_e)
                {
                    dComIfGp_setItemLifeCount(dComIfGs_getMaxLifeGauge(), 0);
                } else if (mEquipItem == dItemNo_CHUCHU_RARE_e || mEquipItem == dItemNo_FAIRY_DROP_e) {
                    dComIfGp_setItemLifeCount(dComIfGs_getMaxLifeGauge(), 0);
                // ============================================
                // NEW CODE — ALBW Port
                // Green potion / green chu jelly restore 1/3 of the ALBW
                // stamina meter. Originally intended to restore the cut magic
                // meter; now restores the stamina meter in the same role.
                // ============================================
#if TARGET_PC
                } else if (mEquipItem == dItemNo_GREEN_BOTTLE_e || mEquipItem == dItemNo_CHUCHU_GREEN_e) {
                    albw_meter_add_base_fraction(1, 3);
#endif
                }
                // ============================================
                // NEW CODE ENDS HERE
                // ============================================
            }
            voiceStart(Z2SE_AL_V_DRINK);
        } else if (frameCtrl_p->checkPass(50.0f)) {
            field_0x071c->setFrame(0.0f);
            mHeldItemModel->getModelData()->entryTexMtxAnimator(field_0x071c);
        }
    } else if (current_anm == getMainBckData(ANM_BOTTLE_DRINK)->m_underID) {
        if (dComIfGp_getItemNowLife() == dComIfGs_getLife()) {
            mpHookTipModel = NULL;

            if (mEquipItem == dItemNo_MILK_BOTTLE_e) {
                if (param_0) {
                    setUpperAnimeParam(getMainBckData(ANM_BOTTLE_DRINK_END)->m_upperID, UPPER_2,
                                       &mpHIO->mItem.mBottle.m.mEndDrinkAnm);
                    setFacePriAnime(ANM_BOTTLE_DRINK_END);
                } else {
                    setSingleAnimeParam(ANM_BOTTLE_DRINK_END,
                                        &mpHIO->mItem.mBottle.m.mEndDrinkAnm);
                }

                field_0x072c->setFrame(1.0f);
                if (mDemo.getDemoType() == daPy_demo_c::DEMO_TYPE_SPECIAL_e) {
                    dComIfGs_setEquipBottleItemIn(mSelectItemId, dItemNo_HALF_MILK_BOTTLE_e);
                }
            } else if (mProcVar2.field_0x300c == 0) {
                mProcVar2.field_0x300c = 1;
                mHeldItemModel->getModelData()->entryTexMtxAnimator(field_0x0720);
            } else if (simpleAnmPlay(field_0x0720)) {
                if (mEquipItem == dItemNo_UGLY_SOUP_e) {
                    if (dComIfGs_getLife() <= 2) {
                        mItemVar0.mBottleItemHealAmount = dComIfGs_getMaxLifeGauge();
                    } else if (dComIfGs_getLife() <= 4) {
                        mItemVar0.mBottleItemHealAmount = (dComIfGs_getMaxLifeGauge() / 2) - dComIfGs_getLife();
                    } else if (dComIfGs_getLife() == dComIfGs_getMaxLifeGauge() || cM_rnd() < 0.5f) {
                        mItemVar0.mBottleItemHealAmount = -4;
                    } else {
                        mItemVar0.mBottleItemHealAmount = 4;
                    }
                } else if (mEquipItem == dItemNo_CHUCHU_PURPLE_e) {
                    f32 rnd = cM_rndF(6.0f);
                    if (rnd < 1.0f) {
                        mItemVar0.mBottleItemHealAmount = dComIfGs_getMaxLifeGauge();
                    } else if (rnd < 2.0f) {
                        mItemVar0.mBottleItemHealAmount = -(dComIfGs_getLife() - 1);
                        if (mItemVar0.mBottleItemHealAmount == 0) {
                            mItemVar0.mBottleItemHealAmount = -1;
                        }
                    } else if (rnd < 4.0f) {
                        mItemVar0.mBottleItemHealAmount = -4;
                    } else {
                        mItemVar0.mBottleItemHealAmount = 4;
                    }
                } else if (mEquipItem == dItemNo_CHUCHU_BLACK_e) {
                    mItemVar0.mBottleItemHealAmount = -4;
                }

                if (mItemVar0.mBottleItemHealAmount < 0 || mEquipItem == dItemNo_BEE_CHILD_e) {
                    if (param_0) {
                        setUpperAnimeParam(getMainBckData(ANM_BOTTLE_DRINK_NASTY)->m_upperID,
                                           UPPER_2, &mpHIO->mItem.mBottle.m.mDrinkNastyAnm);
                        setFacePriAnime(ANM_BOTTLE_DRINK_NASTY);
                    } else {
                        setSingleAnimeParam(ANM_BOTTLE_DRINK_NASTY,
                                            &mpHIO->mItem.mBottle.m.mDrinkNastyAnm);
                    }
                } else if (param_0) {
                    setUpperAnimeParam(getMainBckData(ANM_BOTTLE_DRINK_END)->m_upperID, UPPER_2,
                                       &mpHIO->mItem.mBottle.m.mEndDrinkAnm);
                    setFacePriAnime(ANM_BOTTLE_DRINK_END);
                } else {
                    setSingleAnimeParam(ANM_BOTTLE_DRINK_END,
                                        &mpHIO->mItem.mBottle.m.mEndDrinkAnm);
                }

                field_0x072c->setFrame(2.0f);
                if (mDemo.getDemoType() == daPy_demo_c::DEMO_TYPE_SPECIAL_e) {
#if TARGET_PC
                    if (dAlbwPotion_isSoulboundRedInSlot(
                            dComIfGs_getSelectItemIndex(mSelectItemId)))
                    {
                        dAlbwPotion_consumeSoulboundDrink(mSelectItemId);
                    } else
#endif
                    {
                        dComIfGs_setEquipBottleItemEmpty(mSelectItemId);
                    }
                }

                if (mEquipItem == dItemNo_CHUCHU_RARE_e || mEquipItem == dItemNo_FAIRY_DROP_e) {
                    mSwordUpTimer = 510;
                }
            }
        }
    } else if (current_anm == getMainBckData(ANM_BOTTLE_DRINK_END)->m_underID ||
               current_anm == getMainBckData(ANM_BOTTLE_DRINK_NASTY)->m_underID)
    {
        if (checkAnmEnd(frameCtrl_p)) {
            if (mDemo.getDemoMode() == daPy_demo_c::DEMO_UNK_42_e) {
                setSingleAnimeParam(ANM_PICK_UP, &mpHIO->mItem.mPickUp.m.mPlaceAnm);
            } else {
                allUnequip(mKeepItem == dItemNo_KANTERA_e);
            }
        } else if (frameCtrl_p->checkPass(11.0f)) {
            mHeldItemModel->getModelData()->entryTexMtxAnimator(field_0x0718);
        } else if (mItemVar0.mBottleItemHealAmount >= 0 && frameCtrl_p->checkPass(21.0f)) {
            if (mItemVar0.mBottleItemHealAmount > 0) {
                dComIfGp_setItemLifeCount(mItemVar0.mBottleItemHealAmount, 0);
            }

            voiceStart(Z2SE_AL_V_DRINK_2);
        } else if (mItemVar0.mBottleItemHealAmount < 0) {
            if (frameCtrl_p->checkPass(25.0f)) {
                voiceStart(Z2SE_AL_V_DRINK_DAMAGE);
            } else if (frameCtrl_p->checkPass(17.0f)) {
                mDoAud_seStart(Z2SE_AL_DAMAGE_SMALL, NULL, 0, 0);

                if (dComIfGs_getLife() <= -mItemVar0.mBottleItemHealAmount) {
                    dComIfGp_setItemLifeCount(-(dComIfGs_getLife() - 1), 0);
                } else {
                    dComIfGp_setItemLifeCount(mItemVar0.mBottleItemHealAmount, 0);
                }
            }
        }
    } else if (current_anm == getMainBckData(ANM_PICK_UP)->m_underID) {
        if (checkAnmEnd(frameCtrl_p)) {
            resetUpperAnime(UPPER_2, 3.0f);
            dCam_getBody()->EndEventCamera(fopAcM_GetID(this));
            return 1;
        } else if (frameCtrl_p->checkPass(9.0f)) {
            returnKeepItemData();
        }
    } else if (mEquipItem == dItemNo_NONE_e || mEquipItem == dItemNo_KANTERA_e) {
        resetUpperAnime(UPPER_2, 3.0f);
        if (mEquipItem == dItemNo_NONE_e) {
            returnKeepItemData();
        }

        resetSpecialEvent();
        return 1;
    }

    return 0;
}

// ---- MODIFIED stock fns: ADDED hunks only (place at hook seams) ----