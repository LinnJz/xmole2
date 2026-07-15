#include "xmole2/zip/error.hpp"

namespace xmole2::zip
{

auto make_error(
    ZipErrorCode const code, std::string_view const message, Severity const severity)
    -> Error
{
  return xmole2::make_error(
      ErrorDomain::Zip, static_cast<std::uint32_t>(code), message, severity);
}

} // namespace xmole2::zip
