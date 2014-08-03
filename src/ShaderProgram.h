#pragma once

#include "Shader.h"

#include <GL/glew.h>

#include <memory>
#include <set>

class ShaderProgram {
public:
    typedef std::shared_ptr<ShaderProgram> Ptr;

    ShaderProgram();
    ~ShaderProgram();

    void attach(const Shader::Ptr&);

    GLuint bind();

private:
    GLuint mHandle;
    bool mLinked;

    std::set<Shader::Ptr> mAttached;
};
