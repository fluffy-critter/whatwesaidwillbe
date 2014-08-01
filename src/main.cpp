#include <GL/glew.h>
#include <GL/freeglut.h>

#include <iostream>
#include <stdexcept>

#include <boost/thread.hpp>

#include "Repeater.h"
#include "Visualizer.h"

namespace {
// why doesn't freeglut add user data hooks? ugh
Repeater::Ptr rr;
Visualizer::Ptr vis;

void keyboardFunc(unsigned char key, int, int) {
    switch (key) {
    case 27:
        rr->shutdown();
        break;
        //default:
        //vis->inputblah
    }
}

void reshapeFunc(int x, int y) {
    std::cout << " reshape " << x << ',' << y << std::endl;
    vis->onResize(x, y);
}

void displayFunc() {
    if (vis->onDisplay()) {
        glutLeaveMainLoop();
    } else {
        glutPostRedisplay();
    }
}

}

int main(int argc, char *argv[]) try {
    int ret;
    
    rr = std::make_shared<Repeater>();
    vis = std::make_shared<Visualizer>(rr);

    glutInitContextVersion(2, 0);
    glutInitContextFlags (GLUT_FORWARD_COMPATIBLE);
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_RGB | GLUT_DOUBLE | GLUT_MULTISAMPLE);
    glutSetOption(GLUT_ACTION_ON_WINDOW_CLOSE, GLUT_ACTION_CONTINUE_EXECUTION);

    glutEnterGameMode();

    int err = glewInit();
    if (err != GLEW_OK) {
        std::cerr << "Couldn't initialize GLEW: " << glewGetErrorString(err) << std::endl;
        return 1;
    }

    glutReshapeFunc(reshapeFunc);
    glutDisplayFunc(displayFunc);
    glutKeyboardFunc(keyboardFunc);

    vis->onInit();
    
    boost::thread audioThread([&]() {
            ret = rr->run(argc, argv);
        });

    std::cout << " entering GLUT main loop..." << std::endl;

    glutMainLoop();

    std::cout << "awaiting shutdown..." << std::endl;
    audioThread.join();
    return ret;
} catch (const std::exception& e) {
    std::cerr << "Error: " << e.what() << std::endl;
}

