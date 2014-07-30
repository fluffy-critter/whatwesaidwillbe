#include <cmath>
#include <stdexcept>

#include <boost/throw_exception.hpp>

#include "Buffer.h"

Buffer::Buffer(size_t samples, size_t channels): mChannels(channels),
                                                 mData(samples*channels)
{}

double Buffer::power(size_t count, size_t offset) const {
    double ttl = 0;
    for (const int16_t *iter = at(offset); iter != at(offset + count); ++iter) {
        double moment = *iter*1.0/32768;
        ttl += moment*moment;
    }

    return sqrt(ttl/count);
}

int Buffer::record(snd_pcm_t *source) {
    int frames = snd_pcm_readi(source, begin(), count());
    if (frames < 0) {
        frames = snd_pcm_recover(source, frames, 0);
    }
    if (frames < 0) {
        BOOST_THROW_EXCEPTION(std::runtime_error(snd_strerror(frames)));
    }
    return frames;
}

int Buffer::play(snd_pcm_t *dest, size_t n) const {
    int frames = snd_pcm_writei(dest, begin(), n);
    if (frames < 0) {
        frames = snd_pcm_recover(dest, frames, 0);
    }
    if (frames < 0) {
        BOOST_THROW_EXCEPTION(std::runtime_error(snd_strerror(frames)));
    }
    return frames;
}
