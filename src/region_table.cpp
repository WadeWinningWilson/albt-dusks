#include "region_table.h"

#include "albw_common.h"
#include "config_vars.h"

#include "d/d_com_inf_game.h"
#include "d/d_stage.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace {

int s_damage_scale_depth = 0;

f32 tableMultForSaveTbl(int saveTbl) {
    switch (saveTbl) {
    case dStage_SaveTbl_ORDON:
    case dStage_SaveTbl_FARON:
        return 1.00f;
    case dStage_SaveTbl_PRISON:
        return 1.50f;
    case dStage_SaveTbl_ELDIN:
        return 1.25f;
    case dStage_SaveTbl_LANAYRU:
    case dStage_SaveTbl_CASTLE_TOWN:
    case dStage_SaveTbl_FISHING_POND:
        return 1.50f;
    case dStage_SaveTbl_FIELD:
        return 1.05f;
    case dStage_SaveTbl_GROVE:
        return 2.25f;
    case dStage_SaveTbl_SNOWPEAK:
        return 2.00f;
    case dStage_SaveTbl_DESERT:
        return 1.75f;
    case dStage_SaveTbl_LV1:
        return 1.05f;
    case dStage_SaveTbl_LV2:
        return 1.40f;
    case dStage_SaveTbl_LV3:
        return 1.65f;
    case dStage_SaveTbl_LV4:
        return 1.90f;
    case dStage_SaveTbl_LV5:
        return 2.15f;
    case dStage_SaveTbl_LV6:
        return 2.40f;
    case dStage_SaveTbl_LV7:
        return 2.65f;
    case dStage_SaveTbl_LV8:
        return 2.90f;
    case dStage_SaveTbl_LV9:
        return 3.15f;
    case dStage_SaveTbl_CAVE1:
    case dStage_SaveTbl_CAVE2:
    case dStage_SaveTbl_GROTTO:
        return 1.05f;
    default:
        return 1.00f;
    }
}

f32 tableMultForStageRoom(const char* stage, int roomNo) {
    if (stage == NULL || stage[0] == '\0' || roomNo < 0) {
        return -1.0f;
    }
    if (std::strcmp(stage, "F_SP121") == 0) {
        switch (roomNo) {
        case 1:
        case 6:
        case 15:
            return 1.05f;
        case 0:
        case 2:
        case 3:
        case 4:
        case 5:
        case 7:
            return 1.25f;
        case 9:
        case 10:
        case 11:
        case 12:
        case 13:
        case 14:
            return 1.50f;
        default:
            return -1.0f;
        }
    }
    if (std::strcmp(stage, "F_SP117") == 0) {
        if (roomNo == 1 || roomNo == 2 || roomNo == 3) {
            return 2.25f;
        }
        return -1.0f;
    }
    if (std::strcmp(stage, "F_SP122") == 0) {
        return 1.50f;
    }
    return -1.0f;
}

f32 tableMultForStageName(const char* stage) {
    if (stage == NULL || stage[0] == '\0') {
        return -1.0f;
    }
    if (std::strcmp(stage, "D_SB00") == 0) {
        return 2.00f;
    }
    if (std::strcmp(stage, "D_SB01") == 0) {
        return 2.65f;
    }
    if (std::strcmp(stage, "D_SB02") == 0 || std::strcmp(stage, "D_SB04") == 0) {
        return 1.25f;
    }
    if (std::strcmp(stage, "D_SB03") == 0) {
        return 1.50f;
    }
    if (std::strcmp(stage, "D_SB10") == 0) {
        return 1.00f;
    }
    if (std::strcmp(stage, "F_SP102") == 0) {
        return 1.05f;
    }
    if (std::strcmp(stage, "F_SP123") == 0 || std::strcmp(stage, "F_SP118") == 0 ||
        std::strcmp(stage, "F_SP124") == 0 || std::strcmp(stage, "F_SP125") == 0)
    {
        return 1.75f;
    }
    return -1.0f;
}

int currentRoomNo() {
    const int stay = dStage_roomControl_c::getStayNo();
    if (stay >= 0) {
        return stay;
    }
    return g_dComIfG_gameInfo.play.getStartStageRoomNo();
}

u16 scaleAmountU16(u16 amount, f32 mult) {
    if (amount == 0 || mult <= 1.0f) {
        return amount;
    }
    const f32 scaled = static_cast<f32>(amount) * mult;
    int rounded = static_cast<int>(scaled + 0.5f);
    if (rounded < 1) {
        rounded = 1;
    }
    if (rounded > 0xFFFF) {
        rounded = 0xFFFF;
    }
    return static_cast<u16>(rounded);
}

}  // namespace

f32 albw_region_resolve_table_mult() {
    const char* stage = g_dComIfG_gameInfo.play.getStartStageName();
    const f32 byRoom = tableMultForStageRoom(stage, currentRoomNo());
    if (byRoom > 0.0f) {
        return byRoom;
    }
    const f32 byName = tableMultForStageName(stage);
    if (byName > 0.0f) {
        return byName;
    }
    stage_stag_info_class* stag = g_dComIfG_gameInfo.play.getStage().getStagInfo();
    if (stag == NULL) {
        return 1.00f;
    }
    return tableMultForSaveTbl(dStage_stagInfo_GetSaveTbl(stag));
}

f32 albw_region_damage_mult() {
    if (!albw_cfg_bool(g_region_damage, false) || s_damage_scale_depth <= 0) {
        return 1.0f;
    }
    return albw_region_resolve_table_mult();
}

f32 albw_region_rupee_mult() {
    f32 mult = 1.0f;
    if (albw_cfg_bool(g_region_mult, true) && albw_cfg_bool(g_region_mult_rupees, true)) {
        mult *= albw_region_resolve_table_mult();
    }
    if (albw_cfg_bool(g_region_damage, false)) {
        mult *= 3.0f;
    }
    return mult;
}

u16 albw_region_scale_rupees(u16 amount) {
    return scaleAmountU16(amount, albw_region_rupee_mult());
}

void albw_region_push_damage_scale() {
    s_damage_scale_depth++;
}

void albw_region_pop_damage_scale() {
    if (s_damage_scale_depth > 0) {
        s_damage_scale_depth--;
    }
}

s16 albw_region_scale_hp(s16 hp) {
    if (hp <= 1 || !albw_cfg_bool(g_region_hp, false)) {
        return hp;
    }
    const f32 mult = albw_region_resolve_table_mult();
    if (mult <= 1.0f) {
        return hp;
    }
    const int scaled = static_cast<int>(static_cast<f32>(hp) * mult + 0.5f);
    if (scaled > 32767) {
        return 32767;
    }
    if (scaled < 1) {
        return 1;
    }
    return static_cast<s16>(scaled);
}
