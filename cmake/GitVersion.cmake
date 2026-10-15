# Sets BOWLINGBOX_VERSION from the nearest vX.Y.Z git tag. Included before project().
#
# Tags are created by .github/workflows/version.yml each time a pull request is merged
# into main. Without git or without a tag (source archive, shallow clone) the fallback
# version below is used. Pass -DBOWLINGBOX_VERSION=X.Y.Z to set the version by hand.
set(BOWLINGBOX_FALLBACK_VERSION 1.4.0)

if(NOT BOWLINGBOX_VERSION)
    set(BOWLINGBOX_VERSION ${BOWLINGBOX_FALLBACK_VERSION})
    find_package(Git QUIET)
    if(GIT_FOUND)
        execute_process(
            COMMAND ${GIT_EXECUTABLE} describe --tags --abbrev=0 --match "v[0-9]*.[0-9]*.[0-9]*"
            WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}/..
            OUTPUT_VARIABLE git_tag
            OUTPUT_STRIP_TRAILING_WHITESPACE
            RESULT_VARIABLE git_result
            ERROR_QUIET
        )
        if(git_result EQUAL 0 AND git_tag MATCHES "^v([0-9]+\\.[0-9]+\\.[0-9]+)$")
            set(BOWLINGBOX_VERSION ${CMAKE_MATCH_1})
        endif()
    endif()
endif()

message(STATUS "Bowlingbox version: ${BOWLINGBOX_VERSION}")
