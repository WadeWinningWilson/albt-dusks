cmake_minimum_required(VERSION 3.26)
project(albw CXX)

# Locate a stock dusklight-main checkout (never the ALBT fork). Pass
# -DDUSKLIGHT_DIR=... or set the DUSKLIGHT_DIR env var; otherwise a sibling
# ../dusklight-main next to this repo is used if present.
if (NOT DUSKLIGHT_DIR AND DEFINED ENV{DUSKLIGHT_DIR})
    set(DUSKLIGHT_DIR "$ENV{DUSKLIGHT_DIR}" CACHE PATH "Path to stock Dusklight (main)")
endif ()
if (NOT DUSKLIGHT_DIR AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../dusklight-main/sdk")
    set(DUSKLIGHT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../dusklight-main" CACHE PATH "Path to stock Dusklight (main)")
endif ()

if (NOT DUSKLIGHT_VERSION)
    set(DUSKLIGHT_VERSION "a775418c667f748724c24283b9bad62ebe2dc48b")
endif ()

include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/FetchDusklight.cmake")
add_subdirectory("${DUSKLIGHT_DIR}/sdk" dusklight-sdk EXCLUDE_FROM_ALL)

add_mod(albt_full_plugin
        FEATURES game
        SOURCES
            src/mod.cpp
            src/albw_common.cpp
            src/albw_l1_input.cpp
            src/albw_settings_ui.cpp
            src/albw_stage.cpp
            src/config_vars.cpp
            src/meter.cpp
            src/lockout.cpp
            src/confuse.cpp
            src/magic_jar.cpp
            src/rupee_popup.cpp
            src/shield.cpp
            src/shield_adapt.cpp
            src/shield_game.cpp
            src/shield_hooks.cpp
            src/stick_cycle.cpp
            src/region_table.cpp
            src/region_hp.cpp
            src/soul_of_light.cpp
            src/enemy_rupees.cpp
            src/wolf_combat.cpp
            src/wolf_charge_art.cpp
            src/wolf_howl_combat.cpp
            src/midna_arm.cpp
            src/midna_arm_visual.cpp
            src/wolf_charge_hud.cpp
            src/hold_a_crawl.cpp
            src/quick_swap.cpp
            src/quick_equip.cpp
            src/extra_item_slot.cpp
            src/extra_item_slot_hooks.cpp
            src/z_item_hud.cpp
            src/mail.cpp
            src/mail_hooks.cpp
            src/focused_arts.cpp
            src/sim_time_scale.cpp
            src/flurry_rush.cpp
            src/flurry_hooks.cpp
            src/wolf_arts.cpp
            src/parry_master.cpp
            src/parry_hooks.cpp
            src/albw_combat.cpp
            src/boss_hp_hud.cpp
            src/lop_hud.cpp
            src/lop_item_belt.cpp
            src/boss_refinement.cpp
            src/diababa.cpp
            src/fyrus.cpp
            src/fyrus_golem.cpp
            src/armogohma.cpp
            src/epona_spur_hud.cpp
            src/hidden_skill_charge.cpp
            src/fyrus_phases.cpp
            src/boss_refinement_hooks.cpp
            src/rental_eligibility.cpp
            src/rental_shop.cpp
            src/rental_ui_text.cpp
            src/rental_dialogue.cpp
            src/rental_postman_hooks.cpp
            src/rental_shop_ui.cpp
            src/oocoo.cpp
            src/mq_hearts.cpp
            src/sword_atp.cpp
            src/potion.cpp
            src/shade_refuge.cpp
            src/outfit_stats.cpp
            src/outfit.cpp
            src/outfit_debug.cpp
            src/sumo_test.cpp
            src/wardrobe.cpp
            src/alink_compat.cpp
            src/albw_fork_compat.cpp
            src/albw_gameinfo_compat.cpp
            src/colossal_wallet.cpp
            src/clothes_pipeline.cpp
            src/ext_quick_equip.cpp
            src/menu_ext_status.cpp
            src/menu_ext_members.cpp
            src/menu_window_ext.cpp
            src/menu_ring_ext.cpp
        MOD_JSON mod.json
        RES_DIR res
        OVERLAY_DIR overlay
        TEXTURES_DIR textures
)
if (MSVC)
    target_link_options(albt_full_plugin PRIVATE "/PDBALTPATH:%_PDB%")
endif ()
