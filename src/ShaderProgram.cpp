#include <GL/glew.h>

#include "ShaderProgram.h"

ShaderProgram::ShaderProgram(): mLinked(false) {
    mHandle = glCreateProgram();
}

ShaderProgram::~ShaderProgram() {
    glDeleteProgram(mHandle);
}

void ShaderProgram::attach(const Shader::Ptr& s) {
    glAttachShader(mHandle, s->handle());
    mAttached.insert(s);
}

GLuint ShaderProgram::bind() {
    if (!mLinked) {
        glLinkProgram(mHandle);
        mLinked = true;
    }
    glUseProgram(mHandle);
    return mHandle;
}
