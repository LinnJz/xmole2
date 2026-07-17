#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <optional>
#include <span>
#include <string_view>
#include <utility>

#include "xmole2/cfb/error.hpp"
#include "xmole2/io/error.hpp"

namespace xmole2::cfb::internal
{

inline auto checked_add(std::uint64_t const left, std::uint64_t const right)
    -> std::optional<std::uint64_t>
{
  if (right > std::numeric_limits<std::uint64_t>::max() - left)
  {
    return std::nullopt;
  }
  return left + right;
}

inline auto checked_multiply(std::uint64_t const left, std::uint64_t const right)
    -> std::optional<std::uint64_t>
{
  if (left != 0 && right > std::numeric_limits<std::uint64_t>::max() / left)
  {
    return std::nullopt;
  }
  return left * right;
}

inline auto ceiling_divide(std::uint64_t const value, std::uint64_t const divisor)
    -> std::uint64_t
{
  return value == 0 ? 0 : ((value - 1) / divisor) + 1;
}

inline auto add_memory(
    std::uint64_t &total, std::uint64_t const value, std::uint64_t const limit) -> bool
{
  auto const next = checked_add(total, value);
  if (!next || *next > limit)
  {
    return false;
  }
  total = *next;
  return true;
}

inline auto read_u16(std::span<std::byte const> const bytes, std::size_t const offset)
    -> std::uint16_t
{
  auto const low  = std::to_integer<std::uint16_t>(bytes[offset]);
  auto const high = std::to_integer<std::uint16_t>(bytes[offset + 1]);
  return static_cast<std::uint16_t>(low | static_cast<std::uint16_t>(high << 8U));
}

inline auto read_u32(std::span<std::byte const> const bytes, std::size_t const offset)
    -> std::uint32_t
{
  auto result = std::uint32_t {};
  for (auto index = std::size_t {}; index < 4; ++index)
  {
    result |= std::to_integer<std::uint32_t>(bytes[offset + index])
           << static_cast<std::uint32_t>(index * 8U);
  }
  return result;
}

inline auto read_u64(std::span<std::byte const> const bytes, std::size_t const offset)
    -> std::uint64_t
{
  auto result = std::uint64_t {};
  for (auto index = std::size_t {}; index < 8; ++index)
  {
    result |= std::to_integer<std::uint64_t>(bytes[offset + index])
           << static_cast<std::uint64_t>(index * 8U);
  }
  return result;
}

inline auto bytes_are_zero(
    std::span<std::byte const> const bytes,
    std::size_t const offset,
    std::size_t const count) -> bool
{
  return std::all_of(
      bytes.begin() + static_cast<std::ptrdiff_t>(offset),
      bytes.begin() + static_cast<std::ptrdiff_t>(offset + count),
      [](std::byte const value) { return value == std::byte {}; });
}

inline auto map_io_error(Error const &cause, CfbErrorCode const unexpected_end_code)
    -> CfbErrorCode
{
  if (cause.domain != ErrorDomain::Io)
  {
    return CfbErrorCode::ReadFailed;
  }
  if (cause.code == static_cast<std::uint32_t>(io::IoErrorCode::Cancelled))
  {
    return CfbErrorCode::Cancelled;
  }
  if (cause.code == static_cast<std::uint32_t>(io::IoErrorCode::ResourceLimitExceeded))
  {
    return CfbErrorCode::ResourceLimitExceeded;
  }
  if (cause.code == static_cast<std::uint32_t>(io::IoErrorCode::UnexpectedEndOfFile))
  {
    return unexpected_end_code;
  }
  return CfbErrorCode::ReadFailed;
}

inline auto wrap_io_error(
    std::string_view const message, Error cause, CfbErrorCode const unexpected_end_code)
    -> Error
{
  auto error        = make_error(map_io_error(cause, unexpected_end_code), message);
  error.native_code = cause.native_code;
  error.cause       = std::make_shared<Error const>(std::move(cause));
  return error;
}

} // namespace xmole2::cfb::internal
