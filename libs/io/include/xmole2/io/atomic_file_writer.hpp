/// @file
/// Atomic same-directory file replacement.

#pragma once

#include <filesystem>
#include <memory>

#include "xmole2/base/operation_context.hpp"
#include "xmole2/io/byte_sink.hpp"
#include "xmole2/io/export.hpp"

namespace xmole2::io
{

/// Writes to a same-directory temporary file and replaces the target only on commit().
class XMOLE2_IO_API AtomicFileWriter
{
public:
  AtomicFileWriter() noexcept;
  AtomicFileWriter(AtomicFileWriter const &)                      = delete;
  auto operator= (AtomicFileWriter const &) -> AtomicFileWriter & = delete;
  AtomicFileWriter(AtomicFileWriter &&other) noexcept;
  auto operator= (AtomicFileWriter &&other) noexcept -> AtomicFileWriter &;
  ~AtomicFileWriter();

  [[nodiscard]] static auto create(
      std::filesystem::path const &target, OperationContext const &context)
      -> Result<AtomicFileWriter>;

  [[nodiscard]] auto sink() -> ByteSink &;
  [[nodiscard]] auto temporary_path() const -> std::filesystem::path const &;

  /// Flushes and closes the temporary file before atomically replacing the target.
  [[nodiscard]] auto commit(OperationContext const &context) -> Status;

private:
  struct Impl;

  explicit AtomicFileWriter(std::unique_ptr<Impl> impl);

  std::unique_ptr<Impl> m_impl;
};

} // namespace xmole2::io
