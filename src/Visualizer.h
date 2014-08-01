#pragma once

#include "Repeater.h"
#include "ShaderProgram.h"

class Visualizer {
public:
    typedef std::shared_ptr<Visualizer> Ptr;

    Visualizer(const Repeater::Ptr&);

    // initialize the context
    void onInit();

    // set the display size
    void onResize(int x, int y);

    // paint the screen
    void onDisplay();

private:
    Repeater::Ptr mRepeater;
    Repeater::History mHistory;

    int mWidth, mHeight;
    ShaderProgram::Ptr mRoundShader, mSquareShader;
};
