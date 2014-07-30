cmake_minimum_required (VERSION 2.6)
project (whatwesaidwillbe)

SET(CMAKE_CXX_FLAGS "-std=c++11")

add_executable(whatwesaidwillbe main.cpp Buffer.cpp Drum.cpp)

find_package(ALSA)
target_link_libraries(whatwesaidwillbe ${ALSA_LIBRARIES})

find_package(Boost COMPONENTS program_options REQUIRED)
target_link_libraries(whatwesaidwillbe ${Boost_LIBRARIES})

