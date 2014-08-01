#include <GL/glew.h>
#include <GL/freeglut.h>

#include "Resource.h"
#include "Visualizer.h"

namespace {
void checkError(int line) {
    GLenum err = glGetError();
    if (err) {
        std::cerr << "***ERROR*** " << line
                  << ": " << gluErrorString(err) << " ***ERROR*" << std::endl;
    }
}
}

#define ERRORCHECK() do {                       \
        checkError(__LINE__);                   \
    } while (0)


Visualizer::Visualizer(const Repeater::Ptr& rep): mRepeater(rep),
                                                  mWidth(0),
                                                  mHeight(0)
{}

void Visualizer::onInit() {
    Shader::Ptr rect = std::make_shared<Shader>(GL_VERTEX_SHADER,
                                                LOAD_RESOURCE(src_rect_vert));
    Shader::Ptr polar = std::make_shared<Shader>(GL_VERTEX_SHADER,
                                                 LOAD_RESOURCE(src_polar_vert));
    Shader::Ptr color = std::make_shared<Shader>(GL_FRAGMENT_SHADER,
                                                 LOAD_RESOURCE(src_color_frag));
    ERRORCHECK();

    mRoundShader = std::make_shared<ShaderProgram>();
    mRoundShader->attach(polar);
    mRoundShader->attach(color);
    ERRORCHECK();

    mSquareShader = std::make_shared<ShaderProgram>();
    mSquareShader->attach(rect);
    mSquareShader->attach(color);
    ERRORCHECK();
}

void Visualizer::onResize(int x, int y) {
    mWidth = x;
    mHeight = y;
}

void Visualizer::onDisplay() {
    glViewport(0, 0, mWidth, mHeight);

    glClearColor(1, 1, 1, 1);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(-mWidth*1.0/mHeight, mWidth*1.0/mHeight, -1, 1, -1, 1);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    mRepeater->getHistory(mHistory);
    
    mRoundShader->bind();
    ERRORCHECK();
    glColor4f(0, 0, 0, 1);

    typedef std::vector<float> Buf;
    const size_t count = mHistory.history.size();
    Buf loopBuf(2*count); //!< for a line loop
    {
        for (size_t i = 0; i < count; i++) {
            loopBuf[i*2] = i*2*M_PI/count;
        }
    }
    glEnableClientState(GL_VERTEX_ARRAY);
    glVertexPointer(2, GL_FLOAT, 0, loopBuf.data());

    {
        float maxVal = 0;
        std::vector<float>::iterator iter = loopBuf.begin() + 1;
        for (auto dp : mHistory.history) {
            *iter = dp.recordedPower + 8;
            maxVal = std::max(maxVal, *iter);
            iter += 2;
        }

        glPushMatrix();
        glScalef(1.0/maxVal, 1.0/maxVal, 1.0/maxVal);
        glDrawArrays(GL_LINE_LOOP, 0, count);
        glPopMatrix();
    }

    ERRORCHECK();

    glutSwapBuffers();
}

