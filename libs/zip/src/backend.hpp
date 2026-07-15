#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <string>

#include "xmole2/base/operation_context.hpp"
#include "xmole2/io/random_access_reader.hpp"

namespace xmole2::zip::internal
{

struct BackendEntry
{
  std::int64_t central_directory_offset {};
  std::string name;
  std::uint64_t compressed_size {};
  std::uint64_t uncompressed_size {};
  std::uint32_t crc32 {};
  std::uint16_t compression_method {};
  bool encrypted {};
  bool directory {};
};

struct BackendCloseInfo
{
  std::uint32_t crc32 {};
  std::uint64_t compressed_size {};
  std::uint64_t uncompressed_size {};
};

class BackendArchive
{
public:
  BackendArchive(BackendArchive const &)                      = delete;
  auto operator= (BackendArchive const &) -> BackendArchive & = delete;
  BackendArchive(BackendArchive &&)                           = delete;
  auto operator= (BackendArchive &&) -> BackendArchive &      = delete;
  ~BackendArchive();

  [[nodiscard]] static auto open(
      io::RandomAccessReader source, OperationContext const &context)
      -> Result<std::unique_ptr<BackendArchive>>;

  [[nodiscard]] auto reported_entry_count(OperationContext const &context)
      -> Result<std::uint64_t>;
  [[nodiscard]] auto goto_first_entry(OperationContext const &context) -> Result<bool>;
  [[nodiscard]] auto goto_next_entry(OperationContext const &context) -> Result<bool>;
  [[nodiscard]] auto goto_entry(
      std::int64_t central_directory_offset, OperationContext const &context) -> Status;
  [[nodiscard]] auto current_entry(OperationContext const &context)
      -> Result<BackendEntry>;
  [[nodiscard]] auto open_current_entry(OperationContext const &context) -> Status;
  [[nodiscard]] auto read_entry(
      std::span<std::byte> destination, OperationContext const &context)
      -> Result<std::size_t>;
  [[nodiscard]] auto close_entry(OperationContext const &context)
      -> Result<BackendCloseInfo>;

private:
  struct Impl;

  explicit BackendArchive(std::unique_ptr<Impl> impl);

  std::unique_ptr<Impl> m_impl;
};

} // namespace xmole2::zip::internal
