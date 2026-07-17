#include "xmole2/cfb/compound_file_header.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>
#include <utility>

#include "cfb_internal_utils.hpp"
#include "compound_file_header_internal.hpp"
#include "xmole2/cfb/error.hpp"

namespace xmole2::cfb
{
namespace
{

constexpr auto kHeaderSize        = std::size_t { 512 };
constexpr auto kLittleEndianOrder = std::uint16_t { 0xff'fe };
constexpr auto kVersion3Shift     = std::uint16_t { 9 };
constexpr auto kVersion4Shift     = std::uint16_t { 12 };
constexpr auto kMiniSectorShift   = std::uint16_t { 6 };
constexpr auto kMiniStreamCutoff  = std::uint32_t { 0x10'00 };
constexpr auto kMaxRegularSector  = std::uint32_t { 0xff'ff'ff'faU };
constexpr auto kEndOfChain        = std::uint32_t { 0xff'ff'ff'feU };
constexpr auto kSignature         = std::array {
  std::byte { 0xd0 }, std::byte { 0xcf }, std::byte { 0x11 }, std::byte { 0xe0 },
  std::byte { 0xa1 }, std::byte { 0xb1 }, std::byte { 0x1a }, std::byte { 0xe1 },
};

using internal::bytes_are_zero;
using internal::read_u16;
using internal::read_u32;
using internal::wrap_io_error;

auto invalid_header(std::string_view const message) -> Error
{
  return make_error(CfbErrorCode::InvalidHeader, message);
}

auto resource_error(std::string_view const message) -> Error
{
  return make_error(CfbErrorCode::ResourceLimitExceeded, message);
}

auto validate_chain_start(
    std::uint32_t const first_sector,
    std::uint32_t const declared_sector_count,
    std::uint64_t const physical_sector_count,
    std::string_view const absent_message,
    std::string_view const range_message) -> Status
{
  if (declared_sector_count == 0)
  {
    if (first_sector != kEndOfChain)
    {
      return std::unexpected { invalid_header(absent_message) };
    }
    return {};
  }
  if (first_sector >= physical_sector_count)
  {
    return std::unexpected { invalid_header(range_message) };
  }
  return {};
}

} // namespace

auto internal::read_compound_file_header(
    io::SourceLease const &source, OperationContext const &context)
    -> Result<internal::ParsedCompoundFileHeader>
{
  if (context.cancellation.is_cancelled())
  {
    return std::unexpected { make_error(
        CfbErrorCode::Cancelled, "CFB header read cancelled") };
  }
  if (!source.valid())
  {
    return std::unexpected { make_error(
        CfbErrorCode::InvalidArgument, "CFB source lease is empty") };
  }

  auto source_size = source.size(context);
  if (!source_size)
  {
    return std::unexpected { wrap_io_error(
        "failed to query CFB source size", std::move(source_size.error()),
        CfbErrorCode::InvalidHeader) };
  }
  if (*source_size < kHeaderSize)
  {
    return std::unexpected { invalid_header("CFB source is shorter than its header") };
  }

  auto reader = source.reader(0, context);
  if (!reader)
  {
    return std::unexpected { wrap_io_error(
        "failed to create CFB header reader", std::move(reader.error()),
        CfbErrorCode::InvalidHeader) };
  }
  auto bytes  = std::array<std::byte, kHeaderSize> {};
  auto status = reader->read_exact(bytes, context);
  if (!status)
  {
    return std::unexpected { wrap_io_error(
        "failed to read CFB header", std::move(status.error()),
        CfbErrorCode::InvalidHeader) };
  }
  if (context.cancellation.is_cancelled())
  {
    return std::unexpected { make_error(
        CfbErrorCode::Cancelled, "CFB header read cancelled") };
  }

  auto const view = std::span<std::byte const> { bytes };
  if (!std::equal(kSignature.begin(), kSignature.end(), view.begin()))
  {
    return std::unexpected { invalid_header("CFB signature is invalid") };
  }
  if (!bytes_are_zero(view, 8, 16))
  {
    return std::unexpected { invalid_header("CFB header CLSID must be zero") };
  }
  auto const minor_version = read_u16(view, 24);
  auto const major_version = read_u16(view, 26);
  auto const sector_shift  = read_u16(view, 30);
  if (major_version != static_cast<std::uint16_t>(CfbVersion::Version3) &&
      major_version != static_cast<std::uint16_t>(CfbVersion::Version4))
  {
    return std::unexpected { make_error(
        CfbErrorCode::UnsupportedVersion, "CFB major version is unsupported") };
  }
  if ((major_version == static_cast<std::uint16_t>(CfbVersion::Version3) &&
       sector_shift != kVersion3Shift) ||
      (major_version == static_cast<std::uint16_t>(CfbVersion::Version4) &&
       sector_shift != kVersion4Shift))
  {
    return std::unexpected { invalid_header(
        "CFB sector shift does not match its major version") };
  }
  if (read_u16(view, 28) != kLittleEndianOrder)
  {
    return std::unexpected { invalid_header("CFB byte order is invalid") };
  }
  if (read_u16(view, 32) != kMiniSectorShift)
  {
    return std::unexpected { invalid_header("CFB mini-sector shift is invalid") };
  }
  if (!bytes_are_zero(view, 34, 6))
  {
    return std::unexpected { invalid_header("CFB reserved header bytes must be zero") };
  }
  if (read_u32(view, 56) != kMiniStreamCutoff)
  {
    return std::unexpected { invalid_header("CFB mini-stream cutoff is invalid") };
  }

  auto const sector_size = std::uint64_t { 1 } << sector_shift;
  if (*source_size < sector_size * 2 || (*source_size % sector_size) != 0)
  {
    return std::unexpected { invalid_header(
        "CFB source size is not aligned to its sector size") };
  }
  auto const sector_count = (*source_size / sector_size) - 1;
  if (sector_count > static_cast<std::uint64_t>(kMaxRegularSector) + 1)
  {
    return std::unexpected { invalid_header(
        "CFB source exceeds the addressable sector identifier range") };
  }
  if (sector_count > context.budget.max_cfb_sector_count)
  {
    return std::unexpected { resource_error(
        "CFB physical sector count exceeds the resource budget") };
  }

  auto const directory_sector_count = read_u32(view, 40);
  auto const fat_sector_count       = read_u32(view, 44);
  auto const first_directory_sector = read_u32(view, 48);
  auto const first_mini_fat_sector  = read_u32(view, 60);
  auto const mini_fat_sector_count  = read_u32(view, 64);
  auto const first_difat_sector     = read_u32(view, 68);
  auto const difat_sector_count     = read_u32(view, 72);

  if (major_version == static_cast<std::uint16_t>(CfbVersion::Version3) &&
      directory_sector_count != 0)
  {
    return std::unexpected { invalid_header(
        "CFB version 3 directory sector count must be zero") };
  }
  if (fat_sector_count == 0 || fat_sector_count > sector_count ||
      directory_sector_count > sector_count || mini_fat_sector_count > sector_count ||
      difat_sector_count > sector_count)
  {
    return std::unexpected { invalid_header(
        "CFB declared sector counts exceed the physical file") };
  }
  if (first_directory_sector >= sector_count)
  {
    return std::unexpected { invalid_header(
        "CFB first directory sector is outside the physical file") };
  }

  status = validate_chain_start(
      first_mini_fat_sector, mini_fat_sector_count, sector_count,
      "CFB MiniFAT start must be end-of-chain when no MiniFAT is declared",
      "CFB first MiniFAT sector is outside the physical file");
  if (!status)
  {
    return std::unexpected { std::move(status.error()) };
  }
  status = validate_chain_start(
      first_difat_sector, difat_sector_count, sector_count,
      "CFB DIFAT start must be end-of-chain when no DIFAT is declared",
      "CFB first DIFAT sector is outside the physical file");
  if (!status)
  {
    return std::unexpected { std::move(status.error()) };
  }

  if (context.progress != nullptr)
  {
    context.progress->report(ProgressUpdate { "cfb.header", kHeaderSize, kHeaderSize });
  }

  auto parsed = internal::ParsedCompoundFileHeader {
    .header =
        CompoundFileHeader {
                            .version                 = static_cast<CfbVersion>(major_version),
                            .minor_version           = minor_version,
                            .sector_size             = static_cast<std::uint32_t>(sector_size),
                            .mini_sector_size        = std::uint32_t { 1 } << kMiniSectorShift,
                            .sector_count            = sector_count,
                            .directory_sector_count  = directory_sector_count,
                            .fat_sector_count        = fat_sector_count,
                            .first_directory_sector  = first_directory_sector,
                            .transaction_signature   = read_u32(view, 52),
                            .mini_stream_cutoff_size = kMiniStreamCutoff,
                            .first_mini_fat_sector   = first_mini_fat_sector,
                            .mini_fat_sector_count   = mini_fat_sector_count,
                            .first_difat_sector      = first_difat_sector,
                            .difat_sector_count      = difat_sector_count,
                            },
  };
  for (auto index = std::size_t {}; index < parsed.difat.size(); ++index)
  {
    parsed.difat[index] = read_u32(view, 76 + (index * 4));
  }
  return parsed;
}

auto read_header(io::SourceLease const &source, OperationContext const &context)
    -> Result<CompoundFileHeader>
{
  auto parsed = internal::read_compound_file_header(source, context);
  if (!parsed)
  {
    return std::unexpected { std::move(parsed.error()) };
  }
  return std::move(parsed->header);
}

} // namespace xmole2::cfb
