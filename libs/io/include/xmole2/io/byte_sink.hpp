/// @file
/// Sequential byte sink port.

#pragma once

#include <cstddef>
#include <cstdint>
#include <span>

#include "xmole2/base/operation_context.hpp"
#include "xmole2/io/export.hpp"

namespace xmole2::io
{

/// Receives a byte sequence and either writes each span completely or returns an error.
class XMOLE2_IO_API ByteSink
{
public:
  virtual ~ByteSink() = default;

  [[nodiscard]] virtual auto write(
      std::span<std::byte const> source, OperationContext const &context) -> Status = 0;
  [[nodiscard]] virtual auto flush(OperationContext const &context) -> Status       = 0;
  [[nodiscard]] virtual auto close(OperationContext const &context) -> Status       = 0;
  [[nodiscard]] virtual auto bytes_written() const noexcept -> std::uint64_t        = 0;
};

} // namespace xmole2::io

