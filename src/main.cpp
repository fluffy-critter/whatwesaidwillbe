#include <cstdint>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <vector>

#include <boost/program_options.hpp>
#include <boost/throw_exception.hpp>

#include "Buffer.h"
#include "Drum.h"

namespace po = boost::program_options;

static const int FP_SHIFT = 15;

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
    float feedback = 0.5;
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
    Buffer recBuf(bufSize, channels),
        playBuf(bufSize, channels),
        listenBuf(bufSize*4, channels);

    // autocalibrate the latency
    int latencyAdjust = 0;
    {
        double quietPower = 0;
        int frames;

        // get a quiescent reading
        std::cout << "Obtaining quiescent power level...";
        std::cout.flush();
        for (size_t i = 0; i < 10; i++) {
            frames = recBuf.record(capture);
            quietPower = std::max(quietPower, recBuf.power(frames));
            playBuf.play(playback, frames);
        }
        std::cout << quietPower << std::endl;

        // send a burst of static
        std::cout << "Waiting for static burst...";
        std::cout.flush();
        for (int16_t &t : playBuf) {
            t = rand();
        }

        do {
            frames = recBuf.record(capture);
            playBuf.play(playback, frames);
            std::fill(playBuf.begin(), playBuf.end(), 0);
            latencyAdjust += frames;
        } while (recBuf.power(frames) < 2*quietPower);

        // figure out whereabouts the static started
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
        std::cout << "Adjustment is " << latencyAdjust << " samples (" << latencyAdjust*1.0/rate << " seconds)"
                  << std::endl;
    }

    size_t recPos = 0,
        playPos = loopOffset;

    float curGain = 0, nextGain = 0;

    size_t posDigits = ceil(log(drum.count())/log(10));

    for (;;) {
        int frames = recBuf.record(capture);
        if (frames < 0) {
            frames = snd_pcm_recover(capture, frames, 0);
            if (frames > 0 && (size_t)frames < bufSize) {
                bufSize = frames;
                std::cerr << "Adjusting bufsize to " << bufSize << std::endl;
            }
        }
        if (frames < 0) {
            std::cerr << "Error during capture: " << snd_strerror(frames) << std::endl;
            break;
        }

        if (recDump) {
            recDump.write(reinterpret_cast<const char *>(recBuf.begin()), frames*channels*sizeof(int16_t));
            recDump.flush();
        }

        // compare the recorded power with the expected power
        if (frames > 0) {
            double expected, actual;

            actual = recBuf.power(frames);

            drum.read(listenBuf, playPos - latencyAdjust, frames);
            expected = listenBuf.power(frames);
            if (listenDump) {
                listenDump.write(reinterpret_cast<const char *>(listenBuf.begin()), frames*channels*sizeof(int16_t));
                listenDump.flush();
            }

            if (actual > 0) {
                float adjusted;
                switch (mode) {
                case M_GAIN:
                    adjusted = gain;
                    break;

                case M_FEEDBACK:
                    adjusted = expected/actual;
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

        frames = playBuf.play(playback, frames);

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

