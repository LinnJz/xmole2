#include "file_io.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <charconv>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <limits>
#include <string>
#include <system_error>
#include <utility>

#include "xmole2/io/error.hpp"

#ifdef _WIN32
#  ifndef NOMINMAX
#    define NOMINMAX
#  endif
#  include <Windows.h>
#else
#  include <fcntl.h>
#  include <unistd.h>

#  include <cerrno>
#  include <sys/stat.h>
#  include <sys/types.h>
#endif

namespace xmole2::io::internal
{
namespace
{

constexpr auto kMaximumTemporaryFileAttempts = std::size_t { 128 };
using std::literals::string_view_literals::operator""sv;

auto is_cancelled(OperationContext const &context) -> bool
{
  return context.cancellation.is_cancelled();
}

auto cancelled_error() -> Error
{
  return make_error(IoErrorCode::Cancelled, "I/O operation cancelled");
}

auto resource_error(std::string_view const message) -> Error
{
  return make_error(IoErrorCode::ResourceLimitExceeded, message);
}

auto system_error(
    IoErrorCode const code,
    std::string_view const message,
    std::filesystem::path const &path,
    std::int64_t const native_code) -> Error
{
  auto error           = make_error(code, message);
  auto const path_text = path.generic_u8string();
  error.location       = DocumentLocation {
    LocationKind::FilePath,
    std::string { reinterpret_cast<char const *>(path_text.data()), path_text.size() },
    std::nullopt
  };
  error.native_code = native_code;
  return error;
}

auto validate_path(std::filesystem::path const &path, OperationContext const &context)
    -> Status
{
  if (path.empty())
  {
    return std::unexpected { make_error(
        IoErrorCode::InvalidArgument, "file path must not be empty") };
  }

  if (path.native().size() > context.budget.max_path_length)
  {
    return std::unexpected { resource_error("file path exceeds the resource budget") };
  }

  return {};
}

auto validate_source_size(std::uint64_t const size, OperationContext const &context)
    -> Status
{
  if (size > context.budget.max_input_bytes ||
      size > context.budget.max_single_resource_bytes)
  {
    return std::unexpected { resource_error("source size exceeds the resource budget") };
  }
  return {};
}

auto next_temporary_name() -> std::string
{
  static auto s_counter  = std::atomic_uint64_t {};
  auto const clock_value = static_cast<std::uint64_t>(
      std::chrono::steady_clock::now().time_since_epoch().count());
  auto const value = clock_value ^ s_counter.fetch_add(1, std::memory_order_relaxed);

  auto buffer            = std::array<char, 64> {};
  constexpr auto kPrefix = "xmole2-"sv;
  auto *output           = std::copy(kPrefix.begin(), kPrefix.end(), buffer.data());
  auto const converted = std::to_chars(output, buffer.data() + buffer.size(), value, 16);
  output               = converted.ptr;
  constexpr auto kSuffix = ".tmp"sv;
  output                 = std::copy(kSuffix.begin(), kSuffix.end(), output);
  return std::string(buffer.data(), output);
}

#ifdef _WIN32

using NativeHandle        = HANDLE;
auto const kInvalidHandle = INVALID_HANDLE_VALUE;

auto native_error_code() -> std::int64_t
{
  return static_cast<std::int64_t>(GetLastError());
}

auto close_native_handle(NativeHandle const handle) noexcept -> void
{
  if (handle != kInvalidHandle)
  {
    CloseHandle(handle);
  }
}

#else

using NativeHandle            = int;
constexpr auto kInvalidHandle = -1;

auto native_error_code() -> std::int64_t
{
  return static_cast<std::int64_t>(errno);
}

auto close_native_handle(NativeHandle const handle) noexcept -> void
{
  if (handle != kInvalidHandle)
  {
    ::close(handle);
  }
}

#endif

auto query_file_size(NativeHandle const handle, std::filesystem::path const &path)
    -> Result<std::uint64_t>
{
#ifdef _WIN32
  auto native_size = LARGE_INTEGER {};
  if (!GetFileSizeEx(handle, &native_size))
  {
    return std::unexpected { system_error(
        IoErrorCode::ReadFailed, "failed to query source size", path,
        native_error_code()) };
  }
  if (native_size.QuadPart < 0)
  {
    return std::unexpected { make_error(
        IoErrorCode::ReadFailed, "source reported a negative size") };
  }
  return static_cast<std::uint64_t>(native_size.QuadPart);
#else
  struct stat native_status {};
  if (::fstat(handle, &native_status) != 0)
  {
    return std::unexpected { system_error(
        IoErrorCode::ReadFailed, "failed to query source size", path,
        native_error_code()) };
  }
  if (native_status.st_size < 0)
  {
    return std::unexpected { make_error(
        IoErrorCode::ReadFailed, "source reported a negative size") };
  }
  return static_cast<std::uint64_t>(native_status.st_size);
#endif
}

class FileByteSource final : public ByteSource
{
public:
  FileByteSource(
      NativeHandle const handle,
      std::filesystem::path path,
      bool const remove_on_close,
      std::uint64_t const source_size)
      : m_handle { handle }
      , m_path { std::move(path) }
      , m_remove_on_close { remove_on_close }
      , m_source_size { source_size }
  {
  }

  ~FileByteSource() override
  {
    close_native_handle(m_handle);
    if (m_remove_on_close)
    {
      remove_file(m_path);
    }
  }

  auto size(OperationContext const &context) const -> Result<std::uint64_t> override
  {
    if (is_cancelled(context))
    {
      return std::unexpected { cancelled_error() };
    }

    auto const validation = validate_source_size(m_source_size, context);
    if (!validation)
    {
      return std::unexpected { validation.error() };
    }
    return m_source_size;
  }

  auto read_at(
      std::uint64_t const offset,
      std::span<std::byte> const destination,
      OperationContext const &context) const -> Result<std::size_t> override
  {
    if (is_cancelled(context))
    {
      return std::unexpected { cancelled_error() };
    }
    if (destination.size() > context.budget.max_single_resource_bytes)
    {
      return std::unexpected { resource_error(
          "read request exceeds the resource budget") };
    }
    if (destination.empty())
    {
      return std::size_t {};
    }

    auto const validation = validate_source_size(m_source_size, context);
    if (!validation)
    {
      return std::unexpected { validation.error() };
    }
    if (offset >= m_source_size)
    {
      return std::size_t {};
    }

    auto const available     = m_source_size - offset;
    auto const requested     = static_cast<std::uint64_t>(destination.size());
    auto const total_to_read = static_cast<std::size_t>(std::min(available, requested));
    auto total_read          = std::size_t {};

    while (total_read < total_to_read)
    {
      if (is_cancelled(context))
      {
        return std::unexpected { cancelled_error() };
      }

#ifdef _WIN32
      auto const remaining       = total_to_read - total_read;
      auto const chunk           = static_cast<DWORD>(std::min<std::size_t>(
          remaining, static_cast<std::size_t>(std::numeric_limits<DWORD>::max())));
      auto const absolute_offset = offset + total_read;
      auto overlapped            = OVERLAPPED {};
      overlapped.Offset          = static_cast<DWORD>(absolute_offset & 0xff'ff'ff'ffULL);
      overlapped.OffsetHigh      = static_cast<DWORD>(absolute_offset >> 32U);
      auto bytes_read            = DWORD {};
      auto completed             = ReadFile(
          m_handle, destination.data() + total_read, chunk, &bytes_read, &overlapped);
      if (!completed && GetLastError() == ERROR_IO_PENDING)
      {
        completed = GetOverlappedResult(m_handle, &overlapped, &bytes_read, TRUE);
      }
      if (!completed)
      {
        auto const native_code = GetLastError();
        if (native_code == ERROR_HANDLE_EOF)
        {
          break;
        }
        return std::unexpected { system_error(
            IoErrorCode::ReadFailed, "failed to read source", m_path,
            static_cast<std::int64_t>(native_code)) };
      }
      if (bytes_read == 0)
      {
        break;
      }
      total_read += bytes_read;
#else
      auto const remaining = total_to_read - total_read;
      auto const chunk     = std::min<std::size_t>(
          remaining, static_cast<std::size_t>(std::numeric_limits<ssize_t>::max()));
      auto const bytes_read = ::pread(
          m_handle, destination.data() + total_read, chunk,
          static_cast<off_t>(offset + total_read));
      if (bytes_read < 0)
      {
        if (errno == EINTR)
        {
          continue;
        }
        return std::unexpected { system_error(
            IoErrorCode::ReadFailed, "failed to read source", m_path,
            native_error_code()) };
      }
      if (bytes_read == 0)
      {
        break;
      }
      total_read += static_cast<std::size_t>(bytes_read);
#endif
    }

    return total_read;
  }

private:
  NativeHandle m_handle { kInvalidHandle };
  std::filesystem::path m_path;
  bool m_remove_on_close {};
  std::uint64_t m_source_size {};
};

class FileByteSink final : public ByteSink
{
public:
  FileByteSink(NativeHandle const handle, std::filesystem::path path)
      : m_handle { handle }
      , m_path { std::move(path) }
  {
  }

  ~FileByteSink() override
  {
    auto const ignored = close(OperationContext {});
    static_cast<void>(ignored);
  }

  auto write(std::span<std::byte const> const source, OperationContext const &context)
      -> Status override
  {
    if (m_handle == kInvalidHandle)
    {
      return std::unexpected { make_error(IoErrorCode::SinkClosed, "sink is closed") };
    }
    if (is_cancelled(context))
    {
      return std::unexpected { cancelled_error() };
    }
    if (source.size() > std::numeric_limits<std::uint64_t>::max() - m_bytes_written ||
        m_bytes_written + source.size() > context.budget.max_temporary_storage_bytes)
    {
      return std::unexpected { resource_error("temporary storage budget exceeded") };
    }

    auto total_written = std::size_t {};
    while (total_written < source.size())
    {
      if (is_cancelled(context))
      {
        return std::unexpected { cancelled_error() };
      }

#ifdef _WIN32
      auto const remaining = source.size() - total_written;
      auto const chunk     = static_cast<DWORD>(std::min<std::size_t>(
          remaining, static_cast<std::size_t>(std::numeric_limits<DWORD>::max())));
      auto written         = DWORD {};
      if (!WriteFile(m_handle, source.data() + total_written, chunk, &written, nullptr))
      {
        return std::unexpected { system_error(
            IoErrorCode::WriteFailed, "failed to write sink", m_path,
            native_error_code()) };
      }
      if (written == 0)
      {
        return std::unexpected { make_error(
            IoErrorCode::WriteFailed, "sink made no write progress") };
      }
      total_written += written;
      m_bytes_written += written;
#else
      auto const remaining = source.size() - total_written;
      auto const chunk     = std::min<std::size_t>(
          remaining, static_cast<std::size_t>(std::numeric_limits<ssize_t>::max()));
      auto const written = ::write(m_handle, source.data() + total_written, chunk);
      if (written < 0)
      {
        if (errno == EINTR)
        {
          continue;
        }
        return std::unexpected { system_error(
            IoErrorCode::WriteFailed, "failed to write sink", m_path,
            native_error_code()) };
      }
      if (written == 0)
      {
        return std::unexpected { make_error(
            IoErrorCode::WriteFailed, "sink made no write progress") };
      }
      total_written += static_cast<std::size_t>(written);
      m_bytes_written += static_cast<std::uint64_t>(written);
#endif
    }
    return {};
  }

  auto flush(OperationContext const &context) -> Status override
  {
    if (m_handle == kInvalidHandle)
    {
      return std::unexpected { make_error(IoErrorCode::SinkClosed, "sink is closed") };
    }
    if (is_cancelled(context))
    {
      return std::unexpected { cancelled_error() };
    }

#ifdef _WIN32
    if (!FlushFileBuffers(m_handle))
#else
    if (::fsync(m_handle) != 0)
#endif
    {
      return std::unexpected { system_error(
          IoErrorCode::FlushFailed, "failed to flush sink", m_path,
          native_error_code()) };
    }
    return {};
  }

  auto close(OperationContext const &context) -> Status override
  {
    if (m_handle == kInvalidHandle)
    {
      return {};
    }

    auto flush_result = flush(context);
#ifdef _WIN32
    auto const closed = CloseHandle(m_handle) != 0;
#else
    auto const closed = ::close(m_handle) == 0;
#endif
    auto const close_error = closed ? std::int64_t {} : native_error_code();
    m_handle               = kInvalidHandle;

    if (!flush_result)
    {
      return flush_result;
    }
    if (!closed)
    {
      return std::unexpected { system_error(
          IoErrorCode::CloseFailed, "failed to close sink", m_path, close_error) };
    }
    return {};
  }

  auto bytes_written() const noexcept -> std::uint64_t override
  {
    return m_bytes_written;
  }

private:
  NativeHandle m_handle { kInvalidHandle };
  std::filesystem::path m_path;
  std::uint64_t m_bytes_written {};
};

auto create_file_sink(std::filesystem::path const &path, OperationContext const &context)
    -> Result<std::unique_ptr<ByteSink>>
{
  auto const validation = validate_path(path, context);
  if (!validation)
  {
    return std::unexpected { validation.error() };
  }

#ifdef _WIN32
  auto const handle = CreateFileW(
      path.c_str(), GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_DELETE, nullptr,
      CREATE_NEW, FILE_ATTRIBUTE_TEMPORARY, nullptr);
#else
  auto const handle = ::open(path.c_str(), O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC, 0600);
#endif
  if (handle == kInvalidHandle)
  {
    return std::unexpected { system_error(
        IoErrorCode::TemporaryFileCreationFailed, "failed to create temporary file", path,
        native_error_code()) };
  }

  return std::unique_ptr<ByteSink> {
    new FileByteSink { handle, path }
  };
}

auto is_collision(Error const &error) -> bool
{
  if (!error.native_code)
  {
    return false;
  }
#ifdef _WIN32
  return *error.native_code == ERROR_FILE_EXISTS ||
       *error.native_code == ERROR_ALREADY_EXISTS;
#else
  return *error.native_code == EEXIST;
#endif
}

} // namespace

auto open_file_source(
    std::filesystem::path const &path,
    bool const remove_on_close,
    OperationContext const &context) -> Result<std::shared_ptr<ByteSource>>
{
  if (is_cancelled(context))
  {
    return std::unexpected { cancelled_error() };
  }
  auto const validation = validate_path(path, context);
  if (!validation)
  {
    return std::unexpected { validation.error() };
  }

#ifdef _WIN32
  // Positional overlapped reads avoid a shared mutable file pointer when one
  // ByteSource is read concurrently. The synchronous port waits for completion.
  auto const handle = CreateFileW(
      path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_DELETE, nullptr,
      OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED, nullptr);
#else
  auto const handle = ::open(path.c_str(), O_RDONLY | O_CLOEXEC);
#endif
  if (handle == kInvalidHandle)
  {
    return std::unexpected { system_error(
        IoErrorCode::OpenFailed, "failed to open byte source", path,
        native_error_code()) };
  }

  auto const source_size = query_file_size(handle, path);
  if (!source_size)
  {
    close_native_handle(handle);
    return std::unexpected { source_size.error() };
  }
  auto const size_validation = validate_source_size(*source_size, context);
  if (!size_validation)
  {
    close_native_handle(handle);
    return std::unexpected { size_validation.error() };
  }

  return std::shared_ptr<ByteSource> {
    new FileByteSource { handle, path, remove_on_close, *source_size }
  };
}

auto create_temporary_file(
    std::filesystem::path const &directory, OperationContext const &context)
    -> Result<NativeTemporaryFile>
{
  if (is_cancelled(context))
  {
    return std::unexpected { cancelled_error() };
  }
  auto const validation = validate_path(directory, context);
  if (!validation)
  {
    return std::unexpected { validation.error() };
  }

  auto filesystem_error = std::error_code {};
  auto const status     = std::filesystem::status(directory, filesystem_error);
  if (filesystem_error || !std::filesystem::is_directory(status))
  {
    return std::unexpected { system_error(
        IoErrorCode::TemporaryFileCreationFailed, "temporary directory is unavailable",
        directory, filesystem_error.value()) };
  }

  for (auto attempt = std::size_t {}; attempt < kMaximumTemporaryFileAttempts; ++attempt)
  {
    auto const path = directory / next_temporary_name();
    auto sink       = create_file_sink(path, context);
    if (sink)
    {
      return NativeTemporaryFile { path, std::move(*sink) };
    }
    if (!is_collision(sink.error()))
    {
      return std::unexpected { std::move(sink.error()) };
    }
  }

  return std::unexpected { make_error(
      IoErrorCode::TemporaryFileCreationFailed,
      "temporary file name attempts exhausted") };
}

auto replace_file(
    std::filesystem::path const &temporary_path, std::filesystem::path const &target_path)
    -> Status
{
#ifdef _WIN32
  auto replaced                     = false;
  auto const target_attributes      = GetFileAttributesW(target_path.c_str());
  auto const target_attribute_error = GetLastError();
  if (target_attributes != INVALID_FILE_ATTRIBUTES)
  {
    replaced =
        ReplaceFileW(
            target_path.c_str(), temporary_path.c_str(), nullptr,
            REPLACEFILE_WRITE_THROUGH, nullptr, nullptr) != 0;
  }
  else if (
      target_attribute_error == ERROR_FILE_NOT_FOUND ||
      target_attribute_error == ERROR_PATH_NOT_FOUND)
  {
    replaced =
        MoveFileExW(
            temporary_path.c_str(), target_path.c_str(), MOVEFILE_WRITE_THROUGH) != 0;
  }

  if (!replaced)
  {
    return std::unexpected { system_error(
        IoErrorCode::AtomicReplaceFailed, "failed to atomically replace target",
        target_path, native_error_code()) };
  }
#else
  if (::rename(temporary_path.c_str(), target_path.c_str()) != 0)
  {
    return std::unexpected { system_error(
        IoErrorCode::AtomicReplaceFailed, "failed to atomically replace target",
        target_path, native_error_code()) };
  }
#endif
  return {};
}

auto remove_file(std::filesystem::path const &path) noexcept -> void
{
  auto error = std::error_code {};
  std::filesystem::remove(path, error);
}

} // namespace xmole2::io::internal
