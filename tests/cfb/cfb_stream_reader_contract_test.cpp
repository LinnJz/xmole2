#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <optional>
#include <span>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

#include "cfb_test_utils.hpp"
#include "xmole2/cfb/cfb_stream_reader.hpp"
#include "xmole2/cfb/compound_file_header.hpp"
#include "xmole2/cfb/error.hpp"
#include "xmole2/io/byte_source.hpp"
#include "xmole2/io/error.hpp"
#include "xmole2/io/source_lease.hpp"

namespace
{

constexpr auto kFatSector  = std::uint32_t { 0xff'ff'ff'fdU };
constexpr auto kEndOfChain = std::uint32_t { 0xff'ff'ff'feU };
constexpr auto kFreeSector = std::uint32_t { 0xff'ff'ff'ffU };
constexpr auto kNoStream   = std::uint32_t { 0xff'ff'ff'ffU };

struct Fixture
{
  std::vector<std::byte> bytes;
  std::vector<std::byte> mini_payload;
  std::vector<std::byte> regular_payload;
  std::vector<std::uint32_t> payload_sector_ids;
  std::vector<std::uint32_t> regular_sector_ids;
  std::size_t sector_size {};
};

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
    std::vector<std::byte> &bytes, xmole2::cfb::CfbVersion const version) -> void
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
  write_u32(bytes, 60, 2);
  write_u32(bytes, 64, 1);
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
    std::uint32_t const left,
    std::uint32_t const right,
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
  write_u32(bytes, offset + 68, left);
  write_u32(bytes, offset + 72, right);
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

auto patterned_bytes(std::size_t const count, std::uint32_t const salt)
    -> std::vector<std::byte>
{
  auto bytes = std::vector<std::byte>(count);
  for (auto index = std::size_t {}; index < count; ++index)
  {
    bytes[index] = static_cast<std::byte>((index * 37 + salt) & 0xffU);
  }
  return bytes;
}

auto make_fixture(
    xmole2::cfb::CfbVersion const version, bool const shared_regular = false) -> Fixture
{
  auto result            = Fixture {};
  result.sector_size     = sector_size(version);
  result.mini_payload    = patterned_bytes(100, 11);
  result.regular_payload = patterned_bytes(4096 + 37, 29);
  auto const regular_sectors =
      (result.regular_payload.size() + result.sector_size - 1) / result.sector_size;
  result.regular_sector_ids.reserve(regular_sectors);
  for (auto index = std::size_t {}; index < regular_sectors; ++index)
  {
    result.regular_sector_ids.push_back(static_cast<std::uint32_t>(4 + index));
  }
  if (result.regular_sector_ids.size() > 2)
  {
    std::swap(result.regular_sector_ids[1], result.regular_sector_ids[2]);
  }

  auto const physical_sector_count = static_cast<std::uint32_t>(4 + regular_sectors);
  result.bytes                     = std::vector<std::byte>(
      (static_cast<std::size_t>(physical_sector_count) + 1) * result.sector_size);
  initialize_header(result.bytes, version);
  initialize_unallocated_entries(result.bytes, result.sector_size);

  fill_sector(result.bytes, 0, result.sector_size, kFreeSector);
  write_fat_entry(result.bytes, result.sector_size, 0, kFatSector);
  write_fat_entry(result.bytes, result.sector_size, 1, kEndOfChain);
  write_fat_entry(result.bytes, result.sector_size, 2, kEndOfChain);
  write_fat_entry(result.bytes, result.sector_size, 3, kEndOfChain);
  for (auto index = std::size_t {}; index < result.regular_sector_ids.size(); ++index)
  {
    auto const next =
        index + 1 == result.regular_sector_ids.size()
            ? kEndOfChain
            : result.regular_sector_ids[index + 1];
    write_fat_entry(
        result.bytes, result.sector_size, result.regular_sector_ids[index], next);
  }

  write_directory_entry(
      result.bytes, result.sector_size, 0, u"Root Entry", 5, kNoStream, kNoStream, 1, 3,
      128);
  write_directory_entry(
      result.bytes, result.sector_size, 1, u"Tiny", 2, kNoStream, 2, kNoStream, 0,
      result.mini_payload.size());
  write_directory_entry(
      result.bytes, result.sector_size, 2, u"Regular", 2, shared_regular ? 3U : kNoStream,
      kNoStream, kNoStream, result.regular_sector_ids.front(),
      result.regular_payload.size());
  if (shared_regular)
  {
    write_directory_entry(
        result.bytes, result.sector_size, 3, u"Shared", 2, kNoStream, kNoStream,
        kNoStream, result.regular_sector_ids.front(), result.regular_payload.size());
  }

  fill_sector(result.bytes, 2, result.sector_size, kFreeSector);
  write_mini_fat_entry(result.bytes, result.sector_size, 0, 1);
  write_mini_fat_entry(result.bytes, result.sector_size, 1, kEndOfChain);

  auto const root_offset = sector_offset(3, result.sector_size);
  std::fill_n(
      result.bytes.begin() + static_cast<std::ptrdiff_t>(root_offset), result.sector_size,
      std::byte { 0xee });
  std::copy(
      result.mini_payload.begin(), result.mini_payload.end(),
      result.bytes.begin() + static_cast<std::ptrdiff_t>(root_offset));

  for (auto index = std::size_t {}; index < result.regular_sector_ids.size(); ++index)
  {
    auto const offset =
        sector_offset(result.regular_sector_ids[index], result.sector_size);
    std::fill_n(
        result.bytes.begin() + static_cast<std::ptrdiff_t>(offset), result.sector_size,
        std::byte { 0xee });
    auto const payload_offset = index * result.sector_size;
    auto const count =
        std::min(result.sector_size, result.regular_payload.size() - payload_offset);
    std::copy_n(
        result.regular_payload.begin() + static_cast<std::ptrdiff_t>(payload_offset),
        count, result.bytes.begin() + static_cast<std::ptrdiff_t>(offset));
  }
  result.payload_sector_ids.push_back(3);
  result.payload_sector_ids.insert(
      result.payload_sector_ids.end(), result.regular_sector_ids.begin(),
      result.regular_sector_ids.end());
  return result;
}

auto make_cross_fat_sector_boundary_fixture() -> Fixture
{
  constexpr auto kPhysicalSectorCount = std::uint32_t { 132 };
  constexpr auto kSecondFatSector     = std::uint32_t { 3 };
  auto result                         = Fixture {};
  result.sector_size                  = 512;
  result.regular_payload              = patterned_bytes(4096, 71);
  for (auto sector = std::uint32_t { 124 }; sector <= 131; ++sector)
  {
    result.regular_sector_ids.push_back(sector);
  }
  result.payload_sector_ids = result.regular_sector_ids;
  result.bytes              = std::vector<std::byte>(
      (static_cast<std::size_t>(kPhysicalSectorCount) + 1) * result.sector_size,
      std::byte {});

  initialize_header(result.bytes, xmole2::cfb::CfbVersion::Version3);
  write_u32(result.bytes, 44, 2);
  write_u32(result.bytes, 60, kEndOfChain);
  write_u32(result.bytes, 64, 0);
  write_u32(result.bytes, 80, kSecondFatSector);
  initialize_unallocated_entries(result.bytes, result.sector_size);
  fill_sector(result.bytes, 0, result.sector_size, kFreeSector);
  fill_sector(result.bytes, kSecondFatSector, result.sector_size, kFreeSector);

  auto write_boundary_fat_entry =
      [&](std::uint32_t const sector, std::uint32_t const value) -> void
  {
    auto const fat_sector = sector < 128 ? 0U : kSecondFatSector;
    auto const slot       = sector % 128;
    write_u32(
        result.bytes,
        sector_offset(fat_sector, result.sector_size) +
            (static_cast<std::size_t>(slot) * 4),
        value);
  };
  write_boundary_fat_entry(0, kFatSector);
  write_boundary_fat_entry(kSecondFatSector, kFatSector);
  write_boundary_fat_entry(1, kEndOfChain);
  for (auto index = std::size_t {}; index < result.regular_sector_ids.size(); ++index)
  {
    auto const next =
        index + 1 == result.regular_sector_ids.size()
            ? kEndOfChain
            : result.regular_sector_ids[index + 1];
    write_boundary_fat_entry(result.regular_sector_ids[index], next);
  }

  write_directory_entry(
      result.bytes, result.sector_size, 0, u"Root Entry", 5, kNoStream, kNoStream, 1,
      kEndOfChain, 0);
  write_directory_entry(
      result.bytes, result.sector_size, 1, u"Boundary", 2, kNoStream, kNoStream,
      kNoStream, result.regular_sector_ids.front(), result.regular_payload.size());
  for (auto index = std::size_t {}; index < result.regular_sector_ids.size(); ++index)
  {
    auto const offset =
        sector_offset(result.regular_sector_ids[index], result.sector_size);
    std::copy_n(
        result.regular_payload.begin() +
            static_cast<std::ptrdiff_t>(index * result.sector_size),
        result.sector_size, result.bytes.begin() + static_cast<std::ptrdiff_t>(offset));
  }
  return result;
}

class TrackingSource final : public xmole2::io::ByteSource
{
public:
  TrackingSource(Fixture fixture, std::shared_ptr<std::uint64_t> payload_bytes_read)
      : m_bytes { std::move(fixture.bytes) }
      , m_payload_sector_ids { std::move(fixture.payload_sector_ids) }
      , m_sector_size { fixture.sector_size }
      , m_payload_bytes_read { std::move(payload_bytes_read) }
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
    auto const read_end = offset + count;
    for (auto const sector : m_payload_sector_ids)
    {
      auto const begin = static_cast<std::uint64_t>(sector_offset(sector, m_sector_size));
      auto const end   = begin + m_sector_size;
      if (offset < end && read_end > begin)
      {
        auto const overlap_begin = std::max(offset, begin);
        auto const overlap_end   = std::min(read_end, end);
        *m_payload_bytes_read += overlap_end - overlap_begin;
      }
    }
    return count;
  }

private:
  std::vector<std::byte> m_bytes;
  std::vector<std::uint32_t> m_payload_sector_ids;
  std::size_t m_sector_size {};
  std::shared_ptr<std::uint64_t> m_payload_bytes_read;
};

class PayloadFaultSource final : public xmole2::io::ByteSource
{
public:
  explicit PayloadFaultSource(Fixture fixture)
      : m_bytes { std::move(fixture.bytes) }
      , m_failure_begin { sector_offset(
            fixture.regular_sector_ids.front(), fixture.sector_size) }
      , m_failure_end { m_failure_begin + fixture.sector_size }
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
    if (offset < m_failure_end && offset + destination.size() > m_failure_begin)
    {
      auto error = xmole2::io::make_error(
          xmole2::io::IoErrorCode::ReadFailed, "injected CFB payload failure");
      error.native_code = 2468;
      return std::unexpected { std::move(error) };
    }
    if (offset >= m_bytes.size())
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
  std::uint64_t m_failure_begin {};
  std::uint64_t m_failure_end {};
};

class TruncatedPayloadSource final : public xmole2::io::ByteSource
{
public:
  explicit TruncatedPayloadSource(Fixture fixture)
      : m_bytes { std::move(fixture.bytes) }
      , m_truncated_begin { sector_offset(
            fixture.regular_sector_ids.front(), fixture.sector_size) }
      , m_truncated_end { m_truncated_begin + fixture.sector_size }
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
    if (offset < m_truncated_end && offset + destination.size() > m_truncated_begin)
    {
      return std::size_t {};
    }
    if (offset >= m_bytes.size())
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
  std::uint64_t m_truncated_begin {};
  std::uint64_t m_truncated_end {};
};

class ProgressRecorder final : public xmole2::ProgressSink
{
public:
  auto report(xmole2::ProgressUpdate const &update) -> void override
  {
    if (update.phase == "cfb.stream.read")
    {
      ++read_updates;
    }
    if (update.phase == "cfb.stream.validate")
    {
      ++validate_updates;
      validate_completed = update.completed;
      validate_total     = update.total;
    }
  }

  std::size_t read_updates {};
  std::size_t validate_updates {};
  std::uint64_t validate_completed {};
  std::optional<std::uint64_t> validate_total;
};

auto open_from_buffer(
    std::vector<std::byte> bytes,
    std::uint32_t const entry,
    xmole2::OperationContext const &context)
    -> xmole2::Result<xmole2::cfb::CfbStreamReader>
{
  auto source = xmole2::io::SourceLease::from_buffer(std::move(bytes), context);
  if (!source)
  {
    return std::unexpected { std::move(source.error()) };
  }
  return xmole2::cfb::CfbStreamReader::open(*source, entry, context);
}

auto read_all(
    xmole2::cfb::CfbStreamReader &reader,
    std::size_t const chunk_size,
    xmole2::OperationContext const &context) -> xmole2::Result<std::vector<std::byte>>
{
  auto result = std::vector<std::byte> {};
  auto buffer = std::vector<std::byte>(chunk_size);
  while (true)
  {
    auto count = reader.read(buffer, context);
    if (!count)
    {
      return std::unexpected { std::move(count.error()) };
    }
    if (*count == 0)
    {
      return result;
    }
    result.insert(result.end(), buffer.begin(), buffer.begin() + *count);
  }
}

auto has_error(
    xmole2::Result<xmole2::cfb::CfbStreamReader> const &result,
    xmole2::cfb::CfbErrorCode const code) -> bool
{
  return !result && result.error().domain == xmole2::ErrorDomain::Cfb &&
         result.error().code == static_cast<std::uint32_t>(code);
}

auto test_regular_and_mini_streaming_is_lazy_and_keeps_source_alive() -> bool
{
  static_assert(!std::is_copy_constructible_v<xmole2::cfb::CfbStreamReader>);
  static_assert(std::is_move_constructible_v<xmole2::cfb::CfbStreamReader>);

  for (auto const version :
       { xmole2::cfb::CfbVersion::Version3, xmole2::cfb::CfbVersion::Version4 })
  {
    auto fixture            = make_fixture(version);
    auto const mini_bytes   = fixture.mini_payload;
    auto const regular      = fixture.regular_payload;
    auto payload_bytes_read = std::make_shared<std::uint64_t>();
    auto mini_reader        = xmole2::cfb::CfbStreamReader {};
    auto regular_reader     = xmole2::cfb::CfbStreamReader {};
    auto const context      = xmole2::OperationContext {};
    {
      auto source = xmole2::io::SourceLease::acquire(
          std::make_shared<TrackingSource>(std::move(fixture), payload_bytes_read),
          context);
      if (!source)
      {
        return false;
      }
      auto mini = xmole2::cfb::CfbStreamReader::open(*source, 1, context);
      auto full = xmole2::cfb::CfbStreamReader::open(*source, 2, context);
      if (!mini || !full || *payload_bytes_read != 0)
      {
        return false;
      }
      mini_reader    = std::move(*mini);
      regular_reader = std::move(*full);
    }

    auto mini_output    = read_all(mini_reader, 17, context);
    auto regular_output = read_all(regular_reader, 333, context);
    auto eof_buffer     = std::array<std::byte, 8> {};
    auto eof            = regular_reader.read(eof_buffer, context);
    if (!mini_output || *mini_output != mini_bytes || !regular_output ||
        *regular_output != regular || !eof || *eof != 0 ||
        mini_reader.storage() != xmole2::cfb::CfbStreamStorage::Mini ||
        regular_reader.storage() != xmole2::cfb::CfbStreamStorage::Regular ||
        mini_reader.position() != mini_reader.size() || !mini_reader.finished() ||
        regular_reader.position() != regular_reader.size() || !regular_reader.finished())
    {
      return false;
    }
  }
  return true;
}

auto test_empty_and_invalid_directory_entries() -> bool
{
  auto fixture                = make_fixture(xmole2::cfb::CfbVersion::Version3);
  auto const directory_offset = sector_offset(1, fixture.sector_size);
  write_u32(fixture.bytes, directory_offset + (2 * 128) + 68, 3);
  write_directory_entry(
      fixture.bytes, fixture.sector_size, 3, u"Empty", 2, kNoStream, kNoStream, kNoStream,
      kEndOfChain, 0);
  auto const context = xmole2::OperationContext {};
  auto empty         = open_from_buffer(fixture.bytes, 3, context);
  if (!empty || empty->size() != 0 || !empty->finished() ||
      empty->storage() != xmole2::cfb::CfbStreamStorage::Mini)
  {
    return false;
  }
  auto value = std::byte {};
  auto eof   = empty->read(std::span<std::byte> { &value, 1 }, context);
  auto root  = open_from_buffer(fixture.bytes, 0, context);
  auto past  = open_from_buffer(std::move(fixture.bytes), 99, context);
  return eof && *eof == 0 && has_error(root, xmole2::cfb::CfbErrorCode::InvalidStream) &&
         has_error(past, xmole2::cfb::CfbErrorCode::InvalidStream);
}

auto test_chain_length_and_sharing_validation() -> bool
{
  auto const context = xmole2::OperationContext {};

  auto regular_short = make_fixture(xmole2::cfb::CfbVersion::Version3);
  write_fat_entry(
      regular_short.bytes, regular_short.sector_size,
      regular_short.regular_sector_ids.front(), kEndOfChain);
  if (!has_error(
          open_from_buffer(std::move(regular_short.bytes), 2, context),
          xmole2::cfb::CfbErrorCode::InvalidStream))
  {
    return false;
  }

  auto mini_short = make_fixture(xmole2::cfb::CfbVersion::Version3);
  write_mini_fat_entry(mini_short.bytes, mini_short.sector_size, 0, kEndOfChain);
  if (!has_error(
          open_from_buffer(std::move(mini_short.bytes), 1, context),
          xmole2::cfb::CfbErrorCode::InvalidStream))
  {
    return false;
  }

  auto shared = make_fixture(xmole2::cfb::CfbVersion::Version3, true);
  return has_error(
      open_from_buffer(std::move(shared.bytes), 2, context),
      xmole2::cfb::CfbErrorCode::InvalidStream);
}

auto test_validation_progress_counts_only_nonempty_streams() -> bool
{
  auto fixture     = make_fixture(xmole2::cfb::CfbVersion::Version3);
  auto progress    = ProgressRecorder {};
  auto context     = xmole2::OperationContext {};
  context.progress = &progress;
  auto reader      = open_from_buffer(std::move(fixture.bytes), 1, context);
  return reader && progress.validate_updates == 2 && progress.validate_completed == 2 &&
         progress.validate_total == 2;
}

auto test_cross_fat_sector_boundary_and_exact_size_read() -> bool
{
  auto fixture        = make_cross_fat_sector_boundary_fixture();
  auto const expected = fixture.regular_payload;
  auto const context  = xmole2::OperationContext {};
  auto reader         = open_from_buffer(std::move(fixture.bytes), 1, context);
  if (!reader)
  {
    return false;
  }
  auto output = std::vector<std::byte>(expected.size());
  auto count  = reader->read(output, context);
  auto extra  = std::byte {};
  auto eof    = reader->read(std::span<std::byte> { &extra, 1 }, context);
  return count && *count == output.size() && output == expected && reader->finished() &&
         eof && *eof == 0;
}

auto test_budget_cancellation_progress_and_empty_destination() -> bool
{
  auto fixture             = make_fixture(xmole2::cfb::CfbVersion::Version3);
  auto const setup_context = xmole2::OperationContext {};
  auto source = xmole2::io::SourceLease::from_buffer(fixture.bytes, setup_context);
  if (!source)
  {
    return false;
  }

  auto resource_context                             = xmole2::OperationContext {};
  resource_context.budget.max_single_resource_bytes = 4096;
  if (!has_error(
          xmole2::cfb::CfbStreamReader::open(*source, 2, resource_context),
          xmole2::cfb::CfbErrorCode::ResourceLimitExceeded))
  {
    return false;
  }

  auto memory_context                    = xmole2::OperationContext {};
  memory_context.budget.max_memory_bytes = 1;
  if (!has_error(
          xmole2::cfb::CfbStreamReader::open(*source, 2, memory_context),
          xmole2::cfb::CfbErrorCode::ResourceLimitExceeded))
  {
    return false;
  }

  auto chain_context                               = xmole2::OperationContext {};
  chain_context.budget.max_cfb_stream_chain_length = 1;
  if (!has_error(
          xmole2::cfb::CfbStreamReader::open(*source, 2, chain_context),
          xmole2::cfb::CfbErrorCode::ResourceLimitExceeded))
  {
    return false;
  }

  auto reader = xmole2::cfb::CfbStreamReader::open(*source, 2, setup_context);
  if (!reader)
  {
    return false;
  }
  auto read_budget_context                             = xmole2::OperationContext {};
  read_budget_context.budget.max_single_resource_bytes = 4096;
  auto budget_value                                    = std::byte {};
  auto limited_read =
      reader->read(std::span<std::byte> { &budget_value, 1 }, read_budget_context);
  if (limited_read ||
      limited_read.error().code !=
          static_cast<std::uint32_t>(xmole2::cfb::CfbErrorCode::ResourceLimitExceeded))
  {
    return false;
  }
  auto empty = reader->read(std::span<std::byte> {}, setup_context);
  if (empty || empty.error().code !=
                   static_cast<std::uint32_t>(xmole2::cfb::CfbErrorCode::InvalidArgument))
  {
    return false;
  }

  auto cancellation              = xmole2::CancellationSource {};
  auto cancelled_context         = xmole2::OperationContext {};
  cancelled_context.cancellation = cancellation.token();
  cancellation.request_cancellation();
  auto value     = std::byte {};
  auto cancelled = reader->read(std::span<std::byte> { &value, 1 }, cancelled_context);
  if (cancelled ||
      cancelled.error().code !=
          static_cast<std::uint32_t>(xmole2::cfb::CfbErrorCode::Cancelled))
  {
    return false;
  }

  auto progress             = ProgressRecorder {};
  auto progress_context     = xmole2::OperationContext {};
  progress_context.progress = &progress;
  auto output               = read_all(*reader, 257, progress_context);
  return output && *output == fixture.regular_payload && progress.read_updates > 1;
}

auto test_payload_error_chain_is_preserved() -> bool
{
  auto const context = xmole2::OperationContext {};
  auto source        = xmole2::io::SourceLease::acquire(
      std::make_shared<PayloadFaultSource>(
          make_fixture(xmole2::cfb::CfbVersion::Version3)),
      context);
  if (!source)
  {
    return false;
  }
  auto reader = xmole2::cfb::CfbStreamReader::open(*source, 2, context);
  if (!reader)
  {
    return false;
  }
  source = xmole2::io::SourceLease {};

  auto buffer = std::array<std::byte, 64> {};
  auto result = reader->read(buffer, context);
  if (result || result.error().domain != xmole2::ErrorDomain::Cfb ||
      result.error().code !=
          static_cast<std::uint32_t>(xmole2::cfb::CfbErrorCode::ReadFailed) ||
      result.error().cause == nullptr || result.error().cause->native_code != 2468 ||
      result.error().native_code != 2468)
  {
    return false;
  }

  auto truncated_source = xmole2::io::SourceLease::acquire(
      std::make_shared<TruncatedPayloadSource>(
          make_fixture(xmole2::cfb::CfbVersion::Version3)),
      context);
  if (!truncated_source)
  {
    return false;
  }
  auto truncated_reader =
      xmole2::cfb::CfbStreamReader::open(*truncated_source, 2, context);
  if (!truncated_reader)
  {
    return false;
  }
  auto truncated = truncated_reader->read(buffer, context);
  return !truncated && truncated.error().domain == xmole2::ErrorDomain::Cfb &&
         truncated.error().code ==
             static_cast<std::uint32_t>(xmole2::cfb::CfbErrorCode::InvalidStream) &&
         truncated.error().cause != nullptr &&
         truncated.error().cause->domain == xmole2::ErrorDomain::Io &&
         truncated.error().cause->code ==
             static_cast<std::uint32_t>(xmole2::io::IoErrorCode::UnexpectedEndOfFile);
}

} // namespace

auto main() -> int
{
  if (!test_regular_and_mini_streaming_is_lazy_and_keeps_source_alive())
  {
    return 1;
  }
  if (!test_empty_and_invalid_directory_entries())
  {
    return 2;
  }
  if (!test_chain_length_and_sharing_validation())
  {
    return 3;
  }
  if (!test_validation_progress_counts_only_nonempty_streams())
  {
    return 4;
  }
  if (!test_cross_fat_sector_boundary_and_exact_size_read())
  {
    return 5;
  }
  if (!test_budget_cancellation_progress_and_empty_destination())
  {
    return 6;
  }
  if (!test_payload_error_chain_is_preserved())
  {
    return 7;
  }
  return 0;
}
