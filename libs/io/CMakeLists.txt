add_library(xmole2_io
  src/atomic_file_writer.cpp
  src/error.cpp
  src/file_io.cpp
  src/source_lease.cpp
  src/temporary_file.cpp
)

add_library(xmole2::io ALIAS xmole2_io)

set_target_properties(xmole2_io PROPERTIES
  EXPORT_NAME io
  CXX_VISIBILITY_PRESET hidden
  VISIBILITY_INLINES_HIDDEN YES
)

target_include_directories(xmole2_io PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

target_link_libraries(xmole2_io PUBLIC xmole2::base)
target_compile_features(xmole2_io PUBLIC cxx_std_23)
xmole2_configure_target(xmole2_io PRIVATE)

if(BUILD_SHARED_LIBS)
  target_compile_definitions(xmole2_io
    PRIVATE XMOLE2_IO_EXPORTS
    PUBLIC XMOLE2_BUILD_SHARED
  )
endif()

install(
  TARGETS xmole2_io
  EXPORT xmole2Targets
  ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
  LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
  RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
)

install(
  DIRECTORY include/
  DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
)

