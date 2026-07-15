add_library(xmole2_base
  src/error.cpp
  src/external_resource_resolver.cpp
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

target_compile_features(xmole2_base PUBLIC cxx_std_23)
xmole2_configure_target(xmole2_base PRIVATE)

if(BUILD_SHARED_LIBS)
  target_compile_definitions(xmole2_base
    PRIVATE XMOLE2_BASE_EXPORTS
    PUBLIC XMOLE2_BUILD_SHARED
  )
endif()

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
