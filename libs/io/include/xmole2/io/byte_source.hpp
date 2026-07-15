/// @file
/// Random-access byte source port.

#pragma once

#include <cstddef>
#include <cstdint>
#include <span>

#include "xmole2/base/operation_context.hpp"
#include "xmole2/io/export.hpp"

namespace xmole2::io
{

/// Supplies a stable byte sequence without exposing a platform file handle.
/// Const size() and read_at() calls may execute concurrently on one instance.
class XMOLE2_IO_API ByteSource
{
public:
  virtual ~ByteSource() = default;

  [[nodiscard]] virtual auto size(OperationContext const &context) const
      -> Result<std::uint64_t> = 0;

  /// Reads at most destination.size() bytes. A zero result denotes EOF.
  [[nodiscard]] virtual auto read_at(
      std::uint64_t offset,
      std::span<std::byte> destination,
      OperationContext const &context) const -> Result<std::size_t> = 0;
};

} // namespace xmole2::io
