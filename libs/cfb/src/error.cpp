#include "xmole2/cfb/error.hpp"

namespace xmole2::cfb
{

auto make_error(
    CfbErrorCode const code, std::string_view const message, Severity const severity)
    -> Error
{
  return xmole2::make_error(
      ErrorDomain::Cfb, static_cast<std::uint32_t>(code), message, severity);
}

} // namespace xmole2::cfb
