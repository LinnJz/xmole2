/// @file
/// Shared result, error, and diagnostic types.

#pragma once

#include <cstdint>
#include <expected>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

#include "xmole2/base/export.hpp"

namespace xmole2
{

enum class ErrorDomain : std::uint8_t
{
  Base,
  Io,
  Zip,
  Cfb,
  Opc,
  Xml,
  Crypto,
  Words,
  Cells,
  Slides,
  Rendering,
};

enum class Severity : std::uint8_t
{
  Information,
  Warning,
  Error,
  Fatal,
};

enum class LocationKind : std::uint8_t
{
  Unknown,
  FilePath,
  PackagePart,
  CompoundStream,
  XmlPath,
  WordNode,
  SpreadsheetCell,
  SlideShape,
};

struct DocumentLocation
{
  LocationKind kind { LocationKind::Unknown };
  std::string primary;
  std::optional<std::string> secondary;
};

struct Error
{
  ErrorDomain domain { ErrorDomain::Base };
  std::uint32_t code {};
  Severity severity { Severity::Error };
  std::string message;
  std::optional<DocumentLocation> location;
  std::shared_ptr<Error const> cause;
  std::optional<std::int64_t> native_code;
};

template<typename T>
using Result = std::expected<T, Error>;

using Status = Result<void>;

[[nodiscard]] XMOLE2_BASE_API auto make_error(
    ErrorDomain domain,
    std::uint32_t code,
    std::string_view message,
    Severity severity = Severity::Error) -> Error;

} // namespace xmole2
