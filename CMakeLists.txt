cmake_minimum_required(VERSION 3.20)

project(MotifForgeSeed VERSION 0.5.0 LANGUAGES C CXX)

find_program(MAKE_EXECUTABLE make REQUIRED)

add_custom_target(MotifForgeSeed ALL
    COMMAND ${MAKE_EXECUTABLE} -C ${CMAKE_CURRENT_SOURCE_DIR}/src/plugin/dpf jack
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMMENT "Building MotifForge Seed DPF JACK standalone with NanoVG/OpenGL3"
    VERBATIM
)
