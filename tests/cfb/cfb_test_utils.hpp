#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>
#include <utility>
#include <vector>

#include "xmole2/base/operation_context.hpp"
#include "xmole2/cfb/compound_file_header.hpp"

namespace cfb_test
{

inline auto write_u16(
    std::vector<std::byte> &bytes, std::size_t const offset, std::uint16_t const value)
    -> void
{
  bytes[offset]     = static_cast<std::byte>(value & 0xffU);
  bytes[offset + 1] = static_cast<std::byte>((value >> 8U) & 0xffU);
}

inline auto write_u32(
    std::vector<std::byte> &bytes, std::size_t const offset, std::uint32_t const value)
    -> void
{
  write_u16(bytes, offset, static_cast<std::uint16_t>(value & 0xff'ffU));
  write_u16(bytes, offset + 2, static_cast<std::uint16_t>(value >> 16U));
}

inline auto write_u64(
    std::vector<std::byte> &bytes, std::size_t const offset, std::uint64_t const value)
    -> void
{
  write_u32(bytes, offset, static_cast<std::uint32_t>(value & 0xff'ff'ff'ffULL));
  write_u32(bytes, offset + 4, static_cast<std::uint32_t>(value >> 32U));
}

inline auto sector_size(xmole2::cfb::CfbVersion const version) -> std::size_t
{
  return version == xmole2::cfb::CfbVersion::Version3 ? 512 : 4096;
}

inline auto sector_offset(std::uint32_t const sector, std::size_t const size)
    -> std::size_t
{
  return (static_cast<std::size_t>(sector) + 1) * size;
}

inline auto fill_sector(
    std::vector<std::byte> &bytes,
    std::uint32_t const sector,
    std::size_t const size,
    std::uint32_t const value) -> void
{
  for (auto index = std::size_t {}; index < size / 4; ++index)
  {
    write_u32(bytes, sector_offset(sector, size) + (index * 4), value);
  }
}

class CancellingProgressSink final : public xmole2::ProgressSink
{
public:
  CancellingProgressSink(
      std::string_view const phase, xmole2::CancellationSource cancellation)
      : m_phase { phase }
      , m_cancellation { std::move(cancellation) }
  {
  }

  auto report(xmole2::ProgressUpdate const &update) -> void override
  {
    if (update.phase == m_phase && update.total && update.completed == *update.total)
    {
      m_cancellation.request_cancellation();
    }
  }

private:
  std::string_view m_phase;
  xmole2::CancellationSource m_cancellation;
};

} // namespace cfb_test
