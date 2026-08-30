#pragma once

#include "rental_ui_text.h"
#include "d/d_drawlist.h"

class CPaneMgr;
class J2DScreen;
class J2DTextBox;
class dMsgScrnArrow_c;

// Port of fork dALBWDialogue_c — native TP message window for rental greet/farewell.
class dALBWDialogue_c : public dDlst_base_c {
public:
    dALBWDialogue_c();
    ~dALBWDialogue_c();

    bool tryCreate();
    void showWithText(const char* text);
    void hide();
    void registerDraw();

    bool checkDismiss();
    bool isReady() const { return mReady; }
    bool isVisible() const { return mVisible; }

    void draw() override;

private:
    void buildPages(const char* text);
    bool advancePage();
    void ensureArrow();

    static constexpr int kVanillaMaxLines = 4;
    static constexpr int kMaxPages = 8;
    static constexpr int kPageBufSize = 512;

    J2DScreen* mpScreen = nullptr;
    CPaneMgr* mpFrameMgr = nullptr;
    CPaneMgr* mpArwAnchor = nullptr;
    dMsgScrnArrow_c* mpArrow = nullptr;

    J2DScreen* mpTxScreen = nullptr;
    J2DTextBox* mpTextBox = nullptr;
    CPaneMgr* mpTextMgr = nullptr;
    CPaneMgr* mpT4Mgr = nullptr;

    f32 mFontSizeX = 0.0f;
    f32 mFontSizeY = 0.0f;
    f32 mLineSpace = 0.0f;
    f32 mCharSpace = 0.0f;
    f32 mWrapMaxWidth = 0.0f;
    f32 mTextBoxWidth = 0.0f;
    f32 mTextBoxHeight = 0.0f;
    int mMaxLinesPerPage = kVanillaMaxLines;
    dALBWPaneBounds mTextBounds;

    char mPages[kMaxPages][kPageBufSize] = {};
    int mPageCount = 0;
    int mPageIndex = 0;

    bool mReady = false;
    bool mVisible = false;
    int mCooldown = 0;
};
