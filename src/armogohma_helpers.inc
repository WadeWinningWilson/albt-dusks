// === AUTO-EXTRACTED by port_tool.py — do not hand-edit the bodies ===
// diag defines forced 0 so scaffolding compiles out:
#define D_ALBW_ARMO_DEATH_DIAG 0
#define D_ALBW_ARMO_P3_HP_DIAG 0
#define D_ALBW_ARMO_PURSUIT_TEST 0
#define D_ALBW_ARMO_P3_DIAG_NOSWAP 0
#define D_ALBW_ARMO_REVEAL 0

// ---- pulled fork-local dependencies (verbatim) ----
static mDoExt_McaMorfSO* s_gmRevealMorf = NULL;

static bool s_gmRevealActive = false;

static bool s_gmPhase3Active = false;

static bool s_gmPhase3EyeOpen = false;

static int s_gmPhase3HitCount = 0;

static s16 s_gmLaserSweepCenter = 0;

static const int kAlbwArmoP3EyeCycleFrames = 300;

static const s16 kAlbwArmoLaserSweepHalf   = 0x4000;

static const int kAlbwArmoLaserTurns       = 6;

static const f32 kAlbwArmoLaserTurnSpeedup = 0.8f;

static const int kAlbwArmoP3VulnFrames     = 480;

static const int kAlbwArmoP3LaserFrames    = 600;

static const int kAlbwArmoP3LaserFireAt    = 180;

static u8 s_armoPursuitMode = ARMO_PURSUIT_DASH;

static const s16 kAlbwArmoRevealMouthRotZ = -14294;

static daB_GM_HIO_c l_HIO;

static FILE* b_gm_deathDiagOpen() {
    static bool sReset = false;
    char path[512]; path[0] = '\0';
    const char* user = getenv("USERPROFILE");
    if (user && user[0] != '\0')
        snprintf(path, sizeof(path), "%s/Documents/dusklight/albw_armo_death_debug.txt", user);
    else
        strncpy(path, "albw_armo_death_debug.txt", sizeof(path) - 1);
    FILE* fp = fopen(path, sReset ? "a" : "w");
    if (fp == NULL) fp = fopen("albw_armo_death_debug.txt", sReset ? "a" : "w");
    if (fp != NULL) sReset = true;
    return fp;
}

daB_GM_HIO_c::daB_GM_HIO_c() {
    field_0x4 = 0xFF;
    model_size = 1.0f;
    check_size = 1.5f;

    dash_speed = 35.0f;
    dash_anm_speed = 1.0f;
    move_speed = 15.0f;
    move_anm_speed = 1.0f;
    wait_anm_speed = 1.0f;
    range = 1300.0f;

    smoke_prim_R = 205;
    smoke_prim_G = 200;
    smoke_prim_B = 180;
    smoke_env_R = 103;
    smoke_env_G = 100;
    smoke_env_B = 90;
    smoke_alpha = 110;
    smoke_blend = 0.3;

    bend_degree_1 = 1.0f;
    bend_degree_2 = 1.0f;
    bend_degree_3 = -1.0f;

    foot_pos_check = false;
    eye_check = false;
}

static void armo_anm_init(b_gm_class* i_this, int i_anmID, f32 i_morf, u8 i_attr, f32 i_speed) {
    i_this->mpModelMorf->setAnm((J3DAnmTransform*)dComIfG_getObjectRes("B_gm", i_anmID), i_attr, i_morf, i_speed, 0.0f, -1.0f);
    i_this->mAnmID = i_anmID;
}

static void* s_ko_del(void* i_actor, void* i_data) {
    if (fopAcM_IsActor(i_actor) && fopAcM_GetName(i_actor) == fpcNm_E_GM_e && fopAcM_GetParam(i_actor) == 0) {
        ((daE_GM_c*)i_actor)->InstantKill();
    }

    return NULL;
}

// ---- NEW helpers (verbatim) ----
// [NEW helper] b_gm_revealAnchorMouthJoint
static void b_gm_revealAnchorMouthJoint(J3DModel* model, f32 tx, f32 ty, f32 tz) {
    Mtx local;
    cMtx_ZrotS(local, kAlbwArmoRevealMouthRotZ);         // local = RotZ(-78.5)
    local[0][3] = tx;                                    // + baked mouth translation ->
    local[1][3] = ty;                                    //   local = Trans * RotZ (joint-local)
    local[2][3] = tz;
    cMtx_concat(model->getAnmMtx(6), local, *calc_mtx);  // *calc_mtx = body-world * mouth-local
}

// [NEW helper] b_gm_activateReveal
static void b_gm_activateReveal(b_gm_class* i_this) {
#if D_ALBW_ARMO_DEATH_DIAG
    { FILE* fp = b_gm_deathDiagOpen(); if (fp) {
        fprintf(fp, "[reveal] activateReveal entry: morf=%p active=%d model=%p\n",
                (void*)s_gmRevealMorf, (int)s_gmRevealActive,
                (void*)(s_gmRevealMorf ? s_gmRevealMorf->getModel() : NULL)); fclose(fp); } }
#endif
    if (s_gmRevealMorf == NULL || s_gmRevealActive) {
        return;
    }
    const int anm = i_this->mAnmID;
    i_this->mpModelMorf = s_gmRevealMorf;
    // Actor matrix was bound to the vanilla model's baseTRMtx in Create; re-point it
    // at the reveal model so it tracks the actor's position.
    fopAcM_SetMtx((fopAc_ac_c*)i_this, s_gmRevealMorf->getModel()->getBaseTRMtx());
#if D_ALBW_ARMO_DEATH_DIAG
    { FILE* fp = b_gm_deathDiagOpen(); if (fp) {
        fprintf(fp, "[reveal] post SetMtx: modelData=%p mats=%d\n",
                (void*)s_gmRevealMorf->getModel()->getModelData(),
                (int)s_gmRevealMorf->getModel()->getModelData()->getMaterialNum()); fclose(fp); } }
#endif
    // Re-init the pupil-dilation BTK against the reveal model's material list.
    i_this->mpZoomBtk->init(s_gmRevealMorf->getModel()->getModelData(),
                            (J3DAnmTextureSRTKey*)dComIfG_getObjectRes("B_gm", 0x28), 1, 0, 1.0f, 0, -1);
    // Carry the current animation onto the fresh morf so the swap is seamless.
    armo_anm_init(i_this, anm, 0.0f, J3DFrameCtrl::EMode_LOOP, 1.0f);
    s_gmRevealActive = true;
#if D_ALBW_ARMO_DEATH_DIAG
    { FILE* fp = b_gm_deathDiagOpen(); if (fp) { fprintf(fp, "[reveal] activateReveal DONE\n"); fclose(fp); } }
#endif
}

// [NEW helper] b_gm_beginPursuitTest
static void b_gm_beginPursuitTest(b_gm_class* i_this, u8 i_mode) {
    fopAc_ac_c* a_this = (fopAc_ac_c*)i_this;

    // ============================================
    // NEW CODE — ALBW Port (DIAGNOSTIC — remove with the test triggers)
    // Confirms a trigger actually fired and, for EYETEST, whether the orphaned
    // ANM_EYE_TEST resource even exists. Writes albw_armo_pursuit_debug.txt.
    // No log line after a slingshot ⇒ the trigger never fired (slingshot not
    // registering on the body sphere). res=NULL ⇒ anim missing, guard skipped it.
    // ============================================
    {
        static bool sReset = false;
        char path[512];
        path[0] = '\0';
        const char* user = getenv("USERPROFILE");
        if (user && user[0] != '\0') {
            snprintf(path, sizeof(path), "%s/Documents/dusklight/albw_armo_pursuit_debug.txt", user);
        } else {
            strncpy(path, "albw_armo_pursuit_debug.txt", sizeof(path) - 1);
        }
        FILE* fp = fopen(path, sReset ? "a" : "w");
        if (fp == NULL) {
            fp = fopen("albw_armo_pursuit_debug.txt", sReset ? "a" : "w");
        }
        if (fp != NULL) {
            sReset = true;
            void* eyeRes = (i_mode == ARMO_PURSUIT_EYETEST)
                               ? dComIfG_getObjectRes("B_gm", ANM_EYE_TEST)
                               : (void*)1;
            fprintf(fp, "[beginPursuit] mode=%d (0=dash 1=crawl 2=eyetest) eyeTestRes=%p\n",
                    (int)i_mode, eyeRes);
            fclose(fp);
        }
    }
    // ============================================
    // NEW CODE ENDS HERE
    // ============================================

    // Already chasing in this flavor — nothing to do (but allow a mode switch).
    if (i_this->mAction == ACTION_PURSUIT_TEST && s_armoPursuitMode == i_mode) {
        return;
    }

    s_armoPursuitMode = i_mode;
    i_this->mAction = ACTION_PURSUIT_TEST;
    i_this->mMode = 0;
    a_this->current.angle.x = 0;
    a_this->shape_angle.x = 0;
    i_this->field_0x73c.y = 0.0f;
    i_this->mInvincibilityTimer = 30;
}

// [NEW helper] b_gm_pursuit_test
static void b_gm_pursuit_test(b_gm_class* i_this) {
    fopAc_ac_c* a_this = (fopAc_ac_c*)i_this;
    fopAc_ac_c* player = dComIfGp_getPlayer(0);

    const bool eyeTest = (s_armoPursuitMode == ARMO_PURSUIT_EYETEST);
    const bool crawl = (s_armoPursuitMode == ARMO_PURSUIT_CRAWL);
    // EYETEST plays the orphaned ANM_EYE_TEST in place (stationary, faces Link)
    // so it can be observed; CRAWL uses the cut GOMA_MOVE; DASH the floor skitter.
    const int target_anm = eyeTest ? ANM_EYE_TEST : (crawl ? ANM_GOMA_MOVE : ANM_GOMA_DASH);
    const f32 anm_speed = eyeTest ? 1.0f : (crawl ? l_HIO.dash_anm_speed * 0.5f : l_HIO.dash_anm_speed);
    const f32 max_speed = eyeTest ? 0.0f : (crawl ? l_HIO.dash_speed * 0.35f : l_HIO.dash_speed);
    const s16 turn_rate = crawl ? 0x180 : 0x300;

    // Guard the resource: ANM_EYE_TEST is orphaned, so verify it exists before
    // setAnm rather than risk a null-anim fault.
    if (i_this->mAnmID != target_anm &&
        dComIfG_getObjectRes("B_gm", target_anm) != NULL) {
        armo_anm_init(i_this, target_anm, 10.0f, J3DFrameCtrl::EMode_LOOP, anm_speed);
    }

    // Upright on the floor: no pitch, descend to floor height.
    a_this->current.angle.x = 0;
    cLib_addCalc2(&a_this->current.pos.y, 0.0f, 1.0f, 40.0f);

    f32 target_speed = 0.0f;
    if (player != NULL) {
        s16 yaw = cM_atan2s(player->current.pos.x - a_this->current.pos.x,
                            player->current.pos.z - a_this->current.pos.z);
        cLib_addCalcAngleS2(&a_this->current.angle.y, yaw, 4, turn_rate);

        // mPlayerDistance is refreshed at the top of action() each frame.
        if (i_this->mPlayerDistance > 350.0f) {
            target_speed = max_speed;
        }
    }

    cLib_addCalc2(&a_this->speedF, target_speed, 1.0f, l_HIO.dash_speed * 0.5f + 1.0f);
    i_this->mAcch.CrrPos(dComIfG_Bgsp());
}

// [NEW helper] b_gm_beginPhase3
static void b_gm_beginPhase3(b_gm_class* i_this) {
    fopAc_ac_c* a_this = (fopAc_ac_c*)i_this;
    i_this->mAction = ACTION_PHASE3;
    i_this->mMode = P3_INTRO;
    // Do NOT snap the orientation here. The boss is smashed upside-down; the RETURN
    // "get up" anim carries the reorientation (as vanilla does) and we snap upright
    // only when it finishes. A one-frame flip would leave the foot IK targeting the
    // ceiling and blow the model apart -- the "model breaks" seen in playtest.
    i_this->field_0x73c.y = 0.0f;
    i_this->mInvincibilityTimer = 60;
    s_gmPhase3Active = true;
    s_gmPhase3EyeOpen = false;
    s_gmPhase3HitCount = 0;
    armo_anm_init(i_this, ANM_GOMA_RETURN, 5.0f, J3DFrameCtrl::EMode_NONE, 1.0f);
}

// [NEW helper] b_gm_phase3EnterVuln
static void b_gm_phase3EnterVuln(b_gm_class* i_this) {
    // Stagger: stop, stand exposed, eye open the whole 8s. The counter resets when it
    // ends; hits here drain HP but don't re-count. NOTE: LANDING_WAIT is authored for
    // the on-its-back (upside-down) drop pose and sinks under the floor when played
    // upright, so use the upright-safe WAIT (vanilla plays it upright post-RETURN).
    i_this->mMode = P3_VULN;
    i_this->mTimers[0] = kAlbwArmoP3VulnFrames;
    s_gmPhase3EyeOpen = true;
    armo_anm_init(i_this, ANM_GOMA_WAIT, 5.0f, J3DFrameCtrl::EMode_LOOP, l_HIO.wait_anm_speed);
}

// [NEW helper] b_gm_phase3EnterLaser
static void b_gm_phase3EnterLaser(b_gm_class* i_this) {
    // Laser interlude: lock in place, eye INVULNERABLE (field_0x1ad6 stays 0), track
    // Link and fire the lagging beam from t=3s. Do NOT play eye_test.bck on the reveal
    // model (it would yank the baked eye off the mouth) -- a neutral hold pose keeps
    // the eye put; the beam-render block owns the tracking + laser.
    i_this->mMode = P3_LASER;
    i_this->mTimers[0] = kAlbwArmoP3LaserFrames;
    s_gmPhase3EyeOpen = false;
    i_this->field_0x6c0 = 0.0f;   // beam off until the ignite moment
    i_this->field_0x6f4 = 0;      // re-init the beam aim when it lights
    armo_anm_init(i_this, ANM_GOMA_WAIT, 5.0f, J3DFrameCtrl::EMode_LOOP, l_HIO.wait_anm_speed);
}

// [NEW helper] b_gm_beginPhase3Handoff
static void b_gm_beginPhase3Handoff(b_gm_class* i_this) {
    // Final drain hit: instead of snapping to the death, play the get-up (RETURN) in
    // REVERSE so the boss lies back down into the upside-down smashed pose -- the mirror
    // of how it stood up entering phase 3. Orientation stays anim-driven (angle.x
    // untouched), so RETURN's frame-0 pose lands exactly on the pose ANM_GOMA_DEATH
    // continues from. When the reverse finishes (P3_LIEDOWN in b_gm_phase3) we fire the
    // real death handoff (b_gm_beginPhase3Death). Momentum is killed here so it doesn't
    // drift through the lie-down / cutscene (which would drag eyePos -> the E_GM spawn).
    fopAc_ac_c* a_this = (fopAc_ac_c*)i_this;
    a_this->speedF = 0.0f;
    a_this->speed.set(0.0f, 0.0f, 0.0f);
    i_this->field_0x6c0 = 0.0f;         // kill any lingering beam
    s_gmPhase3EyeOpen = false;
    i_this->mInvincibilityTimer = 120;  // ignore late hits through the transition
    i_this->mMode = P3_LIEDOWN;
    // A NEGATIVE rate makes setAnm seek to the last frame and set the loop boundary for
    // reverse playback (m_Do_ext.cpp setAnm) -- the get-up runs backward as a lie-down.
    // (Seeking manually after a positive-rate init left the loop frame at 0 and snapped
    // straight to frame 0 -- the "instant flip".)
    armo_anm_init(i_this, ANM_GOMA_RETURN, 5.0f, J3DFrameCtrl::EMode_NONE, -1.0f);
}

// [NEW helper] b_gm_beginPhase3Death
static void b_gm_beginPhase3Death(b_gm_class* i_this) {
    // Relocated vanilla transition: the same writes the 3rd statue hit made, just
    // fired from the chase once the pool drains AND the boss has laid back down
    // (P3_LIEDOWN). demo_camera(mDemoMode=30) then runs the disappear cutscene and
    // spawns the E_GM floor eye. b_gm_drop no-ops on mMode 20, matching the vanilla
    // post-statue state.
    s_gmPhase3Active = false;
    i_this->field_0x6c0 = 0.0f;   // kill any lingering beam
    i_this->mAction = ACTION_DROP;
    i_this->mMode = 20;
    i_this->mDemoMode = 30;
    fpcM_Search(s_ko_del, i_this);
    Z2GetAudioMgr()->bgmStop(0x1E, 0);
#if D_ALBW_ARMO_DEATH_DIAG
    { FILE* fp = b_gm_deathDiagOpen(); if (fp) {
        fprintf(fp, "[death] handoff fired -> ACTION_DROP mMode=20 mDemoMode=30\n"); fclose(fp); } }
#endif
}

// [NEW helper] b_gm_phase3
static void b_gm_phase3(b_gm_class* i_this) {
    fopAc_ac_c* a_this = (fopAc_ac_c*)i_this;
    fopAc_ac_c* player = dComIfGp_getPlayer(0);

    // Get-up intro: keep the smashed (upside-down) orientation while RETURN plays,
    // then on its final frame snap upright, swap in the revealed model, and start the
    // chase. We stop "early" -- we never continue into the vanilla wall-climb. Handled
    // before the upright logic below so we don't force angle.x = 0 mid-reorient.
    if (i_this->mMode == P3_INTRO) {
        cLib_addCalc2(&a_this->speedF, 0.0f, 1.0f, l_HIO.dash_speed * 0.5f + 1.0f);
        cLib_addCalc2(&a_this->current.pos.y, 0.0f, 1.0f, 40.0f);
        if (i_this->mpModelMorf->isStop()) {
            a_this->current.angle.x = 0;
            a_this->shape_angle.x = 0;
#if D_ALBW_ARMO_P3_DIAG_NOSWAP
            // DIAGNOSTIC (temporary): keep the vanilla model in phase 3 to isolate the
            // upside-down break. If the vanilla body reorients + dashes upright cleanly,
            // the fault is the reveal BMD's export orientation, not this code. Flip the
            // define back to 0 to restore the reveal swap.
#else
            b_gm_activateReveal(i_this);  // reveal only once upright
#endif
            armo_anm_init(i_this, ANM_GOMA_DASH, 10.0f, J3DFrameCtrl::EMode_LOOP, l_HIO.dash_anm_speed * 0.95f);
            i_this->mMode = P3_DASH;
            i_this->mTimers[0] = kAlbwArmoP3EyeCycleFrames;
        }
        i_this->mAcch.CrrPos(dComIfG_Bgsp());
        return;
    }

    // Final lie-down (mirror of the get-up): RETURN is playing in REVERSE (set up in
    // b_gm_beginPhase3Handoff), laying the boss back into the upside-down smashed pose.
    // Hold still; when the reverse reaches frame 0 the body is upside-down and we fire
    // the real death handoff. angle.x stays 0 -- the reversed anim carries the flip, so
    // it lands exactly on the pose ANM_GOMA_DEATH continues from.
    if (i_this->mMode == P3_LIEDOWN) {
        cLib_addCalc2(&a_this->speedF, 0.0f, 1.0f, 5.0f);
        cLib_addCalc2(&a_this->current.pos.y, 0.0f, 1.0f, 40.0f);
        i_this->mInvincibilityTimer = 5;
        if (i_this->mpModelMorf->getFrame() <= 0.5f || i_this->mpModelMorf->isStop()) {
            b_gm_beginPhase3Death(i_this);  // upside-down now -> disappear cutscene -> E_GM
        }
        i_this->mAcch.CrrPos(dComIfG_Bgsp());
        return;
    }

    a_this->current.angle.x = 0;
    cLib_addCalc2(&a_this->current.pos.y, 0.0f, 1.0f, 40.0f);

    s16 yaw = a_this->current.angle.y;
    if (player != NULL) {
        yaw = cM_atan2s(player->current.pos.x - a_this->current.pos.x,
                        player->current.pos.z - a_this->current.pos.z);
    }

    f32 target_speed = 0.0f;

    switch (i_this->mMode) {
    case P3_DASH:
        // Eye blink/open cycle (5s each). field_0x1ad6 (cleared in action() this
        // frame) gates the core sphere -> the open window IS the vulnerable window.
        if (i_this->mTimers[0] == 0) {
            s_gmPhase3EyeOpen = !s_gmPhase3EyeOpen;
            i_this->mTimers[0] = kAlbwArmoP3EyeCycleFrames;
        }
        if (s_gmPhase3EyeOpen) {
            i_this->field_0x1ad6 = 1;
        }
        // Keep chasing even while the eye is open (do NOT freeze). Dash anim slowed 5%.
        if (i_this->mAnmID != ANM_GOMA_DASH &&
            dComIfG_getObjectRes("B_gm", ANM_GOMA_DASH) != NULL) {
            armo_anm_init(i_this, ANM_GOMA_DASH, 10.0f, J3DFrameCtrl::EMode_LOOP, l_HIO.dash_anm_speed * 0.95f);
        }
        cLib_addCalcAngleS2(&a_this->current.angle.y, yaw, 4, 0x300);
        if (i_this->mPlayerDistance > 350.0f) {
            // ALBW: phase-3 chase dash slowed 3% then a further 3% per playtest
            // (0.97 * 0.97 = 0.9409; movement only -- the dash anim runs at x0.95 above).
            target_speed = l_HIO.dash_speed * 0.9409f;
        }
        break;

    case P3_VULN:
        // Stationary, eye exposed the whole stagger. Slow face toward Link.
        i_this->field_0x1ad6 = 1;
        cLib_addCalcAngleS2(&a_this->current.angle.y, yaw, 4, 0x80);
        if (i_this->mTimers[0] == 0) {
            s_gmPhase3HitCount = 0;
            s_gmPhase3EyeOpen = false;
            i_this->mMode = P3_DASH;
            i_this->mTimers[0] = kAlbwArmoP3EyeCycleFrames;
        }
        break;

    case P3_LASER: {
        // Locked, eye invulnerable (never raise field_0x1ad6). Wind-up faces Link; once
        // the beam lights the body SWEEPS IN PLACE through kAlbwArmoLaserTurns alternating
        // 180 deg turns, each completing its arc 20% faster than the last (the whole
        // series scaled to fill the fire window). The eye-mounted beam (fired straight
        // forward, see the beam block) arcs across the arena. Centered on Link at ignite.
        const int elapsed = kAlbwArmoP3LaserFrames - static_cast<int>(i_this->mTimers[0]);
        if (elapsed < kAlbwArmoP3LaserFireAt) {
            cLib_addCalcAngleS2(&a_this->current.angle.y, yaw, 8, 0x435);
            s_gmLaserSweepCenter = a_this->current.angle.y;  // keep centered on Link until ignite
        } else {
            cLib_addCalc2(&i_this->field_0x6c0, 1.0f, 1.0f, 0.105f);
            const int fireDur = kAlbwArmoP3LaserFrames - kAlbwArmoP3LaserFireAt;
            const int fe = elapsed - kAlbwArmoP3LaserFireAt;

            // Geometric turn durations (each *0.8) scaled so all N turns fill fireDur.
            f32 sum = 0.0f, w = 1.0f;
            for (int t = 0; t < kAlbwArmoLaserTurns; t++) { sum += w; w *= kAlbwArmoLaserTurnSpeedup; }
            f32 dur = (f32)fireDur / sum;   // turn-1 duration (frames)

            int k = kAlbwArmoLaserTurns - 1;   // current turn (0-based)
            f32 p = 1.0f;                      // progress within the turn (0..1)
            f32 acc = 0.0f;
            for (int t = 0; t < kAlbwArmoLaserTurns; t++) {
                if ((f32)fe < acc + dur) { k = t; p = ((f32)fe - acc) / dur; break; }
                acc += dur;
                dur *= kAlbwArmoLaserTurnSpeedup;
            }
            if (p < 0.0f) p = 0.0f; else if (p > 1.0f) p = 1.0f;

            // Alternate direction each turn: even turns sweep -90 -> +90, odd +90 -> -90.
            const f32 span = (f32)(2 * (int)kAlbwArmoLaserSweepHalf) * p;
            s16 off;
            if ((k & 1) == 0) {
                off = (s16)(-(f32)kAlbwArmoLaserSweepHalf + span);
            } else {
                off = (s16)((f32)kAlbwArmoLaserSweepHalf - span);
            }
            a_this->current.angle.y = s_gmLaserSweepCenter + off;
        }
        if (i_this->mTimers[0] == 0) {
            i_this->field_0x6c0 = 0.0f;
            i_this->field_0x6f4 = 0;
            s_gmPhase3EyeOpen = false;
            i_this->mMode = P3_DASH;
            i_this->mTimers[0] = kAlbwArmoP3EyeCycleFrames;
        }
        break;
    }
    }

    cLib_addCalc2(&a_this->speedF, target_speed, 1.0f, l_HIO.dash_speed * 0.5f + 1.0f);
    i_this->mAcch.CrrPos(dComIfG_Bgsp());
}

// ---- MODIFIED stock fns: ADDED hunks only (place at hook seams) ----