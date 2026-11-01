# GLM maths, header-only, through dev_dependency(): an installed glm (e.g.
# Debian/Ubuntu libglm-dev, MSYS2 mingw-w64-ucrt-x86_64-glm) first, else
# release 1.0.1 fetched. Both provide glm::glm. No minimum version: the
# distributions ship 0.9.9.8.

# Only read when glm is fetched.
set(GLM_BUILD_LIBRARY OFF)
set(GLM_BUILD_TESTS OFF)
set(GLM_BUILD_INSTALL OFF)
set(GLM_ENABLE_CXX_20 ON)

dev_dependency(glm REQUIRED
    FETCH_URL https://github.com/g-truc/glm/archive/refs/tags/1.0.1.tar.gz)
