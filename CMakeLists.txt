cmake_minimum_required(VERSION 3.10)
project(cosc422_ass2)
set(CMAKE_CXX_STANDARD 11)

# Dependencies
find_package(GLUT REQUIRED)
include_directories(${GLUT_INCLUDE_DIRS})
link_directories(${GLUT_LIBRARY_DIRS})
add_definitions(${GLUT_DEFINITIONS})
if(NOT GLUT_FOUND)
    message(ERROR " GLUT not found!")
endif(NOT GLUT_FOUND)

find_package(OpenGL REQUIRED)
include_directories(${OpenGL_INCLUDE_DIRS})
link_directories(${OpenGL_LIBRARY_DIRS})
add_definitions(${OpenGL_DEFINITIONS})
if(NOT OPENGL_FOUND)
    message(ERROR " OPENGL not found!")
endif(NOT OPENGL_FOUND)

find_package(GLEW REQUIRED)
include_directories(${GLEW_INCLUDE_DIRS})
link_libraries(${GLEW_LIBRARIES})
if (NOT GLEW_FOUND)
    message(ERROR " GLEW not found!")
endif(NOT GLEW_FOUND)

find_package(assimp REQUIRED)
if (assimp_FOUND)
    link_directories(${ASSIMP_LIBRARY_DIRS})
    include_directories(${ASSIMP_INCLUDE_DIRS})
endif(assimp_FOUND)

find_package(DevIL REQUIRED)
if (DevIL_FOUND)
    link_directories(${DevIL_LIBRARY_DIRS})
    include_directories(${DevIL_INCLUDE_DIRS})
endif(DevIL_FOUND)

add_executable(task1 ModelViewer.cpp)

# Link all dependencies
target_link_libraries(task1 ${OPENGL_LIBRARIES} ${GLUT_LIBRARY} ${GLEW_LIBRARY} ${ASSIMP_LIBRARIES} ${IL_LIBRARIES})

# Copy resources into binary directory
file(COPY models DESTINATION ${CMAKE_BINARY_DIR})
