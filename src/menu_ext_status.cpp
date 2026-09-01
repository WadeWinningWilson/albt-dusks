// ============================================
// NEW CODE - ALBW Port (Ext Status page - Tools / Quest / Atlas)
// dMenu_ExtStatus_c, ported from fork src/d/d_ext_mod_flags.cpp:962-1072.
// This is an entirely NEW class in the fork rather than an edit to a stock one,
// so it ports wholesale - no member-injection problem.
//
// It is registry-driven: rows come from dExtStatus_peekTab, which any mod can
// populate through the published "dev.albt.albw.quick_equip" service.
// ============================================

#include "global.h"
#include <os.h>

#include "JSystem/J2DGraph/J2DPrint.h"
#include "JSystem/J2DGraph/J2DGrafContext.h"
#include "JSystem/JUtility/JUTFont.h"
#include "d/d_com_inf_game.h"
#include "d/d_menu_window.h"
#include "m_Do/m_Do_ext.h"
#include "m_Do/m_Do_controller_pad.h"

#include "menu_ext_status.h"
#include "ext_status.h"
#include "albw_common.h"
#include "albw_game.h"

#include <cstring>

#if TARGET_PC

dMenu_ExtStatus_c::dMenu_ExtStatus_c(STControl* stick, CSTControl* cstick)
    : mpStick(stick),
      mpCStick(cstick),
      mTab(dExtStatusTab_Tools),
      mCursor(0),
      mCollectHandoff(-1),
      mWantsClose(false) {}

void dMenu_ExtStatus_c::_create() {
    mTab = dExtStatusTab_Tools;
    mCursor = 0;
    mCollectHandoff = -1;
    mWantsClose = false;
}

void dMenu_ExtStatus_c::_delete() {}

void dMenu_ExtStatus_c::setTab(dExtStatusTab tab) {
    if (tab >= dExtStatus_kTabCount) {
        return;
    }
    mTab = tab;
    mCursor = 0;
}

void dMenu_ExtStatus_c::_move() {
    mCollectHandoff = -1;
    if (dMw_B_TRIGGER() || dMw_START_TRIGGER()) {
        mWantsClose = true;
        return;
    }
    if (dMw_RIGHT_TRIGGER()) {
        if (mTab >= dExtStatusTab_Atlas) {
            mCollectHandoff = 1;
        } else {
            setTab(static_cast<dExtStatusTab>(mTab + 1));
        }
        return;
    }
    if (dMw_LEFT_TRIGGER()) {
        if (mTab <= dExtStatusTab_Tools) {
            mCollectHandoff = 0;
        } else {
            setTab(static_cast<dExtStatusTab>(mTab - 1));
        }
        return;
    }
    const u8 count = dExtStatus_countTab(mTab);
    if (dMw_DOWN_TRIGGER() && count > 0) {
        mCursor = static_cast<u8>((mCursor + 1) % count);
    } else if (dMw_UP_TRIGGER() && count > 0) {
        mCursor = static_cast<u8>((mCursor + count - 1) % count);
    }
    if (dMw_A_TRIGGER() && mTab == dExtStatusTab_Tools && count > 0) {
        const dExtStatusRow* row = dExtStatus_peekTab(mTab, mCursor);
        if (row != NULL) {
            dExtStatus_tryDeepLinkZ(row->id);
        }
    }
}

void dMenu_ExtStatus_c::draw() {
    J2DGrafContext* graf = dComIfGp_getCurrentGrafPort();
    if (graf == NULL) {
        return;
    }
    graf->setup2D();
    JUTFont* font = mDoExt_getMesgFont();
    if (font == NULL) {
        return;
    }
    const JUtility::TColor white(255, 255, 255, 255);
    const JUtility::TColor dim(180, 180, 180, 255);
    J2DPrint print(font, 0.0f, 20.0f, white, white, JUtility::TColor(0, 0, 0, 0), white);
    print.setFontSize(18.0f, 18.0f);
    font->pushDrawState();
    print.initiate();

    print.print(40.0f, 40.0f, 255, "STATUS");
    static const char* kTabs[] = {"Tools", "Quest", "Atlas"};
    f32 tx = 40.0f;
    for (u8 t = 0; t < dExtStatus_kTabCount; ++t) {
        const bool on = (t == mTab);
        print.setCharColor(on ? white : dim);
        print.print(tx, 70.0f, 255, "%s%s", on ? "[" : " ", kTabs[t]);
        if (on) {
            print.print(tx + 8.0f * static_cast<f32>(std::strlen(kTabs[t])) + 8.0f, 70.0f, 255, "]");
        }
        tx += 100.0f;
    }
    print.setCharColor(white);

    const u8 count = dExtStatus_countTab(mTab);
    if (count == 0) {
        print.print(40.0f, 120.0f, 255, "(empty)");
    } else {
        for (u8 i = 0; i < count && i < 12; ++i) {
            const dExtStatusRow* row = dExtStatus_peekTab(mTab, i);
            if (row == NULL) {
                continue;
            }
            const char* mark = (i == mCursor) ? "> " : "  ";
            const char* lab = row->label[0] != '\0' ? row->label : "(row)";
            print.print(40.0f, 110.0f + 22.0f * static_cast<f32>(i), 255, "%s%s", mark, lab);
        }
    }
    print.print(40.0f, 400.0f, 255, "L/R tabs  A assign  B/START close");
    font->popDrawState();
}


#endif  // TARGET_PC

// ============================================
// NEW CODE ENDS HERE
// ============================================
