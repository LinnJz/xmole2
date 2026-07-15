#include <cstddef>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "xmole2/io/atomic_file_writer.hpp"
#include "xmole2/io/error.hpp"
#include "xmole2/io/source_lease.hpp"
#include "xmole2/io/temporary_file.hpp"

namespace
{

class ScopedTestDirectory
{
public:
  ScopedTestDirectory()
      : m_path { std::filesystem::temp_directory_path() / "xmole2-io-contract" }
  {
    auto error = std::error_code {};
    std::filesystem::remove_all(m_path, error);
    std::filesystem::create_directories(m_path);
  }

  ~ScopedTestDirectory()
  {
    auto error = std::error_code {};
    std::filesystem::remove_all(m_path, error);
  }

  [[nodiscard]] auto path() const -> std::filesystem::path const & { return m_path; }

private:
  std::filesystem::path m_path;
};

auto make_bytes(std::string_view const text) -> std::vector<std::byte>
{
  auto bytes = std::vector<std::byte>(text.size());
  for (std::size_t index = 0; index < text.size(); ++index)
  {
    bytes[index] = static_cast<std::byte>(text[index]);
  }
  return bytes;
}

auto write_text(std::filesystem::path const &path, std::string_view const text) -> bool
{
  auto stream = std::ofstream { path, std::ios::binary | std::ios::trunc };
  stream.write(text.data(), static_cast<std::streamsize>(text.size()));
  return stream.good();
}

auto read_text(std::filesystem::path const &path) -> std::string
{
  auto stream = std::ifstream { path, std::ios::binary };
  return std::string(
      std::istreambuf_iterator<char> { stream }, std::istreambuf_iterator<char> {});
}

auto read_all(
    xmole2::io::SourceLease const &lease, xmole2::OperationContext const &context)
    -> xmole2::Result<std::string>
{
  auto size = lease.size(context);
  if (!size)
  {
    return std::unexpected { std::move(size.error()) };
  }

  auto reader = lease.reader(0, context);
  if (!reader)
  {
    return std::unexpected { std::move(reader.error()) };
  }

  auto bytes  = std::vector<std::byte>(static_cast<std::size_t>(*size));
  auto status = reader->read_exact(bytes, context);
  if (!status)
  {
    return std::unexpected { std::move(status.error()) };
  }

  auto text = std::string(bytes.size(), '\0');
  for (std::size_t index = 0; index < bytes.size(); ++index)
  {
    text[index] = static_cast<char>(bytes[index]);
  }
  return text;
}

auto test_file_source_lease_and_detach(std::filesystem::path const &directory) -> bool
{
  auto const source_path = directory / "source.bin";
  auto const moved_path  = directory / "source-moved.bin";
  auto const context     = xmole2::OperationContext {};

  auto missing = xmole2::io::SourceLease::open_file(directory / "missing.bin", context);
  if (missing || !missing.error().native_code || !missing.error().location ||
      missing.error().location->kind != xmole2::LocationKind::FilePath)
  {
    return false;
  }

  if (!write_text(source_path, "original"))
  {
    return false;
  }

  auto lease_result = xmole2::io::SourceLease::open_file(source_path, context);
  if (!lease_result)
  {
    return false;
  }

  auto lease = std::move(*lease_result);
  std::filesystem::rename(source_path, moved_path);
  if (!write_text(source_path, "replacement"))
  {
    return false;
  }

  auto stable_text = read_all(lease, context);
  if (!stable_text || *stable_text != "original" || !lease.detach(context))
  {
    return false;
  }

  auto error = std::error_code {};
  std::filesystem::remove(moved_path, error);
  if (error)
  {
    return false;
  }

  auto detached_text = read_all(lease, context);
  return detached_text && *detached_text == "original";
}

auto test_temporary_file(std::filesystem::path const &directory) -> bool
{
  auto const context = xmole2::OperationContext {};
  auto temporary     = xmole2::io::TemporaryFile::create(directory, context);
  if (!temporary)
  {
    return false;
  }

  auto const temporary_path = temporary->path();
  auto const payload        = make_bytes("temporary payload");
  if (!temporary->sink().write(payload, context))
  {
    return false;
  }

  {
    auto source = std::move(*temporary).seal(context);
    if (!source || !std::filesystem::exists(temporary_path))
    {
      return false;
    }

    auto text = read_all(*source, context);
    if (!text || *text != "temporary payload")
    {
      return false;
    }
  }

  if (std::filesystem::exists(temporary_path))
  {
    return false;
  }

  auto limited_context                               = xmole2::OperationContext {};
  limited_context.budget.max_temporary_storage_bytes = 3;
  auto limited = xmole2::io::TemporaryFile::create(directory, limited_context);
  if (!limited)
  {
    return false;
  }

  auto result = limited->sink().write(make_bytes("four"), limited_context);
  return !result &&
         result.error().code ==
             static_cast<std::uint32_t>(xmole2::io::IoErrorCode::ResourceLimitExceeded);
}

auto test_atomic_file_writer(std::filesystem::path const &directory) -> bool
{
  auto const target = directory / "atomic-target.bin";
  if (!write_text(target, "old"))
  {
    return false;
  }

  auto const context   = xmole2::OperationContext {};
  auto original_source = xmole2::io::SourceLease::open_file(target, context);
  if (!original_source)
  {
    return false;
  }

  auto writer = xmole2::io::AtomicFileWriter::create(target, context);
  if (!writer)
  {
    return false;
  }

  auto const temporary_path = writer->temporary_path();
  if (!writer->sink().write(make_bytes("new"), context))
  {
    std::fputs("atomic write failed\n", stderr);
    return false;
  }
  if (read_text(target) != "old")
  {
    std::fputs("atomic target changed before commit\n", stderr);
    return false;
  }
  auto commit = writer->commit(context);
  if (!commit)
  {
    std::fputs("atomic commit failed\n", stderr);
    return false;
  }
  if (read_text(target) != "new")
  {
    std::fputs("atomic target content mismatch\n", stderr);
    return false;
  }
  if (std::filesystem::exists(temporary_path))
  {
    std::fputs("atomic temporary path remained\n", stderr);
    return false;
  }

  auto original_text = read_all(*original_source, context);
  if (!original_text || *original_text != "old")
  {
    std::fputs("atomic source lease did not retain original bytes\n", stderr);
    return false;
  }

  auto const new_target = directory / "atomic-new-target.bin";
  auto new_writer       = xmole2::io::AtomicFileWriter::create(new_target, context);
  if (!new_writer || !new_writer->sink().write(make_bytes("created"), context) ||
      !new_writer->commit(context) || read_text(new_target) != "created")
  {
    std::fputs("atomic new target creation failed\n", stderr);
    return false;
  }

  auto cancellation              = xmole2::CancellationSource {};
  auto cancelled_context         = xmole2::OperationContext {};
  cancelled_context.cancellation = cancellation.token();

  auto cancelled_writer = xmole2::io::AtomicFileWriter::create(target, cancelled_context);
  if (!cancelled_writer ||
      !cancelled_writer->sink().write(make_bytes("discarded"), cancelled_context))
  {
    std::fputs("cancelled atomic writer setup failed\n", stderr);
    return false;
  }

  cancellation.request_cancellation();
  auto result = cancelled_writer->commit(cancelled_context);
  if (result ||
      result.error().code !=
          static_cast<std::uint32_t>(xmole2::io::IoErrorCode::Cancelled))
  {
    std::fputs("cancelled atomic writer returned wrong result\n", stderr);
    return false;
  }
  if (read_text(target) != "new")
  {
    std::fputs("cancelled atomic writer changed target\n", stderr);
    return false;
  }
  return true;
}

} // namespace

auto run_storage_contract_tests() -> bool
{
  auto const directory = ScopedTestDirectory {};
  if (!test_file_source_lease_and_detach(directory.path()))
  {
    std::fputs("file source/detach contract failed\n", stderr);
    return false;
  }
  if (!test_temporary_file(directory.path()))
  {
    std::fputs("temporary file contract failed\n", stderr);
    return false;
  }
  if (!test_atomic_file_writer(directory.path()))
  {
    std::fputs("atomic writer contract failed\n", stderr);
    return false;
  }
  return true;
}
