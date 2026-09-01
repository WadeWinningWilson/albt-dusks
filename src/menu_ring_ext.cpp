// ============================================
// NEW CODE - ALBW Port (two-page quick-equip ring)
//
// The 19 methods the fork adds to dMenu_Ring_c, ported from d_menu_ring.cpp.
// Each carries its fork line. Two mechanical translations, applied uniformly:
//
//   1. The seven fork-added MEMBERS (mQuickEquipMode, mUseQuickEquipPages,
//      mQuickEquipForceClose, mQuickEquipPage, mQuickEquipSlotMap, mBagViewOpen,
//      mBagViewId) live in menu_ext_members.cpp, reached via `qe.` - a mod
//      cannot add fields to a stock class. Stock members are untouched (`r->`).
//   2. Methods become free functions over dMenu_Ring_c*, the same treatment used
//      for the alink and midna helpers. The seven the fork declares `static`
//      take no instance, exactly as in the fork.
//
// Bodies are otherwise unchanged, so they stay diffable against the fork.
// ============================================

#include "global.h"
#include <os.h>

#include "JSystem/J2DGraph/J2DPrint.h"
#include "JSystem/J2DGraph/J2DPicture.h"
#include "JSystem/JUtility/JUTFont.h"
#include "d/d_com_inf_game.h"
#include "d/d_item_data.h"
#include "d/d_meter2_info.h"
#include "d/d_lib.h"
#include "d/d_select_cursor.h"
#include "JSystem/JKernel/JKRExpHeap.h"
#include "m_Do/m_Do_ext.h"
#include "Z2AudioLib/Z2Instances.h"
#define private public
#include "d/d_menu_ring.h"
#include "d/d_menu_window.h"
#undef private

#include "menu_ring_ext.h"
#include "menu_ext_members.h"
#include "ext_quick_equip.h"
#include "albw_common.h"
#include "albw_game.h"
#include "albw_dusk_compat.h"
#include "wolf_combat.h"
#include "albw_fork_compat.h"
#include "extra_item_slot.h"

#include <cstdio>

#if TARGET_PC

// fork d_menu_ring.cpp:64-70
namespace {
bool s_pendingQuickEquipRing = false;
bool s_quickEquipLiveWorld = false;
dMenu_Ring_c* s_liveQuickRing = nullptr;
// 70% slowdown -> world runs at 30% sim rate.
constexpr f32 kQuickEquipSimTimeScale = 0.3f;
}  // namespace

// Forward declarations: several of these call each other.
void albw_ring_setPendingQuickEquip(bool quick);
bool albw_ring_peekPendingQuickEquip();
bool albw_ring_isQuickEquipLiveWorld();
bool albw_ring_isQuickEquipPagesExclusive();
f32 albw_ring_getQuickEquipSimScale();
void albw_ring_setQuickEquipLiveWorld(bool live);
void albw_ring_forceQuickConfirmClose();
u8 albw_ring_getQuickRegistrySlot(dMenu_Ring_c* r, int packedIdx);
u8 albw_ring_getQuickRingItem(dMenu_Ring_c* r, int slotIdx);
void albw_ring_clearQuickEquipItemTextures(dMenu_Ring_c* r);
void albw_ring_loadQuickEquipItemTextures(dMenu_Ring_c* r);
void albw_ring_remapQuickEquipFaceSlots(dMenu_Ring_c* r);
void albw_ring_applyQuickEquipPage(dMenu_Ring_c* r, u8 page, bool rebuildTextures);
void albw_ring_applyQuickEquipBagView(dMenu_Ring_c* r, bool rebuildTextures);
void albw_ring_confirmQuickEquipHover(dMenu_Ring_c* r);
bool albw_ring_tryQuickEquipBagOpen(dMenu_Ring_c* r);
bool albw_ring_tryQuickEquipPageFlip(dMenu_Ring_c* r);
void albw_ring_drawQuickEquipPageCue(dMenu_Ring_c* r);
u8 albw_ring_getHighlightedItem(dMenu_Ring_c* r);

// fork d_menu_ring.cpp:72
void albw_ring_setPendingQuickEquip(bool quick) {
    s_pendingQuickEquipRing = quick;
}

// fork d_menu_ring.cpp:76
bool albw_ring_peekPendingQuickEquip() {
    return s_pendingQuickEquipRing;
}

// fork d_menu_ring.cpp:80
bool albw_ring_isQuickEquipLiveWorld() {
    return s_quickEquipLiveWorld;
}

// fork d_menu_ring.cpp:84
bool albw_ring_isQuickEquipPagesExclusive() {
    return s_liveQuickRing != NULL && albw_ring_qe(s_liveQuickRing).usePages;
}

// fork d_menu_ring.cpp:88
f32 albw_ring_getQuickEquipSimScale() {
    return s_quickEquipLiveWorld ? kQuickEquipSimTimeScale : 1.0f;
}

// fork d_menu_ring.cpp:92
void albw_ring_setQuickEquipLiveWorld(bool live) {
    s_quickEquipLiveWorld = live;
}

// fork d_menu_ring.cpp:96
void albw_ring_forceQuickConfirmClose() {
    if (s_liveQuickRing == nullptr || !albw_ring_qe(s_liveQuickRing).mode) {
        return;
    }
    albw_ring_confirmQuickEquipHover(s_liveQuickRing);
    albw_ring_qe(s_liveQuickRing).forceClose = true;
}

// fork d_menu_ring.cpp:104
u8 albw_ring_getQuickRegistrySlot(dMenu_Ring_c* r, int packedIdx) {
    auto& qe = albw_ring_qe(r);
    if (packedIdx < 0 || packedIdx >= r->mItemsTotal) {
        return 0xFF;
    }
    return qe.slotMap[packedIdx];
}

// fork d_menu_ring.cpp:111
u8 albw_ring_getQuickRingItem(dMenu_Ring_c* r, int slotIdx) {
    auto& qe = albw_ring_qe(r);
    if (!qe.usePages || slotIdx < 0 || slotIdx >= r->mItemsTotal) {
        return dItemNo_NONE_e;
    }
    const u8 reg = qe.slotMap[slotIdx];
    if (reg == 0xFF) {
        return dItemNo_NONE_e;
    }
    const dQeSocketDesc* sock =
        qe.bagViewOpen ? dQe_peekBagChild(qe.bagViewId, reg) : dQe_peek(qe.page, reg);
    if (sock == NULL || sock->id == 0 || sock->kind == dQeKind_Empty) {
        return dItemNo_NONE_e;
    }
    if (sock->kind == dQeKind_InvSlot_Z || sock->kind == dQeKind_ZSelect) {
        if (sock->tpInvSlot != 0xFF) {
            return dComIfGs_getItem(sock->tpInvSlot, false);
        }
    }
    return sock->iconItemNo != 0xFF ? sock->iconItemNo : dItemNo_NONE_e;
}

// fork d_menu_ring.cpp:132
void albw_ring_clearQuickEquipItemTextures(dMenu_Ring_c* r) {
    for (int i = 0; i < r->mTotalItemTexToAlloc; i++) {
        for (int j = 0; j < 3; j++) {
            if (r->mpItemTex[i][j] != NULL) {
                JKR_DELETE(r->mpItemTex[i][j]);
                r->mpItemTex[i][j] = NULL;
            }
            r->mItemSlotParam1[i] = 0.0f;
            r->mItemSlotParam2[i] = 0.0f;
        }
    }
}

// fork d_menu_ring.cpp:145
void albw_ring_loadQuickEquipItemTextures(dMenu_Ring_c* r) {
    for (int i = 0; i < r->mItemsTotal; i++) {
        u8 item = albw_ring_getQuickRingItem(r, i);
        if (item == dItemNo_NONE_e || item == 0xFF || r->mpItemBuf[i][0] == NULL) {
            continue;
        }
        if (item == dItemNo_LIGHT_ARROW_e) {
            item = dItemNo_BOW_e;
        }
        const s32 texNum = dMeter2Info_readItemTexture(item, r->mpItemBuf[i][0], NULL, r->mpItemBuf[i][1],
                                                       NULL, r->mpItemBuf[i][2], NULL, NULL, NULL, -1);
        for (int k = 0; k < texNum; k++) {
            r->mpItemTex[i][k] = JKR_NEW J2DPicture(r->mpItemBuf[i][k]);
            r->mpItemTex[i][k]->setBasePosition(J2DBasePosition_4);
        }
        dMeter2Info_setItemColor(item, r->mpItemTex[i][0], r->mpItemTex[i][1], r->mpItemTex[i][2], NULL);
        const u8 texScale = dItem_data::getTexScale(item);
        const f32 fVar1 = (r->mpItemBuf[i][0]->width / 48.0f) * (texScale / 100.0f);
        r->mItemSlotParam1[i] = fVar1;
        r->mItemSlotParam2[i] = (r->mpItemBuf[i][0]->height / 48.0f * (texScale / 100.0f));
    }
}

// fork d_menu_ring.cpp:168
void albw_ring_remapQuickEquipFaceSlots(dMenu_Ring_c* r) {
    // Face-button assign rebuilds X+Y (and Z slide arms) from r->mItemSlots[mX/YButtonSlot].
    // Leaving those at 0xFF after a QE pack makes the "other" buttons write empty (0xFF).
    r->mXButtonSlot = 0xff;
    r->mYButtonSlot = 0xff;
    r->field_0x6ac = 0xff;
    const u8 curX = dComIfGs_getSelectItemIndex(SELECT_ITEM_X);
    const u8 curY = dComIfGs_getSelectItemIndex(SELECT_ITEM_Y);
    const u8 curZ = dComIfGs_getSelectItemIndex(SELECT_ITEM_DOWN);
    const int n = r->mItemsTotal > 0 ? r->mItemsTotal : 0;
    for (int i = 0; i < n; i++) {
        const u8 inv = r->mItemSlots[i];
        if (inv == 0xFF) {
            continue;
        }
        if (curX != 0xFF && inv == curX) {
            r->mXButtonSlot = static_cast<u8>(i);
        }
        if (curY != 0xFF && inv == curY) {
            r->mYButtonSlot = static_cast<u8>(i);
        }
        if (albw_is_extra_item_slot_enabled() && curZ != 0xFF && inv == curZ) {
            r->field_0x6ac = static_cast<u8>(i);
        }
    }
}

// fork d_menu_ring.cpp:195
void albw_ring_applyQuickEquipPage(dMenu_Ring_c* r, u8 page, bool rebuildTextures) {
    auto& qe = albw_ring_qe(r);
    const u8 pageCount = dQe_getPageCount();
    if (pageCount == 0) {
        page = 0;
    } else if (page >= pageCount) {
        page = static_cast<u8>(pageCount - 1);
    }
    qe.page = page;
    qe.bagViewOpen = false;
    qe.bagViewId = 0;

    u8 packed = 0;
    for (int i = 0; i < dQe_kSlotsPerPage; i++) {
        qe.slotMap[i] = 0xFF;
        r->mItemSlots[i] = 0xFF;
        const dQeSocketDesc* sock = dQe_peek(page, static_cast<u8>(i));
        if (sock == NULL || sock->id == 0 || sock->kind == dQeKind_Empty) {
            continue;
        }
        qe.slotMap[packed] = static_cast<u8>(i);
        if ((sock->kind == dQeKind_InvSlot_Z || sock->kind == dQeKind_ZSelect) &&
            sock->tpInvSlot != 0xFF)
        {
            r->mItemSlots[packed] = sock->tpInvSlot;
        }
        packed++;
    }
    r->mItemsTotal = packed > 0 ? packed : 1;
    if (packed == 0) {
        qe.slotMap[0] = 0xFF;
        r->mItemSlots[0] = 0xFF;
    }

    albw_ring_remapQuickEquipFaceSlots(r);
    r->mCurrentSlot = 0;
    if (r->field_0x6ac != 0xff) {
        r->mCurrentSlot = r->field_0x6ac;
    } else {
        const u8 curSword = dComIfGs_getSelectEquipSword();
        const u8 curShield = dComIfGs_getSelectEquipShield();
        for (int i = 0; i < packed; i++) {
            const u8 reg = qe.slotMap[i];
            const dQeSocketDesc* sock = dQe_peek(page, reg);
            if (sock == NULL || sock->id == 0) {
                continue;
            }
            if (sock->kind == dQeKind_SwordEquip && sock->iconItemNo == curSword) {
                r->mCurrentSlot = static_cast<u8>(i);
                break;
            }
            if (sock->kind == dQeKind_ShieldEquip && sock->iconItemNo == curShield) {
                r->mCurrentSlot = static_cast<u8>(i);
                break;
            }
        }
    }

    r->field_0x634 = 0x10000 / (r->mItemsTotal > 0 ? r->mItemsTotal : 1);
    r->field_0x66e = 0x8000;
    r->field_0x670 = 0;
    if (rebuildTextures) {
        albw_ring_clearQuickEquipItemTextures(r);
        albw_ring_loadQuickEquipItemTextures(r);
        r->setRotate();
        r->field_0x670 = r->field_0x63e[r->mCurrentSlot];
        r->field_0x66e = r->field_0x670;
        r->mpDrawCursor->setPos(r->mItemSlotPosX[r->mCurrentSlot] + r->mCenterPosX,
                             r->mItemSlotPosY[r->mCurrentSlot] + r->mCenterPosY);
        const u8 item = albw_ring_getQuickRingItem(r, r->mCurrentSlot);
        if (item != dItemNo_NONE_e) {
            r->mpDrawCursor->setParam(r->mItemSlotParam1[r->mCurrentSlot], r->mItemSlotParam2[r->mCurrentSlot], 0.1f,
                                   0.6f, 0.5f);
        } else {
            r->mpDrawCursor->setParam(1.0f, 1.0f, 0.1f, 0.6f, 0.5f);
        }
    }
}

// fork d_menu_ring.cpp:273
void albw_ring_applyQuickEquipBagView(dMenu_Ring_c* r, bool rebuildTextures) {
    auto& qe = albw_ring_qe(r);
    if (!qe.bagViewOpen || qe.bagViewId == 0) {
        return;
    }
    u8 packed = 0;
    for (u8 i = 0; i < dQe_kBagCapacity; i++) {
        qe.slotMap[i] = 0xFF;
        r->mItemSlots[i] = 0xFF;
        const dQeSocketDesc* sock = dQe_peekBagChild(qe.bagViewId, i);
        if (sock == NULL || sock->id == 0 || sock->kind == dQeKind_Empty) {
            continue;
        }
        qe.slotMap[packed] = i;
        if ((sock->kind == dQeKind_InvSlot_Z || sock->kind == dQeKind_ZSelect) &&
            sock->tpInvSlot != 0xFF)
        {
            r->mItemSlots[packed] = sock->tpInvSlot;
        }
        packed++;
    }
    r->mItemsTotal = packed > 0 ? packed : 1;
    if (packed == 0) {
        qe.slotMap[0] = 0xFF;
        r->mItemSlots[0] = 0xFF;
    }
    albw_ring_remapQuickEquipFaceSlots(r);
    r->mCurrentSlot = 0;
    if (r->field_0x6ac != 0xff) {
        r->mCurrentSlot = r->field_0x6ac;
    }
    r->field_0x634 = 0x10000 / (r->mItemsTotal > 0 ? r->mItemsTotal : 1);
    r->field_0x66e = 0x8000;
    r->field_0x670 = 0;
    if (rebuildTextures) {
        albw_ring_clearQuickEquipItemTextures(r);
        albw_ring_loadQuickEquipItemTextures(r);
        r->setRotate();
        r->field_0x670 = r->field_0x63e[r->mCurrentSlot];
        r->field_0x66e = r->field_0x670;
        r->mpDrawCursor->setPos(r->mItemSlotPosX[r->mCurrentSlot] + r->mCenterPosX,
                             r->mItemSlotPosY[r->mCurrentSlot] + r->mCenterPosY);
        r->mpDrawCursor->setParam(1.0f, 1.0f, 0.1f, 0.6f, 0.5f);
    }
}

// fork d_menu_ring.cpp:318
void albw_ring_confirmQuickEquipHover(dMenu_Ring_c* r) {
    auto& qe = albw_ring_qe(r);
    if (r->mPlayerIsWolf || r->mCurrentSlot >= r->mItemsTotal) {
        return;
    }
    if (qe.bagViewOpen) {
        // Nested bag: confirm child like a normal Z/equip socket.
    }
    const u8 reg = qe.slotMap[r->mCurrentSlot];
    if (reg == 0xFF) {
        return;
    }
    const dQeSocketDesc* sock =
        qe.bagViewOpen ? dQe_peekBagChild(qe.bagViewId, reg) : dQe_peek(qe.page, reg);
    if (sock == NULL || sock->id == 0 || sock->kind == dQeKind_Empty) {
        return;
    }
    switch (sock->kind) {
    case dQeKind_InvSlot_Z:
    case dQeKind_ZSelect:
        if (!albw_is_extra_item_slot_enabled()) {
            return;
        }
        if (sock->tpInvSlot != 0xFF) {
            dComIfGs_setSelectItemIndex(SELECT_ITEM_DOWN, sock->tpInvSlot);
            r->field_0x6ac = r->mCurrentSlot;
        }
        break;
    case dQeKind_SwordEquip:
        if (sock->iconItemNo != 0xFF && dComIfGs_getSelectEquipSword() != sock->iconItemNo) {
            dMeter2Info_setSword(sock->iconItemNo, false);
            Z2GetAudioMgr()->seStart(Z2SE_SY_ITEM_SET_X, NULL, 0, 0, 1.0f, 1.0f, -1.0f, -1.0f, 0);
            dMeter2Info_set2DVibration();
        }
        break;
    case dQeKind_ShieldEquip:
        if (sock->iconItemNo != 0xFF && dComIfGs_getSelectEquipShield() != sock->iconItemNo) {
            if (dMeter2_equipOwnedShield(sock->iconItemNo)) {
                Z2GetAudioMgr()->seStart(Z2SE_SY_ITEM_SET_X, NULL, 0, 0, 1.0f, 1.0f, -1.0f, -1.0f,
                                         0);
                dMeter2Info_set2DVibration();
            }
        }
        break;
    case dQeKind_Bag:
    case dQeKind_Custom:
    case dQeKind_Empty:
    default:
        break;
    }
}

// fork d_menu_ring.cpp:369
bool albw_ring_tryQuickEquipBagOpen(dMenu_Ring_c* r) {
    auto& qe = albw_ring_qe(r);
    if (!qe.usePages || qe.bagViewOpen || r->mCurrentSlot >= r->mItemsTotal) {
        return false;
    }
    const u8 reg = qe.slotMap[r->mCurrentSlot];
    if (reg == 0xFF) {
        return false;
    }
    const dQeSocketDesc* sock = dQe_peek(qe.page, reg);
    if (sock == NULL || sock->kind != dQeKind_Bag || sock->id == 0) {
        return false;
    }
    qe.bagViewOpen = true;
    qe.bagViewId = sock->id;
    albw_ring_applyQuickEquipBagView(r, true);
    r->setStatus(dMenu_Ring_c::STATUS_WAIT);
    r->stick_wait_init();
    Z2GetAudioMgr()->seStart(Z2SE_ITEM_RING_ROLL, NULL, 0, 0, 1.0f, 1.0f, -1.0f, -1.0f, 0);
    return true;
}

// fork d_menu_ring.cpp:390
bool albw_ring_tryQuickEquipPageFlip(dMenu_Ring_c* r) {
    auto& qe = albw_ring_qe(r);
    if (!qe.usePages || qe.bagViewOpen) {
        return false;
    }
    const u8 pageCount = dQe_getPageCount();
    if (pageCount <= 1) {
        return false;
    }
    s8 delta = 0;
    if (dMw_RIGHT_TRIGGER()) {
        delta = 1;
    } else if (dMw_LEFT_TRIGGER()) {
        delta = -1;
    } else {
        return false;
    }
    const u8 next =
        static_cast<u8>((qe.page + pageCount + delta) % pageCount);
    if (next == qe.page) {
        return false;
    }
    albw_ring_applyQuickEquipPage(r, next, true);
    r->setStatus(dMenu_Ring_c::STATUS_WAIT);
    r->stick_wait_init();
    Z2GetAudioMgr()->seStart(Z2SE_ITEM_RING_ROLL, NULL, 0, 0, 1.0f, 1.0f, -1.0f, -1.0f, 0);
    dMeter2Info_set2DVibration();
    return true;
}

// fork d_menu_ring.cpp:419
void albw_ring_drawQuickEquipPageCue(dMenu_Ring_c* r) {
    auto& qe = albw_ring_qe(r);
    if (!qe.usePages) {
        return;
    }
    JUTFont* font = mDoExt_getMesgFont();
    if (font == NULL) {
        return;
    }
    char buf[24];
    if (qe.bagViewOpen) {
        snprintf(buf, sizeof(buf), "BAG %u", static_cast<unsigned>(dQe_countBagOccupied(qe.bagViewId)));
    } else {
        const u8 pageCount = dQe_getPageCount();
        snprintf(buf, sizeof(buf), "%u / %u", static_cast<unsigned>(qe.page + 1),
                 static_cast<unsigned>(pageCount > 0 ? pageCount : 1));
    }
    const JUtility::TColor ink(255, 255, 255, 255);
    J2DPrint print(font, 0.0f, 18.0f, ink, ink, JUtility::TColor(0, 0, 0, 0), ink);
    print.setFontSize(16.0f, 16.0f);
    font->pushDrawState();
    print.initiate();
    const f32 x = FB_WIDTH_BASE * 0.5f - 24.0f;
    const f32 y = 36.0f;
    print.print(x, y, 255, "%s", buf);
    font->popDrawState();
}

// fork d_menu_ring.cpp:2553
u8 albw_ring_getHighlightedItem(dMenu_Ring_c* r) {
    auto& qe = albw_ring_qe(r);
    if (r->mItemsTotal == 0 || r->mCurrentSlot >= r->mItemsTotal) {
        return dItemNo_NONE_e;
    }

#if TARGET_PC
    if (qe.usePages) {
        return albw_ring_getQuickRingItem(r, r->mCurrentSlot);
    }
#endif
    const u8 invSlot = r->mItemSlots[r->mCurrentSlot];
    return dComIfGs_getItem(invSlot, false);
}

/** @details
 * Returns current ammo depending on the current item slot the cursor is on
 * This can be:
 *  - Ammo of any bomb type
 *  - Number of bee larvae in a bottle
 *  - Bow ammo
 *  - Slingshot ammo
*/

// The 21 MODIFIED dMenu_Ring_c methods are not hooked yet - that is the second
// half of this port. Until then these are linked but unreached, so the wheel
// behaves exactly as stock.
ModResult albw_menu_ring_ext_init(ModError*) {
    return MOD_OK;
}

#endif  // TARGET_PC

// ============================================
// NEW CODE ENDS HERE
// ============================================
