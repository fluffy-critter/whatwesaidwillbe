#include <boost/throw_exception.hpp>
#include <stdexcept>
#include <iostream>

#include "Shader.h"
#include "Resource.h"

Shader::Shader(GLuint type, const Resource& r) {
    mHandle = glCreateShader(type);
    const char *data = r.data();
    const int size = r.size();
    glShaderSource(mHandle, 1, &data, &size);
    glCompileShader(mHandle);

    GLint status;
    glGetShaderiv(mHandle, GL_COMPILE_STATUS, &status);
    if (status != GL_TRUE) {
        GLint logSize;
        glGetShaderiv(mHandle, GL_INFO_LOG_LENGTH, &logSize);
        std::string log(logSize, '\0');
        glGetShaderInfoLog(mHandle, log.size(), &logSize, &log[0]);
        log.resize(logSize);
        BOOST_THROW_EXCEPTION(std::runtime_error("Error compiling shader " + r.name()
                                                 + ": " + log));
    }
}

Shader::~Shader() {
    glDeleteShader(mHandle);
}
