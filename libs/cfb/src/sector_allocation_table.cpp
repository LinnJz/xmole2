#include "xmole2/cfb/sector_allocation_table.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <string_view>
#include <utility>
#include <vector>

#include "cfb_internal_utils.hpp"
#include "compound_file_header_internal.hpp"
#include "sector_reader_internal.hpp"
#include "xmole2/cfb/error.hpp"

namespace xmole2::cfb
{
namespace
{

constexpr auto kDifatSector = std::uint32_t { 0xff'ff'ff'fcU };
constexpr auto kFatSector   = std::uint32_t { 0xff'ff'ff'fdU };
constexpr auto kEndOfChain  = std::uint32_t { 0xff'ff'ff'feU };
constexpr auto kFreeSector  = std::uint32_t { 0xff'ff'ff'ffU };

enum class SectorRole : std::uint8_t
{
  Unassigned,
  Fat,
  Difat,
};

using internal::add_memory;
using internal::checked_multiply;
using internal::read_u32;

auto cancelled_error() -> Error
{
  return make_error(CfbErrorCode::Cancelled, "CFB sector table read cancelled");
}

auto invalid_table(std::string_view const message) -> Error
{
  return make_error(CfbErrorCode::InvalidSectorTable, message);
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
    CompoundFileHeader const &header,
    std::uint64_t const fat_entry_count,
    OperationContext const &context) -> Status
{
  auto total = std::uint64_t {};
  auto const fat_id_bytes =
      checked_multiply(header.fat_sector_count, sizeof(std::uint32_t));
  auto const difat_id_bytes =
      checked_multiply(header.difat_sector_count, sizeof(std::uint32_t));
  auto const fat_entry_bytes = checked_multiply(fat_entry_count, sizeof(std::uint32_t));
  auto const role_bytes      = checked_multiply(header.sector_count, sizeof(SectorRole));
  auto const state_bytes = checked_multiply(header.sector_count, sizeof(std::uint8_t));
  auto const path_bytes  = checked_multiply(header.sector_count, sizeof(std::uint32_t));
  if (!fat_id_bytes || !difat_id_bytes || !fat_entry_bytes || !role_bytes ||
      !state_bytes || !path_bytes ||
      !add_memory(total, *fat_id_bytes, context.budget.max_memory_bytes) ||
      !add_memory(total, *difat_id_bytes, context.budget.max_memory_bytes) ||
      !add_memory(total, *fat_entry_bytes, context.budget.max_memory_bytes) ||
      !add_memory(total, *role_bytes, context.budget.max_memory_bytes) ||
      !add_memory(total, *state_bytes, context.budget.max_memory_bytes) ||
      !add_memory(total, *path_bytes, context.budget.max_memory_bytes) ||
      !add_memory(total, header.sector_size, context.budget.max_memory_bytes))
  {
    return std::unexpected { resource_error(
        "CFB sector tables exceed the memory budget") };
  }
  return {};
}

auto validate_fat_entries(
    SectorAllocationTable const &table,
    std::span<SectorRole const> const roles,
    OperationContext const &context) -> Status
{
  auto const physical_count = table.header.sector_count;
  if (table.fat_entries.size() < physical_count)
  {
    return std::unexpected { invalid_table(
        "CFB FAT does not cover every physical sector") };
  }

  for (auto index = std::uint64_t {}; index < physical_count; ++index)
  {
    if (context.cancellation.is_cancelled())
    {
      return std::unexpected { cancelled_error() };
    }
    auto const entry = table.fat_entries[static_cast<std::size_t>(index)];
    auto const role  = roles[static_cast<std::size_t>(index)];
    if ((role == SectorRole::Fat && entry != kFatSector) ||
        (role == SectorRole::Difat && entry != kDifatSector) ||
        (role == SectorRole::Unassigned &&
         (entry == kFatSector || entry == kDifatSector)))
    {
      return std::unexpected { invalid_table(
          "CFB FAT sector role markers do not match the DIFAT") };
    }
    if (entry != kDifatSector && entry != kFatSector && entry != kEndOfChain &&
        entry != kFreeSector && entry >= physical_count)
    {
      return std::unexpected { out_of_range(
          "CFB FAT entry references a sector outside the physical file") };
    }
    if (entry < physical_count)
    {
      auto const target_role  = roles[static_cast<std::size_t>(entry)];
      auto const target_entry = table.fat_entries[static_cast<std::size_t>(entry)];
      if (target_role != SectorRole::Unassigned || target_entry == kFreeSector ||
          target_entry == kFatSector || target_entry == kDifatSector)
      {
        return std::unexpected { invalid_table(
            "CFB FAT chain references an unallocated or table sector") };
      }
    }
  }

  for (auto index = static_cast<std::size_t>(physical_count);
       index < table.fat_entries.size(); ++index)
  {
    if (table.fat_entries[index] != kFreeSector)
    {
      return std::unexpected { invalid_table(
          "CFB FAT entries beyond the physical file must be free") };
    }
  }
  return {};
}

auto detect_fat_cycles(
    SectorAllocationTable const &table, OperationContext const &context) -> Status
{
  auto states =
      std::vector<std::uint8_t>(static_cast<std::size_t>(table.header.sector_count));
  auto path = std::vector<std::uint32_t> {};
  path.reserve(static_cast<std::size_t>(table.header.sector_count));

  for (auto start = std::uint64_t {}; start < table.header.sector_count; ++start)
  {
    if (states[static_cast<std::size_t>(start)] != 0)
    {
      continue;
    }
    path.clear();
    auto current = start;
    while (current < table.header.sector_count)
    {
      if (context.cancellation.is_cancelled())
      {
        return std::unexpected { cancelled_error() };
      }
      auto &state = states[static_cast<std::size_t>(current)];
      if (state == 1)
      {
        return std::unexpected { make_error(
            CfbErrorCode::SectorChainCycle, "CFB FAT contains a sector chain cycle") };
      }
      if (state == 2)
      {
        break;
      }
      auto const entry = table.fat_entries[static_cast<std::size_t>(current)];
      if (entry == kFreeSector || entry == kFatSector || entry == kDifatSector)
      {
        state = 2;
        break;
      }
      if (path.size() >= context.budget.max_cfb_stream_chain_length)
      {
        return std::unexpected { resource_error(
            "CFB FAT chain exceeds the resource budget") };
      }
      state = 1;
      path.push_back(static_cast<std::uint32_t>(current));
      current = entry;
    }
    for (auto const sector : path)
    {
      states[sector] = 2;
    }
  }
  return {};
}

} // namespace

auto read_sector_allocation_table(
    io::SourceLease const &source, OperationContext const &context)
    -> Result<SectorAllocationTable>
{
  auto parsed = internal::read_compound_file_header(source, context);
  if (!parsed)
  {
    return std::unexpected { std::move(parsed.error()) };
  }
  auto const &header            = parsed->header;
  auto const entries_per_sector = static_cast<std::uint64_t>(header.sector_size / 4);
  auto const fat_entry_count =
      checked_multiply(header.fat_sector_count, entries_per_sector);
  auto const difat_capacity =
      checked_multiply(header.difat_sector_count, entries_per_sector - 1);
  if (!fat_entry_count || !difat_capacity ||
      *fat_entry_count > std::numeric_limits<std::size_t>::max() ||
      header.sector_count > std::numeric_limits<std::size_t>::max() ||
      static_cast<std::uint64_t>(internal::kHeaderDifatEntryCount) + *difat_capacity <
          header.fat_sector_count)
  {
    return std::unexpected { invalid_table(
        "CFB DIFAT capacity cannot represent the declared FAT") };
  }
  if (header.fat_sector_count <= internal::kHeaderDifatEntryCount &&
      header.difat_sector_count != 0)
  {
    return std::unexpected { invalid_table(
        "CFB DIFAT sectors are not allowed when the header can list every FAT sector") };
  }

  auto status = validate_memory_budget(header, *fat_entry_count, context);
  if (!status)
  {
    return std::unexpected { std::move(status.error()) };
  }

  auto table = SectorAllocationTable { .header = header };
  table.fat_sector_ids.reserve(header.fat_sector_count);
  table.difat_sector_ids.reserve(header.difat_sector_count);
  table.fat_entries.reserve(static_cast<std::size_t>(*fat_entry_count));
  auto roles = std::vector<SectorRole>(
      static_cast<std::size_t>(header.sector_count), SectorRole::Unassigned);

  auto add_fat_sector = [&](std::uint32_t const sector) -> Status
  {
    if (sector >= header.sector_count)
    {
      return std::unexpected { out_of_range(
          "CFB DIFAT references a FAT sector outside the physical file") };
    }
    auto &role = roles[sector];
    if (role != SectorRole::Unassigned)
    {
      return std::unexpected { invalid_table(
          "CFB DIFAT contains a duplicate or conflicting sector") };
    }
    role = SectorRole::Fat;
    table.fat_sector_ids.push_back(sector);
    return {};
  };

  auto encountered_free = false;
  for (auto const sector : parsed->difat)
  {
    if (sector == kFreeSector)
    {
      encountered_free = true;
      continue;
    }
    if (encountered_free || table.fat_sector_ids.size() >= header.fat_sector_count)
    {
      return std::unexpected { invalid_table(
          "CFB header DIFAT entries are not contiguous") };
    }
    status = add_fat_sector(sector);
    if (!status)
    {
      return std::unexpected { std::move(status.error()) };
    }
  }

  auto sector_bytes         = std::vector<std::byte>(header.sector_size);
  auto current_difat_sector = header.first_difat_sector;
  for (auto index = std::uint32_t {}; index < header.difat_sector_count; ++index)
  {
    if (context.cancellation.is_cancelled())
    {
      return std::unexpected { cancelled_error() };
    }
    if (current_difat_sector >= header.sector_count)
    {
      return std::unexpected { out_of_range(
          "CFB DIFAT chain references a sector outside the physical file") };
    }
    auto &role = roles[current_difat_sector];
    if (role == SectorRole::Difat)
    {
      return std::unexpected { make_error(
          CfbErrorCode::SectorChainCycle, "CFB DIFAT chain contains a cycle") };
    }
    if (role != SectorRole::Unassigned)
    {
      return std::unexpected { invalid_table(
          "CFB sector is assigned to both DIFAT and FAT") };
    }
    role = SectorRole::Difat;
    table.difat_sector_ids.push_back(current_difat_sector);

    status = internal::read_sector(
        source, header, current_difat_sector, sector_bytes, context,
        CfbErrorCode::InvalidSectorTable, "failed to read CFB DIFAT sector");
    if (!status)
    {
      return std::unexpected { std::move(status.error()) };
    }
    auto const view = std::span<std::byte const> { sector_bytes };
    for (auto entry = std::uint64_t {}; entry + 1 < entries_per_sector; ++entry)
    {
      auto const sector = read_u32(view, static_cast<std::size_t>(entry * 4));
      if (table.fat_sector_ids.size() < header.fat_sector_count)
      {
        if (sector == kFreeSector)
        {
          return std::unexpected { invalid_table(
              "CFB DIFAT ended before all FAT sectors were listed") };
        }
        status = add_fat_sector(sector);
        if (!status)
        {
          return std::unexpected { std::move(status.error()) };
        }
      }
      else if (sector != kFreeSector)
      {
        return std::unexpected { invalid_table(
            "CFB DIFAT padding entries must be free") };
      }
    }

    auto const next =
        read_u32(view, static_cast<std::size_t>((entries_per_sector - 1) * 4));
    auto const last = index + 1 == header.difat_sector_count;
    if (last)
    {
      if (next != kEndOfChain)
      {
        if (next < header.sector_count && roles[next] == SectorRole::Difat)
        {
          return std::unexpected { make_error(
              CfbErrorCode::SectorChainCycle, "CFB DIFAT chain contains a cycle") };
        }
        if (next != kDifatSector && next != kFatSector && next != kFreeSector &&
            next >= header.sector_count)
        {
          return std::unexpected { out_of_range(
              "CFB DIFAT next sector is outside the physical file") };
        }
        return std::unexpected { invalid_table(
            "CFB DIFAT chain does not end at its declared length") };
      }
    }
    else
    {
      if (next >= header.sector_count)
      {
        return std::unexpected { out_of_range(
            "CFB DIFAT chain ended early or points outside the physical file") };
      }
      if (roles[next] == SectorRole::Difat)
      {
        return std::unexpected { make_error(
            CfbErrorCode::SectorChainCycle, "CFB DIFAT chain contains a cycle") };
      }
      current_difat_sector = next;
    }

    if (context.progress != nullptr)
    {
      context.progress->report(
          ProgressUpdate { "cfb.difat", static_cast<std::uint64_t>(index) + 1,
                           header.difat_sector_count });
    }
  }

  if (table.fat_sector_ids.size() != header.fat_sector_count)
  {
    return std::unexpected { invalid_table(
        "CFB DIFAT count does not match the declared FAT sector count") };
  }

  for (auto index = std::size_t {}; index < table.fat_sector_ids.size(); ++index)
  {
    status = internal::read_sector(
        source, header, table.fat_sector_ids[index], sector_bytes, context,
        CfbErrorCode::InvalidSectorTable, "failed to read CFB FAT sector");
    if (!status)
    {
      return std::unexpected { std::move(status.error()) };
    }
    auto const view = std::span<std::byte const> { sector_bytes };
    for (auto entry = std::uint64_t {}; entry < entries_per_sector; ++entry)
    {
      table.fat_entries.push_back(read_u32(view, static_cast<std::size_t>(entry * 4)));
    }
    if (context.progress != nullptr)
    {
      context.progress->report(
          ProgressUpdate { "cfb.fat", static_cast<std::uint64_t>(index) + 1,
                           header.fat_sector_count });
    }
  }

  status = validate_fat_entries(table, roles, context);
  if (!status)
  {
    return std::unexpected { std::move(status.error()) };
  }
  status = detect_fat_cycles(table, context);
  if (!status)
  {
    return std::unexpected { std::move(status.error()) };
  }
  return table;
}

} // namespace xmole2::cfb
