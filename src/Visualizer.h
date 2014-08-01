#pragma once

#include "Repeater.h"

class Visualizer {
public:
    typedef std::shared_ptr<Visualizer> Ptr;

    Visualizer(const Repeater::Ptr&);

    void onResize(int x, int y);
    void onDisplay();

private:
    Repeater::Ptr mRepeater;

    int mWidth, mHeight;
};
