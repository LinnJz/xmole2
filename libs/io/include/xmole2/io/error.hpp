/// @file
/// I/O error codes shared by the public I/O ports.

#pragma once

#include <cstdint>
#include <string_view>

#include "xmole2/base/error.hpp"
#include "xmole2/io/export.hpp"

namespace xmole2::io
{

enum class IoErrorCode : std::uint32_t
{
  Cancelled = 1,
  InvalidArgument,
  OpenFailed,
  ReadFailed,
  WriteFailed,
  FlushFailed,
  CloseFailed,
  SourceClosed,
  SinkClosed,
  OffsetOutOfRange,
  UnexpectedEndOfFile,
  ResourceLimitExceeded,
  TemporaryFileCreationFailed,
  AtomicReplaceFailed,
  AlreadyCommitted,
};

[[nodiscard]] XMOLE2_IO_API auto make_error(
    IoErrorCode code, std::string_view message, Severity severity = Severity::Error)
    -> Error;

} // namespace xmole2::io

