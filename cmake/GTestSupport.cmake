include(FetchContent)

# Pin GoogleTest for reproducible builds (do not track `main`).
set(BOWLINGBOX_GTEST_VERSION "v1.15.2" CACHE STRING "GoogleTest git tag")

FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG        ${BOWLINGBOX_GTEST_VERSION}
    GIT_SHALLOW    TRUE
)

set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)

# Third-party code is linked statically; BUILD_SHARED_LIBS stays ON for the
# Bowlingbox libraries, so restore it afterwards (as BulletSupport.cmake does).
set(_bowlingbox_gtest_saved_shared ${BUILD_SHARED_LIBS})
set(BUILD_SHARED_LIBS OFF)
FetchContent_MakeAvailable(googletest)
set(BUILD_SHARED_LIBS ${_bowlingbox_gtest_saved_shared})

include(GoogleTest)
