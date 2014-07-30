#include <cmath>
#include <stdexcept>

#include <boost/throw_exception.hpp>

#include "Buffer.h"

Buffer::Buffer(snd_pcm_t* pipe,
               size_t samples,
               size_t channels):
    mPipe(pipe),
    mChannels(channels),
    mData(samples*channels)
{}

double Buffer::power(size_t count, size_t offset) const {
    double ttl = 0;
    for (const_iterator iter = at(offset); iter != at(offset + count); ++iter) {
        double moment = *iter*1.0/32768;
        ttl += moment*moment;
    }

    return sqrt(ttl/count);
}

int Buffer::record() {
    int frames = snd_pcm_readi(mPipe, &*begin(), count());
    if (frames < 0) {
        frames = snd_pcm_recover(mPipe, frames, 0);
    }
    if (frames < 0) {
        BOOST_THROW_EXCEPTION(std::runtime_error(snd_strerror(frames)));
    }
    return frames;
}

int Buffer::play(size_t n) const {
    int frames = snd_pcm_writei(mPipe, &*begin(), n);
    if (frames < 0) {
        frames = snd_pcm_recover(mPipe, frames, 0);
    }
    if (frames < 0) {
        BOOST_THROW_EXCEPTION(std::runtime_error(snd_strerror(frames)));
    }
    return frames;
}
