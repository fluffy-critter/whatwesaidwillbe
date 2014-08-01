#pragma once

class Resource {
public:
    Resource(const char *start, const char *end): mData(start),
                                                  mSize(end - start)
    {}

    const char * const &data() const { return mData; }
    const int &size() const { return mSize; }

    const char *begin() const { return mData; }
    const char *end() const { return mData + mSize; }

private:
    const char *mData;
    int mSize;
};

#define LOAD_RESOURCE(x) ([]() {                                    \
        extern const char _binary_##x##_start, _binary_##x##_end;   \
        return Resource(&_binary_##x##_start, &_binary_##x##_end);  \
    })()

