/// @file
/// Stable ZIP error codes.

#pragma once

#include <cstdint>
#include <string_view>

#include "xmole2/base/error.hpp"
#include "xmole2/zip/export.hpp"

namespace xmole2::zip
{

enum class ZipErrorCode : std::uint32_t
{
  Cancelled = 1,
  InvalidArgument,
  InvalidArchive,
  EntryNotFound,
  DuplicateEntry,
  UnsafeEntryName,
  EncryptedEntry,
  UnsupportedCompression,
  ResourceLimitExceeded,
  ReadFailed,
  IntegrityCheckFailed,
  ReaderClosed,
};

[[nodiscard]] XMOLE2_ZIP_API auto make_error(
    ZipErrorCode code, std::string_view message, Severity severity = Severity::Error)
    -> Error;

} // namespace xmole2::zip
