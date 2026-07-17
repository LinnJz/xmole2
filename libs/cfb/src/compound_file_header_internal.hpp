#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "xmole2/cfb/compound_file_header.hpp"

namespace xmole2::cfb::internal
{

inline constexpr auto kHeaderDifatEntryCount = std::size_t { 109 };

struct ParsedCompoundFileHeader
{
  CompoundFileHeader header;
  std::array<std::uint32_t, kHeaderDifatEntryCount> difat;
};

[[nodiscard]] auto read_compound_file_header(
    io::SourceLease const &source, OperationContext const &context)
    -> Result<ParsedCompoundFileHeader>;

} // namespace xmole2::cfb::internal
