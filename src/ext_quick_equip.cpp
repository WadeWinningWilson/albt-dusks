// ============================================
// NEW CODE - ALBW Port (quick-equip socket registry + ext-status registry)
//
// Ported from the fork's src/d/d_ext_mod_flags.cpp:343-915 - the runtime store
// behind the two-page item wheel and the Tools/Quest/Atlas start-menu page.
// Both registries are deliberately WW-agnostic in the fork ("callers claim by
// behavior kind. No WW names here"), which is what makes them usable by any mod.
//
// ONE DELIBERATE DIVERGENCE, and it makes the feature MORE capable rather than
// less: the fork discovers third-party claims by scanning enabled mod folders
// for ext_inv/claims.ini (dExtInv_rescanClaims). The mod SDK has no folder
// enumeration, so that scan cannot be ported - but it does not need to be.
// HostService already provides publish_service / get_service / watch_mod_lifecycle
// (sdk/include/mods/svc/host.h:55-97), so the registry is published as a service
// other mods retrieve and call directly, and their claims are dropped when they
// unload. That is the platform's own mechanism instead of a file-scan.
// ============================================

#include "global.h"
#include <os.h>

#include "d/d_com_inf_game.h"
#include "d/d_item_data.h"
#include "d/d_save.h"

#include "ext_quick_equip.h"
#include "ext_status.h"
#include "albw_common.h"
#include "mods/service.hpp"
#include "albw_game.h"
#include "albw_dusk_compat.h"
#include "potion.h"

#include <cstring>

#if TARGET_PC

namespace {

u8 s_qePageCount = dQe_kDefaultPages;
dQeSocketDesc s_qeSockets[dQe_kMaxPages][dQe_kSlotsPerPage] = {};
u16 s_qeNextBuiltinId = 0x1000;

void qeVacate(dQeSocketDesc& s) {
    std::memset(&s, 0, sizeof(s));
    s.kind = dQeKind_Empty;
    s.tpInvSlot = 0xFF;
    s.iconItemNo = 0xFF;
}

bool qeInBounds(u8 page, u8 slot) {
    return page < s_qePageCount && slot < dQe_kSlotsPerPage;
}

bool isTpToolItem(u8 item) {
    switch (item) {
    case dItemNo_BOOMERANG_e:
    case dItemNo_SPINNER_e:
    case dItemNo_IRONBALL_e:
    case dItemNo_BOW_e:
    case dItemNo_LIGHT_ARROW_e:
    case dItemNo_ARROW_LV1_e:
    case dItemNo_ARROW_LV2_e:
    case dItemNo_ARROW_LV3_e:
    case dItemNo_BOMB_ARROW_e:
    case dItemNo_HAWK_ARROW_e:
    case dItemNo_HOOKSHOT_e:
    case dItemNo_W_HOOKSHOT_e:
    case dItemNo_HVY_BOOTS_e:
    case dItemNo_COPY_ROD_e:
    case dItemNo_COPY_ROD_2_e:
    case dItemNo_KANTERA_e:
    case dItemNo_FISHING_ROD_1_e:
    case dItemNo_LURE_ROD_e:
    case dItemNo_BEE_ROD_e:
    case dItemNo_JEWEL_ROD_e:
    case dItemNo_WORM_ROD_e:
    case dItemNo_JEWEL_BEE_ROD_e:
    case dItemNo_JEWEL_WORM_ROD_e:
    case dItemNo_PACHINKO_e:
    case dItemNo_HAWK_EYE_e:
    case dItemNo_BOMB_BAG_LV1_e:
    case dItemNo_BOMB_BAG_LV2_e:
    case dItemNo_BOMB_IN_BAG_e:
    case dItemNo_NORMAL_BOMB_e:
    case dItemNo_WATER_BOMB_e:
    case dItemNo_POKE_BOMB_e:
        return true;
    default:
        return false;
    }
}

bool claimOnPage(u8 page, dQeSocketDesc desc) {
    const u8 slot = dQe_findFreeSlot(page);
    if (slot == 0xFF) {
        return false;
    }
    desc.page = page;
    desc.slot = slot;
    return dQe_claim(desc);
}

}  // namespace

bool dQe_setPageCount(u8 pages) {
    if (pages < dQe_kMinPages || pages > dQe_kMaxPages) {
        return false;
    }
    if (pages < s_qePageCount) {
        for (u8 p = pages; p < s_qePageCount; ++p) {
            for (u8 s = 0; s < dQe_kSlotsPerPage; ++s) {
                qeVacate(s_qeSockets[p][s]);
            }
        }
    } else {
        for (u8 p = s_qePageCount; p < pages; ++p) {
            for (u8 s = 0; s < dQe_kSlotsPerPage; ++s) {
                qeVacate(s_qeSockets[p][s]);
            }
        }
    }
    s_qePageCount = pages;
    return true;
}

u8 dQe_getPageCount() {
    return s_qePageCount;
}

bool dQe_claim(const dQeSocketDesc& desc) {
    if (!qeInBounds(desc.page, desc.slot)) {
        return false;
    }
    if (desc.id == 0 || desc.kind == dQeKind_Empty) {
        return false;
    }
    dQeSocketDesc& dst = s_qeSockets[desc.page][desc.slot];
    if (dst.id != 0) {
        return false;
    }
    dst = desc;
    return true;
}

bool dQe_clear(u8 page, u8 slot) {
    if (!qeInBounds(page, slot)) {
        return false;
    }
    qeVacate(s_qeSockets[page][slot]);
    return true;
}

bool dQe_clearById(u16 id) {
    if (id == 0) {
        return false;
    }
    bool found = false;
    for (u8 p = 0; p < s_qePageCount; ++p) {
        for (u8 s = 0; s < dQe_kSlotsPerPage; ++s) {
            if (s_qeSockets[p][s].id == id) {
                qeVacate(s_qeSockets[p][s]);
                found = true;
            }
        }
    }
    return found;
}

void dQe_clearByFlag(u16 flagMask) {
    for (u8 p = 0; p < s_qePageCount; ++p) {
        for (u8 s = 0; s < dQe_kSlotsPerPage; ++s) {
            if ((s_qeSockets[p][s].flags & flagMask) != 0) {
                qeVacate(s_qeSockets[p][s]);
            }
        }
    }
}

void dQe_clearAll() {
    for (u8 p = 0; p < dQe_kMaxPages; ++p) {
        for (u8 s = 0; s < dQe_kSlotsPerPage; ++s) {
            qeVacate(s_qeSockets[p][s]);
        }
    }
    s_qePageCount = dQe_kDefaultPages;
    s_qeNextBuiltinId = 0x1000;
}

const dQeSocketDesc* dQe_peek(u8 page, u8 slot) {
    if (!qeInBounds(page, slot)) {
        return NULL;
    }
    return &s_qeSockets[page][slot];
}

u8 dQe_findFreeSlot(u8 page) {
    if (page >= s_qePageCount) {
        return 0xFF;
    }
    for (u8 s = 0; s < dQe_kSlotsPerPage; ++s) {
        if (s_qeSockets[page][s].id == 0) {
            return s;
        }
    }
    return 0xFF;
}

void dQe_seedTpBuiltin() {
    if (s_qePageCount < dQe_kDefaultPages) {
        dQe_setPageCount(dQe_kDefaultPages);
    }
    dQe_clearByFlag(dQeFlag_BuiltinSeed);
    s_qeNextBuiltinId = 0x1000;

    // Page 0 — vanilla wheel tools only (InvSlot_Z). Page 1+ stay free for mod
    // sockets (SwordEquip / ShieldEquip / bags / …); never seed TP Collect gear.
    for (int i = 0; i < MAX_ITEM_SLOTS; ++i) {
        const u8 invSlot = dComIfGs_getLineUpItem(i);
        if (invSlot == dItemNo_NONE_e) {
            continue;
        }
        const u8 item = dComIfGs_getItem(invSlot, false);
        if (item == dItemNo_NONE_e || !isTpToolItem(item)) {
            continue;
        }
        dQeSocketDesc desc{};
        desc.id = s_qeNextBuiltinId++;
        desc.kind = dQeKind_InvSlot_Z;
        desc.tpInvSlot = invSlot;
        desc.iconItemNo = item;
        desc.flags = dQeFlag_BuiltinSeed;
        if (!claimOnPage(0, desc)) {
            break;
        }
    }

    // Soulbound red potion (SLOT_11) is not a "tool" but belongs on the tap wheel
    // when the Gameplay toggle has granted it.
    if (dusk::getSettings().game.albwSoulboundRedPotion.getValue() ||
        dAlbwPotion_isSoulboundRedInSlot(kAlbwPotionSoulboundSlot))
    {
        const u8 item = dComIfGs_getItem(kAlbwPotionSoulboundSlot, false);
        if (item != dItemNo_NONE_e && item != 0xFF) {
            bool already = false;
            for (u8 s = 0; s < dQe_kSlotsPerPage; ++s) {
                const dQeSocketDesc* peek = dQe_peek(0, s);
                if (peek != NULL && peek->id != 0 &&
                    (peek->kind == dQeKind_InvSlot_Z || peek->kind == dQeKind_ZSelect) &&
                    peek->tpInvSlot == kAlbwPotionSoulboundSlot)
                {
                    already = true;
                    break;
                }
            }
            if (!already) {
                dQeSocketDesc desc{};
                desc.id = s_qeNextBuiltinId++;
                desc.kind = dQeKind_InvSlot_Z;
                desc.tpInvSlot = kAlbwPotionSoulboundSlot;
                desc.iconItemNo = item;
                desc.flags = dQeFlag_BuiltinSeed;
                claimOnPage(0, desc);
            }
        }
    }
}

// ---------------------------------------------------------------------------
// Nested bags + deep-link
// ---------------------------------------------------------------------------

namespace {

struct BagStore {
    u16 bagId;
    dQeSocketDesc children[dQe_kBagCapacity];
};

BagStore s_bags[dQe_kMaxBags] = {};

BagStore* findBag(u16 bagId) {
    if (bagId == 0) {
        return NULL;
    }
    for (u8 i = 0; i < dQe_kMaxBags; ++i) {
        if (s_bags[i].bagId == bagId) {
            return &s_bags[i];
        }
    }
    return NULL;
}

BagStore* allocBag(u16 bagId) {
    BagStore* existing = findBag(bagId);
    if (existing != NULL) {
        return existing;
    }
    for (u8 i = 0; i < dQe_kMaxBags; ++i) {
        if (s_bags[i].bagId == 0) {
            s_bags[i].bagId = bagId;
            for (u8 c = 0; c < dQe_kBagCapacity; ++c) {
                qeVacate(s_bags[i].children[c]);
            }
            return &s_bags[i];
        }
    }
    return NULL;
}

}  // namespace

bool dQe_claimBagChild(u16 bagId, u8 childSlot, const dQeSocketDesc& child) {
    if (childSlot >= dQe_kBagCapacity || child.id == 0 || child.kind == dQeKind_Empty) {
        return false;
    }
    BagStore* bag = allocBag(bagId);
    if (bag == NULL) {
        return false;
    }
    if (bag->children[childSlot].id != 0) {
        return false;
    }
    bag->children[childSlot] = child;
    bag->children[childSlot].page = 0;
    bag->children[childSlot].slot = childSlot;
    return true;
}

bool dQe_clearBag(u16 bagId) {
    BagStore* bag = findBag(bagId);
    if (bag == NULL) {
        return false;
    }
    for (u8 c = 0; c < dQe_kBagCapacity; ++c) {
        qeVacate(bag->children[c]);
    }
    bag->bagId = 0;
    return true;
}

const dQeSocketDesc* dQe_peekBagChild(u16 bagId, u8 childSlot) {
    BagStore* bag = findBag(bagId);
    if (bag == NULL || childSlot >= dQe_kBagCapacity) {
        return NULL;
    }
    return &bag->children[childSlot];
}

u8 dQe_countBagOccupied(u16 bagId) {
    BagStore* bag = findBag(bagId);
    if (bag == NULL) {
        return 0;
    }
    u8 n = 0;
    for (u8 c = 0; c < dQe_kBagCapacity; ++c) {
        if (bag->children[c].id != 0) {
            ++n;
        }
    }
    return n;
}

bool dQe_deepLinkAssignZ(u8 tpInvSlot, u8 iconItemNo, u16 opaqueId) {
    if (tpInvSlot == 0xFF || opaqueId == 0) {
        return false;
    }
    dQe_clearById(opaqueId);
    dQeSocketDesc desc{};
    desc.id = opaqueId;
    desc.kind = dQeKind_ZSelect;
    desc.tpInvSlot = tpInvSlot;
    desc.iconItemNo = iconItemNo;
    desc.flags = dQeFlag_ModClaim;
    if (!claimOnPage(0, desc)) {
        return false;
    }
    dComIfGs_setSelectItemIndex(SELECT_ITEM_DOWN, tpInvSlot);
    return true;
}

// ---------------------------------------------------------------------------
// Ext Status registry
// ---------------------------------------------------------------------------

namespace {

dExtStatusRow s_extRows[dExtStatus_kMaxRows] = {};

void vacateExt(dExtStatusRow& r) {
    std::memset(&r, 0, sizeof(r));
    r.iconItemNo = 0xFF;
    r.tpInvSlot = 0xFF;
}

}  // namespace

bool dExtStatus_claim(const dExtStatusRow& row) {
    if (row.id == 0 || row.kind == dExtStatusKind_Empty || row.tab >= dExtStatus_kTabCount) {
        return false;
    }
    for (u8 i = 0; i < dExtStatus_kMaxRows; ++i) {
        if (s_extRows[i].id == row.id) {
            s_extRows[i] = row;
            return true;
        }
    }
    for (u8 i = 0; i < dExtStatus_kMaxRows; ++i) {
        if (s_extRows[i].id == 0) {
            s_extRows[i] = row;
            return true;
        }
    }
    return false;
}

bool dExtStatus_clearById(u16 id) {
    if (id == 0) {
        return false;
    }
    bool found = false;
    for (u8 i = 0; i < dExtStatus_kMaxRows; ++i) {
        if (s_extRows[i].id == id) {
            vacateExt(s_extRows[i]);
            found = true;
        }
    }
    return found;
}

void dExtStatus_clearByFlag(u16 flagMask) {
    for (u8 i = 0; i < dExtStatus_kMaxRows; ++i) {
        if ((s_extRows[i].flags & flagMask) != 0) {
            vacateExt(s_extRows[i]);
        }
    }
}

void dExtStatus_clearAll() {
    for (u8 i = 0; i < dExtStatus_kMaxRows; ++i) {
        vacateExt(s_extRows[i]);
    }
}

u8 dExtStatus_countTab(dExtStatusTab tab) {
    u8 n = 0;
    for (u8 i = 0; i < dExtStatus_kMaxRows; ++i) {
        if (s_extRows[i].id != 0 && s_extRows[i].tab == tab) {
            ++n;
        }
    }
    return n;
}

const dExtStatusRow* dExtStatus_peekTab(dExtStatusTab tab, u8 index) {
    u8 n = 0;
    for (u8 i = 0; i < dExtStatus_kMaxRows; ++i) {
        if (s_extRows[i].id == 0 || s_extRows[i].tab != tab) {
            continue;
        }
        if (n == index) {
            return &s_extRows[i];
        }
        ++n;
    }
    return NULL;
}

void dExtStatus_seedDebug() {
    dExtStatus_clearByFlag(dExtStatusFlag_DebugSeed);
    static const char* kTabNames[] = {"Tools", "Quest", "Atlas"};
    for (u8 t = 0; t < dExtStatus_kTabCount; ++t) {
        dExtStatusRow row{};
        row.id = static_cast<u16>(0xE100 + t);
        row.tab = static_cast<dExtStatusTab>(t);
        row.kind = (t == dExtStatusTab_Atlas) ? dExtStatusKind_Chart : dExtStatusKind_Passive;
        row.iconItemNo = 0xFF;
        row.tpInvSlot = 0xFF;
        row.flags = dExtStatusFlag_DebugSeed;
        std::snprintf(row.label, sizeof(row.label), "%s (empty)", kTabNames[t]);
        dExtStatus_claim(row);
    }
}

bool dExtStatus_tryDeepLinkZ(u16 rowId) {
    for (u8 i = 0; i < dExtStatus_kMaxRows; ++i) {
        if (s_extRows[i].id != rowId) {
            continue;
        }
        if (s_extRows[i].kind != dExtStatusKind_Usable || s_extRows[i].tpInvSlot == 0xFF) {
            return false;
        }
        return dQe_deepLinkAssignZ(s_extRows[i].tpInvSlot, s_extRows[i].iconItemNo, rowId);
    }
    return false;
}

// ---------------------------------------------------------------------------
// Third-party claims
//
// The fork discovers other mods' sockets by scanning each enabled mod folder for
// ext_inv/claims.ini (dExtInv_rescanClaims + a small INI parser). The mod SDK
// exposes no folder enumeration, so that scan is NOT ported - and does not need
// to be: HostService already offers publish_service / get_service /
// watch_mod_lifecycle (sdk/include/mods/svc/host.h:55-97).
//
// The registry is therefore published as a service. Another mod calls
// get_service("albt.quick_equip", ...) and claims sockets through these same
// dQe_/dExtStatus_ entry points at runtime - no file format, no parser, and
// claims can be dropped precisely when that mod unloads (which is what
// dQeFlag_ModClaim / dExtStatusFlag_ModClaim already exist for).
//
// This is strictly more capable than the INI scan: claims are live, typed, and
// lifecycle-aware rather than parsed once from disk.
// ---------------------------------------------------------------------------

// The service other mods import. Providers receive the CALLING mod's ModContext
// (modding.md:1086), so every claim is attributed to its owner and dropped when
// that mod detaches - the per-mod lifetime the fork approximates by re-reading
// all claims files on every rescan.
struct AlbtQuickEquipService {
    ServiceHeader header;
    bool (*set_page_count)(ModContext* ctx, u8 pages);
    u8   (*get_page_count)(ModContext* ctx);
    bool (*claim)(ModContext* ctx, const dQeSocketDesc* desc);
    bool (*clear_by_id)(ModContext* ctx, u16 id);
    u8   (*find_free_slot)(ModContext* ctx, u8 page);
    bool (*status_claim)(ModContext* ctx, const dExtStatusRow* row);
    bool (*status_clear_by_id)(ModContext* ctx, u16 id);
};

namespace {

// socket/row id -> owning mod. Small fixed table; the registry itself is capped
// at dQe_kMaxPages*24 + dExtStatus_kMaxRows entries anyway.
struct ClaimOwner { ModContext* ctx; u16 id; bool isStatus; };
ClaimOwner s_owners[dQe_kMaxPages * dQe_kSlotsPerPage + dExtStatus_kMaxRows] = {};

void rememberOwner(ModContext* ctx, u16 id, bool isStatus) {
    if (ctx == nullptr || id == 0) return;
    for (auto& o : s_owners) {
        if (o.ctx == nullptr) { o.ctx = ctx; o.id = id; o.isStatus = isStatus; return; }
    }
}

void forgetOwner(u16 id, bool isStatus) {
    for (auto& o : s_owners) {
        if (o.ctx != nullptr && o.id == id && o.isStatus == isStatus) { o.ctx = nullptr; return; }
    }
}

bool svc_set_pages(ModContext*, u8 pages) { return dQe_setPageCount(pages); }
u8   svc_get_pages(ModContext*) { return dQe_getPageCount(); }
u8   svc_free_slot(ModContext*, u8 page) { return dQe_findFreeSlot(page); }

bool svc_claim(ModContext* ctx, const dQeSocketDesc* d) {
    if (d == nullptr || !dQe_claim(*d)) return false;
    rememberOwner(ctx, d->id, false);
    return true;
}
bool svc_clear_by_id(ModContext*, u16 id) { forgetOwner(id, false); return dQe_clearById(id); }

bool svc_status_claim(ModContext* ctx, const dExtStatusRow* r) {
    if (r == nullptr || !dExtStatus_claim(*r)) return false;
    rememberOwner(ctx, r->id, true);
    return true;
}
bool svc_status_clear_by_id(ModContext*, u16 id) {
    forgetOwner(id, true);
    return dExtStatus_clearById(id);
}

}  // namespace

constexpr AlbtQuickEquipService g_albt_quick_equip_service{
    SERVICE_HEADER(AlbtQuickEquipService, 1, 0),
    svc_set_pages, svc_get_pages, svc_claim, svc_clear_by_id, svc_free_slot,
    svc_status_claim, svc_status_clear_by_id,
};
EXPORT_SERVICE_AS(g_albt_quick_equip_service, "dev.albt.albw.quick_equip");

// Drop only the departing mod's sockets - not every mod claim. The fork clears
// the whole ModClaim flag and re-reads all claims files; with a live service
// there is nothing to re-read, so the owner table makes the cleanup precise.
void albw_ext_registry_on_mod_detached(ModContext* subject) {
    if (subject == nullptr) return;
    for (auto& o : s_owners) {
        if (o.ctx != subject) continue;
        if (o.isStatus) dExtStatus_clearById(o.id);
        else            dQe_clearById(o.id);
        o.ctx = nullptr;
    }
}

#endif  // TARGET_PC

// ============================================
// NEW CODE ENDS HERE
// ============================================