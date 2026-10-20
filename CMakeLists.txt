cmake_minimum_required(VERSION 3.16)
project(PsyTunnel LANGUAGES CXX)

find_package(PkgConfig REQUIRED)
pkg_check_modules(SDL2 REQUIRED IMPORTED_TARGET sdl2)
find_package(OpenGL REQUIRED)

# Shaderit pakataan ohjelman sisään: käynnistys onnistuu mistä hakemistosta tahansa.
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS tunnel.frag fullscreen.vert)
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/tunnel.frag" TUNNEL_FRAGMENT)
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/fullscreen.vert" TUNNEL_VERTEX)
configure_file(shader_sources.hpp.in generated/shader_sources.hpp @ONLY)

add_executable(psytunnel tunnel.cpp)
target_compile_features(psytunnel PRIVATE cxx_std_17)
target_compile_options(psytunnel PRIVATE -Wall -Wextra -Wpedantic)
target_include_directories(psytunnel PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/generated")
target_link_libraries(psytunnel PRIVATE PkgConfig::SDL2 OpenGL::GL)

include(CTest)
if(BUILD_TESTING)
    add_executable(tunnel-tests tunnel_tests.cpp)
    target_compile_features(tunnel-tests PRIVATE cxx_std_17)
    target_compile_options(tunnel-tests PRIVATE -Wall -Wextra -Wpedantic)
    add_test(NAME camera-and-palettes COMMAND tunnel-tests)
endif()
