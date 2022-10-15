cmake_minimum_required(VERSION 3.23)
add_subdirectory(another-toml-cpp)

project(toml-test VERSION 0.1)

add_executable(toml-test-encoder encoder.cpp)
set_property(TARGET toml-test-encoder PROPERTY CXX_STANDARD 20)

target_include_directories(toml-test-encoder PUBLIC ./SimpleJSON)
target_link_libraries(toml-test-encoder another-toml-cpp)

add_executable(toml-test-decoder decoder.cpp)
set_property(TARGET toml-test-decoder PROPERTY CXX_STANDARD 20)

target_include_directories(toml-test-decoder PUBLIC ./SimpleJSON)
target_link_libraries(toml-test-decoder another-toml-cpp)


set_property(DIRECTORY PROPERTY VS_STARTUP_PROJECT toml-test-encoder)