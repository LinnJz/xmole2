add_executable(xmole2_base_contract_tests
  base/error_contract_test.cpp
  base/operation_context_contract_test.cpp
)

target_link_libraries(xmole2_base_contract_tests PRIVATE xmole2::base)
xmole2_configure_target(xmole2_base_contract_tests PRIVATE)

add_test(NAME xmole2.base.contract COMMAND xmole2_base_contract_tests)

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

