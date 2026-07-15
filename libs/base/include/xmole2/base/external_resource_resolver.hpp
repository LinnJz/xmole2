/// @file
/// Policy-controlled external resource resolution.

#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "xmole2/base/operation_context.hpp"

namespace xmole2
{

/// Describes one external resource lookup. Borrowed strings are valid only for
/// the duration of ExternalResourceResolver::resolve().
struct ExternalResourceRequest
{
  std::string_view normalized_uri;
  std::string_view resource_type;
  std::optional<DocumentLocation> source_location;
};

/// Owns a resolved external resource and redirect/content-type metadata.
/// An empty resolved_uri is normalized to the request URI; an empty media_type is unknown.
struct ExternalResource
{
  std::vector<std::byte> bytes;
  std::string resolved_uri;
  std::string media_type;
};

/// Caller-supplied synchronous policy boundary for all external access.
/// Instances shared by concurrent operations must make resolve() thread-safe.
class XMOLE2_BASE_API ExternalResourceResolver
{
public:
  virtual ~ExternalResourceResolver() = default;

  [[nodiscard]] virtual auto resolve(
      ExternalResourceRequest const &request, OperationContext const &context)
      -> Result<ExternalResource> = 0;
};

/// Resolves through the context's resolver, or denies access when none is supplied.
/// Successful responses are checked against input, single-resource, and memory limits.
[[nodiscard]] XMOLE2_BASE_API auto resolve_external_resource(
    ExternalResourceRequest const &request, OperationContext const &context)
    -> Result<ExternalResource>;

} // namespace xmole2
