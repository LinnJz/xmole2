/// @file
/// Streaming reader for one ZIP entry.

#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>

#include "xmole2/base/operation_context.hpp"
#include "xmole2/io/random_access_reader.hpp"
#include "xmole2/zip/export.hpp"
#include "xmole2/zip/zip_entry.hpp"

namespace xmole2::zip
{

class ZipArchive;

class XMOLE2_ZIP_API ZipEntryReader
{
public:
  ZipEntryReader() noexcept;
  ZipEntryReader(ZipEntryReader const &)                      = delete;
  auto operator= (ZipEntryReader const &) -> ZipEntryReader & = delete;
  ZipEntryReader(ZipEntryReader &&other) noexcept;
  auto operator= (ZipEntryReader &&other) noexcept -> ZipEntryReader &;
  ~ZipEntryReader();

  /// Reads decompressed bytes. Zero means EOF and successful integrity validation.
  [[nodiscard]] auto read(
      std::span<std::byte> destination, OperationContext const &context)
      -> Result<std::size_t>;

  /// Completes CRC/size validation after all declared bytes have been consumed.
  [[nodiscard]] auto finish(OperationContext const &context) -> Status;

  [[nodiscard]] auto position() const noexcept -> std::uint64_t;
  [[nodiscard]] auto size() const noexcept -> std::uint64_t;
  [[nodiscard]] auto finished() const noexcept -> bool;

private:
  struct Impl;

  explicit ZipEntryReader(std::unique_ptr<Impl> impl);
  [[nodiscard]] static auto open(
      io::RandomAccessReader source,
      ZipEntry entry,
      std::int64_t central_directory_offset,
      OperationContext const &context) -> Result<ZipEntryReader>;

  std::unique_ptr<Impl> m_impl;

  friend class ZipArchive;
};

} // namespace xmole2::zip
