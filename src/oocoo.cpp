#include "oocoo.h"

#include "albw_game.h"
#include "rental_eligibility.h"

#define private public
#include "d/actor/d_a_alink.h"
#undef private

#include "d/d_com_inf_game.h"
#include "d/d_item_data.h"

#include <cstring>

namespace {

constexpr int kPrice = 15;

struct DungeonEntrance {
    const char* stage;
    s8 room;
    s16 point;
};

// Minimal warp-table baseline for Oocoo (first room / first point per fork resolver).
static const DungeonEntrance kDungeonEntrances[] = {
    {"D_MN01A", 0, 0},  // Forest Temple
    {"D_MN02A", 0, 0},  // Goron Mines (room 1 on some builds — refine when shop ships)
    {"D_MN03A", 0, 0},  // Lakebed
    {"D_MN04A", 1, 0},  // Arbiter's Grounds
    {"D_MN05A", 0, 0},  // Snowpeak
    {"D_MN06A", 0, 0},  // Temple of Time
    {"D_MN07A", 0, 0},  // City in the Sky
    {"D_MN08A", 0, 0},  // Palace of Twilight
    {"D_MN09A", 1, 0},  // Hyrule Castle
    {"D_MN10A", 0, 0},  // Cave of Ordeals
    {"D_MN11A", 0, 0},  // Cave
    {"D_MN11B", 0, 0},
    {"D_MN11C", 0, 0},
    {"D_MN11D", 0, 0},
    {"D_MN11E", 0, 0},
    {"D_MN11F", 0, 0},
    {"D_MN11G", 0, 0},
    {"D_MN11H", 0, 0},
    {"D_MN11I", 0, 0},
    {"D_MN11J", 0, 0},
    {"D_MN11K", 0, 0},
    {"D_MN11L", 0, 0},
    {"D_MN11M", 0, 0},
    {"D_MN11N", 0, 0},
    {"D_MN11O", 0, 0},
    {"D_MN11P", 0, 0},
    {"D_MN11Q", 0, 0},
    {"D_MN11R", 0, 0},
    {"D_MN11S", 0, 0},
    {"D_MN11T", 0, 0},
    {"D_MN11U", 0, 0},
    {"D_MN11V", 0, 0},
    {"D_MN11W", 0, 0},
    {"D_MN11X", 0, 0},
    {"D_MN11Y", 0, 0},
    {"D_MN11Z", 0, 0},
};

char sDeathDungeonStage[8] = {};
bool sDiedInDungeon = false;
bool sChoseOrdonAfterDeath = false;
bool sUsedOocooThisDeath = false;
bool sPendingEntranceWarp = false;

bool hasMetOocooSrOnce() {
    auto& player = g_dComIfG_gameInfo.info.getPlayer();
    return player.getGetItem().isFirstBit((u8)dItemNo_DUNGEON_EXIT_e) ||
           player.getGetItem().isFirstBit((u8)dItemNo_DUNGEON_EXIT_2_e) ||
           player.getGetItem().isFirstBit((u8)dItemNo_LV7_DUNGEON_EXIT_e) ||
           player.getGetItem().isFirstBit((u8)dItemNo_DUNGEON_BACK_e) ||
           player.getGetItem().isFirstBit((u8)dItemNo_TKS_LETTER_e) ||
           g_dComIfG_gameInfo.info.getMemory().getBit().isDungeonItemWarp();
}

bool resolveDungeonEntrance(const char* stage, s8* outRoom, s16* outPoint) {
    if (stage == nullptr || stage[0] == '\0') {
        return false;
    }
    for (const DungeonEntrance& entry : kDungeonEntrances) {
        if (std::strcmp(entry.stage, stage) == 0) {
            *outRoom = entry.room;
            *outPoint = entry.point;
            return true;
        }
    }
    return false;
}

}  // namespace

void albw_oocoo_on_death_context(const char* lastStageName, bool diedInDungeon) {
    sDiedInDungeon = diedInDungeon;
    sChoseOrdonAfterDeath = false;
    sUsedOocooThisDeath = false;
    sPendingEntranceWarp = false;
    sDeathDungeonStage[0] = '\0';

    if (diedInDungeon && lastStageName != nullptr) {
        int i = 0;
        for (; i < 7 && lastStageName[i] != '\0'; ++i) {
            sDeathDungeonStage[i] = lastStageName[i];
        }
        sDeathDungeonStage[i] = '\0';
    }
}

void albw_oocoo_on_warp_choice(int choice) {
    if (choice == 1 && sDiedInDungeon) {
        sChoseOrdonAfterDeath = true;
    }
}

bool albw_oocoo_can_show_in_shop() {
    if (!albw_rental_postman_unlocked()) {
        return false;
    }
    if (!hasMetOocooSrOnce()) {
        return false;
    }
    if (!sDiedInDungeon || !sChoseOrdonAfterDeath) {
        return false;
    }
    s8 roomNo;
    s16 point;
    if (!resolveDungeonEntrance(sDeathDungeonStage, &roomNo, &point)) {
        return false;
    }
    return !sUsedOocooThisDeath;
}

bool albw_oocoo_try_purchase() {
    if (!albw_oocoo_can_show_in_shop() || sDeathDungeonStage[0] == '\0') {
        return false;
    }

    const u16 rupees = g_dComIfG_gameInfo.info.getPlayer().getPlayerStatusA().getRupee();
    if (rupees < static_cast<u16>(kPrice)) {
        return false;
    }

    g_dComIfG_gameInfo.info.getPlayer().getPlayerStatusA().setRupee(rupees - kPrice);
    sUsedOocooThisDeath = true;
    sPendingEntranceWarp = true;
    return true;
}

void albw_oocoo_execute_pending_warp() {
    if (!sPendingEntranceWarp) {
        return;
    }
    sPendingEntranceWarp = false;

    if (sDeathDungeonStage[0] == '\0') {
        return;
    }

    s8 roomNo;
    s16 point;
    if (!resolveDungeonEntrance(sDeathDungeonStage, &roomNo, &point)) {
        const u16 rupees = g_dComIfG_gameInfo.info.getPlayer().getPlayerStatusA().getRupee();
        g_dComIfG_gameInfo.info.getPlayer().getPlayerStatusA().setRupee(rupees + kPrice);
        sUsedOocooThisDeath = false;
        return;
    }

    g_dComIfG_gameInfo.play.setNextStage(sDeathDungeonStage, roomNo, point, -1, 0, 0);
    daAlink_c* link = static_cast<daAlink_c*>(albw_game::link_player());
    if (link != nullptr) {
        link->procDungeonWarpSceneStartInit();
    }
}

const char* albw_oocoo_service_name() {
    return "Oocoo's Return";
}

const char* albw_oocoo_service_desc() {
    return "A small friend flew all this way here to help! Let me feed him "
           "to restore his strength.";
}

int albw_oocoo_service_price() {
    return kPrice;
}
