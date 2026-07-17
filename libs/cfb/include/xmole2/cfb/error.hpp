/// @file
/// Stable CFB error codes.

#pragma once

#include <cstdint>
#include <string_view>

#include "xmole2/base/error.hpp"
#include "xmole2/cfb/export.hpp"

namespace xmole2::cfb
{

enum class CfbErrorCode : std::uint32_t
{
  Cancelled = 1,
  InvalidArgument,
  InvalidHeader,
  UnsupportedVersion,
  ResourceLimitExceeded,
  ReadFailed,
  InvalidSectorTable,
  SectorOutOfRange,
  SectorChainCycle,
  InvalidDirectory,
  InvalidDirectoryEntry,
  DirectoryTreeCycle,
  InvalidMiniFat,
  InvalidMiniStream,
  InvalidStream,
};

[[nodiscard]] XMOLE2_CFB_API auto make_error(
    CfbErrorCode code, std::string_view message, Severity severity = Severity::Error)
    -> Error;

} // namespace xmole2::cfb
