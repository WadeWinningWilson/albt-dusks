#include "mq_hearts.h"
#include "albw_save_flags.h"

#include "albw_common.h"
#include "albw_game.h"
#include "config_vars.h"

#include "d/d_com_inf_game.h"
#include "mods/svc/hook.hpp"

namespace {

// ============================================
// NEW CODE - ALBW Port (bonus hearts actually raise max HP)
// The heart-shop grant writes the bonus half/quarter-heart regs, but the mod
// never APPLIED them to the real max-life gauge, so purchases did nothing. The
// fork adds the bonus inside dComIfGs_getMaxLifeGauge()
// (d_com_inf_game.cpp:2051): gauge += dAlbwMQ_getBonusMaxLifeQuarters(). Stock
// getMaxLifeGauge lacks that, so we hook it and add the bonus (the whole life
// system reads this accessor, so the extra hearts become real everywhere).
// ============================================

static constexpr int kHeartShopTiers = 17;
static constexpr int kMeterShopTiers = 23;
static constexpr int kMeterUnitsPerBuy = 632;

static constexpr int kHeartShopTierReg = ALBW_CTR_HEART_SHOP_TIER;
static constexpr int kMeterShopTierReg = ALBW_CTR_METER_SHOP_TIER;
static constexpr int kBonusHalfHeartsReg = ALBW_CTR_BONUS_HALF_HEARTS;
static constexpr int kBonusQuarterHeartsReg = ALBW_CTR_BONUS_QUARTER_HEARTS;

static constexpr int kHeartShopPrices[kHeartShopTiers] = {
    225, 250, 275, 325, 375, 425, 500, 575, 675, 800,
    1000, 2100, 3200, 4000, 5000, 8000, 9999,
};

static constexpr int kMeterShopPrices[kMeterShopTiers] = {
    100, 200, 300, 400, 500, 600, 700, 800, 900, 1000, 1200, 1400, 1600, 1800, 2000, 2250,
    2500, 2750, 3000, 3033, 3333, 3333, 3333,
};

// Storage moved off the save into config.json (albw_save_flags.h). These
// counters were only ever read back by this module - the bonus reaches the
// game through the getMaxLifeGauge POST hook below, never by writing the
// stock heart count - so nothing in the engine needs to see them.
u8 readReg(int counter) {
    return static_cast<u8>(albw_save_counter_get(counter));
}

void writeReg(int counter, u8 value) {
    albw_save_counter_set(counter, value);
}

// Fork grantHalfHeartMaxCapacity (d_albw_master_quest.cpp:63): bump the bonus
// half-heart reg (grows getMaxLifeGauge) and queue a +2 current-life heal. The
// queued heal is what makes moveLife animate the row up into the new capacity;
// the container growth itself comes from moveLife tracking getDisplayMaxLifeInternal.
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

// fork dAlbwMQ_getBonusMaxLifeQuarters (d_albw_master_quest.cpp:88): the bought
// bonus expressed in quarter-hearts (a half heart = 2 quarters).
int mqBonusMaxLifeQuarters() {
    if (!albw_cfg_bool(g_master_quest, false)) {
        return 0;
    }
    return static_cast<int>(readReg(kBonusHalfHeartsReg)) * 2 +
           static_cast<int>(readReg(kBonusQuarterHeartsReg));
}

// Typed hook: dComIfGs_getMaxLifeGauge is header-declared
// (d/d_com_inf_game.h:1107) and has no inline definition, so the compiler
// mangles it per target instead of relying on a bare name being present in the
// host's symbol manifest. See albw_symbols.h.
DEFINE_HOOK(&dComIfGs_getMaxLifeGauge, MqMaxLifeGauge);

void on_get_max_life_gauge_post(ModContext*, void*, void* retval, void*) {
    if (retval == nullptr) {
        return;
    }
    const int bonus = mqBonusMaxLifeQuarters();
    u16* gauge = static_cast<u16*>(retval);
    if (bonus != 0) {
        *gauge = static_cast<u16>(*gauge + bonus);
    }
}

}  // namespace

bool albw_mq_is_enabled() {
    return albw_cfg_bool(g_master_quest, false);
}

ModResult albw_mq_hearts_init(ModError*) {
    // Apply bought bonus hearts to the real max-life gauge everywhere it is read.
    if (mods::hook::add_post<MqMaxLifeGauge>(on_get_max_life_gauge_post) != MOD_OK) {
        svc_log->error(mod_ctx, "failed to hook dComIfGs_getMaxLifeGauge (mq hearts)");
        return MOD_ERROR;
    }
    return MOD_OK;
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
    return "Heart Upgrade";  // fork dAlbwMQ_getHeartShopName (d_albw_master_quest.cpp:46)
}

// fork dAlbwMQ_getHeartShopDesc (d_albw_master_quest.cpp:53) — dynamic by tier.
const char* albw_mq_heart_shop_desc() {
    if (!albw_mq_can_purchase_heart_shop()) {
        return "Sold out.";
    }
    if (pastSoftCap(heartTier())) {
        return "Permanently increases your maximum health by a quarter heart.";
    }
    return "Permanently increases your maximum health by half a heart.";
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
    return "Permanently expands your ALBW stamina meter.";  // fork d_albw_master_quest.cpp:68
}

// ============================================
// NEW CODE - ALBW Port
// Fork gate for Master Quest features (dAlbwMQ_isEnabled). Exposed so sword_atp
// reads the same config key rather than keeping a second notion of "MQ on".
// ============================================
bool albw_mq_enabled() {
    return albw_cfg_bool(g_master_quest, false);
}
