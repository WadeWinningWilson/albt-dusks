#include "albw_l1_input.h"

#include "m_Do/m_Do_controller_pad.h"

#include <dolphin/pad.h>

// ============================================
// NEW CODE -- ALBT multiplatform
// The host game binary and SDL3 are already in this process; we only need to
// look symbols up by name. Windows does that with GetModuleHandle/GetProcAddress,
// every other Dusklight platform with dlsym(). Same four symbols either way.
// ============================================
#if defined(_WIN32)
#include <Windows.h>
#else
#include <dlfcn.h>
#endif
// ============================================
// NEW CODE ENDS HERE
// ============================================

namespace {

// SDL_GAMEPAD_BUTTON_LEFT_SHOULDER — keep numeric so we don't need SDL headers at link time.
constexpr int kSdlGamepadButtonLeftShoulder = 9;

using PadGetIndexForPortFn = s32 (*)(u32 port);
using PadGetSdlGamepadForIndexFn = void* (*)(u32 index);
using SdlGetGamepadButtonFn = bool (*)(void* gamepad, int button);
using PadGetKeyButtonBindingsFn = PADKeyButtonBinding* (*)(u32 port, u32* buttonCount);

bool s_resolved = false;
PadGetIndexForPortFn s_getIndexForPort = nullptr;
PadGetSdlGamepadForIndexFn s_getSdlGamepad = nullptr;
SdlGetGamepadButtonFn s_getGamepadButton = nullptr;
PadGetKeyButtonBindingsFn s_getKeyBindings = nullptr;

bool s_prevL1[PAD_CHANMAX] = {};

// ============================================
// NEW CODE -- ALBT multiplatform
// host_symbol(): a symbol exported by the running game binary.
// sdl_symbol():  a symbol from SDL3, however this platform ships it -- linked
//                into the host (iOS), a side-by-side shared object (Linux /
//                Android), or a dylib next to the app (macOS).
// A miss stays a miss: every caller already null-checks and degrades to the
// keyboard path, so no fallback is invented here.
// ============================================
void* host_symbol(const char* name) {
#if defined(_WIN32)
    HMODULE game = GetModuleHandleW(nullptr);
    if (game == nullptr) {
        return nullptr;
    }
    return reinterpret_cast<void*>(GetProcAddress(game, name));
#else
    return dlsym(RTLD_DEFAULT, name);
#endif
}

void* sdl_symbol(const char* name) {
#if defined(_WIN32)
    HMODULE sdl = GetModuleHandleW(L"SDL3.dll");
    if (sdl == nullptr) {
        sdl = LoadLibraryW(L"SDL3.dll");
    }
    if (sdl == nullptr) {
        return nullptr;
    }
    return reinterpret_cast<void*>(GetProcAddress(sdl, name));
#else
    // Statically linked into the host, or already pulled in by it.
    if (void* sym = dlsym(RTLD_DEFAULT, name)) {
        return sym;
    }
#if defined(__APPLE__)
    static const char* const kSdlNames[] = {"libSDL3.0.dylib", "libSDL3.dylib"};
#else
    static const char* const kSdlNames[] = {"libSDL3.so.0", "libSDL3.so"};
#endif
    for (const char* soname : kSdlNames) {
        void* handle = dlopen(soname, RTLD_LAZY);
        if (handle == nullptr) {
            continue;
        }
        if (void* sym = dlsym(handle, name)) {
            return sym;
        }
    }
    return nullptr;
#endif
}
// ============================================
// NEW CODE ENDS HERE
// ============================================

void resolve_input_apis() {
    if (s_resolved) {
        return;
    }
    s_resolved = true;

    s_getIndexForPort =
        reinterpret_cast<PadGetIndexForPortFn>(host_symbol("PADGetIndexForPort"));
    s_getSdlGamepad =
        reinterpret_cast<PadGetSdlGamepadForIndexFn>(host_symbol("PADGetSDLGamepadForIndex"));
    s_getKeyBindings =
        reinterpret_cast<PadGetKeyButtonBindingsFn>(host_symbol("PADGetKeyButtonBindings"));
    s_getGamepadButton =
        reinterpret_cast<SdlGetGamepadButtonFn>(sdl_symbol("SDL_GetGamepadButton"));
}

bool keyboard_l_held(u32 port) {
    if (s_getKeyBindings == nullptr) {
        return false;
    }
    u32 count = 0;
    // Non-null means this port is on keyboard bindings (not a gamepad map).
    if (s_getKeyBindings(port, &count) == nullptr) {
        return false;
    }
    return mDoCPd_c::getHoldL(port) != 0;
}

bool sdl_left_shoulder_held(u32 port) {
    if (s_getIndexForPort == nullptr || s_getSdlGamepad == nullptr ||
        s_getGamepadButton == nullptr)
    {
        return false;
    }
    const s32 index = s_getIndexForPort(port);
    if (index < 0) {
        return false;
    }
    void* gamepad = s_getSdlGamepad(static_cast<u32>(index));
    if (gamepad == nullptr) {
        return false;
    }
    return s_getGamepadButton(gamepad, kSdlGamepadButtonLeftShoulder);
}

}  // namespace

bool albw_l1_held(u32 port) {
    if (port >= PAD_CHANMAX) {
        return false;
    }
    resolve_input_apis();

    // Fork Open Item Wheel binds LEFT_SHOULDER. Do NOT use getHoldL on gamepads:
    // aurora emulateTriggers ORs L2 (z-target analog) into PAD_TRIGGER_L.
    if (sdl_left_shoulder_held(port)) {
        return true;
    }
    return keyboard_l_held(port);
}

bool albw_l1_trig(u32 port) {
    if (port >= PAD_CHANMAX) {
        return false;
    }
    const bool down = albw_l1_held(port);
    const bool trig = down && !s_prevL1[port];
    s_prevL1[port] = down;
    return trig;
}
