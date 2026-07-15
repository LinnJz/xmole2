#include "xmole2/base/error.hpp"

auto run_operation_context_contract_tests() -> bool;

auto main() -> int
{
  auto const error =
      xmole2::make_error(xmole2::ErrorDomain::Opc, 42, "invalid relationship");
  if (error.domain != xmole2::ErrorDomain::Opc || error.code != 42 ||
      error.severity != xmole2::Severity::Error ||
      error.message != "invalid relationship")
  {
    return 1;
  }

  return run_operation_context_contract_tests() ? 0 : 1;
}

