find_package(minizip-ng CONFIG REQUIRED)

add_library(xmole2_zip
  src/backend.cpp
  src/error.cpp
  src/zip_archive.cpp
  src/zip_entry_reader.cpp
)

add_library(xmole2::zip ALIAS xmole2_zip)

set_target_properties(xmole2_zip PROPERTIES
  EXPORT_NAME zip
  CXX_VISIBILITY_PRESET hidden
  VISIBILITY_INLINES_HIDDEN YES
)

target_include_directories(xmole2_zip PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

target_link_libraries(xmole2_zip
  PUBLIC
    xmole2::base
    xmole2::io
  PRIVATE
    MINIZIP::minizip-ng
)
target_compile_features(xmole2_zip PUBLIC cxx_std_23)
xmole2_configure_target(xmole2_zip PRIVATE)

if(BUILD_SHARED_LIBS)
  target_compile_definitions(xmole2_zip
    PRIVATE XMOLE2_ZIP_EXPORTS
    PUBLIC XMOLE2_BUILD_SHARED
  )
endif()

install(
  TARGETS xmole2_zip
  EXPORT xmole2Targets
  ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
  LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
  RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
)

install(
  DIRECTORY include/
  DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
)
