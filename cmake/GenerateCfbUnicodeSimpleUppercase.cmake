cmake_minimum_required(VERSION 3.28)
cmake_policy(SET CMP0007 NEW)

if(NOT DEFINED INPUT OR NOT DEFINED OUTPUT)
  message(FATAL_ERROR "INPUT and OUTPUT are required")
endif()

set(expected_sha256 "2E1EFC1DCB59C575EEDF5CCAE60F95229F706EE6D031835247D843C11D96470C")
file(SHA256 "${INPUT}" actual_sha256)
string(TOUPPER "${actual_sha256}" actual_sha256)
if(NOT actual_sha256 STREQUAL expected_sha256)
  message(FATAL_ERROR
    "UnicodeData.txt SHA-256 mismatch: expected ${expected_sha256}, got ${actual_sha256}"
  )
endif()

file(STRINGS "${INPUT}" unicode_lines REGEX "^[0-9A-F]+;")
set(sources "")
set(targets "")
foreach(line IN LISTS unicode_lines)
  set(fields "${line}")
  list(GET fields 0 source_hex)
  list(GET fields 12 target_hex)
  if(target_hex STREQUAL "")
    continue()
  endif()
  math(EXPR source "0x${source_hex}")
  if(source GREATER 65535)
    continue()
  endif()
  math(EXPR target "0x${target_hex}")
  if(target GREATER 65535)
    message(FATAL_ERROR
      "BMP source U+${source_hex} maps outside the BMP to U+${target_hex}"
    )
  endif()
  list(APPEND sources "${source}")
  list(APPEND targets "${target}")
endforeach()

list(LENGTH sources mapping_count)
set(index 0)
set(segment_count 0)
set(segment_entries "")
while(index LESS mapping_count)
  set(best_end ${index})
  set(best_step 1)
  foreach(step IN ITEMS 1 2)
    set(candidate_end ${index})
    while(TRUE)
      math(EXPR next "${candidate_end} + 1")
      if(next GREATER_EQUAL mapping_count)
        break()
      endif()
      list(GET sources ${candidate_end} current_source)
      list(GET sources ${next} next_source)
      list(GET targets ${candidate_end} current_target)
      list(GET targets ${next} next_target)
      math(EXPR source_step "${next_source} - ${current_source}")
      math(EXPR target_step "${next_target} - ${current_target}")
      if(NOT source_step EQUAL step OR NOT target_step EQUAL step)
        break()
      endif()
      set(candidate_end ${next})
    endwhile()
    if(candidate_end GREATER best_end)
      set(best_end ${candidate_end})
      set(best_step ${step})
    endif()
  endforeach()

  list(GET sources ${index} first)
  list(GET sources ${best_end} last)
  list(GET targets ${index} first_target)
  math(EXPR delta "${first_target} - ${first}")
  string(APPEND segment_entries
    "  SimpleUppercaseSegment { ${first}, ${last}, ${best_step}, ${delta} },\n"
  )
  math(EXPR segment_count "${segment_count} + 1")
  math(EXPR index "${best_end} + 1")
endwhile()

set(generated [=[// Generated from Unicode 17.0.0 UnicodeData.txt; do not edit manually.
// Source: https://www.unicode.org/Public/17.0.0/ucd/UnicodeData.txt
// SHA-256: 2E1EFC1DCB59C575EEDF5CCAE60F95229F706EE6D031835247D843C11D96470C
//
// UNICODE LICENSE V3
//
// COPYRIGHT AND PERMISSION NOTICE
//
// Copyright © 1991-2026 Unicode, Inc.
//
// NOTICE TO USER: Carefully read the following legal agreement. BY
// DOWNLOADING, INSTALLING, COPYING OR OTHERWISE USING DATA FILES, AND/OR
// SOFTWARE, YOU UNEQUIVOCALLY ACCEPT, AND AGREE TO BE BOUND BY, ALL OF THE
// TERMS AND CONDITIONS OF THIS AGREEMENT. IF YOU DO NOT AGREE, DO NOT
// DOWNLOAD, INSTALL, COPY, DISTRIBUTE OR USE THE DATA FILES OR SOFTWARE.
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of data files and any associated documentation (the "Data Files") or
// software and any associated documentation (the "Software") to deal in the
// Data Files or Software without restriction, including without limitation
// the rights to use, copy, modify, merge, publish, distribute, and/or sell
// copies of the Data Files or Software, and to permit persons to whom the Data
// Files or Software are furnished to do so, provided that either (a) this
// copyright and permission notice appear with all copies of the Data Files or
// Software, or (b) this copyright and permission notice appear in associated
// Documentation.
//
// THE DATA FILES AND SOFTWARE ARE PROVIDED "AS IS", WITHOUT WARRANTY OF ANY
// KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT OF
// THIRD PARTY RIGHTS.
//
// IN NO EVENT SHALL THE COPYRIGHT HOLDER OR HOLDERS INCLUDED IN THIS NOTICE
// BE LIABLE FOR ANY CLAIM, OR ANY SPECIAL INDIRECT OR CONSEQUENTIAL DAMAGES,
// OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,
// WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION,
// ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THE DATA
// FILES OR SOFTWARE.
//
// Except as contained in this notice, the name of a copyright holder shall
// not be used in advertising or otherwise to promote the sale, use or other
// dealings in these Data Files or Software without prior written
// authorization of the copyright holder.

#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace xmole2::cfb::internal
{

struct SimpleUppercaseSegment
{
  std::uint16_t first;
  std::uint16_t last;
  std::uint8_t step;
  std::int32_t delta;
};

]=])
string(APPEND generated
  "// clang-format off\n"
  "inline constexpr auto kSimpleUppercaseMappingCount = std::size_t { ${mapping_count} };\n"
  "inline constexpr auto kSimpleUppercaseSegments =\n"
  "    std::array<SimpleUppercaseSegment, ${segment_count}> {\n"
  "${segment_entries}"
  "    };\n"
  "// clang-format on\n\n"
  "} // namespace xmole2::cfb::internal\n"
)

file(WRITE "${OUTPUT}" "${generated}")
message(STATUS
  "Generated ${segment_count} simple-uppercase segments for ${mapping_count} BMP mappings"
)
