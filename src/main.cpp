#include <cstdint>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <vector>

#include <boost/program_options.hpp>
#include <boost/throw_exception.hpp>

#include "Buffer.h"
#include "Calibrate.h"
#include "Drum.h"

namespace po = boost::program_options;

int main(int argc, char *argv[]) try {
    const size_t channels = 2;

    enum {
        M_GAIN,
        M_FEEDBACK,
        M_TARGET
    } mode = M_GAIN;

    float dampen = 0.99;
    unsigned int rate = 44100;
    size_t bufSize = 1024;
    float loopDelay = 10.0;
    float gain = 1.0;
    float feedback = 0.5, feedbackThreshold = 0;
    float powerCap = 0.2;
    float target = 500;
    int latencyALSA = 120000;
    std::string captureDevice = "default";
    std::string playbackDevice = "default";

    std::string recDumpFile, listenDumpFile;

    po::options_description desc("General options");
    desc.add_options()
        ("help,h", "show this help")
        ("list,l", "list devices and exit")
        ("dampen,d", po::value<float>(&dampen)->default_value(dampen),
         "dampening factor")
        ("rate,r", po::value<unsigned int>(&rate)->default_value(rate), "sampling rate")
        ("bufSize,k", po::value<size_t>(&bufSize)->default_value(bufSize), "buffer size")
        ("loopDelay,c", po::value<float>(&loopDelay)->default_value(loopDelay),
         "loop delay, in seconds")
        ("feedback,f", po::value<float>(&feedback),
         "feedback factor")
        ("feedThresh,F", po::value<float>(&feedbackThreshold), "feedback adjustment threshold")
        ("target,t", po::value<float>(&target),
         "target power level")
        ("gain,g", po::value<float>(&gain), "ordinary gain")
        ("limiter,L", po::value<float>(&powerCap)->default_value(powerCap), "power limiter")
        ("latency,q", po::value<int>(&latencyALSA)->default_value(latencyALSA), "ALSA latency, in microseconds")
        ("capture", po::value<std::string>(&captureDevice)->default_value(captureDevice),
         "ALSA capture device")
        ("playback", po::value<std::string>(&playbackDevice)->default_value(playbackDevice),
         "ALSA playback device")
        ("recDump", po::value<std::string>(&recDumpFile), "Recording dump file (raw PCM)")
        ("listenDump", po::value<std::string>(&listenDumpFile), "Play dump file (raw PCM)")
        ;

    {
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
            mode = M_TARGET;

            if (target < powerCap) {
                std::cerr << "Target power can't be less than the power cap" << std::endl;
                return 1;
            }
        }
        if (vm.count("feedback")) {
            ++modeCount;
            mode = M_FEEDBACK;
        }
        if (vm.count("gain")) {
            ++modeCount;
            mode = M_GAIN;
        }
        if (modeCount > 1) {
            std::cerr << "Error: Can only specify one mode" << std::endl;
            return 1;
        }
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
                                  channels, rate, 1, latencyALSA)) < 0) {
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
                                  channels, rate, 1, latencyALSA)) < 0) {
        std::cerr << "Couldn't configure " << captureDevice << " for capture: "
                  << snd_strerror(err) << std::endl;
        return 1;
    }

    snd_pcm_wait(capture, -1);
    snd_pcm_wait(playback, -1);

    const size_t loopOffset = rate*loopDelay;

    Drum drum(std::max(bufSize*4, loopOffset*2), channels);
    Buffer recBuf(capture, bufSize, channels),
        playBuf(playback, bufSize, channels),
        listenBuf(NULL, bufSize*2, channels);

    int latencyAdjust;
    {
        Calibrate cc;
        for (int i = 0; i < 4; i++) {
            cc.go(recBuf, playBuf);
        }
        latencyAdjust = cc.getLatency();
        std::cout << "Overall latency: " << latencyAdjust << std::endl;

        if (feedbackThreshold <= 0) {
            feedbackThreshold = cc.getQuietPower()*3;
            std::cout << "Feedback threshold: " << feedbackThreshold << std::endl;
        }
    }

    size_t recPos = 0,
        playPos = loopOffset - latencyAdjust;

    float curGain = 0, nextGain = 0;

    size_t posDigits = ceil(log(drum.count())/log(10));

    for (;;) {
        int frames = recBuf.record();

        if (recDump) {
            recDump.write(reinterpret_cast<const char *>(&*recBuf.begin()), frames*channels*sizeof(int16_t));
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

            if (actual > 0) {
                float adjusted;
                switch (mode) {
                case M_GAIN:
                    adjusted = gain;
                    break;

                case M_FEEDBACK:
                    if (expected > feedbackThreshold && actual > feedbackThreshold) {
                        // we have sound, and we are expecting sound
                        adjusted = (expected - feedbackThreshold)*feedback/(actual - feedbackThreshold);
                    } else if (expected < feedbackThreshold) {
                        // we have no sound yet, so just set the gain to 1
                        adjusted = 1;
                    } else {
                        // we are not expecting sound, so keep it the same
                        adjusted = curGain;
                    }
                    break;

                case M_TARGET:
                    adjusted = target/actual;
                    break;
                }

                if (actual > powerCap) {
                    adjusted = adjusted*powerCap/actual;
                    std::cout << 'L';
                } else {
                    std::cout << ' ';
                }

                std::cout << "expect=" << std::setw(8) << std::setprecision(3) << expected;
                std::cout << " actual=" << std::setw(8) << std::setprecision(3) << actual;
                nextGain = curGain*dampen + adjusted*(1.0 - dampen);

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
        playPos = drum.read(playBuf, playPos, frames, curGain, nextGain);
        curGain = nextGain;

        recPos = drum.write(recBuf, recPos, frames);

        frames = playBuf.play(frames);

        std::cout << "  pos="
                  << std::setw(posDigits) << playPos
                  << " " << std::setw(posDigits) << recPos;
        std::cout << '\r';
        std::cout.flush();
    }

    snd_pcm_close(capture);
    snd_pcm_close(playback);
    return 0;
} catch (const std::exception& e) {
    std::cerr << "Error: " << e.what() << std::endl;
}

