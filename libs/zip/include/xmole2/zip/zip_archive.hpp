/// @file
/// Immutable ZIP entry index over a stable SourceLease.

#pragma once

#include <cstddef>
#include <memory>
#include <optional>
#include <string_view>

#include "xmole2/base/operation_context.hpp"
#include "xmole2/io/source_lease.hpp"
#include "xmole2/zip/export.hpp"
#include "xmole2/zip/zip_entry.hpp"
#include "xmole2/zip/zip_entry_reader.hpp"

namespace xmole2::zip
{

class XMOLE2_ZIP_API ZipArchive
{
public:
  ZipArchive() noexcept;
  ZipArchive(ZipArchive const &)                      = delete;
  auto operator= (ZipArchive const &) -> ZipArchive & = delete;
  ZipArchive(ZipArchive &&other) noexcept;
  auto operator= (ZipArchive &&other) noexcept -> ZipArchive &;
  ~ZipArchive();

  [[nodiscard]] static auto open(io::SourceLease source, OperationContext const &context)
      -> Result<ZipArchive>;

  [[nodiscard]] auto entry_count() const noexcept -> std::size_t;
  [[nodiscard]] auto entry_at(std::size_t index) const -> Result<ZipEntry>;
  [[nodiscard]] auto find_entry(std::string_view name) const
      -> Result<std::optional<ZipEntry>>;
  [[nodiscard]] auto open_entry(std::size_t index, OperationContext const &context) const
      -> Result<ZipEntryReader>;
  [[nodiscard]] auto open_entry(std::string_view name, OperationContext const &context)
      const -> Result<ZipEntryReader>;

private:
  struct Impl;

  explicit ZipArchive(std::unique_ptr<Impl> impl);

  std::unique_ptr<Impl> m_impl;
};

} // namespace xmole2::zip
