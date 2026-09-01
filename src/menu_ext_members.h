#pragma once

// ============================================
// NEW CODE - ALBT multiplatform (side storage for fork-added menu members)
//
// The fork adds member variables to two STOCK classes:
//
//   dMenu_Ring_c   mQuickEquipMode, mUseQuickEquipPages, mQuickEquipForceClose,
//                  mQuickEquipPage, mQuickEquipSlotMap[24], mBagViewOpen, mBagViewId
//   dMw_c          mpMenuExtStatus
//
// A mod cannot add fields to a stock class, so the fields live here instead,
// keyed by the owning instance. The ported function bodies are otherwise
// unchanged: `mQuickEquipPage` becomes `QE(this).page` and nothing else moves,
// so the logic stays diffable against the fork line by line.
//
// Keyed rather than a single static because the menu objects are created and
// destroyed per open; keying means a stale entry from a previous instance can
// never be read as if it belonged to the current one.
// ============================================

#include "global.h"
#include <os.h>

class dMenu_Ring_c;
class dMw_c;
class dMenu_ExtStatus_c;

struct AlbwRingQuickEquip {
    bool mode;
    bool usePages;
    bool forceClose;
    u8   page;
    u8   slotMap[24];       // MAX_ITEM_SLOTS (d_save.h:18)
    bool bagViewOpen;
    u16  bagViewId;
};

/** Storage for `ring`, zero-initialised on first use. Never returns null. */
AlbwRingQuickEquip& albw_ring_qe(const dMenu_Ring_c* ring);
/** Forget `ring`'s slot (call from its _delete) so the entry can be reused. */
void albw_ring_qe_release(const dMenu_Ring_c* ring);

/** dMw_c::mpMenuExtStatus. */
dMenu_ExtStatus_c*  albw_mw_ext_status(const dMw_c* mw);
void albw_mw_set_ext_status(const dMw_c* mw, dMenu_ExtStatus_c* page);
void albw_mw_release(const dMw_c* mw);
