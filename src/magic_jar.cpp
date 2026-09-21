// ============================================
// NEW CODE - ALBW Port (green magic jar: item models + the spawn/get chain)
//
// WHY THE JAR NEVER SPAWNS - THE ACTUAL CAUSE
// ------------------------------------------------------------------
// The two static model tables below were already correct (they are byte-identical
// to the fork's d_item_data.cpp:23 and :292), and the arc they name, "O_mD_gren",
// ships in the base game (stock references it at d_a_shop_item_static.cpp:40).
// They were simply never reached.
//
// fopAcM_createItem runs check_itemno() on the requested id BEFORE it builds the
// actor params (f_op_actor_mng.cpp:1685 -> MAKE_ITEM_PARAMS at :1692), and stock
// check_itemno rewrites BOTH magic items to a green rupee UNCONDITIONALLY
// (d_item.cpp:2141-2143 - no flag, no condition). So enemy_rupees.cpp's
// fopAcM_createItem(dItemNo_L_MAGIC_e, ...) produced a GREEN RUPEE actor: the jar
// was never requested, and field_item_res[0x09] / item_resource[0x09] were never
// indexed. The fork fixes this with an early return at d_item.cpp:2231-2233.
//
// Three more fork edits sit downstream of that gate and are equally load-bearing:
//   - daItem_c::itemGetNextExecute  fork d_a_obj_item.cpp:876-880
//         S_MAGIC/L_MAGIC -> procInitSimpleGetDemo(); itemGet();
//         (stock falls to default: -> OS_REPORT_ERROR, so the jar is un-pickable)
//   - daItem_c::itemGet             fork d_a_obj_item.cpp:945-949
//         S_MAGIC/L_MAGIC -> Z2SE_RED_LUPY_GET + execItemGet(m_itemNo)
//   - item_func_S/L_MAGIC           fork d_item.cpp:618-630 / :634-646
//         dMeter2_addALBWFraction(1,5) / (1,3) in place of the vanilla
//         dComIfGp_setItemMagicCount(4) / (8).
//         The last one is NOT optional: the mod's existing setItemMagicCount
//         bridge (meter.cpp:882-903) divides by getMaxMagic(), which is 0 on a
//         normal TP save, so it grants nothing and still skips the original.
//
//   - item_func_GREEN/BLUE_RUPEE   fork d_item.cpp:562-579 / :581-596
//         dMeter2_addALBWFraction(1,15) IN FRONT OF the vanilla
//         dComIfGp_setItemRupeeCount(1) / (5) - the fork's small refill source,
//         and the only pickup arms besides the two magic ids that the fork
//         touches. YELLOW/RED/PURPLE/ORANGE/SILVER stay vanilla in the fork.
//
// All seven targets are header-declared and exported, so every hook here takes the
// portable DEFINE_HOOK(&fn, Tag) form (tools/check_hooks.py BASELINE untouched).
// Every hook self-gates on albw_meter_is_enabled(): meter off == stock behaviour.
// ============================================

#include "global.h"

#include "d/actor/d_a_obj_item.h"
#include "d/d_com_inf_game.h"
#include "d/d_item.h"
#include "d/d_item_data.h"
#include "f_op/f_op_actor_mng.h"
#include "m_Do/m_Do_audio.h"
#include "Z2AudioLib/Z2SeMgr.h"

#include "albw_common.h"
#include "meter_bridge.h"
#include "albw_dusk_log.h"
#include "modules.h"
#include "mods/svc/hook.hpp"

// Temporary chain probe: jars spawn and can be picked up but grant nothing.
// Logs each link once per pickup so one run shows exactly where it dies.
// MUST be 0 before release (docs/RELEASE-PROCEDURE.md step 1).
#define ALBW_MAGICJAR_PROBE 1

#if TARGET_PC

namespace {

// fork d_item.cpp:2221 / d_a_obj_item.cpp:823, :898 / d_item.cpp:618, :634
DEFINE_HOOK(&check_itemno, CheckItemNo);
DEFINE_HOOK(&daItem_c::itemGetNextExecute, ItemGetNextExecute);
DEFINE_HOOK(&daItem_c::itemGet, ItemGet);
DEFINE_HOOK(&item_func_S_MAGIC, ItemFuncSMagic);
DEFINE_HOOK(&item_func_L_MAGIC, ItemFuncLMagic);
// fork d_item.cpp:562-579 / :581-596 - the OTHER half of the ALBW refill economy.
DEFINE_HOOK(&item_func_GREEN_RUPEE, ItemFuncGreenRupee);
DEFINE_HOOK(&item_func_BLUE_RUPEE, ItemFuncBlueRupee);

bool magic_drops_active() {
    return albw_meter_is_enabled();
}

bool is_magic_item(u8 itemNo) {
    return itemNo == dItemNo_S_MAGIC_e || itemNo == dItemNo_L_MAGIC_e;
}

// ============================================
// fork d_item.cpp:2231-2233 (inside check_itemno):
//     if (i_itemNo == dItemNo_S_MAGIC_e || i_itemNo == dItemNo_L_MAGIC_e) {
//         return (u8)i_itemNo;
//     }
// The fork returns before either vanilla rewrite. A post-hook that restores the
// requested id is behaviourally identical for these two ids (neither vanilla arm
// has a side effect - both only compute the return value) and leaves every other
// id exactly as stock computed it. Same shape as colossal_wallet.cpp:28-35.
// ============================================
void on_check_itemno_post(ModContext*, void* args, void* retval, void*) {
    if (retval == nullptr || !magic_drops_active()) {
        return;
    }
    const int itemNo = mods::arg<int>(args, 0);
    if (itemNo == dItemNo_S_MAGIC_e || itemNo == dItemNo_L_MAGIC_e) {
        *static_cast<u8*>(retval) = static_cast<u8>(itemNo);
    }
}

// ============================================
// fork d_a_obj_item.cpp:823-896, magic arm at :876-880.
// Stock's switch has no S_MAGIC/L_MAGIC case, so it reaches default: at
// d_a_obj_item.cpp:899-906 and (mItemOverridden being false for an engine-spawned
// drop) takes the OS_REPORT_ERROR arm - the item is flagged as taken and nothing
// is granted. Reproduced verbatim, including the fork's guard (:823-825) and the
// shared tail (:890-894), under HOOK_SKIP_ORIGINAL so the error arm never runs.
// ============================================
HookAction on_item_get_next_execute_pre(ModContext*, void* args, void*, void*) {
    auto* item = mods::arg<daItem_c*>(args, 0);
    if (item == nullptr || !magic_drops_active() || !is_magic_item(item->m_itemNo)) {
        return HOOK_CONTINUE;
    }

    if (!item->checkFlag(daItem_c::FLAG_DELETE_ITEM_e) &&
        !item->checkFlag(daItem_c::FLAG_INIT_GET_ITEM_e))
    {
        item->setFlag(daItem_c::FLAG_INIT_GET_ITEM_e);

        // fork :878-879 - magic drops behave like HEART / GREEN_RUPEE: no
        // canoe/horse and no wallet-full branching, the meter always accepts.
#if ALBW_MAGICJAR_PROBE
        DuskLog.info("[jar] 1 nextExecute arm: itemNo={} meter={}/{}", (int)item->m_itemNo,
                     albw_meter_get_value(), albw_meter_get_max());
#endif
        item->procInitSimpleGetDemo();
        item->itemGet();

        // fork :890-894
        fopAcM_onItem(item, item->mItemBitNo);
        item->mCcCyl.SetTgType(0);
        item->mCcCyl.OffCoSPrmBit(1);
        item->mCcCyl.ClrTgHit();
        item->mCcCyl.ClrCoHit();
    }
    return HOOK_SKIP_ORIGINAL;
}

// ============================================
// fork d_a_obj_item.cpp:945-949 (inside daItem_c::itemGet):
//     case dItemNo_S_MAGIC_e:
//     case dItemNo_L_MAGIC_e:
//         mDoAud_seStart(Z2SE_RED_LUPY_GET, NULL, 0, 0);
//         execItemGet(m_itemNo);
//         break;
// The fork's own comment records the orange-rupee SE as a deliberate placeholder
// until a dedicated magic-pickup sound exists; carried over unchanged.
// ============================================
HookAction on_item_get_pre(ModContext*, void* args, void*, void*) {
    auto* item = mods::arg<daItem_c*>(args, 0);
    if (item == nullptr || !magic_drops_active() || !is_magic_item(item->m_itemNo)) {
        return HOOK_CONTINUE;
    }
#if ALBW_MAGICJAR_PROBE
    DuskLog.info("[jar] 2 itemGet arm -> execItemGet({})", (int)item->m_itemNo);
#endif
    mDoAud_seStart(Z2SE_RED_LUPY_GET, NULL, 0, 0);
    execItemGet(item->m_itemNo);
#if ALBW_MAGICJAR_PROBE
    DuskLog.info("[jar] 4 after execItemGet: meter={}/{}", albw_meter_get_value(),
                 albw_meter_get_max());
#endif
    return HOOK_SKIP_ORIGINAL;
}

// ============================================
// fork d_item.cpp:618-630 / :634-646. The fork replaces the vanilla
// dComIfGp_setItemMagicCount(4) / (8) outright, so the ported body skips the
// original. 1/5 and 1/3 of the CURRENT ceiling (fork dMeter2_addALBWFraction,
// d_meter2.cpp:741-745 - sOilMaxVar, not the base pool), so the refill scales
// with meter upgrades.
// ============================================
HookAction on_item_func_s_magic_pre(ModContext*, void*, void*, void*) {
    if (!magic_drops_active()) {
        return HOOK_CONTINUE;
    }
#if ALBW_MAGICJAR_PROBE
    DuskLog.info("[jar] 3 item_func_S_MAGIC FIRED, meter={}/{}", albw_meter_get_value(),
                 albw_meter_get_max());
#endif
    albw_meter_add_fraction(1, 5);
    return HOOK_SKIP_ORIGINAL;
}

HookAction on_item_func_l_magic_pre(ModContext*, void*, void*, void*) {
    if (!magic_drops_active()) {
        return HOOK_CONTINUE;
    }
#if ALBW_MAGICJAR_PROBE
    DuskLog.info("[jar] 3 item_func_L_MAGIC FIRED, meter={}/{}", albw_meter_get_value(),
                 albw_meter_get_max());
#endif
    albw_meter_add_fraction(1, 3);
    return HOOK_SKIP_ORIGINAL;
}

// ============================================
// NEW CODE - ALBW Port (green/blue rupee meter refill)
//
// fork d_item.cpp:562-579
//     void item_func_GREEN_RUPEE() {
//     #if TARGET_PC
//         dMeter2_addALBWFraction(1, 15);
//     #endif
//         dComIfGp_setItemRupeeCount(1);
//     }
// fork d_item.cpp:581-596 is the same with (1, 15) and setItemRupeeCount(5).
//
// These two are the fork's SMALL refill source and they were never ported: the
// mod granted meter only from the dedicated magic drops, so the fork's actual
// moment-to-moment economy (grass/pot/enemy green rupees topping the meter up)
// was missing entirely. The fork's own comment records the design intent -
// "Green rupees are the small magic fill source ... intentionally smaller than
// L_MAGIC (1/3) so the dedicated orange-rupee drop feels meaningfully larger."
//
// UNLIKE the magic arms above these do NOT skip the original: the fork ADDS the
// meter call in front of the vanilla dComIfGp_setItemRupeeCount and keeps the
// wallet credit. A pre-hook returning HOOK_CONTINUE reproduces the fork's exact
// statement order (meter first, then rupee count).
//
// The fork does NOT touch YELLOW/RED/PURPLE/ORANGE/SILVER (d_item.cpp:598-616
// are vanilla one-liners), so neither do we - the refill is the two smallest
// denominations only.
// ============================================
HookAction on_item_func_green_rupee_pre(ModContext*, void*, void*, void*) {
    if (!magic_drops_active()) {
        return HOOK_CONTINUE;
    }
    albw_meter_add_fraction(1, 15);
    return HOOK_CONTINUE;
}

HookAction on_item_func_blue_rupee_pre(ModContext*, void*, void*, void*) {
    if (!magic_drops_active()) {
        return HOOK_CONTINUE;
    }
    albw_meter_add_fraction(1, 15);
    return HOOK_CONTINUE;
}

// Loud-but-non-fatal, same shape as meter.cpp:1982 `install`. Returning MOD_ERROR
// here would short-circuit mod_initialize (mod.cpp:187) and unload the WHOLE mod
// over one missing jar hook; a silent skip would hide the miss. So: log it by
// name, keep running, and let the boot log say which link of the chain is absent.
void install(const char* what, ModResult r) {
    if (r != MOD_OK && svc_log != nullptr) {
        svc_log->error(mod_ctx, what);
        svc_log->error(mod_ctx,
                       "albw magic jar: hook above did NOT install - the jar chain is "
                       "broken at this link for this run");
    }
}

}  // namespace

ModResult albw_magic_jar_init(ModError*) {
    // --- field (dropped-in-world) models: {arc, bmd, bck, brk, field_0xa, heap} ---
    // S_MAGIC: green rupee field model (Always arc, tev variant 1).
    dItem_data::field_item_res[dItemNo_S_MAGIC_e] =
        {"Always", 0x0017, -0x0001, 0x0031, 0x1, 0x1000};
    // L_MAGIC: green magic jar (O_mD_gren, local bmd index 3).
    dItem_data::field_item_res[dItemNo_L_MAGIC_e] =
        {"O_mD_gren", 0x0003, -0x0001, -0x0001, 0xFF, 0x1000};

    // --- get-demo (pickup) models:
    //     {arc, bmd, btk, bck, brk, btp, tevFrm, btpFrm, texture, texScale, f0x14} ---
    // S_MAGIC: green rupee appearance (F_gD_rupy, TevFrame=1).
    dItem_data::item_resource[dItemNo_S_MAGIC_e] =
        {"F_gD_rupy", 0x0004, -0x0001, -0x0001, 0x0007, -0x0001, 0x1, -0x1, 0x002D, 0x64, 0x0000};
    // L_MAGIC: green jar (O_mD_gren), 0x82 scale.
    dItem_data::item_resource[dItemNo_L_MAGIC_e] =
        {"O_mD_gren", 0x0003, -0x0001, -0x0001, -0x0001, -0x0001, -0x1, -0x1, 0x0000, 0x82, 0x0000};

    install("check_itemno", mods::hook::add_post<CheckItemNo>(on_check_itemno_post));
    install("daItem_c::itemGetNextExecute",
            mods::hook::add_pre<ItemGetNextExecute>(on_item_get_next_execute_pre));
    install("daItem_c::itemGet", mods::hook::add_pre<ItemGet>(on_item_get_pre));
    install("item_func_S_MAGIC", mods::hook::add_pre<ItemFuncSMagic>(on_item_func_s_magic_pre));
    install("item_func_L_MAGIC", mods::hook::add_pre<ItemFuncLMagic>(on_item_func_l_magic_pre));
    install("item_func_GREEN_RUPEE",
            mods::hook::add_pre<ItemFuncGreenRupee>(on_item_func_green_rupee_pre));
    install("item_func_BLUE_RUPEE",
            mods::hook::add_pre<ItemFuncBlueRupee>(on_item_func_blue_rupee_pre));

    return MOD_OK;
}

#endif  // TARGET_PC

// ============================================
// NEW CODE ENDS HERE
// ============================================
