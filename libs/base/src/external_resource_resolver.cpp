#include "xmole2/base/external_resource_resolver.hpp"

#include <cstdint>
#include <limits>
#include <string_view>
#include <utility>

namespace xmole2
{
namespace
{

auto make_base_error(BaseErrorCode const code, std::string_view const message) -> Error
{
  return make_error(ErrorDomain::Base, static_cast<std::uint32_t>(code), message);
}

auto with_request_location(Error error, ExternalResourceRequest const &request) -> Error
{
  error.location = request.source_location;
  return error;
}

} // namespace

auto resolve_external_resource(
    ExternalResourceRequest const &request, OperationContext const &context)
    -> Result<ExternalResource>
{
  if (request.normalized_uri.empty() || request.resource_type.empty())
  {
    return std::unexpected { with_request_location(
        make_base_error(
            BaseErrorCode::InvalidArgument,
            "external resource URI and type must not be empty"),
        request) };
  }
  if (context.cancellation.is_cancelled())
  {
    return std::unexpected { with_request_location(
        make_base_error(
            BaseErrorCode::Cancelled, "external resource resolution cancelled"),
        request) };
  }
  if (context.external_resources == nullptr)
  {
    return std::unexpected { with_request_location(
        make_base_error(
            BaseErrorCode::ExternalAccessDenied, "external resource access denied"),
        request) };
  }

  auto resource = context.external_resources->resolve(request, context);
  if (!resource)
  {
    return std::unexpected { std::move(resource.error()) };
  }
  if (context.cancellation.is_cancelled())
  {
    return std::unexpected { with_request_location(
        make_base_error(
            BaseErrorCode::Cancelled, "external resource resolution cancelled"),
        request) };
  }
  if (resource->bytes.size() > std::numeric_limits<std::uint64_t>::max())
  {
    return std::unexpected { with_request_location(
        make_base_error(
            BaseErrorCode::ResourceLimitExceeded,
            "external resource size cannot be represented by the resource budget"),
        request) };
  }

  auto const size = static_cast<std::uint64_t>(resource->bytes.size());
  if (size > context.budget.max_input_bytes ||
      size > context.budget.max_single_resource_bytes ||
      size > context.budget.max_memory_bytes)
  {
    return std::unexpected { with_request_location(
        make_base_error(
            BaseErrorCode::ResourceLimitExceeded,
            "external resource exceeds the resource budget"),
        request) };
  }
  if (resource->resolved_uri.empty())
  {
    resource->resolved_uri = request.normalized_uri;
  }
  return resource;
}

} // namespace xmole2
