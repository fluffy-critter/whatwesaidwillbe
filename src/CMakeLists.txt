CMAKE_MINIMUM_REQUIRED(VERSION 2.6)
PROJECT(whatwesaidwillbe)

SET(CMAKE_CXX_FLAGS "-std=c++11 -g -Wall -Werror -flto")

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

FIND_PACKAGE(GLEW REQUIRED)
INCLUDE_DIRECTORIES(${GLEW_INCLUDE_DIRS})
LINK_DIRECTORIES(${GLEW_LIBRARY_DIRS})
ADD_DEFINITIONS(${GLEW_DEFINITIONS})


# stuff for shaders
# adapted from http://www.cmake.org/pipermail/cmake/2010-June/037733.html
# (why doesn't CMake allow custom per-project build rules?)
FUNCTION(ADD_RESOURCES out_var)
  SET(result)
  FOREACH(in_f ${ARGN})
    FILE(RELATIVE_PATH src_f ${CMAKE_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/${in_f})
    SET(out_f "${PROJECT_BINARY_DIR}/${in_f}.o")
    ADD_CUSTOM_COMMAND(OUTPUT ${out_f}
      COMMAND ld -r -b binary -o ${out_f} ${src_f}
      DEPENDS ${in_f}
      WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
      COMMENT "Building GLSL object ${out_f}"
      VERBATIM
      )
    LIST(APPEND result ${out_f})
  ENDFOREACH()
  SET(${out_var} "${result}" PARENT_SCOPE)
ENDFUNCTION()

ADD_RESOURCES(shaders
  rect.vert
  polar.vert
  color.frag
  )

ADD_EXECUTABLE(whatwesaidwillbe
  ${shaders}
  main.cpp
  Buffer.cpp
  Calibrator.cpp 
  Drum.cpp
  Repeater.cpp
  Shader.cpp
  ShaderProgram.cpp
  Visualizer.cpp
  )

TARGET_LINK_LIBRARIES(whatwesaidwillbe 
  ${ALSA_LIBRARIES}
  ${Boost_LIBRARIES}
  ${OPENGL_LIBRARIES}
  ${GLEW_LIBRARIES}
  ${GLUT_LIBRARIES}
  )

