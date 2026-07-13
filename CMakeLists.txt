cmake_minimum_required(VERSION 3.28)

project(xmole2
  VERSION 0.1.0
  DESCRIPTION "Cross-platform Office document processing library"
  LANGUAGES CXX
)

include(CMakePackageConfigHelpers)
include(GNUInstallDirs)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(XMOLE2_BUILD_TESTS "Build xmole2 tests" ON)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(Xmole2Module)
include(Xmole2Dependencies)

add_subdirectory(libs)

if(XMOLE2_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()

configure_package_config_file(
  "${CMAKE_CURRENT_SOURCE_DIR}/cmake/xmole2Config.cmake.in"
  "${CMAKE_CURRENT_BINARY_DIR}/xmole2Config.cmake"
  INSTALL_DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/xmole2"
)

write_basic_package_version_file(
  "${CMAKE_CURRENT_BINARY_DIR}/xmole2ConfigVersion.cmake"
  VERSION "${PROJECT_VERSION}"
  COMPATIBILITY SameMajorVersion
)

install(
  EXPORT xmole2Targets
  NAMESPACE xmole2::
  DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/xmole2"
)

install(
  FILES
    "${CMAKE_CURRENT_BINARY_DIR}/xmole2Config.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/xmole2ConfigVersion.cmake"
  DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/xmole2"
)
