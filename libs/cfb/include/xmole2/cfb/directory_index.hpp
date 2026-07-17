/// @file
/// CFB directory sector chain and 128-byte entry index contract.

#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "xmole2/cfb/export.hpp"
#include "xmole2/cfb/sector_allocation_table.hpp"

namespace xmole2::cfb
{

enum class DirectoryEntryType : std::uint8_t
{
  Unallocated = 0,
  Storage     = 1,
  Stream      = 2,
  RootStorage = 5,
};

enum class DirectoryColor : std::uint8_t
{
  Red   = 0,
  Black = 1,
};

/// One owning, validated CFB directory slot. Indices preserve physical directory IDs.
struct DirectoryEntry
{
  std::uint32_t index {};
  std::u16string name;
  DirectoryEntryType type { DirectoryEntryType::Unallocated };
  DirectoryColor color { DirectoryColor::Red };
  std::uint32_t left_sibling_id {};
  std::uint32_t right_sibling_id {};
  std::uint32_t child_id {};
  std::array<std::byte, 16> clsid;
  std::uint32_t state_bits {};
  std::uint64_t creation_time {};
  std::uint64_t modified_time {};
  std::uint32_t starting_sector {};
  std::uint64_t stream_size {};
};

struct DirectoryIndex
{
  SectorAllocationTable allocation_table;
  std::vector<std::uint32_t> directory_sector_ids;
  std::vector<DirectoryEntry> entries;
};

/// Reads the FAT-backed directory chain and indexes every physical directory slot.
[[nodiscard]] XMOLE2_CFB_API auto read_directory_index(
    io::SourceLease const &source, OperationContext const &context)
    -> Result<DirectoryIndex>;

} // namespace xmole2::cfb
