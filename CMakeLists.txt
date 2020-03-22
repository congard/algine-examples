cmake_minimum_required(VERSION 3.15)
project(algine_examples)

set(CMAKE_CXX_STANDARD 14)

add_subdirectory(lib/algine)
include_directories(lib/algine/include)
include_directories(lib/algine/contrib/tulz/include)
include_directories(lib/algine/contrib/glm)
include_directories(lib/algine/contrib/glew/include)

if (ALGINE_USE_SYSTEM_GLFW)
    find_package(glfw3 REQUIRED)
    message(STATUS "Algine: use system GLFW")
else()
    set(GLFW_BUILD_DOCS OFF)
    set(GLFW_BUILD_EXAMPLES OFF)
    set(GLFW_BUILD_TESTS OFF)

    add_subdirectory(lib/glfw)
    include_directories(lib/glfw/include)
endif()

add_executable(algine_examples src/main.cpp)

# TODO: add Windows support
target_link_libraries(algine_examples algine assimp ${GLEW_LIBRARY} glfw GL pthread tulz)