/// @file
/// CFB MiniFAT table and root mini-stream sector mapping contract.

#pragma once

#include <cstdint>
#include <vector>

#include "xmole2/cfb/directory_index.hpp"
#include "xmole2/cfb/export.hpp"

namespace xmole2::cfb
{

/// Validated MiniFAT metadata and the FAT-backed root mini-stream mapping.
struct MiniStreamAllocationTable
{
  DirectoryIndex directory_index;
  std::vector<std::uint32_t> mini_fat_sector_ids;
  std::vector<std::uint32_t> mini_fat_entries;
  std::vector<std::uint32_t> root_mini_stream_sector_ids;
  std::uint64_t root_mini_sector_count {};
};

/// Reads MiniFAT sectors and maps the root mini-stream without reading its payload.
[[nodiscard]] XMOLE2_CFB_API auto read_mini_stream_allocation_table(
    io::SourceLease const &source, OperationContext const &context)
    -> Result<MiniStreamAllocationTable>;

} // namespace xmole2::cfb
