// ============================================
// NEW CODE - ALBW Port (Soulbound Red Potion - drink / heal / consume seam)
// The fork's drink behavior lives in exported daAlink_c bottle METHODS. Two of
// them (commonBottleDrink) interleave the soulbound edits (RED-bottle heal via
// dAlbwPotion_getHealQuarters + charge decrement via dAlbwPotion_consumeSoulbound-
// Drink) with the giant vanilla body, so the whole fork body is ported verbatim
// onto a layout-compatible subclass AlbwPotionLink_c and dispatched to for a
// soulbound-red drink (qualified call = static dispatch); sibling calls inside
// the body resolve to the exported stock methods via inheritance. Bodies are
// auto-extracted (tools/port/potion_bottle.json -> potion_bottle_port.inc).
//
// The other three fork edits (procBottleDrinkInit, procBottleOpenInit,
// procBottleSwingInit) are pure top-of-function early-return GUARDS. They are
// reproduced verbatim as guard-only pre-hooks rather than whole-body ports:
//   - procBottleDrinkInit: block starting a drink at 0 charges (canDrink==false).
//   - procBottleOpenInit / procBottleSwingInit: block using the soulbound bottle
//     to catch (open/swing) -> return checkWaitAction().
// procBottleOpenInit's vanilla body calls the runtime-resolved (DUSK_NOINLINE)
// fopAcM_create, which is not link-visible in a mod TU, so a whole-body port of
// it could not link; the guard is the only fork change and is reproduced exactly.
//
// GREEN-POTION NOTE: commonBottleDrink's fork body also carries the unrelated
// green-potion stamina edit (dMeter2_addALBWFraction), a symbol the .dusk does
// not expose. It is dead code on this dispatch path (the soulbound slot holds a
// RED bottle, so mEquipItem is never GREEN), so the port_tool substitution routes
// it to the .dusk-native meter seam albw_meter_add_base_fraction purely to satisfy
// the linker; it never executes here.
// ============================================

#include "helpers/string.hpp"  // TEXT_SPAN - must precede any d_save.h include

#define private public
#include "d/actor/d_a_alink.h"
#undef private

#include "potion_bottle.h"
#include "albw_common.h"
#include "config_vars.h"
#include "modules.h"
#include "mods/svc/hook.hpp"

#if TARGET_PC

#include "potion.h"
#include "albw_game.h"
#include "meter_bridge.h"
#include "d/d_com_inf_game.h"

// ============================================
// NEW CODE - ALBW Port
// Layout-compatible subclass carrying the ported fork bottle body. It must NOT
// redefine the exported daAlink_c methods (LNK2005); it declares its own copies
// and the auto-extracted .inc supplies their bodies.
// ============================================
class AlbwPotionLink_c : public daAlink_c {
public:
    int commonBottleDrink(BOOL param_0);
};

#include "potion_bottle_port.inc"  // AlbwPotionLink_c::commonBottleDrink (ported fork body)

namespace {

// A drink that concerns the soulbound red potion in the select slot. Mirrors the
// fork's own predicate (dAlbwPotion_isSoulboundRedInSlot on the resolved slot);
// self-gates to false for every other bottle so the stock path is untouched.
bool soulbound_drink(const daAlink_c* link) {
    if (link == nullptr) {
        return false;
    }
    return dAlbwPotion_isSoulboundRedInSlot(
        albw_game::get_select_item_index(static_cast<int>(link->mSelectItemId)));
}

// commonBottleDrink: run the ported fork body (RED heal + charge decrement) for a
// soulbound-red drink; stock body for everyone else.
DEFINE_HOOK(&daAlink_c::commonBottleDrink, CommonBottleDrink);
HookAction on_CommonBottleDrink(ModContext*, void* args, void* retval, void*) {
    daAlink_c* link = mods::arg<daAlink_c*>(args, 0);
    if (!soulbound_drink(link)) {
        return HOOK_CONTINUE;
    }
    const BOOL param0 = mods::arg<BOOL>(args, 1);
    const int r = static_cast<AlbwPotionLink_c*>(link)->AlbwPotionLink_c::commonBottleDrink(param0);
    if (retval != nullptr) {
        *static_cast<int*>(retval) = r;
    }
    return HOOK_SKIP_ORIGINAL;
}

// procBottleDrinkInit: block starting a drink when the soulbound bottle is empty
// (fork guard: if (!canDrinkSelectItem(mSelectItemId, itemNo)) return 0;).
// canDrinkSelectItem self-gates (true for any non-soulbound bottle).
DEFINE_HOOK(&daAlink_c::procBottleDrinkInit, ProcBottleDrinkInit);
HookAction on_ProcBottleDrinkInit(ModContext*, void* args, void* retval, void*) {
    daAlink_c* link = mods::arg<daAlink_c*>(args, 0);
    if (link == nullptr) {
        return HOOK_CONTINUE;
    }
    const u16 itemNo = mods::arg<u16>(args, 1);
    if (!dAlbwPotion_canDrinkSelectItem(static_cast<int>(link->mSelectItemId),
                                        static_cast<u8>(itemNo))) {
        if (retval != nullptr) {
            *static_cast<int*>(retval) = 0;
        }
        return HOOK_SKIP_ORIGINAL;
    }
    return HOOK_CONTINUE;
}

// procBottleOpenInit: block opening the soulbound bottle to catch (fork guard:
// if (isSoulboundBottleSlot(getSelectItemIndex(mSelectItemId))) return checkWaitAction();).
DEFINE_HOOK(&daAlink_c::procBottleOpenInit, ProcBottleOpenInit);
HookAction on_ProcBottleOpenInit(ModContext*, void* args, void* retval, void*) {
    daAlink_c* link = mods::arg<daAlink_c*>(args, 0);
    if (link == nullptr) {
        return HOOK_CONTINUE;
    }
    // FUNDAMENTAL FIX: gate on the feature-gated soulbound predicate, NOT the raw
    // slot number - SLOT_11 is the first VANILLA bottle slot, so the raw check
    // blocked open/swing (catching, scooping) for ANY bottle there with every
    // toggle off. isSoulboundRedInSlot requires the toggle AND a red bottle.
    if (dAlbwPotion_isSoulboundRedInSlot(
            albw_game::get_select_item_index(static_cast<int>(link->mSelectItemId)))) {
        if (retval != nullptr) {
            *static_cast<int*>(retval) = static_cast<int>(link->checkWaitAction());
        }
        return HOOK_SKIP_ORIGINAL;
    }
    return HOOK_CONTINUE;
}

// procBottleSwingInit: block swinging the soulbound bottle to catch (same fork
// guard as procBottleOpenInit).
DEFINE_HOOK(&daAlink_c::procBottleSwingInit, ProcBottleSwingInit);
HookAction on_ProcBottleSwingInit(ModContext*, void* args, void* retval, void*) {
    daAlink_c* link = mods::arg<daAlink_c*>(args, 0);
    if (link == nullptr) {
        return HOOK_CONTINUE;
    }
    // FUNDAMENTAL FIX: gate on the feature-gated soulbound predicate, NOT the raw
    // slot number - SLOT_11 is the first VANILLA bottle slot, so the raw check
    // blocked open/swing (catching, scooping) for ANY bottle there with every
    // toggle off. isSoulboundRedInSlot requires the toggle AND a red bottle.
    if (dAlbwPotion_isSoulboundRedInSlot(
            albw_game::get_select_item_index(static_cast<int>(link->mSelectItemId)))) {
        if (retval != nullptr) {
            *static_cast<int*>(retval) = static_cast<int>(link->checkWaitAction());
        }
        return HOOK_SKIP_ORIGINAL;
    }
    return HOOK_CONTINUE;
}

}  // namespace

ModResult albw_potion_bottle_init(ModError* error) {
    const bool ok =
        mods::hook::add_pre<CommonBottleDrink>(on_CommonBottleDrink) == MOD_OK &&
        mods::hook::add_pre<ProcBottleDrinkInit>(on_ProcBottleDrinkInit) == MOD_OK &&
        mods::hook::add_pre<ProcBottleOpenInit>(on_ProcBottleOpenInit) == MOD_OK &&
        mods::hook::add_pre<ProcBottleSwingInit>(on_ProcBottleSwingInit) == MOD_OK;
    if (!ok) {
        if (svc_log != nullptr) {
            svc_log->error(mod_ctx, "failed to hook a soulbound potion bottle proc");
        }
        mods::set_error(error, MOD_ERROR, "potion bottle hooks");
        return MOD_ERROR;
    }
    if (svc_log != nullptr) {
        svc_log->info(mod_ctx, "albw soulbound potion bottle (drink/heal/consume) ready");
    }
    return MOD_OK;
}

#else

ModResult albw_potion_bottle_init(ModError*) { return MOD_OK; }

#endif  // TARGET_PC
