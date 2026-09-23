#pragma once

// ============================================
// ALBW_DARKNUT_PROBE - the ONE Darknut parry/bash probe switch.
//
// WHY THIS EXISTS. The fork instruments the whole guard-break system through
// daB_TN_c::albwDebugLogEvent (fork src/d/actor/d_a_b_tn.cpp:1295-1335), and
// that body does an UNCONDITIONAL fopen + fprintf + fclose to
//     %USERPROFILE%/Documents/dusklight/albw_darknut_debug.txt
// on every logged event. Two problems, both disqualifying for this port:
//
//   1. HOT PATH. The call sites include damage_check and the per-frame
//      executeAttackH "stop blocked" branch, so it is a file open per frame
//      during the fight, not per event.
//   2. USER-PATH LEAK. The literal %USERPROFILE% expansion is exactly what the
//      release privacy scan (docs/RELEASE-PROCEDURE.md) exists to catch, and it
//      would ship inside the binary as a string.
//
// So albwDebugLogEvent is the ONE fork body this port does not reproduce: it is
// an ADAPTER on AlbwBtn_c (btn_parry.cpp) that writes through DuskLog behind
// this switch, never to a file. Every fork call site stays byte-verbatim
// because the adapter keeps the donor's name and signature.
//
// DEFAULT 0. Unlike ALBW_FLURRY_PROBE this ships off, because the surface it
// instruments is a boss fight the player can be in for minutes. Set to 1 for a
// bring-up session (steps 2-6 of the staged plan), then back to 0; at 0 the
// adapter body compiles out entirely and the TU carries no probe state.
// ============================================
#define ALBW_DARKNUT_PROBE 0
