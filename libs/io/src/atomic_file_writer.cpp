#include "xmole2/io/atomic_file_writer.hpp"

#include <filesystem>
#include <memory>
#include <utility>

#include "file_io.hpp"
#include "xmole2/io/byte_sink.hpp"
#include "xmole2/io/error.hpp"

namespace xmole2::io
{

struct AtomicFileWriter::Impl
{
  std::filesystem::path target_path;
  std::filesystem::path temporary_path;
  std::unique_ptr<ByteSink> sink;
  bool committed {};

  ~Impl()
  {
    if (sink)
    {
      auto const ignored = sink->close(OperationContext {});
      static_cast<void>(ignored);
    }
    if (!committed)
    {
      internal::remove_file(temporary_path);
    }
  }
};

AtomicFileWriter::AtomicFileWriter() noexcept = default;

AtomicFileWriter::AtomicFileWriter(std::unique_ptr<Impl> impl)
    : m_impl { std::move(impl) }
{
}

AtomicFileWriter::AtomicFileWriter(AtomicFileWriter &&other) noexcept = default;

auto AtomicFileWriter::operator= (AtomicFileWriter &&other) noexcept
    -> AtomicFileWriter & = default;

AtomicFileWriter::~AtomicFileWriter() = default;

auto AtomicFileWriter::create(
    std::filesystem::path const &target, OperationContext const &context)
    -> Result<AtomicFileWriter>
{
  if (target.empty())
  {
    return std::unexpected { make_error(
        IoErrorCode::InvalidArgument, "atomic target path must not be empty") };
  }
  if (target.native().size() > context.budget.max_path_length)
  {
    return std::unexpected { make_error(
        IoErrorCode::ResourceLimitExceeded,
        "atomic target path exceeds the resource budget") };
  }

  auto directory = target.parent_path();
  if (directory.empty())
  {
    auto filesystem_error = std::error_code {};
    directory             = std::filesystem::current_path(filesystem_error);
    if (filesystem_error)
    {
      auto error = make_error(
          IoErrorCode::TemporaryFileCreationFailed, "current directory is unavailable");
      error.native_code = filesystem_error.value();
      return std::unexpected { std::move(error) };
    }
  }

  auto native = internal::create_temporary_file(directory, context);
  if (!native)
  {
    return std::unexpected { std::move(native.error()) };
  }

  auto impl            = std::make_unique<Impl>();
  impl->target_path    = target;
  impl->temporary_path = std::move(native->path);
  impl->sink           = std::move(native->sink);
  return AtomicFileWriter { std::move(impl) };
}

auto AtomicFileWriter::sink() -> ByteSink &
{
  return *m_impl->sink;
}

auto AtomicFileWriter::temporary_path() const -> std::filesystem::path const &
{
  return m_impl->temporary_path;
}

auto AtomicFileWriter::commit(OperationContext const &context) -> Status
{
  if (!m_impl || m_impl->committed)
  {
    return std::unexpected { make_error(
        IoErrorCode::AlreadyCommitted, "atomic writer is already committed") };
  }
  if (context.cancellation.is_cancelled())
  {
    return std::unexpected { make_error(
        IoErrorCode::Cancelled, "I/O operation cancelled") };
  }

  auto close_result = m_impl->sink->close(context);
  if (!close_result)
  {
    return close_result;
  }
  if (context.cancellation.is_cancelled())
  {
    return std::unexpected { make_error(
        IoErrorCode::Cancelled, "I/O operation cancelled") };
  }

  auto replace_result =
      internal::replace_file(m_impl->temporary_path, m_impl->target_path);
  if (!replace_result)
  {
    return replace_result;
  }

  m_impl->committed = true;
  return {};
}

} // namespace xmole2::io
