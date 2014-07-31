#include <iostream>
#include <stdexcept>

#include <boost/thread.hpp>

#include "Repeater.h"

int main(int argc, char *argv[]) try {
    int ret;
    Repeater rr;
    boost::thread audioThread([&]() {
            ret = rr.run(argc, argv);
        });

    audioThread.join();
    return ret;
} catch (const std::exception& e) {
    std::cerr << "Error: " << e.what() << std::endl;
}

