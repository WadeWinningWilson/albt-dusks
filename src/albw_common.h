#pragma once

#include "mods/svc/config.h"
#include "mods/svc/hook.h"
#include "mods/svc/host.h"
#include "mods/svc/log.h"
#include "mods/svc/ui.h"

// ============================================
// Debug file-dump gate. The *_debugLog / dump* helpers write albw_*_debug.txt
// into the player's Documents/dusklight/ folder — diagnostic scaffolding only.
// Ships OFF: 0 = no writes and no path strings in the binary. Flip to 1 in a
// local build to re-enable a trace. Keep 0 for any release/CI build.
// ============================================
#ifndef ALBW_DEBUG_DUMPS
#define ALBW_DEBUG_DUMPS 0
#endif

extern ModContext* mod_ctx;
extern const LogService* svc_log;
extern const HookService* svc_hook;
extern const ConfigService* svc_config;
extern const UiService* svc_ui;
// Used to publish the quick-equip socket registry so other mods can claim
// slots (host.h:55-71 publish_service / get_service).
extern const HostService* svc_host;

bool albw_cfg_bool(ConfigVarHandle var, bool fallback);
int albw_cfg_int(ConfigVarHandle var, int fallback);

bool albw_coming_soon_disabled(ModContext* ctx, void* user_data);

ModResult albw_ui_add_toggle(UiElementHandle panel, const char* label, const char* help,
                             ConfigVarHandle var, UiPredicateFn is_disabled = nullptr,
                             void* user_data = nullptr);
ModResult albw_ui_add_number(UiElementHandle panel, const char* label, const char* help,
                             ConfigVarHandle var, int min, int max);
ModResult albw_ui_add_select(UiElementHandle panel, const char* label, const char* help,
                             ConfigVarHandle var, const char* const* options, size_t option_count);
/** Labeled option buttons (Mods panel has no SELECT help pane — use this instead of 0/1/2 steppers). */
void albw_ui_reset_int_choice_storage();
ModResult albw_ui_add_int_choice_group(UiElementHandle panel, const char* section, const char* help,
                                       ConfigVarHandle var, const char* const* labels,
                                       size_t label_count);
ModResult albw_ui_add_coming_soon(UiElementHandle panel, const char* label, const char* help,
                                  ConfigVarHandle var);

ModResult albw_register_bool(const char* name, bool default_value, ConfigVarHandle* out);
ModResult albw_register_int(const char* name, int default_value, ConfigVarHandle* out);

ModResult albw_register_gameplay_bool(const char* section, const char* label, const char* help,
                                      ConfigVarHandle var);
