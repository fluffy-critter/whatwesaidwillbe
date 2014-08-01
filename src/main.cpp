#include <GL/glew.h>
#include <GL/freeglut.h>

#include <iostream>
#include <stdexcept>

#include <boost/thread.hpp>

#include "Repeater.h"
#include "Visualizer.h"

namespace {
    // why doesn't freeglut add user data hooks? ugh
    Visualizer::Ptr vis;
}

void reshapeFunc(int x, int y) {
    std::cout << " reshape " << x << ',' << y << std::endl;
    vis->onResize(x, y);
}

void displayFunc() {
    vis->onDisplay();
    glutPostRedisplay();
}


int main(int argc, char *argv[]) try {
    int ret;
    
    Repeater::Ptr rr = std::make_shared<Repeater>();
    vis = std::make_shared<Visualizer>(rr);

    glutInitContextVersion(2, 0);
    glutInitContextFlags (GLUT_FORWARD_COMPATIBLE);
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_RGBA | GLUT_DOUBLE | GLUT_ACCUM | GLUT_DEPTH | GLUT_MULTISAMPLE);
    glutSetOption(GLUT_ACTION_ON_WINDOW_CLOSE, GLUT_ACTION_CONTINUE_EXECUTION);

    glutEnterGameMode();

    int err = glewInit();
    if (err != GLEW_OK) {
        std::cerr << "Couldn't initialize GLEW: " << glewGetErrorString(err) << std::endl;
        return 1;
    }

    glutReshapeFunc(reshapeFunc);
    glutDisplayFunc(displayFunc);

    vis->onInit();
    
    boost::thread audioThread([&]() {
            ret = rr->run(argc, argv);
        });

    std::cout << " entering GLUT main loop..." << std::endl;

    glutMainLoop();

    rr->shutdown();
    audioThread.join();
    return ret;
} catch (const std::exception& e) {
    std::cerr << "Error: " << e.what() << std::endl;
}

