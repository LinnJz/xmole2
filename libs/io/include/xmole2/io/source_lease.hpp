/// @file
/// Stable ownership of a byte source and its backing storage.

#pragma once

#include <cstdint>
#include <filesystem>
#include <memory>
#include <vector>

#include "xmole2/base/operation_context.hpp"
#include "xmole2/io/export.hpp"
#include "xmole2/io/random_access_reader.hpp"

namespace xmole2::io
{

class ByteSource;

/// Keeps the opened source identity alive even if its original path is replaced.
class XMOLE2_IO_API SourceLease
{
public:
  SourceLease() noexcept;
  SourceLease(SourceLease const &)                      = delete;
  auto operator= (SourceLease const &) -> SourceLease & = delete;
  SourceLease(SourceLease &&other) noexcept;
  auto operator= (SourceLease &&other) noexcept -> SourceLease &;
  ~SourceLease();

  [[nodiscard]] static auto open_file(
      std::filesystem::path const &path, OperationContext const &context)
      -> Result<SourceLease>;
  [[nodiscard]] static auto from_buffer(
      std::vector<std::byte> buffer, OperationContext const &context)
      -> Result<SourceLease>;
  [[nodiscard]] static auto acquire(
      std::unique_ptr<ByteSource> source, OperationContext const &context)
      -> Result<SourceLease>;
  [[nodiscard]] static auto acquire(
      std::shared_ptr<ByteSource> source, OperationContext const &context)
      -> Result<SourceLease>;

  [[nodiscard]] auto valid() const noexcept -> bool;
  [[nodiscard]] auto size(OperationContext const &context) const -> Result<std::uint64_t>;
  [[nodiscard]] auto reader(std::uint64_t offset, OperationContext const &context) const
      -> Result<RandomAccessReader>;

  /// Copies the source into owned temporary storage and releases this lease's original source.
  [[nodiscard]] auto detach(OperationContext const &context) -> Status;

private:
  struct Impl;

  explicit SourceLease(std::shared_ptr<ByteSource> source);

  std::unique_ptr<Impl> m_impl;
};

} // namespace xmole2::io
