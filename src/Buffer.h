#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>
#include <alsa/asoundlib.h>

class Buffer {
public:
    Buffer(size_t samples, size_t channels);

    //! The number of samples
    size_t count() const { return mData.size() / mChannels; }
    //! The number of channels per sample
    size_t channels() const { return mChannels; }

    //! First sample
    const int16_t *begin() const { return &mData.front(); }
    //! One past the last sample
    const int16_t *end() const { return &mData.back(); }
    //! Specific sample
    const int16_t *at(size_t ofs) const { return begin() + mChannels*ofs; }

    //! First sample
    int16_t *begin() { return &mData.front(); }
    //! One past the last sample
    int16_t *end() { return &mData.back(); }
    //! Specific sample
    int16_t *at(size_t ofs) { return begin() + mChannels*ofs; }

    //! Current stored power level
    double power(size_t count, size_t offset = 0) const;

    int record(snd_pcm_t *source);
    int play(snd_pcm_t *dest, size_t count) const;

private:
    size_t mChannels;
    std::vector<int16_t> mData;
};
