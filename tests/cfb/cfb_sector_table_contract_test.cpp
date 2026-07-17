#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <span>
#include <utility>
#include <vector>

#include "cfb_test_utils.hpp"
#include "xmole2/cfb/error.hpp"
#include "xmole2/cfb/sector_allocation_table.hpp"
#include "xmole2/io/byte_source.hpp"
#include "xmole2/io/error.hpp"
#include "xmole2/io/source_lease.hpp"

namespace
{

constexpr auto kDifatSector = std::uint32_t { 0xff'ff'ff'fcU };
constexpr auto kFatSector   = std::uint32_t { 0xff'ff'ff'fdU };
constexpr auto kEndOfChain  = std::uint32_t { 0xff'ff'ff'feU };
constexpr auto kFreeSector  = std::uint32_t { 0xff'ff'ff'ffU };

using cfb_test::fill_sector;
using cfb_test::sector_offset;
using cfb_test::sector_size;
using cfb_test::write_u16;
using cfb_test::write_u32;

auto write_sector_entry(
    std::vector<std::byte> &bytes,
    std::uint32_t const sector,
    std::size_t const size,
    std::size_t const entry,
    std::uint32_t const value) -> void
{
  write_u32(bytes, sector_offset(sector, size) + (entry * 4), value);
}

auto initialize_header(
    std::vector<std::byte> &bytes,
    xmole2::cfb::CfbVersion const version,
    std::uint32_t const fat_sector_count,
    std::uint32_t const first_directory_sector,
    std::uint32_t const first_difat_sector,
    std::uint32_t const difat_sector_count) -> void
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
  write_u32(bytes, 44, fat_sector_count);
  write_u32(bytes, 48, first_directory_sector);
  write_u32(bytes, 56, 0x10'00);
  write_u32(bytes, 60, kEndOfChain);
  write_u32(bytes, 64, 0);
  write_u32(bytes, 68, first_difat_sector);
  write_u32(bytes, 72, difat_sector_count);
  for (auto index = std::size_t {}; index < 109; ++index)
  {
    write_u32(bytes, 76 + (index * 4), kFreeSector);
  }
}

auto make_basic_file(
    xmole2::cfb::CfbVersion const version, std::uint32_t const extra_sector_count = 0)
    -> std::vector<std::byte>
{
  auto const size                  = sector_size(version);
  auto const physical_sector_count = std::uint32_t { 2 } + extra_sector_count;
  auto bytes                       = std::vector<std::byte>(
      (static_cast<std::size_t>(physical_sector_count) + 1) * size);
  initialize_header(bytes, version, 1, 1, kEndOfChain, 0);
  write_u32(bytes, 76, 0);

  fill_sector(bytes, 0, size, kFreeSector);
  write_sector_entry(bytes, 0, size, 0, kFatSector);
  write_sector_entry(bytes, 0, size, 1, kEndOfChain);
  return bytes;
}

auto make_extended_difat_file(bool const cycle) -> std::vector<std::byte>
{
  constexpr auto kFatSectorCount = std::uint32_t { 110 };
  auto const difat_sector_count  = cycle ? std::uint32_t { 2 } : std::uint32_t { 1 };
  auto const directory_sector    = kFatSectorCount + difat_sector_count;
  auto const physical_count      = directory_sector + 1;
  auto const size                = std::size_t { 512 };
  auto bytes =
      std::vector<std::byte>((static_cast<std::size_t>(physical_count) + 1) * size);
  initialize_header(
      bytes, xmole2::cfb::CfbVersion::Version3, kFatSectorCount, directory_sector,
      kFatSectorCount, difat_sector_count);

  for (auto index = std::uint32_t {}; index < 109; ++index)
  {
    write_u32(bytes, 76 + (static_cast<std::size_t>(index) * 4), index);
  }

  for (auto sector = std::uint32_t {}; sector < kFatSectorCount; ++sector)
  {
    fill_sector(bytes, sector, size, kFreeSector);
  }
  for (auto sector = std::uint32_t {}; sector < kFatSectorCount; ++sector)
  {
    auto const fat_sector = sector / 128;
    auto const fat_entry  = sector % 128;
    write_sector_entry(bytes, fat_sector, size, fat_entry, kFatSector);
  }

  fill_sector(bytes, kFatSectorCount, size, kFreeSector);
  write_sector_entry(bytes, kFatSectorCount, size, 0, 109);
  write_sector_entry(
      bytes, kFatSectorCount, size, 127, cycle ? kFatSectorCount + 1 : kEndOfChain);

  if (cycle)
  {
    fill_sector(bytes, kFatSectorCount + 1, size, kFreeSector);
    write_sector_entry(bytes, kFatSectorCount + 1, size, 127, kFatSectorCount);
  }

  auto const marker_fat_sector = kFatSectorCount / 128;
  auto const marker_fat_entry  = kFatSectorCount % 128;
  write_sector_entry(bytes, marker_fat_sector, size, marker_fat_entry, kDifatSector);
  if (cycle)
  {
    write_sector_entry(
        bytes, marker_fat_sector, size, marker_fat_entry + 1, kDifatSector);
  }
  write_sector_entry(bytes, marker_fat_sector, size, directory_sector % 128, kEndOfChain);
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

class CancellingSource final : public xmole2::io::ByteSource
{
public:
  CancellingSource(std::vector<std::byte> bytes, xmole2::CancellationSource cancellation)
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
    m_cancellation.request_cancellation();
    return count;
  }

private:
  std::vector<std::byte> m_bytes;
  mutable xmole2::CancellationSource m_cancellation;
};

class FatFaultSource final : public xmole2::io::ByteSource
{
public:
  explicit FatFaultSource(std::vector<std::byte> bytes)
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
    if (offset >= 512)
    {
      auto error = xmole2::io::make_error(
          xmole2::io::IoErrorCode::ReadFailed, "injected FAT sector failure");
      error.native_code = 2468;
      return std::unexpected { std::move(error) };
    }
    auto const count = std::min<std::size_t>(destination.size(), m_bytes.size());
    std::memcpy(destination.data(), m_bytes.data(), count);
    return count;
  }

private:
  std::vector<std::byte> m_bytes;
};

class TruncatedFatSource final : public xmole2::io::ByteSource
{
public:
  explicit TruncatedFatSource(std::vector<std::byte> bytes)
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
    if (offset >= 512)
    {
      return std::size_t {};
    }
    auto const count = std::min<std::size_t>(destination.size(), 512);
    std::memcpy(destination.data(), m_bytes.data(), count);
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
    if (update.phase == "cfb.difat")
    {
      ++difat_updates;
    }
    if (update.phase == "cfb.fat")
    {
      ++fat_updates;
    }
  }

  std::size_t difat_updates {};
  std::size_t fat_updates {};
};

auto read_from_buffer(
    std::vector<std::byte> bytes, xmole2::OperationContext const &context)
    -> xmole2::Result<xmole2::cfb::SectorAllocationTable>
{
  auto source = xmole2::io::SourceLease::from_buffer(std::move(bytes), context);
  if (!source)
  {
    return std::unexpected { std::move(source.error()) };
  }
  return xmole2::cfb::read_sector_allocation_table(*source, context);
}

auto is_error(
    xmole2::Result<xmole2::cfb::SectorAllocationTable> const &result,
    xmole2::cfb::CfbErrorCode const code) -> bool
{
  return !result && result.error().domain == xmole2::ErrorDomain::Cfb &&
         result.error().code == static_cast<std::uint32_t>(code);
}

auto test_embedded_difat_and_fat_versions() -> bool
{
  auto progress    = ProgressRecorder {};
  auto context     = xmole2::OperationContext {};
  context.progress = &progress;
  auto version3 =
      read_from_buffer(make_basic_file(xmole2::cfb::CfbVersion::Version3), context);
  auto version4 =
      read_from_buffer(make_basic_file(xmole2::cfb::CfbVersion::Version4), context);

  return version3 && version4 && version3->fat_sector_ids == std::vector { 0U } &&
         version3->difat_sector_ids.empty() && version3->fat_entries.size() == 128 &&
         version3->fat_entries[0] == kFatSector &&
         version3->fat_entries[1] == kEndOfChain &&
         version4->header.version == xmole2::cfb::CfbVersion::Version4 &&
         version4->fat_entries.size() == 1024 && progress.fat_updates == 2 &&
         progress.difat_updates == 0;
}

auto test_extended_difat_is_bounded_and_ordered() -> bool
{
  auto const context = xmole2::OperationContext {};
  auto bytes_read    = std::make_shared<std::uint64_t>();
  auto source        = xmole2::io::SourceLease::acquire(
      std::make_shared<CountingSource>(make_extended_difat_file(false), bytes_read),
      context);
  if (!source)
  {
    return false;
  }
  auto table = xmole2::cfb::read_sector_allocation_table(*source, context);
  return table && table->fat_sector_ids.size() == 110 &&
         table->fat_sector_ids.front() == 0 && table->fat_sector_ids.back() == 109 &&
         table->difat_sector_ids == std::vector { 110U } &&
         table->fat_entries[110] == kDifatSector && *bytes_read == 512 + (111 * 512);
}

auto test_difat_cycle_and_out_of_range() -> bool
{
  auto const context = xmole2::OperationContext {};
  auto cycle         = read_from_buffer(make_extended_difat_file(true), context);
  if (!is_error(cycle, xmole2::cfb::CfbErrorCode::SectorChainCycle))
  {
    return false;
  }

  auto out_of_range = make_basic_file(xmole2::cfb::CfbVersion::Version3);
  write_u32(out_of_range, 76, 9);
  return is_error(
      read_from_buffer(std::move(out_of_range), context),
      xmole2::cfb::CfbErrorCode::SectorOutOfRange);
}

auto test_fat_cycle_out_of_range_and_role_mismatch() -> bool
{
  auto const context = xmole2::OperationContext {};

  auto out_of_range = make_basic_file(xmole2::cfb::CfbVersion::Version3);
  write_sector_entry(out_of_range, 0, 512, 1, 9);
  if (!is_error(
          read_from_buffer(std::move(out_of_range), context),
          xmole2::cfb::CfbErrorCode::SectorOutOfRange))
  {
    return false;
  }

  auto cycle = make_basic_file(xmole2::cfb::CfbVersion::Version3, 1);
  write_sector_entry(cycle, 0, 512, 1, 2);
  write_sector_entry(cycle, 0, 512, 2, 1);
  if (!is_error(
          read_from_buffer(std::move(cycle), context),
          xmole2::cfb::CfbErrorCode::SectorChainCycle))
  {
    return false;
  }

  auto role_mismatch = make_basic_file(xmole2::cfb::CfbVersion::Version3);
  write_sector_entry(role_mismatch, 0, 512, 0, kEndOfChain);
  if (!is_error(
          read_from_buffer(std::move(role_mismatch), context),
          xmole2::cfb::CfbErrorCode::InvalidSectorTable))
  {
    return false;
  }

  auto duplicate = make_basic_file(xmole2::cfb::CfbVersion::Version3, 1);
  write_u32(duplicate, 44, 2);
  write_u32(duplicate, 80, 0);
  return is_error(
      read_from_buffer(std::move(duplicate), context),
      xmole2::cfb::CfbErrorCode::InvalidSectorTable);
}

auto test_budget_and_running_cancellation() -> bool
{
  auto const setup_context = xmole2::OperationContext {};
  auto source              = xmole2::io::SourceLease::from_buffer(
      make_basic_file(xmole2::cfb::CfbVersion::Version3), setup_context);
  if (!source)
  {
    return false;
  }

  auto memory_context                    = xmole2::OperationContext {};
  memory_context.budget.max_memory_bytes = 100;
  auto memory_limited =
      xmole2::cfb::read_sector_allocation_table(*source, memory_context);
  if (!is_error(memory_limited, xmole2::cfb::CfbErrorCode::ResourceLimitExceeded))
  {
    return false;
  }

  auto sector_context                        = xmole2::OperationContext {};
  sector_context.budget.max_cfb_sector_count = 1;
  auto sector_limited =
      xmole2::cfb::read_sector_allocation_table(*source, sector_context);
  if (!is_error(sector_limited, xmole2::cfb::CfbErrorCode::ResourceLimitExceeded))
  {
    return false;
  }

  auto chain_bytes = make_basic_file(xmole2::cfb::CfbVersion::Version3, 1);
  write_sector_entry(chain_bytes, 0, 512, 1, 2);
  write_sector_entry(chain_bytes, 0, 512, 2, kEndOfChain);
  auto chain_source =
      xmole2::io::SourceLease::from_buffer(std::move(chain_bytes), setup_context);
  if (!chain_source)
  {
    return false;
  }
  auto chain_context                               = xmole2::OperationContext {};
  chain_context.budget.max_cfb_stream_chain_length = 1;
  auto chain_limited =
      xmole2::cfb::read_sector_allocation_table(*chain_source, chain_context);
  if (!is_error(chain_limited, xmole2::cfb::CfbErrorCode::ResourceLimitExceeded))
  {
    return false;
  }

  auto cancellation              = xmole2::CancellationSource {};
  auto cancelled_context         = xmole2::OperationContext {};
  cancelled_context.cancellation = cancellation.token();
  auto cancelling_source         = xmole2::io::SourceLease::acquire(
      std::make_shared<CancellingSource>(
          make_basic_file(xmole2::cfb::CfbVersion::Version3), cancellation),
      setup_context);
  if (!cancelling_source)
  {
    return false;
  }
  auto cancelled =
      xmole2::cfb::read_sector_allocation_table(*cancelling_source, cancelled_context);
  return is_error(cancelled, xmole2::cfb::CfbErrorCode::Cancelled);
}

auto test_fat_computation_phase_cancellation() -> bool
{
  auto const setup_context = xmole2::OperationContext {};
  auto source              = xmole2::io::SourceLease::from_buffer(
      make_basic_file(xmole2::cfb::CfbVersion::Version3), setup_context);
  if (!source)
  {
    return false;
  }

  auto cancellation    = xmole2::CancellationSource {};
  auto progress        = cfb_test::CancellingProgressSink { "cfb.fat", cancellation };
  auto context         = xmole2::OperationContext {};
  context.cancellation = cancellation.token();
  context.progress     = &progress;
  auto result          = xmole2::cfb::read_sector_allocation_table(*source, context);
  return is_error(result, xmole2::cfb::CfbErrorCode::Cancelled);
}

auto test_sector_io_error_chain_is_preserved() -> bool
{
  auto const context = xmole2::OperationContext {};
  auto source        = xmole2::io::SourceLease::acquire(
      std::make_shared<FatFaultSource>(
          make_basic_file(xmole2::cfb::CfbVersion::Version3)),
      context);
  if (!source)
  {
    return false;
  }
  auto table = xmole2::cfb::read_sector_allocation_table(*source, context);
  if (!is_error(table, xmole2::cfb::CfbErrorCode::ReadFailed) ||
      table.error().cause == nullptr || table.error().cause->native_code != 2468 ||
      table.error().native_code != 2468)
  {
    return false;
  }

  auto truncated_source = xmole2::io::SourceLease::acquire(
      std::make_shared<TruncatedFatSource>(
          make_basic_file(xmole2::cfb::CfbVersion::Version3)),
      context);
  if (!truncated_source)
  {
    return false;
  }
  auto truncated = xmole2::cfb::read_sector_allocation_table(*truncated_source, context);
  return is_error(truncated, xmole2::cfb::CfbErrorCode::InvalidSectorTable) &&
         truncated.error().cause != nullptr &&
         truncated.error().cause->domain == xmole2::ErrorDomain::Io &&
         truncated.error().cause->code ==
             static_cast<std::uint32_t>(xmole2::io::IoErrorCode::UnexpectedEndOfFile);
}

} // namespace

auto main() -> int
{
  if (!test_embedded_difat_and_fat_versions())
  {
    return 1;
  }
  if (!test_extended_difat_is_bounded_and_ordered())
  {
    return 2;
  }
  if (!test_difat_cycle_and_out_of_range())
  {
    return 3;
  }
  if (!test_fat_cycle_out_of_range_and_role_mismatch())
  {
    return 4;
  }
  if (!test_budget_and_running_cancellation())
  {
    return 5;
  }
  if (!test_fat_computation_phase_cancellation())
  {
    return 6;
  }
  if (!test_sector_io_error_chain_is_preserved())
  {
    return 7;
  }
  return 0;
}
