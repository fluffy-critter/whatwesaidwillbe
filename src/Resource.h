#pragma once

#include <string>

class Resource {
public:
    Resource(const std::string& name,
             const char *start, const char *end): mName(name),
                                                  mData(start),
                                                  mSize(end - start)
    {}

    const std::string& name() const { return mName; }

    const char * const &data() const { return mData; }
    const int &size() const { return mSize; }

    const char *begin() const { return mData; }
    const char *end() const { return mData + mSize; }

private:
    std::string mName;
    const char *mData;
    int mSize;
};

#define LOAD_RESOURCE(x) ([]() {                                    \
        extern const char _binary_##x##_start, _binary_##x##_end;   \
        return Resource(#x, &_binary_##x##_start, &_binary_##x##_end); \
    })()

