cmake_minimum_required(VERSION 3.15)
project(algine_examples)

set(CMAKE_CXX_STANDARD 17)

if (NOT TARGET algine)
    add_subdirectory(lib/algine)
endif()

include_directories(lib/algine/include)
include_directories(lib/algine/contrib/tulz/include)
include_directories(lib/algine/contrib/glm)
include_directories(lib/algine/contrib/glew/include)
include_directories(lib/algine/contrib)

add_executable(examples
        src/Main.cpp
        src/ColorShader.h src/BlendShader.h src/constants.h
        src/ExampleChessContent.cpp src/ExampleChessContent.h
        src/LoopThread.cpp src/LoopThread.h
        src/LampMoveThread.cpp src/LampMoveThread.h)

if (NOT MSVC)
    set(EXAMPLES_LINK_LIBS pthread)
endif()

target_link_libraries(examples algine ${EXAMPLES_LINK_LIBS})

if (WIN32)
    algine_target_mklink_win(examples lib/algine)
endif()

add_executable(ProjectFilesGenerator src/ProjectFilesGenerator.cpp src/constants.h)

target_link_libraries(ProjectFilesGenerator algine)
