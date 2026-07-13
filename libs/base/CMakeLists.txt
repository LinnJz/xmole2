add_library(xmole2_base
  src/error.cpp
  src/operation_context.cpp
)

add_library(xmole2::base ALIAS xmole2_base)

set_target_properties(xmole2_base PROPERTIES
  EXPORT_NAME base
  CXX_VISIBILITY_PRESET hidden
  VISIBILITY_INLINES_HIDDEN YES
)

target_include_directories(xmole2_base PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

xmole2_configure_target(xmole2_base PUBLIC)

install(
  TARGETS xmole2_base
  EXPORT xmole2Targets
  ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
  LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
  RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
)

install(
  DIRECTORY include/
  DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
)
