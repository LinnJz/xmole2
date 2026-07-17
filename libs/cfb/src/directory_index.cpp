#include "xmole2/cfb/directory_index.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <span>
#include <string_view>
#include <utility>
#include <vector>

#include "cfb_internal_utils.hpp"
#include "cfb_name_internal.hpp"
#include "sector_reader_internal.hpp"
#include "xmole2/cfb/error.hpp"

namespace xmole2::cfb
{
namespace
{

constexpr auto kDirectoryEntrySize = std::uint64_t { 128 };
constexpr auto kMaximumNameBytes   = std::uint16_t { 64 };
constexpr auto kFatSector          = std::uint32_t { 0xff'ff'ff'fdU };
constexpr auto kDifatSector        = std::uint32_t { 0xff'ff'ff'fcU };
constexpr auto kEndOfChain         = std::uint32_t { 0xff'ff'ff'feU };
constexpr auto kFreeSector         = std::uint32_t { 0xff'ff'ff'ffU };
constexpr auto kNoStream           = std::uint32_t { 0xff'ff'ff'ffU };

struct TreeFrame
{
  std::uint32_t entry {};
  std::uint8_t next_edge {};
};

using internal::add_memory;
using internal::bytes_are_zero;
using internal::checked_multiply;
using internal::read_u16;
using internal::read_u32;
using internal::read_u64;

auto cancelled_error() -> Error
{
  return make_error(CfbErrorCode::Cancelled, "CFB directory read cancelled");
}

auto invalid_directory(std::string_view const message) -> Error
{
  return make_error(CfbErrorCode::InvalidDirectory, message);
}

auto invalid_entry(std::string_view const message) -> Error
{
  return make_error(CfbErrorCode::InvalidDirectoryEntry, message);
}

auto resource_error(std::string_view const message) -> Error
{
  return make_error(CfbErrorCode::ResourceLimitExceeded, message);
}

auto allocation_memory(SectorAllocationTable const &table) -> std::optional<std::uint64_t>
{
  auto total = static_cast<std::uint64_t>(sizeof(SectorAllocationTable));
  auto const fat_ids =
      checked_multiply(table.fat_sector_ids.size(), sizeof(std::uint32_t));
  auto const difat_ids =
      checked_multiply(table.difat_sector_ids.size(), sizeof(std::uint32_t));
  auto const fat_entries =
      checked_multiply(table.fat_entries.size(), sizeof(std::uint32_t));
  if (!fat_ids || !difat_ids || !fat_entries ||
      !add_memory(total, *fat_ids, std::numeric_limits<std::uint64_t>::max()) ||
      !add_memory(total, *difat_ids, std::numeric_limits<std::uint64_t>::max()) ||
      !add_memory(total, *fat_entries, std::numeric_limits<std::uint64_t>::max()))
  {
    return std::nullopt;
  }
  return total;
}

auto is_valid_utf16(std::u16string_view const name) -> bool
{
  for (auto index = std::size_t {}; index < name.size(); ++index)
  {
    auto const value = static_cast<std::uint16_t>(name[index]);
    if (value >= 0xd8'00 && value <= 0xdb'ff)
    {
      if (index + 1 >= name.size())
      {
        return false;
      }
      auto const low = static_cast<std::uint16_t>(name[index + 1]);
      if (low < 0xdc'00 || low > 0xdf'ff)
      {
        return false;
      }
      ++index;
    }
    else if (value >= 0xdc'00 && value <= 0xdf'ff)
    {
      return false;
    }
  }
  return true;
}

auto parse_entry(
    std::span<std::byte const> const bytes,
    std::uint32_t const index,
    CfbVersion const version,
    OperationContext const &context) -> Result<DirectoryEntry>
{
  auto const raw_type = std::to_integer<std::uint8_t>(bytes[66]);
  auto type           = DirectoryEntryType::Unallocated;
  if (raw_type == static_cast<std::uint8_t>(DirectoryEntryType::Storage))
  {
    type = DirectoryEntryType::Storage;
  }
  else if (raw_type == static_cast<std::uint8_t>(DirectoryEntryType::Stream))
  {
    type = DirectoryEntryType::Stream;
  }
  else if (raw_type == static_cast<std::uint8_t>(DirectoryEntryType::RootStorage))
  {
    type = DirectoryEntryType::RootStorage;
  }
  else if (raw_type != static_cast<std::uint8_t>(DirectoryEntryType::Unallocated))
  {
    return std::unexpected { invalid_entry("CFB directory entry type is invalid") };
  }

  auto const name_length = read_u16(bytes, 64);
  if (type == DirectoryEntryType::Unallocated)
  {
    auto const left  = read_u32(bytes, 68);
    auto const right = read_u32(bytes, 72);
    auto const child = read_u32(bytes, 76);
    if (!bytes_are_zero(bytes, 0, 68) || left != kNoStream || right != kNoStream ||
        child != kNoStream || !bytes_are_zero(bytes, 80, 48))
    {
      return std::unexpected { invalid_entry(
          "unallocated CFB directory entry metadata is invalid") };
    }
    return DirectoryEntry {
      .index            = index,
      .type             = type,
      .color            = DirectoryColor::Red,
      .left_sibling_id  = left,
      .right_sibling_id = right,
      .child_id         = child,
    };
  }

  if (name_length < 2 || name_length > kMaximumNameBytes || (name_length % 2) != 0)
  {
    return std::unexpected { invalid_entry(
        "CFB directory entry name length is invalid") };
  }
  auto const name_code_units = static_cast<std::size_t>((name_length / 2) - 1);
  if (read_u16(bytes, name_code_units * 2) != 0)
  {
    return std::unexpected { invalid_entry(
        "CFB directory entry name is not null terminated") };
  }
  auto name = std::u16string {};
  name.reserve(name_code_units);
  for (auto name_index = std::size_t {}; name_index < name_code_units; ++name_index)
  {
    auto const code_unit = read_u16(bytes, name_index * 2);
    if (code_unit == 0)
    {
      return std::unexpected { invalid_entry(
          "CFB directory entry name contains an embedded null") };
    }
    name.push_back(static_cast<char16_t>(code_unit));
  }
  if (!is_valid_utf16(name))
  {
    return std::unexpected { invalid_entry(
        "CFB directory entry name is not valid UTF-16") };
  }
  if (name.size() > context.budget.max_path_length)
  {
    return std::unexpected { resource_error(
        "CFB directory entry name exceeds the path budget") };
  }

  auto const raw_color = std::to_integer<std::uint8_t>(bytes[67]);
  if (raw_color > static_cast<std::uint8_t>(DirectoryColor::Black))
  {
    return std::unexpected { invalid_entry("CFB directory entry color is invalid") };
  }
  auto const raw_stream_size = read_u64(bytes, 120);
  auto const stream_size =
      version == CfbVersion::Version3
          ? static_cast<std::uint64_t>(static_cast<std::uint32_t>(raw_stream_size))
          : raw_stream_size;
  if (version == CfbVersion::Version3 && stream_size > 0x80'00'00'00ULL)
  {
    return std::unexpected { invalid_entry(
        "CFB version 3 directory stream size exceeds 2 GiB") };
  }
  auto const starting_sector = read_u32(bytes, 116);
  if (type == DirectoryEntryType::Storage &&
      (starting_sector != 0 || raw_stream_size != 0))
  {
    return std::unexpected { invalid_entry(
        "CFB storage entries must not declare stream data") };
  }
  auto const child = read_u32(bytes, 76);
  if (type == DirectoryEntryType::Stream && child != kNoStream)
  {
    return std::unexpected { invalid_entry(
        "CFB stream entries must not have child entries") };
  }
  auto const state_bits    = read_u32(bytes, 96);
  auto const creation_time = read_u64(bytes, 100);
  auto const modified_time = read_u64(bytes, 108);
  if (type == DirectoryEntryType::Stream &&
      (!bytes_are_zero(bytes, 80, 16) || creation_time != 0 || modified_time != 0))
  {
    return std::unexpected { invalid_entry(
        "CFB stream entries must not contain storage metadata") };
  }
  if (type == DirectoryEntryType::RootStorage && creation_time != 0)
  {
    return std::unexpected { invalid_entry(
        "CFB root storage creation time must be zero") };
  }
  if ((type == DirectoryEntryType::Stream || type == DirectoryEntryType::RootStorage) &&
      stream_size == 0 && starting_sector != kEndOfChain)
  {
    return std::unexpected { invalid_entry(
        "empty CFB streams must start at end-of-chain") };
  }

  auto result = DirectoryEntry {
    .index            = index,
    .name             = std::move(name),
    .type             = type,
    .color            = static_cast<DirectoryColor>(raw_color),
    .left_sibling_id  = read_u32(bytes, 68),
    .right_sibling_id = read_u32(bytes, 72),
    .child_id         = child,
    .state_bits       = state_bits,
    .creation_time    = creation_time,
    .modified_time    = modified_time,
    .starting_sector  = starting_sector,
    .stream_size      = stream_size,
  };
  std::copy_n(bytes.begin() + 80, result.clsid.size(), result.clsid.begin());
  return result;
}

auto validate_directory_tree(
    std::span<DirectoryEntry const> const entries, OperationContext const &context)
    -> Status
{
  if (entries.empty() || entries[0].type != DirectoryEntryType::RootStorage)
  {
    return std::unexpected { invalid_directory(
        "CFB directory entry zero must be the root storage") };
  }
  auto const &root = entries[0];
  if (root.name != u"Root Entry" || root.left_sibling_id != kNoStream ||
      root.right_sibling_id != kNoStream)
  {
    return std::unexpected { invalid_directory(
        "CFB root directory entry metadata is invalid") };
  }

  auto incoming = std::vector<std::uint32_t>(entries.size());
  auto validate_reference =
      [&](DirectoryEntry const &entry, std::uint32_t const target) -> Status
  {
    if (target == kNoStream)
    {
      return {};
    }
    if (target >= entries.size() ||
        entries[target].type == DirectoryEntryType::Unallocated || target == entry.index)
    {
      return std::unexpected { invalid_entry(
          "CFB directory entry reference is invalid") };
    }
    if (++incoming[target] > 1)
    {
      return std::unexpected { invalid_entry(
          "CFB directory entry has multiple parents") };
    }
    return {};
  };

  for (auto const &entry : entries)
  {
    if (context.cancellation.is_cancelled())
    {
      return std::unexpected { cancelled_error() };
    }
    if (entry.type == DirectoryEntryType::Unallocated)
    {
      continue;
    }
    if (entry.index != 0 && entry.type == DirectoryEntryType::RootStorage)
    {
      return std::unexpected { invalid_directory(
          "CFB directory contains more than one root storage") };
    }
    auto status = validate_reference(entry, entry.left_sibling_id);
    if (!status)
    {
      return status;
    }
    status = validate_reference(entry, entry.right_sibling_id);
    if (!status)
    {
      return status;
    }
    status = validate_reference(entry, entry.child_id);
    if (!status)
    {
      return status;
    }
  }
  if (incoming[0] != 0)
  {
    return std::unexpected { invalid_entry(
        "CFB root directory entry must not have a parent") };
  }

  for (auto const &entry : entries)
  {
    if (entry.type == DirectoryEntryType::Unallocated)
    {
      continue;
    }
    if (entry.child_id != kNoStream &&
        entries[entry.child_id].color != DirectoryColor::Black)
    {
      return std::unexpected { invalid_directory(
          "CFB directory sibling tree root must be black") };
    }
    if (entry.color == DirectoryColor::Red &&
        ((entry.left_sibling_id != kNoStream &&
          entries[entry.left_sibling_id].color == DirectoryColor::Red) ||
         (entry.right_sibling_id != kNoStream &&
          entries[entry.right_sibling_id].color == DirectoryColor::Red)))
    {
      return std::unexpected { invalid_directory(
          "CFB red directory entry must not have a red child") };
    }
  }

  auto states = std::vector<std::uint8_t>(entries.size());
  auto stack  = std::vector<TreeFrame> {};
  stack.reserve(entries.size());
  auto traverse_from = [&](std::uint32_t const start) -> Status
  {
    stack.clear();
    stack.push_back(TreeFrame { .entry = start });
    states[start] = 1;
    while (!stack.empty())
    {
      if (context.cancellation.is_cancelled())
      {
        return std::unexpected { cancelled_error() };
      }
      auto &frame       = stack.back();
      auto const &entry = entries[frame.entry];
      if (frame.next_edge == 3)
      {
        states[frame.entry] = 2;
        stack.pop_back();
        continue;
      }
      auto const target =
          frame.next_edge == 0 ? entry.left_sibling_id
          : frame.next_edge == 1
              ? entry.right_sibling_id
              : entry.child_id;
      ++frame.next_edge;
      if (target == kNoStream || states[target] == 2)
      {
        continue;
      }
      if (states[target] == 1)
      {
        return std::unexpected { make_error(
            CfbErrorCode::DirectoryTreeCycle, "CFB directory tree contains a cycle") };
      }
      states[target] = 1;
      stack.push_back(TreeFrame { .entry = target });
    }
    return {};
  };

  for (auto const &entry : entries)
  {
    if (entry.type != DirectoryEntryType::Unallocated && states[entry.index] == 0)
    {
      auto status = traverse_from(entry.index);
      if (!status)
      {
        return status;
      }
    }
  }

  std::fill(states.begin(), states.end(), std::uint8_t {});
  auto status = traverse_from(0);
  if (!status)
  {
    return status;
  }

  for (auto const &entry : entries)
  {
    if (entry.type != DirectoryEntryType::Unallocated && states[entry.index] == 0)
    {
      return std::unexpected { invalid_directory(
          "CFB directory contains an unreachable allocated entry") };
    }
  }

  auto validate_sibling_order = [&](std::uint32_t const start) -> Status
  {
    auto previous = std::optional<std::uint32_t> {};
    stack.clear();
    stack.push_back(TreeFrame { .entry = start });
    while (!stack.empty())
    {
      if (context.cancellation.is_cancelled())
      {
        return std::unexpected { cancelled_error() };
      }
      auto &frame       = stack.back();
      auto const &entry = entries[frame.entry];
      if (frame.next_edge == 0)
      {
        frame.next_edge = 1;
        if (entry.left_sibling_id != kNoStream)
        {
          stack.push_back(TreeFrame { .entry = entry.left_sibling_id });
        }
        continue;
      }
      if (frame.next_edge == 1)
      {
        frame.next_edge = 2;
        if (previous)
        {
          auto const order =
              internal::compare_directory_names(entries[*previous].name, entry.name);
          if (order == std::strong_ordering::equal)
          {
            return std::unexpected { invalid_directory(
                "CFB sibling directory entries have duplicate names") };
          }
          if (order == std::strong_ordering::greater)
          {
            return std::unexpected { invalid_directory(
                "CFB sibling directory entries are not sorted") };
          }
        }
        previous = entry.index;
        continue;
      }
      if (frame.next_edge == 2)
      {
        frame.next_edge = 3;
        if (entry.right_sibling_id != kNoStream)
        {
          stack.push_back(TreeFrame { .entry = entry.right_sibling_id });
        }
        continue;
      }
      stack.pop_back();
    }
    return {};
  };

  for (auto const &entry : entries)
  {
    if ((entry.type == DirectoryEntryType::Storage ||
         entry.type == DirectoryEntryType::RootStorage) &&
        entry.child_id != kNoStream)
    {
      status = validate_sibling_order(entry.child_id);
      if (!status)
      {
        return status;
      }
    }
  }
  return {};
}

auto validate_memory_budget(
    SectorAllocationTable const &table,
    std::uint64_t const directory_sector_count,
    std::uint64_t const entry_count,
    OperationContext const &context) -> Status
{
  auto allocation_bytes = allocation_memory(table);
  auto const sector_id_bytes =
      checked_multiply(directory_sector_count, sizeof(std::uint32_t));
  auto const entry_bytes =
      checked_multiply(entry_count, sizeof(DirectoryEntry) + kMaximumNameBytes);
  auto const incoming_bytes = checked_multiply(entry_count, sizeof(std::uint32_t));
  auto const state_bytes    = checked_multiply(entry_count, sizeof(std::uint8_t));
  auto const stack_bytes    = checked_multiply(entry_count, sizeof(TreeFrame));
  if (!allocation_bytes || !sector_id_bytes || !entry_bytes || !incoming_bytes ||
      !state_bytes || !stack_bytes ||
      !add_memory(
          *allocation_bytes, sizeof(DirectoryIndex), context.budget.max_memory_bytes) ||
      !add_memory(*allocation_bytes, *sector_id_bytes, context.budget.max_memory_bytes) ||
      !add_memory(*allocation_bytes, *entry_bytes, context.budget.max_memory_bytes) ||
      !add_memory(*allocation_bytes, *incoming_bytes, context.budget.max_memory_bytes) ||
      !add_memory(*allocation_bytes, *state_bytes, context.budget.max_memory_bytes) ||
      !add_memory(*allocation_bytes, *stack_bytes, context.budget.max_memory_bytes) ||
      !add_memory(
          *allocation_bytes, table.header.sector_size, context.budget.max_memory_bytes))
  {
    return std::unexpected { resource_error(
        "CFB directory index exceeds the memory budget") };
  }
  return {};
}

} // namespace

auto read_directory_index(io::SourceLease const &source, OperationContext const &context)
    -> Result<DirectoryIndex>
{
  auto allocation = read_sector_allocation_table(source, context);
  if (!allocation)
  {
    return std::unexpected { std::move(allocation.error()) };
  }
  auto const &header = allocation->header;

  auto directory_sectors = std::vector<std::uint32_t> {};
  auto current           = header.first_directory_sector;
  while (true)
  {
    if (context.cancellation.is_cancelled())
    {
      return std::unexpected { cancelled_error() };
    }
    if (directory_sectors.size() >= context.budget.max_cfb_stream_chain_length)
    {
      return std::unexpected { resource_error(
          "CFB directory chain exceeds the resource budget") };
    }
    if (current >= header.sector_count)
    {
      return std::unexpected { make_error(
          CfbErrorCode::SectorOutOfRange,
          "CFB directory chain references a sector outside the physical file") };
    }
    auto const next = allocation->fat_entries[current];
    if (next == kFatSector || next == kDifatSector || next == kFreeSector)
    {
      return std::unexpected { invalid_directory(
          "CFB directory chain references an allocation table or free sector") };
    }
    directory_sectors.push_back(current);
    if (next == kEndOfChain)
    {
      break;
    }
    current = next;
  }

  if (header.version == CfbVersion::Version4 &&
      directory_sectors.size() != header.directory_sector_count)
  {
    return std::unexpected { invalid_directory(
        "CFB directory chain length does not match the header") };
  }
  auto const entries_per_sector = header.sector_size / kDirectoryEntrySize;
  auto const entry_count = checked_multiply(directory_sectors.size(), entries_per_sector);
  if (!entry_count || *entry_count > context.budget.max_cfb_directory_entry_count ||
      *entry_count >
          static_cast<std::uint64_t>(std::numeric_limits<std::uint32_t>::max()) + 1 ||
      *entry_count > std::numeric_limits<std::size_t>::max())
  {
    return std::unexpected { resource_error(
        "CFB directory entry count exceeds the resource budget") };
  }
  auto status = validate_memory_budget(
      *allocation, directory_sectors.size(), *entry_count, context);
  if (!status)
  {
    return std::unexpected { std::move(status.error()) };
  }

  auto result = DirectoryIndex {
    .allocation_table     = std::move(*allocation),
    .directory_sector_ids = std::move(directory_sectors),
  };
  result.entries.reserve(static_cast<std::size_t>(*entry_count));
  auto sector_bytes = std::vector<std::byte>(header.sector_size);
  for (auto sector_index = std::size_t {};
       sector_index < result.directory_sector_ids.size(); ++sector_index)
  {
    status = internal::read_sector(
        source, header, result.directory_sector_ids[sector_index], sector_bytes, context,
        CfbErrorCode::InvalidDirectory, "failed to read CFB directory sector");
    if (!status)
    {
      return std::unexpected { std::move(status.error()) };
    }
    auto const view = std::span<std::byte const> { sector_bytes };
    for (auto slot = std::uint64_t {}; slot < entries_per_sector; ++slot)
    {
      auto const entry_index = static_cast<std::uint32_t>(result.entries.size());
      auto entry             = parse_entry(
          view.subspan(
              static_cast<std::size_t>(slot * kDirectoryEntrySize),
              static_cast<std::size_t>(kDirectoryEntrySize)),
          entry_index, header.version, context);
      if (!entry)
      {
        return std::unexpected { std::move(entry.error()) };
      }
      result.entries.push_back(std::move(*entry));
    }
    if (context.progress != nullptr)
    {
      context.progress->report(
          ProgressUpdate { "cfb.directory", static_cast<std::uint64_t>(sector_index) + 1,
                           result.directory_sector_ids.size() });
    }
  }

  status = validate_directory_tree(result.entries, context);
  if (!status)
  {
    return std::unexpected { std::move(status.error()) };
  }
  return result;
}

} // namespace xmole2::cfb
