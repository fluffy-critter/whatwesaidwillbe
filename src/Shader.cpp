#include "Shader.h"

Shader::Shader(GLuint type, const Resource& r): mType(type), mCompiled(false) {
    mHandle = glCreateShader(type);
    const char *data = r.data();
    const int size = r.size();
    glShaderSource(mHandle, 1, &data, &size);
    glCompileShader(mHandle);
}

Shader::~Shader() {
    glDeleteShader(mHandle);
}
