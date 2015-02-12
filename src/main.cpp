#include <iostream>
#include <stdexcept>

#include <boost/program_options.hpp>
#include <thread>

#include <alsa/asoundlib.h>
#include <GL/glew.h>
#include <GL/freeglut.h>

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
    default:
        vis->onKeyboard(key);
    }
}

void specialFunc(int key, int, int) {
    vis->onSpecialKey(key);
}

void reshapeFunc(int x, int y) {
    std::cout << " reshape " << x << ',' << y << std::endl;
    vis->onResize(x, y);
}

void displayFunc() {
    if (vis->onDisplay()) {
	glutLeaveMainLoop();
    }
    glutPostRedisplay();
}

}

int main(int argc, char *argv[]) try {
    glutInitContextVersion(2, 0);
    glutInitContextFlags (GLUT_FORWARD_COMPATIBLE);
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_RGB | GLUT_DOUBLE | GLUT_MULTISAMPLE);
    glutSetOption(GLUT_ACTION_ON_WINDOW_CLOSE, GLUT_ACTION_CONTINUE_EXECUTION);

    Repeater::Options opts;
    Repeater::Knobs knobs;
    bool fullScreen = true;

    {
        namespace po = boost::program_options;

        std::string initMode;

        po::options_description desc("General options");
        desc.add_options()
            ("help,h", "show this help")
            ("list-devices", "list devices and exit")

            ("rate,r", po::value<unsigned int>(&opts.sampleRate)->default_value(opts.sampleRate), "sampling rate")
            ("bufSize,k", po::value<size_t>(&opts.bufSize)->default_value(opts.bufSize), "buffer size")
            ("historySize,H", po::value<size_t>(&opts.historySize)->default_value(opts.historySize),
             "Size of the history buffer")
            ("loopDelay,c", po::value<double>(&opts.loopDelay)->default_value(opts.loopDelay),
             "loop delay, in seconds")
            ("latency,q", po::value<int>(&opts.latencyALSA)->default_value(opts.latencyALSA),
             "ALSA latency, in microseconds")
            ("capture", po::value<std::string>(&opts.captureDevice)->default_value(opts.captureDevice),
             "ALSA capture device")
            ("playback", po::value<std::string>(&opts.playbackDevice)->default_value(opts.playbackDevice),
             "ALSA playback device")
            ("recDump", po::value<std::string>(&opts.recDumpFile), "Recording dump file (raw PCM)")
            ("listenDump", po::value<std::string>(&opts.listenDumpFile), "Play dump file (raw PCM)")
            
            ("dampen,d", po::value<double>(&knobs.dampen)->default_value(knobs.dampen),
             "dampening factor")
            ("feedThresh,F", po::value<double>(&knobs.feedbackThreshold),
             "feedback adjustment threshold; < 0 = autodetect at startup")
            ("limiter,L", po::value<double>(&knobs.limitPower)->default_value(knobs.limitPower), "power limiter")
            ("mode,m", po::value<std::string>(&initMode)->default_value("gain"),
             "initial volume model (gain, feedback, target)")
            ("feedback,f", po::value<double>(&knobs.levels[Repeater::M_FEEDBACK])
             ->default_value(knobs.levels[Repeater::M_FEEDBACK]),
             "feedback factor")
            ("target,t", po::value<double>(&knobs.levels[Repeater::M_TARGET])
             ->default_value(knobs.levels[Repeater::M_TARGET]),
             "target power level")
            ("gain,g", po::value<double>(&knobs.levels[Repeater::M_GAIN])
             ->default_value(knobs.levels[Repeater::M_GAIN]),
             "ordinary gain")
            ("fullscreen,S", po::value<bool>(&fullScreen)->default_value(fullScreen),
             "fullscreen mode")
            ;

        po::variables_map vm;
        po::store(po::command_line_parser(argc, argv).options(desc).run(), vm);

        if (vm.count("help")) {
            std::cerr << desc << std::endl;
            return 1;
        }

        if (vm.count("list")) {
            char **hints;
            int err;
            if ((err = snd_device_name_hint(-1, "pcm", (void***)&hints))) {
                std::cerr << "Couldn't get device list: " << snd_strerror(err) << std::endl;
                return 1;
            }

            while (*hints) {
                std::cout << "Name: " << snd_device_name_get_hint(*hints, "NAME") << std::endl
                          << "Desc: " << snd_device_name_get_hint(*hints, "DESC") << std::endl
                          << std::endl;
                ++hints;
            }
            return 0;
        }

        po::notify(vm);

        if (initMode == "gain") {
            knobs.mode = Repeater::M_GAIN;
        } else if (initMode == "feedback") {
            knobs.mode = Repeater::M_FEEDBACK;
        } else if (initMode== "target") {
            knobs.mode = Repeater::M_TARGET;
        } else {
            std::cerr << "Unknown volume model '" << initMode << "'" << std::endl;
            return 1;
        }
    }

    int ret;
    
    rr = std::make_shared<Repeater>(opts, knobs);
    vis = std::make_shared<Visualizer>(rr);

    if (fullScreen) {
        glutEnterGameMode();
        glutSetCursor(GLUT_CURSOR_NONE);
    } else {
        glutCreateWindow("whatwesaidwillbe");
    }

    int err = glewInit();
    if (err != GLEW_OK) {
        std::cerr << "Couldn't initialize GLEW: " << glewGetErrorString(err) << std::endl;
        return 1;
    }

    glutReshapeFunc(reshapeFunc);
    glutDisplayFunc(displayFunc);
    glutKeyboardFunc(keyboardFunc);
    glutSpecialFunc(specialFunc);

    vis->onInit();
    
    std::thread audioThread(
        [&]() {
            try {
                ret = rr->run();
            } catch (const std::exception& e) {
                std::cerr << "Audio thread: " << e.what() << std::endl;
                ret = 1;
            }
        });

    std::cout << "entering GLUT main loop..." << std::endl;

    glutMainLoop();

    std::cout << "awaiting shutdown..." << std::endl;
    audioThread.join();
    return ret;
} catch (const std::exception& e) {
    std::cerr << "Error: " << e.what() << std::endl;
}

