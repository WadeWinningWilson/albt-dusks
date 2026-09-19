// A Link Between Twilight — collective mod (dev.albt.albw).
// Standalone feature dusks (dev.albt.*) remain separate products; do not load both.

#include "albw_common.h"
#include "albw_settings_ui.h"
#include "albw_stage.h"
#include "config_vars.h"
#include "colossal_wallet.h"
#include "clothes_pipeline.h"
#include "menu_window_ext.h"
#include "menu_ring_ext.h"
#include "focused_arts.h"
#include "flurry_rush.h"
#include "hold_a_crawl.h"
#include "enemy_lockout.h"
#include "lockout.h"
#include "mail.h"
#include "meter.h"
#include "quick_swap.h"
#include "quick_equip.h"
#include "ext_status.h"
#include "extra_item_slot.h"
#include "extra_item_slot_hooks.h"
#include "z_item_hud.h"
#include "parry_hooks.h"
#include "wolf_arts.h"
#include "wolf_combat.h"
#include "end_game_transform.h"
#include "hurricane_spin.h"
#include "deku_leaf.h"
#include "outfit_swim.h"
#include "modules.h"
#include "tear_actor.hpp"  // albw_tear_actor_init / albw_tear_glow_init (Dusklight 2.0 tear)
#include "potion_grant.h"  // albw_potion_grant_init / albw_potion_grant_tick (soulbound potion)
#include "potion_bottle.h"  // albw_potion_bottle_init (soulbound potion drink/heal/consume)
#include "mq_hearts.h"
#include "shield_mod.h"
#include "boss_refinement_hooks.h"
#include "diababa.h"
#include "fyrus.h"
#include "armogohma.h"
#include "epona_spur_hud.h"
#include "lop_hud.h"
#include "rental_shop.h"

#include "mods/service.hpp"
#include "mods/svc/config.h"
#include "mods/svc/hook.h"
#include "mods/svc/log.h"
#include "mods/svc/ui.h"

DEFINE_MOD();
IMPORT_SERVICE(LogService, svc_log);
IMPORT_SERVICE(HookService, svc_hook);
// Host may ship config@1.0 (Mods panel only) or @1.1+ (Settings → ALBT tab).
IMPORT_SERVICE_VERSION(ConfigService, svc_config, 0);
IMPORT_SERVICE(UiService, svc_ui);
IMPORT_SERVICE(HostService, svc_host);

namespace {

ModResult register_all_config(ModError* error) {
    if (albw_register_bool("meter", true, &g_meter_enabled) != MOD_OK ||
        albw_register_bool("stick_cycle", true, &g_stick_cycle) != MOD_OK ||
        albw_register_int("hp_normal", 1, &g_hp_normal) != MOD_OK ||
        albw_register_int("hp_midboss", 1, &g_hp_midboss) != MOD_OK ||
        albw_register_int("hp_boss", 1, &g_hp_boss) != MOD_OK ||
        albw_register_int("hp_final", 1, &g_hp_final) != MOD_OK ||
        albw_register_int("link_damage_decrease", 1, &g_link_damage_decrease) != MOD_OK ||
        albw_register_int("incoming_damage_scale", 1, &g_incoming_damage_scale) != MOD_OK ||
        // Fork parity (settings.cpp:117-120): region master OFF, health axis pre-armed.
        albw_register_bool("region_hp", true, &g_region_hp) != MOD_OK ||
        albw_register_bool("region_damage", false, &g_region_damage) != MOD_OK ||
        albw_register_bool("region_mult", false, &g_region_mult) != MOD_OK ||
        albw_register_bool("region_mult_rupees", true, &g_region_mult_rupees) != MOD_OK ||
        albw_register_bool("recovery_orb", true, &g_recovery_orb) != MOD_OK ||
        albw_register_bool("kill_rupees", true, &g_kill_rupees) != MOD_OK ||
        albw_register_bool("manual_shield", false, &g_manual_shield) != MOD_OK ||
        albw_register_bool("shield_parry", false, &g_shield_parry) != MOD_OK ||
        albw_register_bool("shield_durability", false, &g_shield_durability) != MOD_OK ||
        albw_register_bool("focused_arts", false, &g_focused_arts) != MOD_OK ||
        albw_register_bool("flurry_rush", false, &g_flurry_rush) != MOD_OK ||
        albw_register_bool("wolf_combat", false, &g_wolf_combat) != MOD_OK ||
        albw_register_bool("wolf_arts_dev_test", false, &g_wolf_arts_dev_test) != MOD_OK ||
        albw_register_bool("hold_a_crawl", false, &g_hold_a_crawl) != MOD_OK ||
        albw_register_bool("extra_item_slot_enabled", false, &g_extra_item_slot_enabled) != MOD_OK ||
        albw_register_bool("extra_item_slot_quick_swap", false, &g_extra_item_slot_quick_swap) != MOD_OK ||
        albw_register_int("extra_item_slot_mode", 0, &g_extra_item_slot_mode) != MOD_OK ||
        albw_register_bool("extra_item_slot", false, &g_extra_item_slot_legacy) != MOD_OK ||
        albw_register_bool("quick_equip_wheel", false, &g_quick_equip_wheel) != MOD_OK ||
        albw_register_bool("postman_mail", true, &g_postman_mail) != MOD_OK ||
        albw_register_bool("postman_mail_test", false, &g_postman_mail_test) != MOD_OK ||
        albw_register_bool("parry_master", false, &g_parry_master) != MOD_OK ||
        albw_register_bool("boss_hp_bars", false, &g_boss_hp_bars) != MOD_OK ||
        albw_register_bool("enemy_hp_bars", false, &g_enemy_hp_bars) != MOD_OK ||
        albw_register_bool("boss_refinement", false, &g_boss_refinement) != MOD_OK ||
        albw_register_bool("shade_refuge", false, &g_shade_refuge) != MOD_OK ||
        albw_register_bool("outfit_stats", false, &g_outfit_stats) != MOD_OK ||
        // Outfit cluster - fork host settings with no stock equivalent (see
        // albw_dusk_compat.h). cap_wear is an int choice: 0=Off 1=None 2=Green
        // 3=Red 4=Blue, matching fork dusk::CapWearMode.
        albw_register_bool("dpad_quick_swap", false, &g_dpad_quick_swap) != MOD_OK ||
        albw_register_bool("sumo_outfit_fists", false, &g_sumo_outfit_fists) != MOD_OK ||
        albw_register_bool("wardrobe_recovery_debug", false, &g_wardrobe_recovery_debug) != MOD_OK ||
        albw_register_int("cap_wear", 0, &g_cap_wear) != MOD_OK ||
        albw_register_bool("soulbound_potion", false, &g_soulbound_potion) != MOD_OK ||
        albw_register_bool("ext_status_page", false, &g_ext_status_page) != MOD_OK ||
        albw_register_bool("postman_rental", true, &g_postman_rental) != MOD_OK ||
        albw_register_bool("true_albw", false, &g_true_albw) != MOD_OK ||
        albw_register_bool("end_game_transform", false, &g_end_game_transform) != MOD_OK ||
        albw_register_bool("deku_leaf", false, &g_deku_leaf) != MOD_OK ||
        albw_register_bool("master_quest", false, &g_master_quest) != MOD_OK ||
        albw_register_int("lop_hud_mode", 0, &g_lop_hud_mode) != MOD_OK ||
        albw_register_int("parry_icons_mode", 0, &g_parry_icons_mode) != MOD_OK ||
        albw_register_int("shield_hud_visibility", 0, &g_shield_hud_visibility) != MOD_OK ||
        albw_register_bool("epona_spur_hud", true, &g_epona_spur_hud) != MOD_OK ||
        albw_register_int("focused_arts_cheat", 0, &g_focused_arts_cheat) != MOD_OK)
    {
        if (error != nullptr) {
            error->code = MOD_ERROR;
        }
        svc_log->error(mod_ctx, "failed to register config");
        return MOD_ERROR;
    }
    return MOD_OK;
}

ModResult register_gameplay_settings() {
    // Fork ALBW tab names/sections; extras remain in the Mods panel for now.
    if (albw_register_gameplay_bool(
            "Systems", "Soul of Light",
            "After Talo is rescued, dying halves your rupees and leaves a Soul of Light at the "
            "death spot to recover part of them. Off keeps your wallet unchanged and spawns "
            "nothing. Item strip and meter refill on death are unaffected.",
            g_recovery_orb) != MOD_OK)
    {
        svc_log->warn(mod_ctx, "Settings ALBT tab unavailable (host config v1.1+ required)");
        return MOD_OK;
    }
    albw_register_gameplay_bool(
        "Systems", "Enemy Death Rupees",
        "Credit rupees directly to your wallet when enemies die and when boss fights end. "
        "Vanilla drop tables (hearts, jars, ground rupees) are unchanged.",
        g_kill_rupees);
    albw_register_gameplay_bool(
        "Quality of Life", "Stick Cycle Lock-on",
        "While Z-targeting, right stick left/right cycles between nearby enemies that are "
        "in combat with you instead of manually rotating the lock-on camera.",
        g_stick_cycle);
    albw_register_gameplay_bool(
        "Systems", "Manual Shielding",
        "Hold ZR (R2) with a shield equipped to raise guard without Z-target lock-on. When on, "
        "Z-target guard also requires holding ZR.",
        g_manual_shield);
    albw_register_gameplay_bool(
        "Systems", "Shield Parry & Bash Charges",
        "LoP-style perfect guard window after raising shield; bash charge economy on blocks and "
        "shield bashes. Disables vanilla R-bash in favor of ZR+B when enabled.",
        g_shield_parry);
    albw_register_gameplay_bool(
        "Systems", "Shield Durability",
        "Shields take durability damage on blocks; break empties the slot and marks it rental-"
        "eligible. Off by default.",
        g_shield_durability);
    return MOD_OK;
}

}  // namespace

extern "C" {

MOD_EXPORT ModResult mod_initialize(ModError* error) {
    if (register_all_config(error) != MOD_OK) {
        return MOD_ERROR;
    }
    albw_extra_item_slot_migrate_legacy_config();
    albw_extra_item_slot_sync_bools_from_mode();
    register_gameplay_settings();

    if (albw_meter_init(error) != MOD_OK || albw_lockout_init(error) != MOD_OK ||
        albw_enemy_lockout_init(error) != MOD_OK ||
        albw_magic_jar_init(error) != MOD_OK ||
        albw_rupee_popup_init(error) != MOD_OK ||
        albw_tear_particles_init(error) != MOD_OK ||
        albw_mq_hearts_init(error) != MOD_OK ||
        albw_mq_heart_meter_init(error) != MOD_OK ||
        albw_shield_init(error) != MOD_OK ||
        albw_focused_arts_init(error) != MOD_OK ||
        albw_hurricane_init(error) != MOD_OK ||
        albw_deku_leaf_init(error) != MOD_OK ||
        albw_outfit_swim_init(error) != MOD_OK ||
        albw_flurry_init(error) != MOD_OK ||
        albw_wolf_combat_init(error) != MOD_OK ||
        albw_wolf_charge_art_init(error) != MOD_OK ||
        albw_wolf_howl_combat_init(error) != MOD_OK ||
        albw_midna_arm_init(error) != MOD_OK ||
        albw_midna_arm_visual_init(error) != MOD_OK ||
        albw_wolf_arts_init(error) != MOD_OK ||
        albw_hold_a_crawl_init(error) != MOD_OK ||
        albw_extra_item_slot_init(error) != MOD_OK ||
        albw_extra_item_slot_hooks_init(error) != MOD_OK ||
        albw_z_item_hud_hooks_init(error) != MOD_OK ||
        albw_quick_swap_init(error) != MOD_OK ||
        albw_quick_equip_init(error) != MOD_OK ||
        albw_mail_init(error) != MOD_OK ||
        albw_parry_master_init(error) != MOD_OK ||
        albw_boss_refinement_init(error) != MOD_OK ||
        albw_diababa_init(error) != MOD_OK ||
        albw_fyrus_init(error) != MOD_OK ||
        albw_fyrus_golem_init(error) != MOD_OK ||
        albw_fyrus_phases_init(error) != MOD_OK ||
        albw_armogohma_init(error) != MOD_OK ||
        albw_lop_hud_init(error) != MOD_OK ||
        albw_rental_shop_init(error) != MOD_OK ||
        albw_stick_cycle_init(error) != MOD_OK ||
        albw_region_hp_init(error) != MOD_OK || albw_soul_of_light_init(error) != MOD_OK ||
        albw_tear_actor_init(error) != MOD_OK || albw_tear_glow_init(error) != MOD_OK ||
        albw_enemy_rupees_init(error) != MOD_OK ||
        albw_colossal_wallet_init(error) != MOD_OK ||
        albw_clothes_pipeline_init(error) != MOD_OK ||
        albw_menu_window_ext_init(error) != MOD_OK ||
        albw_menu_ring_ext_init(error) != MOD_OK ||
        albw_potion_grant_init(error) != MOD_OK ||
        albw_potion_bottle_init(error) != MOD_OK ||
        albw_epona_spur_hud_init(error) != MOD_OK)
    {
        return MOD_ERROR;
    }

    // fork custom_assets.cpp:1872 - claims.ini ingest runs when the boot asset
    // scan completes; the mod's equivalent moment is the end of activation.
    dExtInv_rescanClaims();

    if (albw_settings_ui_register_panel(error) != MOD_OK) {
        svc_log->warn(mod_ctx, "mods panel unavailable; config keys still work");
    }

    svc_log->info(mod_ctx, "dev.albt.albw ready (collective; standalones ship separately)");
    return MOD_OK;
}

MOD_EXPORT ModResult mod_update(ModError*) {
    albw_extra_item_slot_config_tick();
    albw_stage_tick();
    albw_mail_update();
    albw_quick_swap_tick();
    albw_quick_equip_tick();
    albw_wolf_arts_tick();
    albw_focused_arts_tick();
    albw_flurry_tick();
    albw_colossal_wallet_tick();
    albw_rental_shop_tick();
    albw_end_game_transform_tick();
    albw_deku_leaf_tick();
    albw_potion_grant_tick();
    albw_meter_update();
    return MOD_OK;
}

MOD_EXPORT ModResult mod_shutdown(ModError* error) {
    albw_enemy_rupees_shutdown(error);
    albw_soul_of_light_shutdown(error);
    albw_region_hp_shutdown(error);
    albw_stick_cycle_shutdown(error);
    albw_rental_shop_shutdown(error);
    albw_mail_shutdown(error);
    albw_parry_master_shutdown(error);
    albw_boss_refinement_shutdown(error);
    albw_diababa_shutdown(error);
    albw_fyrus_shutdown(error);
    albw_lop_hud_shutdown(error);
    albw_extra_item_slot_hooks_shutdown(error);
    albw_z_item_hud_hooks_shutdown(error);
    albw_extra_item_slot_shutdown(error);
    albw_quick_equip_shutdown(error);
    albw_quick_swap_shutdown(error);
    albw_hold_a_crawl_shutdown(error);
    albw_wolf_arts_shutdown(error);
    albw_wolf_combat_shutdown(error);
    albw_flurry_shutdown(error);
    albw_focused_arts_shutdown(error);
    albw_shield_shutdown(error);
    albw_meter_shutdown(error);
    albw_lockout_shutdown(error);
    return MOD_OK;
}

}
