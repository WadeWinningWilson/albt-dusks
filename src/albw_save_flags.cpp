// ============================================
// NEW CODE - ALBT: the mod's persistent-flag allocator. See albw_save_flags.h
// for why this exists and what it replaced.
// ============================================

#include "albw_save_flags.h"

#include "d/d_com_inf_game.h"

namespace {

// First event-register byte owned by this allocator. 100-113 are already taken
// by mq_hearts (100-102, 104), focused arts (103), potion (105) and sword_atp
// (106-113); stock uses none of 100-234.
constexpr int kFlagRegBase = 114;

// Stock's own registers begin at 235; running into them would alias real save
// data, which is the entire bug this file exists to prevent.
constexpr int kFirstStockReg = 235;
static_assert(kFlagRegBase + ((ALBW_FLAG_COUNT + 7) / 8) <= kFirstStockReg,
              "albw save flags have grown into stock's register range - pick a new home "
              "rather than widening this bound");

// `index << 8 | mask`, the encoding dSv_event_c::setEventReg/getEventReg use:
// the high byte selects the register and the low byte masks bits within it.
u16 regFor(int flag) {
    return static_cast<u16>((kFlagRegBase + (flag >> 3)) << 8) |
           static_cast<u8>(1u << (flag & 7));
}

}  // namespace

bool albw_save_flag_get(int flag) {
    if (flag < 0 || flag >= ALBW_FLAG_COUNT) {
        return false;
    }
    return dComIfGs_getEventReg(regFor(flag)) != 0;
}

void albw_save_flag_set(int flag, bool on) {
    if (flag < 0 || flag >= ALBW_FLAG_COUNT) {
        return;
    }
    const u16 reg = regFor(flag);
    // setEventReg clears the masked bits then ORs in the value, so the mask
    // itself sets exactly this bit and 0 clears it; neither touches a
    // neighbour sharing the byte.
    dComIfGs_setEventReg(reg, on ? static_cast<u8>(reg) : static_cast<u8>(0));
}

// ============================================
// NEW CODE ENDS HERE
// ============================================
