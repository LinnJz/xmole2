if(NOT DEFINED XMOLE2_SOURCE_DIR)
  message(FATAL_ERROR "XMOLE2_SOURCE_DIR is required")
endif()

file(GLOB_RECURSE public_headers LIST_DIRECTORIES FALSE
  "${XMOLE2_SOURCE_DIR}/libs/*/include/*.h"
  "${XMOLE2_SOURCE_DIR}/libs/*/include/*.hpp"
  "${XMOLE2_SOURCE_DIR}/libs/*/include/*.ixx"
)

set(forbidden_patterns
  "[<\"]absl/"
  "[<\"]boost/"
  "[<\"]bq_log/"
  "[<\"]fmt/"
  "[<\"]frozen/"
  "[<\"]magic_enum/"
  "[<\"]minizip-ng/"
  "[<\"]openssl/"
  "[<\"]pugixml"
  "[<\"]re2/"
  "[<\"]simdutf"
)

foreach(public_header IN LISTS public_headers)
  file(READ "${public_header}" contents)
  foreach(forbidden_pattern IN LISTS forbidden_patterns)
    if(contents MATCHES "${forbidden_pattern}")
      message(FATAL_ERROR
        "Third-party dependency leaked into public header ${public_header}: "
        "${forbidden_pattern}"
      )
    endif()
  endforeach()
endforeach()

list(LENGTH public_headers public_header_count)
message(STATUS "Checked ${public_header_count} public headers for third-party leakage")
