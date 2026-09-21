// Ported verbatim from the fork's src/d/d_albw_sword_atp.cpp.
// The ONLY changes are ABI translations the mod cannot avoid:
//   - dComIfGs_* accessors are DUSK_NOINLINE and missing from the Windows stub,
//     so they route through albw_game.h (g_dComIfG_gameInfo). Same reads.
//   - dAlbwMQ_isEnabled() -> albw_mq_enabled(), the mod's master_quest config gate.
//   - the TARGET_PC guard is dropped; a mod is always PC.
// Every constant, tier gate, price and reg index is the fork's, unmodified.

/**
 * d_albw_sword_atp.cpp
 * ALBW Master Quest — per-sword Atp shop (regs 106–113).
 */


#include "sword_atp.h"
#include "albw_save_flags.h"
#include "albw_common.h"
#include "albw_game.h"
#include "config_vars.h"
#include "d/d_com_inf_game.h"
#include "d/d_item_data.h"

#include <cstdio>

namespace {

static constexpr int kTier1Steps = 5;
static constexpr int kTier2Steps = 5;
static constexpr int kTier3Steps = 5;
static constexpr int kTier3EndStep = kTier1Steps + kTier2Steps + kTier3Steps;  // 15

static constexpr int kTier1Prices[kTier1Steps] = {100, 200, 300, 400, 500};

static constexpr int kTier2BugGate[kTier2Steps]  = {4, 6, 8, 10, 12};
static constexpr int kTier2Prices[kTier2Steps]   = {100, 200, 300, 400, 500};

static constexpr int kTier3SoulGate[kTier3Steps] = {5, 12, 20, 27, 35};
static constexpr int kTier3Prices[kTier3Steps]   = {500, 625, 750, 875, 1000};

static constexpr u8 kSwordItemNos[kAlbwSwordAtpCount] = {
    static_cast<u8>(dItemNo_WOOD_STICK_e),
    static_cast<u8>(dItemNo_SWORD_e),
    static_cast<u8>(dItemNo_MASTER_SWORD_e),
    static_cast<u8>(dItemNo_LIGHT_SWORD_e),
};

static constexpr const char* kSwordNames[kAlbwSwordAtpCount] = {
    "Wooden Sword",
    "Ordon Sword",
    "Master Sword",
    "Light Sword",
};

// Event regs 106–109 = Atp bonus u8 ×4; 110–113 = purchase step u8 ×4.
// ============================================
// SAVE-DATA FIX (encoding repair, deliberate divergence from the fork): the fork
// (d_albw_sword_atp.cpp:45-63) adds swordId to the ENCODED value ((106<<8)|0xFF
// + id), which corrupts the low-byte clear-mask for swords 1-3 (they alias reg
// 107/111 with masks 0x00-0x02 — setEventReg clears ~mask then ORs, so values
// pile up unreadably). Encode per-index like every other reg user in this mod:
// ((base + id) << 8) | 0xFF. The fork has the same latent bug upstream.
// (These regs also used to collide with rental eligibility, which has moved to
// its fork-native saveBitLabels[673+] storage — see rental_eligibility.cpp.)
// ============================================
static constexpr int kBonusRegIndexBase = 106;
static constexpr int kStepRegIndexBase  = 110;

static char sDescBuf[kAlbwSwordAtpCount][256];

// Storage moved off the save into config.json (albw_save_flags.h). The
// reg-shaped helpers are kept so the call sites are unchanged; what they now
// pass is an AlbwSaveCounter id rather than an encoded register.
static u8 readReg(int counter) {
    return static_cast<u8>(albw_save_counter_get(counter));
}

static void writeReg(int counter, u8 value) {
    albw_save_counter_set(counter, value);
}

static int bonusRegFor(int swordId) {
    return ALBW_CTR_SWORD_ATP_BONUS_0 + swordId;
}

static int stepRegFor(int swordId) {
    return ALBW_CTR_SWORD_ATP_STEP_0 + swordId;
}

static int countInsectFirstBits() {
    static const u8 kInsects[] = {
        dItemNo_M_BEETLE_e,      dItemNo_F_BEETLE_e,      dItemNo_M_BUTTERFLY_e, dItemNo_F_BUTTERFLY_e,
        dItemNo_M_STAG_BEETLE_e, dItemNo_F_STAG_BEETLE_e, dItemNo_M_GRASSHOPPER_e, dItemNo_F_GRASSHOPPER_e,
        dItemNo_M_NANAFUSHI_e,   dItemNo_F_NANAFUSHI_e,   dItemNo_M_DANGOMUSHI_e,  dItemNo_F_DANGOMUSHI_e,
        dItemNo_M_MANTIS_e,      dItemNo_F_MANTIS_e,      dItemNo_M_LADYBUG_e,   dItemNo_F_LADYBUG_e,
        dItemNo_M_SNAIL_e,       dItemNo_F_SNAIL_e,       dItemNo_M_DRAGONFLY_e, dItemNo_F_DRAGONFLY_e,
        dItemNo_M_ANT_e,         dItemNo_F_ANT_e,         dItemNo_M_MAYFLY_e,    dItemNo_F_MAYFLY_e,
    };

    int count = 0;
    for (u8 itemNo : kInsects) {
        if (albw_game::is_item_first_bit(itemNo)) {
            count++;
        }
    }
    return count;
}

static bool swordIdValid(int swordId) {
    return swordId >= 0 && swordId < kAlbwSwordAtpCount;
}

static int tierForStep(int step) {
    if (step < kTier1Steps) {
        return 1;
    }
    if (step < kTier1Steps + kTier2Steps) {
        return 2;
    }
    if (step < kTier3EndStep) {
        return 3;
    }
    return 4;
}

static bool meetsGateForStep(int step) {
    if (step < kTier1Steps) {
        return true;
    }
    if (step < kTier1Steps + kTier2Steps) {
        const int idx = step - kTier1Steps;
        return countInsectFirstBits() >= kTier2BugGate[idx];
    }
    if (step < kTier3EndStep) {
        const int idx = step - kTier1Steps - kTier2Steps;
        return albw_game::poh_spirit_num() >= kTier3SoulGate[idx];
    }
    return albw_game::clear_count() > 0;
}

static int gateCountForStep(int step) {
    if (step < kTier1Steps + kTier2Steps && step >= kTier1Steps) {
        return kTier2BugGate[step - kTier1Steps];
    }
    if (step < kTier3EndStep) {
        return kTier3SoulGate[step - kTier1Steps - kTier2Steps];
    }
    return 0;
}

}  // namespace

u8 dAlbwSwordAtp_getItemNo(int swordId) {
    if (!swordIdValid(swordId)) {
        return static_cast<u8>(dItemNo_NONE_e);
    }
    return kSwordItemNos[swordId];
}

bool dAlbwSwordAtp_isSwordPossessed(int swordId) {
    if (!swordIdValid(swordId)) {
        return false;
    }
    return albw_game::is_item_first_bit(kSwordItemNos[swordId]);
}

bool dAlbwSwordAtp_pageHasVisibleRows() {
    if (!albw_mq_enabled()) {
        return false;
    }
    for (int i = 0; i < kAlbwSwordAtpCount; ++i) {
        if (dAlbwSwordAtp_isSwordPossessed(i)) {
            return true;
        }
    }
    return false;
}

int dAlbwSwordAtp_getBonus(int swordId) {
    if (!swordIdValid(swordId)) {
        return 0;
    }
    return readReg(bonusRegFor(swordId));
}

int dAlbwSwordAtp_getBonusForEquipped() {
    if (!albw_mq_enabled()) {
        return 0;
    }
    const u8 sword = albw_game::select_equip_sword();
    for (int i = 0; i < kAlbwSwordAtpCount; ++i) {
        if (kSwordItemNos[i] == sword) {
            return dAlbwSwordAtp_getBonus(i);
        }
    }
    return 0;
}

int dAlbwSwordAtp_getShopStep(int swordId) {
    if (!swordIdValid(swordId)) {
        return 0;
    }
    return readReg(stepRegFor(swordId));
}

int dAlbwSwordAtp_getShopPrice(int swordId) {
    if (!swordIdValid(swordId)) {
        return 0;
    }
    const int step = dAlbwSwordAtp_getShopStep(swordId);
    if (step < kTier1Steps) {
        return kTier1Prices[step];
    }
    if (step < kTier1Steps + kTier2Steps) {
        return kTier2Prices[step - kTier1Steps];
    }
    if (step < kTier3EndStep) {
        return kTier3Prices[step - kTier1Steps - kTier2Steps];
    }
    return kTier1Prices[(step - kTier3EndStep) % kTier1Steps];
}

bool dAlbwSwordAtp_canPurchase(int swordId) {
    if (!albw_mq_enabled() || !dAlbwSwordAtp_isSwordPossessed(swordId)) {
        return false;
    }
    const int step  = dAlbwSwordAtp_getShopStep(swordId);
    const int bonus = dAlbwSwordAtp_getBonus(swordId);
    if (bonus >= 255) {
        return false;
    }
    if (step >= kTier3EndStep && albw_game::clear_count() == 0) {
        return false;
    }
    if (step >= kTier3EndStep && bonus >= 254) {
        return false;
    }
    return true;
}

bool dAlbwSwordAtp_tryPurchase(int swordId) {
    if (!dAlbwSwordAtp_canPurchase(swordId)) {
        return false;
    }
    const int step = dAlbwSwordAtp_getShopStep(swordId);
    if (!meetsGateForStep(step)) {
        return false;
    }
    const int gain = (step >= kTier3EndStep) ? 2 : 1;
    const int bonus = dAlbwSwordAtp_getBonus(swordId);
    if (bonus + gain > 255) {
        return false;
    }
    writeReg(bonusRegFor(swordId), static_cast<u8>(bonus + gain));
    if (step < 255) {
        writeReg(stepRegFor(swordId), static_cast<u8>(step + 1));
    }
    return true;
}

const char* dAlbwSwordAtp_getShopName(int swordId) {
    if (!swordIdValid(swordId)) {
        return "";
    }
    return kSwordNames[swordId];
}

const char* dAlbwSwordAtp_getShopDesc(int swordId) {
    if (!swordIdValid(swordId)) {
        return "";
    }

    char* buf = sDescBuf[swordId];
    const int step = dAlbwSwordAtp_getShopStep(swordId);

    if (!dAlbwSwordAtp_canPurchase(swordId)) {
        if (dAlbwSwordAtp_getBonus(swordId) >= 255) {
            std::snprintf(buf, sizeof(sDescBuf[swordId]), "%s", "Sold out.");
            return buf;
        }
        if (step >= kTier3EndStep && albw_game::clear_count() == 0) {
            std::snprintf(buf, sizeof(sDescBuf[swordId]), "%s",
                          "Sharpening secrets await a second journey.");
            return buf;
        }
    }

    switch (tierForStep(step)) {
    case 1:
    case 4:
        std::snprintf(buf, sizeof(sDescBuf[swordId]), "%s",
                      " I hope you don't mind, but I've been using your house to sleep in every "
                      "now and then, and I found this whetstone. I can sharpen your sword to "
                      "repay your hospitality!");
        break;
    case 2: {
        const int n = gateCountForStep(step);
        std::snprintf(buf, sizeof(sDescBuf[swordId]),
                      " I'm going to let you in on a new tip from a master swordsman in castle "
                      "town, they say if you lather the pheremones of %d bugs, your sword will "
                      "get stronger",
                      n);
        break;
    }
    case 3: {
        const int n = gateCountForStep(step);
        std::snprintf(buf, sizeof(sDescBuf[swordId]),
                      "A gruff resistance member scolded me recently, saying slathering bugs on a "
                      "sword never works. The true secret is refining with %d souls, creepy magic "
                      "but true.",
                      n);
        break;
    }
    default:
        buf[0] = '\0';
        break;
    }
    return buf;
}
