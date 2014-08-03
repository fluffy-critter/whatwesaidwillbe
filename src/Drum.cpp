#include "Drum.h"

#include <boost/throw_exception.hpp>

#include <algorithm>
#include <stdexcept>

Drum::Drum(size_t samples, size_t channels): Buffer(NULL, samples, channels)
{}

size_t Drum::write(const Buffer& buf, size_t offset, size_t n) {
    if (buf.channels() != channels()) {
        BOOST_THROW_EXCEPTION(std::runtime_error("Mismatched channel count"));
    }
    n = std::min(n, buf.count());

    const size_t bufSz = count();
    size_t start = offset % bufSz;

    const size_t first = std::min(n, bufSz - start);
    const size_t second = n - first;
    std::copy(buf.begin(), buf.at(first), at(start));
    if (second) {
        std::copy(buf.at(first), buf.end(), begin());
        return second;
    }
    return start + first;
}

size_t Drum::read(Buffer& buf, ssize_t offset, size_t n) const {
    if (buf.channels() != channels()) {
        BOOST_THROW_EXCEPTION(std::runtime_error("Mismatched channel count"));
    }
    n = std::min(n, buf.count());

    const size_t bufSz = count();
    size_t start = (offset + bufSz) % bufSz;

    const size_t first = std::min(n, bufSz - start);
    const size_t second = n - first;
    std::copy(at(start), at(start + first), buf.begin());
    if (second) {
        std::copy(begin(), at(second), buf.at(first));
        return second;
    }
    return start + first;
}

size_t Drum::read(Buffer& buf, ssize_t offset, size_t n, float gain0, float gain1) const {
    if (buf.channels() != channels()) {
        BOOST_THROW_EXCEPTION(std::runtime_error("Mismatched channel count"));
    }

    const size_t channels = buf.channels();
   
    int64_t curGain = gain0*(1 << 24);
    int64_t gainStep = ((gain1 - gain0)*(1 << 24))/buf.count();

    size_t start = (offset + count()) % count();

    Buffer::const_iterator in = at(start);
    Buffer::iterator out = buf.begin();

    for (size_t i = 0; i < n; i++) {
        for (size_t k = 0; k < channels; k++) {
            int64_t val = (*in++)*curGain >> 24;
            *out++ = std::max(-32768L, std::min(32767L, val));
            if (in >= end()) {
                in = begin();
            }
        }
        curGain += gainStep;
    }

    return (start + n) % count();
}

float Drum::maxGain(size_t offset, size_t n) const {
    size_t start = offset % count();
    int16_t minVal, maxVal;

    Buffer::const_iterator rp = at(start);
    minVal = maxVal = *rp++;
    for (size_t i = 0; i < n; i++) {
        if (rp == end()) {
            rp = begin();
        }

        minVal = std::min(minVal, *rp);
        maxVal = std::max(maxVal, *rp);
        ++rp;
    }

    if (minVal || maxVal) {
        return 32768.0/std::max(abs(maxVal), abs(minVal));
    }
    return 0;
}
