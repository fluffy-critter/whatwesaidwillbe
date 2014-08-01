#include <cmath>
#include <stdexcept>
#include <iostream>

#include <boost/throw_exception.hpp>

#include "Buffer.h"
#include "Calibrator.h"

Calibrator::Calibrator():
    mTrials(0),
    mTotalLatency(0),
    mMaxQuiet(0)
{}

void Calibrator::go(Buffer& recBuf, Buffer& playBuf) {
    // autocalibrate the latency
    int latencyAdjust = 0;
    int frames;

    // get a quiescent reading
    std::cout << "Obtaining quiescent power level...";
    std::cout.flush();

    std::fill(playBuf.begin(), playBuf.end(), 0);
    double quietPower = 0;
    for (size_t i = 0; i < 10; i++) {
        frames = recBuf.record();
        quietPower = std::max(quietPower, recBuf.power(frames));
        playBuf.play(frames);
    }
    std::cout << quietPower << std::endl;

    // send a brief burst of a tonal sound thing
    std::cout << "Waiting for burst...";
    std::cout.flush();
    Buffer::iterator out = playBuf.begin();
    size_t period = playBuf.count()*playBuf.channels();
    for (size_t i = 0; i < period; i++) {
        float x = i*2*M_PI/period;
        float y = (sin(x*163) + sin(x*67) + sin(x*69)/3 + sin(x*71)/5)/4;
        *out++ = y*32767;
    }

    time_t startTime = time(NULL);
    do {
        frames = recBuf.record();
        playBuf.play(frames);
        latencyAdjust += frames;
    } while (recBuf.power(frames) < 2*quietPower && time(NULL) < startTime + 2);
    if (recBuf.power(frames) < 2*quietPower) {
        BOOST_THROW_EXCEPTION(std::runtime_error("Timed out waiting for calibration burst"));
    }

    std::cout << "Burst detected, power=" << recBuf.power(frames)/quietPower << "x" << std::endl;

    // figure out whereabouts the burst started (this is naive but who cares)
    size_t maxPos = 0;
    float maxDelta = 0;
    float lastVal = recBuf.power(frames);
    for (int split = 0; split < frames; split++) {
        float val = recBuf.power(frames - split, split);
        float delta = val - lastVal;
        if (delta > maxDelta) {
            maxPos = split;
            maxDelta = delta;
        }
        lastVal = val;
    }
    latencyAdjust -= frames - maxPos;

    // wait for silence to return
    std::fill(playBuf.begin(), playBuf.end(), 0);
    do {
        frames = recBuf.record();
        playBuf.play(frames);
    } while (recBuf.power(frames) >= quietPower*1.5);
    std::cout << "Result: " << latencyAdjust << std::endl;

    mTotalLatency += latencyAdjust;
    mMaxQuiet = std::max(mMaxQuiet, quietPower);
    ++mTrials;
}

