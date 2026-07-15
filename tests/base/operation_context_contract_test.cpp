#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <thread>
#include <utility>
#include <vector>

#include "xmole2/base/external_resource_resolver.hpp"
#include "xmole2/base/operation_context.hpp"

namespace
{

class FixedExternalResourceResolver final : public xmole2::ExternalResourceResolver
{
public:
  auto resolve(
      xmole2::ExternalResourceRequest const &request, xmole2::OperationContext const &)
      -> xmole2::Result<xmole2::ExternalResource> override
  {
    ++m_call_count;
    if (request.normalized_uri != "https://example.invalid/image.png" ||
        request.resource_type != "image")
    {
      return std::unexpected { xmole2::make_error(
          xmole2::ErrorDomain::Base, 999, "unexpected external resource request") };
    }
    return xmole2::ExternalResource {
      .bytes = { std::byte { 1 }, std::byte { 2 }, std::byte { 3 }, std::byte { 4 } },
      .resolved_uri = {},
      .media_type   = "image/png",
    };
  }

  [[nodiscard]] auto call_count() const noexcept -> std::size_t { return m_call_count; }

private:
  std::size_t m_call_count {};
};

class FaultExternalResourceResolver final : public xmole2::ExternalResourceResolver
{
public:
  auto resolve(xmole2::ExternalResourceRequest const &, xmole2::OperationContext const &)
      -> xmole2::Result<xmole2::ExternalResource> override
  {
    auto cause        = xmole2::make_error(xmole2::ErrorDomain::Io, 77, "network error");
    cause.native_code = 1234;
    auto error  = xmole2::make_error(xmole2::ErrorDomain::Base, 88, "resolve failed");
    error.cause = std::make_shared<xmole2::Error const>(std::move(cause));
    return std::unexpected { std::move(error) };
  }
};

auto test_cancellation_publication() -> bool
{
  constexpr auto kPublishedValue = std::uint64_t { 0x12'34'56'78'9a'bc'de'f0 };
  auto source                    = xmole2::CancellationSource {};
  auto const token               = source.token();
  auto published_value           = std::uint64_t {};

  auto publisher = std::jthread { [&source, &published_value]
  {
    published_value = kPublishedValue;
    source.request_cancellation();
  } };
  while (!token.is_cancelled())
  {
    std::this_thread::yield();
  }
  publisher.join();
  return published_value == kPublishedValue;
}

auto test_external_resource_resolver() -> bool
{
  auto const request = xmole2::ExternalResourceRequest {
    .normalized_uri = "https://example.invalid/image.png",
    .resource_type  = "image",
    .source_location =
        xmole2::DocumentLocation {
                                  .kind      = xmole2::LocationKind::PackagePart,
                                  .primary   = "/word/document.xml",
                                  .secondary = std::nullopt,
                                  },
  };

  auto context = xmole2::OperationContext {};
  auto denied  = xmole2::resolve_external_resource(request, context);
  if (denied ||
      denied.error().code !=
          static_cast<std::uint32_t>(xmole2::BaseErrorCode::ExternalAccessDenied) ||
      !denied.error().location ||
      denied.error().location->primary != "/word/document.xml")
  {
    return false;
  }

  auto resolver              = FixedExternalResourceResolver {};
  context.external_resources = &resolver;
  auto resolved              = xmole2::resolve_external_resource(request, context);
  if (!resolved || resolved->bytes.size() != 4 ||
      resolved->resolved_uri != request.normalized_uri ||
      resolved->media_type != "image/png" || resolver.call_count() != 1)
  {
    return false;
  }

  auto limited_context                    = context;
  limited_context.budget.max_memory_bytes = 3;
  auto limited = xmole2::resolve_external_resource(request, limited_context);
  if (limited ||
      limited.error().code !=
          static_cast<std::uint32_t>(xmole2::BaseErrorCode::ResourceLimitExceeded))
  {
    return false;
  }

  auto cancellation              = xmole2::CancellationSource {};
  auto cancelled_context         = context;
  cancelled_context.cancellation = cancellation.token();
  cancellation.request_cancellation();
  auto cancelled = xmole2::resolve_external_resource(request, cancelled_context);
  if (cancelled ||
      cancelled.error().code !=
          static_cast<std::uint32_t>(xmole2::BaseErrorCode::Cancelled) ||
      resolver.call_count() != 2)
  {
    return false;
  }

  auto fault_resolver        = FaultExternalResourceResolver {};
  context.external_resources = &fault_resolver;
  auto fault                 = xmole2::resolve_external_resource(request, context);
  return !fault && fault.error().cause != nullptr &&
         fault.error().cause->native_code == 1234;
}

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
  return test_collecting_diagnostic_sink() && test_cancellation_publication() &&
         test_external_resource_resolver() && context.budget.max_input_bytes > 0 &&
         context.budget.max_xml_attributes > 0 &&
         context.budget.max_cfb_sector_count > 0 &&
         context.budget.max_kdf_iterations > 0 && context.budget.max_image_pixels > 0 &&
         context.budget.max_formula_graph_node_count > 0 && context.progress == nullptr &&
         context.diagnostics == nullptr && context.external_resources == nullptr;
}
