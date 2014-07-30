#include <cmath>
#include "Buffer.h"

Buffer::Buffer(size_t samples, size_t channels): mChannels(channels),
                                                 mData(samples*channels)
{}

double Buffer::power(size_t count) const {
    double ttl = 0;
    for (const int16_t *iter = begin(); iter != at(count); ++iter) {
        double moment = *iter*1.0/32768;
        ttl += moment*moment;
    }

    return sqrt(ttl/count);
}
