# dev.albt.albw.shield

> Status board / index: [CURRENT-STATE.md](CURRENT-STATE.md)

Hey Kamilink — with both our mods installed, ALBT's shield bash never fires,
because we both hook the same `daAlink_c` guard path (`procGuardAttackInit`,
`checkGuardActionChange`, `setBStatus`, `procGuardSlipInit`, `setShieldGuard`,
`procCutNormalInit`) and whoever reaches the guard chord first wins, which comes
down to hook registration order. I've published an optional service on my side
so we can make the handoff explicit instead of leaving it to load order — the
header is `src/albt_shield_api.h` in my repo, it only depends on the Dusklight
SDK, and you can copy it in as-is. Totally happy to reshape it if a different
split works better for how Lazy Tweaks is built.

```cpp
#include "albt_shield_api.h"
IMPORT_OPTIONAL_SERVICE(AlbtShieldService, svc_albt_shield);

// before consuming guard / B input:
if (svc_albt_shield != nullptr && svc_albt_shield->owns_guard_input(mod_ctx)) {
    return;  // ALBT is mid-chord this frame
}

// or, when Lazy Tweaks is taking the chord itself:
svc_albt_shield->request_input_yield(mod_ctx, 4);  // frames
```

```c
#define ALBT_SHIELD_SERVICE_ID "dev.albt.albw.shield"

typedef struct AlbtShieldService {
    ServiceHeader header;
    bool (*is_parry_combat_enabled)(ModContext* ctx);
    bool (*owns_guard_input)(ModContext* ctx);
    bool (*bash_available)(ModContext* ctx);
    unsigned char (*get_bash_charges)(ModContext* ctx);
    unsigned char (*get_max_bash_charges)(ModContext* ctx);
    void (*request_input_yield)(ModContext* ctx, unsigned char frames);
} AlbtShieldService;
```

`IMPORT_OPTIONAL_SERVICE` means no hard dependency — with ALBT absent the
pointer is `NULL` and Lazy Tweaks behaves exactly as it does now. Major 1 is
append-only, and the loader resolves all exports before any imports, so load
order between us doesn't matter.
