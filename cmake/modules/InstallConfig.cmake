# Install / export package configuration for consumers of Bowlingbox libraries.

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

set(BOWLINGBOX_INSTALL_CMAKEDIR ${CMAKE_INSTALL_LIBDIR}/cmake/${CMAKE_ROOT_NAME})

write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_ROOT_NAME}ConfigVersion.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

# configure_package_config_file defines @PACKAGE_INIT@ and
# check_required_components() for the template.
configure_package_config_file(
    "${CMAKE_SOURCE_DIR}/cmake/BowlingboxConfig.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_ROOT_NAME}Config.cmake"
    INSTALL_DESTINATION ${BOWLINGBOX_INSTALL_CMAKEDIR}
)

install(
    FILES
        "${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_ROOT_NAME}Config.cmake"
        "${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_ROOT_NAME}ConfigVersion.cmake"
    DESTINATION ${BOWLINGBOX_INSTALL_CMAKEDIR}
)

install(
    EXPORT ${CMAKE_ROOT_NAME}Targets
    NAMESPACE ${CMAKE_ROOT_NAME}::
    DESTINATION ${BOWLINGBOX_INSTALL_CMAKEDIR}
)
