#include <cmath>
#include "Buffer.h"

Buffer::Buffer(size_t samples, size_t channels): mChannels(channels),
                                                 mData(samples*channels)
{}

uint64_t Buffer::power(size_t count) const {
    uint64_t ttl = 0;
    const int16_t *read = at(1);
    while (read != end()) {
        int16_t moment = *read - *(read - mChannels);
        ttl += moment*moment;
        ++read;
    }

    return sqrt(ttl);
}
