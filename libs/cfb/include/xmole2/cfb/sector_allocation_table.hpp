/// @file
/// CFB DIFAT and FAT sector table contract.

#pragma once

#include <cstdint>
#include <vector>

#include "xmole2/cfb/compound_file_header.hpp"
#include "xmole2/cfb/export.hpp"

namespace xmole2::cfb
{

/// Validated sector allocation metadata in on-disk DIFAT order.
struct SectorAllocationTable
{
  CompoundFileHeader header;
  std::vector<std::uint32_t> fat_sector_ids;
  std::vector<std::uint32_t> difat_sector_ids;
  std::vector<std::uint32_t> fat_entries;
};

/// Reads the DIFAT chain and FAT sectors without reading directory or stream data.
[[nodiscard]] XMOLE2_CFB_API auto read_sector_allocation_table(
    io::SourceLease const &source, OperationContext const &context)
    -> Result<SectorAllocationTable>;

} // namespace xmole2::cfb
