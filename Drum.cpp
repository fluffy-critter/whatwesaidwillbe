#include <algorithm>
#include <stdexcept>

#include <boost/throw_exception.hpp>

#include "Drum.h"

Drum::Drum(size_t samples, size_t channels): mData(samples, channels)
{}

size_t Drum::write(const Buffer& buf, size_t offset, size_t count) {
    if (buf.channels() != mData.channels()) {
        BOOST_THROW_EXCEPTION(std::runtime_error("Mismatched channel count"));
    }
    count = std::min(count, buf.count());

    const size_t bufSz = mData.count();
    size_t start = offset % bufSz;

    const size_t first = std::min(count, bufSz - start);
    const size_t second = count - first;
    std::copy(buf.begin(), buf.at(first), mData.at(start));
    if (second) {
        std::copy(buf.at(first), buf.end(), mData.begin());
        return second;
    }
    return start + first;
}

size_t Drum::read(Buffer& buf, size_t offset, size_t count) const {
    if (buf.channels() != mData.channels()) {
        BOOST_THROW_EXCEPTION(std::runtime_error("Mismatched channel count"));
    }
    count = std::min(count, buf.count());

    const size_t bufSz = mData.count();
    size_t start = offset % bufSz;

    const size_t first = std::min(count, bufSz - start);
    const size_t second = count - first;
    std::copy(mData.at(start), mData.at(start + first), buf.begin());
    if (second) {
        std::copy(mData.begin(), mData.at(second), buf.at(first));
        return second;
    }
    return start + first;
}

size_t Drum::read(Buffer& buf, size_t offset, size_t count, int gain0, int gain1) const {
    if (buf.channels() != mData.channels()) {
        BOOST_THROW_EXCEPTION(std::runtime_error("Mismatched channel count"));
    }

    const size_t channels = buf.channels();
   
    int64_t curGain = gain0 << 16;
    int64_t gainStep = ((gain1 - gain0) << 16) / buf.count();

    size_t start = offset % mData.count();
    const int16_t *in = mData.at(start);
    int16_t *out = buf.begin();

    for (size_t i = 0; i < count; i++) {
        for (size_t k = 0; k < channels; k++) {
            int val = (*in++)*curGain >> 10;
            *out++ = std::max(-32768, std::min(32767, val));
        }
        curGain += gainStep;
        if (in == mData.end()) {
            in = mData.begin();
        }
    }
    return (start + count) % mData.count();
}

