cmake_minimum_required(VERSION 3.28)

project(xmole2_package_consumer LANGUAGES CXX)

find_package(xmole2 CONFIG REQUIRED COMPONENTS zip cfb)

add_executable(xmole2_package_consumer main.cpp)
target_link_libraries(xmole2_package_consumer PRIVATE xmole2::zip xmole2::cfb)
target_compile_features(xmole2_package_consumer PRIVATE cxx_std_23)
