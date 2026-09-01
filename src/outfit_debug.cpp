// Ported from the fork's src/d/d_albw_outfit_debug.cpp.
// Temporary outfit quick-swap trace - remove when cycle/warp is stable.
//
// Only translation: the fork resolves the log directory via the host's
// dusk::GetLogFilePath(), which is not in the mod SDK. The fork's OWN fallback
// branch (USERPROFILE/Documents/dusklight) is used instead - no new behaviour.
#include "outfit_debug.h"

#if TARGET_PC && D_ALBW_OUTFIT_SWAP_DEBUG

#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <filesystem>

namespace {

FILE* debugFile() {
    static FILE* sFile = nullptr;
    if (sFile != nullptr) {
        return sFile;
    }

    std::filesystem::path path = "outfit_swap_debug.txt";
    if (const char* user = std::getenv("USERPROFILE")) {
        path = std::filesystem::path(user) / "Documents" / "dusklight" / "outfit_swap_debug.txt";
    } else if (const char* home = std::getenv("HOME")) {
        path = std::filesystem::path(home) / "dusklight" / "outfit_swap_debug.txt";
    }

    sFile = fopen(path.string().c_str(), "a");
    return sFile;
}

}  // namespace

void dAlbwOutfit_debugLog(const char* fmt, ...) {
    FILE* file = debugFile();
    if (file == nullptr) {
        return;
    }

    va_list args;
    va_start(args, fmt);
    vfprintf(file, fmt, args);
    va_end(args);
    fputc('\n', file);
    fflush(file);
}

#endif
