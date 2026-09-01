// ============================================
// NEW CODE - ALBT multiplatform (side storage for fork-added menu members)
// See menu_ext_members.h for why this exists.
// ============================================

#include "menu_ext_members.h"
#include "albw_common.h"

#include <cstring>

#if TARGET_PC

namespace {

// Only one ring and one menu window are alive at a time in practice; the extra
// slots exist so a create-before-delete overlap cannot alias two instances.
constexpr int kSlots = 4;

template <typename T, typename V>
struct Table {
    const T* keys[kSlots];
    V values[kSlots];

    V& get(const T* key) {
        for (int i = 0; i < kSlots; ++i) {
            if (keys[i] == key) return values[i];
        }
        for (int i = 0; i < kSlots; ++i) {
            if (keys[i] == nullptr) {
                keys[i] = key;
                std::memset(&values[i], 0, sizeof(values[i]));
                return values[i];
            }
        }
        // Table full: recycle slot 0 and say so - silently aliasing two menus
        // would show one instance another's page.
        if (svc_log != nullptr) {
            svc_log->warn(mod_ctx, "albw: menu member table full; recycling oldest slot");
        }
        keys[0] = key;
        std::memset(&values[0], 0, sizeof(values[0]));
        return values[0];
    }

    void release(const T* key) {
        for (int i = 0; i < kSlots; ++i) {
            if (keys[i] == key) {
                keys[i] = nullptr;
                std::memset(&values[i], 0, sizeof(values[i]));
                return;
            }
        }
    }
};

Table<dMenu_Ring_c, AlbwRingQuickEquip> s_ring = {};
Table<dMw_c, dMenu_ExtStatus_c*> s_mw = {};

}  // namespace

AlbwRingQuickEquip& albw_ring_qe(const dMenu_Ring_c* ring) { return s_ring.get(ring); }
void albw_ring_qe_release(const dMenu_Ring_c* ring) { s_ring.release(ring); }

dMenu_ExtStatus_c* albw_mw_ext_status(const dMw_c* mw) { return s_mw.get(mw); }
void albw_mw_set_ext_status(const dMw_c* mw, dMenu_ExtStatus_c* page) { s_mw.get(mw) = page; }
void albw_mw_release(const dMw_c* mw) { s_mw.release(mw); }

#endif  // TARGET_PC

// ============================================
// NEW CODE ENDS HERE
// ============================================
