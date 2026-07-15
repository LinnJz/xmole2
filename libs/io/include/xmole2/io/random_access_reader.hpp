/// @file
/// Cursor-based reader over a random-access ByteSource.

#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>

#include "xmole2/base/operation_context.hpp"
#include "xmole2/io/export.hpp"

namespace xmole2::io
{

class ByteSource;
class SourceLease;

/// Move-only cursor. One instance is single-thread driven; create independent
/// readers from the same SourceLease for concurrent access.
class XMOLE2_IO_API RandomAccessReader
{
public:
  RandomAccessReader() noexcept;
  RandomAccessReader(RandomAccessReader const &)                      = delete;
  auto operator= (RandomAccessReader const &) -> RandomAccessReader & = delete;
  RandomAccessReader(RandomAccessReader &&other) noexcept;
  auto operator= (RandomAccessReader &&other) noexcept -> RandomAccessReader &;
  ~RandomAccessReader();

  [[nodiscard]] auto read(
      std::span<std::byte> destination, OperationContext const &context)
      -> Result<std::size_t>;
  [[nodiscard]] auto read_exact(
      std::span<std::byte> destination, OperationContext const &context) -> Status;
  [[nodiscard]] auto seek(std::uint64_t offset, OperationContext const &context)
      -> Status;
  [[nodiscard]] auto position() const noexcept -> std::uint64_t;
  [[nodiscard]] auto size(OperationContext const &context) const -> Result<std::uint64_t>;

private:
  struct Impl;

  explicit RandomAccessReader(
      std::shared_ptr<ByteSource> source,
      std::uint64_t source_size,
      std::uint64_t offset);

  std::unique_ptr<Impl> m_impl;

  friend class SourceLease;
};

} // namespace xmole2::io
