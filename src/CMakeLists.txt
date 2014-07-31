CMAKE_MINIMUM_REQUIRED(VERSION 2.6)
PROJECT(whatwesaidwillbe)

SET(CMAKE_CXX_FLAGS "-std=c++11")

FIND_PACKAGE(ALSA REQUIRED)
INCLUDE_DIRECTORIES(${ALSA_INCLUDE_DIR})

FIND_PACKAGE(Boost COMPONENTS program_options thread system REQUIRED)
INCLUDE_DIRECTORIES(${BOOST_INCLUDE_DIR})

FIND_PACKAGE(GLUT REQUIRED)
INCLUDE_DIRECTORIES(${GLUT_INCLUDE_DIR})
LINK_DIRECTORIES(${GLUT_LIBRARY_DIRS})
ADD_DEFINITIONS(${GLUT_DEFINITIONS})

FIND_PACKAGE(OpenGL REQUIRED)
INCLUDE_DIRECTORIES(${OpenGL_INCLUDE_DIRS})
LINK_DIRECTORIES(${OpenGL_LIBRARY_DIRS})
ADD_DEFINITIONS(${OpenGL_DEFINITIONS})

ADD_EXECUTABLE(whatwesaidwillbe
        main.cpp
        Buffer.cpp
        Calibrator.cpp 
        Drum.cpp
        Repeater.cpp        
)

TARGET_LINK_LIBRARIES(whatwesaidwillbe 
                                       ${ALSA_LIBRARIES}
                                       ${Boost_LIBRARIES}
                                       ${OpenGL_LIBRARIES}
                                       ${GLUT_LIBRARIES}
)

