// ============================================
// NEW CODE — ALBW Port (WW Deku Leaf glide) — part of dev.albt.albw.
//
// Port of the fork's Deku Leaf feature (d_a_alink.cpp:374-846 + the procAutoJump /
// execute / draw seams and the f_ap_game R+A trigger). See deku_leaf.h for the design
// note: the fork re-gated the INLINE checkGrabGlide() (uneditable in the stock exe), so
// here we hook the OUT-OF-LINE checkGrabRooster() it inlines and force it TRUE while the
// leaf is out — the whole native cucco glide chassis then engages for the leaf. Only the
// leaf-specific behaviour (R+A takeoff lift, aerial bomb drop, meter cost, 1.20x glide
// speed, canopy model) is overlaid via hooks.
//
// The daAlink_c methods the fork wrote are adapted to free functions on `link`; every
// member/method they touch is public in stock d_a_alink.h.
// ============================================

#include "deku_leaf.h"

#include "global.h"
#include "albw_common.h"
#include "config_vars.h"
#include "meter_bridge.h"
#include "modules.h"
#include "mods/svc/hook.hpp"
#include "mods/svc/resource.h"
#include "mods/svc/log.h"

#include "d/actor/d_a_alink.h"
#include "d/actor/d_a_player.h"
#include "d/d_com_inf_game.h"
#include "d/d_resorce.h"
#include "d/d_particle_name.h"
#include "m_Do/m_Do_ext.h"
#include "m_Do/m_Do_mtx.h"
#include "m_Do/m_Do_controller_pad.h"
#include "SSystem/SComponent/c_math.h"
#include "f_pc/f_pc_name.h"
#include "f_op/f_op_actor_mng.h"
#include "Z2AudioLib/Z2SeMgr.h"
#include "JSystem/JKernel/JKRExpHeap.h"
#include "albw_symbols.h"

#if TARGET_PC

// svc_resource is defined (IMPORT_SERVICE) in armogohma.cpp — reference that single
// definition here rather than importing a second copy (LNK2005 otherwise).
extern const ResourceService* svc_resource;

namespace {

bool leaf_on() { return albw_cfg_bool(g_deku_leaf, false); }

// ============================================
// Bomb spawn bridge. dBomb_c::createNormalBombPlayer is an inline that calls
// fopAcM_fastCreate, which is not link-importable in the mod — resolve it by symbol at
// runtime (the armogohma pattern) and replicate the two bomb create calls.
// ============================================
using LeafFastCreateFn = fopAc_ac_c* (*)(s16, u32, const cXyz*, int, const csXyz*, const cXyz*,
                                         s8, int (*)(void*), void*, u32, u8);

fopAc_ac_c* leaf_create_bomb(u32 params, cXyz* pos) {
    static LeafFastCreateFn fn = nullptr;
    if (fn == nullptr && svc_hook != nullptr) {
        void* addr = nullptr;
        if (svc_hook->resolve(mod_ctx, ALBT_SYM_FASTCREATE, &addr, nullptr) == MOD_OK) {
            fn = reinterpret_cast<LeafFastCreateFn>(addr);
        }
    }
    if (fn == nullptr) {
        return nullptr;
    }
    return fn(fpcNm_NBOMB_e, params, pos, -1, nullptr, nullptr, -1, nullptr, nullptr, 0, 0xFF);
}

// ============================================
// Glide state (fork d_a_alink.cpp:374-406)
// ============================================
bool s_dekuLeafGlideActive  = false;
bool s_dekuLeafTakeoffPending = false;  // handed to procAutoJumpInit so it opens glided
bool s_dekuLeafTakeoffRising  = false;  // R+A still lifting this airtime
f32  s_dekuLeafTakeoffBaseY   = 0.0f;   // launch altitude, for the (disabled) height cap

// Tuning. Lift starts at Moon Jump parity (56.0f); forward drift is a deliberate drift.
f32 s_dekuLeafTakeoffLift     = 56.0f;
f32 s_dekuLeafTakeoffFwd      = 6.0f;
f32 s_dekuLeafTakeoffFwdAccel = 0.1f;
f32 s_dekuLeafTakeoffMaxRise  = 0.0f;   // 0 = uncapped (shipped shape)
// The leaf flies 20% faster than the vanilla cucco glide (playtested).
const f32 s_dekuLeafGlideSpeedScale = 1.20f;

// Takeoff gust emitters (fork d_a_alink.cpp:424-436).
u32 s_dekuLeafGustFxId[3]         = {0, 0, 0};
s16 s_dekuLeafGustFxTimer         = 0;
s16 s_dekuLeafGustFxLifeFrames    = 300;  // ~5s at 60fps: the outer deadline
s16 s_dekuLeafGustFxEmitStopAfter = 30;   // stop EMITTING this many frames in

u8  s_dekuLeafBombCooldown       = 0;
u8  s_dekuLeafBombCooldownFrames = 20;     // spam guard, in frames
f32 s_dekuLeafBombDropOffsetY    = -20.0f; // spawn just under Link's feet
f32 s_dekuLeafBombDropSpeedY     = -10.0f; // a push down, not a drop from rest
f32 s_dekuLeafBombDropSpeedF     = 3.0f;   // slight forward throw along the glide

// ============================================
// Model (fork d_a_alink.cpp:633-846). Bundled BMD loaded armogohma-style.
// ============================================
J3DModelData* s_dekuLeafModelData = nullptr;
ResourceBuffer s_dekuLeafBuf = RESOURCE_BUFFER_INIT;
J3DModel* s_dekuLeafModel = nullptr;
s16 s_dekuLeafBillowPhase = 0;

// Hand-weld: pin the two grip ribs (jnt 2 -> LEFT hand, jnt 4 -> RIGHT hand) and freeze
// billow on the grip strand; the free ribs flutter (fork P3e, confirmed-working).
cXyz s_dekuLeafLHandW(0.0f, 0.0f, 0.0f);
cXyz s_dekuLeafRHandW(0.0f, 0.0f, 0.0f);
cXyz s_dekuLeafTipLocal2(-17.98f, 30.07f, 17.93f);  // jnt 2 (Larm2 -> RIGHT hand)
cXyz s_dekuLeafTipLocal4(8.35f, 39.61f, 10.00f);    // jnt 4 (Rarm2 -> LEFT  hand)

// Overhead weld orientation — the confirmed-working trio (fork d_a_alink.cpp:741-757).
cXyz s_dekuLeafOffset(0.0f, 0.0f, 0.0f);
f32  s_dekuLeafScale = 1.0f;
s16 s_dekuLeafBaseRotY = 0xC000;   // turn the leaf to face front about the vertical axis
s16 s_dekuLeafBaseRotX = -0x4000;  // -90 deg X (the confirmed-good orientation)
s16 s_dekuLeafSpin     = 0x4000;   // 90 deg spin, bring the grip ribs around to the hands
s16 s_dekuLeafFlipX    = 0x0000;   // disabled

// ============================================
// Predicates
// ============================================
// The engine's own "leaf out AND engaged" test (fork checkDekuLeafGlide).
bool leaf_check_glide() { return leaf_on() && s_dekuLeafGlideActive; }

// The REAL rooster test (fork checkGrabRooster body). Our checkGrabRooster POST hook
// forces the engine's copy TRUE during glide, so our own logic must use this instead of
// link->checkGrabRooster() to know whether an actual cucco/TKJ2 is being carried.
bool leaf_real_rooster(daAlink_c* link) {
    fopAc_ac_c* ac = link->mGrabItemAcKeep.getActor();
    if (ac != nullptr && (fopAcM_GetName(ac) == fpcNm_NI_e ||
                          fopAcM_GetName(ac) == fpcNm_NPC_TKJ2_e)) {
        return true;
    }
    return false;
}

bool leaf_metamorphose_active(daAlink_c* link) {
    return link->mProcID == daAlink_c::PROC_METAMORPHOSE ||
           link->mProcID == daAlink_c::PROC_METAMORPHOSE_ONLY;
}

int leaf_assignable_button_count() {
    return albw_cfg_bool(g_extra_item_slot_enabled, false) ? 3 : 2;
}

// ============================================
// updateDekuLeafGustFx (fork d_a_alink.cpp:442)
// ============================================
void leaf_update_gust_fx() {
    if (s_dekuLeafGustFxTimer == 0) {
        return;
    }
    s_dekuLeafGustFxTimer--;

    const s16 elapsed = s_dekuLeafGustFxLifeFrames - s_dekuLeafGustFxTimer;
    if (elapsed == s_dekuLeafGustFxEmitStopAfter) {
        for (int i = 0; i < 3; i++) {
            if (s_dekuLeafGustFxId[i] != 0) {
                dComIfGp_particle_setStopContinue(s_dekuLeafGustFxId[i]);
            }
        }
    }

    if (s_dekuLeafGustFxTimer == 0) {
        for (int i = 0; i < 3; i++) {
            if (s_dekuLeafGustFxId[i] != 0) {
                JPABaseEmitter* emitter = dComIfGp_particle_getEmitter(s_dekuLeafGustFxId[i]);
                if (emitter != nullptr) {
                    emitter->becomeInvalidEmitter();
                }
                s_dekuLeafGustFxId[i] = 0;
            }
        }
    }
}

// ============================================
// updateDekuLeafBombDrop (fork d_a_alink.cpp:470)
// ============================================
void leaf_update_bomb_drop(daAlink_c* link) {
    if (s_dekuLeafBombCooldown != 0) {
        s_dekuLeafBombCooldown--;
        return;
    }

    const int buttonCount = leaf_assignable_button_count();
    for (int i = 0; i < buttonCount; i++) {
        const s32 item = dComIfGp_getSelectItem(i);
        if (item != dItemNo_NORMAL_BOMB_e && item != dItemNo_WATER_BOMB_e) {
            continue;
        }
        if (!link->itemTriggerCheck(1 << i)) {
            continue;
        }
        // Same two refusals the ground path applies: meter first, then the 3-bomb cap.
        if (!albw_meter_can_bomb() || link->mActiveBombNum >= 3) {
            return;
        }

        cXyz pos = link->current.pos;
        pos.y += s_dekuLeafBombDropOffsetY;
        // fastCreate params: 8 = normal player bomb, 9 = water bomb (d_bomb.h).
        fopAc_ac_c* bomb = leaf_create_bomb(item == dItemNo_NORMAL_BOMB_e ? 8 : 9, &pos);
        if (bomb != nullptr) {
            link->mActiveBombNum++;
            albw_meter_on_bomb();
            bomb->speed.y = s_dekuLeafBombDropSpeedY;
            bomb->speed.x = cM_ssin(link->shape_angle.y) * s_dekuLeafBombDropSpeedF;
            bomb->speed.z = cM_scos(link->shape_angle.y) * s_dekuLeafBombDropSpeedF;
            bomb->shape_angle.y = link->shape_angle.y;
            s_dekuLeafBombCooldown = s_dekuLeafBombCooldownFrames;
        }
        return;
    }
}

// ============================================
// stowDekuLeaf (fork d_a_alink.cpp:510): flag, glide bit and gravity move together.
// ============================================
void leaf_stow(daAlink_c* link) {
    s_dekuLeafGlideActive   = false;
    s_dekuLeafTakeoffRising = false;
    link->mProcVar2.field_0x300c = 0;
    link->setSpecialGravity(link->mpHIO->mAutoJump.m.mGravity,
                            link->mpHIO->mAutoJump.m.mMaxFallSpeed, TRUE);
}

// ============================================
// updateDekuLeafGlideState (fork d_a_alink.cpp:523): per-frame truth check + gust tick.
// ============================================
void leaf_update_glide_state(daAlink_c* link) {
    leaf_update_gust_fx();

    if ((s_dekuLeafGlideActive || s_dekuLeafTakeoffRising) && !link->checkCokkoGlide()) {
        leaf_stow(link);
    }
}

// ============================================
// checkDekuLeafTakeoff (fork d_a_alink.cpp:535): may R+A launch right now?
// ============================================
bool leaf_check_takeoff(daAlink_c* link) {
    if (!leaf_on()) {
        return false;
    }
    if (link->checkWolf() || link->checkRideOn() || leaf_metamorphose_active(link)) {
        return false;
    }
    if (leaf_real_rooster(link) || link->mGrabItemAcKeep.getActorConst() != nullptr) {
        return false;
    }
    if (dComIfGp_event_runCheck()) {
        return false;
    }
    if (!link->mLinkAcch.ChkGroundHit()) {
        return false;
    }
    return albw_meter_can_deku_leaf();
}

// ============================================
// startDekuLeafTakeoff (fork d_a_alink.cpp:558): enter the glide chassis with the canopy
// already open. procAutoJumpInit clears the glide flag, so the request is a pending flag
// it consumes (our procAutoJumpInit POST hook does the handoff).
// ============================================
void leaf_start_takeoff(daAlink_c* link) {
    s_dekuLeafTakeoffPending = true;
    link->procAutoJumpInit(0);

    link->mMaxSpeed = link->mpHIO->mAutoJump.m.mCuccoJumpMaxSpeed;
    link->field_0x3478 = link->mpHIO->mAutoJump.m.mCuccoFallMaxSpeed;
    link->setSpecialGravity(-1.0f, link->field_0x3478, FALSE);
    link->mProcVar2.field_0x300c = 1;
    albw_meter_on_deku_leaf_start();

    s_dekuLeafTakeoffRising = true;
    s_dekuLeafTakeoffBaseY = link->current.pos.y;

    // The gust supplies its own lift from frame 1, the way Moon Jump does.
    link->speed.y = s_dekuLeafTakeoffLift;

    // Takeoff gust FX — the gale boomerang's own effects (fork d_a_alink.cpp:579-601).
    cXyz fxPos = link->current.pos;
    csXyz fxAngle(0, link->shape_angle.y, 0);
    s_dekuLeafGustFxId[0] = dComIfGp_particle_set(s_dekuLeafGustFxId[0], ID_ZI_J_SPBOOM_SYOUGEKI_A,
                                                 &fxPos, &link->tevStr, &fxAngle, NULL, 0xFF, NULL,
                                                 -1, NULL, NULL, NULL);
    s_dekuLeafGustFxId[1] = dComIfGp_particle_set(s_dekuLeafGustFxId[1], ID_ZI_J_SPBOOM_SYOUGEKI_B,
                                                 &fxPos, &link->tevStr, &fxAngle, NULL, 0xFF, NULL,
                                                 -1, NULL, NULL, NULL);
    s_dekuLeafGustFxId[2] = dComIfGp_particle_set(s_dekuLeafGustFxId[2], ID_ZI_J_SPBOOM_LEAF_A,
                                                 &fxPos, &link->tevStr, &fxAngle, NULL, 0xFF, NULL,
                                                 -1, NULL, NULL, NULL);
    s_dekuLeafGustFxTimer = s_dekuLeafGustFxLifeFrames;
    link->seStartOnlyReverb(Z2SE_AL_BACKTEN_WIND);
}

// ============================================
// updateDekuLeafTakeoff (fork d_a_alink.cpp:608): sustained lift while R+A held.
// ============================================
void leaf_update_takeoff(daAlink_c* link) {
    const bool held = mDoCPd_c::getHoldR(PAD_1) != 0 && mDoCPd_c::getHoldA(PAD_1) != 0;
    if (!held || !albw_meter_can_deku_leaf()) {
        s_dekuLeafTakeoffRising = false;
        return;
    }
    if (link->mLinkAcch.ChkRoofHit()) {
        s_dekuLeafTakeoffRising = false;
        return;
    }
    if (s_dekuLeafTakeoffMaxRise > 0.0f &&
        (link->current.pos.y - s_dekuLeafTakeoffBaseY) >= s_dekuLeafTakeoffMaxRise) {
        s_dekuLeafTakeoffRising = false;
        return;
    }

    s_dekuLeafTakeoffRising = true;
    link->speed.y = s_dekuLeafTakeoffLift;
}

// ============================================
// dekuLeafJointCB (fork d_a_alink.cpp:675): rib billow + grip-rib hand weld.
// ============================================
int leaf_joint_cb(J3DJoint* i_joint, int param_1) {
    if (param_1 != 0) {
        return 1;
    }
    J3DModel* model = j3dSys.getModel();
    if (model == nullptr) {
        return 1;
    }
    int jnt = i_joint->getJntNo();
    // Grip ribs held by the hands: jnt 2 -> LEFT hand, jnt 4 -> RIGHT hand.
    if (jnt == 2 || jnt == 4) {
        Mtx m;
        MTXCopy(model->getAnmMtx(jnt), m);      // keep the rib's orientation...
        const cXyz& hp = (jnt == 2) ? s_dekuLeafLHandW : s_dekuLeafRHandW;
        // ...and offset the origin so the grip TIP (not the joint) rests in the hand.
        const cXyz& tl = (jnt == 4) ? s_dekuLeafTipLocal4 : s_dekuLeafTipLocal2;
        const f32 ox = m[0][0] * tl.x + m[0][1] * tl.y + m[0][2] * tl.z;
        const f32 oy = m[1][0] * tl.x + m[1][1] * tl.y + m[1][2] * tl.z;
        const f32 oz = m[2][0] * tl.x + m[2][1] * tl.y + m[2][2] * tl.z;
        m[0][3] = hp.x - ox;
        m[1][3] = hp.y - oy;
        m[2][3] = hp.z - oz;
        model->setAnmMtx(jnt, m);
        MTXCopy(m, J3DSys::mCurrentMtx);
        return 1;
    }
    if (jnt == 1 || jnt == 3) {                  // grip-rib inner segment: frozen (no billow)
        MTXCopy(model->getAnmMtx(jnt), J3DSys::mCurrentMtx);
        return 1;
    }
    if (jnt >= 1 && jnt <= 8) {
        Mtx m;
        MTXCopy(model->getAnmMtx(jnt), m);
        const s16 amp = (jnt % 2 == 0) ? 0x0900 : 0x0480;      // tips flap ~2x the arm-roots
        const s16 phase = (s16)(s_dekuLeafBillowPhase + jnt * 0x1800);  // per-arm offset
        const s16 ang = (s16)(amp * cM_ssin(phase));
        cMtx_ZrotM(m, ang);
        model->setAnmMtx(jnt, m);
        MTXCopy(m, J3DSys::mCurrentMtx);
    }
    return 1;
}

// Load the bundled leaf BMD -> J3DModelData (armogohma pattern). Cached: the leaf is
// Link's persistent item, so it stays parsed for the session.
J3DModelData* leaf_load_model_data() {
    if (s_dekuLeafModelData != nullptr) {
        return s_dekuLeafModelData;
    }
    if (svc_resource == nullptr) {
        return nullptr;
    }
    if (svc_resource->load(mod_ctx, "dekuleaf/itemmdl_21.bmd", &s_dekuLeafBuf) != MOD_OK ||
        s_dekuLeafBuf.data == nullptr) {
        if (svc_log != nullptr) svc_log->error(mod_ctx, "deku leaf: itemmdl_21.bmd load failed");
        return nullptr;
    }
    s_dekuLeafModelData = dRes_info_c::loaderBasicBmd('BMDV', s_dekuLeafBuf.data);
    return s_dekuLeafModelData;
}

// ============================================
// updateDekuLeafModel (fork d_a_alink.cpp:759): load/hold the canopy above the hands.
// ============================================
void leaf_update_model(daAlink_c* link) {
    if (s_dekuLeafModel == nullptr) {
        J3DModelData* data = leaf_load_model_data();
        if (data == nullptr) {
            return;  // bundle missing — no leaf this frame
        }
        JKRHeap* gameHeap = mDoExt_getGameHeap();
        JKRHeap* prevHeap = (gameHeap != nullptr) ? mDoExt_setCurrentHeap(gameHeap) : nullptr;
        s_dekuLeafModel = mDoExt_J3DModel__create(data, 0x80000, 0x11000084);
        if (prevHeap != nullptr) {
            mDoExt_setCurrentHeap(prevHeap);
        }
        if (s_dekuLeafModel == nullptr) {
            return;
        }
        for (u16 jn = 1; jn <= 8; jn++) {
            data->getJointNodePointer(jn)->setCallBack(leaf_joint_cb);
        }
    }

    s_dekuLeafBillowPhase += 0x82f;

    // Route B weld-anchor: midpoint of Link's two hand matrices (WORLD).
    cXyz lHand, rHand;
    mDoMtx_multVecZero(link->getLeftHandMatrix(), &lHand);
    mDoMtx_multVecZero(link->getRightHandMatrix(), &rHand);
    s_dekuLeafLHandW = lHand;
    s_dekuLeafRHandW = rHand;
    cXyz anchor((lHand.x + rHand.x) * 0.5f, (lHand.y + rHand.y) * 0.5f,
                (lHand.z + rHand.z) * 0.5f);
    mDoMtx_stack_c::transS(anchor.x, anchor.y, anchor.z);
    mDoMtx_stack_c::YrotM(link->shape_angle.y);
    mDoMtx_stack_c::transM(s_dekuLeafOffset.x, s_dekuLeafOffset.y, s_dekuLeafOffset.z);
    mDoMtx_stack_c::YrotM(s_dekuLeafBaseRotY);
    mDoMtx_stack_c::XrotM(s_dekuLeafBaseRotX);
    mDoMtx_stack_c::ZrotM(s_dekuLeafSpin);
    mDoMtx_stack_c::XrotM(s_dekuLeafFlipX);
    s_dekuLeafModel->setBaseTRMtx(mDoMtx_stack_c::get());
    s_dekuLeafModel->setBaseScale(cXyz(s_dekuLeafScale, s_dekuLeafScale, s_dekuLeafScale));
    link->modelCalc(s_dekuLeafModel);
}

// ============================================
// Hooks
// ============================================
DEFINE_HOOK(&daAlink_c::checkGrabRooster, GrabRooster);
DEFINE_HOOK(&daAlink_c::procAutoJumpInit, AutoJumpInit);
DEFINE_HOOK(&daAlink_c::procAutoJump, AutoJump);
DEFINE_HOOK(&daAlink_c::procLandInit, LandInit);
DEFINE_HOOK(&daAlink_c::checkItemChangeFromButton, ItemChange);
DEFINE_HOOK(&daAlink_c::draw, AlinkDraw);

// Force the engine's checkGrabRooster TRUE while the leaf is out, so every inlined
// checkGrabGlide() site (chase, gravity, wind, model-flag, camera) treats the leaf as a
// glide and the native cucco chassis runs it. Only overrides when actually leaf-gliding.
void on_grab_rooster_post(ModContext*, void*, void* retval, void*) {
    if (retval == nullptr) return;
    if (leaf_on() && (s_dekuLeafGlideActive || s_dekuLeafTakeoffRising)) {
        *static_cast<BOOL*>(retval) = TRUE;
    }
}

// procAutoJumpInit consumes the pending-takeoff request into the live glide flag (fork
// d_a_alink.cpp:18574). Stock clears the flag by not touching it, so we set it here.
void on_auto_jump_init_post(ModContext*, void* args, void*, void*) {
    (void)args;
    if (leaf_on()) {
        s_dekuLeafGlideActive = s_dekuLeafTakeoffPending;
    } else {
        s_dekuLeafGlideActive = false;
    }
    s_dekuLeafTakeoffPending = false;
}

// procAutoJump PRE: the leaf-specific inserts the fork interleaved into procAutoJump
// (A-toggle, meter cost, sustained lift, bomb drop, 1.20x glide speed, launch land-hold).
// The base glide itself runs natively (checkGrabRooster forced TRUE above).
HookAction on_auto_jump_pre(ModContext*, void* args, void* retval, void*) {
    auto* link = mods::arg<daAlink_c*>(args, 0);
    if (link == nullptr || !leaf_on()) {
        return HOOK_CONTINUE;
    }

    // A-toggle (fork d_a_alink.cpp:18626): A alone (NOT R+A) opens/closes the canopy.
    if (!leaf_real_rooster(link) &&
        !(mDoCPd_c::getHoldR(PAD_1) != 0) && link->doTrigger()) {
        if (!s_dekuLeafGlideActive && !albw_meter_can_deku_leaf()) {
            // no charge — leave the toggle alone and fall normally
        } else {
            s_dekuLeafGlideActive = !s_dekuLeafGlideActive;
            if (s_dekuLeafGlideActive) {
                link->mMaxSpeed = link->mpHIO->mAutoJump.m.mCuccoJumpMaxSpeed;
                link->field_0x3478 = link->mpHIO->mAutoJump.m.mCuccoFallMaxSpeed;
                link->setSpecialGravity(-1.0f, link->field_0x3478, FALSE);
                link->mProcVar2.field_0x300c = 1;
                albw_meter_on_deku_leaf_start();
            } else {
                leaf_stow(link);
            }
        }
    }

    if (leaf_check_glide()) {
        if (!albw_meter_can_deku_leaf()) {
            leaf_stow(link);
        } else {
            albw_meter_on_deku_leaf();
            leaf_update_takeoff(link);
            leaf_update_bomb_drop(link);
            link->mProcVar2.field_0x300c = 1;
            if (!link->checkUpperAnime(dRes_ID_ALANM_BCK_WALKHBS_e)) {
                link->setUpperAnimeBaseSpeed(dRes_ID_ALANM_BCK_GRABD_e, 0.0f, 3.0f);
                link->setUpperAnime(dRes_ID_ALANM_BCK_WALKHBS_e, daAlink_c::UPPER_1, 1.0f, 0.0f,
                                    -1, 3.0f);
            }
            // Glide-speed feed: native procAutoJump chases mNormalSpeed -> mMaxSpeed at
            // accel 0.1 (same accel the fork used). Setting mMaxSpeed here reproduces the
            // fork's 1.20x scale, or the low forward drift while the gust is lifting.
            if (s_dekuLeafTakeoffRising) {
                link->mMaxSpeed = s_dekuLeafTakeoffFwd;
            } else {
                link->mMaxSpeed = link->mpHIO->mAutoJump.m.mCuccoJumpMaxSpeed *
                                  s_dekuLeafGlideSpeedScale;
            }

            // Launch land-hold (fork d_a_alink.cpp:18729): a gust takeoff launches from a
            // standing/running start, so the launch-frame collision still reads ground.
            // Native would hand Link to checkLandAction and drop the glide. While the gust
            // is actually lifting, skip the native proc for this frame — physics elsewhere
            // applies the speed.y we just wrote, and next frame Link is airborne.
            if (s_dekuLeafTakeoffRising && link->mLinkAcch.ChkGroundHit()) {
                if (retval != nullptr) {
                    *static_cast<int*>(retval) = 1;
                }
                return HOOK_SKIP_ORIGINAL;
            }
        }
    }

    return HOOK_CONTINUE;
}

// procLandInit POST: landing ends the glide through the same stow path (fork
// d_a_alink.cpp:19111), so the gust-rise flag and gravity come back with it.
void on_land_init_post(ModContext*, void* args, void*, void*) {
    auto* link = mods::arg<daAlink_c*>(args, 0);
    if (link != nullptr && leaf_on()) {
        leaf_stow(link);
    }
}

// checkItemChangeFromButton PRE: while the leaf is out the hands are on the canopy, so the
// stock equip path would take a bomb press as a grab and fight the leaf pose (fork
// d_a_alink.cpp:18844 suppresses it). Item buttons do nothing else mid-glide.
HookAction on_item_change_pre(ModContext*, void* args, void* retval, void*) {
    auto* link = mods::arg<daAlink_c*>(args, 0);
    if (link != nullptr && leaf_on() && leaf_check_glide()) {
        if (retval != nullptr) {
            *static_cast<BOOL*>(retval) = FALSE;
        }
        return HOOK_SKIP_ORIGINAL;
    }
    return HOOK_CONTINUE;
}

// draw POST: draw the canopy while gliding (fork d_a_alink.cpp:22016).
void on_alink_draw_post(ModContext*, void* args, void*, void*) {
    auto* link = mods::arg<daAlink_c*>(args, 0);
    if (link == nullptr || !leaf_on()) {
        return;
    }
    if (leaf_check_glide() && link->checkCokkoGlide()) {
        leaf_update_model(link);
        if (s_dekuLeafModel != nullptr) {
            link->modelDraw(s_dekuLeafModel, 0);
        }
    }
}

bool install(ModError* error, const char* name, ModResult r) {
    if (r != MOD_OK) {
        if (svc_log != nullptr) svc_log->error(mod_ctx, name);
        mods::set_error(error, MOD_ERROR, name);
        return false;
    }
    return true;
}

}  // namespace

// ============================================
// SHUTDOWN LEAK FIX: the bundled Deku Leaf BMD was loaded and never released -
// the host reported "reclaimed 1 resource buffer(s) that were never freed" at
// every quit. It cannot be freed at load time: loaderBasicBmd parses the model
// data IN PLACE out of this buffer (J3D data is pointer-fixed), so the buffer
// must outlive every model built from it. Release order therefore matters -
// model first, then the parsed handle, then the backing buffer (the same order
// albw_armo_free_reveal uses for the Armogohma reveal BMD).
// ============================================
ModResult albw_deku_leaf_shutdown(ModError*) {
    if (s_dekuLeafModel != nullptr) {
        JKR_DELETE(s_dekuLeafModel);
        s_dekuLeafModel = nullptr;
    }
    s_dekuLeafModelData = nullptr;  // parsed in place out of the buffer below
    if (svc_resource != nullptr && s_dekuLeafBuf.data != nullptr) {
        svc_resource->free(mod_ctx, &s_dekuLeafBuf);
    }
    s_dekuLeafBuf = ResourceBuffer RESOURCE_BUFFER_INIT;
    return MOD_OK;
}

ModResult albw_deku_leaf_init(ModError* error) {
    if (!install(error, "DekuLeafGrabRooster",
                 mods::hook::add_post<GrabRooster>(on_grab_rooster_post)) ||
        !install(error, "DekuLeafAutoJumpInit",
                 mods::hook::add_post<AutoJumpInit>(on_auto_jump_init_post)) ||
        !install(error, "DekuLeafAutoJump",
                 mods::hook::add_pre<AutoJump>(on_auto_jump_pre)) ||
        !install(error, "DekuLeafLandInit",
                 mods::hook::add_post<LandInit>(on_land_init_post)) ||
        !install(error, "DekuLeafItemChange",
                 mods::hook::add_pre<ItemChange>(on_item_change_pre)) ||
        !install(error, "DekuLeafDraw",
                 mods::hook::add_post<AlinkDraw>(on_alink_draw_post))) {
        return MOD_ERROR;
    }
    if (svc_log != nullptr) svc_log->info(mod_ctx, "albw deku leaf glide ready");
    return MOD_OK;
}

void albw_deku_leaf_tick() {
    if (!leaf_on()) {
        return;
    }
    daAlink_c* link = static_cast<daAlink_c*>(g_dComIfG_gameInfo.play.getPlayer(0));
    if (link == nullptr) {
        return;
    }

    // Fork execute() seam (d_a_alink.cpp:19751): glide truth-check + gust retire.
    leaf_update_glide_state(link);

    // Fork f_ap_game R+A gesture (f_ap_game.cpp:822): while the leaf is on, R+A belongs to
    // the leaf for human Link. Entry is the A EDGE; the sustain in procAutoJump reads the
    // hold, so one press lifts for as long as R+A is held.
    if (mDoCPd_c::getHoldR(PAD_1) != 0 && mDoCPd_c::getHoldA(PAD_1) != 0 && !link->checkWolf()) {
        if (mDoCPd_c::getTrigA(PAD_1) != 0 && !s_dekuLeafTakeoffRising &&
            leaf_check_takeoff(link)) {
            leaf_start_takeoff(link);
        }
    }
}

#endif  // TARGET_PC
