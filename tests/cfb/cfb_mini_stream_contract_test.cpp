#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <optional>
#include <span>
#include <string_view>
#include <utility>
#include <vector>

#include "cfb_test_utils.hpp"
#include "xmole2/cfb/error.hpp"
#include "xmole2/cfb/mini_stream_allocation_table.hpp"
#include "xmole2/io/byte_source.hpp"
#include "xmole2/io/error.hpp"
#include "xmole2/io/source_lease.hpp"

namespace
{

constexpr auto kFatSector  = std::uint32_t { 0xff'ff'ff'fdU };
constexpr auto kEndOfChain = std::uint32_t { 0xff'ff'ff'feU };
constexpr auto kFreeSector = std::uint32_t { 0xff'ff'ff'ffU };
constexpr auto kNoStream   = std::uint32_t { 0xff'ff'ff'ffU };

using cfb_test::fill_sector;
using cfb_test::sector_offset;
using cfb_test::sector_size;
using cfb_test::write_u16;
using cfb_test::write_u32;
using cfb_test::write_u64;

auto write_fat_entry(
    std::vector<std::byte> &bytes,
    std::size_t const size,
    std::uint32_t const sector,
    std::uint32_t const value) -> void
{
  write_u32(
      bytes, sector_offset(0, size) + (static_cast<std::size_t>(sector) * 4), value);
}

auto write_mini_fat_entry(
    std::vector<std::byte> &bytes,
    std::size_t const size,
    std::uint32_t const mini_sector,
    std::uint32_t const value) -> void
{
  write_u32(
      bytes, sector_offset(2, size) + (static_cast<std::size_t>(mini_sector) * 4), value);
}

auto initialize_header(
    std::vector<std::byte> &bytes,
    xmole2::cfb::CfbVersion const version,
    bool const has_mini_fat) -> void
{
  auto const signature = std::array {
    std::byte { 0xd0 }, std::byte { 0xcf }, std::byte { 0x11 }, std::byte { 0xe0 },
    std::byte { 0xa1 }, std::byte { 0xb1 }, std::byte { 0x1a }, std::byte { 0xe1 },
  };
  std::copy(signature.begin(), signature.end(), bytes.begin());
  write_u16(bytes, 24, 0x00'3e);
  write_u16(bytes, 26, static_cast<std::uint16_t>(version));
  write_u16(bytes, 28, 0xff'fe);
  write_u16(bytes, 30, version == xmole2::cfb::CfbVersion::Version3 ? 9 : 12);
  write_u16(bytes, 32, 6);
  write_u32(bytes, 40, version == xmole2::cfb::CfbVersion::Version3 ? 0U : 1U);
  write_u32(bytes, 44, 1);
  write_u32(bytes, 48, 1);
  write_u32(bytes, 56, 0x10'00);
  write_u32(bytes, 60, has_mini_fat ? 2U : kEndOfChain);
  write_u32(bytes, 64, has_mini_fat ? 1U : 0U);
  write_u32(bytes, 68, kEndOfChain);
  write_u32(bytes, 72, 0);
  for (auto index = std::size_t {}; index < 109; ++index)
  {
    write_u32(bytes, 76 + (index * 4), kFreeSector);
  }
  write_u32(bytes, 76, 0);
}

auto write_directory_entry(
    std::vector<std::byte> &bytes,
    std::size_t const size,
    std::uint32_t const index,
    std::u16string_view const name,
    std::uint8_t const type,
    std::uint32_t const child,
    std::uint32_t const starting_sector,
    std::uint64_t const stream_size) -> void
{
  auto const offset = sector_offset(1, size) + (static_cast<std::size_t>(index) * 128);
  for (auto name_index = std::size_t {}; name_index < name.size(); ++name_index)
  {
    write_u16(
        bytes, offset + (name_index * 2), static_cast<std::uint16_t>(name[name_index]));
  }
  write_u16(bytes, offset + 64, static_cast<std::uint16_t>((name.size() + 1) * 2));
  bytes[offset + 66] = static_cast<std::byte>(type);
  bytes[offset + 67] = std::byte { 1 };
  write_u32(bytes, offset + 68, kNoStream);
  write_u32(bytes, offset + 72, kNoStream);
  write_u32(bytes, offset + 76, child);
  write_u32(bytes, offset + 116, starting_sector);
  write_u64(bytes, offset + 120, stream_size);
}

auto initialize_unallocated_entries(std::vector<std::byte> &bytes, std::size_t const size)
    -> void
{
  for (auto index = std::size_t {}; index < size / 128; ++index)
  {
    auto const offset = sector_offset(1, size) + (index * 128);
    write_u32(bytes, offset + 68, kNoStream);
    write_u32(bytes, offset + 72, kNoStream);
    write_u32(bytes, offset + 76, kNoStream);
  }
}

auto make_mini_stream_file(xmole2::cfb::CfbVersion const version)
    -> std::vector<std::byte>
{
  auto const size      = sector_size(version);
  auto bytes           = std::vector<std::byte>(6 * size);
  auto const root_size = static_cast<std::uint64_t>(size) + 64;
  initialize_header(bytes, version, true);
  initialize_unallocated_entries(bytes, size);

  fill_sector(bytes, 0, size, kFreeSector);
  write_fat_entry(bytes, size, 0, kFatSector);
  write_fat_entry(bytes, size, 1, kEndOfChain);
  write_fat_entry(bytes, size, 2, kEndOfChain);
  write_fat_entry(bytes, size, 3, 4);
  write_fat_entry(bytes, size, 4, kEndOfChain);

  write_directory_entry(bytes, size, 0, u"Root Entry", 5, 1, 3, root_size);
  write_directory_entry(bytes, size, 1, u"Tiny", 2, kNoStream, 0, 128);

  fill_sector(bytes, 2, size, kFreeSector);
  write_mini_fat_entry(bytes, size, 0, 1);
  write_mini_fat_entry(bytes, size, 1, kEndOfChain);
  std::fill_n(
      bytes.begin() + static_cast<std::ptrdiff_t>(sector_offset(3, size)), size * 2,
      std::byte { 0x5a });
  return bytes;
}

auto make_empty_file(xmole2::cfb::CfbVersion const version) -> std::vector<std::byte>
{
  auto const size = sector_size(version);
  auto bytes      = std::vector<std::byte>(3 * size);
  initialize_header(bytes, version, false);
  initialize_unallocated_entries(bytes, size);
  fill_sector(bytes, 0, size, kFreeSector);
  write_fat_entry(bytes, size, 0, kFatSector);
  write_fat_entry(bytes, size, 1, kEndOfChain);
  write_directory_entry(bytes, size, 0, u"Root Entry", 5, kNoStream, kEndOfChain, 0);
  return bytes;
}

class CountingSource final : public xmole2::io::ByteSource
{
public:
  CountingSource(std::vector<std::byte> bytes, std::shared_ptr<std::uint64_t> bytes_read)
      : m_bytes { std::move(bytes) }
      , m_bytes_read { std::move(bytes_read) }
  {
  }

  auto size(xmole2::OperationContext const &) const
      -> xmole2::Result<std::uint64_t> override
  {
    return static_cast<std::uint64_t>(m_bytes.size());
  }

  auto read_at(
      std::uint64_t const offset,
      std::span<std::byte> const destination,
      xmole2::OperationContext const &) const -> xmole2::Result<std::size_t> override
  {
    if (offset >= m_bytes.size())
    {
      return std::size_t {};
    }
    auto const count = static_cast<std::size_t>(std::min<std::uint64_t>(
        destination.size(), static_cast<std::uint64_t>(m_bytes.size()) - offset));
    std::memcpy(destination.data(), m_bytes.data() + offset, count);
    *m_bytes_read += count;
    return count;
  }

private:
  std::vector<std::byte> m_bytes;
  std::shared_ptr<std::uint64_t> m_bytes_read;
};

class MiniFatCancellingSource final : public xmole2::io::ByteSource
{
public:
  MiniFatCancellingSource(
      std::vector<std::byte> bytes, xmole2::CancellationSource cancellation)
      : m_bytes { std::move(bytes) }
      , m_cancellation { std::move(cancellation) }
  {
  }

  auto size(xmole2::OperationContext const &) const
      -> xmole2::Result<std::uint64_t> override
  {
    return static_cast<std::uint64_t>(m_bytes.size());
  }

  auto read_at(
      std::uint64_t const offset,
      std::span<std::byte> const destination,
      xmole2::OperationContext const &) const -> xmole2::Result<std::size_t> override
  {
    auto const count = static_cast<std::size_t>(std::min<std::uint64_t>(
        destination.size(), static_cast<std::uint64_t>(m_bytes.size()) - offset));
    std::memcpy(destination.data(), m_bytes.data() + offset, count);
    if (offset == sector_offset(2, 512))
    {
      m_cancellation.request_cancellation();
    }
    return count;
  }

private:
  std::vector<std::byte> m_bytes;
  mutable xmole2::CancellationSource m_cancellation;
};

class MiniFatFaultSource final : public xmole2::io::ByteSource
{
public:
  explicit MiniFatFaultSource(std::vector<std::byte> bytes)
      : m_bytes { std::move(bytes) }
  {
  }

  auto size(xmole2::OperationContext const &) const
      -> xmole2::Result<std::uint64_t> override
  {
    return static_cast<std::uint64_t>(m_bytes.size());
  }

  auto read_at(
      std::uint64_t const offset,
      std::span<std::byte> const destination,
      xmole2::OperationContext const &) const -> xmole2::Result<std::size_t> override
  {
    if (offset == sector_offset(2, 512))
    {
      auto error = xmole2::io::make_error(
          xmole2::io::IoErrorCode::ReadFailed, "injected MiniFAT sector failure");
      error.native_code = 9753;
      return std::unexpected { std::move(error) };
    }
    auto const count = static_cast<std::size_t>(std::min<std::uint64_t>(
        destination.size(), static_cast<std::uint64_t>(m_bytes.size()) - offset));
    std::memcpy(destination.data(), m_bytes.data() + offset, count);
    return count;
  }

private:
  std::vector<std::byte> m_bytes;
};

class MiniFatTruncatedSource final : public xmole2::io::ByteSource
{
public:
  explicit MiniFatTruncatedSource(std::vector<std::byte> bytes)
      : m_bytes { std::move(bytes) }
  {
  }

  auto size(xmole2::OperationContext const &) const
      -> xmole2::Result<std::uint64_t> override
  {
    return static_cast<std::uint64_t>(m_bytes.size());
  }

  auto read_at(
      std::uint64_t const offset,
      std::span<std::byte> const destination,
      xmole2::OperationContext const &) const -> xmole2::Result<std::size_t> override
  {
    if (offset == sector_offset(2, 512))
    {
      return std::size_t {};
    }
    auto const count = static_cast<std::size_t>(std::min<std::uint64_t>(
        destination.size(), static_cast<std::uint64_t>(m_bytes.size()) - offset));
    std::memcpy(destination.data(), m_bytes.data() + offset, count);
    return count;
  }

private:
  std::vector<std::byte> m_bytes;
};

class ProgressRecorder final : public xmole2::ProgressSink
{
public:
  auto report(xmole2::ProgressUpdate const &update) -> void override
  {
    if (update.phase == "cfb.minifat")
    {
      ++mini_fat_updates;
      mini_fat_completed = update.completed;
      mini_fat_total     = update.total;
    }
    if (update.phase == "cfb.root_mini_stream")
    {
      ++root_mapping_updates;
      root_mapping_completed = update.completed;
      root_mapping_total     = update.total;
    }
  }

  std::size_t mini_fat_updates {};
  std::size_t root_mapping_updates {};
  std::uint64_t mini_fat_completed {};
  std::optional<std::uint64_t> mini_fat_total;
  std::uint64_t root_mapping_completed {};
  std::optional<std::uint64_t> root_mapping_total;
};

auto read_from_buffer(
    std::vector<std::byte> bytes, xmole2::OperationContext const &context)
    -> xmole2::Result<xmole2::cfb::MiniStreamAllocationTable>
{
  auto source = xmole2::io::SourceLease::from_buffer(std::move(bytes), context);
  if (!source)
  {
    return std::unexpected { std::move(source.error()) };
  }
  return xmole2::cfb::read_mini_stream_allocation_table(*source, context);
}

auto is_error(
    xmole2::Result<xmole2::cfb::MiniStreamAllocationTable> const &result,
    xmole2::cfb::CfbErrorCode const code) -> bool
{
  return !result && result.error().domain == xmole2::ErrorDomain::Cfb &&
         result.error().code == static_cast<std::uint32_t>(code);
}

auto test_version_3_and_4_tables_and_lazy_mapping() -> bool
{
  auto const context = xmole2::OperationContext {};
  for (auto const version :
       { xmole2::cfb::CfbVersion::Version3, xmole2::cfb::CfbVersion::Version4 })
  {
    auto bytes_read = std::make_shared<std::uint64_t>();
    auto source     = xmole2::io::SourceLease::acquire(
        std::make_shared<CountingSource>(make_mini_stream_file(version), bytes_read),
        context);
    if (!source)
    {
      return false;
    }
    auto table      = xmole2::cfb::read_mini_stream_allocation_table(*source, context);
    auto const size = sector_size(version);
    if (!table || table->mini_fat_sector_ids != std::vector { 2U } ||
        table->mini_fat_entries[0] != 1 || table->mini_fat_entries[1] != kEndOfChain ||
        table->root_mini_stream_sector_ids != std::vector({ 3U, 4U }) ||
        table->root_mini_sector_count != (size / 64) + 1 ||
        *bytes_read != 512 + static_cast<std::uint64_t>(size * 3))
    {
      return false;
    }
  }
  return true;
}

auto test_absent_mini_fat_and_empty_root_stream() -> bool
{
  auto const context = xmole2::OperationContext {};
  auto result =
      read_from_buffer(make_empty_file(xmole2::cfb::CfbVersion::Version3), context);
  return result && result->mini_fat_sector_ids.empty() &&
         result->mini_fat_entries.empty() &&
         result->root_mini_stream_sector_ids.empty() &&
         result->root_mini_sector_count == 0;
}

auto test_mini_fat_chain_length_and_role_validation() -> bool
{
  auto const context = xmole2::OperationContext {};

  auto short_chain = make_mini_stream_file(xmole2::cfb::CfbVersion::Version3);
  write_u32(short_chain, 64, 2);
  if (!is_error(
          read_from_buffer(std::move(short_chain), context),
          xmole2::cfb::CfbErrorCode::InvalidMiniFat))
  {
    return false;
  }

  auto long_chain = make_mini_stream_file(xmole2::cfb::CfbVersion::Version3);
  write_fat_entry(long_chain, 512, 2, 3);
  if (!is_error(
          read_from_buffer(std::move(long_chain), context),
          xmole2::cfb::CfbErrorCode::InvalidMiniFat))
  {
    return false;
  }

  auto overlap = make_mini_stream_file(xmole2::cfb::CfbVersion::Version3);
  write_u32(overlap, 60, 1);
  return is_error(
      read_from_buffer(std::move(overlap), context),
      xmole2::cfb::CfbErrorCode::InvalidMiniFat);
}

auto test_mini_fat_entry_range_padding_and_cycle() -> bool
{
  auto const context = xmole2::OperationContext {};

  auto out_of_range = make_mini_stream_file(xmole2::cfb::CfbVersion::Version3);
  write_mini_fat_entry(out_of_range, 512, 0, 10);
  if (!is_error(
          read_from_buffer(std::move(out_of_range), context),
          xmole2::cfb::CfbErrorCode::SectorOutOfRange))
  {
    return false;
  }

  auto bad_padding = make_mini_stream_file(xmole2::cfb::CfbVersion::Version3);
  write_mini_fat_entry(bad_padding, 512, 9, kEndOfChain);
  if (!is_error(
          read_from_buffer(std::move(bad_padding), context),
          xmole2::cfb::CfbErrorCode::InvalidMiniFat))
  {
    return false;
  }

  auto shared_sector = make_mini_stream_file(xmole2::cfb::CfbVersion::Version3);
  write_mini_fat_entry(shared_sector, 512, 0, 2);
  write_mini_fat_entry(shared_sector, 512, 1, 2);
  write_mini_fat_entry(shared_sector, 512, 2, kEndOfChain);
  if (!is_error(
          read_from_buffer(std::move(shared_sector), context),
          xmole2::cfb::CfbErrorCode::InvalidMiniFat))
  {
    return false;
  }

  auto cycle = make_mini_stream_file(xmole2::cfb::CfbVersion::Version3);
  write_mini_fat_entry(cycle, 512, 1, 0);
  return is_error(
      read_from_buffer(std::move(cycle), context),
      xmole2::cfb::CfbErrorCode::SectorChainCycle);
}

auto test_root_mini_stream_mapping_validation() -> bool
{
  auto const context = xmole2::OperationContext {};

  auto short_chain = make_mini_stream_file(xmole2::cfb::CfbVersion::Version3);
  write_fat_entry(short_chain, 512, 3, kEndOfChain);
  if (!is_error(
          read_from_buffer(std::move(short_chain), context),
          xmole2::cfb::CfbErrorCode::InvalidMiniStream))
  {
    return false;
  }

  auto long_chain = make_mini_stream_file(xmole2::cfb::CfbVersion::Version3);
  write_u64(long_chain, sector_offset(1, 512) + 120, 64);
  if (!is_error(
          read_from_buffer(std::move(long_chain), context),
          xmole2::cfb::CfbErrorCode::InvalidMiniStream))
  {
    return false;
  }

  auto overlap = make_mini_stream_file(xmole2::cfb::CfbVersion::Version3);
  write_u32(overlap, sector_offset(1, 512) + 116, 2);
  write_u64(overlap, sector_offset(1, 512) + 120, 64);
  if (!is_error(
          read_from_buffer(std::move(overlap), context),
          xmole2::cfb::CfbErrorCode::InvalidMiniStream))
  {
    return false;
  }

  auto out_of_range = make_mini_stream_file(xmole2::cfb::CfbVersion::Version3);
  write_u32(out_of_range, sector_offset(1, 512) + 116, 9);
  return is_error(
      read_from_buffer(std::move(out_of_range), context),
      xmole2::cfb::CfbErrorCode::SectorOutOfRange);
}

auto test_budget_progress_cancellation_and_error_chain() -> bool
{
  auto const setup_context = xmole2::OperationContext {};
  auto source              = xmole2::io::SourceLease::from_buffer(
      make_mini_stream_file(xmole2::cfb::CfbVersion::Version3), setup_context);
  if (!source)
  {
    return false;
  }

  auto memory_context                    = xmole2::OperationContext {};
  memory_context.budget.max_memory_bytes = 1800;
  if (!is_error(
          xmole2::cfb::read_mini_stream_allocation_table(*source, memory_context),
          xmole2::cfb::CfbErrorCode::ResourceLimitExceeded))
  {
    return false;
  }

  auto chain_context                               = xmole2::OperationContext {};
  chain_context.budget.max_cfb_stream_chain_length = 1;
  if (!is_error(
          xmole2::cfb::read_mini_stream_allocation_table(*source, chain_context),
          xmole2::cfb::CfbErrorCode::ResourceLimitExceeded))
  {
    return false;
  }

  auto resource_context                             = xmole2::OperationContext {};
  resource_context.budget.max_single_resource_bytes = 512;
  if (!is_error(
          xmole2::cfb::read_mini_stream_allocation_table(*source, resource_context),
          xmole2::cfb::CfbErrorCode::ResourceLimitExceeded))
  {
    return false;
  }

  auto progress             = ProgressRecorder {};
  auto progress_context     = xmole2::OperationContext {};
  progress_context.progress = &progress;
  auto progress_result =
      xmole2::cfb::read_mini_stream_allocation_table(*source, progress_context);
  if (!progress_result || progress.mini_fat_updates != 1 ||
      progress.root_mapping_updates != 2 || progress.mini_fat_completed != 1 ||
      progress.mini_fat_total != 1 || progress.root_mapping_completed != 2 ||
      progress.root_mapping_total != 2)
  {
    return false;
  }

  auto cancellation              = xmole2::CancellationSource {};
  auto cancelled_context         = xmole2::OperationContext {};
  cancelled_context.cancellation = cancellation.token();
  auto cancelling_source         = xmole2::io::SourceLease::acquire(
      std::make_shared<MiniFatCancellingSource>(
          make_mini_stream_file(xmole2::cfb::CfbVersion::Version3), cancellation),
      setup_context);
  if (!cancelling_source)
  {
    return false;
  }
  auto cancelled = xmole2::cfb::read_mini_stream_allocation_table(
      *cancelling_source, cancelled_context);
  if (!is_error(cancelled, xmole2::cfb::CfbErrorCode::Cancelled))
  {
    return false;
  }

  auto fault_source = xmole2::io::SourceLease::acquire(
      std::make_shared<MiniFatFaultSource>(
          make_mini_stream_file(xmole2::cfb::CfbVersion::Version3)),
      setup_context);
  if (!fault_source)
  {
    return false;
  }
  auto failed =
      xmole2::cfb::read_mini_stream_allocation_table(*fault_source, setup_context);
  if (!is_error(failed, xmole2::cfb::CfbErrorCode::ReadFailed) ||
      failed.error().cause == nullptr || failed.error().cause->native_code != 9753 ||
      failed.error().native_code != 9753)
  {
    return false;
  }

  auto truncated_source = xmole2::io::SourceLease::acquire(
      std::make_shared<MiniFatTruncatedSource>(
          make_mini_stream_file(xmole2::cfb::CfbVersion::Version3)),
      setup_context);
  if (!truncated_source)
  {
    return false;
  }
  auto truncated =
      xmole2::cfb::read_mini_stream_allocation_table(*truncated_source, setup_context);
  return is_error(truncated, xmole2::cfb::CfbErrorCode::InvalidMiniFat) &&
         truncated.error().cause != nullptr &&
         truncated.error().cause->domain == xmole2::ErrorDomain::Io &&
         truncated.error().cause->code ==
             static_cast<std::uint32_t>(xmole2::io::IoErrorCode::UnexpectedEndOfFile);
}

auto test_mini_fat_computation_phase_cancellation() -> bool
{
  auto const setup_context = xmole2::OperationContext {};
  auto source              = xmole2::io::SourceLease::from_buffer(
      make_mini_stream_file(xmole2::cfb::CfbVersion::Version3), setup_context);
  if (!source)
  {
    return false;
  }

  auto cancellation    = xmole2::CancellationSource {};
  auto progress        = cfb_test::CancellingProgressSink { "cfb.minifat", cancellation };
  auto context         = xmole2::OperationContext {};
  context.cancellation = cancellation.token();
  context.progress     = &progress;
  auto result          = xmole2::cfb::read_mini_stream_allocation_table(*source, context);
  return is_error(result, xmole2::cfb::CfbErrorCode::Cancelled);
}

} // namespace

auto main() -> int
{
  if (!test_version_3_and_4_tables_and_lazy_mapping())
  {
    return 1;
  }
  if (!test_absent_mini_fat_and_empty_root_stream())
  {
    return 2;
  }
  if (!test_mini_fat_chain_length_and_role_validation())
  {
    return 3;
  }
  if (!test_mini_fat_entry_range_padding_and_cycle())
  {
    return 4;
  }
  if (!test_root_mini_stream_mapping_validation())
  {
    return 5;
  }
  if (!test_budget_progress_cancellation_and_error_chain())
  {
    return 6;
  }
  if (!test_mini_fat_computation_phase_cancellation())
  {
    return 7;
  }
  return 0;
}
