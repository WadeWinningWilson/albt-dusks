#pragma once

// ============================================
// NEW CODE - ALBT multiplatform (DuskLog compat shim)
// The ported outfit cluster logs through the host's fmt-style DuskLog
// ("...{}...", args). That logger is host-internal and not in the mod SDK, so
// this maps the same call shape onto svc_log. Keeping the shim here lets the
// ported .cpp files keep their DuskLog lines byte-identical to the fork.
// Only the subset the cluster uses is provided: .debug / .info / .warn.
// ============================================

#include "albw_common.h"

#include <cstdio>
#include <cstring>
#include <string>

namespace albw_log_detail {

inline void append(std::string& out, const char* v) { out += (v != nullptr ? v : "(null)"); }
inline void append(std::string& out, char* v) { out += (v != nullptr ? v : "(null)"); }
inline void append(std::string& out, const std::string& v) { out += v; }
inline void append(std::string& out, bool v) { out += (v ? "true" : "false"); }
inline void append(std::string& out, const void* v) {
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%p", v);
    out += buf;
}
template <typename T>
inline void append(std::string& out, T v) {
    char buf[64];
    if (static_cast<double>(static_cast<long long>(v)) == static_cast<double>(v)) {
        std::snprintf(buf, sizeof(buf), "%lld", static_cast<long long>(v));
    } else {
        std::snprintf(buf, sizeof(buf), "%g", static_cast<double>(v));
    }
    out += buf;
}

// Substitute one "{}" placeholder per argument, in order. Any format spec inside
// the braces is ignored (the cluster only uses plain "{}").
inline void format_into(std::string& out, const char*& fmt) {
    out += fmt;
    fmt += std::strlen(fmt);
}
template <typename T, typename... Rest>
inline void format_into(std::string& out, const char*& fmt, T&& first, Rest&&... rest) {
    while (*fmt != '\0') {
        if (fmt[0] == '{') {
            const char* close = std::strchr(fmt, '}');
            if (close != nullptr) {
                fmt = close + 1;
                append(out, first);
                format_into(out, fmt, static_cast<Rest&&>(rest)...);
                return;
            }
        }
        out += *fmt++;
    }
}

template <typename... Args>
inline std::string format(const char* fmt, Args&&... args) {
    std::string out;
    const char* p = fmt;
    format_into(out, p, static_cast<Args&&>(args)...);
    return out;
}

}  // namespace albw_log_detail

struct AlbwDuskLog {
    template <typename... Args>
    void debug(const char* fmt, Args&&... args) const {
        if (svc_log != nullptr)
            svc_log->info(mod_ctx,
                          albw_log_detail::format(fmt, static_cast<Args&&>(args)...).c_str());
    }
    template <typename... Args>
    void info(const char* fmt, Args&&... args) const {
        if (svc_log != nullptr)
            svc_log->info(mod_ctx,
                          albw_log_detail::format(fmt, static_cast<Args&&>(args)...).c_str());
    }
    template <typename... Args>
    void warn(const char* fmt, Args&&... args) const {
        if (svc_log != nullptr)
            svc_log->warn(mod_ctx,
                          albw_log_detail::format(fmt, static_cast<Args&&>(args)...).c_str());
    }
    // The fork reaches for OSReport on the paths that must never pass quietly
    // (a resource that did not resolve). error() is the loud channel here.
    template <typename... Args>
    void error(const char* fmt, Args&&... args) const {
        if (svc_log != nullptr)
            svc_log->error(mod_ctx,
                           albw_log_detail::format(fmt, static_cast<Args&&>(args)...).c_str());
    }
};

static const AlbwDuskLog DuskLog;

// ============================================
// NEW CODE ENDS HERE
// ============================================
