#include "extra_item_slot.h"

#include "albw_common.h"
#include "config_vars.h"

int albw_extra_item_slot_mode() {
    const int mode = albw_cfg_int(g_extra_item_slot_mode, ALBW_EXTRA_ITEM_SLOT_OFF);
    if (mode < ALBW_EXTRA_ITEM_SLOT_OFF || mode > ALBW_EXTRA_ITEM_SLOT_EXTRA_AND_QUICK_SWAP) {
        return ALBW_EXTRA_ITEM_SLOT_OFF;
    }
    return mode;
}

void albw_extra_item_slot_sync_bools_from_mode() {
    const int mode = albw_extra_item_slot_mode();
    if (g_extra_item_slot_enabled != 0) {
        svc_config->set_bool(mod_ctx, g_extra_item_slot_enabled, mode >= ALBW_EXTRA_ITEM_SLOT_EXTRA_ONLY);
    }
    if (g_extra_item_slot_quick_swap != 0) {
        svc_config->set_bool(mod_ctx, g_extra_item_slot_quick_swap,
                             mode >= ALBW_EXTRA_ITEM_SLOT_EXTRA_AND_QUICK_SWAP);
    }
}

void albw_extra_item_slot_config_tick() {
    // Mode select is authoritative. Never re-promote Off from a stale enabled bool —
    // that made "Off" snap back to Extra Only / Quick Swap on the next tick.
    albw_extra_item_slot_sync_bools_from_mode();
}

bool albw_is_extra_item_slot_enabled() {
    return albw_extra_item_slot_mode() >= ALBW_EXTRA_ITEM_SLOT_EXTRA_ONLY;
}

bool albw_is_dpad_quick_swap_enabled() {
    return albw_extra_item_slot_mode() == ALBW_EXTRA_ITEM_SLOT_EXTRA_AND_QUICK_SWAP;
}

bool albw_quick_swap_field_active() {
    return albw_is_dpad_quick_swap_enabled();
}

ModResult albw_extra_item_slot_init(ModError*) {
    return MOD_OK;
}

ModResult albw_extra_item_slot_shutdown(ModError*) {
    return MOD_OK;
}

void albw_extra_item_slot_migrate_legacy_config() {
    bool legacy = false;
    if (g_extra_item_slot_legacy != 0 &&
        svc_config->get_bool(mod_ctx, g_extra_item_slot_legacy, &legacy) == MOD_OK && legacy)
    {
        svc_config->set_bool(mod_ctx, g_extra_item_slot_enabled, true);
        svc_config->set_bool(mod_ctx, g_extra_item_slot_quick_swap, true);
    }

    int64_t mode = ALBW_EXTRA_ITEM_SLOT_OFF;
    if (g_extra_item_slot_mode != 0 &&
        svc_config->get_int(mod_ctx, g_extra_item_slot_mode, &mode) == MOD_OK && mode > 0)
    {
        svc_config->set_bool(mod_ctx, g_extra_item_slot_enabled, true);
        svc_config->set_bool(mod_ctx, g_extra_item_slot_quick_swap, mode >= 2);
    }

    albw_extra_item_slot_sync_bools_from_mode();
}
