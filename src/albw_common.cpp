#include "albw_common.h"

bool albw_cfg_bool(ConfigVarHandle var, bool fallback) {
    bool value = fallback;
    if (var != 0 && svc_config->get_bool(mod_ctx, var, &value) == MOD_OK) {
        return value;
    }
    return fallback;
}

int albw_cfg_int(ConfigVarHandle var, int fallback) {
    int64_t value = fallback;
    if (var != 0 && svc_config->get_int(mod_ctx, var, &value) == MOD_OK) {
        return static_cast<int>(value);
    }
    return fallback;
}

bool albw_coming_soon_disabled(ModContext*, void*) {
    return true;
}

ModResult albw_ui_add_toggle(UiElementHandle panel, const char* label, const char* help,
                             ConfigVarHandle var, UiPredicateFn is_disabled, void* user_data) {
    UiControlDesc control = UI_CONTROL_DESC_INIT;
    control.kind = UI_CONTROL_TOGGLE;
    control.label = label;
    control.help_rml = help;
    control.binding = UI_BINDING_CONFIG_VAR;
    control.config_var = var;
    control.is_disabled = is_disabled;
    control.user_data = user_data;
    return svc_ui->pane_add_control(mod_ctx, panel, &control, nullptr);
}

ModResult albw_ui_add_number(UiElementHandle panel, const char* label, const char* help,
                             ConfigVarHandle var, int min, int max) {
    UiControlDesc control = UI_CONTROL_DESC_INIT;
    control.kind = UI_CONTROL_NUMBER;
    control.label = label;
    control.help_rml = help;
    control.binding = UI_BINDING_CONFIG_VAR;
    control.config_var = var;
    control.min = min;
    control.max = max;
    control.step = 1;
    control.suffix = "x";
    return svc_ui->pane_add_control(mod_ctx, panel, &control, nullptr);
}

ModResult albw_ui_add_select(UiElementHandle panel, const char* label, const char* help,
                             ConfigVarHandle var, const char* const* options, size_t option_count) {
    UiControlDesc control = UI_CONTROL_DESC_INIT;
    control.kind = UI_CONTROL_SELECT;
    control.label = label;
    control.help_rml = help;
    control.binding = UI_BINDING_CONFIG_VAR;
    control.config_var = var;
    control.options = options;
    control.option_count = option_count;
    const ModResult r = svc_ui->pane_add_control(mod_ctx, panel, &control, nullptr);
    if (r == MOD_UNSUPPORTED) {
        control.kind = UI_CONTROL_NUMBER;
        control.min = 0;
        control.max = static_cast<int64_t>(option_count > 0 ? option_count - 1 : 0);
        control.step = 1;
        control.options = nullptr;
        control.option_count = 0;
        return svc_ui->pane_add_control(mod_ctx, panel, &control, nullptr);
    }
    return r;
}

struct IntChoiceUserData {
    ConfigVarHandle var;
    int index;
};

namespace {

void on_int_choice_pressed(ModContext*, void* user_data) {
    auto* choice = static_cast<IntChoiceUserData*>(user_data);
    if (choice != nullptr && choice->var != 0) {
        svc_config->set_int(mod_ctx, choice->var, choice->index);
    }
}

bool int_choice_is_selected(ModContext*, void* user_data) {
    auto* choice = static_cast<IntChoiceUserData*>(user_data);
    if (choice == nullptr || choice->var == 0) {
        return false;
    }
    return albw_cfg_int(choice->var, 0) == choice->index;
}

}  // namespace

static IntChoiceUserData s_choices[32];
static size_t s_choice_count = 0;

void albw_ui_reset_int_choice_storage() {
    s_choice_count = 0;
}

ModResult albw_ui_add_int_choice_group(UiElementHandle panel, const char* section, const char* help,
                                       ConfigVarHandle var, const char* const* labels,
                                       size_t label_count) {
    if (section != nullptr && svc_ui->pane_add_section(mod_ctx, panel, section) != MOD_OK) {
        return MOD_ERROR;
    }

    for (size_t i = 0; i < label_count; ++i) {
        if (s_choice_count >= sizeof(s_choices) / sizeof(s_choices[0])) {
            return MOD_ERROR;
        }
        s_choices[s_choice_count] = {var, static_cast<int>(i)};

        UiControlDesc control = UI_CONTROL_DESC_INIT;
        control.kind = UI_CONTROL_BUTTON;
        control.label = labels[i];
        control.help_rml = help;
        control.on_pressed = on_int_choice_pressed;
        control.is_selected = int_choice_is_selected;
        control.user_data = &s_choices[s_choice_count];
        if (svc_ui->pane_add_control(mod_ctx, panel, &control, nullptr) != MOD_OK) {
            return MOD_ERROR;
        }
        ++s_choice_count;
    }
    return MOD_OK;
}

ModResult albw_ui_add_coming_soon(UiElementHandle panel, const char* label, const char* help,
                                  ConfigVarHandle var) {
    UiControlDesc control = UI_CONTROL_DESC_INIT;
    control.kind = UI_CONTROL_TOGGLE;
    control.label = label;
    control.help_rml = help;
    control.binding = UI_BINDING_CONFIG_VAR;
    control.config_var = var;
    control.is_disabled = albw_coming_soon_disabled;
    return svc_ui->pane_add_control(mod_ctx, panel, &control, nullptr);
}

ModResult albw_register_bool(const char* name, bool default_value, ConfigVarHandle* out) {
    ConfigVarDesc desc = CONFIG_VAR_DESC_INIT;
    desc.name = name;
    desc.type = CONFIG_VAR_BOOL;
    desc.default_bool = default_value;
    return svc_config->register_var(mod_ctx, &desc, out);
}

ModResult albw_register_int(const char* name, int default_value, ConfigVarHandle* out) {
    ConfigVarDesc desc = CONFIG_VAR_DESC_INIT;
    desc.name = name;
    desc.type = CONFIG_VAR_INT;
    desc.default_int = default_value;
    return svc_config->register_var(mod_ctx, &desc, out);
}

ModResult albw_register_gameplay_bool(const char* section, const char* label, const char* help,
                                      ConfigVarHandle var) {
#if CONFIG_SERVICE_MINOR >= 1
    if (svc_config == nullptr ||
        !SERVICE_HAS(svc_config, ConfigService, register_gameplay_ui) ||
        svc_config->register_gameplay_ui == nullptr)
    {
        return MOD_UNSUPPORTED;
    }
    ConfigGameplayUiDesc desc = CONFIG_GAMEPLAY_UI_DESC_INIT;
    desc.var = var;
    desc.section = section;
    desc.label = label;
    desc.help_text = help;
    return svc_config->register_gameplay_ui(mod_ctx, &desc);
#else
    (void)section;
    (void)label;
    (void)help;
    (void)var;
    return MOD_UNSUPPORTED;
#endif
}
