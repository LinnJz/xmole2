add_executable(xmole2_base_contract_tests
  base/error_contract_test.cpp
  base/operation_context_contract_test.cpp
)

target_link_libraries(xmole2_base_contract_tests PRIVATE xmole2::base)
xmole2_configure_target(xmole2_base_contract_tests PRIVATE)

add_test(NAME xmole2.base.contract COMMAND xmole2_base_contract_tests)

add_executable(xmole2_io_contract_tests
  io/io_contract_main.cpp
  io/source_contract_test.cpp
  io/storage_contract_test.cpp
)

target_link_libraries(xmole2_io_contract_tests PRIVATE xmole2::io)
xmole2_configure_target(xmole2_io_contract_tests PRIVATE)

add_test(NAME xmole2.io.contract COMMAND xmole2_io_contract_tests)

add_executable(xmole2_zip_contract_tests
  zip/zip_contract_test.cpp
)

target_link_libraries(xmole2_zip_contract_tests PRIVATE xmole2::zip)
xmole2_configure_target(xmole2_zip_contract_tests PRIVATE)

add_test(NAME xmole2.zip.contract COMMAND xmole2_zip_contract_tests)

if(WIN32 AND BUILD_SHARED_LIBS)
  set_tests_properties(xmole2.base.contract PROPERTIES
    ENVIRONMENT_MODIFICATION
      "PATH=path_list_prepend:$<TARGET_FILE_DIR:xmole2_base>"
  )
  set_tests_properties(xmole2.io.contract PROPERTIES
    ENVIRONMENT_MODIFICATION
      "PATH=path_list_prepend:$<TARGET_FILE_DIR:xmole2_io>;$<TARGET_FILE_DIR:xmole2_base>"
  )
  set_tests_properties(xmole2.zip.contract PROPERTIES
    ENVIRONMENT_MODIFICATION
      "PATH=path_list_prepend:$<TARGET_FILE_DIR:xmole2_zip>;$<TARGET_FILE_DIR:xmole2_io>;$<TARGET_FILE_DIR:xmole2_base>"
  )
endif()

add_test(
  NAME xmole2.architecture.public_headers
  COMMAND ${CMAKE_COMMAND}
    -DXMOLE2_SOURCE_DIR=${PROJECT_SOURCE_DIR}
    -P ${PROJECT_SOURCE_DIR}/cmake/CheckPublicHeaders.cmake
)

if(XMOLE2_VERIFY_DEPENDENCIES)
  add_executable(xmole2_dependency_smoke_tests
    dependencies/dependency_smoke_test.cpp
  )

  target_link_libraries(xmole2_dependency_smoke_tests PRIVATE
    Boost::uuid
    GTest::gtest_main
    MINIZIP::minizip-ng
    OpenSSL::Crypto
    absl::flat_hash_map
    absl::strings
    fmt::fmt
    frozen::frozen
    magic_enum::magic_enum
    pugixml::pugixml
    re2::re2
    simdutf::simdutf
  )

  if(XMOLE2_HAVE_BQLOG)
    target_link_libraries(xmole2_dependency_smoke_tests PRIVATE BqLog::BqLog)
    target_compile_definitions(xmole2_dependency_smoke_tests PRIVATE XMOLE2_HAVE_BQLOG=1)
  else()
    target_compile_definitions(xmole2_dependency_smoke_tests PRIVATE XMOLE2_HAVE_BQLOG=0)
  endif()

  xmole2_configure_target(xmole2_dependency_smoke_tests PRIVATE)

  add_test(NAME xmole2.dependencies.smoke COMMAND xmole2_dependency_smoke_tests)
endif()
