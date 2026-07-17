#include <cstddef>
#include <cstdint>
#include <type_traits>
#include <vector>

#include "xmole2/cfb/cfb_stream_reader.hpp"
#include "xmole2/io/source_lease.hpp"
#include "xmole2/zip/zip_archive.hpp"

namespace
{

auto append_u16(std::vector<std::byte> &bytes, std::uint16_t const value) -> void
{
  bytes.push_back(static_cast<std::byte>(value & 0xffU));
  bytes.push_back(static_cast<std::byte>((value >> 8U) & 0xffU));
}

auto append_u32(std::vector<std::byte> &bytes, std::uint32_t const value) -> void
{
  append_u16(bytes, static_cast<std::uint16_t>(value & 0xff'ffU));
  append_u16(bytes, static_cast<std::uint16_t>(value >> 16U));
}

auto empty_zip() -> std::vector<std::byte>
{
  auto bytes = std::vector<std::byte> {};
  append_u32(bytes, 0x06'05'4b'50U);
  append_u16(bytes, 0);
  append_u16(bytes, 0);
  append_u16(bytes, 0);
  append_u16(bytes, 0);
  append_u32(bytes, 0);
  append_u32(bytes, 0);
  append_u16(bytes, 0);
  return bytes;
}

} // namespace

auto main() -> int
{
  static_assert(!std::is_copy_constructible_v<xmole2::cfb::CfbStreamReader>);
  static_assert(std::is_move_constructible_v<xmole2::cfb::CfbStreamReader>);

  auto const cfb_reader = xmole2::cfb::CfbStreamReader {};
  auto const context    = xmole2::OperationContext {};
  auto source           = xmole2::io::SourceLease::from_buffer(empty_zip(), context);
  if (!source)
  {
    return 1;
  }

  auto archive = xmole2::zip::ZipArchive::open(std::move(*source), context);
  return archive && archive->entry_count() == 0 && !cfb_reader.finished() ? 0 : 1;
}
