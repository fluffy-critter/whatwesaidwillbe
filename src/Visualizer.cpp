#include <GL/glew.h>
#include <GL/freeglut.h>

#include "Visualizer.h"

Visualizer::Visualizer(const Repeater::Ptr& rep): mRepeater(rep),
                                                  mWidth(0),
                                                  mHeight(0)
{}

void Visualizer::onResize(int x, int y) {
    mWidth = x;
    mHeight = y;
}

void Visualizer::onDisplay() {
    glViewport(0, 0, mWidth, mHeight);

    glClearColor(1, 1, 1, 1);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glColor4f(0,0,0,1);
    glRasterPos2f(0, 0);
    glutBitmapString(GLUT_BITMAP_9_BY_15, (const unsigned char *)"This is a test");

    glutSwapBuffers();
}

