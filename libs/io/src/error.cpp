#include "xmole2/io/error.hpp"

namespace xmole2::io
{

auto make_error(
    IoErrorCode const code, std::string_view const message, Severity const severity)
    -> Error
{
  return xmole2::make_error(
      ErrorDomain::Io, static_cast<std::uint32_t>(code), message, severity);
}

} // namespace xmole2::io

