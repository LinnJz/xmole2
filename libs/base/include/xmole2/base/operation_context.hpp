/// @file
/// Shared resource, cancellation, progress, and diagnostic context.

#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string_view>

#include "xmole2/base/error.hpp"
#include "xmole2/base/export.hpp"

namespace xmole2
{

struct ResourceBudget
{
  std::uint64_t max_input_bytes { 4ULL * 1024 * 1024 * 1024 };
  std::uint64_t max_expanded_bytes { 8ULL * 1024 * 1024 * 1024 };
  std::uint64_t max_single_resource_bytes { 1ULL * 1024 * 1024 * 1024 };
  std::size_t max_entry_count { 64 * 1024 };
  std::size_t max_xml_depth { 512 };
  std::size_t max_xml_nodes { 10'000'000 };
  std::size_t max_relationship_count { 1'000'000 };
  std::uint64_t max_memory_bytes { 2ULL * 1024 * 1024 * 1024 };
  std::uint64_t max_temporary_storage_bytes { 16ULL * 1024 * 1024 * 1024 };
  std::size_t max_diagnostic_count { 10'000 };
};

class XMOLE2_BASE_API CancellationToken
{
public:
  CancellationToken() noexcept = default;

  [[nodiscard]] auto is_cancelled() const noexcept -> bool;

private:
  explicit CancellationToken(std::shared_ptr<std::atomic_bool const> state) noexcept;

  std::shared_ptr<std::atomic_bool const> m_state;

  friend class CancellationSource;
};

class XMOLE2_BASE_API CancellationSource
{
public:
  CancellationSource();

  [[nodiscard]] auto token() const noexcept -> CancellationToken;
  auto request_cancellation() noexcept -> void;

private:
  std::shared_ptr<std::atomic_bool> m_state;
};

struct ProgressUpdate
{
  std::string_view phase;
  std::uint64_t completed {};
  std::optional<std::uint64_t> total;
};

class ProgressSink
{
public:
  virtual ~ProgressSink()                                   = default;
  virtual auto report(ProgressUpdate const &update) -> void = 0;
};

class DiagnosticSink
{
public:
  virtual ~DiagnosticSink()                            = default;
  virtual auto report(Error const &diagnostic) -> void = 0;
};

class ExternalResourceResolver;

struct OperationContext
{
  ResourceBudget budget;
  CancellationToken cancellation;
  ProgressSink *progress {};
  DiagnosticSink *diagnostics {};
  ExternalResourceResolver *external_resources {};
};

} // namespace xmole2
