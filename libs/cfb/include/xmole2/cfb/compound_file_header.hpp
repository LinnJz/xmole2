/// @file
/// CFB header metadata and the first bounded read contract.

#pragma once

#include <cstdint>

#include "xmole2/base/operation_context.hpp"
#include "xmole2/cfb/export.hpp"
#include "xmole2/io/source_lease.hpp"

namespace xmole2::cfb
{

enum class CfbVersion : std::uint16_t
{
  Version3 = 3,
  Version4 = 4,
};

/// Validated physical metadata from the fixed 512-byte CFB header.
struct CompoundFileHeader
{
  CfbVersion version { CfbVersion::Version3 };
  std::uint16_t minor_version {};
  std::uint32_t sector_size {};
  std::uint32_t mini_sector_size {};
  std::uint64_t sector_count {};
  std::uint32_t directory_sector_count {};
  std::uint32_t fat_sector_count {};
  std::uint32_t first_directory_sector {};
  std::uint32_t transaction_signature {};
  std::uint32_t mini_stream_cutoff_size {};
  std::uint32_t first_mini_fat_sector {};
  std::uint32_t mini_fat_sector_count {};
  std::uint32_t first_difat_sector {};
  std::uint32_t difat_sector_count {};
};

/// Reads and validates only the CFB header without materializing later sectors.
[[nodiscard]] XMOLE2_CFB_API auto read_header(
    io::SourceLease const &source, OperationContext const &context)
    -> Result<CompoundFileHeader>;

} // namespace xmole2::cfb
