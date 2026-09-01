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
#include "d/d_meter_HIO.h"
#include "d/d_kantera_icon_meter.h"
#include "d/d_menu_item_explain.h"
#include "d/d_meter2.h"
#include "d/d_meter2_draw.h"
#include "d/d_msg_string.h"
#include "JSystem/J2DGraph/J2DOrthoGraph.h"
#include "JSystem/J2DGraph/J2DTextBox.h"
#include "m_Do/m_Do_graphic.h"
#include "SSystem/SComponent/c_math.h"
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
#include "quick_equip.h"
#include "albw_l1_input.h"
#include "mods/hook.hpp"
#include "d/d_save.h"
#include "quick_equip.h"
#include "mods/hook.hpp"
#include "d/d_save.h"

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
void albw_ring_ctor_init(dMenu_Ring_c* r);
void albw_ringmod__move(dMenu_Ring_c* r);
void albw_ringmod__draw(dMenu_Ring_c* r);
bool albw_ringmod_isMoveEnd(dMenu_Ring_c* r);
void albw_ringmod_setItem(dMenu_Ring_c* r);
void albw_ringmod_setJumpItem(dMenu_Ring_c* r, bool i_useVibrationM);
void albw_ringmod_setScale(dMenu_Ring_c* r);
void albw_ringmod_setNameString(dMenu_Ring_c* r, u32 i_stringID);
void albw_ringmod_setActiveCursor(dMenu_Ring_c* r);
void albw_ringmod_setMixItem(dMenu_Ring_c* r);
void albw_ringmod_drawItem(dMenu_Ring_c* r);
void albw_ringmod_drawItem2(dMenu_Ring_c* r);
void albw_ringmod_stick_wait_init(dMenu_Ring_c* r);
void albw_ringmod_stick_wait_proc(dMenu_Ring_c* r);
bool albw_ringmod_pointerMove(dMenu_Ring_c* r);
void albw_ringmod_stick_move_init(dMenu_Ring_c* r);
void albw_ringmod_stick_explain_force_proc(dMenu_Ring_c* r);
void albw_ringmod_setSelectItem(dMenu_Ring_c* r, int i_idx, u8 i_itemNo);
void albw_ringmod_drawSelectItem(dMenu_Ring_c* r);
u8 albw_ringmod_getCursorPos(dMenu_Ring_c* r, u8 i_slotNo);
u8 albw_ringmod_getItemNum(dMenu_Ring_c* r, u8 i_slotNo);
u8 albw_ringmod_getItemMaxNum(dMenu_Ring_c* r, u8 i_slotNo);
bool albw_ringmod_checkCombineBomb(dMenu_Ring_c* r, int param_0);
void albw_ringmod_setCombineBomb(dMenu_Ring_c* r, int param_0);
u8 albw_ringmod_getItem(dMenu_Ring_c* r, int i_slot_no, u8 i_mix_slot);
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

// ---- constructor initialisation (fork d_menu_ring.cpp:447) -----------------
// A constructor cannot be hooked by member pointer, so the fork's ctor additions
// run in a _create pre-hook instead: the object exists, quick-equip state is
// initialised, then vanilla _create builds the panes.
void albw_ring_ctor_init(dMenu_Ring_c* r) {
    auto& qe = albw_ring_qe(r);
    qe.mode = s_pendingQuickEquipRing;
    s_pendingQuickEquipRing = false;
    qe.forceClose = false;
    qe.bagViewOpen = false;
    qe.bagViewId = 0;
    qe.page = 0;
    qe.usePages = albw_quick_equip_enabled() && !r->mPlayerIsWolf;
    for (int qi = 0; qi < MAX_ITEM_SLOTS; qi++) {
        qe.slotMap[qi] = 0xFF;
    }
    if (qe.usePages) {
        s_liveQuickRing = r;
        dQe_seedTpBuiltin();
        r->mTotalItemTexToAlloc = dQe_kSlotsPerPage;
        albw_ring_applyQuickEquipPage(r, 0, false);
    }
}

// ---- three functions that need wholesale replacement -----------------------
// Their quick-equip change sits INSIDE a per-item loop (which icon/ammo to draw),
// so it cannot be added by a pre- or post-hook. Verified first that none of the
// three touch the host subsystems the SDK lacks (menu_pointer, frame_interp,
// game_clock, ActionBinds) - they are self-contained.
// fork d_menu_ring.cpp:1744
void albw_ringmod_setScale(dMenu_Ring_c* r) {
    auto& qe = albw_ring_qe(r);
    u32 itemId;
    for (int i = 0; i < r->mItemsTotal; i++) {
        if (r->field_0x6cf != 0xff) {
            itemId = 0;
            switch (r->field_0x6cf) {
            case 0:
                itemId = 0x4DE;
                break;
            case 1:
                itemId = 0x4E0;
                break;
            }
            r->setNameString(itemId);
            r->setItemScale(i, g_ringHIO.mUnselectItemScale);
            for (int j = 0; j < 2; j++) {
                if (j == r->field_0x6cf) {
                    r->setButtonScale(j, g_ringHIO.mSelectButtonScale);
                } else {
                    r->setButtonScale(j, g_ringHIO.mUnselectButtonScale);
                }
            }
        } else {
            if (i == r->mCurrentSlot && (r->mStatus == dMenu_Ring_c::STATUS_WAIT || r->mStatus == dMenu_Ring_c::STATUS_EXPLAIN || r->mStatus == dMenu_Ring_c::STATUS_EXPLAIN_FORCE)) {
                if (qe.mode) {
                    const u8 qItem = albw_ring_getQuickRingItem(r, i);
                    itemId = (qItem != dItemNo_NONE_e && qItem != 0xFF) ? (qItem + 0x165) : 0;
                } else
                {
                    itemId = dComIfGs_getItem(r->mItemSlots[i], false) + 0x165;
                    if (dMeter2Info_getRentalBombBag() != 0xff &&
                        r->mItemSlots[i] == dMeter2Info_getRentalBombBag() + 0xf)
                    {
                        itemId = 0x16D;
                    }
                }
                r->setNameString(itemId);
                r->setItemScale(i, g_ringHIO.mSelectItemScale);
            } else {
                r->setItemScale(i, g_ringHIO.mUnselectItemScale);
            }
            for (int j = 0; j < 2; j++) {
                r->setButtonScale(j, g_ringHIO.mUnselectButtonScale);
            }
        }
    }
}

// fork d_menu_ring.cpp:1989
void albw_ringmod_drawItem(dMenu_Ring_c* r) {
    auto& qe = albw_ring_qe(r);
    r->field_0x684++;
    if (r->field_0x684 >= g_ringHIO.mItemAlphaFlashDuration) {
        r->field_0x684 = 0;
    }
    s32 halfFlashDuration = g_ringHIO.mItemAlphaFlashDuration / 2;
    f32 fVar16;
    if (r->field_0x684 < halfFlashDuration) {
        fVar16 = r->field_0x684 / (f32)halfFlashDuration;
    } else {
        fVar16 = (g_ringHIO.mItemAlphaFlashDuration - r->field_0x684) / (f32)halfFlashDuration;
    }
    f32 ringAlpha =
        (g_ringHIO.mItemAlphaMin + fVar16 * (g_ringHIO.mItemAlphaMax - g_ringHIO.mItemAlphaMin));
    for (int i = 0; i < r->mItemsTotal; i++) {
        if (i != r->mCurrentSlot || (r->mStatus != dMenu_Ring_c::STATUS_WAIT && r->mStatus != dMenu_Ring_c::STATUS_EXPLAIN && r->mStatus != dMenu_Ring_c::STATUS_EXPLAIN_FORCE)) {
            J2DDrawFrame(r->mItemSlotPosX[i] - 24.0f + r->mCenterPosX, r->mItemSlotPosY[i] - 24.0f + r->mCenterPosY,
                         48.0f, 48.0f, g_ringHIO.mItemFrame[g_ringHIO.UNSELECT_FRAME], 6);
            f32 fVar17 = 1.0f;
            if (i != r->mCurrentSlot) {
                fVar17 = ringAlpha / 255.0f;
            }
            for (int j = 0; j < 3; j++) {
                if (r->mpItemTex[i][j] != NULL) {
                    if (r->mPlayerIsWolf) {
                        r->mpItemTex[i][j]->setAlpha(g_ringHIO.mItemIconAlpha_Wolf * r->mAlphaRate);
                    } else {
                        r->mpItemTex[i][j]->setAlpha(g_ringHIO.mItemIconAlpha * r->mAlphaRate * fVar17);
                    }
                    f32 f0 = r->mItemSlotParam1[i] * 48.0f;
                    f32 f1 = r->mItemSlotParam2[i] * 48.0f;
                    f32 x = (48.0f - f0) * 0.5f + (r->mItemSlotPosX[i] - 24.0f + r->mCenterPosX);
                    f32 y = (48.0f - f1) * 0.5f + (r->mItemSlotPosY[i] - 24.0f + r->mCenterPosY);
                    r->mpItemTex[i][j]->draw(x, y, f0, f1, 0, 0, 0);
                    u8 item = qe.usePages ? albw_ring_getQuickRingItem(r, i) :
                                                   dComIfGs_getItem(r->mItemSlots[i], false);
                    bool ammoOk = !qe.usePages;
                    if (qe.usePages) {
                        const u8 reg = qe.slotMap[i];
                        const dQeSocketDesc* sock =
                            reg == 0xFF ? NULL :
                            (qe.bagViewOpen ? dQe_peekBagChild(qe.bagViewId, reg) :
                                           dQe_peek(qe.page, reg));
                        ammoOk = sock != NULL && sock->tpInvSlot != 0xFF &&
                                 (sock->kind == dQeKind_InvSlot_Z || sock->kind == dQeKind_ZSelect);
                    }
                    if (ammoOk && ((j == 0 && item != dItemNo_BEE_CHILD_e) ||
                                   (j == 2 && item == dItemNo_BEE_CHILD_e)))
                    {
                        u8 itemNum = r->getItemNum(r->mItemSlots[i]);
                        u8 itemMaxNum = r->getItemMaxNum(r->mItemSlots[i]);
                        if (itemMaxNum != 0) {
                            // If it's an ammo-based item, display ammo digits
                            r->drawNumber(itemNum, itemMaxNum, x + 24.0f, y + 48.0f);
                        }
                    }
                    if (j == 0 && item == dItemNo_KANTERA_e /* Lantern */) {
                        r->setKanteraPos(x + 24.0f + 15.0f, y + 48.0f + 10.0f);
                        r->mpKanteraMeter->setScale(0.64f, 0.64f);
                        r->mpKanteraMeter->setNowGauge(dComIfGs_getMaxOil(), dComIfGs_getOil());
                        u8 alpha = r->mpItemTex[i][j]->getAlpha();
                        r->mpKanteraMeter->setAlphaRate(alpha / 255.0f);
                        r->mpKanteraMeter->drawSelf();
                    }
                }
            }
        }
    }
}

// fork d_menu_ring.cpp:2064
void albw_ringmod_drawItem2(dMenu_Ring_c* r) {
    auto& qe = albw_ring_qe(r);
    s32 idx = r->mCurrentSlot;
    if (r->mStatus == dMenu_Ring_c::STATUS_WAIT || r->mStatus == dMenu_Ring_c::STATUS_EXPLAIN || r->mStatus == dMenu_Ring_c::STATUS_EXPLAIN_FORCE) {
        J2DDrawFrame(r->mItemSlotPosX[idx] - 24.0f + r->mCenterPosX, r->mItemSlotPosY[idx] - 24.0f + r->mCenterPosY,
                     48.0f, 48.0f, g_ringHIO.mItemFrame[g_ringHIO.SELECT_FRAME], 6);

        for (int i = 0; i < 3; i++) {
            if (r->mpItemTex[idx][i] != NULL) {
                if (r->mPlayerIsWolf != 0) {
                    r->mpItemTex[idx][i]->setAlpha(g_ringHIO.mItemIconAlpha_Wolf * r->mAlphaRate);
                } else {
                    r->mpItemTex[idx][i]->setAlpha(r->mAlphaRate * 255.0f);
                }

                f32 f0 = r->mItemSlotParam1[idx] * 48.0f;
                f32 f1 = r->mItemSlotParam2[idx] * 48.0f;
                f32 x = (48.0f - f0) * 0.5f + (r->mItemSlotPosX[idx] - 24.0f + r->mCenterPosX);
                f32 y = (48.0f - f1) * 0.5f + (r->mItemSlotPosY[idx] - 24.0f + r->mCenterPosY);
                r->mpItemTex[idx][i]->draw(x, y, f0, f1, 0, 0, 0);
                u8 item = qe.usePages ? albw_ring_getQuickRingItem(r, idx) :
                                               dComIfGs_getItem(r->mItemSlots[idx], false);
                bool ammoOk = !qe.usePages;
                if (qe.usePages) {
                    const u8 reg = qe.slotMap[idx];
                    const dQeSocketDesc* sock =
                        reg == 0xFF ? NULL :
                        (qe.bagViewOpen ? dQe_peekBagChild(qe.bagViewId, reg) :
                                       dQe_peek(qe.page, reg));
                    ammoOk = sock != NULL && sock->tpInvSlot != 0xFF &&
                             (sock->kind == dQeKind_InvSlot_Z || sock->kind == dQeKind_ZSelect);
                }
                if (ammoOk && ((i == 0 && item != dItemNo_BEE_CHILD_e) ||
                               (i == 2 && item == dItemNo_BEE_CHILD_e)))
                {
                    u8 itemNum = r->getItemNum(r->mItemSlots[idx]);
                    u8 itemMaxNum = r->getItemMaxNum(r->mItemSlots[idx]);
                    if (itemMaxNum != 0) {
                        // If it's an ammo-based item, display ammo digits
                        r->drawNumber(itemNum, itemMaxNum, x + 24.0f, y + 48.0f);
                    }
                }
                if (i == 0 && item == dItemNo_KANTERA_e) {
                    r->setKanteraPos(x + 24.0f + 15.0f, y + 48.0f + 10.0f);
                    r->mpKanteraMeter->setScale(0.64f, 0.64f);
                    r->mpKanteraMeter->setNowGauge(dComIfGs_getMaxOil(), dComIfGs_getOil());
                    u8 alpha = r->mpItemTex[idx][i]->getAlpha();
                    r->mpKanteraMeter->setAlphaRate(alpha / 255.0f);
                    r->mpKanteraMeter->drawSelf();
                }
            }
        }
    }
}

namespace {

// ============================================
// Surgical hooks. Each applies ONLY the quick-equip hunk for that function - 51
// lines across 13 functions in total. The rest of each fork body belongs to
// OTHER fork features (mix-items, combine-bomb, menu_pointer, frame_interp) and
// is deliberately not carried: replacing whole bodies would have dragged in six
// host subsystems the SDK does not expose.
//
// Every hook falls through to vanilla when the feature is off.
// ============================================

DEFINE_HOOK(&dMenu_Ring_c::_create, Ring_Create);
DEFINE_HOOK(&dMenu_Ring_c::_delete, Ring_Delete);
DEFINE_HOOK(&dMenu_Ring_c::_move, Ring_Move);
DEFINE_HOOK(&dMenu_Ring_c::_draw, Ring_Draw);
DEFINE_HOOK(&dMenu_Ring_c::isMoveEnd, Ring_IsMoveEnd);
DEFINE_HOOK(&dMenu_Ring_c::setActiveCursor, Ring_SetActiveCursor);
DEFINE_HOOK(&dMenu_Ring_c::stick_wait_init, Ring_StickWaitInit);
DEFINE_HOOK(&dMenu_Ring_c::stick_wait_proc, Ring_StickWaitProc);
DEFINE_HOOK(&dMenu_Ring_c::stick_move_init, Ring_StickMoveInit);
DEFINE_HOOK(&dMenu_Ring_c::setScale, Ring_SetScale);
DEFINE_HOOK(&dMenu_Ring_c::drawItem, Ring_DrawItem);
DEFINE_HOOK(&dMenu_Ring_c::drawItem2, Ring_DrawItem2);

bool qe_on(dMenu_Ring_c* r) { return r != nullptr && albw_ring_qe(r).usePages; }

// fork ctor hunks 1-3 (d_menu_ring.cpp:447)
HookAction on_ring_create_pre(ModContext*, void* args, void*, void*) {
    auto* r = mods::arg<dMenu_Ring_c*>(args, 0);
    if (r != nullptr) albw_ring_ctor_init(r);
    return HOOK_CONTINUE;   // vanilla _create still builds the panes
}

// fork ~dMenu_Ring_c:571
void on_ring_delete_post(ModContext*, void* args, void*, void*) {
    auto* r = mods::arg<dMenu_Ring_c*>(args, 0);
    if (r == nullptr) return;
    if (s_liveQuickRing == r) s_liveQuickRing = nullptr;
    albw_ring_qe_release(r);
}

// fork _move:686 - page flip on tap or hold; short-circuits the rest of _move.
HookAction on_ring_move_pre(ModContext*, void* args, void*, void*) {
    auto* r = mods::arg<dMenu_Ring_c*>(args, 0);
    if (!qe_on(r)) return HOOK_CONTINUE;
    if (albw_ring_tryQuickEquipPageFlip(r)) {
        r->mRingRadiusH = g_ringHIO.mRingRadiusH;
        r->mRingRadiusV = g_ringHIO.mRingRadiusV;
        r->mOldStatus = r->mStatus;
        r->setScale();
        r->setActiveCursor();
        return HOOK_SKIP_ORIGINAL;
    }
    return HOOK_CONTINUE;
}

// fork _draw:827
void on_ring_draw_post(ModContext*, void* args, void*, void*) {
    auto* r = mods::arg<dMenu_Ring_c*>(args, 0);
    if (qe_on(r)) albw_ring_drawQuickEquipPageCue(r);
}

// fork isMoveEnd:878 - quick-equip close/cancel. The fork reads the
// OPEN_ITEM_WHEEL action binding to test "still held". The SDK has no binding
// system, and this mod already translates that button to L1 in quick_equip.cpp,
// so the same predicate is reused here.
HookAction on_ring_is_move_end_pre(ModContext*, void* args, void* retval, void*) {
    auto* r = mods::arg<dMenu_Ring_c*>(args, 0);
    if (r == nullptr || retval == nullptr) return HOOK_CONTINUE;
    auto& qe = albw_ring_qe(r);
    if (!qe.mode) return HOOK_CONTINUE;
    if (!(r->mStatus == dMenu_Ring_c::STATUS_WAIT &&
          r->mOldStatus != dMenu_Ring_c::STATUS_EXPLAIN_FORCE &&
          r->mOldStatus != dMenu_Ring_c::STATUS_EXPLAIN)) {
        return HOOK_CONTINUE;
    }
    if (qe.forceClose || !albw_l1_held(PAD_1)) {
        if (!qe.forceClose) albw_ring_confirmQuickEquipHover(r);
        r->mRingOrigin = 0xff;
        Z2GetAudioMgr()->seStart(Z2SE_ITEM_RING_OUT, NULL, 0, 0, 1.0f, 1.0f, -1.0f, -1.0f, 0);
        dMeter2Info_set2DVibrationM();
        *static_cast<bool*>(retval) = true;
        return HOOK_SKIP_ORIGINAL;
    }
    if (qe.bagViewOpen && dMw_B_TRIGGER()) {
        albw_ring_applyQuickEquipPage(r, qe.page, true);
        r->setStatus(dMenu_Ring_c::STATUS_WAIT);
        r->stick_wait_init();
        Z2GetAudioMgr()->seStart(Z2SE_ITEM_RING_ROLL, NULL, 0, 0, 1.0f, 1.0f, -1.0f, -1.0f, 0);
        *static_cast<bool*>(retval) = false;
        return HOOK_SKIP_ORIGINAL;
    }
    if (dMw_B_TRIGGER() || dMw_UP_TRIGGER() || dMw_DOWN_TRIGGER() ||
        dMeter2Info_getWarpStatus() == 2 || dMeter2Info_getWarpStatus() == 1 ||
        dMeter2Info_isTouchKeyCheck(0xe)) {
        r->mRingOrigin = 0xff;
        Z2GetAudioMgr()->seStart(Z2SE_ITEM_RING_OUT, NULL, 0, 0, 1.0f, 1.0f, -1.0f, -1.0f, 0);
        dMeter2Info_set2DVibrationM();
        *static_cast<bool*>(retval) = true;
        return HOOK_SKIP_ORIGINAL;
    }
    *static_cast<bool*>(retval) = false;
    return HOOK_SKIP_ORIGINAL;
}

// fork setActiveCursor:1819 - release-to-Z only; ignore face-button assigns
// while the wheel is held open.
HookAction on_ring_set_active_cursor_pre(ModContext*, void* args, void*, void*) {
    auto* r = mods::arg<dMenu_Ring_c*>(args, 0);
    if (r == nullptr) return HOOK_CONTINUE;
    if (albw_ring_qe(r).mode && r->mStatus == dMenu_Ring_c::STATUS_WAIT &&
        r->mOldStatus != dMenu_Ring_c::STATUS_EXPLAIN_FORCE &&
        r->mOldStatus != dMenu_Ring_c::STATUS_EXPLAIN) {
        return HOOK_SKIP_ORIGINAL;
    }
    return HOOK_CONTINUE;
}

// fork stick_wait_init:2123
void on_ring_stick_wait_init_post(ModContext*, void* args, void*, void*) {
    auto* r = mods::arg<dMenu_Ring_c*>(args, 0);
    if (r == nullptr) return;
    if (albw_ring_qe(r).mode && r->mWaitFrames > 1) {
        r->mWaitFrames = static_cast<s16>(r->mWaitFrames / 2);
    }
}

// fork stick_wait_proc:2147
HookAction on_ring_stick_wait_proc_pre(ModContext*, void* args, void*, void*) {
    auto* r = mods::arg<dMenu_Ring_c*>(args, 0);
    if (!qe_on(r)) return HOOK_CONTINUE;
    auto& qe = albw_ring_qe(r);
    if (qe.bagViewOpen && dMw_B_TRIGGER()) {
        albw_ring_applyQuickEquipPage(r, qe.page, true);
        r->setStatus(dMenu_Ring_c::STATUS_WAIT);
        r->stick_wait_init();
        Z2GetAudioMgr()->seStart(Z2SE_ITEM_RING_ROLL, NULL, 0, 0, 1.0f, 1.0f, -1.0f, -1.0f, 0);
        return HOOK_SKIP_ORIGINAL;
    }
    if (dMw_A_TRIGGER() && albw_ring_tryQuickEquipBagOpen(r)) {
        return HOOK_SKIP_ORIGINAL;
    }
    if (qe.mode) {
        if (r->mWaitFrames > 0) {
            r->mWaitFrames--;
        } else if (r->getStickInfo(r->mpStick) != 0) {
            r->setStatus(dMenu_Ring_c::STATUS_MOVE);
            r->field_0x6b2 = 0;
        }
        return HOOK_SKIP_ORIGINAL;
    }
    return HOOK_CONTINUE;
}

// fork stick_move_init:2279 - quick-equip spins the cursor faster.
void on_ring_stick_move_init_post(ModContext*, void* args, void*, void*) {
    auto* r = mods::arg<dMenu_Ring_c*>(args, 0);
    if (r == nullptr || !albw_ring_qe(r).mode) return;
    r->mCursorSpeed = static_cast<s16>(r->mCursorSpeed + g_ringHIO.mCursorAccel);
    if (r->mCursorSpeed > g_ringHIO.mCursorMax) r->mCursorSpeed = g_ringHIO.mCursorMax;
}

// ---- the three wholesale replacements --------------------------------------
// Their quick-equip change sits inside a per-item loop, so it cannot be added by
// a pre- or post-hook. Verified none of the three touch the host subsystems the
// SDK lacks before taking this route.
HookAction on_ring_set_scale_pre(ModContext*, void* args, void*, void*) {
    auto* r = mods::arg<dMenu_Ring_c*>(args, 0);
    if (!qe_on(r)) return HOOK_CONTINUE;
    albw_ringmod_setScale(r);
    return HOOK_SKIP_ORIGINAL;
}

HookAction on_ring_draw_item_pre(ModContext*, void* args, void*, void*) {
    auto* r = mods::arg<dMenu_Ring_c*>(args, 0);
    if (!qe_on(r)) return HOOK_CONTINUE;
    albw_ringmod_drawItem(r);
    return HOOK_SKIP_ORIGINAL;
}

HookAction on_ring_draw_item2_pre(ModContext*, void* args, void*, void*) {
    auto* r = mods::arg<dMenu_Ring_c*>(args, 0);
    if (!qe_on(r)) return HOOK_CONTINUE;
    albw_ringmod_drawItem2(r);
    return HOOK_SKIP_ORIGINAL;
}

bool install(ModError* error, const char* name, ModResult rr) {
    if (rr != MOD_OK) {
        if (svc_log != nullptr) svc_log->error(mod_ctx, name);
        mods::set_error(error, MOD_ERROR, name);
        return false;
    }
    return true;
}

}  // namespace

ModResult albw_menu_ring_ext_init(ModError* error) {
    if (!install(error, "Ring_Create",
                 mods::hook_add_pre<Ring_Create>(svc_hook, on_ring_create_pre)) ||
        !install(error, "Ring_Delete",
                 mods::hook_add_post<Ring_Delete>(svc_hook, on_ring_delete_post)) ||
        !install(error, "Ring_Move",
                 mods::hook_add_pre<Ring_Move>(svc_hook, on_ring_move_pre)) ||
        !install(error, "Ring_Draw",
                 mods::hook_add_post<Ring_Draw>(svc_hook, on_ring_draw_post)) ||
        !install(error, "Ring_IsMoveEnd",
                 mods::hook_add_pre<Ring_IsMoveEnd>(svc_hook, on_ring_is_move_end_pre)) ||
        !install(error, "Ring_SetActiveCursor",
                 mods::hook_add_pre<Ring_SetActiveCursor>(svc_hook, on_ring_set_active_cursor_pre)) ||
        !install(error, "Ring_StickWaitInit",
                 mods::hook_add_post<Ring_StickWaitInit>(svc_hook, on_ring_stick_wait_init_post)) ||
        !install(error, "Ring_StickWaitProc",
                 mods::hook_add_pre<Ring_StickWaitProc>(svc_hook, on_ring_stick_wait_proc_pre)) ||
        !install(error, "Ring_StickMoveInit",
                 mods::hook_add_post<Ring_StickMoveInit>(svc_hook, on_ring_stick_move_init_post)) ||
        !install(error, "Ring_SetScale",
                 mods::hook_add_pre<Ring_SetScale>(svc_hook, on_ring_set_scale_pre)) ||
        !install(error, "Ring_DrawItem",
                 mods::hook_add_pre<Ring_DrawItem>(svc_hook, on_ring_draw_item_pre)) ||
        !install(error, "Ring_DrawItem2",
                 mods::hook_add_pre<Ring_DrawItem2>(svc_hook, on_ring_draw_item2_pre)))
    {
        return MOD_ERROR;
    }
    return MOD_OK;
}

#endif  // TARGET_PC

// ============================================
// NEW CODE ENDS HERE
// ============================================
