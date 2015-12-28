#include "Buffer.h"
#include "Calibrator.h"
#include "Drum.h"
#include "Repeater.h"

#include <boost/throw_exception.hpp>

#include <fstream>
#include <iostream>

Repeater::Repeater(const Options& opts, const Knobs& knobs):
    mOptions(opts),
    mKnobs(knobs),
    mKnobsNext(knobs),
    mKnobsUpdated(false),
    mState(S_STARTUP)
{
}

Repeater::History::History():
    playPos(0),
    recordPos(0)
{}

Repeater::History::DataPoint::DataPoint():
    recordedPower(0),
    expectedPower(0),
    limitPower(0),
    targetGain(0),
    actualGain(0)
{}

Repeater::State Repeater::getState() const {
    return mState;
}    

void Repeater::shutdown() {
    if (mState == S_SHUTDOWN_REQUESTED
        || mState == S_SHUTTING_DOWN) {
        mState = S_GONE;
    } else {
        mState = S_SHUTDOWN_REQUESTED;
    }
}

void Repeater::setKnobs(const Knobs& k) {
    mKnobsNext = k;
    mKnobsUpdated = true;
}

void Repeater::getHistory(History& out) const {
    boost::mutex::scoped_lock lock(mHistoryMutex);
    out = mHistory;
}

int Repeater::run() {
    const size_t channels = 2;

    mHistory.history.resize(mOptions.historySize);
    mHistPos = 0;
    mCurDataSamples = 0;

    std::ofstream recDump, listenDump;
    if (!mOptions.recDumpFile.empty()) {
        recDump.open(mOptions.recDumpFile);
    }
    if (!mOptions.listenDumpFile.empty()) {
        listenDump.open(mOptions.listenDumpFile);
    }

    snd_pcm_t *capture, *playback;

    {
        const Options &o = mOptions;
        int err;
        if ((err = snd_pcm_open(&capture, o.captureDevice.c_str(),
                                SND_PCM_STREAM_CAPTURE, 0)) < 0) {
            std::cerr << "Couldn't open " << o.captureDevice << " for capture: "
                      << snd_strerror(err) << std::endl;
            return 1;
        }

        if ((err = snd_pcm_set_params(capture,
                                      SND_PCM_FORMAT_S16, SND_PCM_ACCESS_RW_INTERLEAVED,
                                      channels, o.sampleRate, 1, o.latencyALSA)) < 0) {
            std::cerr << "Couldn't configure " << o.captureDevice << " for capture: "
                      << snd_strerror(err) << std::endl;
            return 1;
        }

        if ((err = snd_pcm_open(&playback, o.captureDevice.c_str(),
                                SND_PCM_STREAM_PLAYBACK, 0)) < 0) {
            std::cerr << "Couldn't open " << o.playbackDevice << " for playback: "
                      << snd_strerror(err) << std::endl;
            return 1;
        }

        if ((err = snd_pcm_set_params(playback,
                                      SND_PCM_FORMAT_S16, SND_PCM_ACCESS_RW_INTERLEAVED,
                                      channels, o.sampleRate, 1, o.latencyALSA)) < 0) {
            std::cerr << "Couldn't configure " << o.playbackDevice << " for playback: "
                      << snd_strerror(err) << std::endl;
            return 1;
        }
    }

    snd_pcm_wait(capture, -1);
    snd_pcm_wait(playback, -1);

    const unsigned int sampleRate = mOptions.sampleRate;
    const double loopDelay = mOptions.loopDelay;
    const size_t bufSize = mOptions.bufSize;

    const size_t loopOffset = sampleRate*loopDelay;

    Drum drum(std::max(bufSize*4, loopOffset*2), channels);
    Buffer recBuf(capture, bufSize, channels),
        playBuf(playback, bufSize, channels),
        listenBuf(NULL, bufSize*2, channels);

    int latencyAdjust;
    try {
        Calibrator cc;
        cc.go(recBuf, playBuf);

        latencyAdjust = cc.getLatency();
        std::cout << "Overall latency: " << latencyAdjust
                  << " (" << latencyAdjust*1.0/sampleRate << "sec)" << std::endl;

        if (mKnobs.feedbackThreshold <= 0) {
            mKnobs.feedbackThreshold = cc.getQuietPower()*3;
            std::cout << "Feedback threshold: " << mKnobs.feedbackThreshold << std::endl;
        }
    } catch (const std::exception& e) {
        std::cerr << "Calibration failed: " << e.what() << std::endl;
        mState = S_GONE;
    }

    size_t recPos = loopOffset - latencyAdjust,
        playPos = 0;

    double curGain = 0, nextGain = 0;

    while (mState != S_GONE) {
        while (mKnobsUpdated.fetch_and(false)) {
            mKnobs = mKnobsNext;
        }
        const Knobs& k = mKnobs;

        switch (mState) {
        case S_STARTUP:
            mState = S_RUNNING;
            break;
        case S_RUNNING:
            break;
        case S_SHUTDOWN_REQUESTED:
            mState = S_SHUTTING_DOWN;
            break;
        case S_SHUTTING_DOWN:
        case S_GONE:
            break;
        }            

        History::DataPoint frameStats;

        int frames = recBuf.record();

        if (recDump) {
            recDump.write(reinterpret_cast<const char *>(&*recBuf.begin()),
                          frames*channels*sizeof(int16_t));
            recDump.flush();
        }

        // compare the recorded power with the expected power
        if (frames > 0) {
            double expected, actual;

            actual = recBuf.power(frames);

            drum.read(listenBuf, playPos - latencyAdjust - bufSize/2, bufSize*2);
            expected = listenBuf.power(frames);
            if (listenDump) {
                listenDump.write(reinterpret_cast<const char *>(&*listenBuf.begin()),
                                 frames*channels*sizeof(int16_t));
                listenDump.flush();
            }

            frameStats.recordedPower = actual;
            frameStats.expectedPower = expected;

            if (actual > 0) {
                double target;
                double level = k.levels.find(k.mode)->second;
                switch (k.mode) {
                case M_GAIN:
                    target = level;
                    break;

                case M_FEEDBACK:
                    if (expected > k.feedbackThreshold && actual > k.feedbackThreshold) {
                        // we have sound, and we are expecting sound
                        target = (expected - k.feedbackThreshold)*level
                            /(actual - k.feedbackThreshold) + k.feedbackThreshold;
                    } else if (expected < k.feedbackThreshold) {
                        // we have no sound yet, so just set the gain to 1
                        target = 1;
                    } else {
                        // we are not expecting sound, so keep it the same
                        target = curGain;
                    }
                    break;

                case M_TARGET:
                    target = level/std::max(0.00001, actual - k.feedbackThreshold);
                    break;
                }

                frameStats.targetGain = target;
                frameStats.limitPower = k.limitPower;

                float cut = 1;
                if (actual > k.limitPower) {
                    cut *= k.limitPower/actual;
                }
                if (expected > k.limitPower) {
                    cut *= k.limitPower/expected;
                }
                target *= cut;

                double factor = k.dampen;
                nextGain = curGain*factor + target*(1 - factor);
            }
        }

        double maxGain = drum.maxGain(playPos, frames);
        nextGain = std::min(maxGain, nextGain);

        if (mState == S_SHUTTING_DOWN) {
            // we're shutting down so just fade out
            nextGain = curGain - bufSize*1.0/sampleRate;
            if (nextGain <= 0) {
                nextGain = 0;
                mState = S_GONE;
            }
        }

        frameStats.actualGain = nextGain;

        playPos = drum.read(playBuf, playPos, frames, curGain, nextGain);
        curGain = nextGain;

        recPos = drum.write(recBuf, recPos, frames);

        frames = playBuf.play(frames);

        {
            boost::mutex::scoped_lock lock(mHistoryMutex);

            size_t histSize = mHistory.history.size();
            size_t dataPos = (recPos*histSize/drum.count()) % histSize;
            if (dataPos != mHistPos) {
                // fill in the history gap
                History::DataPoint prev = mHistory.history[mHistPos];
                while (mHistPos != dataPos) {
                    mHistPos = (mHistPos + 1) % histSize;
                    if (mHistPos != dataPos) {
                        mHistory.history[mHistPos] = prev;
                    }
                }

                // start a new history recording
                mCurData = History::DataPoint();
                mCurDataSamples = 0;
            }

            History::DataPoint &dp = mHistory.history[mHistPos];
            const History::DataPoint &fs = frameStats;
            dp.mode = k.mode;
  
            ++mCurDataSamples;
            dp.recordedPower = (mCurData.recordedPower += fs.recordedPower)/mCurDataSamples;
            dp.expectedPower = (mCurData.expectedPower += fs.expectedPower)/mCurDataSamples;
            dp.limitPower = (mCurData.limitPower += fs.limitPower)/mCurDataSamples;
            dp.targetGain = (mCurData.targetGain += fs.targetGain)/mCurDataSamples;
            dp.actualGain = (mCurData.actualGain += fs.actualGain)/mCurDataSamples;

            size_t drumSize = drum.count();
            mHistory.playPos = ((playPos - latencyAdjust + drumSize)*histSize/drumSize) % histSize;
            mHistory.recordPos = (recPos*histSize/drumSize) % histSize;
        }
    }

    snd_pcm_close(capture);
    snd_pcm_close(playback);
    return 0;
}
