// ============================================
// NEW CODE - ALBW Boss Refinement (Diababa, Pass 2)
// Fork sources: d_a_b_bq.cpp (35 sites), d_a_b_bh.cpp (8, all fork-authored).
//
// Method: docs/state/boss-refinement-mod-port-method.md in the fork repo.
// The refinement BRAIN is already ported - boss_refinement.h/.cpp carry every
// dAlbwBoss_diababa* helper. This file is wiring only.
//
// Reachability (measured 2026-08-31):
//   unique names, hookable by DEFINE_HOOK_SYMBOL:
//     daB_BQ_Create, daB_BQ_Execute, b_bq_damage, b_bq_wait,
//     b_bq_attack, b_bq_runaway_test, b_bq_lunge, daB_BH_Execute
//   duplicate names, NOT resolvable (53 and 94 actors define one):
//     damage_check (8 sites), action (4 sites)
//
// Hook installs here log and CONTINUE rather than failing mod_initialize. A
// single unresolved boss hook must not take the whole collective down the way
// SetItemMagicCount did on Linux; the miss is loud in the log, not silent.
// ============================================

#pragma once

#include "albw_common.h"

ModResult albw_diababa_init(ModError* error);
ModResult albw_diababa_shutdown(ModError* error);
// ============================================
// NEW CODE ENDS HERE
// ============================================
