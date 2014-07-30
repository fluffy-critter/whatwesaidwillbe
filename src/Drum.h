#pragma once

#include "Buffer.h"

class Drum {
public:
    Drum(size_t samples, size_t channels);

    size_t count() const { return mData.count(); }

    /*! @brief Write from a buffer
     *
     *  @param buf The buffer
     *  @param offset Where to start writing
     *  @param count The number of samples to write
     *  @returns next write position
     */
    size_t write(const Buffer& buf, size_t offset, size_t count);

    /*! @brief Read into a buffer
     *
     *  @param buf The buffer
     *  @param offset Where to read from
     *  @param count The number of samples to read
     *  @returns next read position
     */
    size_t read(Buffer& buf, ssize_t offset, size_t count) const;

    /*! @brief read into a buffer, with gain attenuation
     *
     *  @param gain0 Start gain value
     *  @param gain1 End gain value
     */
    size_t read(Buffer& buf, ssize_t offset, size_t count, float gain0, float gain1) const;

    //! Get the maximum allowable gain for a segment
    float maxGain(size_t offset, size_t count) const;

private:
    Buffer mData;
};
