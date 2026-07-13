include_guard(GLOBAL)

find_package(BqLog 2.3.1 CONFIG QUIET)
set(XMOLE2_HAVE_BQLOG OFF)
if(TARGET BqLog::BqLog)
  set(XMOLE2_HAVE_BQLOG ON)
else()
  message(STATUS "BqLog 2.3.1 is unavailable; optional logging integration is disabled.")
endif()

if(XMOLE2_BUILD_TESTS)
  find_package(GTest CONFIG REQUIRED)
endif()

find_package(OpenSSL REQUIRED COMPONENTS Crypto)
find_package(absl CONFIG REQUIRED)
find_package(boost_uuid CONFIG REQUIRED)
find_package(fmt CONFIG REQUIRED)
find_package(frozen CONFIG REQUIRED)
find_package(magic_enum CONFIG REQUIRED)
find_package(minizip-ng CONFIG REQUIRED)
find_package(pugixml CONFIG REQUIRED)
find_package(re2 CONFIG REQUIRED)
find_package(simdutf CONFIG REQUIRED)

set(required_targets
  Boost::uuid
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

if(XMOLE2_BUILD_TESTS)
  list(APPEND required_targets GTest::gtest_main)
endif()

foreach(required_target IN LISTS required_targets)
  if(NOT TARGET ${required_target})
    message(FATAL_ERROR "Required imported target is missing: ${required_target}")
  endif()
endforeach()

unset(required_target)
unset(required_targets)
