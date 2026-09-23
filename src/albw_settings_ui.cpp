#include "albw_settings_ui.h"

#include "albw_common.h"
#include "albw_save_flags.h"
#include "config_vars.h"
#include "modules.h"

namespace {

UiWindowHandle g_settingsWindow = 0;

// ============================================
// "New Game" reset (config.json shop/unlock reset). Destructive, so it routes
// through a DANGER confirm dialog; the confirm action calls the store's own
// reset (albw_save_flags_reset_all). Nothing here touches the player's save.
// ============================================
void on_new_game_confirmed(ModContext*, UiDialogHandle, void*) {
    albw_save_flags_reset_all();
}

void on_new_game_pressed(ModContext*, void*) {
    // static: the host may retain the actions array while the dialog is visible
    // (is_disabled is polled every frame), so it must outlive this call.
    static UiDialogAction actions[2];
    actions[0] = UI_DIALOG_ACTION_INIT;
    actions[0].label = "Reset everything";
    actions[0].on_pressed = on_new_game_confirmed;
    actions[1] = UI_DIALOG_ACTION_INIT;
    actions[1].label = "Cancel";

    UiDialogDesc dlg = UI_DIALOG_DESC_INIT;
    dlg.title = "New Game — reset ALBW shop progress?";
    dlg.body_rml =
        "Wipes <b>only this mod's shop progression</b> — the ALBW purchases and unlocks kept "
        "in the mod's own config (Focused Arts tiers, Flurry Rush, Wolf Link combat and "
        "Midna's Shield, heart / meter / potion upgrades, sword upgrades, rental eligibility "
        "and stored gear, Sumo unlocks).<br/><br/>Nothing in the actual game is changed: your "
        "Zelda save file, items, hearts, and story progress are all left exactly as they are. "
        "This cannot be undone.";
    dlg.variant = UI_DIALOG_DANGER;
    dlg.actions = actions;
    dlg.action_count = 2;
    svc_ui->dialog_push(mod_ctx, &dlg, nullptr);
}

static const char* kExtraItemSlotModes[] = {
    "Off",
    "Extra Only",
    "Extra + Quick Swap",
};

static const char* kExtraItemSlotHelp =
    "<br/><b>Off</b>: vanilla d-pad.<br/>"
    "<b>Extra Only</b>: Midna on left D-pad, third item on Z (use with Z / R1).<br/>"
    "<b>Extra + Quick Swap</b>: physical <b>L1 / LB</b> opens the item wheel (not L2 "
    "z-target); D-pad Up/Right/Down cycle sword, shield, and outfit. Midna stays on Left.";

// Names match the fork setting game.lopHud (LopHudMode Off / VanillaHearts /
// HealthBar) so the mod and the fork describe the same feature.
static const char* kLopHudModes[] = {
    "Off",
    "Vanilla Hearts",
    "Health Bar",
};

static const char* kParryIconModes[] = {
    "Spur only",
    "Spur + shield emblem",
    "Shield emblem only",
};

static const char* kShieldHudVisibilityModes[] = {
    "Off (combat + linger)",
    "Durability meter always on",
    "Parry charge always on",
    "Durability + parry always on",
};

// ============================================
// Settings window — six functional tabs (docs/SETTINGS-REORG-PLAN.md, user
// 2026-09-22). Grouped by FUNCTION, not development order: the old "Tier B"
// dumping ground is retired and the dev-only Postman mail test bypass + Junior
// Postman mail rows are gone from the window. Every remaining control keeps its
// config var and meaning — this is a pure re-home, no behavior change.
// ============================================

// --- Tab 1: Combat ---------------------------------------------------------
ModResult build_combat_tab(ModContext*, UiWindowHandle, UiElementHandle left, UiElementHandle,
                           void*, ModError* error) {
    if (svc_ui->pane_add_section(mod_ctx, left, "Meter & Arts") != MOD_OK) {
        return MOD_ERROR;
    }
    if (albw_ui_add_toggle(left, "ALBW meter",
                           "An ALBW like stamina meter that tracks items and player sword swings. "
                           "Be sure to test your items when you're all out of stamina for secret "
                           "effects that may even get that stamina back!",
                           g_meter_enabled) != MOD_OK ||
        albw_ui_add_toggle(left, "Focused Arts",
                           "New moves and systems for the Hidden Skills, build your meter for new "
                           "finishers and damage payoffs!",
                           g_focused_arts) != MOD_OK ||
        albw_ui_add_toggle(left, "Flurry Rush",
                           "Sold at the rental shop after all the Focused Arts are bought. "
                           "Backflip while locking onto an enemy with a full shield bash bar to "
                           "unleash a fast combo! Requires Focused Arts",
                           g_flurry_rush) != MOD_OK)
    {
        return MOD_ERROR;
    }

    if (svc_ui->pane_add_section(mod_ctx, left, "Shield & Parry") != MOD_OK) {
        return MOD_ERROR;
    }
    if (albw_ui_add_toggle(left, "Manual shielding",
                           "Hold ZR (R2) to guard without Z-target.",
                           g_manual_shield) != MOD_OK ||
        albw_ui_add_toggle(left, "Shield parry & bash",
                           "Perfect-guard window, bash charges, meter gain/loss on block.",
                           g_shield_parry) != MOD_OK ||
        albw_ui_add_toggle(left, "Shield durability",
                           "Normal blocks wear and tear your shield until it breaks. Perfect "
                           "Parries keep your shield's health! Buy your shields again easily at "
                           "the Postman shop",
                           g_shield_durability) != MOD_OK ||
        albw_ui_add_toggle(left, "Parry Master",
                           "Failed parries chip your health, so strike back to earn that health "
                           "again!",
                           g_parry_master) != MOD_OK ||
        albw_ui_add_select(left, "Parry charge icons",
                           "Icon style for shield-bash charge HUD when shield parry is on.",
                           g_parry_icons_mode, kParryIconModes,
                           sizeof(kParryIconModes) / sizeof(kParryIconModes[0])) != MOD_OK ||
        albw_ui_add_select(left, "Shield HUD visibility",
                           "Choose what type of parry charges stay on screen and when",
                           g_shield_hud_visibility, kShieldHudVisibilityModes,
                           sizeof(kShieldHudVisibilityModes) / sizeof(kShieldHudVisibilityModes[0])) !=
            MOD_OK)
    {
        return MOD_ERROR;
    }

    if (svc_ui->pane_add_section(mod_ctx, left, "Enemies") != MOD_OK) {
        return MOD_ERROR;
    }
    if (albw_ui_add_toggle(left, "Devil Trigger",
                           "Below 50% health enemies become fed up with Link and unleash their "
                           "true speed and damage, parry them to stand a chance! Currently "
                           "covered: Darknuts and Bokoblins",
                           g_devil_trigger) != MOD_OK ||
        albw_ui_add_toggle(left, "Boss Refinement",
                           "New and improved Boss Fights! Currently supported: Armogohma, Fyrus",
                           g_boss_refinement) != MOD_OK)
    {
        return MOD_ERROR;
    }

    if (svc_ui->pane_add_section(mod_ctx, left, "Wolf Link") != MOD_OK) {
        return MOD_ERROR;
    }
    if (albw_ui_add_toggle(left, "Wolf Link combat",
                           "Grants new field-attack stun, bite charges, wolf howl, Midna's "
                           "hand, charge HUD, Midna's shield.<br/>Once unlocked in the shop, "
                           "press d-pad up for wolf howl and d-pad right for Midna's Hand; "
                           "hold R to raise Midna's shield (parry to open enemies).",
                           g_wolf_combat) != MOD_OK)
    {
        return MOD_ERROR;
    }
    (void)error;
    return MOD_OK;
}

// --- Tab 2: HUD (non-shield) ----------------------------------------------
ModResult build_hud_tab(ModContext*, UiWindowHandle, UiElementHandle left, UiElementHandle,
                        void*, ModError* error) {
    if (svc_ui->pane_add_section(mod_ctx, left, "HUD") != MOD_OK) {
        return MOD_ERROR;
    }
    if (albw_ui_add_select(left, "LoP HUD mode",
                           "A Soulslike HUD inspired by Lies of P",
                           g_lop_hud_mode, kLopHudModes,
                           sizeof(kLopHudModes) / sizeof(kLopHudModes[0])) != MOD_OK ||
        albw_ui_add_toggle(left, "Epona dash-spur HUD",
                           "Show the Epona dash-spur icons while riding. Off hides them. "
                           "Does not affect wolf charges, shield HUD, or parry icons.",
                           g_epona_spur_hud) != MOD_OK ||
        albw_ui_add_toggle(left, "Boss HP bars",
                           "LoP-style name + bar for lock-on dungeon bosses.",
                           g_boss_hp_bars) != MOD_OK ||
        albw_ui_add_toggle(left, "Enemy HP bars",
                           "Floating bar + current/max numbers over each regular enemy's head.",
                           g_enemy_hp_bars) != MOD_OK)
    {
        return MOD_ERROR;
    }
    (void)error;
    return MOD_OK;
}

// --- Tab 3: Items & Outfits ------------------------------------------------
ModResult build_items_tab(ModContext*, UiWindowHandle, UiElementHandle left, UiElementHandle,
                          void*, ModError* error) {
    if (svc_ui->pane_add_section(mod_ctx, left, "Items") != MOD_OK) {
        return MOD_ERROR;
    }
    if (albw_ui_add_select(left, "Extra Item Slot", kExtraItemSlotHelp, g_extra_item_slot_mode,
                           kExtraItemSlotModes,
                           sizeof(kExtraItemSlotModes) / sizeof(kExtraItemSlotModes[0])) != MOD_OK ||
        albw_ui_add_toggle(left, "Quick-Equip Wheel",
                           "With Extra Item Slot on, hold LB to quickly select an item for "
                           "battle. Tap for normal behavior ~In Progress~",
                           g_quick_equip_wheel) != MOD_OK ||
        albw_ui_add_toggle(left, "Deku Leaf glide",
                           "WW Deku Leaf: R+A launches a gust takeoff, hold to keep rising, then "
                           "glide (drains the meter). Bomb button drops a live bomb mid-glide.",
                           g_deku_leaf) != MOD_OK ||
        albw_ui_add_toggle(left, "Soulbound Red Potion",
                           "Souls-style flask, heal 2 hearts per swig mid-combat and reclaim "
                           "drinks on death. Upgrades available through the shop ~In Progress~",
                           g_soulbound_potion) != MOD_OK)
    {
        return MOD_ERROR;
    }

    if (svc_ui->pane_add_section(mod_ctx, left, "Outfits & Forms") != MOD_OK) {
        return MOD_ERROR;
    }
    if (albw_ui_add_toggle(left, "Outfit Stats",
                           "Specific outfits now have different stats, equip sumo if you want to "
                           "test your limits. Also unlocks Zora swim for non Zora outfits",
                           g_outfit_stats) != MOD_OK ||
        albw_ui_add_toggle(left, "Sumo Fists Only",
                           "Hide Link's sword/shield for a bare-knuckle look while the "
                           "Sumo Outfit is worn.",
                           g_sumo_outfit_fists) != MOD_OK ||
        albw_ui_add_toggle(left, "End-Game Transform",
                           "Unlock wolf transform early, but you still need to progress the game "
                           "for certain features",
                           g_end_game_transform) != MOD_OK)
    {
        return MOD_ERROR;
    }
    (void)error;
    return MOD_OK;
}

// --- Tab 4: Difficulty & Economy -------------------------------------------
ModResult build_difficulty_tab(ModContext*, UiWindowHandle, UiElementHandle left,
                               UiElementHandle right, void*, ModError* error) {
    if (svc_ui->pane_add_section(mod_ctx, left, "Difficulty") != MOD_OK) {
        return MOD_ERROR;
    }
    // Region HP steppers + Region Damage + the Region Multipliers group row; the
    // group opens its master/axes subpanel in the RIGHT detail pane.
    if (albw_region_hp_build_panel(left, right, error) != MOD_OK) {
        return MOD_ERROR;
    }
    if (albw_ui_add_toggle(left, "Master Quest hearts",
                           "Upgrade health straight through the Postman's shop!",
                           g_master_quest) != MOD_OK)
    {
        return MOD_ERROR;
    }

    if (svc_ui->pane_add_section(mod_ctx, left, "Economy") != MOD_OK) {
        return MOD_ERROR;
    }
    if (albw_ui_add_toggle(left, "Enemy death rupees",
                           "Tilt the TP economy in your favor with more rupees per enemy kill",
                           g_kill_rupees) != MOD_OK ||
        albw_ui_add_toggle(left, "ALBW Magic Armor",
                           "Turns the armor into a risk and reward. Defeating an enemy gains you "
                           "300 rupees, getting hit drains you by 500. ~In Progress~",
                           g_albw_magic_armor) != MOD_OK)
    {
        return MOD_ERROR;
    }
    return MOD_OK;
}

// --- Tab 5: Progression & Story --------------------------------------------
ModResult build_progression_tab(ModContext*, UiWindowHandle, UiElementHandle left, UiElementHandle,
                                void*, ModError* error) {
    if (svc_ui->pane_add_section(mod_ctx, left, "Rental & Story") != MOD_OK) {
        return MOD_ERROR;
    }
    if (albw_ui_add_toggle(left, "Postman death strip",
                           "ALBW item stripping, 13 of your items get sent to the postman shop "
                           "on your death ~In Progress~",
                           g_postman_rental) != MOD_OK ||
        albw_ui_add_toggle(left, "True ALBW",
                           "Tired of waiting until you save Talo? Unlock the postman's shop and "
                           "all purchasables at any point. Postman himself is by Link's House",
                           g_true_albw) != MOD_OK)
    {
        return MOD_ERROR;
    }

    if (svc_ui->pane_add_section(mod_ctx, left, "Progression") != MOD_OK) {
        return MOD_ERROR;
    }
    if (albw_soul_of_light_build_panel(left, error) != MOD_OK) {
        return MOD_ERROR;
    }

    // New Game: reset the mod's per-install progression (config.json). Confirmed
    // via a danger dialog. Placed last so a misclick is unlikely.
    if (svc_ui->pane_add_section(mod_ctx, left, "Reset") != MOD_OK) {
        return MOD_ERROR;
    }
    UiControlDesc newGameBtn = UI_CONTROL_DESC_INIT;
    newGameBtn.kind = UI_CONTROL_BUTTON;
    newGameBtn.label = "New Game (reset ALBW shop progress)";
    newGameBtn.help_rml =
        "Resets ONLY this mod's shop progression — the ALBW purchases and unlocks stored in "
        "the mod's config (per install, shared across save files). Nothing in the actual game "
        "or your Zelda save file is changed. Asks to confirm first.";
    newGameBtn.on_pressed = on_new_game_pressed;
    if (svc_ui->pane_add_control(mod_ctx, left, &newGameBtn, nullptr) != MOD_OK) {
        return MOD_ERROR;
    }
    return MOD_OK;
}

// --- Tab 6: Quality of Life ------------------------------------------------
ModResult build_qol_tab(ModContext*, UiWindowHandle, UiElementHandle left, UiElementHandle,
                        void*, ModError* error) {
    if (svc_ui->pane_add_section(mod_ctx, left, "Quality of Life") != MOD_OK) {
        return MOD_ERROR;
    }
    if (albw_stick_cycle_build_panel(left, error) != MOD_OK ||
        albw_ui_add_toggle(left, "Hold-A crawl",
                           "Crawl, just like the name implies",
                           g_hold_a_crawl) != MOD_OK)
    {
        return MOD_ERROR;
    }
    return MOD_OK;
}

void on_settings_window_closed(ModContext*, UiWindowHandle, void*) {
    g_settingsWindow = 0;
}

void on_open_settings_window(ModContext*, void*) {
    if (g_settingsWindow != 0) {
        if (svc_ui->window_close(mod_ctx, g_settingsWindow) == MOD_OK) {
            g_settingsWindow = 0;
        }
        return;
    }

    UiTabDesc tabs[] = {
        UI_TAB_DESC_INIT, UI_TAB_DESC_INIT, UI_TAB_DESC_INIT,
        UI_TAB_DESC_INIT, UI_TAB_DESC_INIT, UI_TAB_DESC_INIT,
    };
    tabs[0].title = "Combat";
    tabs[0].build = build_combat_tab;
    tabs[1].title = "HUD";
    tabs[1].build = build_hud_tab;
    tabs[2].title = "Items & Outfits";
    tabs[2].build = build_items_tab;
    tabs[3].title = "Difficulty & Economy";
    tabs[3].build = build_difficulty_tab;
    tabs[4].title = "Progression & Story";
    tabs[4].build = build_progression_tab;
    tabs[5].title = "Quality of Life";
    tabs[5].build = build_qol_tab;

    UiWindowDesc desc = UI_WINDOW_DESC_INIT;
    desc.tabs = tabs;
    desc.tab_count = sizeof(tabs) / sizeof(tabs[0]);
    desc.on_closed = on_settings_window_closed;

    if (svc_ui->window_push(mod_ctx, &desc, &g_settingsWindow) != MOD_OK) {
        svc_log->error(mod_ctx, "failed to open ALBT settings window");
    }
}

ModResult build_mods_launcher(ModContext*, UiElementHandle panel, void*, ModError*) {
    if (svc_ui->pane_add_section(mod_ctx, panel, "A Link Between Twilight") != MOD_OK) {
        return MOD_ERROR;
    }

    if (svc_ui->pane_add_text(
            mod_ctx, panel,
            "Full settings use a two-pane window: controls on the left, option descriptions and "
            "SELECT pickers on the right.",
            nullptr) != MOD_OK)
    {
        return MOD_ERROR;
    }

    UiControlDesc openBtn = UI_CONTROL_DESC_INIT;
    openBtn.kind = UI_CONTROL_BUTTON;
    openBtn.label = "Open ALBT settings…";
    openBtn.help_rml =
        "Opens the tabbed settings window with a help pane for multi-option controls.";
    openBtn.on_pressed = on_open_settings_window;
    if (svc_ui->pane_add_control(mod_ctx, panel, &openBtn, nullptr) != MOD_OK) {
        return MOD_ERROR;
    }

    if (svc_ui->pane_add_section(mod_ctx, panel, "Quick toggles") != MOD_OK) {
        return MOD_ERROR;
    }
    if (albw_ui_add_toggle(panel, "ALBW meter", "Toggle the ALBW meter on/off", g_meter_enabled) !=
            MOD_OK ||
        albw_ui_add_toggle(panel, "Extra item slot", "Z third item + Midna on Left d-pad.",
                           g_extra_item_slot_enabled) != MOD_OK ||
        albw_ui_add_toggle(panel, "True ALBW",
                           "Unlock the Postman's shop and all purchasables early. Postman himself "
                           "is in Ordon near Link's House",
                           g_true_albw) != MOD_OK)
    {
        return MOD_ERROR;
    }

    if (svc_ui->pane_add_section(mod_ctx, panel, "Notes") != MOD_OK) {
        return MOD_ERROR;
    }
    if (svc_ui->pane_add_text(
            mod_ctx, panel,
            "Standalone dev.albt.* dusks remain separate products — do not load both.",
            nullptr) != MOD_OK)
    {
        return MOD_ERROR;
    }
    return MOD_OK;
}

}  // namespace

ModResult albw_settings_ui_register_panel(ModError* error) {
    UiModsPanelDesc panel = UI_MODS_PANEL_DESC_INIT;
    panel.build = build_mods_launcher;
    if (svc_ui->register_mods_panel(mod_ctx, &panel) != MOD_OK) {
        if (error != nullptr) {
            error->code = MOD_UNSUPPORTED;
        }
        svc_log->warn(mod_ctx, "mods panel unavailable; use Settings → ALBT or config files");
        return MOD_UNSUPPORTED;
    }
    return MOD_OK;
}
