#pragma once

#include <memory>
#include <set>

#include <GL/glew.h>

#include "Shader.h"

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
