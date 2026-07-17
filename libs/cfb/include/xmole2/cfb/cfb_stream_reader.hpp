/// @file
/// Lazy sequential reader for CFB regular and mini streams.

#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>

#include "xmole2/base/operation_context.hpp"
#include "xmole2/cfb/export.hpp"
#include "xmole2/io/source_lease.hpp"

namespace xmole2::cfb
{

enum class CfbStreamStorage : std::uint8_t
{
  Regular,
  Mini,
};

/// Move-only forward reader that retains the stable source independently of its lease.
class XMOLE2_CFB_API CfbStreamReader
{
public:
  CfbStreamReader() noexcept;
  CfbStreamReader(CfbStreamReader const &)                      = delete;
  auto operator= (CfbStreamReader const &) -> CfbStreamReader & = delete;
  CfbStreamReader(CfbStreamReader &&other) noexcept;
  auto operator= (CfbStreamReader &&other) noexcept -> CfbStreamReader &;
  ~CfbStreamReader();

  /// Validates allocation metadata without reading the selected stream payload.
  [[nodiscard]] static auto open(
      io::SourceLease const &source,
      std::uint32_t directory_entry_id,
      OperationContext const &context) -> Result<CfbStreamReader>;

  /// Reads at most destination.size() declared stream bytes. Zero means EOF.
  [[nodiscard]] auto read(
      std::span<std::byte> destination, OperationContext const &context)
      -> Result<std::size_t>;

  [[nodiscard]] auto position() const noexcept -> std::uint64_t;
  [[nodiscard]] auto size() const noexcept -> std::uint64_t;
  [[nodiscard]] auto storage() const noexcept -> CfbStreamStorage;
  [[nodiscard]] auto finished() const noexcept -> bool;

private:
  struct Impl;

  explicit CfbStreamReader(std::unique_ptr<Impl> impl);

  std::unique_ptr<Impl> m_impl;
};

} // namespace xmole2::cfb
