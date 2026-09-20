#pragma once

// ============================================
// NEW CODE - ALBT multiplatform (fork-added game-code compat)
//
// The ported outfit cluster calls six things the FORK added to shared game code
// (d_item_data.h, d_meter2.cpp, d_a_e_nz.cpp, d_a_alink.h). Verified against
// stock with `git grep` on origin/main: all six return 0 files there.
//
// Each one below is backed by something REAL in stock - a repurposed enum slot,
// an existing file-static, or an equivalent the mod already implements. None of
// them is a stand-in that quietly reports a default. Where a value could not be
// obtained honestly it would be logged loudly, not guessed.
// ============================================

#include "global.h"
#include <os.h>
#include "albw_common.h"
#include "f_pc/f_pc_base.h"  // fpc_ProcID (items 15-18)

// ---- 1/2. Item ids the fork added over stock's unused NOENTRY slots ----------
// fork include/d/d_item_data.h:156-157. Stock has these exact slots as
// dItemNo_NOENTRY_55_e (0x37) and dItemNo_NOENTRY_56_e (0x38) - unused entries,
// so the fork's reuse does not collide with any real stock item.
// DEITY_ARMOR is an ownership/ability flag, not a physical inventory item.
constexpr int dItemNo_WALLET_LV4_e  = 0x37;
constexpr int dItemNo_DEITY_ARMOR_e = 0x38;

// ---- 3. Ghost-rat cling state ------------------------------------------------
// fork d_a_e_nz.cpp:148 - `return data_8072C454[0] != 0;` over the actor's own
// stick-slot bitfield. That static (`static u8 data_8072C454[4]`) IS present in
// stock d_a_e_nz.cpp:136, so this reads the game's real state, resolved by name
// through the symbol manifest (which covers non-exported statics).
bool dE_NZ_isRatStuckOnPlayer();

// ---- 4. ALBW meter movement-exhaustion --------------------------------------
// fork d_meter2.cpp:442 - `return sALBWMovementExhausted;`. The mod's own meter
// tracks the identical flag as albw_meter_impl::g_exhausted (meter.cpp:87,
// set at :219, cleared at :225), so this forwards to the live mod meter.
bool dMeter2_isALBWMovementExhausted();

// ---- 5. Rental ownership -----------------------------------------------------
// fork d_meter2.cpp:902. The mod already ports this logic as
// playerOwnsRentalItem() in rental_eligibility.cpp; this is the fork-named entry
// point onto that same implementation.
bool dMeter2_playerOwnsRentalItem(u8 itemNo);

// ---- 6. Metamorphose-proc predicate -----------------------------------------
// fork d_a_alink.h:3442 is an INLINE member added to daAlink_c:
//     mProcID == PROC_METAMORPHOSE || mProcID == PROC_METAMORPHOSE_ONLY
// A mod cannot add a member to the class, but the body touches only stock state
// (mProcID at d_a_alink.h:4213 and both PROC_ ids exist in stock), so it is
// re-expressed here as a free function over the same members. The single call
// site in outfit.cpp is annotated where it changes from method to function.
class daAlink_c;
bool albw_checkMetamorphoseProcActive(const daAlink_c* link);

// ---- 7-13. Fork meter/shield surface used by the wardrobe module -------------
// All fork-added (stock: 0 files each). Every one forwards to an equivalent the
// mod already implements - the shield trio to quick_swap.cpp's existing ports,
// the armor/recovery trio to the live mod meter - so none of them is a guess.
bool dMeter2_isShieldItem(u8 itemNo);              // fork d_meter2.cpp:1022
bool dMeter2_shieldIsOwned(u8 itemNo);             // fork d_meter2.cpp:1054
bool dMeter2_equipOwnedShield(u8 itemNo);          // fork d_meter2.cpp
void dMeter2_applyEquippedShield(u8 itemNo);       // fork d_meter2.cpp
bool dMeter2_isALBWArmorDepleted();                // fork d_meter2.cpp:696
int  dMeter2_getALBWNormalRecoveryRate();          // fork d_meter2.cpp:384
int  dMeter2_getALBWLockoutRecoveryRate();         // fork d_meter2.cpp:388

// ---- 15-18. ALBW Magic Armor economy (exposure batch) ------------------------
// Fork-added armor-economy surface (fork d_meter2.cpp; stock: 0 files each).
// All four forward to the live mod meter (meter.cpp, albw_meter_impl - external
// linkage, NOT anon-namespace), the same implementation whose depleted flag
// dMeter2_isALBWArmorDepleted above already reads.
void dMeter2_onALBWArmorHit();                     // fork d_meter2.cpp:690
bool dMeter2_canALBWArmorBlock();                  // fork d_meter2.cpp:697
void dMeter2_onArmorEncounterHit(fpc_ProcID actorID, bool dealtHPDamage);  // fork d_meter2.cpp:708
void dMeter2_onArmorAttackHit(fpc_ProcID actorID);                         // fork d_meter2.cpp:726

// ---- 14. stricmp ------------------------------------------------------------
// The fork's sumo module includes dusk/extras.h, which for non-MSVC targets
// DECLARES stricmp/strnicmp (fork include/dusk/extras.h:8-11) and relies on the
// MSL extras implementation inside the game binary. A mod cannot link against
// that, and only MSVC's CRT ships stricmp - which is why the six non-Windows CI
// jobs failed on it while both Windows jobs passed.
//
// POSIX strcasecmp has stricmp's exact semantics, so this is a rename, not a
// reimplementation. Guarded exactly the way the fork guards its declaration.
#ifndef _MSC_VER
int stricmp(const char* str1, const char* str2);
int strnicmp(const char* str1, const char* str2, int n);
#endif

// ============================================
// NEW CODE ENDS HERE
// ============================================
