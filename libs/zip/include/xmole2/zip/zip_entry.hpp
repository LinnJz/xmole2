/// @file
/// ZIP entry metadata exposed independently of the backend.

#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

namespace xmole2::zip
{

struct ZipEntry
{
  std::size_t index {};
  std::string name;
  std::uint64_t compressed_size {};
  std::uint64_t uncompressed_size {};
  std::uint32_t crc32 {};
  std::uint16_t compression_method {};
  bool encrypted {};
  bool directory {};
};

} // namespace xmole2::zip
