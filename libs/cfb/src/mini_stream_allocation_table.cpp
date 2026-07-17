#include "xmole2/cfb/mini_stream_allocation_table.hpp"

#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <string_view>
#include <utility>
#include <vector>

#include "cfb_internal_utils.hpp"
#include "sector_reader_internal.hpp"
#include "xmole2/cfb/error.hpp"

namespace xmole2::cfb
{
namespace
{

constexpr auto kEndOfChain = std::uint32_t { 0xff'ff'ff'feU };
constexpr auto kFreeSector = std::uint32_t { 0xff'ff'ff'ffU };

enum class SectorRole : std::uint8_t
{
  Unassigned,
  Fat,
  Difat,
  Directory,
  MiniFat,
  RootMiniStream,
};

using internal::add_memory;
using internal::ceiling_divide;
using internal::checked_multiply;
using internal::read_u32;

auto cancelled_error() -> Error
{
  return make_error(CfbErrorCode::Cancelled, "CFB mini-stream mapping cancelled");
}

auto invalid_mini_fat(std::string_view const message) -> Error
{
  return make_error(CfbErrorCode::InvalidMiniFat, message);
}

auto invalid_mini_stream(std::string_view const message) -> Error
{
  return make_error(CfbErrorCode::InvalidMiniStream, message);
}

auto out_of_range(std::string_view const message) -> Error
{
  return make_error(CfbErrorCode::SectorOutOfRange, message);
}

auto resource_error(std::string_view const message) -> Error
{
  return make_error(CfbErrorCode::ResourceLimitExceeded, message);
}

auto validate_memory_budget(
    DirectoryIndex const &directory,
    std::uint64_t const mini_fat_entry_count,
    std::uint64_t const root_sector_count,
    std::uint64_t const root_mini_sector_count,
    OperationContext const &context) -> Status
{
  auto const &table = directory.allocation_table;
  auto total        = static_cast<std::uint64_t>(sizeof(MiniStreamAllocationTable));
  auto const fat_ids =
      checked_multiply(table.fat_sector_ids.size(), sizeof(std::uint32_t));
  auto const difat_ids =
      checked_multiply(table.difat_sector_ids.size(), sizeof(std::uint32_t));
  auto const fat_entries =
      checked_multiply(table.fat_entries.size(), sizeof(std::uint32_t));
  auto const directory_ids =
      checked_multiply(directory.directory_sector_ids.size(), sizeof(std::uint32_t));
  auto const directory_entries = checked_multiply(
      directory.entries.size(), sizeof(DirectoryEntry) + std::uint64_t { 64 });
  auto const mini_fat_ids =
      checked_multiply(table.header.mini_fat_sector_count, sizeof(std::uint32_t));
  auto const mini_fat_entries =
      checked_multiply(mini_fat_entry_count, sizeof(std::uint32_t));
  auto const root_ids   = checked_multiply(root_sector_count, sizeof(std::uint32_t));
  auto const role_bytes = checked_multiply(table.header.sector_count, sizeof(SectorRole));
  auto const state_bytes = checked_multiply(root_mini_sector_count, sizeof(std::uint8_t));
  auto const incoming_bytes =
      checked_multiply(root_mini_sector_count, sizeof(std::uint32_t));
  auto const path_bytes = checked_multiply(root_mini_sector_count, sizeof(std::uint32_t));
  if (!fat_ids || !difat_ids || !fat_entries || !directory_ids || !directory_entries ||
      !mini_fat_ids || !mini_fat_entries || !root_ids || !role_bytes || !state_bytes ||
      !incoming_bytes || !path_bytes ||
      !add_memory(total, *fat_ids, context.budget.max_memory_bytes) ||
      !add_memory(total, *difat_ids, context.budget.max_memory_bytes) ||
      !add_memory(total, *fat_entries, context.budget.max_memory_bytes) ||
      !add_memory(total, *directory_ids, context.budget.max_memory_bytes) ||
      !add_memory(total, *directory_entries, context.budget.max_memory_bytes) ||
      !add_memory(total, *mini_fat_ids, context.budget.max_memory_bytes) ||
      !add_memory(total, *mini_fat_entries, context.budget.max_memory_bytes) ||
      !add_memory(total, *root_ids, context.budget.max_memory_bytes) ||
      !add_memory(total, *role_bytes, context.budget.max_memory_bytes) ||
      !add_memory(total, *state_bytes, context.budget.max_memory_bytes) ||
      !add_memory(total, *incoming_bytes, context.budget.max_memory_bytes) ||
      !add_memory(total, *path_bytes, context.budget.max_memory_bytes) ||
      !add_memory(total, table.header.sector_size, context.budget.max_memory_bytes))
  {
    return std::unexpected { resource_error(
        "CFB MiniFAT metadata exceeds the memory budget") };
  }
  return {};
}

auto mark_existing_roles(
    DirectoryIndex const &directory, std::span<SectorRole> const roles) -> Status
{
  auto mark = [&](std::uint32_t const sector, SectorRole const role) -> Status
  {
    if (sector >= roles.size())
    {
      return std::unexpected { out_of_range(
          "CFB allocation metadata references a sector outside the physical file") };
    }
    if (roles[sector] != SectorRole::Unassigned)
    {
      return std::unexpected { invalid_mini_fat(
          "CFB allocation metadata assigns conflicting sector roles") };
    }
    roles[sector] = role;
    return {};
  };

  for (auto const sector : directory.allocation_table.fat_sector_ids)
  {
    auto status = mark(sector, SectorRole::Fat);
    if (!status)
    {
      return status;
    }
  }
  for (auto const sector : directory.allocation_table.difat_sector_ids)
  {
    auto status = mark(sector, SectorRole::Difat);
    if (!status)
    {
      return status;
    }
  }
  for (auto const sector : directory.directory_sector_ids)
  {
    auto status = mark(sector, SectorRole::Directory);
    if (!status)
    {
      return status;
    }
  }
  return {};
}

auto collect_mini_fat_sectors(
    SectorAllocationTable const &allocation,
    std::span<SectorRole> const roles,
    OperationContext const &context) -> Result<std::vector<std::uint32_t>>
{
  auto const &header = allocation.header;
  if (header.mini_fat_sector_count > context.budget.max_cfb_stream_chain_length)
  {
    return std::unexpected { resource_error(
        "CFB MiniFAT chain exceeds the resource budget") };
  }

  auto sectors = std::vector<std::uint32_t> {};
  sectors.reserve(header.mini_fat_sector_count);
  auto current = header.first_mini_fat_sector;
  for (auto index = std::uint32_t {}; index < header.mini_fat_sector_count; ++index)
  {
    if (context.cancellation.is_cancelled())
    {
      return std::unexpected { cancelled_error() };
    }
    if (current >= header.sector_count)
    {
      return std::unexpected { out_of_range(
          "CFB MiniFAT chain references a sector outside the physical file") };
    }
    if (roles[current] == SectorRole::MiniFat)
    {
      return std::unexpected { make_error(
          CfbErrorCode::SectorChainCycle, "CFB MiniFAT sector chain contains a cycle") };
    }
    if (roles[current] != SectorRole::Unassigned)
    {
      return std::unexpected { invalid_mini_fat(
          "CFB MiniFAT sector conflicts with another sector role") };
    }
    roles[current] = SectorRole::MiniFat;
    sectors.push_back(current);

    auto const next = allocation.fat_entries[current];
    auto const last = index + 1 == header.mini_fat_sector_count;
    if (last)
    {
      if (next != kEndOfChain)
      {
        return std::unexpected { invalid_mini_fat(
            "CFB MiniFAT sector chain exceeds its declared length") };
      }
    }
    else
    {
      if (next >= header.sector_count)
      {
        return std::unexpected { invalid_mini_fat(
            "CFB MiniFAT sector chain ends before its declared length") };
      }
      current = next;
    }
  }
  return sectors;
}

auto collect_root_mini_stream_sectors(
    SectorAllocationTable const &allocation,
    DirectoryEntry const &root,
    std::span<SectorRole> const roles,
    OperationContext const &context) -> Result<std::vector<std::uint32_t>>
{
  auto const &header = allocation.header;
  if (root.stream_size > context.budget.max_single_resource_bytes)
  {
    return std::unexpected { resource_error(
        "CFB root mini-stream exceeds the single-resource budget") };
  }
  auto const sector_count = ceiling_divide(root.stream_size, header.sector_size);
  if (sector_count > context.budget.max_cfb_stream_chain_length)
  {
    return std::unexpected { resource_error(
        "CFB root mini-stream chain exceeds the resource budget") };
  }

  auto sectors = std::vector<std::uint32_t> {};
  sectors.reserve(static_cast<std::size_t>(sector_count));
  auto current = root.starting_sector;
  for (auto index = std::uint64_t {}; index < sector_count; ++index)
  {
    if (context.cancellation.is_cancelled())
    {
      return std::unexpected { cancelled_error() };
    }
    if (current >= header.sector_count)
    {
      return std::unexpected { out_of_range(
          "CFB root mini-stream references a sector outside the physical file") };
    }
    if (roles[current] != SectorRole::Unassigned)
    {
      return std::unexpected { invalid_mini_stream(
          "CFB root mini-stream sector conflicts with another sector role") };
    }
    roles[current] = SectorRole::RootMiniStream;
    sectors.push_back(current);

    auto const next = allocation.fat_entries[current];
    auto const last = index + 1 == sector_count;
    if (last)
    {
      if (next != kEndOfChain)
      {
        return std::unexpected { invalid_mini_stream(
            "CFB root mini-stream chain exceeds the size declared by the root entry") };
      }
    }
    else
    {
      if (next >= header.sector_count)
      {
        return std::unexpected {
          invalid_mini_stream(
              "CFB root mini-stream chain ends before the size declared by the root entry")
        };
      }
      current = next;
    }

    if (context.progress != nullptr)
    {
      context.progress->report(
          ProgressUpdate {
              "cfb.root_mini_stream",
              index + 1,
              sector_count,
          });
    }
  }
  return sectors;
}

auto validate_mini_fat_entries(
    std::span<std::uint32_t const> const entries,
    std::uint64_t const root_mini_sector_count,
    OperationContext const &context) -> Status
{
  if (root_mini_sector_count > entries.size())
  {
    return std::unexpected { invalid_mini_fat(
        "CFB MiniFAT does not cover the root mini-stream") };
  }

  auto incoming =
      std::vector<std::uint32_t>(static_cast<std::size_t>(root_mini_sector_count));
  for (auto index = std::uint64_t {}; index < root_mini_sector_count; ++index)
  {
    if (context.cancellation.is_cancelled())
    {
      return std::unexpected { cancelled_error() };
    }
    auto const entry = entries[static_cast<std::size_t>(index)];
    if (entry == kFreeSector || entry == kEndOfChain)
    {
      continue;
    }
    if (entry >= root_mini_sector_count)
    {
      return std::unexpected { out_of_range(
          "CFB MiniFAT entry references a mini-sector outside the root mini-stream") };
    }
    if (entries[entry] == kFreeSector)
    {
      return std::unexpected { invalid_mini_fat(
          "CFB MiniFAT chain references an unallocated mini-sector") };
    }
    if (++incoming[entry] > 1)
    {
      return std::unexpected { invalid_mini_fat(
          "CFB MiniFAT chains share a mini-sector") };
    }
  }
  for (auto index = static_cast<std::size_t>(root_mini_sector_count);
       index < entries.size(); ++index)
  {
    if (entries[index] != kFreeSector)
    {
      return std::unexpected { invalid_mini_fat(
          "CFB MiniFAT entries beyond the root mini-stream must be free") };
    }
  }

  auto states =
      std::vector<std::uint8_t>(static_cast<std::size_t>(root_mini_sector_count));
  auto path = std::vector<std::uint32_t> {};
  path.reserve(static_cast<std::size_t>(root_mini_sector_count));
  for (auto start = std::uint64_t {}; start < root_mini_sector_count; ++start)
  {
    if (states[static_cast<std::size_t>(start)] != 0)
    {
      continue;
    }
    path.clear();
    auto current = start;
    while (current < root_mini_sector_count)
    {
      if (context.cancellation.is_cancelled())
      {
        return std::unexpected { cancelled_error() };
      }
      auto &state = states[static_cast<std::size_t>(current)];
      if (state == 1)
      {
        return std::unexpected { make_error(
            CfbErrorCode::SectorChainCycle, "CFB MiniFAT contains a mini-sector cycle") };
      }
      if (state == 2)
      {
        break;
      }
      auto const next = entries[static_cast<std::size_t>(current)];
      if (next == kFreeSector)
      {
        state = 2;
        break;
      }
      if (path.size() >= context.budget.max_cfb_stream_chain_length)
      {
        return std::unexpected { resource_error(
            "CFB MiniFAT chain exceeds the resource budget") };
      }
      state = 1;
      path.push_back(static_cast<std::uint32_t>(current));
      if (next == kEndOfChain)
      {
        break;
      }
      current = next;
    }
    for (auto const sector : path)
    {
      states[sector] = 2;
    }
  }
  return {};
}

} // namespace

auto read_mini_stream_allocation_table(
    io::SourceLease const &source, OperationContext const &context)
    -> Result<MiniStreamAllocationTable>
{
  auto directory = read_directory_index(source, context);
  if (!directory)
  {
    return std::unexpected { std::move(directory.error()) };
  }
  auto const &allocation = directory->allocation_table;
  auto const &header     = allocation.header;
  auto const &root       = directory->entries.front();
  if ((root.stream_size % header.mini_sector_size) != 0)
  {
    return std::unexpected { invalid_mini_stream(
        "CFB root mini-stream size is not aligned to the mini-sector size") };
  }

  auto const entries_per_sector = static_cast<std::uint64_t>(header.sector_size / 4);
  auto const mini_fat_entry_count =
      checked_multiply(header.mini_fat_sector_count, entries_per_sector);
  auto const root_sector_count = ceiling_divide(root.stream_size, header.sector_size);
  auto const root_mini_sector_count = root.stream_size / header.mini_sector_size;
  if (!mini_fat_entry_count ||
      *mini_fat_entry_count > std::numeric_limits<std::size_t>::max() ||
      root_sector_count > std::numeric_limits<std::size_t>::max() ||
      root_mini_sector_count > std::numeric_limits<std::size_t>::max())
  {
    return std::unexpected { resource_error(
        "CFB mini-stream metadata exceeds the addressable memory range") };
  }

  auto status = validate_memory_budget(
      *directory, *mini_fat_entry_count, root_sector_count, root_mini_sector_count,
      context);
  if (!status)
  {
    return std::unexpected { std::move(status.error()) };
  }

  auto roles = std::vector<SectorRole>(
      static_cast<std::size_t>(header.sector_count), SectorRole::Unassigned);
  status = mark_existing_roles(*directory, roles);
  if (!status)
  {
    return std::unexpected { std::move(status.error()) };
  }

  auto mini_fat_sectors = collect_mini_fat_sectors(allocation, roles, context);
  if (!mini_fat_sectors)
  {
    return std::unexpected { std::move(mini_fat_sectors.error()) };
  }
  auto root_sectors = collect_root_mini_stream_sectors(allocation, root, roles, context);
  if (!root_sectors)
  {
    return std::unexpected { std::move(root_sectors.error()) };
  }

  auto mini_fat_entries = std::vector<std::uint32_t> {};
  mini_fat_entries.reserve(static_cast<std::size_t>(*mini_fat_entry_count));
  auto sector_bytes = std::vector<std::byte>(header.sector_size);
  for (auto index = std::size_t {}; index < mini_fat_sectors->size(); ++index)
  {
    status = internal::read_sector(
        source, header, (*mini_fat_sectors)[index], sector_bytes, context,
        CfbErrorCode::InvalidMiniFat, "failed to read CFB MiniFAT sector");
    if (!status)
    {
      return std::unexpected { std::move(status.error()) };
    }
    auto const view = std::span<std::byte const> { sector_bytes };
    for (auto entry = std::uint64_t {}; entry < entries_per_sector; ++entry)
    {
      mini_fat_entries.push_back(read_u32(view, static_cast<std::size_t>(entry * 4)));
    }
    if (context.progress != nullptr)
    {
      context.progress->report(
          ProgressUpdate {
              "cfb.minifat",
              static_cast<std::uint64_t>(index) + 1,
              mini_fat_sectors->size(),
          });
    }
  }

  status = validate_mini_fat_entries(mini_fat_entries, root_mini_sector_count, context);
  if (!status)
  {
    return std::unexpected { std::move(status.error()) };
  }

  return MiniStreamAllocationTable {
    .directory_index             = std::move(*directory),
    .mini_fat_sector_ids         = std::move(*mini_fat_sectors),
    .mini_fat_entries            = std::move(mini_fat_entries),
    .root_mini_stream_sector_ids = std::move(*root_sectors),
    .root_mini_sector_count      = root_mini_sector_count,
  };
}

} // namespace xmole2::cfb
