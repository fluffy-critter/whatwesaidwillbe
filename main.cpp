#include <iostream>
#include <iomanip>
#include <vector>
#include <boost/program_options.hpp>
#include <alsa/asoundlib.h>
#include <stdint.h>

#include "Drum.h"
#include "Buffer.h"

namespace po = boost::program_options;

static const int FP_SHIFT = 15;

int main(int argc, char *argv[]) {
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
    float target = 500;
	std::string captureDevice = "default";
	std::string playbackDevice = "default";

	po::options_description desc("General options");
	desc.add_options()
        ("help,h", "show this help")
		("dampen,d", po::value<float>(&dampen)->default_value(dampen),
         "dampening factor (0-1024)")
		("rate,r", po::value<unsigned int>(&rate)->default_value(rate), "sampling rate")
		("bufSize,k", po::value<size_t>(&bufSize)->default_value(bufSize), "buffer size")
		("loopDelay,c", po::value<float>(&loopDelay)->default_value(loopDelay),
         "loop delay, in seconds")
        ("feedback,f", po::value<float>(&feedback),
         "feedback factor")
        ("target,t", po::value<float>(&target),
         "target power level")
        ("gain,g", po::value<float>(&gain), "ordinary gain")
		("capture", po::value<std::string>(&captureDevice)->default_value(captureDevice),
         "ALSA capture device")
		("playback", po::value<std::string>(&playbackDevice)->default_value(playbackDevice),
         "ALSA playback device")
		;

	try {
		po::variables_map vm;
		po::store(po::command_line_parser(argc, argv).options(desc).run(), vm);
        if (vm.count("help")) {
            std::cerr << desc << std::endl;
            return 1;
        }
		po::notify(vm);

        int modeCount = 0;
        if (vm.count("target")) {
            ++modeCount;
            mode = M_TARGET;
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
	} catch (const std::exception& e) {
		std::cerr << e.what() << std::endl;
		return 1;
	}

	snd_pcm_t *capture, *playback;

	int err;
	if ((err = snd_pcm_open(&capture, captureDevice.c_str(), SND_PCM_STREAM_CAPTURE, 0)) < 0) {
		std::cerr << "Couldn't open " << captureDevice << " for capture: "
                  << snd_strerror(err) << std::endl;
		return 1;
	}

    const int latency = 75000;

	if ((err = snd_pcm_set_params(capture,
                                  SND_PCM_FORMAT_S16, SND_PCM_ACCESS_RW_INTERLEAVED,
                                  channels, rate, 1, latency)) < 0) {
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
                                  channels, rate, 1, latency)) < 0) {
		std::cerr << "Couldn't configure " << captureDevice << " for capture: "
                  << snd_strerror(err) << std::endl;
        return 1;
    }

    const size_t loopOffset = rate*loopDelay;

    Drum drum(std::max(bufSize*4, loopOffset*2), channels);
    Buffer recBuf(bufSize, channels),
        playBuf(bufSize, channels),
        listenBuf(bufSize*4, channels);

    size_t latencyTime = latency + bufSize*1000000/rate;
    size_t latencySamples = rate*latencyTime/1000000;

    size_t recPos = 0,
        playPos = loopOffset;

    int curGain = 0, nextGain = 0;

    size_t posDigits = ceil(log(drum.count())/log(10));

    for (;;) {
        int frames = snd_pcm_readi(capture,
                                   recBuf.begin(),
                                   recBuf.count());
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

        // compare the recorded power with the expected power
        if (frames > 0) {
            double expected, actual;
            switch (mode) {
            case M_GAIN:
                expected = gain;
                actual = 1.0;
                break;

            case M_FEEDBACK:
                drum.read(listenBuf, playPos + loopOffset - latencySamples, bufSize*4);
                expected = listenBuf.power(bufSize*4)*feedback;
                actual = recBuf.power(frames);
                break;

            case M_TARGET:
                drum.read(listenBuf, recPos + drum.count() - bufSize*4, bufSize*4);
                expected = target;
                actual = recBuf.power(bufSize*4);
                break;
            }

            if (actual > 0 && expected > 0) {
                int adjust = expected*(1 << 10)/actual;

                std::cout << "expect=" << std::setw(8) << std::setprecision(3) << expected;
                std::cout << " actual=" << std::setw(8) << std::setprecision(3) << actual;
                nextGain = curGain*dampen + adjust*(1.0 - dampen);

                std::cout << " gain="
                          << std::setw(6) << curGain << "->"
                          << std::setw(6) << nextGain;
            }
        }

        recPos = drum.write(recBuf, recPos, frames);

        int maxGain = drum.maxGain(playPos, frames);
        if (maxGain < nextGain) {
            std::cout << 'c';
            nextGain = maxGain;
        } else {
            std::cout << ' ';
        }
        playPos = drum.read(playBuf, playPos, frames, curGain, nextGain);
        curGain = nextGain;

        frames = snd_pcm_writei(playback,
                                playBuf.begin(),
                                frames);
        if (frames < 0) {
            frames = snd_pcm_recover(playback, frames, 0);
            if (frames > 0 && (size_t)frames < bufSize) {
                bufSize = frames;
                std::cerr << "Adjusting bufsize to " << bufSize << std::endl;
            }
        }
        if (frames < 0) {
            std::cerr << "Error during playback: " << snd_strerror(frames) << std::endl;
            break;
        }

        std::cout << "  pos="
                  << std::setw(posDigits) << playPos
                  << " " << std::setw(posDigits) << recPos;
        std::cout << '\r';
        std::cout.flush();
    }

    snd_pcm_close(capture);
    snd_pcm_close(playback);
    return 0;
}
