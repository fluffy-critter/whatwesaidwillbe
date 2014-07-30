CMAKE_MINIMUM_REQUIRED(VERSION 2.6)
PROJECT(whatwesaidwillbe)

SET(CMAKE_CXX_FLAGS "-std=c++11")

ADD_EXECUTABLE(whatwesaidwillbe main.cpp Buffer.cpp Drum.cpp)

FIND_PACKAGE(ALSA)
INCLUDE_DIRECTORIES( ${ALSA_INCLUDE_DIR} )


FIND_PACKAGE(Boost COMPONENTS program_options REQUIRED)
INCLUDE_DIRECTORIES( ${BOOST_INCLUDE_DIR} )

TARGET_LINK_LIBRARIES(whatwesaidwillbe 
                                       ${ALSA_LIBRARIES}
                                       ${Boost_LIBRARIES})

