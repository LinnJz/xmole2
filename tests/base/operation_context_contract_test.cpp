#include <cassert>

#include "xmole2/base/operation_context.hpp"

auto run_operation_context_contract_tests() -> void
{
  xmole2::CancellationSource source;
  auto const token = source.token();

  assert(!token.is_cancelled());
  source.request_cancellation();
  assert(token.is_cancelled());
}

