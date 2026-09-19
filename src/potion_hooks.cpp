// ============================================
// NEW CODE - ALBW Port (Soulbound Red Potion - HUD / select-item / save seams)
// Reproduces, at hook seams, the fork's soulbound-red branches added to the
// select-item count family (d_com_inf_game.cpp dComIfGp_getSelectItemNum /
// getSelectItemMaxNum / setSelectItemNum / addSelectItemNum), the HUD count
// refresh (d_meter2.cpp dMeter2_c::moveBottleNum) and the bottle-fill guard
// (d_save.cpp dSv_player_item_c::setBottleItemIn). Every branch self-gates on
// dAlbwPotion_isSoulboundRedInSlot / isSoulboundRedItem, so the vanilla path is
// untouched for any non-soulbound bottle. The ring count seam
// (dMenu_Ring_c::getItemNum/getItemMaxNum) lives in menu_ring_ext.cpp.
// ============================================

#include "helpers/string.hpp"  // TEXT_SPAN - must precede any d_save.h include

#define private public
#include "d/d_save.h"     // dSv_player_item_c::mItems
#include "d/d_meter2.h"   // dMeter2_c::mBottleNum / mpMeterDraw
#undef private

#include "potion_hooks.h"

#if TARGET_PC

#include "potion.h"
#include "albw_common.h"
#include "modules.h"
#include "mods/svc/hook.hpp"
#include "d/d_com_inf_game.h"
#include "d/d_meter2_draw.h"

namespace {

// Verbatim reproduction of the file-static dSv_item_rename (d_save.cpp:43): the
// donor helper is file-local and not link-visible, so the setBottleItemIn guard
// below reproduces it to match the fork's renamed comparison exactly.
u8 potion_item_rename(u8 i_itemNo) {
    switch (i_itemNo) {
    case dItemNo_OIL_BOTTLE_2_e:
        return dItemNo_OIL_BOTTLE_e;
    case dItemNo_RED_BOTTLE_2_e:
        return dItemNo_RED_BOTTLE_e;
    case dItemNo_OIL2_e:
        return dItemNo_OIL_e;
    default:
        return i_itemNo;
    }
}

// dComIfGp_getSelectItemNum POST: fork branch — a soulbound-red select item
// reports its remaining charges. Vanilla has no RED case, so it returned 0.
DEFINE_HOOK(&dComIfGp_getSelectItemNum, GetSelectItemNum);
void on_GetSelectItemNum_post(ModContext*, void* args, void* retval, void*) {
    if (retval == nullptr) {
        return;
    }
    const int idx = mods::arg<int>(args, 0);
    if (dAlbwPotion_isSoulboundRedInSlot(dComIfGs_getSelectItemIndex(idx))) {
        *static_cast<s16*>(retval) =
            static_cast<s16>(dComIfGs_getBottleNum(kAlbwPotionSoulboundBottleIdx));
    }
}

// dComIfGp_getSelectItemMaxNum POST: fork branch — max charges for the bottle.
DEFINE_HOOK(&dComIfGp_getSelectItemMaxNum, GetSelectItemMaxNum);
void on_GetSelectItemMaxNum_post(ModContext*, void* args, void* retval, void*) {
    if (retval == nullptr) {
        return;
    }
    const int idx = mods::arg<int>(args, 0);
    if (dAlbwPotion_isSoulboundRedInSlot(dComIfGs_getSelectItemIndex(idx))) {
        *static_cast<int*>(retval) = static_cast<int>(dAlbwPotion_getMaxUses());
    }
}

// dComIfGp_setSelectItemNum PRE: fork branch — clamp to max and write the bottle
// charge count. Vanilla has no RED case (it would no-op), so skip the original.
DEFINE_HOOK(&dComIfGp_setSelectItemNum, SetSelectItemNum);
HookAction on_SetSelectItemNum_pre(ModContext*, void* args, void*, void*) {
    const int idx = mods::arg<int>(args, 0);
    if (!dAlbwPotion_isSoulboundRedInSlot(dComIfGs_getSelectItemIndex(idx))) {
        return HOOK_CONTINUE;
    }
    s16 num = mods::arg<s16>(args, 1);
    if (num > static_cast<s16>(dAlbwPotion_getMaxUses())) {
        num = static_cast<s16>(dAlbwPotion_getMaxUses());
    }
    dComIfGs_setBottleNum(kAlbwPotionSoulboundBottleIdx, static_cast<u8>(num));
    return HOOK_SKIP_ORIGINAL;
}

// dComIfGp_addSelectItemNum PRE: fork branch — add to the bottle charge count.
DEFINE_HOOK(&dComIfGp_addSelectItemNum, AddSelectItemNum);
HookAction on_AddSelectItemNum_pre(ModContext*, void* args, void*, void*) {
    const int idx = mods::arg<int>(args, 0);
    if (!dAlbwPotion_isSoulboundRedInSlot(dComIfGs_getSelectItemIndex(idx))) {
        return HOOK_CONTINUE;
    }
    const s16 num = mods::arg<s16>(args, 1);
    dComIfGs_addBottleNum(kAlbwPotionSoulboundBottleIdx, num);
    return HOOK_SKIP_ORIGINAL;
}

// dMeter2_c::moveBottleNum POST: fork widened the per-frame refresh loop from
// BEE_CHILD-only to "BEE_CHILD OR soulbound-red slot". Vanilla already handled
// BEE_CHILD; this adds the soulbound-red iterations it skips, so the HUD button
// count redraws the moment a charge is spent or refilled.
DEFINE_HOOK(&dMeter2_c::moveBottleNum, MoveBottleNum);
void on_MoveBottleNum_post(ModContext*, void* args, void*, void*) {
    dMeter2_c* meter = mods::arg<dMeter2_c*>(args, 0);
    if (meter == nullptr || meter->mpMeterDraw == nullptr) {
        return;
    }
    for (int i = 0; i < 4; i++) {
        if (!dAlbwPotion_isSoulboundRedInSlot(static_cast<u8>(i + SLOT_11))) {
            continue;
        }
        if (meter->mBottleNum[i] != dComIfGs_getBottleNum(i)) {
            for (int j = 0; j < 2; j++) {
                if (i + SLOT_11 == dComIfGs_getSelectItemIndex(j)) {
                    meter->mpMeterDraw->setItemNum(
                        static_cast<u8>(j),
                        static_cast<u8>(dComIfGp_getSelectItemNum(j)),
                        static_cast<u8>(dComIfGp_getSelectItemMaxNum(j)));
                    meter->mBottleNum[i] = dComIfGs_getBottleNum(i);
                }
            }
        }
    }
}

// dSv_player_item_c::setBottleItemIn PRE: fork guard — never let a bottle-fill
// overwrite the soulbound-red bottle with a non-soulbound-red item. Reproduces
// the fork's first-match loop so it fires only when vanilla would have targeted
// the protected slot; every other case falls through to the stock body.
DEFINE_HOOK(&dSv_player_item_c::setBottleItemIn, SetBottleItemIn);
HookAction on_SetBottleItemIn_pre(ModContext*, void* args, void*, void*) {
    dSv_player_item_c* self = mods::arg<dSv_player_item_c*>(args, 0);
    if (self == nullptr) {
        return HOOK_CONTINUE;
    }
    const u8 cur = potion_item_rename(mods::arg<u8>(args, 1));
    const u8 nw = potion_item_rename(mods::arg<u8>(args, 2));
    for (int i = 0; i < 4; i++) {
        if (cur == self->mItems[i + SLOT_11]) {
            if (dAlbwPotion_isSoulboundRedInSlot(static_cast<u8>(i + SLOT_11)) &&
                !dAlbwPotion_isSoulboundRedItem(nw)) {
                return HOOK_SKIP_ORIGINAL;  // protect the soulbound bottle
            }
            return HOOK_CONTINUE;  // first match is not protected -> stock handles it
        }
    }
    return HOOK_CONTINUE;
}

}  // namespace

ModResult albw_potion_hooks_init(ModError* error) {
    const bool ok =
        mods::hook::add_post<GetSelectItemNum>(on_GetSelectItemNum_post) == MOD_OK &&
        mods::hook::add_post<GetSelectItemMaxNum>(on_GetSelectItemMaxNum_post) == MOD_OK &&
        mods::hook::add_pre<SetSelectItemNum>(on_SetSelectItemNum_pre) == MOD_OK &&
        mods::hook::add_pre<AddSelectItemNum>(on_AddSelectItemNum_pre) == MOD_OK &&
        mods::hook::add_post<MoveBottleNum>(on_MoveBottleNum_post) == MOD_OK &&
        mods::hook::add_pre<SetBottleItemIn>(on_SetBottleItemIn_pre) == MOD_OK;
    if (!ok) {
        if (svc_log != nullptr) {
            svc_log->error(mod_ctx, "failed to hook a soulbound potion count seam");
        }
        mods::set_error(error, MOD_ERROR, "potion count hooks");
        return MOD_ERROR;
    }
    if (svc_log != nullptr) {
        svc_log->info(mod_ctx, "albw soulbound potion count/HUD/save seams ready");
    }
    return MOD_OK;
}

#else

ModResult albw_potion_hooks_init(ModError*) { return MOD_OK; }

#endif  // TARGET_PC
