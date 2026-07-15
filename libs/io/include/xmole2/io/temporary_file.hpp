/// @file
/// Budgeted temporary file storage.

#pragma once

#include <filesystem>
#include <memory>

#include "xmole2/base/operation_context.hpp"
#include "xmole2/io/byte_sink.hpp"
#include "xmole2/io/export.hpp"
#include "xmole2/io/source_lease.hpp"

namespace xmole2::io
{

/// Owns a temporary path and removes it unless ownership is transferred by seal().
class XMOLE2_IO_API TemporaryFile
{
public:
  TemporaryFile() noexcept;
  TemporaryFile(TemporaryFile const &)                      = delete;
  auto operator= (TemporaryFile const &) -> TemporaryFile & = delete;
  TemporaryFile(TemporaryFile &&other) noexcept;
  auto operator= (TemporaryFile &&other) noexcept -> TemporaryFile &;
  ~TemporaryFile();

  [[nodiscard]] static auto create(OperationContext const &context)
      -> Result<TemporaryFile>;
  [[nodiscard]] static auto create(
      std::filesystem::path const &directory, OperationContext const &context)
      -> Result<TemporaryFile>;

  [[nodiscard]] auto path() const -> std::filesystem::path const &;
  [[nodiscard]] auto sink() -> ByteSink &;

  /// Closes the sink and returns a lease that deletes the file when released.
  [[nodiscard]] auto seal(OperationContext const &context) && -> Result<SourceLease>;

private:
  struct Impl;

  explicit TemporaryFile(std::unique_ptr<Impl> impl);

  std::unique_ptr<Impl> m_impl;
};

} // namespace xmole2::io
