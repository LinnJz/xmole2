#include "sector_reader_internal.hpp"

#include <cstdint>

#include "cfb_internal_utils.hpp"
#include "xmole2/cfb/error.hpp"

namespace xmole2::cfb::internal
{
namespace
{

auto cancelled_error() -> Error
{
  return make_error(CfbErrorCode::Cancelled, "CFB sector read cancelled");
}

} // namespace

auto read_sector(
    io::SourceLease const &source,
    CompoundFileHeader const &header,
    std::uint32_t const sector,
    std::span<std::byte> const destination,
    OperationContext const &context,
    CfbErrorCode const unexpected_end_code,
    std::string_view const failure_message) -> Status
{
  if (context.cancellation.is_cancelled())
  {
    return std::unexpected { cancelled_error() };
  }
  if (destination.size() != header.sector_size)
  {
    return std::unexpected { make_error(
        CfbErrorCode::InvalidArgument,
        "CFB sector destination does not match the sector size") };
  }
  if (sector >= header.sector_count)
  {
    return std::unexpected { make_error(
        CfbErrorCode::SectorOutOfRange,
        "CFB sector identifier is outside the physical file") };
  }

  auto const sector_number = static_cast<std::uint64_t>(sector) + 1;
  auto const offset        = checked_multiply(sector_number, header.sector_size);
  if (!offset)
  {
    return std::unexpected { make_error(
        CfbErrorCode::SectorOutOfRange, "CFB sector offset overflows") };
  }
  auto reader = source.reader(*offset, context);
  if (!reader)
  {
    return std::unexpected { wrap_io_error(
        failure_message, std::move(reader.error()), unexpected_end_code) };
  }
  auto status = reader->read_exact(destination, context);
  if (!status)
  {
    return std::unexpected { wrap_io_error(
        failure_message, std::move(status.error()), unexpected_end_code) };
  }
  if (context.cancellation.is_cancelled())
  {
    return std::unexpected { cancelled_error() };
  }
  return {};
}

} // namespace xmole2::cfb::internal
