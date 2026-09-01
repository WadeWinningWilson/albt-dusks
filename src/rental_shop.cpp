// Port of fork d_albw_rental session FSM + reclaim catalog (Phase 2C).

#include "rental_shop.h"

#include "albw_game.h"
#include "config_vars.h"
#include "focused_arts.h"
#include "mq_hearts.h"
#include "oocoo.h"
#include "rental_eligibility.h"
#include "rental_postman_hooks.h"
#include "shield_game.h"
#include "sword_atp.h"

#include "d/d_com_inf_game.h"
#include "d/d_item_data.h"
#include "d/d_save.h"
#include "m_Do/m_Do_controller_pad.h"

#include <cstring>

#define private public
#include "d/actor/d_a_alink.h"
#undef private

namespace {

enum ALBWRentalState {
    STATE_CLOSED = 0,
    STATE_GREETING,
    STATE_SHOP,
    STATE_FAREWELL,
};

enum ALBWShopCategory {
    CAT_ITEMS = 0,
    CAT_SHIELDS,
    CAT_ARMOR,
    CAT_UPGRADES,
    CAT_SWORD_ATP,  // Master Quest per-sword Atp upgrades (fork d_albw_rental.cpp:78)
    CAT_COUNT,
};

enum VisibleKind {
    VISIBLE_ITEM = 0,
    VISIBLE_OOCOO,
    VISIBLE_MQ_HEART,
    VISIBLE_MQ_METER,
    VISIBLE_FA_TIER,
    VISIBLE_SWORD_ATP,  // fork d_albw_rental.cpp:303
};

struct ALBWRentalEntry {
    const char* name;
    u8 itemNo;
    int slotNo;
    int price;
    const char* desc;
    ALBWShopCategory category;
    bool isShield;
    bool isClothes;
};

struct PageDef {
    ALBWShopCategory category;
    const char* title;
};

struct VisibleEntry {
    VisibleKind kind;
    int catalogIdx;
    bool purchasable;
};

constexpr int kVisibleListMax = 48;

ALBWRentalState sState = STATE_CLOSED;
int s_menuResGen = 0;

bool sJustClosed = false;
bool sJustEnteredGreeting = false;
bool sJustEnteredShop = false;
bool sJustEnteredFarewell = false;
bool sJustPurchased = false;
bool sJustFailedPurchase = false;
bool sHideVanillaTalkMsg = false;
bool sNativeDialogueJustDismissed = false;
bool sPurchasedThisSession = false;

const char* sGreetingText = "";
const char* sGreetingPage2 = nullptr;
const char* sGreetingPage3 = nullptr;
const char* sFarewellText = "";

int sSelectedIdx = 0;
int sStickNavCooldown = 0;
int sCurrentPageIdx = 0;
int sActivePageCount = 0;
ALBWShopCategory sActivePages[CAT_COUNT] = {};

VisibleEntry sVisibleList[kVisibleListMax] = {};
int sVisibleCount = 0;
dALBWVisibleEntry sPubList[kVisibleListMax] = {};

// Fork tab order minus the deferred Swords page (that one is Quick-Swap
// wardrobe storage and needs d_albw_wardrobe, which is not ported).
static const PageDef kPages[] = {
    {CAT_ITEMS, "Items"},
    {CAT_SWORD_ATP, "Sword Upgrades"},
    {CAT_SHIELDS, "Shields"},
    {CAT_ARMOR, "Armor"},
    {CAT_UPGRADES, "Upgrades & Services"},
};
static constexpr int kPageCount = sizeof(kPages) / sizeof(kPages[0]);

static const ALBWRentalEntry kItems[] = {
    {"Slingshot", (u8)dItemNo_PACHINKO_e, SLOT_23, 15,
     "A child's reliable pellet launcher, perfect for tiny insects.", CAT_ITEMS, false, false},
    {"Boomerang", (u8)dItemNo_BOOMERANG_e, SLOT_0, 30,
     "A mysterious boomerang that supposedly houses the Fairy of Winds...has a pungent odor",
     CAT_ITEMS, false, false},
    {"Bombs", (u8)dItemNo_BOMB_BAG_LV1_e, SLOT_15, 50, "Standard-issue explosives, handle with care",
     CAT_ITEMS, false, false},
    {"Bomblings", (u8)dItemNo_POKE_BOMB_e, SLOT_17, 50,
     "These little fellows roll themselves...why did my boss bring these here", CAT_ITEMS, false,
     false},
    {"Water Bombs", (u8)dItemNo_BOMB_BAG_LV2_e, SLOT_16, 150,
     "Specially sealed for aquatic demolition...", CAT_ITEMS, false, false},
    {"Dominion Rod", (u8)dItemNo_COPY_ROD_e, SLOT_8, 150,
     "Bring to life statues of old...no refunds if broken on transit", CAT_ITEMS, false, false},
    {"Clawshot", (u8)dItemNo_HOOKSHOT_e, SLOT_9, 100,
     "Long gone are the days of single hook traversal", CAT_ITEMS, false, false},
    {"Bow", (u8)dItemNo_BOW_e, SLOT_4, 150,
     "A sacred treasure, moistuerize properly to undo wood warping", CAT_ITEMS, false, false},
    {"Double Clawshot", (u8)dItemNo_W_HOOKSHOT_e, SLOT_10, 200,
     "Double the traversal....I need a pair like these", CAT_ITEMS, false, false},
    {"Spinner", (u8)dItemNo_SPINNER_e, SLOT_2, 200,
     "Ancient item that grants you.......what is this darn thing?", CAT_ITEMS, false, false},
    {"Ball and Chain", (u8)dItemNo_IRONBALL_e, SLOT_6, 350,
     "Heavy...heavy...please pick it up yourself", CAT_ITEMS, false, false},
    {"Ordon Shield", (u8)dItemNo_WOOD_SHIELD_e, -1, 50,
     "A humble offering for the Princess' Royal Family. Do be more gentle with it now!", CAT_SHIELDS,
     true, false},
    {"Wooden Shield", (u8)dItemNo_SHIELD_e, -1, 150,
     "A sturdier wooden shield with subtle metal reinforcements....sadly didn't stop me "
     "from getting this nasty splinter.",
     CAT_SHIELDS, true, false},
    {"Hylian Shield", (u8)dItemNo_HYLIA_SHIELD_e, -1, 500,
     "An expert duelists' shield of choice, passed down by the Hyrulean Royal family.", CAT_SHIELDS,
     true, false},
    {"Ordon Clothes", (u8)dItemNo_WEAR_CASUAL_e, -1, 50,
     "Humble small town wear fit for any...unassuming explorer.", CAT_ARMOR, false, true},
    {"Hero's Clothes", (u8)dItemNo_WEAR_KOKIRI_e, -1, 50,
     "You know, I had an ancestor that swore he saw a legendary hero wear a garb like this.",
     CAT_ARMOR, false, true},
    {"Zora Armor", (u8)dItemNo_WEAR_ZORA_e, -1, 200,
     "Scaly and sleek — for diving where others drown.", CAT_ARMOR, false, true},
};
static constexpr int kItemCount = sizeof(kItems) / sizeof(kItems[0]);

bool clothesEligible(const ALBWRentalEntry& e) {
    switch (e.itemNo) {
    case (u8)dItemNo_WEAR_CASUAL_e:
        return albw_game::is_event_bit(dSv_event_flag_c::saveBitLabels[47]);
    case (u8)dItemNo_WEAR_KOKIRI_e:
        return g_dComIfG_gameInfo.info.getPlayer().getCollect().isCollect(COLLECT_CLOTHING,
                                                                         KOKIRI_CLOTHES_FLAG) != 0 ||
               albw_game::is_item_first_bit((u8)dItemNo_WEAR_KOKIRI_e);
    case (u8)dItemNo_WEAR_ZORA_e:
        return albw_game::is_item_first_bit((u8)dItemNo_WEAR_ZORA_e);
    default:
        return false;
    }
}

bool entryEligible(const ALBWRentalEntry& e) {
    if (e.isClothes) {
        return clothesEligible(e);
    }
    if (e.isShield) {
        return albw_rental_is_shield_eligible(e.itemNo);
    }
    return albw_rental_is_eligible(e.itemNo);
}

bool slotHasPossessionForm(u8 rentalItemNo) {
    if (albw_game::is_item_first_bit(rentalItemNo)) {
        return true;
    }
    for (int slot = SLOT_0; slot <= SLOT_23; slot++) {
        if (g_dComIfG_gameInfo.info.getPlayer().getItem().getItem(slot, false) == rentalItemNo) {
            return true;
        }
    }
    if (rentalItemNo == (u8)dItemNo_BOMB_BAG_LV1_e) {
        for (int slot = SLOT_0; slot <= SLOT_23; slot++) {
            if (g_dComIfG_gameInfo.info.getPlayer().getItem().getItem(slot, false) ==
                (u8)dItemNo_NORMAL_BOMB_e)
            {
                return true;
            }
        }
    } else if (rentalItemNo == (u8)dItemNo_BOMB_BAG_LV2_e) {
        for (int slot = SLOT_0; slot <= SLOT_23; slot++) {
            if (g_dComIfG_gameInfo.info.getPlayer().getItem().getItem(slot, false) ==
                (u8)dItemNo_WATER_BOMB_e)
            {
                return true;
            }
        }
    } else if (rentalItemNo == (u8)dItemNo_COPY_ROD_e) {
        for (int slot = SLOT_0; slot <= SLOT_23; slot++) {
            if (g_dComIfG_gameInfo.info.getPlayer().getItem().getItem(slot, false) ==
                (u8)dItemNo_COPY_ROD_2_e)
            {
                return true;
            }
        }
    }
    return false;
}

bool playerOwns(const ALBWRentalEntry& e) {
    if (e.isClothes) {
        // Outfit switcher rows stay listed; ownership does not hide them.
        return false;
    }
    if (e.isShield) {
        return albw_shield_game::get_select_equip_shield() == e.itemNo;
    }
    return slotHasPossessionForm(e.itemNo);
}

bool itemRowVisible(const ALBWRentalEntry& e) {
    // Hide tools/shields the player currently owns. Clothes stay listed.
    return e.isClothes || !playerOwns(e);
}

bool categoryHasContent(ALBWShopCategory cat) {
    // fork d_albw_rental.cpp:504
    if (cat == CAT_SWORD_ATP && dAlbwSwordAtp_pageHasVisibleRows()) {
        return true;
    }
    if (cat == CAT_UPGRADES) {
        if (albw_oocoo_can_show_in_shop()) {
            return true;
        }
        if (albw_mq_is_enabled()) {
            return true;
        }
        if (dFocusedArts_shouldShowShopTierRow()) {
            return true;
        }
        return false;
    }
    for (int i = 0; i < kItemCount; ++i) {
        if (kItems[i].category == cat && itemRowVisible(kItems[i])) {
            return true;
        }
    }
    return false;
}

void rebuildActivePages() {
    sActivePageCount = 0;
    for (int p = 0; p < kPageCount; ++p) {
        if (categoryHasContent(kPages[p].category)) {
            sActivePages[sActivePageCount++] = kPages[p].category;
        }
    }
    if (sActivePageCount == 0) {
        sActivePages[sActivePageCount++] = CAT_ITEMS;
    }
    if (sCurrentPageIdx >= sActivePageCount) {
        sCurrentPageIdx = 0;
    }
}

ALBWShopCategory currentCategory() {
    if (sActivePageCount <= 0) {
        return CAT_ITEMS;
    }
    return sActivePages[sCurrentPageIdx];
}

void appendVisible(VisibleKind kind, int catalogIdx, bool purchasable) {
    if (sVisibleCount >= kVisibleListMax) {
        return;
    }
    sVisibleList[sVisibleCount].kind = kind;
    sVisibleList[sVisibleCount].catalogIdx = catalogIdx;
    sVisibleList[sVisibleCount].purchasable = purchasable;
    ++sVisibleCount;
}

void rebuildVisibleList() {
    sVisibleCount = 0;
    std::memset(sVisibleList, 0, sizeof(sVisibleList));
    const ALBWShopCategory cat = currentCategory();

    // fork d_albw_rental.cpp:696 - one row per possessed sword, gated on MQ.
    if (cat == CAT_SWORD_ATP && albw_mq_is_enabled()) {
        for (int swordId = 0; swordId < kAlbwSwordAtpCount; ++swordId) {
            if (!dAlbwSwordAtp_isSwordPossessed(swordId)) {
                continue;
            }
            appendVisible(VISIBLE_SWORD_ATP, swordId, dAlbwSwordAtp_canPurchase(swordId));
        }
    }

    if (cat == CAT_UPGRADES) {
        if (albw_mq_is_enabled()) {
            appendVisible(VISIBLE_MQ_HEART, -1, albw_mq_can_purchase_heart_shop());
            appendVisible(VISIBLE_MQ_METER, -1, albw_mq_can_purchase_meter_shop());
        }
        if (dFocusedArts_shouldShowShopTierRow()) {
            appendVisible(VISIBLE_FA_TIER, -1, dFocusedArts_canPurchaseShopTier());
        }
        if (albw_oocoo_can_show_in_shop()) {
            appendVisible(VISIBLE_OOCOO, -1, true);
        }
    }

    for (int i = 0; i < kItemCount; ++i) {
        const ALBWRentalEntry& entry = kItems[i];
        if (entry.category != cat) {
            continue;
        }
        const bool eligible = entryEligible(entry);
        const bool owned = playerOwns(entry);
        // Tools/shields: hide while owned. Clothes: always list once eligible path applies.
        if (owned) {
            continue;
        }
        appendVisible(VISIBLE_ITEM, i, eligible && !owned);
    }

    if (sSelectedIdx >= sVisibleCount) {
        sSelectedIdx = (sVisibleCount > 0) ? sVisibleCount - 1 : 0;
    }
}

void changePage(int delta) {
    if (sActivePageCount <= 1) {
        return;
    }
    sCurrentPageIdx = (sCurrentPageIdx + delta + sActivePageCount) % sActivePageCount;
    sSelectedIdx = 0;
    rebuildVisibleList();
}

u16 getRupees() {
    return g_dComIfG_gameInfo.info.getPlayer().getPlayerStatusA().getRupee();
}

void setRupees(u16 v) {
    g_dComIfG_gameInfo.info.getPlayer().getPlayerStatusA().setRupee(v);
}

void grantTool(const ALBWRentalEntry& e) {
    g_dComIfG_gameInfo.info.getPlayer().getGetItem().onFirstBit(e.itemNo);
    if (e.slotNo >= 0) {
        g_dComIfG_gameInfo.info.getPlayer().getItem().setItem(e.slotNo, e.itemNo);
    }
}

void grantShield(u8 itemNo) {
    g_dComIfG_gameInfo.info.getPlayer().getGetItem().onFirstBit(itemNo);
    albw_shield_game::set_shield(itemNo, false);
}

void grantClothes(u8 itemNo) {
    g_dComIfG_gameInfo.info.getPlayer().getGetItem().onFirstBit(itemNo);
    g_dComIfG_gameInfo.info.getPlayer().getPlayerStatusA().setSelectEquip(COLLECT_CLOTHING, itemNo);
    if (itemNo == (u8)dItemNo_WEAR_KOKIRI_e) {
        g_dComIfG_gameInfo.info.getPlayer().getCollect().setCollect(COLLECT_CLOTHING,
                                                                   KOKIRI_CLOTHES_FLAG);
    }
    daPy_py_c* player = albw_game::link_player();
    if (player != nullptr) {
        player->setClothesChange(0);
    }
}

void tryPurchase(int visIdx) {
    if (visIdx < 0 || visIdx >= sVisibleCount) {
        return;
    }
    const VisibleEntry& row = sVisibleList[visIdx];
    if (!row.purchasable) {
        return;
    }

    if (row.kind == VISIBLE_OOCOO) {
        if (!albw_oocoo_try_purchase()) {
            sJustFailedPurchase = true;
            return;
        }
        sPurchasedThisSession = true;
        sJustPurchased = true;
        rebuildActivePages();
        rebuildVisibleList();
        return;
    }
    if (row.kind == VISIBLE_MQ_HEART) {
        const u16 price = static_cast<u16>(albw_mq_heart_shop_price());
        const u16 rupees = getRupees();
        if (rupees < price) {
            sJustFailedPurchase = true;
            return;
        }
        setRupees(rupees - price);
        if (!albw_mq_try_purchase_heart_shop()) {
            setRupees(rupees);
            sJustFailedPurchase = true;
            return;
        }
        sPurchasedThisSession = true;
        sJustPurchased = true;
        rebuildVisibleList();
        return;
    }
    if (row.kind == VISIBLE_MQ_METER) {
        const u16 price = static_cast<u16>(albw_mq_meter_shop_price());
        const u16 rupees = getRupees();
        if (rupees < price) {
            sJustFailedPurchase = true;
            return;
        }
        setRupees(rupees - price);
        if (!albw_mq_try_purchase_meter_shop()) {
            setRupees(rupees);
            sJustFailedPurchase = true;
            return;
        }
        sPurchasedThisSession = true;
        sJustPurchased = true;
        rebuildVisibleList();
        return;
    }
    // fork d_albw_rental.cpp:1057. The fork also sets sStatusMsg strings here;
    // this shop has no status-message system (unported), so it reports through
    // sJustFailedPurchase / sJustPurchased like every other row does.
    if (row.kind == VISIBLE_SWORD_ATP) {
        const int swordId = row.catalogIdx;
        const int price = dAlbwSwordAtp_getShopPrice(swordId);
        if (price <= 0) {
            return;
        }
        const u16 rupees = getRupees();
        if (rupees < static_cast<u16>(price)) {
            sJustFailedPurchase = true;
            return;
        }
        if (!dAlbwSwordAtp_tryPurchase(swordId)) {
            sJustFailedPurchase = true;
            return;
        }
        setRupees(static_cast<u16>(rupees - static_cast<u16>(price)));
        sPurchasedThisSession = true;
        sJustPurchased = true;
        rebuildActivePages();
        rebuildVisibleList();
        return;
    }
    if (row.kind == VISIBLE_FA_TIER) {
        const int price = dFocusedArts_getNextShopTierPrice();
        if (price <= 0) {
            return;
        }
        const u16 rupees = getRupees();
        if (rupees < static_cast<u16>(price)) {
            sJustFailedPurchase = true;
            return;
        }
        if (!dFocusedArts_tryPurchaseShopTier()) {
            sJustFailedPurchase = true;
            return;
        }
        setRupees(rupees - static_cast<u16>(price));
        sPurchasedThisSession = true;
        sJustPurchased = true;
        rebuildActivePages();
        rebuildVisibleList();
        return;
    }

    const ALBWRentalEntry& e = kItems[row.catalogIdx];
    const u16 rupees = getRupees();
    if (rupees < static_cast<u16>(e.price)) {
        sJustFailedPurchase = true;
        return;
    }
    if (e.isShield && albw_shield_game::get_select_equip_shield() != dItemNo_NONE_e &&
        albw_shield_game::get_select_equip_shield() != 0xFF)
    {
        sJustFailedPurchase = true;
        return;
    }

    setRupees(rupees - static_cast<u16>(e.price));
    if (e.isClothes) {
        grantClothes(e.itemNo);
    } else if (e.isShield) {
        grantShield(e.itemNo);
    } else {
        grantTool(e);
    }
    sPurchasedThisSession = true;
    sJustPurchased = true;
    rebuildActivePages();
    rebuildVisibleList();
}

bool hasAnyEligible() {
    for (int i = 0; i < kItemCount; ++i) {
        if (entryEligible(kItems[i])) {
            return true;
        }
    }
    return false;
}

}  // namespace

bool albw_rental_shop_is_open() {
    return sState != STATE_CLOSED;
}

void albw_rental_shop_set_open(bool) {}

int albw_rental_menu_res_swap_generation() {
    return s_menuResGen;
}

void albw_rental_menu_res_bump_generation() {
    ++s_menuResGen;
}

void dALBWRental_open() {
    if (sState != STATE_CLOSED) {
        return;
    }

    sPurchasedThisSession = false;
    sJustClosed = false;
    sJustEnteredGreeting = false;
    sJustEnteredShop = false;
    sJustEnteredFarewell = false;
    sJustPurchased = false;
    sJustFailedPurchase = false;
    sNativeDialogueJustDismissed = false;
    sSelectedIdx = 0;
    sStickNavCooldown = 0;
    sCurrentPageIdx = 0;

    rebuildActivePages();
    if (albw_oocoo_can_show_in_shop()) {
        for (int p = 0; p < sActivePageCount; ++p) {
            if (sActivePages[p] == CAT_UPGRADES) {
                sCurrentPageIdx = p;
                break;
            }
        }
    }
    rebuildVisibleList();

    const bool returning = hasAnyEligible();
    if (returning) {
        sGreetingText =
            "Greetings! Lost your treasured possessions? Never fear, I have a new shipment for "
            "you! All for a.....small fee!";
        sGreetingPage2 = nullptr;
    } else {
        sGreetingText =
            "Greetings! As an ever dutiful Junior mail carrier I return all that is lost or "
            "misplaced!";
        sGreetingPage2 =
            "It seems to me that this is your first time using our services, browse away and keep "
            "us in mind. You may never know when something slips off your person!";
    }
    sGreetingPage3 = nullptr;

    sState = STATE_GREETING;
    sJustEnteredGreeting = true;
    sHideVanillaTalkMsg = true;
    albw_rental_menu_res_bump_generation();
}

void dALBWRental_close() {
    if (sState != STATE_SHOP) {
        return;
    }

    sFarewellText = sPurchasedThisSession
        ? "Ahhh isn't it a joy to be reunited with one's cherished belongings? "
          "Well our business is concluded, onward to packaging!"
        : "My friend do not shed a tear, save up and return. "
          "Even I can't leave this town yet without a rupee or two more in the bank!";

    sState = STATE_FAREWELL;
    sJustEnteredFarewell = true;
}

bool dALBWRental_isOpen() {
    return sState != STATE_CLOSED;
}

void dALBWRental_armVanillaTalkSuppress() {
    sHideVanillaTalkMsg = true;
}

void dALBWRental_clearVanillaTalkSuppress() {
    sHideVanillaTalkMsg = false;
}

bool dALBWRental_shouldSuppressVanillaTalkMsg() {
    if (sState == STATE_CLOSED) {
        sHideVanillaTalkMsg = false;
    }
    return sHideVanillaTalkMsg || dALBWRental_isOpen();
}

bool dALBWRental_justClosed() {
    if (sJustClosed) {
        sJustClosed = false;
        return true;
    }
    return false;
}

bool dALBWRental_justEnteredGreeting() {
    if (sJustEnteredGreeting) {
        sJustEnteredGreeting = false;
        return true;
    }
    return false;
}

bool dALBWRental_justEnteredShop() {
    if (sJustEnteredShop) {
        sJustEnteredShop = false;
        return true;
    }
    return false;
}

bool dALBWRental_justEnteredFarewell() {
    if (sJustEnteredFarewell) {
        sJustEnteredFarewell = false;
        return true;
    }
    return false;
}

bool dALBWRental_justPurchased() {
    if (sJustPurchased) {
        sJustPurchased = false;
        return true;
    }
    return false;
}

bool dALBWRental_justFailedPurchase() {
    if (sJustFailedPurchase) {
        sJustFailedPurchase = false;
        return true;
    }
    return false;
}

void dALBWRental_tick() {
    if (sState == STATE_CLOSED || sState == STATE_GREETING || sState == STATE_FAREWELL) {
        return;
    }

    if (sNativeDialogueJustDismissed) {
        sNativeDialogueJustDismissed = false;
        return;
    }

    if (mDoCPd_c::getTrigB(PAD_1)) {
        dALBWRental_close();
        return;
    }

    if (sActivePageCount > 1) {
        if (mDoCPd_c::getTrigRight(PAD_1)) {
            changePage(+1);
            return;
        }
        if (mDoCPd_c::getTrigLeft(PAD_1)) {
            changePage(-1);
            return;
        }
    }

    if (sVisibleCount > 0) {
        if (mDoCPd_c::getTrigDown(PAD_1) && sSelectedIdx < sVisibleCount - 1) {
            ++sSelectedIdx;
        } else if (mDoCPd_c::getTrigUp(PAD_1) && sSelectedIdx > 0) {
            --sSelectedIdx;
        } else if (mDoCPd_c::getTrigA(PAD_1)) {
            tryPurchase(sSelectedIdx);
        }
    }

    if (sStickNavCooldown > 0) {
        --sStickNavCooldown;
    }
    if (sVisibleCount > 0 && sStickNavCooldown == 0) {
        const f32 stickY = mDoCPd_c::getStickY(PAD_1);
        if (stickY < -0.5f && sSelectedIdx < sVisibleCount - 1) {
            ++sSelectedIdx;
            sStickNavCooldown = 12;
        } else if (stickY > 0.5f && sSelectedIdx > 0) {
            --sSelectedIdx;
            sStickNavCooldown = 12;
        }
    }
}

void dALBWRental_advanceToShop() {
    if (sState == STATE_GREETING) {
        sState = STATE_SHOP;
        sJustEnteredShop = true;
        sNativeDialogueJustDismissed = true;
        rebuildVisibleList();
    }
}

void dALBWRental_advanceToClosed() {
    if (sState == STATE_FAREWELL) {
        sJustClosed = true;
        sState = STATE_CLOSED;
        sHideVanillaTalkMsg = false;
        albw_oocoo_execute_pending_warp();
    }
}

bool dALBWRental_isShopState() {
    return sState == STATE_SHOP;
}

bool dALBWRental_isGreetingState() {
    return sState == STATE_GREETING;
}

bool dALBWRental_isFarewellState() {
    return sState == STATE_FAREWELL;
}

const char* dALBWRental_getGreetingText() {
    return sGreetingText;
}

const char* dALBWRental_getGreetingPage2() {
    return sGreetingPage2;
}

const char* dALBWRental_getGreetingPage3() {
    return sGreetingPage3;
}

const char* dALBWRental_getFarewellText() {
    return sFarewellText;
}

const dALBWVisibleEntry* dALBWRental_getVisibleList(int* outCount) {
    for (int i = 0; i < sVisibleCount; ++i) {
        dALBWVisibleEntry& pub = sPubList[i];
        std::memset(&pub, 0, sizeof(pub));
        const VisibleEntry& row = sVisibleList[i];
        if (row.kind == VISIBLE_MQ_HEART) {
            pub.name = albw_mq_heart_shop_name();
            pub.price = row.purchasable ? albw_mq_heart_shop_price() : 0;
            pub.purchasable = row.purchasable;
            pub.desc = albw_mq_heart_shop_desc();
            pub.itemNo = (u8)dItemNo_KAKERA_HEART_e;
            pub.showNameWhenSoldOut = true;
        } else if (row.kind == VISIBLE_MQ_METER) {
            pub.name = albw_mq_meter_shop_name();
            pub.price = row.purchasable ? albw_mq_meter_shop_price() : 0;
            pub.purchasable = row.purchasable;
            pub.desc = albw_mq_meter_shop_desc();
            pub.itemNo = (u8)dItemNo_MAGIC_LV1_e;
            pub.showNameWhenSoldOut = true;
        } else if (row.kind == VISIBLE_OOCOO) {
            pub.name = albw_oocoo_service_name();
            pub.price = albw_oocoo_service_price();
            pub.purchasable = true;
            pub.desc = albw_oocoo_service_desc();
            pub.itemNo = (u8)dItemNo_BEE_CHILD_e;
            pub.isOocooService = true;
            pub.showNameWhenSoldOut = true;
        } else if (row.kind == VISIBLE_SWORD_ATP) {
            // fork d_albw_rental.cpp:1723
            const int swordId = row.catalogIdx;
            pub.name = dAlbwSwordAtp_getShopName(swordId);
            pub.price = row.purchasable ? dAlbwSwordAtp_getShopPrice(swordId) : 0;
            pub.purchasable = row.purchasable;
            pub.desc = dAlbwSwordAtp_getShopDesc(swordId);
            pub.itemNo = dAlbwSwordAtp_getItemNo(swordId);
            pub.showNameWhenSoldOut = true;
        } else if (row.kind == VISIBLE_FA_TIER) {
            const int tier = dFocusedArts_getNextShopTierIndex();
            pub.name = dFocusedArts_getShopTierName(tier);
            pub.price = dFocusedArts_getNextShopTierPrice();
            pub.purchasable = row.purchasable;
            pub.desc = dFocusedArts_getShopTierDesc(tier);
            pub.itemNo = (u8)dItemNo_LV1_SOUP_e;
            pub.showNameWhenSoldOut = true;
        } else {
            const ALBWRentalEntry& e = kItems[row.catalogIdx];
            if (row.purchasable) {
                pub.name = e.name;
                pub.price = e.price;
                pub.desc = e.desc;
                pub.itemNo = e.itemNo;
            } else {
                pub.name = "?????";
                pub.price = e.price;
                pub.desc = nullptr;
                pub.itemNo = 0xff;
            }
            pub.purchasable = row.purchasable;
        }
    }
    if (outCount != nullptr) {
        *outCount = sVisibleCount;
    }
    return sPubList;
}

int dALBWRental_getSelectedIdx() {
    return sSelectedIdx;
}

const char* dALBWRental_getPageTitle() {
    const ALBWShopCategory cat = currentCategory();
    for (int p = 0; p < kPageCount; ++p) {
        if (kPages[p].category == cat) {
            return kPages[p].title;
        }
    }
    return "Items";
}

int dALBWRental_getPageNumber() {
    return (sActivePageCount > 0) ? sCurrentPageIdx + 1 : 1;
}

int dALBWRental_getPageCount() {
    return (sActivePageCount > 0) ? sActivePageCount : 1;
}

ModResult albw_rental_shop_init(ModError* error) {
    sState = STATE_CLOSED;
    s_menuResGen = 0;
    sHideVanillaTalkMsg = false;
    return albw_rental_postman_hooks_init(error);
}

ModResult albw_rental_shop_shutdown(ModError* error) {
    sState = STATE_CLOSED;
    sHideVanillaTalkMsg = false;
    return albw_rental_postman_hooks_shutdown(error);
}

void albw_rental_shop_tick() {
    albw_rental_postman_tick();
}
