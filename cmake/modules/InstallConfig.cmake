# Install / export package configuration for consumers of Sandbox libraries.

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

set(SANDBOX_INSTALL_CMAKEDIR ${CMAKE_INSTALL_LIBDIR}/cmake/${CMAKE_ROOT_NAME})

write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_ROOT_NAME}ConfigVersion.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

configure_file(
    "${CMAKE_SOURCE_DIR}/cmake/SandboxConfig.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_ROOT_NAME}Config.cmake"
    @ONLY
)

install(
    FILES
        "${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_ROOT_NAME}Config.cmake"
        "${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_ROOT_NAME}ConfigVersion.cmake"
    DESTINATION ${SANDBOX_INSTALL_CMAKEDIR}
)

install(
    EXPORT ${CMAKE_ROOT_NAME}Targets
    NAMESPACE ${CMAKE_ROOT_NAME}::
    DESTINATION ${SANDBOX_INSTALL_CMAKEDIR}
)
