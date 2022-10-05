cmake_minimum_required(VERSION 3.23)
add_subdirectory(another-toml-cpp)

project(toml-test VERSION 0.1)

add_executable(toml-test encoder_decoder.cpp ./jute/jute.cpp)
set_property(TARGET toml-test PROPERTY CXX_STANDARD 20)

target_include_directories(toml-test PUBLIC ./jute ./SimpleJSON)
target_link_libraries(toml-test another-toml-cpp)

set_property(DIRECTORY PROPERTY VS_STARTUP_PROJECT ortho)