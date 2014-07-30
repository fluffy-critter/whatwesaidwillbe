#include <cmath>
#include <stdexcept>
#include <iostream>

#include <boost/throw_exception.hpp>

#include "Buffer.h"
#include "Calibrate.h"

Calibrate::Calibrate():
    mTrials(0),
    mTotalLatency(0),
    mMaxQuiet(0)
{}

void Calibrate::go(Buffer& recBuf, Buffer& playBuf) {
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
    int period = playBuf.count()*playBuf.channels();
    for (size_t i = 0; i < period; i++) {
        float x = i*2*M_PI/period;
        float y = (sin(i*127) + sin(i*11) + sin(i*23) + sin(i*71))/4;
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

    // figure out whereabouts the static started (TODO: make this algorithm better)
    size_t left = 0, right = frames;
    while (left + 50 < right) {
        size_t mid = (left + right)/2;
        double powLeft = recBuf.power(mid - left, left);
        double powRight = recBuf.power(right - mid, mid);
        if (powLeft < 2*quietPower) {
            left = mid;
        } else {
            right = mid;
        }
    }
    latencyAdjust -= frames - left;

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

