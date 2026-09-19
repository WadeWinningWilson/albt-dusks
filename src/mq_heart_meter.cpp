// ============================================
// NEW CODE - ALBW Port (MQ bought hearts grow the vanilla heart meter)
// The fork's dMeter2_c::moveLife routes the life-meter max + heal cap through
// getMaxLifeGauge / getDisplayMaxLifeInternal when Master Quest is on, so bought
// hearts (which live in the gauge as quarter-hearts) add heart containers AND are
// fillable. Stock moveLife uses (getMaxLife/5)*4 and ignores the bonus, so the
// containers never grow. We can't redefine the exported dMeter2_c::moveLife, so
// the port_tool-extracted fork body lives on a layout-compatible subclass
// AlbwMeter2_c; the stock method is hooked and, when MQ is on, dispatched to the
// subclass copy (qualified call = static dispatch). Same technique as the swim
// port. Sibling calls (alphaAnimeLife, mpMeterDraw->drawLife) resolve to the
// exported stock methods via inheritance.
// ============================================
#include "albw_common.h"
#include "config_vars.h"
#include "mq_hearts.h"      // albw_mq_is_enabled, albw_mq_display_max_internal
#include "parry_master.h"   // dParryMaster_onHeal / dParryMaster_clearQueue

#include "d/d_com_inf_game.h"
#include "mods/svc/hook.hpp"

#include <algorithm>

#define private public
#include "d/d_meter2.h"       // dMeter2_c (+ members: mMaxLife, mNowLifeGauge, mpMeterDraw, ...)
#include "d/d_meter2_draw.h"  // dMeter2Draw_c::drawLife
#include "d/d_meter2_info.h"  // dMeter2Info_onLifeGaugeSE / getLifeGaugeSE / offLifeGaugeSE
#include "d/d_meter_HIO.h"    // g_drawHIO
#undef private

// Fork dAlbwMQ_getDisplayMaxLifeInternal (d_albw_master_quest.cpp:95): round the
// bonus-inclusive gauge up to whole containers * 5, so mMaxLife (raw-unit) tracks it.
unsigned short albw_mq_display_max_internal() {
    const int gauge = static_cast<int>(dComIfGs_getMaxLifeGauge());
    return static_cast<unsigned short>(((gauge + 3) / 4) * 5);
}

// Layout-compatible subclass holding the ported fork moveLife (no new members).
class AlbwMeter2_c : public dMeter2_c {
public:
    void moveLife();
};

#include "mq_move_life_port.inc"  // AlbwMeter2_c::moveLife (fork body, transformed)

namespace {

DEFINE_HOOK(&dMeter2_c::moveLife, MeterMoveLife);

HookAction on_move_life_pre(ModContext*, void* args, void*, void*) {
    if (!albw_mq_is_enabled()) {
        return HOOK_CONTINUE;  // stock meter for non-MQ
    }
    dMeter2_c* self = mods::arg<dMeter2_c*>(args, 0);
    if (self == nullptr) {
        return HOOK_CONTINUE;
    }
    static_cast<AlbwMeter2_c*>(self)->AlbwMeter2_c::moveLife();
    return HOOK_SKIP_ORIGINAL;
}

}  // namespace

ModResult albw_mq_heart_meter_init(ModError*) {
    if (mods::hook::add_pre<MeterMoveLife>(on_move_life_pre) != MOD_OK) {
        svc_log->error(mod_ctx, "failed to hook dMeter2_c::moveLife (mq heart meter)");
        return MOD_ERROR;
    }
    return MOD_OK;
}
