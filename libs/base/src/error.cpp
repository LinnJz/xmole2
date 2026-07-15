#include "xmole2/base/error.hpp"

namespace xmole2
{

auto make_error(
    ErrorDomain const domain,
    std::uint32_t const code,
    std::string_view const message,
    Severity const severity) -> Error
{
  return Error {
    .domain      = domain,
    .code        = code,
    .severity    = severity,
    .message     = std::string { message },
    .location    = std::nullopt,
    .cause       = nullptr,
    .native_code = std::nullopt,
  };
}

} // namespace xmole2
