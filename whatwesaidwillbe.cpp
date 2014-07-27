#include <vector>
#include <boost/program_options.hpp>
#include <alsa/asoundlib.h>
#include <stdint.h>

namespace po = boost::program_options;

static const int FP_SHIFT = 14;

struct Buffer {
    typedef std::vector<int16_t> Storage;
    Storage data;
    size_t frames;
    int minVal, maxVal;

    Buffer(size_t size): data(size, 0), frames(0) {}
};

int main(int argc, char *argv[]) {
	int target = 1024;
	int dampen = 900;
	int rate = 44100;
	int bufSize = 2048;
    int channels = 2;
    float loopDelay = 10.0;
	std::string captureDevice = "default";
	std::string playbackDevice = "default";

	po::options_description desc("General options");
	desc.add_options()
        ("help,h", "show this help")
		("target,t", po::value<int>(&target)->default_value(target), "target feedback level (1024 = 100%)")
		("dampen,d", po::value<int>(&dampen)->default_value(dampen), "dampening factor (0-1024)")
		("rate,r", po::value<int>(&rate)->default_value(rate), "sampling rate")
		("bufSize,k", po::value<int>(&bufSize)->default_value(bufSize), "buffer size")
		("loopDelay,c", po::value<float>(&loopDelay)->default_value(loopDelay), "loop delay, in seconds")
		("capture", po::value<std::string>(&captureDevice)->default_value(captureDevice), "ALSA capture device")
		("playback", po::value<std::string>(&playbackDevice)->default_value(playbackDevice), "ALSA playback device")
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

    const int bufCount = rate*loopDelay/bufSize;

	snd_pcm_t *capture, *playback;

	int err;
	if ((err = snd_pcm_open(&capture, captureDevice.c_str(), SND_PCM_STREAM_CAPTURE, 0)) < 0) {
		std::cerr << "Couldn't open " << captureDevice << " for capture: " << snd_strerror(err) << std::endl;
		return 1;
	}

	if ((err = snd_pcm_set_params(capture,
                                  SND_PCM_FORMAT_S16, SND_PCM_ACCESS_RW_INTERLEAVED,
                                  channels, rate, 1, 100000)) < 0) {
		std::cerr << "Couldn't configure " << captureDevice << " for capture: " << snd_strerror(err) << std::endl;
        return 1;
    }

	if ((err = snd_pcm_open(&playback, captureDevice.c_str(), SND_PCM_STREAM_PLAYBACK, 0)) < 0) {
		std::cerr << "Couldn't open " << captureDevice << " for capture: " << snd_strerror(err) << std::endl;
		return 1;
	}

	if ((err = snd_pcm_set_params(playback,
                                  SND_PCM_FORMAT_S16, SND_PCM_ACCESS_RW_INTERLEAVED,
                                  channels, rate, 1, 100000)) < 0) {
		std::cerr << "Couldn't configure " << captureDevice << " for capture: " << snd_strerror(err) << std::endl;
        return 1;
    }

    std::vector<Buffer> buffers(bufCount, Buffer(bufSize*channels));
    Buffer outBuf(bufSize*channels);

    size_t recPos = 0;

    int curGain = 1 << FP_SHIFT, tgtGain = 1 << FP_SHIFT;

    for (;;) {
        Buffer &playBuf = buffers[(recPos + 1) % bufCount];
        if (playBuf.frames) {
            int maxGain = std::min((32767 << FP_SHIFT)/playBuf.maxVal,
                                   (-32768 << FP_SHIFT)/playBuf.minVal);

            tgtGain = std::min(tgtGain, maxGain);

            int startGain = curGain;
            int gainStep = (tgtGain - curGain)/playBuf.frames/channels;

            for (size_t i = 0; i < playBuf.frames*channels; i++) {
                outBuf.data[i] = std::max(-32768, std::min(32767, (playBuf.data[i]*curGain) >> FP_SHIFT));
                curGain += gainStep;
            }

            int frames;
            if ((frames = snd_pcm_writei(playback, &outBuf.data.front(), playBuf.frames)) < 0) {
                std::cerr << std::endl << "Warning: playback: " << snd_strerror(frames) << std::endl;
                frames = snd_pcm_recover(playback, frames, 0);
                if (frames < 0) {
                    std::cerr << "Couldn't recover playback" << std::endl;
                    break;
                }
            } else {
                std::cerr << "played " << playBuf.frames
                          << " frames, gain = "
                          << startGain << '+' << gainStep << " -> " << curGain
                          << " (wanted " << tgtGain << "); ";
            }
            
        }

        std::cerr << "recording; ";
        Buffer &recBuf = buffers[recPos];
        int frames = snd_pcm_readi(capture, &recBuf.data.front(), bufSize);
        if (frames < 0) {
            frames = snd_pcm_recover(capture, frames, 0);
        }
        if (frames < 0) {
            std::cerr << "Error: capture: " << snd_strerror(frames) << std::endl;
            break;
        }
        std::cerr << "recorded " << frames << " frames to position " << recPos;
        recBuf.frames = frames;

        int16_t minVal = -1, maxVal = 1;
        for (size_t i = 0; i < recBuf.frames*channels; i++) {
            minVal = std::min(minVal, recBuf.data[i]);
            maxVal = std::max(maxVal, recBuf.data[i]);
        }
        recBuf.minVal = minVal;
        recBuf.maxVal = maxVal;

        int matchMax = std::max(1, (playBuf.maxVal << FP_SHIFT)/recBuf.maxVal*target/1024);
        int matchMin = std::max(1, (playBuf.minVal << FP_SHIFT)/recBuf.minVal*target/1024);

        int nextGain = (matchMax + matchMin)/2;
        tgtGain = (tgtGain*dampen + nextGain*(1024 - dampen))/1024;

        recPos = (recPos + 1) % bufCount;
        std::cerr << "  target gain = " << nextGain << " -> " << tgtGain << "        \r";
    }

    snd_pcm_close(capture);
    snd_pcm_close(playback);
    return 0;
}
