#pragma once

#include <functional>

#include "Repeater.h"
#include "ShaderProgram.h"

class Visualizer {
public:
    typedef std::shared_ptr<Visualizer> Ptr;

    Visualizer(const Repeater::Ptr&);

    //! initialize the context
    void onInit();

    //! set the display size
    void onResize(int x, int y);

    //! paint the screen
    bool onDisplay();

    //! normal key handler
    void onKeyboard(unsigned char c);

    //! special key handler
    void onSpecialKey(int k);

private:
    Repeater::Ptr mRepeater;
    Repeater::History mHistory;

    int mWidth, mHeight;
    double mZoom;

    double mVolume;

    ShaderProgram::Ptr mRoundShader, mSquareShader;

    struct Adjustment {
        std::string name;
        typedef std::function<float(Repeater::Knobs&,float)> Callback;
        Callback cb;
        Adjustment(const std::string& name, const Callback& cb): name(name), cb(cb) {}
    };
    std::map<unsigned char, Adjustment> mAdjustments;

    char mCurAdjustment;
    double mLastAdjustTime;

    void drawHistory();
    void drawBanner();
};
