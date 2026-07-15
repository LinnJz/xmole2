#include <utility>

#include "xmole2/base/operation_context.hpp"

auto run_operation_context_contract_tests() -> bool
{
  xmole2::CancellationSource source;
  auto const token = source.token();

  if (token.is_cancelled())
  {
    return false;
  }

  auto copied_token   = token;
  auto assigned_token = xmole2::CancellationToken {};
  assigned_token      = copied_token;
  auto moved_token    = xmole2::CancellationToken {};
  moved_token         = std::move(assigned_token);

  auto moved_source = std::move(source);
  if (moved_source.token().is_cancelled())
  {
    return false;
  }

  auto copied_source         = moved_source;
  auto assigned_source       = xmole2::CancellationSource {};
  assigned_source            = copied_source;
  auto moved_assigned_source = xmole2::CancellationSource {};
  moved_assigned_source      = std::move(assigned_source);

  moved_assigned_source.request_cancellation();
  if (!token.is_cancelled() || !copied_token.is_cancelled() ||
      !moved_token.is_cancelled() || !moved_source.token().is_cancelled())
  {
    return false;
  }

  source.request_cancellation();

  auto const context = xmole2::OperationContext {};
  return context.budget.max_input_bytes > 0 && context.budget.max_xml_attributes > 0 &&
         context.budget.max_cfb_sector_count > 0 &&
         context.budget.max_kdf_iterations > 0 && context.budget.max_image_pixels > 0 &&
         context.budget.max_formula_graph_node_count > 0 && context.progress == nullptr &&
         context.diagnostics == nullptr && context.external_resources == nullptr;
}
