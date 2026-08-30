// Port of fork daNpc_Post_c rental blocks (bitSW 0x42) + F_SP103 room-1 spawn.

#include "rental_postman_hooks.h"

#include "albw_common.h"
#include "albw_game.h"
#include "config_vars.h"
#include "rental_dialogue.h"
#include "rental_eligibility.h"
#include "rental_shop.h"
#include "rental_shop_ui.h"

#include "Z2AudioLib/Z2SceneMgr.h"
#include "Z2AudioLib/Z2SeMgr.h"
#include "Z2AudioLib/Z2SeqMgr.h"
#include "d/actor/d_a_player.h"
#include "d/d_com_inf_game.h"
#include "d/d_msg_object.h"
#include "f_pc/f_pc_name.h"
#include "m_Do/m_Do_audio.h"
#include "mods/hook.hpp"

#include <cstring>

#define private public
#include "d/actor/d_a_npc_post.h"
#undef private

namespace {

constexpr u8 kRentalBitSW = 0x42;
constexpr u32 kRentalPostParam = (static_cast<u32>(kRentalBitSW) << 8) | 0x00u;
constexpr int kEvtNoResponse = 1;  // EVT_NO_RESPONSE

enum {
    FACE_MOT_HELLO = 3,
    FACE_MOT_BYE = 5,
    FACE_MOT_NONE_2 = 0xE,
    MOT_WAIT_A = 0,
    MOT_HELLO = 3,
    MOT_BYE = 5,
};

static const cXyz kRentalPostPos = {1505.3680f, 800.0000f, -2529.2832f};
static const csXyz kRentalPostAngle = {0, static_cast<s16>(-13008), 0};
static const cXyz kRentalPostScale = {1.0f, 1.0f, 1.0f};

using FopAcMCreateFn = fpc_ProcID (*)(s16, u32, const cXyz*, int, const csXyz*, const cXyz*, s8);
FopAcMCreateFn s_fopAcM_create = nullptr;

bool sOrdonSpawned = false;
int sOrdonLastStayRoom = -1;
bool sRentalEvtStarted = false;
int sGreetStep = 0;
bool sBgmPlaying = false;

dALBWDialogue_c* s_pDialogue = nullptr;
dALBWShop_c* s_pShop = nullptr;

bool rental_enabled() {
    return albw_cfg_bool(g_postman_rental, true);
}

bool ensure_fopAcM_create() {
    if (s_fopAcM_create != nullptr) {
        return true;
    }
    if (svc_hook == nullptr) {
        return false;
    }
    void* addr = nullptr;
    if (svc_hook->resolve(mod_ctx, "?fopAcM_create@@YAIFIPEBUcXyz@@HPEBVcsXyz@@0CIE@Z", &addr,
                          nullptr) != MOD_OK ||
        addr == nullptr)
    {
        return false;
    }
    s_fopAcM_create = reinterpret_cast<FopAcMCreateFn>(addr);
    return true;
}

bool is_rental_postman(daNpc_Post_c* post) {
    return post != nullptr && post->getBitSW() == kRentalBitSW;
}

void ensure_dialogue() {
    if (s_pDialogue == nullptr) {
        s_pDialogue = JKR_NEW dALBWDialogue_c();
    }
}

void ensure_shop() {
    if (s_pShop == nullptr) {
        s_pShop = JKR_NEW dALBWShop_c();
    }
}

void destroy_shop_ui() {
    JKR_DELETE(s_pShop);
    s_pShop = nullptr;
    JKR_DELETE(s_pDialogue);
    s_pDialogue = nullptr;
    sGreetStep = 0;
}

void stop_postman_bgm() {
    if (sBgmPlaying) {
        mDoAud_subBgmStop();
        sBgmPlaying = false;
    }
}

void try_spawn_ordon_rental() {
    if (!rental_enabled() || !albw_rental_postman_unlocked()) {
        return;
    }
    if (std::strcmp(albw_game::stage_name(), "F_SP103") != 0) {
        sOrdonLastStayRoom = -1;
        sOrdonSpawned = false;
        return;
    }

    const int stayRoom = albw_game::stay_room();
    if (stayRoom != 1) {
        if (sOrdonLastStayRoom == 1) {
            sOrdonSpawned = false;
        }
        sOrdonLastStayRoom = stayRoom;
        return;
    }

    if (sOrdonLastStayRoom != 1) {
        sOrdonSpawned = false;
    }
    sOrdonLastStayRoom = stayRoom;

    if (sOrdonSpawned) {
        return;
    }
    // create() ends with Execute→setParam→checkNowWolf(), which null-derefs
    // when Link is not ready yet (room load before ALINK). Wait for player.
    if (albw_game::link_player() == nullptr) {
        return;
    }
    if (!ensure_fopAcM_create()) {
        return;
    }

    s_fopAcM_create(fpcNm_NPC_POST_e, kRentalPostParam, &kRentalPostPos, stayRoom,
                    &kRentalPostAngle, &kRentalPostScale, -1);
    sOrdonSpawned = true;
}

DEFINE_HOOK(&daNpc_Post_c::evtTalk, PostEvtTalk);
DEFINE_HOOK(&daNpc_Post_c::Execute, PostExecute);
DEFINE_HOOK(&daNpc_Post_c::Draw, PostDraw);
DEFINE_HOOK(&daNpc_Post_c::Delete, PostDelete);
DEFINE_HOOK(&daNpc_Post_c::create, PostCreate);
DEFINE_HOOK(&dMsgObject_c::_draw, MsgObjectDraw);

HookAction on_create_pre(ModContext*, void* args, void*, void*) {
    // Zero ALBW voice latch before create()'s nested Execute() can read it.
    // Stock reset() memset ends before field_0x1014 — always garbage for every
    // Post type. Zero all; getBitSW() is not valid yet in create pre.
    auto* post = mods::arg<daNpc_Post_c*>(args, 0);
    if (post != nullptr) {
        post->field_0x1014 = 0;
    }
    return HOOK_CONTINUE;
}

HookAction on_evt_talk_pre(ModContext*, void* args, void* retval, void*) {
    if (!rental_enabled() || retval == nullptr) {
        return HOOK_CONTINUE;
    }

    auto* post = mods::arg<daNpc_Post_c*>(args, 0);
    if (!is_rental_postman(post)) {
        return HOOK_CONTINUE;
    }

    dALBWRental_armVanillaTalkSuppress();
    ensure_dialogue();

    if (dALBWRental_isGreetingState()) {
        if (s_pDialogue != nullptr && !s_pDialogue->isVisible()) {
            const char* text = nullptr;
            switch (sGreetStep) {
            case 0:
                text = dALBWRental_getGreetingText();
                break;
            case 1:
                text = dALBWRental_getGreetingPage2();
                break;
            case 2:
                text = dALBWRental_getGreetingPage3();
                break;
            default:
                break;
            }
            if (text != nullptr) {
                s_pDialogue->showWithText(text);
            }
        }
        if (s_pDialogue != nullptr && s_pDialogue->checkDismiss()) {
            s_pDialogue->hide();
            if (sGreetStep == 0 && dALBWRental_getGreetingPage2() != nullptr) {
                sGreetStep = 1;
            } else if (sGreetStep == 1 && dALBWRental_getGreetingPage3() != nullptr) {
                sGreetStep = 2;
            } else {
                sGreetStep = 0;
                dALBWRental_advanceToShop();
            }
        }
        *static_cast<BOOL*>(retval) = TRUE;
        return HOOK_SKIP_ORIGINAL;
    }

    if (dALBWRental_isFarewellState()) {
        if (s_pDialogue != nullptr && !s_pDialogue->isVisible()) {
            s_pDialogue->showWithText(dALBWRental_getFarewellText());
        }
        if (s_pDialogue != nullptr && s_pDialogue->checkDismiss()) {
            s_pDialogue->hide();
            dALBWRental_advanceToClosed();
        }
        *static_cast<BOOL*>(retval) = TRUE;
        return HOOK_SKIP_ORIGINAL;
    }

    if (dALBWRental_justClosed()) {
        sRentalEvtStarted = false;
        dALBWRental_clearVanillaTalkSuppress();
        stop_postman_bgm();
        post->mEvtNo = kEvtNoResponse;
        post->evtChange();
        *static_cast<BOOL*>(retval) = TRUE;
        return HOOK_SKIP_ORIGINAL;
    }

    if (!dALBWRental_isOpen()) {
        if (albw_game::is_wolf_form()) {
            post->mEvtNo = kEvtNoResponse;
            dALBWRental_clearVanillaTalkSuppress();
            post->evtChange();
        } else {
            post->mEvtNo = kEvtNoResponse;
            if (!sRentalEvtStarted) {
                ensure_shop();
                dALBWRental_open();
                sRentalEvtStarted = true;
            }
        }
    }

    *static_cast<BOOL*>(retval) = TRUE;
    return HOOK_SKIP_ORIGINAL;
}

void on_execute_post(ModContext*, void* args, void*, void*) {
    if (!rental_enabled()) {
        return;
    }

    auto* post = mods::arg<daNpc_Post_c*>(args, 0);
    if (!is_rental_postman(post)) {
        return;
    }

    // Stock reset() memset stops *before* field_0x1014, so it can be garbage on
    // the first Execute() inside create(). Clamp before any voice table index.
    if (post->field_0x1014 > 4) {
        post->field_0x1014 = 0;
    }

    // Actor heap may still be current during create→Execute; never JKR_NEW UI here
    // until the shop is actually open (talk event / game heap).
    if (dALBWRental_isOpen()) {
        ensure_dialogue();
        ensure_shop();
        if (s_pDialogue != nullptr && !s_pDialogue->isReady()) {
            s_pDialogue->tryCreate();
        }
        if (s_pShop != nullptr) {
            s_pShop->update();
        }
    }

    dALBWRental_tick();

    if (Z2GetSceneMgr() != nullptr && Z2GetSceneMgr()->getSeLoadStatus(0x4B) == 0) {
        Z2GetSceneMgr()->loadSeWave(0x4B);
    }

    if (dALBWRental_justEnteredGreeting()) {
        post->mFaceMotionSeqMngr.setNo(FACE_MOT_HELLO, -1.0f, FALSE, 0);
        post->mMotionSeqMngr.setNo(MOT_HELLO, -1.0f, FALSE, 0);
        post->field_0x1014 = 1;
        mDoAud_subBgmStart(Z2BGM_POSTMAN);
        sBgmPlaying = true;
    } else if (dALBWRental_justEnteredShop()) {
        post->mFaceMotionSeqMngr.setNo(FACE_MOT_NONE_2, -1.0f, FALSE, 0);
        post->mMotionSeqMngr.setNo(MOT_WAIT_A, -1.0f, FALSE, 0);
    } else if (dALBWRental_justEnteredFarewell()) {
        post->mFaceMotionSeqMngr.setNo(FACE_MOT_BYE, -1.0f, FALSE, 0);
        post->mMotionSeqMngr.setNo(MOT_BYE, -1.0f, FALSE, 0);
        post->field_0x1014 = 0;
    } else if (dALBWRental_justPurchased()) {
        post->field_0x1014 = 3;
    } else if (dALBWRental_justFailedPurchase()) {
        post->field_0x1014 = 4;
    }

    if (post->field_0x1014 != 0 && post->field_0x1014 <= 4 && Z2GetSceneMgr() != nullptr &&
        Z2GetSceneMgr()->getSeLoadStatus(0x4B) == 2)
    {
        static const u32 kVoiceSounds[5] = {
            0,
            Z2SE_POST_V_APPEAR,
            Z2SE_POST_V_THEME,
            Z2SE_POST_V_FANFARE,
            Z2SE_POST_V_RUN_HIGH,
        };
        post->mSound.startCreatureVoice(kVoiceSounds[post->field_0x1014], -1);
        post->field_0x1014 = 0;
    }

    if (sBgmPlaying && !dALBWRental_isOpen()) {
        stop_postman_bgm();
    }
}

void on_draw_post(ModContext*, void* args, void*, void*) {
    if (!rental_enabled()) {
        return;
    }
    auto* post = mods::arg<daNpc_Post_c*>(args, 0);
    if (!is_rental_postman(post)) {
        return;
    }
    if (s_pShop != nullptr && dALBWRental_isShopState()) {
        s_pShop->registerDraw();
    }
    if (s_pDialogue != nullptr) {
        s_pDialogue->registerDraw();
    }
}

void on_create_post(ModContext*, void* args, void*, void*) {
    if (!rental_enabled()) {
        return;
    }
    auto* post = mods::arg<daNpc_Post_c*>(args, 0);
    if (!is_rental_postman(post)) {
        return;
    }
    // Must clear before any Execute post uses this as a voice-table index.
    // Stock reset() does not cover field_0x1014.
    post->field_0x1014 = 0;
    if (Z2GetSceneMgr() != nullptr) {
        Z2GetSceneMgr()->loadSeWave(0x4B);
    }
    // Do NOT allocate dialogue/shop here — create() still has the actor solid
    // heap current (and already ran Execute once). UI is created on talk open.
}

void on_delete_post(ModContext*, void* args, void*, void*) {
    auto* post = mods::arg<daNpc_Post_c*>(args, 0);
    if (!is_rental_postman(post)) {
        return;
    }
    dALBWRental_clearVanillaTalkSuppress();
    destroy_shop_ui();
    sRentalEvtStarted = false;
    stop_postman_bgm();
    if (Z2GetSceneMgr() != nullptr) {
        Z2GetSceneMgr()->eraseSeWave(0x4B);
    }
}

HookAction on_msg_object_draw_pre(ModContext*, void*, void* retval, void*) {
    if (!rental_enabled() || !dALBWRental_shouldSuppressVanillaTalkMsg()) {
        return HOOK_CONTINUE;
    }
    if (retval != nullptr) {
        *static_cast<int*>(retval) = 1;
    }
    return HOOK_SKIP_ORIGINAL;
}

bool install(ModError* error, const char* name, ModResult r) {
    if (r != MOD_OK) {
        svc_log->error(mod_ctx, name);
        mods::set_error(error, MOD_ERROR, name);
        return false;
    }
    return true;
}

}  // namespace

ModResult albw_rental_postman_hooks_init(ModError* error) {
    if (!rental_enabled()) {
        return MOD_OK;
    }
    ensure_fopAcM_create();
    if (!install(error, "PostEvtTalkPre",
                 mods::hook_add_pre<PostEvtTalk>(svc_hook, on_evt_talk_pre)) ||
        !install(error, "PostCreatePre",
                 mods::hook_add_pre<PostCreate>(svc_hook, on_create_pre)) ||
        !install(error, "PostExecutePost",
                 mods::hook_add_post<PostExecute>(svc_hook, on_execute_post)) ||
        !install(error, "PostDrawPost", mods::hook_add_post<PostDraw>(svc_hook, on_draw_post)) ||
        !install(error, "PostCreatePost",
                 mods::hook_add_post<PostCreate>(svc_hook, on_create_post)) ||
        !install(error, "PostDeletePost",
                 mods::hook_add_post<PostDelete>(svc_hook, on_delete_post)) ||
        !install(error, "MsgObjectDrawPre",
                 mods::hook_add_pre<MsgObjectDraw>(svc_hook, on_msg_object_draw_pre)))
    {
        return MOD_ERROR;
    }
    return MOD_OK;
}

ModResult albw_rental_postman_hooks_shutdown(ModError*) {
    destroy_shop_ui();
    stop_postman_bgm();
    sRentalEvtStarted = false;
    sOrdonSpawned = false;
    return MOD_OK;
}

void albw_rental_postman_tick() {
    try_spawn_ordon_rental();
}
