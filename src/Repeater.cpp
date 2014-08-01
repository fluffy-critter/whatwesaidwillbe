#include <fstream>
#include <iomanip>
#include <iostream>

#include <boost/program_options.hpp>
#include <boost/throw_exception.hpp>

#include "Buffer.h"
#include "Calibrator.h"
#include "Drum.h"
#include "Repeater.h"

namespace po = boost::program_options;

Repeater::Repeater():
    mMode(M_GAIN),
    mDampenFactor(0.99),
    mLoopTime(10.0),
    mLimiter(0.2),
    mShutdown(false)
{
    mLevels[M_GAIN] = 1;
    mLevels[M_FEEDBACK] = 0.5;
    mLevels[M_TARGET] = 0.2;
}

Repeater::History::DataPoint::DataPoint():
    recordedPower(0),
    expectedPower(0),
    limitPower(0),
    targetGain(0),
    actualGain(0)
{}


void Repeater::shutdown() {
    boost::mutex::scoped_lock lock(mConfigMutex);
    mShutdown = true;
}

Repeater::Mode Repeater::getMode() const {
    boost::mutex::scoped_lock lock(mConfigMutex);
    return mMode;
}

void Repeater::setMode(Mode m) {
    boost::mutex::scoped_lock lock(mConfigMutex);
    mMode = m;
}

float Repeater::getModeLevel(Mode m) const {
    boost::mutex::scoped_lock lock(mConfigMutex);
    auto ll = mLevels.find(m);
    if (ll == mLevels.end()) {
        BOOST_THROW_EXCEPTION(std::range_error("Nonexistent model level"));
    }
    return ll->second;
}

void Repeater::setModeLevel(Mode m, float level) {
    boost::mutex::scoped_lock lock(mConfigMutex);
    mLevels[m] = level;
}

float Repeater::getDampenFactor() const {
    boost::mutex::scoped_lock lock(mConfigMutex);
    return mDampenFactor;
}

void Repeater::setDampenFactor(float f) {
    boost::mutex::scoped_lock lock(mConfigMutex);
    mDampenFactor = f;
}

float Repeater::getLoopTime() const {
    boost::mutex::scoped_lock lock(mConfigMutex);
    return mLoopTime;
}

float Repeater::getLimiterLevel() const {
    boost::mutex::scoped_lock lock(mConfigMutex);
    return mLimiter;
}

void Repeater::setLimiterLevel(float ll) {
    boost::mutex::scoped_lock lock(mConfigMutex);
    mLimiter = ll;
}

void Repeater::getHistory(History& out) const {
    boost::mutex::scoped_lock lock(mHistoryMutex);
    out = mHistory;
}

int Repeater::run(int argc, char *argv[]) {
    const size_t channels = 2;

    unsigned int sampleRate = 44100;
    size_t bufSize = 1024;
    float loopDelay = 10.0;
    float feedbackThreshold = 0;
    int latencyALSA = 120000;
    size_t historySize = 1024;
    std::string captureDevice = "default";
    std::string playbackDevice = "default";

    std::string recDumpFile, listenDumpFile;

    po::options_description desc("General options");
    desc.add_options()
        ("help,h", "show this help")
        ("list,l", "list devices and exit")
        ("dampen,d", po::value<float>(&mDampenFactor)->default_value(mDampenFactor),
         "dampening factor")
        ("rate,r", po::value<unsigned int>(&sampleRate)->default_value(sampleRate), "sampling rate")
        ("bufSize,k", po::value<size_t>(&bufSize)->default_value(bufSize), "buffer size")
        ("loopDelay,c", po::value<float>(&loopDelay)->default_value(loopDelay),
         "loop delay, in seconds")
        ("feedback,f", po::value<float>(&mLevels[M_FEEDBACK]),
         "feedback factor")
        ("feedThresh,F", po::value<float>(&feedbackThreshold), "feedback adjustment threshold")
        ("target,t", po::value<float>(&mLevels[M_TARGET]),
         "target power level")
        ("gain,g", po::value<float>(&mLevels[M_GAIN]), "ordinary gain")
        ("limiter,L", po::value<float>(&mLimiter)->default_value(mLimiter), "power limiter")
        ("latency,q", po::value<int>(&latencyALSA)->default_value(latencyALSA),
         "ALSA latency, in microseconds")
        ("capture", po::value<std::string>(&captureDevice)->default_value(captureDevice),
         "ALSA capture device")
        ("playback", po::value<std::string>(&playbackDevice)->default_value(playbackDevice),
         "ALSA playback device")
        ("recDump", po::value<std::string>(&recDumpFile), "Recording dump file (raw PCM)")
        ("listenDump", po::value<std::string>(&listenDumpFile), "Play dump file (raw PCM)")
        ("historySize", po::value<size_t>(&historySize), "Size of the history buffer")
        ;

    {
        boost::mutex::scoped_lock l1(mConfigMutex), l2(mHistoryMutex);

        po::variables_map vm;
        po::store(po::command_line_parser(argc, argv).options(desc).run(), vm);

        if (vm.count("help")) {
            std::cerr << desc << std::endl;
            return 1;
        }

        if (vm.count("list")) {
            char **hints;
            int err;
            if ((err = snd_device_name_hint(-1, "pcm", (void***)&hints))) {
                std::cerr << "Couldn't get device list: " << snd_strerror(err) << std::endl;
                return 1;
            }

            while (*hints) {
                std::cout << "Name: " << snd_device_name_get_hint(*hints, "NAME") << std::endl
                          << "Desc: " << snd_device_name_get_hint(*hints, "DESC") << std::endl
                          << std::endl;
                ++hints;
            }
            return 0;
        }

        po::notify(vm);

        int modeCount = 0;
        if (vm.count("target")) {
            ++modeCount;
            mMode = M_TARGET;
        }
        if (vm.count("feedback")) {
            ++modeCount;
            mMode = M_FEEDBACK;
        }
        if (vm.count("gain")) {
            ++modeCount;
            mMode = M_GAIN;
        }
        if (modeCount > 1) {
            std::cerr << "Error: Can only specify one mode" << std::endl;
            return 1;
        }

        mHistory.history.resize(historySize);
        mHistPos = ~0;
    }

    std::ofstream recDump, listenDump;
    if (!recDumpFile.empty()) {
        recDump.open(recDumpFile);
    }
    if (!listenDumpFile.empty()) {
        listenDump.open(listenDumpFile);
    }

    snd_pcm_t *capture, *playback;

    int err;
    if ((err = snd_pcm_open(&capture, captureDevice.c_str(), SND_PCM_STREAM_CAPTURE, 0)) < 0) {
        std::cerr << "Couldn't open " << captureDevice << " for capture: "
                  << snd_strerror(err) << std::endl;
        return 1;
    }

    if ((err = snd_pcm_set_params(capture,
                                  SND_PCM_FORMAT_S16, SND_PCM_ACCESS_RW_INTERLEAVED,
                                  channels, sampleRate, 1, latencyALSA)) < 0) {
        std::cerr << "Couldn't configure " << captureDevice << " for capture: "
                  << snd_strerror(err) << std::endl;
        return 1;
    }

    if ((err = snd_pcm_open(&playback, captureDevice.c_str(), SND_PCM_STREAM_PLAYBACK, 0)) < 0) {
        std::cerr << "Couldn't open " << captureDevice << " for capture: "
                  << snd_strerror(err) << std::endl;
        return 1;
    }

    if ((err = snd_pcm_set_params(playback,
                                  SND_PCM_FORMAT_S16, SND_PCM_ACCESS_RW_INTERLEAVED,
                                  channels, sampleRate, 1, latencyALSA)) < 0) {
        std::cerr << "Couldn't configure " << captureDevice << " for capture: "
                  << snd_strerror(err) << std::endl;
        return 1;
    }

    snd_pcm_wait(capture, -1);
    snd_pcm_wait(playback, -1);

    const size_t loopOffset = sampleRate*loopDelay;

    Drum drum(std::max(bufSize*4, loopOffset*2), channels);
    Buffer recBuf(capture, bufSize, channels),
        playBuf(playback, bufSize, channels),
        listenBuf(NULL, bufSize*2, channels);

    int latencyAdjust;
    {
        Calibrator cc;
        cc.go(recBuf, playBuf);

        latencyAdjust = cc.getLatency();
        std::cout << "Overall latency: " << latencyAdjust
                  << " (" << latencyAdjust*1.0/sampleRate << "sec)" << std::endl;

        if (feedbackThreshold <= 0) {
            feedbackThreshold = cc.getQuietPower()*3;
            std::cout << "Feedback threshold: " << feedbackThreshold << std::endl;
        }
    }

    size_t recPos = loopOffset - latencyAdjust,
        playPos = 0;

    float curGain = 0, nextGain = 0;

    size_t posDigits = ceil(log(drum.count())/log(10));

    bool shouldQuit = false;
    while (!shouldQuit) {
        {
            size_t dataPos = (recPos*historySize/drum.count()) % historySize;
            if (dataPos != mHistPos) {
                mCurData = History::DataPoint();
                mCurDataSamples = 0;
                mHistPos = dataPos;
            }
        }

        boost::mutex::scoped_lock lock(mConfigMutex);
        if (mShutdown) {
            shouldQuit = true;
        }
        
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

            mCurData.recordedPower += actual;
            mCurData.expectedPower += expected;

            if (actual > 0) {
                float adjusted;
                switch (mMode) {
                case M_GAIN:
                    adjusted = mLevels[M_GAIN];
                    break;

                case M_FEEDBACK:
                    if (expected > feedbackThreshold && actual > feedbackThreshold) {
                        // we have sound, and we are expecting sound
                        adjusted = (expected - feedbackThreshold)*mLevels[M_FEEDBACK]
                            /(actual - feedbackThreshold);
                    } else if (expected < feedbackThreshold) {
                        // we have no sound yet, so just set the gain to 1
                        adjusted = 1;
                    } else {
                        // we are not expecting sound, so keep it the same
                        adjusted = curGain;
                    }
                    break;

                case M_TARGET:
                    adjusted = mLevels[M_TARGET]/actual;
                    break;
                }

                mCurData.targetGain += adjusted;
                mCurData.limitPower += mLimiter;

                if (actual > mLimiter) {
                    adjusted = adjusted*mLimiter/actual;
                    std::cout << 'L';
                } else {
                    std::cout << ' ';
                }

                std::cout << "expect=" << std::setw(8) << std::setprecision(3) << expected;
                std::cout << " actual=" << std::setw(8) << std::setprecision(3) << actual;
                nextGain = curGain*mDampenFactor + adjusted*(1.0 - mDampenFactor);

                std::cout << " gain="
                          << std::setw(6) << curGain << "->"
                          << std::setw(6) << nextGain;
            }
        }

        int maxGain = drum.maxGain(playPos, frames);
        if (maxGain < nextGain) {
            std::cout << 'c';
            nextGain = maxGain;
        } else {
            std::cout << ' ';
        }

        if (shouldQuit) {
            // we're shutting down so just fade out
            nextGain = 0;
        }

        playPos = drum.read(playBuf, playPos, frames, curGain, nextGain);
        curGain = nextGain;

        mCurData.actualGain += nextGain;

        recPos = drum.write(recBuf, recPos, frames);

        frames = playBuf.play(frames);

        std::cout << "  pos="
                  << std::setw(posDigits) << playPos
                  << " " << std::setw(posDigits) << recPos;
        std::cout << '\r';
        std::cout.flush();

        {
            ++mCurDataSamples;
            boost::mutex::scoped_lock lock(mHistoryMutex);
            History::DataPoint &dp = mHistory.history[mHistPos];
            dp.mode = mMode;
            dp.recordedPower = mCurData.recordedPower/mCurDataSamples;
            dp.expectedPower = mCurData.expectedPower/mCurDataSamples;
            dp.limitPower = mCurData.limitPower/mCurDataSamples;
            dp.targetGain = mCurData.targetGain/mCurDataSamples;
            dp.actualGain = mCurData.actualGain/mCurDataSamples;
        }
    }

    snd_pcm_close(capture);
    snd_pcm_close(playback);
    return 0;
}
