#include "xmole2/base/error.hpp"

auto main() -> int
{
  auto const error = xmole2::make_error(xmole2::ErrorDomain::Base, 1, "consumer");
  return error.message == "consumer" ? 0 : 1;
}

