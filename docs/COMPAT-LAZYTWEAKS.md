# Proposal: `dev.albt.albw.shield` — guard/bash compatibility service

**To:** Kamilink (Lazy Tweaks)
**From:** WadeWinningWilson (A Link Between Twilight)
**Status:** implemented and published on the ALBT side; nothing required of you yet

---

## The problem

With both mods installed, **ALBT's shield bash stops firing** — even with a full
charge bar, the right shield equipped, and every one of its own gates passing.
Disabling Lazy Tweaks restores it.

Neither mod is doing anything wrong. We hook the same functions on `daAlink_c`:

| symbol | ALBT uses it for | Lazy Tweaks also hooks it |
|---|---|---|
| `procGuardAttackInit` | the bash proc itself | yes |
| `checkGuardActionChange` | a predicate in ALBT's bash condition chain | yes |
| `setBStatus` | — | yes |
| `procGuardSlipInit` | shield durability on a blocked hit | yes |
| `setShieldGuard` | guard state | yes |
| `procCutNormalInit` | sword-vs-shield arbitration | yes |

ALBT's bash entry lives inside `daAlink_c::checkItemAction` and requires the
guard chord (guard held **+** B) to be observable on one frame. Whoever reaches
the input first decides, and the result depends on hook registration order —
which is not something either of us controls or should rely on.

Instrumenting ALBT's own entry showed every predicate true except the B trigger:

```
btn=1  B=0  guardChg=1  shield=1  ground=1  parry=1  canSpend=1  chg=4/6
```

Charges fine, guard fine, bash never starts.

## The proposal

ALBT now publishes a small optional service. It is **query-first**: ALBT tells
you what it is doing with the guard input, and you decide. There is also one
call in the other direction so you can claim the input when it is yours.

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

The full header is `src/albt_shield_api.h` in the ALBT repo. It depends only on
the Dusklight mod SDK — copy it in as-is, no ALBT source required.

### Integration

```cpp
#include "albt_shield_api.h"
IMPORT_OPTIONAL_SERVICE(AlbtShieldService, svc_albt_shield);
```

`IMPORT_OPTIONAL_SERVICE` means **no hard dependency**: with ALBT absent the
pointer is `NULL` and Lazy Tweaks behaves exactly as it does today.

Before consuming guard or B input:

```cpp
if (svc_albt_shield != nullptr && svc_albt_shield->owns_guard_input(mod_ctx)) {
    return;  // ALBT is mid-chord this frame
}
```

If you would rather yield only when a bash could actually land, use
`bash_available()` instead — it is true only when the charge economy would
permit one, so you keep the input on every other guard frame.

And when Lazy Tweaks is taking the chord itself:

```cpp
svc_albt_shield->request_input_yield(mod_ctx, 4);  // frames
```

ALBT will not begin a bash during that window. `0` clears it.

### What ALBT already does unilaterally

`request_input_yield` is honoured at ALBT's bash entry today, and the whole
service is inert unless someone calls it — so shipping this changes nothing for
users who do not have Lazy Tweaks, and nothing for you until you opt in.

### Versioning

Major 1 is append-only: fields are never reordered, removed or repurposed, and
a consumer built against minor *N* keeps working against any provider with minor
≥ *N*. Anything else gets a major bump. Per the SDK's rules the loader resolves
all exports before any imports, so load order between our mods does not matter.

## Happy to change the shape

If the yield direction is more useful than the query direction for how Lazy
Tweaks is structured — or if you would rather publish a service and have ALBT
consume it — say so and I will adapt. The goal is that the two mods stop racing,
not that this particular interface wins.
