/// @file
/// Shared resource, cancellation, progress, and diagnostic context.

#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string_view>
#include <vector>

#include "xmole2/base/error.hpp"
#include "xmole2/base/export.hpp"

namespace xmole2
{

class CancellationState;

struct ResourceBudget
{
  std::uint64_t max_input_bytes { 4ULL * 1024 * 1024 * 1024 };
  std::uint64_t max_expanded_bytes { 8ULL * 1024 * 1024 * 1024 };
  std::uint64_t max_single_resource_bytes { 1ULL * 1024 * 1024 * 1024 };
  std::size_t max_entry_count { 64 * 1024 };
  std::size_t max_xml_depth { 512 };
  std::size_t max_xml_nodes { 10'000'000 };
  std::size_t max_xml_attributes { 50'000'000 };
  std::size_t max_xml_attributes_per_element { 4'096 };
  std::uint64_t max_xml_text_bytes { 2ULL * 1024 * 1024 * 1024 };
  std::uint64_t max_cfb_sector_count { 16ULL * 1024 * 1024 };
  std::size_t max_cfb_directory_entry_count { 1'000'000 };
  std::uint64_t max_cfb_stream_chain_length { 16ULL * 1024 * 1024 };
  std::size_t max_relationship_count { 1'000'000 };
  std::size_t max_path_length { 64 * 1024 };
  std::size_t max_recursion_depth { 512 };
  std::size_t max_external_resource_count { 1'024 };
  std::uint64_t max_kdf_iterations { 10'000'000 };
  std::size_t max_password_attempt_count { 8 };
  std::uint64_t max_decrypted_bytes { 8ULL * 1024 * 1024 * 1024 };
  std::uint64_t max_image_pixels { 268'435'456 };
  std::size_t max_font_count { 4'096 };
  std::size_t max_embedded_object_count { 4'096 };
  std::uint64_t max_render_operation_count { 100'000'000 };
  std::uint64_t max_worksheet_row_count { 1'048'576 };
  std::uint64_t max_worksheet_column_count { 16'384 };
  std::uint64_t max_formula_graph_node_count { 10'000'000 };
  std::uint64_t max_calculation_step_count { 100'000'000 };
  std::uint64_t max_memory_bytes { 2ULL * 1024 * 1024 * 1024 };
  std::uint64_t max_temporary_storage_bytes { 16ULL * 1024 * 1024 * 1024 };
  std::size_t max_diagnostic_count { 10'000 };
};

class XMOLE2_BASE_API CancellationToken
{
public:
  CancellationToken() noexcept = default;
  CancellationToken(CancellationToken const &other) noexcept;
  auto operator= (CancellationToken const &other) noexcept -> CancellationToken &;
  CancellationToken(CancellationToken &&other) noexcept;
  auto operator= (CancellationToken &&other) noexcept -> CancellationToken &;
  ~CancellationToken();

  [[nodiscard]] auto is_cancelled() const noexcept -> bool;

private:
  explicit CancellationToken(CancellationState *state) noexcept;

  CancellationState *m_state {};

  friend class CancellationSource;
};

class XMOLE2_BASE_API CancellationSource
{
public:
  CancellationSource();
  CancellationSource(CancellationSource const &other) noexcept;
  auto operator= (CancellationSource const &other) noexcept -> CancellationSource &;
  CancellationSource(CancellationSource &&other) noexcept;
  auto operator= (CancellationSource &&other) noexcept -> CancellationSource &;
  ~CancellationSource();

  [[nodiscard]] auto token() const noexcept -> CancellationToken;
  auto request_cancellation() noexcept -> void;

private:
  CancellationState *m_state {};
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

/// Stores diagnostics for post-operation inspection without losing Error metadata.
class XMOLE2_BASE_API CollectingDiagnosticSink final : public DiagnosticSink
{
public:
  CollectingDiagnosticSink();
  CollectingDiagnosticSink(CollectingDiagnosticSink const &) = delete;
  auto operator= (CollectingDiagnosticSink const &)
      -> CollectingDiagnosticSink & = delete;
  CollectingDiagnosticSink(CollectingDiagnosticSink &&other);
  auto operator= (CollectingDiagnosticSink &&other) -> CollectingDiagnosticSink &;
  ~CollectingDiagnosticSink() override;

  auto report(Error const &diagnostic) -> void override;

  /// Returns a stable copy of the diagnostics collected so far.
  [[nodiscard]] auto snapshot() const -> std::vector<Error>;

  /// Atomically removes and returns the diagnostics collected so far.
  [[nodiscard]] auto take() -> std::vector<Error>;

  auto clear() -> void;
  [[nodiscard]] auto size() const -> std::size_t;
  [[nodiscard]] auto empty() const -> bool;

private:
  struct Impl;

  std::unique_ptr<Impl> m_impl;
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
