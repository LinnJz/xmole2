#include <cstddef>
#include <cstdint>
#include <memory>
#include <thread>
#include <utility>
#include <vector>

#include "xmole2/base/operation_context.hpp"

namespace
{

auto test_collecting_diagnostic_sink() -> bool
{
  auto collector = xmole2::CollectingDiagnosticSink {};

  auto cause        = xmole2::make_error(xmole2::ErrorDomain::Io, 71, "read failed");
  cause.native_code = 1234;
  auto diagnostic   = xmole2::make_error(
      xmole2::ErrorDomain::Opc, 91, "relationship was ignored",
      xmole2::Severity::Warning);
  diagnostic.location = xmole2::DocumentLocation {
    .kind      = xmole2::LocationKind::PackagePart,
    .primary   = "/word/document.xml",
    .secondary = std::nullopt,
  };
  diagnostic.cause = std::make_shared<xmole2::Error const>(std::move(cause));
  collector.report(diagnostic);

  auto snapshot = collector.snapshot();
  if (snapshot.size() != 1 || snapshot[0].location == std::nullopt ||
      snapshot[0].location->primary != "/word/document.xml" ||
      snapshot[0].cause == nullptr || snapshot[0].cause->native_code != 1234)
  {
    return false;
  }

  auto moved = std::move(collector);
  if (!collector.empty() || moved.size() != 1)
  {
    return false;
  }

  constexpr auto kThreadCount      = std::size_t { 4 };
  constexpr auto kReportsPerThread = std::size_t { 64 };
  auto workers                     = std::vector<std::jthread> {};
  workers.reserve(kThreadCount);
  for (auto thread_index = std::size_t {}; thread_index < kThreadCount; ++thread_index)
  {
    workers.emplace_back([&moved, thread_index]
    {
      for (auto report_index = std::size_t {}; report_index < kReportsPerThread;
           ++report_index)
      {
        moved.report(
            xmole2::make_error(
                xmole2::ErrorDomain::Base,
                static_cast<std::uint32_t>(
                    (thread_index * kReportsPerThread) + report_index),
                "collected diagnostic", xmole2::Severity::Information));
      }
    });
  }
  workers.clear();

  auto taken = moved.take();
  if (taken.size() != 1 + (kThreadCount * kReportsPerThread) || !moved.empty())
  {
    return false;
  }

  moved.report(diagnostic);
  moved.clear();
  return moved.empty() && moved.snapshot().empty();
}

} // namespace

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
  return test_collecting_diagnostic_sink() && context.budget.max_input_bytes > 0 &&
         context.budget.max_xml_attributes > 0 &&
         context.budget.max_cfb_sector_count > 0 &&
         context.budget.max_kdf_iterations > 0 && context.budget.max_image_pixels > 0 &&
         context.budget.max_formula_graph_node_count > 0 && context.progress == nullptr &&
         context.diagnostics == nullptr && context.external_resources == nullptr;
}
