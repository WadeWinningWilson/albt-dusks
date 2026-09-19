// Extracted moveLife (AlbwMeter2_c) from port_tool (mq_move_life.json). Do not hand-edit.
// [WHOLE replacement] moveLife
void AlbwMeter2_c::moveLife() {
    s16 life_count = 0;
    bool draw_life = false;

    if (dComIfGp_getItemMaxLifeCount() != 0) {
        s16 max_count = dComIfGs_getMaxLife() + dComIfGp_getItemMaxLifeCount();
        if (max_count > 100) {
            max_count = 100;
        } else if (max_count < 15) {
            max_count = 15;
        }

        dComIfGs_setMaxLife(max_count);
#if TARGET_PC
        if (albw_mq_is_enabled()) {
            life_count = static_cast<s16>(dComIfGs_getMaxLifeGauge());
        } else
#endif
        {
            life_count = (max_count / 5) * 4;
        }

        s16 current_life = life_count - dComIfGs_getLife();
        dComIfGp_setItemLifeCount(current_life, 0);
        dComIfGp_clearItemMaxLifeCount();
        draw_life = true;
    }

    f32 item_life_count = dComIfGp_getItemLifeCount();
    f32 tmp = 0.0f;
    if (item_life_count != tmp) {
        mLifeCountType = dComIfGp_getItemLifeCountType();
        if (!draw_life) {
#if TARGET_PC
            if (albw_mq_is_enabled()) {
                life_count = static_cast<s16>(dComIfGs_getMaxLifeGauge());
            } else
#endif
            {
                life_count = (dComIfGs_getMaxLife() / 5) * 4;
            }
        }

        s16 old_life = dComIfGs_getLife();
        s16 new_life = old_life + dComIfGp_getItemLifeCount();
        if (new_life > life_count) {
            new_life = life_count;
        } else if (new_life < 0) {
            new_life = 0;
        }

        dComIfGs_setLife((u8)new_life);
        dComIfGp_clearItemLifeCount();
#if TARGET_PC
        {
            const int delta = static_cast<int>(new_life) - static_cast<int>(old_life);
            if (delta > 0) {
                dParryMaster_onHeal(delta);
            } else if (new_life == 0) {
                dParryMaster_clearQueue();
            }
        }
#endif

        if (mNowLifeGauge == dComIfGs_getLife() && mLifeCountType != 0) {
            mLifeCountType = 0;
        }
        draw_life = true;
    }

    u16 max_life = dComIfGs_getMaxLife();
#if TARGET_PC
    if (albw_mq_is_enabled()) {
        max_life = albw_mq_display_max_internal();
    }
#endif
    if (mMaxLife != max_life) {
        if (mMaxLife < max_life) {
            mMaxLife++;
            dMeter2Info_onLifeGaugeSE();
            draw_life = true;
        } else if (mMaxLife > max_life) {
            mMaxLife--;
            draw_life = true;
        }
    }

    u16 current_life = dComIfGs_getLife();
    if (mNowLifeGauge != current_life) {
        if (mNowLifeGauge < current_life) {
            mNowLifeGauge++;
            if (dComIfGp_checkPlayerStatus1(0, 0x2000) ||
                dComIfGp_checkPlayerStatus0(0, 0x20000000) || dMeter2Info_getLifeGaugeSE())
            {
                if (mNowLifeGauge % 4 == 0) {
                    mDoAud_seStart(Z2SE_HP_GAUGE_INC, 0, 0, 0);
                }
            } else if (mLifeCountType == 1) {
                mDoAud_seStart(Z2SE_HP_GAUGE_INC, 0, 0, 0);
            }

            u16 life = dComIfGs_getLife();
            if (mNowLifeGauge == life && mLifeCountType != 0) {
                mLifeCountType = 0;
            }
            draw_life = true;
        } else if (mNowLifeGauge > current_life) {
            mNowLifeGauge--;
            if (mLifeCountType != 0) {
                mLifeCountType = 0;
            }
            draw_life = true;
        }
    } else if (dMeter2Info_getLifeGaugeSE()) {
        dMeter2Info_offLifeGaugeSE();
    }

    f32 life_gauge_x = g_drawHIO.mLifeGaugePosX;
    f32 life_gauge_y = g_drawHIO.mLifeGaugePosY;

    if (mLifeGaugePosX != g_drawHIO.mLifeGaugePosX) {
        mLifeGaugePosX = g_drawHIO.mLifeGaugePosX;
        draw_life = true;
    }

    if (mLifeGaugePosY != g_drawHIO.mLifeGaugePosY) {
        mLifeGaugePosY = g_drawHIO.mLifeGaugePosY;
        draw_life = true;
    }

#if TARGET_PC
    const f32 lifeGaugeScale =
        g_drawHIO.mLifeParentScale *
        std::clamp(1.0f, 0.5f, 2.0f);
#else
    const f32 lifeGaugeScale = g_drawHIO.mLifeParentScale;
#endif
    if (mLifeGaugeScale != lifeGaugeScale) {
        mLifeGaugeScale = lifeGaugeScale;
        draw_life = true;
    }

    if (mHeartScale != g_drawHIO.mHeartMarkScale) {
        mHeartScale = g_drawHIO.mHeartMarkScale;
        draw_life = true;
    }

    if (mLargeHeartScale != g_drawHIO.mBigHeartScale) {
        mLargeHeartScale = g_drawHIO.mBigHeartScale;
        draw_life = true;
    }

    if (draw_life == true) {
        mpMeterDraw->drawLife(mMaxLife, mNowLifeGauge, life_gauge_x, life_gauge_y);
    }

    alphaAnimeLife();
    dComIfGp_setItemNowLife((u8)mNowLifeGauge);
}

