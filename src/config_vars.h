#pragma once

#include "mods/svc/config.h"

extern ConfigVarHandle g_meter_enabled;

extern ConfigVarHandle g_stick_cycle;
extern ConfigVarHandle g_hp_normal;
extern ConfigVarHandle g_hp_midboss;
extern ConfigVarHandle g_hp_boss;
extern ConfigVarHandle g_hp_final;
extern ConfigVarHandle g_region_hp;
extern ConfigVarHandle g_region_damage;
extern ConfigVarHandle g_region_mult;
extern ConfigVarHandle g_region_mult_rupees;
extern ConfigVarHandle g_recovery_orb;
extern ConfigVarHandle g_kill_rupees;

extern ConfigVarHandle g_manual_shield;
extern ConfigVarHandle g_shield_parry;
extern ConfigVarHandle g_shield_durability;
extern ConfigVarHandle g_focused_arts;
extern ConfigVarHandle g_flurry_rush;
extern ConfigVarHandle g_wolf_combat;
extern ConfigVarHandle g_wolf_arts_dev_test;
extern ConfigVarHandle g_hold_a_crawl;
extern ConfigVarHandle g_extra_item_slot_enabled;
extern ConfigVarHandle g_extra_item_slot_quick_swap;
extern ConfigVarHandle g_extra_item_slot_mode;
extern ConfigVarHandle g_extra_item_slot_legacy;
extern ConfigVarHandle g_quick_equip_wheel;
extern ConfigVarHandle g_postman_mail;
extern ConfigVarHandle g_postman_mail_test;

extern ConfigVarHandle g_parry_master;
extern ConfigVarHandle g_boss_hp_bars;
extern ConfigVarHandle g_enemy_hp_bars;
extern ConfigVarHandle g_link_damage_decrease;
extern ConfigVarHandle g_incoming_damage_scale;  // index: 0=0.5x 1=1x 2=2x 3=4x incoming damage
extern ConfigVarHandle g_boss_refinement;
extern ConfigVarHandle g_postman_rental;
extern ConfigVarHandle g_true_albw;
extern ConfigVarHandle g_end_game_transform;
extern ConfigVarHandle g_deku_leaf;
extern ConfigVarHandle g_master_quest;
extern ConfigVarHandle g_shade_refuge;
extern ConfigVarHandle g_outfit_stats;
// Outfit cluster (outfit / sumo_test / wardrobe) - fork host settings that have
// no stock equivalent; see albw_dusk_compat.h.
extern ConfigVarHandle g_dpad_quick_swap;
extern ConfigVarHandle g_sumo_outfit_fists;
extern ConfigVarHandle g_wardrobe_recovery_debug;
extern ConfigVarHandle g_cap_wear;
extern ConfigVarHandle g_soulbound_potion;
// ALBW Magic Armor economy exposure - maps to the fork's Settings->ALBW
// armorRupeeDrain == ALBW mode through the albw_dusk_compat.h shim.
extern ConfigVarHandle g_albw_magic_armor;
extern ConfigVarHandle g_ext_status_page;

extern ConfigVarHandle g_lop_hud_mode;
extern ConfigVarHandle g_parry_icons_mode;
extern ConfigVarHandle g_shield_hud_visibility;
extern ConfigVarHandle g_epona_spur_hud;
extern ConfigVarHandle g_focused_arts_cheat;

// Mod-owned persistent progress, stored in config.json - NOT in the player's
// save file. See albw_save_flags.h. Deliberately never given a UI control.
extern ConfigVarHandle g_progress_flags_a;
extern ConfigVarHandle g_progress_flags_b;

// Mod progression counters, config.json-backed (were event regs 100-113).
extern ConfigVarHandle g_ctr_fa_tier;
extern ConfigVarHandle g_ctr_potion_tier;
extern ConfigVarHandle g_ctr_heart_shop_tier;
extern ConfigVarHandle g_ctr_meter_shop_tier;
extern ConfigVarHandle g_ctr_bonus_half_hearts;
extern ConfigVarHandle g_ctr_bonus_quarter_hearts;
extern ConfigVarHandle g_ctr_sword_atp_bonus_0;
extern ConfigVarHandle g_ctr_sword_atp_bonus_1;
extern ConfigVarHandle g_ctr_sword_atp_bonus_2;
extern ConfigVarHandle g_ctr_sword_atp_bonus_3;
extern ConfigVarHandle g_ctr_sword_atp_step_0;
extern ConfigVarHandle g_ctr_sword_atp_step_1;
extern ConfigVarHandle g_ctr_sword_atp_step_2;
extern ConfigVarHandle g_ctr_sword_atp_step_3;
