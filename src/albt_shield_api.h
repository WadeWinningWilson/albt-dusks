// ============================================
// A Link Between Twilight - shield/guard compatibility service
//
// SHARED INTERFACE HEADER. Copy this file verbatim into any mod that wants to
// coexist with ALBT's parry/bash combat. It depends only on the Dusklight mod
// SDK, never on ALBT's internals, so it can be vendored as-is.
//
// WHY THIS EXISTS
// ALBT and Lazy Tweaks both hook the same guard/attack path on daAlink_c -
// procGuardAttackInit, checkGuardActionChange, setBStatus, procGuardSlipInit,
// setShieldGuard, procCutNormalInit. With both installed, whichever mod
// observes or consumes the guard chord first wins, and ALBT's shield bash stops
// firing even with a full charge bar. Neither mod is wrong; they are racing.
//
// This service makes the arrangement explicit instead of order-dependent:
// ALBT publishes what it is doing with the guard input, and a cooperating mod
// can either stand down for that frame or claim the input itself.
//
// HOW TO USE IT (consumer side, e.g. Lazy Tweaks)
//
//     #include "albt_shield_api.h"
//     IMPORT_OPTIONAL_SERVICE(AlbtShieldService, svc_albt_shield);
//
//     // before consuming guard / B input:
//     if (svc_albt_shield != nullptr &&
//         svc_albt_shield->owns_guard_input(mod_ctx)) {
//         return;  // ALBT is mid-chord this frame; leave the input alone
//     }
//
//     // or, when you are taking the input yourself for a few frames:
//     if (svc_albt_shield != nullptr) {
//         svc_albt_shield->request_input_yield(mod_ctx, 4);
//     }
//
// IMPORT_OPTIONAL_SERVICE means there is no hard dependency: with ALBT absent
// the pointer is simply NULL and the consumer behaves exactly as it does today.
//
// VERSIONING
// Within major 1 this struct is append-only: fields are never reordered,
// removed or repurposed, and a consumer built against minor N keeps working
// against any provider with minor >= N. Anything else requires a major bump.
// ============================================

#pragma once

#include "mods/api.h"

#define ALBT_SHIELD_SERVICE_ID "dev.albt.albw.shield"
#define ALBT_SHIELD_SERVICE_MAJOR 1u
#define ALBT_SHIELD_SERVICE_MINOR 0u

typedef struct AlbtShieldService {
    ServiceHeader header;

    // --- minor 0 ---

    /* True when ALBT's parry/bash combat is switched on at all. When false,
     * ALBT is not touching guard behaviour and a consumer need not coordinate. */
    bool (*is_parry_combat_enabled)(ModContext* ctx);

    /* True while ALBT currently owns the guard chord: a shield is equipped and
     * ALBT's manual-guard input is held. A cooperating mod should not consume
     * guard or B input on a frame where this reads true - that is the frame
     * ALBT's shield bash is evaluated on, and consuming it is what silently
     * disables the bash. */
    bool (*owns_guard_input)(ModContext* ctx);

    /* True when the charge economy would permit a bash right now (a shield
     * tier with charges, and enough of them banked). Useful if a consumer wants
     * to yield only when a bash could actually happen, rather than on every
     * guard frame. */
    bool (*bash_available)(ModContext* ctx);

    /* Current / maximum bash charges for the equipped shield. 0/0 when parry
     * combat is off or no shield is equipped. Exposed so a consumer can render
     * or reason about the bar without reaching into ALBT. */
    unsigned char (*get_bash_charges)(ModContext* ctx);
    unsigned char (*get_max_bash_charges)(ModContext* ctx);

    /* Tell ALBT that the caller is taking guard/B input for the next `frames`
     * frames; ALBT will not begin a bash during that window. Use this when your
     * mod owns the chord, so the two sides never both act on one press. Passing
     * 0 clears an outstanding yield. Frames are game frames; a small number
     * (2-6) is normally right. */
    void (*request_input_yield)(ModContext* ctx, unsigned char frames);
} AlbtShieldService;

#ifdef __cplusplus
#include "mods/service.hpp"
template <>
struct mods::ServiceTraits<AlbtShieldService> {
    static constexpr const char* id = ALBT_SHIELD_SERVICE_ID;
    static constexpr uint16_t major_version = ALBT_SHIELD_SERVICE_MAJOR;
    static constexpr uint16_t minor_version = ALBT_SHIELD_SERVICE_MINOR;
};
#endif
