#pragma once

#include "Buffer.h"

class Calibrate {
public:
    Calibrate();

    void go(Buffer& rec, Buffer& play);

    int getLatency() const { return mTotalLatency/mTrials; }
    double getQuietPower() const { return mMaxQuiet; }

private:
    size_t mTrials;
    int mTotalLatency;
    double mMaxQuiet;
};

