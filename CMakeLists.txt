project(json11)

cmake_minimum_required(VERSION 2.6)

add_compile_options("-std=c++11")

add_library(json11
    json11.hpp
    json11.cpp
)

add_executable(json11-test
    test.cpp
    json11.hpp
    json11.cpp
)

add_custom_command(
    TARGET json11-test
    POST_BUILD
    COMMAND json11-test
    WORKING_DIRECTORY ${json11_SOURCE_DIR}
)
add_custom_target(check
    COMMAND json11-test
    WORKING_DIRECTORY ${json11_SOURCE_DIR}
)
