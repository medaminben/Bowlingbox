# Minimal project that uses an installed Bowlingbox the way an outside user would.
cmake_minimum_required(VERSION 3.29)
project(BowlingboxConsumer LANGUAGES CXX)

find_package(Bowlingbox 1.4 REQUIRED)

add_executable(consumer main.cpp)
target_compile_features(consumer PRIVATE cxx_std_20)
target_link_libraries(consumer PRIVATE Bowlingbox::Score)
