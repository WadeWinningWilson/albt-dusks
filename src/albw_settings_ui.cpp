#include "albw_settings_ui.h"

#include "albw_common.h"
#include "config_vars.h"
#include "modules.h"

namespace {

UiWindowHandle g_settingsWindow = 0;

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

ModResult build_combat_tab(ModContext*, UiWindowHandle, UiElementHandle left, UiElementHandle,
                           void*, ModError* error) {
    if (svc_ui->pane_add_section(mod_ctx, left, "ALBW combat") != MOD_OK) {
        return MOD_ERROR;
    }
    if (albw_ui_add_toggle(left, "ALBW meter",
                           "P3: pool, spends, lockout perks (slingshot stun, bombling orbit, "
                           "double-claw finisher).",
                           g_meter_enabled) != MOD_OK ||
        albw_ui_add_toggle(left, "Manual shielding",
                           "Hold ZR (R2) to guard without Z-target.",
                           g_manual_shield) != MOD_OK ||
        albw_ui_add_toggle(left, "Shield parry & bash",
                           "Perfect-guard window, bash charges, meter gain/loss on block.",
                           g_shield_parry) != MOD_OK ||
        albw_ui_add_toggle(left, "Shield durability",
                           "Blocks wear the equipped shield; break clears it and unlocks Postman "
                           "rental eligibility for that shield.",
                           g_shield_durability) != MOD_OK ||
        albw_ui_add_toggle(left, "Focused Arts",
                           "Hidden-skill rework: mortal draw / large spins bill the ALBW meter "
                           "and respect lockout gates.",
                           g_focused_arts) != MOD_OK ||
        albw_ui_add_toggle(left, "Flurry Rush",
                           "Perfect-dodge slow-mo chain (requires Focused Arts + OC telegraph).",
                           g_flurry_rush) != MOD_OK)
    {
        return MOD_ERROR;
    }

    if (svc_ui->pane_add_section(mod_ctx, left, "Shield HUD") != MOD_OK) {
        return MOD_ERROR;
    }
    if (albw_ui_add_select(left, "LoP HUD mode",
                           "Reanchors bash/wolf charge icons only. Full Lies-of-P meter "
                           "relayout (wallet/hearts/item belt move) is Tier C2 — not in this "
                           "build; turning this on will not rearrange the rest of the HUD.",
                           g_lop_hud_mode, kLopHudModes,
                           sizeof(kLopHudModes) / sizeof(kLopHudModes[0])) != MOD_OK ||
        albw_ui_add_select(left, "Parry charge icons",
                           "Icon style for shield-bash charge HUD when shield parry is on.",
                           g_parry_icons_mode, kParryIconModes,
                           sizeof(kParryIconModes) / sizeof(kParryIconModes[0])) != MOD_OK ||
        albw_ui_add_select(left, "Shield HUD visibility",
                           "When durability and parry aux HUD stay visible outside guard.",
                           g_shield_hud_visibility, kShieldHudVisibilityModes,
                           sizeof(kShieldHudVisibilityModes) / sizeof(kShieldHudVisibilityModes[0])) !=
            MOD_OK ||
        albw_ui_add_toggle(left, "Epona dash-spur HUD",
                           "Show the Epona dash-spur icons while riding. Off hides them. "
                           "Does not affect wolf charges, shield HUD, or parry icons.",
                           g_epona_spur_hud) != MOD_OK)
    {
        return MOD_ERROR;
    }
    (void)error;
    return MOD_OK;
}

ModResult build_qol_tab(ModContext*, UiWindowHandle, UiElementHandle left, UiElementHandle,
                        void*, ModError* error) {
    if (svc_ui->pane_add_section(mod_ctx, left, "Field QoL") != MOD_OK) {
        return MOD_ERROR;
    }
    if (albw_stick_cycle_build_panel(left, error) != MOD_OK ||
        albw_soul_of_light_build_panel(left, error) != MOD_OK ||
        albw_ui_add_toggle(left, "Hold-A crawl",
                           "Hold A ~1s while standing to enter crawl (fork QoL).",
                           g_hold_a_crawl) != MOD_OK)
    {
        return MOD_ERROR;
    }

    if (svc_ui->pane_add_section(mod_ctx, left, "Extra item slot") != MOD_OK) {
        return MOD_ERROR;
    }
    if (albw_ui_add_select(left, "Extra Item Slot", kExtraItemSlotHelp, g_extra_item_slot_mode,
                           kExtraItemSlotModes,
                           sizeof(kExtraItemSlotModes) / sizeof(kExtraItemSlotModes[0])) != MOD_OK ||
        albw_ui_add_toggle(left, "Quick Equip Wheel",
                           "With Extra Item Slot on: hold L1/LB ~0.25s for Z-only assign; tap "
                           "L1/LB for the full item ring (not L2 z-target).",
                           g_quick_equip_wheel) != MOD_OK)
    {
        return MOD_ERROR;
    }
    return MOD_OK;
}

ModResult build_difficulty_tab(ModContext*, UiWindowHandle, UiElementHandle left, UiElementHandle,
                               void*, ModError* error) {
    if (svc_ui->pane_add_section(mod_ctx, left, "Region & economy") != MOD_OK) {
        return MOD_ERROR;
    }
    if (albw_region_hp_build_panel(left, error) != MOD_OK) {
        return MOD_ERROR;
    }
    if (albw_ui_add_toggle(left, "Enemy death rupees",
                           "Wallet credit on kill and fight victory.",
                           g_kill_rupees) != MOD_OK)
    {
        return MOD_ERROR;
    }
    return MOD_OK;
}

ModResult build_story_tab(ModContext*, UiWindowHandle, UiElementHandle left, UiElementHandle,
                          void*, ModError* error) {
    if (svc_ui->pane_add_section(mod_ctx, left, "Postman & mail") != MOD_OK) {
        return MOD_ERROR;
    }
    if (albw_ui_add_toggle(left, "Junior Postman mail",
                           "North Faron deliver Postman + onboarding letters (phase 0).",
                           g_postman_mail) != MOD_OK ||
        albw_ui_add_toggle(left, "Postman mail test bypass",
                           "Editor-equivalent: skip story gates for mail spawn (test only).",
                           g_postman_mail_test) != MOD_OK)
    {
        return MOD_ERROR;
    }

    if (svc_ui->pane_add_section(mod_ctx, left, "Wolf Link") != MOD_OK) {
        return MOD_ERROR;
    }
    if (albw_ui_add_toggle(left, "Wolf Link combat",
                           "Field-attack stun, bite charges, charge HUD.",
                           g_wolf_combat) != MOD_OK)
    {
        return MOD_ERROR;
    }

    if (svc_ui->pane_add_section(mod_ctx, left, "Tier B") != MOD_OK) {
        return MOD_ERROR;
    }
    if (albw_ui_add_toggle(left, "Parry Master",
                           "Failed blocks chip HP into a reclaim pool.",
                           g_parry_master) != MOD_OK ||
        albw_ui_add_toggle(left, "Boss HP bars",
                           "LoP-style name + bar for lock-on dungeon bosses.",
                           g_boss_hp_bars) != MOD_OK ||
        albw_ui_add_toggle(left, "Enemy HP bars",
                           "Floating bar + current/max numbers over each regular enemy's head.",
                           g_enemy_hp_bars) != MOD_OK ||
        albw_ui_add_toggle(left, "Boss Refinement",
                           "Any-sword boss gates, Armogohma pacing brain, Fyrus/Morpheel helpers. "
                           "Arena scripts land in follow-up passes.",
                           g_boss_refinement) != MOD_OK ||
        albw_ui_add_toggle(left, "Postman death strip",
                           "After Talo rescue, death strips ALBW items for rental recovery.",
                           g_postman_rental) != MOD_OK ||
        albw_ui_add_toggle(left, "True ALBW",
                           "Postman rental shop unlocked at any point, full catalog available "
                           "from the start (bypasses the Talo-rescue story gate).",
                           g_true_albw) != MOD_OK ||
        albw_ui_add_toggle(left, "End-Game Transform",
                           "Grants the free wolf transform (shadow crystal + Midna + LV0-3) so "
                           "Wolf Combat is usable. Always on when True ALBW is enabled.",
                           g_end_game_transform) != MOD_OK ||
        albw_ui_add_toggle(left, "Deku Leaf glide",
                           "WW Deku Leaf: R+A launches a gust takeoff, hold to keep rising, then "
                           "glide (drains the meter). Bomb button drops a live bomb mid-glide.",
                           g_deku_leaf) != MOD_OK ||
        albw_ui_add_toggle(left, "Outfit Stats",
                           "Per-outfit combat + swim traits: outgoing/incoming damage scaling "
                           "(Sumo glass-cannon, etc.) and human underwater swimming.",
                           g_outfit_stats) != MOD_OK ||
        albw_ui_add_toggle(left, "Master Quest hearts",
                           "MQ heart tiers + stamina meter bonus (shop rows when rental ships).",
                           g_master_quest) != MOD_OK)
    {
        return MOD_ERROR;
    }
    (void)error;
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
        UI_TAB_DESC_INIT,
        UI_TAB_DESC_INIT,
        UI_TAB_DESC_INIT,
        UI_TAB_DESC_INIT,
    };
    tabs[0].title = "Combat & HUD";
    tabs[0].build = build_combat_tab;
    tabs[1].title = "Quality of life";
    tabs[1].build = build_qol_tab;
    tabs[2].title = "Difficulty";
    tabs[2].build = build_difficulty_tab;
    tabs[3].title = "Story & Tier B";
    tabs[3].build = build_story_tab;

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
    if (albw_ui_add_toggle(panel, "ALBW meter", "Toggle the ALBW energy meter suite.", g_meter_enabled) !=
            MOD_OK ||
        albw_ui_add_toggle(panel, "Extra item slot", "Z third item + Midna on Left d-pad.",
                           g_extra_item_slot_enabled) != MOD_OK)
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
