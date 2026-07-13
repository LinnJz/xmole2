#include <cassert>

#include "xmole2/base/error.hpp"

auto run_operation_context_contract_tests() -> void;

auto main() -> int
{
  auto const error =
      xmole2::make_error(xmole2::ErrorDomain::Opc, 42, "invalid relationship");
  assert(error.domain == xmole2::ErrorDomain::Opc);
  assert(error.code == 42);
  assert(error.message == "invalid relationship");

  run_operation_context_contract_tests();
}

