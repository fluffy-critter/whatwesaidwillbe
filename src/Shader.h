#pragma once

#include <memory>

#include <GL/glew.h>

#include "Resource.h"

class Shader {
public:
    typedef std::shared_ptr<Shader> Ptr;

    Shader(GLuint type, const Resource&);
    ~Shader();

    GLuint handle() const { return mHandle; }

private:
    GLuint mHandle;
    GLuint mType;
    bool mCompiled;
};

