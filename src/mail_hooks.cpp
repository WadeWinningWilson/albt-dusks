#include "mail_hooks.h"

#include "mail.h"
#include "albw_common.h"
#include "albw_game.h"
#include "config_vars.h"
#include "mods/hook.hpp"

#include "d/actor/d_a_horse.h"
#include "d/actor/d_a_npc.h"
#define private public
#include "d/actor/d_a_npc_post.h"
#undef private
#include "d/actor/d_a_player.h"
#include "d/actor/d_a_tag_evtarea.h"
#include "d/d_com_inf_game.h"
#include "d/d_event_data.h"
#include "d/d_meter2_info.h"
#include "d/d_msg_flow.h"
#include "d/d_msg_string_base.h"
#include "d/d_pane_class.h"
#include "d/d_save.h"

#include <cstdio>

#define private public
#include "d/d_menu_letter.h"
#undef private

namespace {

constexpr int kTypeDeliver = 1;
constexpr int kModeRun = 2;
constexpr int kEvtDeliver = 2;
constexpr int kEvtDeliverHorse = 3;
constexpr int kEvtDeliverWolf = 4;
#ifndef D_MENU_LETTER_LINE_MAX
constexpr int kLetterLineMax = 12;
#else
constexpr int kLetterLineMax = D_MENU_LETTER_LINE_MAX;
#endif

static dMsgFlow_c sDeliverFlow;

void refresh_runtime_letter_read_ui(dMenu_Letter_c* menu) {
    if (menu == nullptr) {
        return;
    }

    const u8 idx = menu->field_0x3ac[menu->field_0x36f * 6 + menu->mIndex] - 1;
    if (!dAlbwMail_isRuntimeLetter(idx)) {
        return;
    }

    for (int i = 0; i < 2; i++) {
        J2DTextBox* text1 = nullptr;
        if (menu->field_0x2f4[i] != nullptr) {
            text1 = static_cast<J2DTextBox*>(menu->field_0x2f4[i]->getPanePtr());
        }
        J2DTextBox* text2 = static_cast<J2DTextBox*>(menu->field_0x2ec[i]->getPanePtr());
        dAlbwMail_drawLetterBodyPage(idx, menu->field_0x3e3 - 1, kLetterLineMax, text2, text1,
                                     reinterpret_cast<dMsgStringBase_c*>(menu->mpString));
    }

    menu->field_0x3e2 = static_cast<u8>(dAlbwMail_getLetterBodyPageMax(
        idx, reinterpret_cast<dMsgStringBase_c*>(menu->mpString), kLetterLineMax));

    if (menu->field_0x3e2 > 1) {
        char pageLabel[20];
        std::snprintf(pageLabel, sizeof(pageLabel), "%u/%u", menu->field_0x3e3, menu->field_0x3e2);
        for (int i = 0; i < 2; i++) {
            if (menu->field_0x1e4[i] != nullptr) {
                menu->field_0x1e4[i]->show();
                menu->field_0x1e4[i]->setString(20, pageLabel);
            }
        }
    }
}

bool mail_hooks_enabled() {
    return albw_cfg_bool(g_postman_mail, true);
}

int cut_deliver_prm(int staffId) {
    int* pi = reinterpret_cast<int*>(g_dComIfG_gameInfo.play.getEvtManager().getMySubstanceP(
        staffId, "prm", dEvDtData_c::TYPE_INT));
    return pi != nullptr ? *pi : -1;
}

DEFINE_HOOK(&daNpc_Post_c::wait, PostWait);
DEFINE_HOOK(&daNpc_Post_c::cutDeliver, PostCutDeliver);
DEFINE_HOOK(dMeter2Info_recieveLetter, RecieveLetter);
DEFINE_HOOK(&dMenu_Letter_c::setPageText, LetterSetPageText);
DEFINE_HOOK(&dMenu_Letter_c::setDMYPageText, LetterSetDmyPageText);
DEFINE_HOOK(&dMenu_Letter_c::read_open_init, LetterReadOpenInit);
DEFINE_HOOK(&dMenu_Letter_c::read_next_fadein_init, LetterReadNextFadeinInit);

void on_post_wait_post(ModContext*, void* args, void*, void*) {
    if (!mail_hooks_enabled()) {
        return;
    }

    auto* post = mods::arg<daNpc_Post_c*>(args, 0);
    if (post == nullptr || !dAlbwMail_isDeliverPostman(post) || post->mType != kTypeDeliver ||
        post->mMode != kModeRun)
    {
        return;
    }

    daPy_py_c* player = albw_game::link_player();
    if (player == nullptr) {
        return;
    }

    if (albw_game::link_player()->checkWolf() && !daNpcT_chkEvtBit(110)) {
        return;
    }

    for (int i = 0; i < 4; i++) {
        auto* area = static_cast<daTag_EvtArea_c*>(post->mActorMngrs[i].getActorP());
        if (area == nullptr || !area->chkPointInArea(player->current.pos)) {
            continue;
        }
        if (player->checkBoarRide()) {
            area->noEffect();
            break;
        }
        if (g_dComIfG_gameInfo.play.getEvent()->runCheck() || !dAlbwMail_canTriggerDeliver()) {
            break;
        }
        dAlbwMail_onDeliverCutsceneOrdered();
        post->mActorPos = area->current.pos;
        if (player->checkHorseRide()) {
            post->mEvtNo = kEvtDeliverHorse;
        } else if (player->checkWolf()) {
            post->mEvtNo = kEvtDeliverWolf;
        } else {
            post->mEvtNo = kEvtDeliver;
        }
        break;
    }
}

void on_cut_deliver_post(ModContext*, void* args, void* retval, void*) {
    if (!mail_hooks_enabled()) {
        return;
    }

    auto* post = mods::arg<daNpc_Post_c*>(args, 0);
    const int staffId = mods::arg<int>(args, 1);
    if (post == nullptr || !dAlbwMail_isDeliverPostman(post)) {
        return;
    }

    const int prm = cut_deliver_prm(staffId);
    const bool advance = g_dComIfG_gameInfo.play.getEvtManager().getIsAddvance(staffId);

    if (advance && prm == 5) {
        sDeliverFlow.initWord(post, dAlbwMail_getDeliverSpeech(), 0xFF, 0, NULL);
    } else if (!advance && prm == 5) {
        const int rv = sDeliverFlow.doFlow(post, NULL, 0) ? 1 : 0;
        if (retval != nullptr) {
            *static_cast<int*>(retval) = rv;
        }
    } else if (!advance && prm == 8 && retval != nullptr && *static_cast<int*>(retval) == 1) {
        if (!dAlbwMail_hasReceivedBundle()) {
            dMeter2Info_recieveLetter();
        }
        dAlbwMail_onDeliverCutsceneFinished(dAlbwMail_hasReceivedBundle());
    } else if (advance && prm == 10) {
        dAlbwMail_onDeliverCutsceneFinished(dAlbwMail_hasReceivedBundle());
    }
}

void on_recieve_letter_post(ModContext*, void*, void*, void*) {
    if (!mail_hooks_enabled()) {
        return;
    }
    if (dAlbwMail_hasReceivedBundle()) {
        dAlbwMail_onDeliveryComplete();
    }
}

void on_set_page_text_post(ModContext*, void* args, void*, void*) {
    if (!mail_hooks_enabled()) {
        return;
    }
    auto* menu = mods::arg<dMenu_Letter_c*>(args, 0);
    if (menu == nullptr) {
        return;
    }
    const int pageBase = menu->field_0x36f * 6;
    for (int i = 0; i < menu->field_0x373; i++) {
        const u8 idx = menu->field_0x3ac[i + pageBase] - 1;
        if (!dAlbwMail_isRuntimeLetter(idx)) {
            continue;
        }
        dAlbwMail_drawLetterSubject(idx, menu->field_0x124[i][0]);
        dAlbwMail_drawLetterSubject(idx, menu->field_0x124[i][1]);
        dAlbwMail_drawLetterSender(idx, menu->field_0x124[i][2]);
        dAlbwMail_drawLetterSender(idx, menu->field_0x124[i][3]);
    }
}

void on_set_dmy_page_text_post(ModContext*, void* args, void*, void*) {
    if (!mail_hooks_enabled()) {
        return;
    }
    auto* menu = mods::arg<dMenu_Letter_c*>(args, 0);
    if (menu == nullptr) {
        return;
    }
    const int pageBase = menu->field_0x372 * 6;
    for (int i = 0; i < menu->field_0x373; i++) {
        const u8 idx = menu->field_0x3ac[i + pageBase] - 1;
        if (!dAlbwMail_isRuntimeLetter(idx)) {
            continue;
        }
        dAlbwMail_drawLetterSubject(idx, menu->field_0x184[i][0]);
        dAlbwMail_drawLetterSubject(idx, menu->field_0x184[i][1]);
        dAlbwMail_drawLetterSender(idx, menu->field_0x184[i][2]);
        dAlbwMail_drawLetterSender(idx, menu->field_0x184[i][3]);
    }
}

void on_read_open_init_post(ModContext*, void* args, void*, void*) {
    if (!mail_hooks_enabled()) {
        return;
    }
    refresh_runtime_letter_read_ui(mods::arg<dMenu_Letter_c*>(args, 0));
}

void on_read_next_fadein_init_post(ModContext*, void* args, void*, void*) {
    if (!mail_hooks_enabled()) {
        return;
    }
    refresh_runtime_letter_read_ui(mods::arg<dMenu_Letter_c*>(args, 0));
}

// ============================================
// A hook that fails to resolve must NOT abort mod_initialize.
//
// This helper used to call mods::set_error(..., MOD_ERROR, ...) and return
// false, which made mod_initialize return MOD_ERROR - so ONE unresolved symbol
// unloaded the ENTIRE mod. That is how a single missing hook target reached
// players as "Failed - Reason: <hook name>" with nothing loaded at all, on a
// build where every other feature was fine. It is the same doctrine fyrus.cpp
// already states for the boss hooks.
//
// Now the miss is LOUD and SCOPED: the feature that needed the hook is
// inactive for the run and says so by name in the log, and everything else
// still loads. Never make this silent - a quiet miss turns "never bound" into
// "plausibly wrong forever".
// ============================================
bool install(ModError*, const char* name, ModResult r) {
    if (r != MOD_OK) {
        if (svc_log != nullptr) {
            svc_log->error(mod_ctx, name);
            svc_log->error(mod_ctx,
                           "hook above did NOT install - that feature is inactive this run");
        }
    }
    return true;
}

}  // namespace

ModResult albw_mail_hooks_init(ModError* error) {
    if (!install(error, "PostWaitPost", mods::hook_add_post<PostWait>(svc_hook, on_post_wait_post)) ||
        !install(error, "PostCutDeliverPost",
                 mods::hook_add_post<PostCutDeliver>(svc_hook, on_cut_deliver_post)) ||
        !install(error, "RecieveLetterPost",
                 mods::hook_add_post<RecieveLetter>(svc_hook, on_recieve_letter_post)) ||
        !install(error, "LetterSetPagePost",
                 mods::hook_add_post<LetterSetPageText>(svc_hook, on_set_page_text_post)) ||
        !install(error, "LetterSetDmyPost",
                 mods::hook_add_post<LetterSetDmyPageText>(svc_hook, on_set_dmy_page_text_post)) ||
        !install(error, "LetterReadOpenPost",
                 mods::hook_add_post<LetterReadOpenInit>(svc_hook, on_read_open_init_post)) ||
        !install(error, "LetterReadFadeinPost",
                 mods::hook_add_post<LetterReadNextFadeinInit>(svc_hook,
                                                               on_read_next_fadein_init_post)))
    {
        return MOD_ERROR;
    }
    return MOD_OK;
}

ModResult albw_mail_hooks_shutdown(ModError*) {
    return MOD_OK;
}
