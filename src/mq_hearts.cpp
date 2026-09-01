#include "mq_hearts.h"

#include "albw_common.h"
#include "albw_game.h"
#include "config_vars.h"

#include "d/d_com_inf_game.h"

namespace {

static constexpr int kHeartShopTiers = 17;
static constexpr int kMeterShopTiers = 23;
static constexpr int kMeterUnitsPerBuy = 632;

static constexpr u16 kHeartShopTierReg = static_cast<u16>(100 << 8) | 0xFF;
static constexpr u16 kMeterShopTierReg = static_cast<u16>(101 << 8) | 0xFF;
static constexpr u16 kBonusHalfHeartsReg = static_cast<u16>(102 << 8) | 0xFF;
static constexpr u16 kBonusQuarterHeartsReg = static_cast<u16>(104 << 8) | 0xFF;

static constexpr int kHeartShopPrices[kHeartShopTiers] = {
    225, 250, 275, 325, 375, 425, 500, 575, 675, 800,
    1000, 2100, 3200, 4000, 5000, 8000, 9999,
};

static constexpr int kMeterShopPrices[kMeterShopTiers] = {
    100, 200, 300, 400, 500, 600, 700, 800, 900, 1000, 1200, 1400, 1600, 1800, 2000, 2250,
    2500, 2750, 3000, 3033, 3333, 3333, 3333,
};

u8 readReg(u16 reg) {
    return albw_game::get_event_reg(reg);
}

void writeReg(u16 reg, u8 value) {
    albw_game::set_event_reg(reg, value);
}

void grantHalfHeart() {
    const u8 halves = readReg(kBonusHalfHeartsReg);
    if (halves >= 255) {
        return;
    }
    writeReg(kBonusHalfHeartsReg, static_cast<u8>(halves + 1));
    g_dComIfG_gameInfo.play.setItemLifeCount(2.0f, 0);
}

void grantQuarterHeart() {
    const u8 quarters = readReg(kBonusQuarterHeartsReg);
    if (quarters >= 255) {
        return;
    }
    writeReg(kBonusQuarterHeartsReg, static_cast<u8>(quarters + 1));
    g_dComIfG_gameInfo.play.setItemLifeCount(1.0f, 0);
}

int heartTier() {
    return readReg(kHeartShopTierReg);
}

int meterTier() {
    const int tier = readReg(kMeterShopTierReg);
    if (tier > kMeterShopTiers) {
        return kMeterShopTiers;
    }
    return tier;
}

bool pastSoftCap(int tier) {
    return tier >= kHeartShopTiers;
}

int heartPrice(int tier) {
    if (tier < 0) {
        return 0;
    }
    if (tier < kHeartShopTiers) {
        return kHeartShopPrices[tier];
    }
    return 9999 + 333 * (tier - (kHeartShopTiers - 1));
}

}  // namespace

bool albw_mq_is_enabled() {
    return albw_cfg_bool(g_master_quest, false);
}

bool albw_mq_can_purchase_heart_shop() {
    if (!albw_mq_is_enabled()) {
        return false;
    }
    const int tier = heartTier();
    if (pastSoftCap(tier)) {
        return readReg(kBonusQuarterHeartsReg) < 255;
    }
    return readReg(kBonusHalfHeartsReg) < 255;
}

bool albw_mq_try_purchase_heart_shop() {
    if (!albw_mq_can_purchase_heart_shop()) {
        return false;
    }
    const int tier = heartTier();
    if (pastSoftCap(tier)) {
        grantQuarterHeart();
    } else {
        grantHalfHeart();
    }
    if (tier < 255) {
        writeReg(kHeartShopTierReg, static_cast<u8>(tier + 1));
    }
    return true;
}

int albw_mq_heart_shop_price() {
    return heartPrice(heartTier());
}

const char* albw_mq_heart_shop_name() {
    return "Heart Container Upgrade";
}

const char* albw_mq_heart_shop_desc() {
    return "Master Quest shrinks heart rewards — buy back capacity here, one half-heart at a time.";
}

int albw_mq_get_meter_shop_tiers() {
    if (!albw_mq_is_enabled()) {
        return 0;
    }
    return meterTier();
}

int albw_mq_meter_bonus_units() {
    return albw_mq_get_meter_shop_tiers() * kMeterUnitsPerBuy;
}

bool albw_mq_can_purchase_meter_shop() {
    if (!albw_mq_is_enabled()) {
        return false;
    }
    return meterTier() < kMeterShopTiers;
}

bool albw_mq_try_purchase_meter_shop() {
    if (!albw_mq_can_purchase_meter_shop()) {
        return false;
    }
    writeReg(kMeterShopTierReg, static_cast<u8>(meterTier() + 1));
    return true;
}

int albw_mq_meter_shop_price() {
    const int tier = meterTier();
    if (tier < 0 || tier >= kMeterShopTiers) {
        return 0;
    }
    return kMeterShopPrices[tier];
}

const char* albw_mq_meter_shop_name() {
    return "Stamina Upgrade";
}

const char* albw_mq_meter_shop_desc() {
    return "Raises ALBW meter capacity. Sold at the Postman shop when rental UI ships.";
}

// ============================================
// NEW CODE - ALBW Port
// Fork gate for Master Quest features (dAlbwMQ_isEnabled). Exposed so sword_atp
// reads the same config key rather than keeping a second notion of "MQ on".
// ============================================
bool albw_mq_enabled() {
    return albw_cfg_bool(g_master_quest, false);
}
