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
#include "xmole2/cfb/compound_file_header.hpp"
#include "xmole2/cfb/error.hpp"
#include "xmole2/io/byte_source.hpp"
#include "xmole2/io/error.hpp"
#include "xmole2/io/source_lease.hpp"

namespace
{

constexpr auto kEndOfChain = std::uint32_t { 0xff'ff'ff'feU };
constexpr auto kFreeSector = std::uint32_t { 0xff'ff'ff'ffU };

using cfb_test::write_u16;
using cfb_test::write_u32;

auto make_compound_file(xmole2::cfb::CfbVersion const version) -> std::vector<std::byte>
{
  auto const major = static_cast<std::uint16_t>(version);
  auto const shift =
      static_cast<std::uint16_t>(version == xmole2::cfb::CfbVersion::Version3 ? 9 : 12);
  auto const sector_size = std::size_t { 1 } << shift;
  auto bytes             = std::vector<std::byte>(sector_size * 3);
  auto const signature   = std::array {
    std::byte { 0xd0 }, std::byte { 0xcf }, std::byte { 0x11 }, std::byte { 0xe0 },
    std::byte { 0xa1 }, std::byte { 0xb1 }, std::byte { 0x1a }, std::byte { 0xe1 },
  };
  std::copy(std::begin(signature), std::end(signature), bytes.begin());
  write_u16(bytes, 24, 0x00'3e);
  write_u16(bytes, 26, major);
  write_u16(bytes, 28, 0xff'fe);
  write_u16(bytes, 30, shift);
  write_u16(bytes, 32, 6);
  write_u32(bytes, 40, version == xmole2::cfb::CfbVersion::Version3 ? 0U : 1U);
  write_u32(bytes, 44, 1);
  write_u32(bytes, 48, 1);
  write_u32(bytes, 52, 0x12'34'56'78U);
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

class FaultSource final : public xmole2::io::ByteSource
{
public:
  auto size(xmole2::OperationContext const &) const
      -> xmole2::Result<std::uint64_t> override
  {
    return 1536;
  }

  auto read_at(std::uint64_t, std::span<std::byte>, xmole2::OperationContext const &)
      const -> xmole2::Result<std::size_t> override
  {
    auto error = xmole2::io::make_error(
        xmole2::io::IoErrorCode::ReadFailed, "injected CFB source failure");
    error.native_code = 9876;
    return std::unexpected { std::move(error) };
  }
};

class TruncatedHeaderSource final : public xmole2::io::ByteSource
{
public:
  auto size(xmole2::OperationContext const &) const
      -> xmole2::Result<std::uint64_t> override
  {
    return 1536;
  }

  auto read_at(std::uint64_t, std::span<std::byte>, xmole2::OperationContext const &)
      const -> xmole2::Result<std::size_t> override
  {
    return std::size_t {};
  }
};

class VirtualHeaderSource final : public xmole2::io::ByteSource
{
public:
  VirtualHeaderSource(std::vector<std::byte> header, std::uint64_t const reported_size)
      : m_header { std::move(header) }
      , m_reported_size { reported_size }
  {
    m_header.resize(512);
  }

  auto size(xmole2::OperationContext const &) const
      -> xmole2::Result<std::uint64_t> override
  {
    return m_reported_size;
  }

  auto read_at(
      std::uint64_t const offset,
      std::span<std::byte> const destination,
      xmole2::OperationContext const &) const -> xmole2::Result<std::size_t> override
  {
    if (offset >= m_header.size())
    {
      return std::size_t {};
    }
    auto const count = static_cast<std::size_t>(std::min<std::uint64_t>(
        destination.size(), static_cast<std::uint64_t>(m_header.size()) - offset));
    std::memcpy(destination.data(), m_header.data() + offset, count);
    return count;
  }

private:
  std::vector<std::byte> m_header;
  std::uint64_t m_reported_size {};
};

class ProgressRecorder final : public xmole2::ProgressSink
{
public:
  auto report(xmole2::ProgressUpdate const &update) -> void override
  {
    if (update.phase == "cfb.header" && update.completed == 512 && update.total == 512)
    {
      ++header_updates;
    }
  }

  std::size_t header_updates {};
};

auto read_from_buffer(
    std::vector<std::byte> bytes, xmole2::OperationContext const &context)
    -> xmole2::Result<xmole2::cfb::CompoundFileHeader>
{
  auto source = xmole2::io::SourceLease::from_buffer(std::move(bytes), context);
  if (!source)
  {
    return std::unexpected { std::move(source.error()) };
  }
  return xmole2::cfb::read_header(*source, context);
}

auto is_error(
    xmole2::Result<xmole2::cfb::CompoundFileHeader> const &result,
    xmole2::cfb::CfbErrorCode code) -> bool;

auto test_version_3_and_4_headers() -> bool
{
  auto progress    = ProgressRecorder {};
  auto context     = xmole2::OperationContext {};
  context.progress = &progress;
  auto version3 =
      read_from_buffer(make_compound_file(xmole2::cfb::CfbVersion::Version3), context);
  auto version4 =
      read_from_buffer(make_compound_file(xmole2::cfb::CfbVersion::Version4), context);

  return version3 && version4 && version3->version == xmole2::cfb::CfbVersion::Version3 &&
         version3->sector_size == 512 && version3->mini_sector_size == 64 &&
         version3->sector_count == 2 && version3->fat_sector_count == 1 &&
         version3->first_directory_sector == 1 &&
         version3->transaction_signature == 0x12'34'56'78U &&
         version4->version == xmole2::cfb::CfbVersion::Version4 &&
         version4->sector_size == 4096 && version4->directory_sector_count == 1 &&
         version4->sector_count == 2 && progress.header_updates == 2;
}

auto test_minor_version_is_advisory_and_preserved() -> bool
{
  auto const context = xmole2::OperationContext {};
  auto version3      = make_compound_file(xmole2::cfb::CfbVersion::Version3);
  auto version4      = make_compound_file(xmole2::cfb::CfbVersion::Version4);
  write_u16(version3, 24, 0x12'34);
  write_u16(version4, 24, 0x00'04);

  auto parsed3 = read_from_buffer(std::move(version3), context);
  auto parsed4 = read_from_buffer(std::move(version4), context);
  return parsed3 && parsed4 && parsed3->minor_version == 0x12'34 &&
         parsed4->minor_version == 0x00'04;
}

auto test_header_read_is_bounded() -> bool
{
  auto const context = xmole2::OperationContext {};
  auto bytes_read    = std::make_shared<std::uint64_t>();
  auto source        = xmole2::io::SourceLease::acquire(
      std::make_shared<CountingSource>(
          make_compound_file(xmole2::cfb::CfbVersion::Version4), bytes_read),
      context);
  if (!source)
  {
    return false;
  }
  auto header = xmole2::cfb::read_header(*source, context);
  return header && *bytes_read == 512;
}

auto test_addressable_sector_boundary_without_materializing_large_file() -> bool
{
  constexpr auto kMaxRegularSector = std::uint64_t { 0xff'ff'ff'faU };
  constexpr auto kSectorSize       = std::uint64_t { 4096 };
  constexpr auto kSectorCount      = kMaxRegularSector + 1;
  constexpr auto kSourceSize       = (kSectorCount + 1) * kSectorSize;

  auto context                             = xmole2::OperationContext {};
  context.budget.max_input_bytes           = kSourceSize + kSectorSize;
  context.budget.max_single_resource_bytes = kSourceSize + kSectorSize;
  context.budget.max_cfb_sector_count      = kSectorCount + 1;
  auto source                              = xmole2::io::SourceLease::acquire(
      std::make_shared<VirtualHeaderSource>(
          make_compound_file(xmole2::cfb::CfbVersion::Version4), kSourceSize),
      context);
  if (!source)
  {
    return false;
  }
  auto boundary = xmole2::cfb::read_header(*source, context);
  if (!boundary || boundary->sector_count != kSectorCount)
  {
    return false;
  }

  auto oversized_source = xmole2::io::SourceLease::acquire(
      std::make_shared<VirtualHeaderSource>(
          make_compound_file(xmole2::cfb::CfbVersion::Version4),
          kSourceSize + kSectorSize),
      context);
  return oversized_source &&
         is_error(
             xmole2::cfb::read_header(*oversized_source, context),
             xmole2::cfb::CfbErrorCode::InvalidHeader);
}

auto is_error(
    xmole2::Result<xmole2::cfb::CompoundFileHeader> const &result,
    xmole2::cfb::CfbErrorCode const code) -> bool
{
  return !result && result.error().domain == xmole2::ErrorDomain::Cfb &&
         result.error().code == static_cast<std::uint32_t>(code);
}

auto test_malformed_headers() -> bool
{
  auto const context = xmole2::OperationContext {};

  auto truncated = std::vector<std::byte>(511);
  if (!is_error(
          read_from_buffer(std::move(truncated), context),
          xmole2::cfb::CfbErrorCode::InvalidHeader))
  {
    return false;
  }

  auto invalid_signature = make_compound_file(xmole2::cfb::CfbVersion::Version3);
  invalid_signature[0]   = std::byte {};
  if (!is_error(
          read_from_buffer(std::move(invalid_signature), context),
          xmole2::cfb::CfbErrorCode::InvalidHeader))
  {
    return false;
  }

  auto unsupported = make_compound_file(xmole2::cfb::CfbVersion::Version3);
  write_u16(unsupported, 26, 5);
  if (!is_error(
          read_from_buffer(std::move(unsupported), context),
          xmole2::cfb::CfbErrorCode::UnsupportedVersion))
  {
    return false;
  }

  auto wrong_order = make_compound_file(xmole2::cfb::CfbVersion::Version3);
  write_u16(wrong_order, 28, 0xfe'ff);
  if (!is_error(
          read_from_buffer(std::move(wrong_order), context),
          xmole2::cfb::CfbErrorCode::InvalidHeader))
  {
    return false;
  }

  auto invalid_count = make_compound_file(xmole2::cfb::CfbVersion::Version3);
  write_u32(invalid_count, 44, 3);
  if (!is_error(
          read_from_buffer(std::move(invalid_count), context),
          xmole2::cfb::CfbErrorCode::InvalidHeader))
  {
    return false;
  }

  auto missing_mini_fat_start = make_compound_file(xmole2::cfb::CfbVersion::Version3);
  write_u32(missing_mini_fat_start, 64, 1);
  return is_error(
      read_from_buffer(std::move(missing_mini_fat_start), context),
      xmole2::cfb::CfbErrorCode::InvalidHeader);
}

auto test_budget_and_cancellation() -> bool
{
  auto const setup_context = xmole2::OperationContext {};
  auto source              = xmole2::io::SourceLease::from_buffer(
      make_compound_file(xmole2::cfb::CfbVersion::Version3), setup_context);
  if (!source)
  {
    return false;
  }

  auto sector_context                        = xmole2::OperationContext {};
  sector_context.budget.max_cfb_sector_count = 1;
  auto sector_limited = xmole2::cfb::read_header(*source, sector_context);
  if (!is_error(sector_limited, xmole2::cfb::CfbErrorCode::ResourceLimitExceeded))
  {
    return false;
  }

  auto input_context                   = xmole2::OperationContext {};
  input_context.budget.max_input_bytes = 512;
  auto input_limited                   = xmole2::cfb::read_header(*source, input_context);
  if (!is_error(input_limited, xmole2::cfb::CfbErrorCode::ResourceLimitExceeded) ||
      input_limited.error().cause == nullptr)
  {
    return false;
  }

  auto cancellation              = xmole2::CancellationSource {};
  auto cancelled_context         = xmole2::OperationContext {};
  cancelled_context.cancellation = cancellation.token();
  cancellation.request_cancellation();
  auto cancelled = xmole2::cfb::read_header(*source, cancelled_context);
  return is_error(cancelled, xmole2::cfb::CfbErrorCode::Cancelled);
}

auto test_source_error_chain_is_preserved() -> bool
{
  auto const context = xmole2::OperationContext {};
  auto source =
      xmole2::io::SourceLease::acquire(std::make_shared<FaultSource>(), context);
  if (!source)
  {
    return false;
  }
  auto header = xmole2::cfb::read_header(*source, context);
  if (!is_error(header, xmole2::cfb::CfbErrorCode::ReadFailed) ||
      header.error().cause == nullptr ||
      header.error().cause->domain != xmole2::ErrorDomain::Io ||
      header.error().cause->native_code != 9876 || header.error().native_code != 9876)
  {
    return false;
  }

  auto truncated_source = xmole2::io::SourceLease::acquire(
      std::make_shared<TruncatedHeaderSource>(), context);
  if (!truncated_source)
  {
    return false;
  }
  auto truncated = xmole2::cfb::read_header(*truncated_source, context);
  return is_error(truncated, xmole2::cfb::CfbErrorCode::InvalidHeader) &&
         truncated.error().cause != nullptr &&
         truncated.error().cause->domain == xmole2::ErrorDomain::Io &&
         truncated.error().cause->code ==
             static_cast<std::uint32_t>(xmole2::io::IoErrorCode::UnexpectedEndOfFile);
}

auto test_empty_source_is_rejected() -> bool
{
  auto const context = xmole2::OperationContext {};
  auto source        = xmole2::io::SourceLease {};
  return is_error(
      xmole2::cfb::read_header(source, context),
      xmole2::cfb::CfbErrorCode::InvalidArgument);
}

} // namespace

auto main() -> int
{
  if (!test_version_3_and_4_headers())
  {
    return 1;
  }
  if (!test_header_read_is_bounded())
  {
    return 2;
  }
  if (!test_addressable_sector_boundary_without_materializing_large_file())
  {
    return 3;
  }
  if (!test_minor_version_is_advisory_and_preserved())
  {
    return 4;
  }
  if (!test_malformed_headers())
  {
    return 5;
  }
  if (!test_budget_and_cancellation())
  {
    return 6;
  }
  if (!test_source_error_chain_is_preserved())
  {
    return 7;
  }
  if (!test_empty_source_is_rejected())
  {
    return 8;
  }
  return 0;
}
