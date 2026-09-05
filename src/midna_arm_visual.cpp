// ============================================
// NEW CODE - ALBW Port ("Midna's Grasp" - the hair STRIKE visual)
//
// Fork d_a_midna.cpp, daMidna_c::setHairAngle. The arm actor was ported but
// its strike LOOK lives here, and without it the hair never expresses the jab
// (the reported "tracks and attacks but doesn't appear correctly"). The fork's
// own decision note: the hair pipeline is vanilla EXCEPT one strike-gated fan
// collapse (STRIKE UNCURL) and the 1-frame STAB SPEED assignment:
//
//   - STRIKE UNCURL: the fan is angle-only - a weathervane; the one extension
//     the chain can express is UNCURLING (fVar1 base 0.85 step 0.15 instead of
//     0.05/0.3), straightening the arch into a line at the target.
//   - STAB SPEED: while striking the pose is ASSIGNED to the strike line
//     (1-frame snap); the return glide and the idle keep the vanilla slew.
//
// Both sit mid-loop, so the whole function is replaced via pre-hook with the
// fork body (minus its ALBW-ARMIK probes, which the fork labels
// "instrumentation, non-behavioral"). Everything else below is STOCK
// setHairAngle, line for line; l_hairScale/hairOffset are const file data
// re-declared verbatim (d_a_midna.cpp:52).
// ============================================

#include "global.h"
#include <os.h>

#include "SSystem/SComponent/c_math.h"
#include "d/d_com_inf_game.h"
#include "JSystem/JMath/JMath.h"
#define private public
#include "d/actor/d_a_alink.h"
#include "d/actor/d_a_midna.h"
#undef private

#include "albw_common.h"
#include "wolf_combat.h"
#include "mods/hook.hpp"

#if TARGET_PC

namespace {

DEFINE_HOOK(&daMidna_c::setHairAngle, MidnaSetHairAngle);

// stock d_a_midna.cpp:60 - the Joint enum is TU-local; values mirrored by
// declaration order (verbatim comment markers in the stock file).
constexpr int kJntHead  = 0x04;  // JNT_HEAD
constexpr int kJntHair1 = 0x06;  // JNT_HAIR_1
constexpr int kJntHair2 = 0x07;  // JNT_HAIR_2
constexpr int kJntHair5 = 0x0A;  // JNT_HAIR_5

// stock d_a_midna.cpp:52 - const file data, re-declared verbatim.
static Vec const l_hairScale[5] = {
    {0.3f, 0.8f, 0.7f},
    {0.2f, 0.8f, 0.4f},
    {0.15f, 0.75f, 0.5f},
    {0.1f, 0.7f, 0.7f},
    {1.0f, 1.0f, 1.0f},
};

// fork STRIKE UNCURL tuning (d_a_midna.cpp, user-tuned 2026-07-16).
static const f32 kArmJabFanBase = 0.85f;
static const f32 kArmJabFanStep = 0.15f;

void albw_midna_setHairAngle(daMidna_c* m) {
    if (!m->checkStateFlg0(daMidna_c::FLG0_UNK_8)) {
        m->initHairAngle();
        return;
    }

    cXyz prev_pos, head_dir;
    mDoMtx_multVecSR(m->mpShadowModel->getAnmMtx(kJntHead), &cXyz::BaseX, &head_dir);
    mDoMtx_multVecZero(m->mpShadowModel->getAnmMtx(kJntHair1), &prev_pos);
    s16 head_angle = head_dir.atan2sX_Z();
    s16 inv_head_angle = head_angle + 0x8000;
    cXyz vec, old_pos;

    int i;
    cXyz* pos = m->mHairPos;
    cXyz* dir = m->mHairDir;
    cXyz* scale = m->mHairScale;
    s16* angle_z = m->mHairAngleZ;
    s16* angle_y = m->mHairAngleY;

    f32 fVar4 = 0.75f;
    f32 fVar1 = 0.05f;
    s16 target_angle_y;
    BOOL bVar5 = false;
    f32 fVar2 = fabsf(m->speedF) * 0.04f;
    if (fVar2 > 1.0f) {
        fVar2 = 1.0f;
    }
    s16 target_angle_z, iVar16;
    iVar16 = m->field_0x872;
    BOOL bVar4 = false;
    m->field_0x872 += fVar2 * 0x1000 + 0x800;

    cXyz* atn_pos = NULL;
    daAlink_c* link = daAlink_getAlinkActorClass();
    if (link->checkMidnaHairAtnPos() && !daMidna_c::checkMidnaTired() &&
        !m->checkStateFlg0((daMidna_c::daMidna_FLG0)(daMidna_c::FLG0_NO_HAIR_SCALE |
                                                     daMidna_c::FLG0_UNK_200000 |
                                                     daMidna_c::FLG0_TAG_WAIT |
                                                     daMidna_c::FLG0_UNK_100)))
    {
        atn_pos = link->getMidnaHairAtnPos();
        m->onStateFlg0(daMidna_c::FLG0_UNK_10000000);
    } else {
        m->offStateFlg0(daMidna_c::FLG0_UNK_10000000);
    }

    // fork STRIKE UNCURL + STAB SPEED gate: only while the arm art publishes a
    // STRIKING reach. The idle/READY hover and the return glide keep vanilla.
    const BOOL armJabStraight = atn_pos != NULL && dAlbwMidnaArm_getReachPos(NULL) &&
                                dAlbwMidnaArm_isReachStriking();
    if (armJabStraight) {
        fVar1 = kArmJabFanBase;
    }

    for (i = 0; i < 5; i++, pos++, dir++, angle_z++, angle_y++, scale++) {
        if (m->checkStateFlg0(daMidna_c::FLG0_UNK_10000000)) {
            cLib_chasePos(scale, l_hairScale[4], 0.1f);
        } else {
            cLib_chasePos(scale, l_hairScale[i], 0.1f);
        }

        old_pos = *pos;

        if (m->checkStateFlg0(daMidna_c::FLG0_UNK_200000)) {
            if (i == 4) {
                mDoMtx_multVec(m->mpShadowModel->getAnmMtx(kJntHair5), &cXyz::BaseX,
                               pos);
            } else {
                mDoMtx_multVecZero(m->mpShadowModel->getAnmMtx(kJntHair2 + i), pos);
            }

            old_pos = *pos;
            vec = *pos - prev_pos;
            *angle_z = cM_atan2s(-vec.y, -vec.z);
            *angle_y = cM_atan2s(vec.x, JMAFastSqrt(vec.y * vec.y + vec.z * vec.z));
            prev_pos = *pos;
        } else if (atn_pos != NULL) {
            vec = *atn_pos - prev_pos;
            mDoMtx_stack_c::YrotS(-m->shape_angle.y);
            mDoMtx_stack_c::multVec(&vec, &vec);
            target_angle_y = fVar1 * cM_atan2s(vec.x, JMAFastSqrt(vec.y * vec.y + vec.z * vec.z));
            if (i == 0 && (vec.z < 0.0f || vec.y >= 0.0f)) {
                bVar4 = true;
            }

            if (bVar4) {
                if (vec.y < 1.0f && i < 4) {
                    vec.y = 1.0f;
                }
                target_angle_z = fVar1 * cM_atan2s(-vec.y, -vec.z) - (1.0f - fVar1) * 0x4000;
            } else {
                target_angle_z =
                    fVar1 * (cM_atan2s(-vec.y, -vec.z) - 0x10000) - (1.0f - fVar1) * 0x4000;
            }

            // fork STRIKE UNCURL + STAB SPEED: 1-frame snap to the strike line
            // while striking; vanilla slew + 0.3 fan otherwise.
            if (armJabStraight) {
                *angle_z = target_angle_z;
                *angle_y = target_angle_y;
            } else {
                cLib_addCalcAngleS(angle_z, target_angle_z, 5, 0x1800, 0x100);
                cLib_addCalcAngleS(angle_y, target_angle_y, 5, 0x1800, 0x100);
            }
            fVar1 += armJabStraight ? kArmJabFanStep : 0.3f;
            if (fVar1 > 1.0f) {
                fVar1 = 1.0f;
            }
        } else {
            vec = *pos - prev_pos + *dir;
            vec += daAlink_getAlinkActorClass()->getWindSpeed();
            if (m->checkEndResetStateFlg0(daMidna_c::ERFLG0_UNK_20)) {
                vec = cXyz::Zero;
            }

            vec.y -= 2.0f;
            vec.y += fVar2 * (cM_rndFX(3.0f) + 3.0f) * cM_ssin(iVar16);
            mDoMtx_stack_c::YrotS(-head_angle);
            mDoMtx_stack_c::multVec(&vec, &vec);
            if (vec.abs() < 1.0f) {
                cLib_addCalcAngleS(angle_z, 0, 5, 0x1800, 0x100);
                cLib_addCalcAngleS(angle_y, 0, 5, 0x1800, 0x100);
            } else {
                if (i == 0) {
                    if (vec.z > 0.0f) {
                        vec.z *= -1.0f;
                    }
                    f32 fVar21 = vec.absXZ();
                    if (fVar21 < vec.y * -1.732f) {
                        if (fVar21 < 1.0f) {
                            vec.x = 0.0f;
                            vec.z = -1.0f;
                            vec.y = -0.577367f;
                        } else {
                            if (fabsf(m->speedF) < 0.1f) {
                                vec.x *= 0.5f;
                                fVar21 = vec.absXZ();
                            }
                            vec.y = fVar21 * -0.577367f;
                        }
                        bVar5 = true;
                    }
                }

                *angle_y = cM_atan2s(vec.x, JMAFastSqrt(vec.y * vec.y + vec.z * vec.z));
                if (i == 4) {
                    *angle_z = cM_atan2s(-vec.y, -vec.z);
                } else {
                    *angle_z = 0;
                }
            }

            iVar16 -= (fVar2 * 0x1000 + 0x800);
        }

        static Vec const hairOffset = {0.0f, 0.0f, 28.0f};
        if (!m->checkStateFlg0(daMidna_c::FLG0_UNK_200000)) {
            mDoMtx_stack_c::transS(prev_pos);
            mDoMtx_stack_c::YrotM(inv_head_angle);
            mDoMtx_stack_c::XrotM(*angle_z);
            mDoMtx_stack_c::YrotM(-*angle_y);
            mDoMtx_stack_c::scaleM(*scale);
            mDoMtx_stack_c::multVec(&hairOffset, pos);
        }

        if (m->checkEndResetStateFlg0(daMidna_c::ERFLG0_UNK_20)) {
            *dir = cXyz::Zero;
        } else {
            *dir = (*pos - old_pos) * fVar4;
            if (bVar5) {
                *dir *= 0.5f;
            }
        }

        prev_pos = *pos;
        fVar4 -= 0.1f;
    }
}

HookAction on_midna_set_hair_angle_pre(ModContext*, void* args, void*, void*) {
    auto* m = mods::arg<daMidna_c*>(args, 0);
    if (m == nullptr) {
        return HOOK_CONTINUE;
    }
    albw_midna_setHairAngle(m);
    return HOOK_SKIP_ORIGINAL;
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

ModResult albw_midna_arm_visual_init(ModError* error) {
    if (!install(error, "MidnaSetHairAngleStrike",
                 mods::hook_add_pre<MidnaSetHairAngle>(svc_hook, on_midna_set_hair_angle_pre)))
    {
        return MOD_ERROR;
    }
    svc_log->info(mod_ctx, "albw midna-arm strike visual ready");
    return MOD_OK;
}

#endif  // TARGET_PC

// ============================================
// NEW CODE ENDS HERE
// ============================================
