#include "xmole2/cfb/cfb_stream_reader.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string_view>
#include <utility>
#include <vector>

#include "cfb_internal_utils.hpp"
#include "xmole2/cfb/error.hpp"
#include "xmole2/cfb/mini_stream_allocation_table.hpp"
#include "xmole2/io/random_access_reader.hpp"

namespace xmole2::cfb
{
namespace
{

constexpr auto kEndOfChain  = std::uint32_t { 0xff'ff'ff'feU };
constexpr auto kNoOwner     = std::uint32_t { 0xff'ff'ff'ffU };
constexpr auto kMetadata    = std::uint32_t { 0xff'ff'ff'feU };
constexpr auto kMaximumName = std::uint64_t { 64 };

using internal::add_memory;
using internal::ceiling_divide;
using internal::checked_add;
using internal::checked_multiply;
using internal::wrap_io_error;

auto cancelled_error() -> Error
{
  return make_error(CfbErrorCode::Cancelled, "CFB stream operation cancelled");
}

auto invalid_stream(std::string_view const message) -> Error
{
  return make_error(CfbErrorCode::InvalidStream, message);
}

auto resource_error(std::string_view const message) -> Error
{
  return make_error(CfbErrorCode::ResourceLimitExceeded, message);
}

auto validate_owner_memory(
    MiniStreamAllocationTable const &table, OperationContext const &context) -> Status
{
  auto const &directory  = table.directory_index;
  auto const &allocation = directory.allocation_table;
  auto total             = static_cast<std::uint64_t>(sizeof(table));
  auto const fat_ids =
      checked_multiply(allocation.fat_sector_ids.size(), sizeof(std::uint32_t));
  auto const difat_ids =
      checked_multiply(allocation.difat_sector_ids.size(), sizeof(std::uint32_t));
  auto const fat_entries =
      checked_multiply(allocation.fat_entries.size(), sizeof(std::uint32_t));
  auto const directory_ids =
      checked_multiply(directory.directory_sector_ids.size(), sizeof(std::uint32_t));
  auto const directory_entries =
      checked_multiply(directory.entries.size(), sizeof(DirectoryEntry) + kMaximumName);
  auto const mini_fat_ids =
      checked_multiply(table.mini_fat_sector_ids.size(), sizeof(std::uint32_t));
  auto const mini_fat_entries =
      checked_multiply(table.mini_fat_entries.size(), sizeof(std::uint32_t));
  auto const root_ids =
      checked_multiply(table.root_mini_stream_sector_ids.size(), sizeof(std::uint32_t));
  auto const regular_owners =
      checked_multiply(allocation.header.sector_count, sizeof(std::uint32_t));
  auto const mini_owners =
      checked_multiply(table.root_mini_sector_count, sizeof(std::uint32_t));
  if (!fat_ids || !difat_ids || !fat_entries || !directory_ids || !directory_entries ||
      !mini_fat_ids || !mini_fat_entries || !root_ids || !regular_owners ||
      !mini_owners || !add_memory(total, *fat_ids, context.budget.max_memory_bytes) ||
      !add_memory(total, *difat_ids, context.budget.max_memory_bytes) ||
      !add_memory(total, *fat_entries, context.budget.max_memory_bytes) ||
      !add_memory(total, *directory_ids, context.budget.max_memory_bytes) ||
      !add_memory(total, *directory_entries, context.budget.max_memory_bytes) ||
      !add_memory(total, *mini_fat_ids, context.budget.max_memory_bytes) ||
      !add_memory(total, *mini_fat_entries, context.budget.max_memory_bytes) ||
      !add_memory(total, *root_ids, context.budget.max_memory_bytes) ||
      !add_memory(total, *regular_owners, context.budget.max_memory_bytes) ||
      !add_memory(total, *mini_owners, context.budget.max_memory_bytes))
  {
    return std::unexpected { resource_error(
        "CFB stream allocation validation exceeds the memory budget") };
  }
  return {};
}

auto mark_metadata_sector(
    std::span<std::uint32_t> const owners, std::uint32_t const sector) -> Status
{
  if (sector >= owners.size() || owners[sector] != kNoOwner)
  {
    return std::unexpected { invalid_stream(
        "CFB allocation metadata contains conflicting sector roles") };
  }
  owners[sector] = kMetadata;
  return {};
}

auto mark_metadata(
    MiniStreamAllocationTable const &table, std::span<std::uint32_t> const owners)
    -> Status
{
  auto const mark_all = [&](std::span<std::uint32_t const> const sectors) -> Status
  {
    for (auto const sector : sectors)
    {
      auto status = mark_metadata_sector(owners, sector);
      if (!status)
      {
        return status;
      }
    }
    return {};
  };

  auto status = mark_all(table.directory_index.allocation_table.fat_sector_ids);
  if (!status)
  {
    return status;
  }
  status = mark_all(table.directory_index.allocation_table.difat_sector_ids);
  if (!status)
  {
    return status;
  }
  status = mark_all(table.directory_index.directory_sector_ids);
  if (!status)
  {
    return status;
  }
  status = mark_all(table.mini_fat_sector_ids);
  if (!status)
  {
    return status;
  }
  return mark_all(table.root_mini_stream_sector_ids);
}

auto validate_regular_chain(
    DirectoryEntry const &entry,
    SectorAllocationTable const &allocation,
    std::span<std::uint32_t> const owners,
    OperationContext const &context) -> Status
{
  auto const required = ceiling_divide(entry.stream_size, allocation.header.sector_size);
  if (required > context.budget.max_cfb_stream_chain_length)
  {
    return std::unexpected { resource_error(
        "CFB regular stream chain exceeds the resource budget") };
  }
  auto current = entry.starting_sector;
  for (auto index = std::uint64_t {}; index < required; ++index)
  {
    if (context.cancellation.is_cancelled())
    {
      return std::unexpected { cancelled_error() };
    }
    if (current >= allocation.header.sector_count ||
        current >= allocation.fat_entries.size())
    {
      return std::unexpected { invalid_stream(
          "CFB regular stream references a sector outside the physical file") };
    }
    if (owners[current] != kNoOwner)
    {
      return std::unexpected { invalid_stream(
          "CFB regular stream sector is shared or used by container metadata") };
    }
    owners[current] = entry.index;

    auto const next = allocation.fat_entries[current];
    auto const last = index + 1 == required;
    if (last)
    {
      if (next != kEndOfChain)
      {
        return std::unexpected { invalid_stream(
            "CFB regular stream chain does not match its declared size") };
      }
    }
    else
    {
      if (next >= allocation.header.sector_count)
      {
        return std::unexpected { invalid_stream(
            "CFB regular stream chain ends before its declared size") };
      }
      current = next;
    }
  }
  return {};
}

auto validate_mini_chain(
    DirectoryEntry const &entry,
    MiniStreamAllocationTable const &table,
    std::span<std::uint32_t> const owners,
    OperationContext const &context) -> Status
{
  auto const mini_sector_size =
      table.directory_index.allocation_table.header.mini_sector_size;
  auto const required = ceiling_divide(entry.stream_size, mini_sector_size);
  if (required > context.budget.max_cfb_stream_chain_length)
  {
    return std::unexpected { resource_error(
        "CFB mini stream chain exceeds the resource budget") };
  }
  auto current = entry.starting_sector;
  for (auto index = std::uint64_t {}; index < required; ++index)
  {
    if (context.cancellation.is_cancelled())
    {
      return std::unexpected { cancelled_error() };
    }
    if (current >= table.root_mini_sector_count ||
        current >= table.mini_fat_entries.size())
    {
      return std::unexpected { invalid_stream(
          "CFB mini stream references a mini-sector outside the root mini stream") };
    }
    if (owners[current] != kNoOwner)
    {
      return std::unexpected { invalid_stream(
          "CFB mini stream sector is shared by multiple directory entries") };
    }
    owners[current] = entry.index;

    auto const next = table.mini_fat_entries[current];
    auto const last = index + 1 == required;
    if (last)
    {
      if (next != kEndOfChain)
      {
        return std::unexpected { invalid_stream(
            "CFB mini stream chain does not match its declared size") };
      }
    }
    else
    {
      if (next >= table.root_mini_sector_count)
      {
        return std::unexpected { invalid_stream(
            "CFB mini stream chain ends before its declared size") };
      }
      current = next;
    }
  }
  return {};
}

auto validate_stream_chains(
    MiniStreamAllocationTable const &table, OperationContext const &context) -> Status
{
  auto stream_count = std::uint64_t {};
  for (auto const &entry : table.directory_index.entries)
  {
    if (context.cancellation.is_cancelled())
    {
      return std::unexpected { cancelled_error() };
    }
    if (entry.type == DirectoryEntryType::Stream && entry.stream_size != 0)
    {
      ++stream_count;
    }
  }

  auto status = validate_owner_memory(table, context);
  if (!status)
  {
    return status;
  }

  auto regular_owners = std::vector<std::uint32_t>(
      static_cast<std::size_t>(
          table.directory_index.allocation_table.header.sector_count),
      kNoOwner);
  auto mini_owners = std::vector<std::uint32_t>(
      static_cast<std::size_t>(table.root_mini_sector_count), kNoOwner);
  status = mark_metadata(table, regular_owners);
  if (!status)
  {
    return status;
  }

  auto const &header          = table.directory_index.allocation_table.header;
  auto validated_stream_count = std::uint64_t {};
  for (auto const &entry : table.directory_index.entries)
  {
    if (entry.type != DirectoryEntryType::Stream || entry.stream_size == 0)
    {
      continue;
    }
    if (entry.stream_size > context.budget.max_single_resource_bytes)
    {
      return std::unexpected { resource_error(
          "CFB stream exceeds the single-resource budget") };
    }
    if (entry.stream_size < header.mini_stream_cutoff_size)
    {
      status = validate_mini_chain(entry, table, mini_owners, context);
    }
    else
    {
      status = validate_regular_chain(
          entry, table.directory_index.allocation_table, regular_owners, context);
    }
    if (!status)
    {
      return status;
    }
    ++validated_stream_count;
    if (context.progress != nullptr)
    {
      context.progress->report(
          ProgressUpdate { "cfb.stream.validate", validated_stream_count, stream_count });
    }
  }
  return {};
}

} // namespace

struct CfbStreamReader::Impl
{
  io::RandomAccessReader source;
  CompoundFileHeader header;
  std::vector<std::uint32_t> allocation_entries;
  std::vector<std::uint32_t> root_mini_stream_sector_ids;
  CfbStreamStorage storage { CfbStreamStorage::Regular };
  std::uint32_t current_sector { kEndOfChain };
  std::uint64_t stream_size {};
  std::uint64_t position {};
  std::uint64_t required_sector_count {};
};

CfbStreamReader::CfbStreamReader() noexcept = default;

CfbStreamReader::CfbStreamReader(std::unique_ptr<Impl> impl)
    : m_impl { std::move(impl) }
{
}

CfbStreamReader::CfbStreamReader(CfbStreamReader &&other) noexcept = default;

auto CfbStreamReader::operator= (CfbStreamReader &&other) noexcept
    -> CfbStreamReader & = default;

CfbStreamReader::~CfbStreamReader() = default;

auto CfbStreamReader::open(
    io::SourceLease const &source,
    std::uint32_t const directory_entry_id,
    OperationContext const &context) -> Result<CfbStreamReader>
{
  if (context.cancellation.is_cancelled())
  {
    return std::unexpected { cancelled_error() };
  }
  auto table = read_mini_stream_allocation_table(source, context);
  if (!table)
  {
    return std::unexpected { std::move(table.error()) };
  }
  if (directory_entry_id >= table->directory_index.entries.size() ||
      table->directory_index.entries[directory_entry_id].type !=
          DirectoryEntryType::Stream)
  {
    return std::unexpected { invalid_stream(
        "CFB directory entry does not identify a stream") };
  }

  auto status = validate_stream_chains(*table, context);
  if (!status)
  {
    return std::unexpected { std::move(status.error()) };
  }

  auto source_reader = source.reader(0, context);
  if (!source_reader)
  {
    return std::unexpected { wrap_io_error(
        "failed to create CFB stream source reader", std::move(source_reader.error()),
        CfbErrorCode::InvalidStream) };
  }

  auto const &entry    = table->directory_index.entries[directory_entry_id];
  auto const &header   = table->directory_index.allocation_table.header;
  auto impl            = std::make_unique<Impl>();
  impl->source         = std::move(*source_reader);
  impl->header         = header;
  impl->stream_size    = entry.stream_size;
  impl->current_sector = entry.starting_sector;
  if (entry.stream_size < header.mini_stream_cutoff_size)
  {
    impl->storage = CfbStreamStorage::Mini;
    impl->required_sector_count =
        ceiling_divide(entry.stream_size, header.mini_sector_size);
    impl->allocation_entries          = std::move(table->mini_fat_entries);
    impl->root_mini_stream_sector_ids = std::move(table->root_mini_stream_sector_ids);
  }
  else
  {
    impl->storage               = CfbStreamStorage::Regular;
    impl->required_sector_count = ceiling_divide(entry.stream_size, header.sector_size);
    impl->allocation_entries =
        std::move(table->directory_index.allocation_table.fat_entries);
  }
  return CfbStreamReader { std::move(impl) };
}

auto CfbStreamReader::read(
    std::span<std::byte> const destination, OperationContext const &context)
    -> Result<std::size_t>
{
  if (!m_impl)
  {
    return std::unexpected { make_error(
        CfbErrorCode::InvalidArgument, "CFB stream reader is empty") };
  }
  if (context.cancellation.is_cancelled())
  {
    return std::unexpected { cancelled_error() };
  }
  if (m_impl->stream_size > context.budget.max_single_resource_bytes ||
      m_impl->required_sector_count > context.budget.max_cfb_stream_chain_length ||
      destination.size() > context.budget.max_single_resource_bytes)
  {
    return std::unexpected { resource_error(
        "CFB stream read exceeds the current resource budget") };
  }
  if (m_impl->position == m_impl->stream_size)
  {
    return std::size_t {};
  }
  if (destination.empty())
  {
    return std::unexpected { make_error(
        CfbErrorCode::InvalidArgument,
        "CFB stream read destination must not be empty before EOF") };
  }

  auto const remaining = m_impl->stream_size - m_impl->position;
  auto const request =
      static_cast<std::size_t>(std::min<std::uint64_t>(remaining, destination.size()));
  auto completed = std::size_t {};
  while (completed < request)
  {
    if (context.cancellation.is_cancelled())
    {
      return std::unexpected { cancelled_error() };
    }
    auto const unit_size =
        m_impl->storage == CfbStreamStorage::Regular
            ? m_impl->header.sector_size
            : m_impl->header.mini_sector_size;
    auto const within_unit = m_impl->position % unit_size;
    auto const count       = static_cast<std::size_t>(
        std::min<std::uint64_t>(request - completed, unit_size - within_unit));

    auto physical_sector = m_impl->current_sector;
    auto physical_within = within_unit;
    if (m_impl->storage == CfbStreamStorage::Mini)
    {
      auto const mini_offset =
          checked_multiply(m_impl->current_sector, m_impl->header.mini_sector_size);
      if (!mini_offset)
      {
        return std::unexpected { invalid_stream("CFB mini stream offset overflows") };
      }
      auto const root_index = *mini_offset / m_impl->header.sector_size;
      if (root_index >= m_impl->root_mini_stream_sector_ids.size())
      {
        return std::unexpected { invalid_stream(
            "CFB mini stream mapping is outside the root mini stream") };
      }
      physical_sector =
          m_impl->root_mini_stream_sector_ids[static_cast<std::size_t>(root_index)];
      physical_within = (*mini_offset % m_impl->header.sector_size) + within_unit;
    }

    auto const sector_number = static_cast<std::uint64_t>(physical_sector) + 1;
    auto const sector_offset =
        checked_multiply(sector_number, m_impl->header.sector_size);
    auto const file_offset =
        sector_offset ? checked_add(*sector_offset, physical_within) : std::nullopt;
    if (!file_offset)
    {
      return std::unexpected { invalid_stream("CFB stream file offset overflows") };
    }

    auto status = m_impl->source.seek(*file_offset, context);
    if (!status)
    {
      return std::unexpected { wrap_io_error(
          "failed to seek to CFB stream payload", std::move(status.error()),
          CfbErrorCode::InvalidStream) };
    }
    status = m_impl->source.read_exact(destination.subspan(completed, count), context);
    if (!status)
    {
      return std::unexpected { wrap_io_error(
          "failed to read CFB stream payload", std::move(status.error()),
          CfbErrorCode::InvalidStream) };
    }

    completed += count;
    m_impl->position += count;
    if (m_impl->position < m_impl->stream_size && (m_impl->position % unit_size) == 0)
    {
      if (m_impl->current_sector >= m_impl->allocation_entries.size())
      {
        return std::unexpected { invalid_stream(
            "CFB stream allocation changed after validation") };
      }
      auto const next = m_impl->allocation_entries[m_impl->current_sector];
      if (next == kEndOfChain || next >= m_impl->allocation_entries.size())
      {
        return std::unexpected { invalid_stream(
            "CFB stream chain ended after validation") };
      }
      m_impl->current_sector = next;
    }

    if (context.progress != nullptr)
    {
      context.progress->report(
          ProgressUpdate { "cfb.stream.read", m_impl->position, m_impl->stream_size });
    }
  }
  return completed;
}

auto CfbStreamReader::position() const noexcept -> std::uint64_t
{
  return m_impl ? m_impl->position : 0;
}

auto CfbStreamReader::size() const noexcept -> std::uint64_t
{
  return m_impl ? m_impl->stream_size : 0;
}

auto CfbStreamReader::storage() const noexcept -> CfbStreamStorage
{
  return m_impl ? m_impl->storage : CfbStreamStorage::Regular;
}

auto CfbStreamReader::finished() const noexcept -> bool
{
  return m_impl != nullptr && m_impl->position == m_impl->stream_size;
}

} // namespace xmole2::cfb
