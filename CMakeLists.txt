cmake_minimum_required(VERSION 3.15)
project(algine_examples)

set(CMAKE_CXX_STANDARD 14)

add_subdirectory(lib/algine)
include_directories(lib/algine/include)
include_directories(lib/algine/contrib/tulz/include)
include_directories(lib/algine/contrib/glm)
include_directories(lib/algine/contrib/glew/include)
include_directories(lib/algine/contrib)

if (USE_SYSTEM_GLFW)
    find_package(glfw3 REQUIRED)
    message(STATUS "Algine examples: use system GLFW")
else()
    set(GLFW_BUILD_DOCS OFF)
    set(GLFW_BUILD_EXAMPLES OFF)
    set(GLFW_BUILD_TESTS OFF)

    add_subdirectory(lib/glfw)
    include_directories(lib/glfw/include)
endif()

add_executable(examples src/main.cpp src/ColorShader.h src/BlendShader.h src/constants.h)

target_link_libraries(examples algine glfw pthread)

if (WIN32)
    if (NOT ALGINE_LINK_LIBS_STATICALLY)
        if (CMAKE_BUILD_TYPE MATCHES Debug)
            set(libalgine libalgined.dll)
            set(libassimp libassimpd.dll)
            set(libtulz libtulzd.dll)
            set(libglew glew32d.dll)
            set(libglfw glfw3d.dll)
        else() # if not Debug or Release CMake must throw an error in the very beginning
            set(libalgine libalgine.dll)
            set(libassimp libassimp.dll)
            set(libtulz libtulz.dll)
            set(libglew glew32.dll)
            set(libglfw glfw3.dll)
        endif()

        add_custom_command(TARGET algine_examples POST_BUILD
                COMMENT "Creating symlinks to libs..."
                # algine
                COMMAND ${CMAKE_COMMAND} -E create_symlink
                    ${CMAKE_CURRENT_BINARY_DIR}/lib/algine/${libalgine}
                    ${CMAKE_CURRENT_BINARY_DIR}/${libalgine}
                # assimp
                COMMAND ${CMAKE_COMMAND} -E create_symlink
                    ${CMAKE_CURRENT_BINARY_DIR}/lib/algine/contrib/assimp/code/${libassimp}
                    ${CMAKE_CURRENT_BINARY_DIR}/${libassimp}
                # tulz
                COMMAND ${CMAKE_COMMAND} -E create_symlink
                    ${CMAKE_CURRENT_BINARY_DIR}/lib/algine/contrib/tulz/${libtulz}
                    ${CMAKE_CURRENT_BINARY_DIR}/${libtulz}
                # glew
                COMMAND ${CMAKE_COMMAND} -E create_symlink
                    ${CMAKE_CURRENT_BINARY_DIR}/bin/${libglew}
                    ${CMAKE_CURRENT_BINARY_DIR}/${libglew}
                # glfw
                COMMAND ${CMAKE_COMMAND} -E create_symlink
                    ${CMAKE_CURRENT_BINARY_DIR}/lib/glfw/src/${libglfw}
                    ${CMAKE_CURRENT_BINARY_DIR}/${libglfw})
    endif()
endif()

add_executable(ProjectFilesGenerator src/ProjectFilesGenerator.cpp src/constants.h)

target_link_libraries(ProjectFilesGenerator algine)
