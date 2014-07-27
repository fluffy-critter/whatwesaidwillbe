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

	unsigned int dampen = 256;
	unsigned int rate = 44100;
    size_t bufSize = 1024;
    float loopDelay = 10.0;
	std::string captureDevice = "default";
	std::string playbackDevice = "default";

	po::options_description desc("General options");
	desc.add_options()
        ("help,h", "show this help")
		("dampen,d", po::value<unsigned int>(&dampen)->default_value(dampen),
         "dampening factor (0-1024)")
		("rate,r", po::value<unsigned int>(&rate)->default_value(rate), "sampling rate")
		("bufSize,k", po::value<size_t>(&bufSize)->default_value(bufSize), "buffer size")
		("loopDelay,c", po::value<float>(&loopDelay)->default_value(loopDelay),
         "loop delay, in seconds")
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

    const int latency = 100000;

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

    Drum drum(loopOffset*2, channels);
    Buffer recBuf(bufSize, channels),
        playBuf(bufSize, channels);

    size_t recPos = 0,
        playPos = loopOffset;

    for (;;) {
        int frames = snd_pcm_readi(capture,
                                   recBuf.begin(),
                                   recBuf.count());
        if (frames < 0) {
            frames = snd_pcm_recover(capture, frames, 0);
        }
        if (frames < 0) {
            std::cerr << "Error during capture: " << snd_strerror(frames) << std::endl;
            break;
        }

        recPos = drum.write(recBuf, recPos, frames);
        std::cout << std::setw(10) << recBuf.power(frames);

        playPos = drum.read(playBuf, playPos, frames);
        frames = snd_pcm_writei(playback,
                                playBuf.begin(),
                                frames);
        if (frames < 0) {
            frames = snd_pcm_recover(playback, frames, 0);
        }
        if (frames < 0) {
            std::cerr << "Error during playback: " << snd_strerror(frames) << std::endl;
            break;
        }

        std::cout << std::setw(6) << playPos << " " << std::setw(6) << recPos;
        std::cout.flush();
    }

    snd_pcm_close(capture);
    snd_pcm_close(playback);
    return 0;
}
