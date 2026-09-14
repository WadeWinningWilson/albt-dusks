// ============================================
// NEW CODE - ALBW Port (green magic-jar item models)
//
// WHY THE JAR "NEVER SPAWNS"
// ------------------------------------------------------------------
// The pickup path (item_func_S/L_MAGIC -> ALBW meter refill) was ported, and the
// enemy-drop spawn calls fopAcM_createItem(dItemNo_L_MAGIC_e, ...). But an item's
// in-world appearance comes from two STATIC data tables the fork also edits and
// the mod never got:
//   - dItem_data::field_item_res[] : the FIELD (dropped-in-world) model. L_MAGIC's
//     stock row is the orange-rupee placeholder, so the dropped jar had no valid
//     model and never appeared.
//   - dItem_data::item_resource[]  : the get-demo (pickup) model.
// The fork points both at the vanilla "O_mD_gren" green-jar arc (present in the
// base game) for L_MAGIC and the green rupee for S_MAGIC. These arrays are
// exported, writable DUSK_GAME_DATA (dllimport) — the mod already reads them via
// dItem_data::getTexScale/getTexture — so we patch the two rows at init instead
// of forking the whole d_item_data.cpp table. Values verbatim from the fork.
// ============================================

#include "global.h"

#include "d/d_item_data.h"
#include "modules.h"

#if TARGET_PC

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

    return MOD_OK;
}

#endif  // TARGET_PC

// ============================================
// NEW CODE ENDS HERE
// ============================================
