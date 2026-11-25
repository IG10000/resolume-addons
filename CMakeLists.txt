cmake_minimum_required(VERSION 3.16)
project(IMMINENCE VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

set(FFGL_DIR ${CMAKE_CURRENT_SOURCE_DIR}/external/ffgl)

# Everything shared by the plugins is compiled once as an object library and linked into every plugin, so each
# plugin file is fully self-contained (it carries its own copy of the FFGL entry point plugMain).
file(GLOB FFGL_SOURCES
    ${FFGL_DIR}/source/lib/ffgl/*.cpp
    ${FFGL_DIR}/source/lib/ffglex/*.cpp)

add_library(imcore OBJECT
    ${FFGL_SOURCES}
    src/core/ImPlugin.cpp
    src/core/ImLicense.cpp
    src/core/ImText.cpp)

target_include_directories(imcore PUBLIC
    ${FFGL_DIR}/source/lib
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/external/stb)

if(WIN32)
    target_include_directories(imcore PUBLIC ${FFGL_DIR}/deps/glew-2.1.0/include)
    target_compile_definitions(imcore PUBLIC GLEW_STATIC _CRT_SECURE_NO_WARNINGS NOMINMAX WIN32_LEAN_AND_MEAN)
    if(MSVC)
        target_compile_options(imcore PUBLIC /W3 /MP /utf-8)
        # one runtime for everything, and no dependency on a Visual C++ redistributable on the show machine
        set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
        set_property(TARGET imcore PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    endif()
    set(IM_GL_LIBS ${FFGL_DIR}/deps/glew-2.1.0/lib/Release/x64/glew32s.lib opengl32)
else()
    find_package(OpenGL REQUIRED)
    find_package(GLEW REQUIRED)
    target_link_libraries(imcore PUBLIC GLEW::GLEW OpenGL::GL)
    set(IM_GL_LIBS GLEW::GLEW OpenGL::GL)
endif()

include(src/generated/plugins.cmake)

foreach(plugin ${IM_PLUGINS})
    add_library(${plugin} MODULE src/plugins/${plugin}.cpp $<TARGET_OBJECTS:imcore>)
    target_include_directories(${plugin} PRIVATE $<TARGET_PROPERTY:imcore,INTERFACE_INCLUDE_DIRECTORIES>)
    target_compile_definitions(${plugin} PRIVATE $<TARGET_PROPERTY:imcore,INTERFACE_COMPILE_DEFINITIONS>)
    target_link_libraries(${plugin} PRIVATE ${IM_GL_LIBS})
    set_target_properties(${plugin} PROPERTIES PREFIX "" OUTPUT_NAME "IMMINENCE_${plugin}")
    if(MSVC)
        set_property(TARGET ${plugin} PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
        target_compile_options(${plugin} PRIVATE /utf-8)
    endif()
endforeach()

# Linux-only test host used to render every style without Arena (see tools/host).
if(NOT WIN32 AND IM_BUILD_TEST_HOST)
    find_package(glfw3 REQUIRED)
    add_executable(imhost tools/host/imhost.cpp)
    target_include_directories(imhost PRIVATE ${FFGL_DIR}/source/lib ${CMAKE_CURRENT_SOURCE_DIR}/external/stb)
    target_link_libraries(imhost PRIVATE glfw GLEW::GLEW OpenGL::GL ${CMAKE_DL_LIBS})
endif()
