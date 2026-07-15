#pragma once

#include <filesystem>
#include <memory>

#include "xmole2/io/byte_sink.hpp"
#include "xmole2/io/byte_source.hpp"

namespace xmole2::io::internal
{

struct NativeTemporaryFile
{
  std::filesystem::path path;
  std::unique_ptr<ByteSink> sink;
};

[[nodiscard]] auto open_file_source(
    std::filesystem::path const &path,
    bool remove_on_close,
    OperationContext const &context) -> Result<std::shared_ptr<ByteSource>>;

[[nodiscard]] auto create_temporary_file(
    std::filesystem::path const &directory, OperationContext const &context)
    -> Result<NativeTemporaryFile>;

[[nodiscard]] auto replace_file(
    std::filesystem::path const &temporary_path, std::filesystem::path const &target_path)
    -> Status;

auto remove_file(std::filesystem::path const &path) noexcept -> void;

} // namespace xmole2::io::internal

