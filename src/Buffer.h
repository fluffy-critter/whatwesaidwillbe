#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>
#include <alsa/asoundlib.h>

class Buffer {
public:
    typedef std::vector<int16_t> Storage;
    typedef Storage::iterator iterator;
    typedef Storage::const_iterator const_iterator;

    Buffer(snd_pcm_t* pipe, size_t samples, size_t channels);

    //! The number of samples
    size_t count() const { return mData.size() / mChannels; }
    //! The number of channels per sample
    size_t channels() const { return mChannels; }

    //! First sample
    const_iterator begin() const { return mData.begin(); }
    //! One past the last sample
    const_iterator end() const { return mData.end(); }
    //! Specific sample
    const_iterator at(size_t ofs) const { return begin() + mChannels*ofs; }

    //! First sample
    iterator begin() { return mData.begin(); }
    //! One past the last sample
    iterator end() { return mData.end(); }
    //! Specific sample
    iterator at(size_t ofs) { return begin() + mChannels*ofs; }

    //! Current stored power level
    double power(size_t count, size_t offset = 0) const;

    int record();
    int play(size_t count) const;

private:
    snd_pcm_t *mPipe;
    size_t mChannels;
    std::vector<int16_t> mData;
};
