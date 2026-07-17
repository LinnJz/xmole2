#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <span>
#include <string_view>
#include <utility>
#include <vector>

#include "cfb_test_utils.hpp"
#include "xmole2/cfb/directory_index.hpp"
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

using cfb_test::fill_sector;
using cfb_test::sector_offset;
using cfb_test::sector_size;
using cfb_test::write_u16;
using cfb_test::write_u32;
using cfb_test::write_u64;

auto directory_entry_offset(
    xmole2::cfb::CfbVersion const version, std::uint32_t const entry) -> std::size_t
{
  auto const size               = sector_size(version);
  auto const entries_per_sector = static_cast<std::uint32_t>(size / 128);
  auto const directory_sector   = std::uint32_t { 1 } + (entry / entries_per_sector);
  auto const slot               = entry % entries_per_sector;
  return sector_offset(directory_sector, size) + (static_cast<std::size_t>(slot) * 128);
}

auto write_fat_entry(
    std::vector<std::byte> &bytes,
    std::size_t const size,
    std::uint32_t const sector,
    std::uint32_t const value) -> void
{
  write_u32(
      bytes, sector_offset(0, size) + (static_cast<std::size_t>(sector) * 4), value);
}

auto initialize_header(
    std::vector<std::byte> &bytes,
    xmole2::cfb::CfbVersion const version,
    std::uint32_t const directory_sector_count) -> void
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
  write_u32(
      bytes, 40,
      version == xmole2::cfb::CfbVersion::Version3 ? 0U : directory_sector_count);
  write_u32(bytes, 44, 1);
  write_u32(bytes, 48, 1);
  write_u32(bytes, 56, 0x10'00);
  write_u32(bytes, 60, kEndOfChain);
  write_u32(bytes, 64, 0);
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
    xmole2::cfb::CfbVersion const version,
    std::uint32_t const index,
    std::u16string_view const name,
    std::uint8_t const type,
    std::uint8_t const color,
    std::uint32_t const left,
    std::uint32_t const right,
    std::uint32_t const child,
    std::uint32_t const starting_sector,
    std::uint64_t const stream_size) -> void
{
  auto const offset = directory_entry_offset(version, index);
  for (auto name_index = std::size_t {}; name_index < name.size(); ++name_index)
  {
    write_u16(
        bytes, offset + (name_index * 2), static_cast<std::uint16_t>(name[name_index]));
  }
  write_u16(bytes, offset + 64, static_cast<std::uint16_t>((name.size() + 1) * 2));
  bytes[offset + 66] = static_cast<std::byte>(type);
  bytes[offset + 67] = static_cast<std::byte>(color);
  write_u32(bytes, offset + 68, left);
  write_u32(bytes, offset + 72, right);
  write_u32(bytes, offset + 76, child);
  if (type == 1 || type == 5)
  {
    bytes[offset + 80] = std::byte { 0x42 };
    write_u32(bytes, offset + 96, 0x12'34'56'78U);
    if (type == 1)
    {
      write_u64(bytes, offset + 100, 0x01'02'03'04'05'06'07'08ULL);
    }
    write_u64(bytes, offset + 108, 0x11'12'13'14'15'16'17'18ULL);
  }
  write_u32(bytes, offset + 116, starting_sector);
  write_u64(bytes, offset + 120, stream_size);
}

auto make_directory_file(
    xmole2::cfb::CfbVersion const version,
    std::uint32_t const directory_sector_count = 1,
    std::uint32_t const extra_sector_count     = 0) -> std::vector<std::byte>
{
  auto const size = sector_size(version);
  auto const physical_sector_count =
      std::uint32_t { 1 } + directory_sector_count + extra_sector_count;
  auto bytes = std::vector<std::byte>(
      (static_cast<std::size_t>(physical_sector_count) + 1) * size, std::byte {});
  initialize_header(bytes, version, directory_sector_count);

  auto const entry_count =
      directory_sector_count * static_cast<std::uint32_t>(size / 128);
  for (auto index = std::uint32_t {}; index < entry_count; ++index)
  {
    auto const offset = directory_entry_offset(version, index);
    write_u32(bytes, offset + 68, kNoStream);
    write_u32(bytes, offset + 72, kNoStream);
    write_u32(bytes, offset + 76, kNoStream);
  }

  fill_sector(bytes, 0, size, kFreeSector);
  write_fat_entry(bytes, size, 0, kFatSector);
  for (auto index = std::uint32_t {}; index < directory_sector_count; ++index)
  {
    auto const sector = index + 1;
    auto const next   = index + 1 == directory_sector_count ? kEndOfChain : sector + 1;
    write_fat_entry(bytes, size, sector, next);
  }

  write_directory_entry(
      bytes, version, 0, u"Root Entry", 5, 1, kNoStream, kNoStream, 1, kEndOfChain, 0);
  write_directory_entry(
      bytes, version, 1, u"Folder", 1, 1, 2, kNoStream, kNoStream, 0, 0);
  write_directory_entry(
      bytes, version, 2, u"Data\U0001f600", 2, 0, kNoStream, kNoStream, kNoStream,
      kEndOfChain, 0);
  if (directory_sector_count > 1)
  {
    auto const entries_per_sector = static_cast<std::uint32_t>(size / 128);
    write_u32(bytes, directory_entry_offset(version, 2) + 68, entries_per_sector);
    write_directory_entry(
        bytes, version, entries_per_sector, u"Tail", 2, 1, kNoStream, kNoStream,
        kNoStream, kEndOfChain, 0);
  }
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

class DirectoryCancellingSource final : public xmole2::io::ByteSource
{
public:
  DirectoryCancellingSource(
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
    if (offset >= 1024)
    {
      m_cancellation.request_cancellation();
    }
    return count;
  }

private:
  std::vector<std::byte> m_bytes;
  mutable xmole2::CancellationSource m_cancellation;
};

class DirectoryFaultSource final : public xmole2::io::ByteSource
{
public:
  explicit DirectoryFaultSource(std::vector<std::byte> bytes)
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
    if (offset >= 1024)
    {
      auto error = xmole2::io::make_error(
          xmole2::io::IoErrorCode::ReadFailed, "injected directory sector failure");
      error.native_code = 1357;
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

class DirectoryTruncatedSource final : public xmole2::io::ByteSource
{
public:
  explicit DirectoryTruncatedSource(std::vector<std::byte> bytes)
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
    if (offset >= 1024)
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
    if (update.phase == "cfb.directory")
    {
      ++directory_updates;
    }
  }

  std::size_t directory_updates {};
};

auto read_from_buffer(
    std::vector<std::byte> bytes, xmole2::OperationContext const &context)
    -> xmole2::Result<xmole2::cfb::DirectoryIndex>
{
  auto source = xmole2::io::SourceLease::from_buffer(std::move(bytes), context);
  if (!source)
  {
    return std::unexpected { std::move(source.error()) };
  }
  return xmole2::cfb::read_directory_index(*source, context);
}

auto is_error(
    xmole2::Result<xmole2::cfb::DirectoryIndex> const &result,
    xmole2::cfb::CfbErrorCode const code) -> bool
{
  return !result && result.error().domain == xmole2::ErrorDomain::Cfb &&
         result.error().code == static_cast<std::uint32_t>(code);
}

auto test_version_3_and_4_directory_entries() -> bool
{
  auto progress    = ProgressRecorder {};
  auto context     = xmole2::OperationContext {};
  context.progress = &progress;
  auto version3 =
      read_from_buffer(make_directory_file(xmole2::cfb::CfbVersion::Version3), context);
  auto version4 =
      read_from_buffer(make_directory_file(xmole2::cfb::CfbVersion::Version4), context);
  if (!version3 || !version4 || version3->entries.size() != 4 ||
      version4->entries.size() != 32 ||
      version3->directory_sector_ids != std::vector { 1U } ||
      progress.directory_updates != 2)
  {
    return false;
  }

  auto const &root    = version3->entries[0];
  auto const &storage = version3->entries[1];
  auto const &stream  = version3->entries[2];
  return root.type == xmole2::cfb::DirectoryEntryType::RootStorage &&
         root.name == u"Root Entry" && root.child_id == 1 &&
         root.clsid[0] == std::byte { 0x42 } && root.state_bits == 0x12'34'56'78U &&
         storage.type == xmole2::cfb::DirectoryEntryType::Storage &&
         storage.color == xmole2::cfb::DirectoryColor::Black &&
         storage.left_sibling_id == 2 && root.creation_time == 0 &&
         root.modified_time == 0x11'12'13'14'15'16'17'18ULL &&
         stream.type == xmole2::cfb::DirectoryEntryType::Stream &&
         stream.name == u"Data\U0001f600" && stream.creation_time == 0 &&
         stream.modified_time == 0;
}

auto test_multi_sector_chain_is_bounded() -> bool
{
  auto const context = xmole2::OperationContext {};
  auto bytes_read    = std::make_shared<std::uint64_t>();
  auto source        = xmole2::io::SourceLease::acquire(
      std::make_shared<CountingSource>(
          make_directory_file(xmole2::cfb::CfbVersion::Version3, 2, 1), bytes_read),
      context);
  if (!source)
  {
    return false;
  }
  auto directory = xmole2::cfb::read_directory_index(*source, context);
  return directory && directory->directory_sector_ids == std::vector { 1U, 2U } &&
         directory->entries.size() == 8 && directory->entries[4].name == u"Tail" &&
       *bytes_read == 512 + (3 * 512);
}

auto test_chain_and_header_consistency_errors() -> bool
{
  auto const context = xmole2::OperationContext {};

  auto cycle = make_directory_file(xmole2::cfb::CfbVersion::Version3, 2);
  write_fat_entry(cycle, 512, 2, 1);
  if (!is_error(
          read_from_buffer(std::move(cycle), context),
          xmole2::cfb::CfbErrorCode::SectorChainCycle))
  {
    return false;
  }

  auto table_sector = make_directory_file(xmole2::cfb::CfbVersion::Version3);
  write_u32(table_sector, 48, 0);
  if (!is_error(
          read_from_buffer(std::move(table_sector), context),
          xmole2::cfb::CfbErrorCode::InvalidDirectory))
  {
    return false;
  }

  auto count_mismatch = make_directory_file(xmole2::cfb::CfbVersion::Version4);
  write_u32(count_mismatch, 40, 2);
  return is_error(
      read_from_buffer(std::move(count_mismatch), context),
      xmole2::cfb::CfbErrorCode::InvalidDirectory);
}

auto test_malformed_directory_entries() -> bool
{
  auto const context = xmole2::OperationContext {};

  auto odd_name = make_directory_file(xmole2::cfb::CfbVersion::Version3);
  write_u16(
      odd_name, directory_entry_offset(xmole2::cfb::CfbVersion::Version3, 1) + 64, 3);
  if (!is_error(
          read_from_buffer(std::move(odd_name), context),
          xmole2::cfb::CfbErrorCode::InvalidDirectoryEntry))
  {
    return false;
  }

  auto invalid_type = make_directory_file(xmole2::cfb::CfbVersion::Version3);
  invalid_type[directory_entry_offset(xmole2::cfb::CfbVersion::Version3, 1) + 66] =
      std::byte { 3 };
  if (!is_error(
          read_from_buffer(std::move(invalid_type), context),
          xmole2::cfb::CfbErrorCode::InvalidDirectoryEntry))
  {
    return false;
  }

  auto invalid_utf16     = make_directory_file(xmole2::cfb::CfbVersion::Version3);
  auto const name_offset = directory_entry_offset(xmole2::cfb::CfbVersion::Version3, 1);
  write_u16(invalid_utf16, name_offset, 0xd8'00);
  write_u16(invalid_utf16, name_offset + 2, 0);
  write_u16(invalid_utf16, name_offset + 64, 4);
  if (!is_error(
          read_from_buffer(std::move(invalid_utf16), context),
          xmole2::cfb::CfbErrorCode::InvalidDirectoryEntry))
  {
    return false;
  }

  auto malformed_unused = make_directory_file(xmole2::cfb::CfbVersion::Version3);
  malformed_unused[directory_entry_offset(xmole2::cfb::CfbVersion::Version3, 3) + 10] =
      std::byte { 1 };
  if (!is_error(
          read_from_buffer(std::move(malformed_unused), context),
          xmole2::cfb::CfbErrorCode::InvalidDirectoryEntry))
  {
    return false;
  }

  auto large_v3_stream = make_directory_file(xmole2::cfb::CfbVersion::Version3);
  write_u32(
      large_v3_stream, directory_entry_offset(xmole2::cfb::CfbVersion::Version3, 2) + 120,
      0x80'00'00'01U);
  return is_error(
      read_from_buffer(std::move(large_v3_stream), context),
      xmole2::cfb::CfbErrorCode::InvalidDirectoryEntry);
}

auto test_version_3_stream_size_compatibility() -> bool
{
  auto const context = xmole2::OperationContext {};
  auto bytes         = make_directory_file(xmole2::cfb::CfbVersion::Version3);
  auto const offset  = directory_entry_offset(xmole2::cfb::CfbVersion::Version3, 2);
  write_u32(bytes, offset + 124, 0xde'ad'be'efU);

  auto directory = read_from_buffer(std::move(bytes), context);
  return directory && directory->entries[2].stream_size == 0;
}

auto test_metadata_compatibility_and_constraints() -> bool
{
  auto const context = xmole2::OperationContext {};

  auto stream_state = make_directory_file(xmole2::cfb::CfbVersion::Version3);
  write_u32(
      stream_state, directory_entry_offset(xmole2::cfb::CfbVersion::Version3, 2) + 96,
      0xca'fe'ba'beU);
  auto stream_state_result = read_from_buffer(std::move(stream_state), context);
  if (!stream_state_result ||
      stream_state_result->entries[2].state_bits != 0xca'fe'ba'beU)
  {
    return false;
  }

  auto red_root          = make_directory_file(xmole2::cfb::CfbVersion::Version3);
  auto const root_offset = directory_entry_offset(xmole2::cfb::CfbVersion::Version3, 0);
  red_root[root_offset + 67] = std::byte {};
  if (!read_from_buffer(std::move(red_root), context))
  {
    return false;
  }

  auto root_creation = make_directory_file(xmole2::cfb::CfbVersion::Version3);
  write_u64(root_creation, root_offset + 100, 1);
  return is_error(
      read_from_buffer(std::move(root_creation), context),
      xmole2::cfb::CfbErrorCode::InvalidDirectoryEntry);
}

auto test_red_black_color_constraints() -> bool
{
  auto const context = xmole2::OperationContext {};

  auto red_child_root = make_directory_file(xmole2::cfb::CfbVersion::Version3);
  red_child_root[directory_entry_offset(xmole2::cfb::CfbVersion::Version3, 1) + 67] =
      std::byte {};
  if (!is_error(
          read_from_buffer(std::move(red_child_root), context),
          xmole2::cfb::CfbErrorCode::InvalidDirectory))
  {
    return false;
  }

  auto red_red = make_directory_file(xmole2::cfb::CfbVersion::Version3);
  write_u32(
      red_red, directory_entry_offset(xmole2::cfb::CfbVersion::Version3, 2) + 68, 3);
  write_directory_entry(
      red_red, xmole2::cfb::CfbVersion::Version3, 3, u"Nested", 2, 0, kNoStream,
      kNoStream, kNoStream, kEndOfChain, 0);
  return is_error(
      read_from_buffer(std::move(red_red), context),
      xmole2::cfb::CfbErrorCode::InvalidDirectory);
}

auto make_name_order_file(
    std::u16string_view const root_name,
    std::u16string_view const left_name,
    std::u16string_view const right_name) -> std::vector<std::byte>
{
  auto bytes = make_directory_file(xmole2::cfb::CfbVersion::Version3);
  for (auto index = std::uint32_t { 1 }; index <= 3; ++index)
  {
    auto const offset = directory_entry_offset(xmole2::cfb::CfbVersion::Version3, index);
    std::fill_n(bytes.begin() + static_cast<std::ptrdiff_t>(offset), 128, std::byte {});
  }
  write_directory_entry(
      bytes, xmole2::cfb::CfbVersion::Version3, 1, root_name, 2, 1, 2, 3, kNoStream,
      kEndOfChain, 0);
  write_directory_entry(
      bytes, xmole2::cfb::CfbVersion::Version3, 2, left_name, 2, 0, kNoStream, kNoStream,
      kNoStream, kEndOfChain, 0);
  write_directory_entry(
      bytes, xmole2::cfb::CfbVersion::Version3, 3, right_name, 2, 0, kNoStream, kNoStream,
      kNoStream, kEndOfChain, 0);
  return bytes;
}

auto test_unicode_name_ordering() -> bool
{
  auto const context = xmole2::OperationContext {};

  // U+0131 maps to 'I'. Its raw value is greater than 'J', so this only forms a
  // valid left child when Unicode simple-uppercase comparison is applied.
  auto unicode_order = make_name_order_file(u"J", u"\u0131", u"K");
  return read_from_buffer(std::move(unicode_order), context).has_value();
}

auto test_invalid_name_ordering() -> bool
{
  auto const context = xmole2::OperationContext {};

  auto invalid_order = make_name_order_file(u"J", u"K", u"L");
  return is_error(
      read_from_buffer(std::move(invalid_order), context),
      xmole2::cfb::CfbErrorCode::InvalidDirectory);
}

auto test_duplicate_name_rejection() -> bool
{
  auto const context = xmole2::OperationContext {};

  auto duplicate_name = make_name_order_file(u"B", u"A", u"b");
  return is_error(
      read_from_buffer(std::move(duplicate_name), context),
      xmole2::cfb::CfbErrorCode::InvalidDirectory);
}

auto test_surrogate_name_ordering() -> bool
{
  auto const context = xmole2::OperationContext {};

  // MS-CFB compares UTF-16 code units and never uppercases surrogate pairs.
  auto surrogate_order =
      make_name_order_file(u"\U00010410", u"\U00010400", u"\U00010428");
  return read_from_buffer(std::move(surrogate_order), context).has_value();
}

auto test_root_and_reference_validation() -> bool
{
  auto const context = xmole2::OperationContext {};

  auto missing_root      = make_directory_file(xmole2::cfb::CfbVersion::Version3);
  auto const root_offset = directory_entry_offset(xmole2::cfb::CfbVersion::Version3, 0);
  missing_root[root_offset + 66] = std::byte { 1 };
  write_u32(missing_root, root_offset + 116, 0);
  if (!is_error(
          read_from_buffer(std::move(missing_root), context),
          xmole2::cfb::CfbErrorCode::InvalidDirectory))
  {
    return false;
  }

  auto bad_reference = make_directory_file(xmole2::cfb::CfbVersion::Version3);
  write_u32(
      bad_reference, directory_entry_offset(xmole2::cfb::CfbVersion::Version3, 0) + 76,
      99);
  if (!is_error(
          read_from_buffer(std::move(bad_reference), context),
          xmole2::cfb::CfbErrorCode::InvalidDirectoryEntry))
  {
    return false;
  }

  auto stream_child = make_directory_file(xmole2::cfb::CfbVersion::Version3);
  write_u32(
      stream_child, directory_entry_offset(xmole2::cfb::CfbVersion::Version3, 2) + 76, 1);
  if (!is_error(
          read_from_buffer(std::move(stream_child), context),
          xmole2::cfb::CfbErrorCode::InvalidDirectoryEntry))
  {
    return false;
  }

  auto tree_cycle = make_directory_file(xmole2::cfb::CfbVersion::Version3);
  write_u32(
      tree_cycle, directory_entry_offset(xmole2::cfb::CfbVersion::Version3, 0) + 76,
      kNoStream);
  write_u32(
      tree_cycle, directory_entry_offset(xmole2::cfb::CfbVersion::Version3, 2) + 72, 1);
  return is_error(
      read_from_buffer(std::move(tree_cycle), context),
      xmole2::cfb::CfbErrorCode::DirectoryTreeCycle);
}

auto test_budget_cancellation_and_error_chain() -> bool
{
  auto const setup_context = xmole2::OperationContext {};
  auto source              = xmole2::io::SourceLease::from_buffer(
      make_directory_file(xmole2::cfb::CfbVersion::Version3), setup_context);
  if (!source)
  {
    return false;
  }

  auto count_context                                 = xmole2::OperationContext {};
  count_context.budget.max_cfb_directory_entry_count = 3;
  auto count_limited = xmole2::cfb::read_directory_index(*source, count_context);
  if (!is_error(count_limited, xmole2::cfb::CfbErrorCode::ResourceLimitExceeded))
  {
    return false;
  }

  auto path_context                   = xmole2::OperationContext {};
  path_context.budget.max_path_length = 5;
  auto path_limited = xmole2::cfb::read_directory_index(*source, path_context);
  if (!is_error(path_limited, xmole2::cfb::CfbErrorCode::ResourceLimitExceeded))
  {
    return false;
  }

  auto memory_context                    = xmole2::OperationContext {};
  memory_context.budget.max_memory_bytes = 1500;
  auto memory_limited = xmole2::cfb::read_directory_index(*source, memory_context);
  if (!is_error(memory_limited, xmole2::cfb::CfbErrorCode::ResourceLimitExceeded))
  {
    return false;
  }

  auto cancellation              = xmole2::CancellationSource {};
  auto cancelled_context         = xmole2::OperationContext {};
  cancelled_context.cancellation = cancellation.token();
  auto cancelling_source         = xmole2::io::SourceLease::acquire(
      std::make_shared<DirectoryCancellingSource>(
          make_directory_file(xmole2::cfb::CfbVersion::Version3), cancellation),
      setup_context);
  if (!cancelling_source)
  {
    return false;
  }
  auto cancelled =
      xmole2::cfb::read_directory_index(*cancelling_source, cancelled_context);
  if (!is_error(cancelled, xmole2::cfb::CfbErrorCode::Cancelled))
  {
    return false;
  }

  auto fault_source = xmole2::io::SourceLease::acquire(
      std::make_shared<DirectoryFaultSource>(
          make_directory_file(xmole2::cfb::CfbVersion::Version3)),
      setup_context);
  if (!fault_source)
  {
    return false;
  }
  auto failed = xmole2::cfb::read_directory_index(*fault_source, setup_context);
  if (!is_error(failed, xmole2::cfb::CfbErrorCode::ReadFailed) ||
      failed.error().cause == nullptr || failed.error().cause->native_code != 1357 ||
      failed.error().native_code != 1357)
  {
    return false;
  }

  auto truncated_source = xmole2::io::SourceLease::acquire(
      std::make_shared<DirectoryTruncatedSource>(
          make_directory_file(xmole2::cfb::CfbVersion::Version3)),
      setup_context);
  if (!truncated_source)
  {
    return false;
  }
  auto truncated = xmole2::cfb::read_directory_index(*truncated_source, setup_context);
  return is_error(truncated, xmole2::cfb::CfbErrorCode::InvalidDirectory) &&
         truncated.error().cause != nullptr &&
         truncated.error().cause->domain == xmole2::ErrorDomain::Io &&
         truncated.error().cause->code ==
             static_cast<std::uint32_t>(xmole2::io::IoErrorCode::UnexpectedEndOfFile);
}

auto test_directory_computation_phase_cancellation() -> bool
{
  auto const setup_context = xmole2::OperationContext {};
  auto source              = xmole2::io::SourceLease::from_buffer(
      make_directory_file(xmole2::cfb::CfbVersion::Version3), setup_context);
  if (!source)
  {
    return false;
  }

  auto cancellation = xmole2::CancellationSource {};
  auto progress     = cfb_test::CancellingProgressSink { "cfb.directory", cancellation };
  auto context      = xmole2::OperationContext {};
  context.cancellation = cancellation.token();
  context.progress     = &progress;
  auto result          = xmole2::cfb::read_directory_index(*source, context);
  return is_error(result, xmole2::cfb::CfbErrorCode::Cancelled);
}

} // namespace

auto main() -> int
{
  if (!test_version_3_and_4_directory_entries())
  {
    return 1;
  }
  if (!test_multi_sector_chain_is_bounded())
  {
    return 2;
  }
  if (!test_chain_and_header_consistency_errors())
  {
    return 3;
  }
  if (!test_malformed_directory_entries())
  {
    return 4;
  }
  if (!test_root_and_reference_validation())
  {
    return 5;
  }
  if (!test_version_3_stream_size_compatibility())
  {
    return 6;
  }
  if (!test_metadata_compatibility_and_constraints())
  {
    return 7;
  }
  if (!test_red_black_color_constraints())
  {
    return 8;
  }
  if (!test_unicode_name_ordering())
  {
    return 9;
  }
  if (!test_invalid_name_ordering())
  {
    return 10;
  }
  if (!test_duplicate_name_rejection())
  {
    return 11;
  }
  if (!test_surrogate_name_ordering())
  {
    return 12;
  }
  if (!test_budget_cancellation_and_error_chain())
  {
    return 13;
  }
  if (!test_directory_computation_phase_cancellation())
  {
    return 14;
  }
  return 0;
}
