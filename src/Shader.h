#pragma once

#include <GL/glew.h>

#include <memory>

class Resource;

class Shader {
public:
    typedef std::shared_ptr<Shader> Ptr;

    Shader(GLuint type, const Resource&);
    ~Shader();

    GLuint handle() const { return mHandle; }

private:
    GLuint mHandle;
};

