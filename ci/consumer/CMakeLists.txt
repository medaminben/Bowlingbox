# Minimal project that uses an installed Bowlingbox the way an outside user would.
cmake_minimum_required(VERSION 3.29)
project(BowlingboxConsumer LANGUAGES CXX)

# install-smoke.sh passes the version of the build tree it installed.
if(BOWLINGBOX_EXPECTED_VERSION)
    find_package(Bowlingbox ${BOWLINGBOX_EXPECTED_VERSION} EXACT REQUIRED)
else()
    find_package(Bowlingbox REQUIRED)
endif()

add_executable(consumer main.cpp)
target_compile_features(consumer PRIVATE cxx_std_20)
target_link_libraries(consumer PRIVATE Bowlingbox::Score)
