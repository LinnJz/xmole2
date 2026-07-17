add_library(xmole2_cfb
  src/cfb_name_internal.cpp
  src/cfb_stream_reader.cpp
  src/compound_file_header.cpp
  src/directory_index.cpp
  src/error.cpp
  src/mini_stream_allocation_table.cpp
  src/sector_allocation_table.cpp
  src/sector_reader_internal.cpp
)

add_library(xmole2::cfb ALIAS xmole2_cfb)

set_target_properties(xmole2_cfb PROPERTIES
  EXPORT_NAME cfb
  CXX_VISIBILITY_PRESET hidden
  VISIBILITY_INLINES_HIDDEN YES
)

target_include_directories(xmole2_cfb PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

target_link_libraries(xmole2_cfb
  PUBLIC
    xmole2::base
    xmole2::io
)
target_compile_features(xmole2_cfb PUBLIC cxx_std_23)
xmole2_configure_target(xmole2_cfb PRIVATE)

if(BUILD_SHARED_LIBS)
  target_compile_definitions(xmole2_cfb
    PRIVATE XMOLE2_CFB_EXPORTS
    PUBLIC XMOLE2_BUILD_SHARED
  )
endif()

install(
  TARGETS xmole2_cfb
  EXPORT xmole2Targets
  ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
  LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
  RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
)

install(
  DIRECTORY include/
  DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
)
