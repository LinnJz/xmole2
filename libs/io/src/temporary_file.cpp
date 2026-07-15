#include "xmole2/io/temporary_file.hpp"

#include <filesystem>
#include <memory>
#include <utility>

#include "file_io.hpp"
#include "xmole2/io/byte_sink.hpp"
#include "xmole2/io/error.hpp"

namespace xmole2::io
{

struct TemporaryFile::Impl
{
  std::filesystem::path path;
  std::unique_ptr<ByteSink> sink;
  bool owns_path { true };

  ~Impl()
  {
    if (sink)
    {
      auto const ignored = sink->close(OperationContext {});
      static_cast<void>(ignored);
    }
    if (owns_path)
    {
      internal::remove_file(path);
    }
  }
};

TemporaryFile::TemporaryFile() noexcept = default;

TemporaryFile::TemporaryFile(std::unique_ptr<Impl> impl)
    : m_impl { std::move(impl) }
{
}

TemporaryFile::TemporaryFile(TemporaryFile &&other) noexcept = default;

auto TemporaryFile::operator= (TemporaryFile &&other) noexcept
    -> TemporaryFile & = default;

TemporaryFile::~TemporaryFile() = default;

auto TemporaryFile::create(OperationContext const &context) -> Result<TemporaryFile>
{
  auto filesystem_error = std::error_code {};
  auto directory        = std::filesystem::temp_directory_path(filesystem_error);
  if (filesystem_error)
  {
    auto error = make_error(
        IoErrorCode::TemporaryFileCreationFailed,
        "system temporary directory is unavailable");
    error.native_code = filesystem_error.value();
    return std::unexpected { std::move(error) };
  }
  return create(directory, context);
}

auto TemporaryFile::create(
    std::filesystem::path const &directory, OperationContext const &context)
    -> Result<TemporaryFile>
{
  auto native = internal::create_temporary_file(directory, context);
  if (!native)
  {
    return std::unexpected { std::move(native.error()) };
  }

  auto impl  = std::make_unique<Impl>();
  impl->path = std::move(native->path);
  impl->sink = std::move(native->sink);
  return TemporaryFile { std::move(impl) };
}

auto TemporaryFile::path() const -> std::filesystem::path const &
{
  return m_impl->path;
}

auto TemporaryFile::sink() -> ByteSink &
{
  return *m_impl->sink;
}

auto TemporaryFile::seal(OperationContext const &context) && -> Result<SourceLease>
{
  if (!m_impl || !m_impl->sink)
  {
    return std::unexpected { make_error(
        IoErrorCode::SinkClosed, "temporary file sink is unavailable") };
  }

  auto close_result = m_impl->sink->close(context);
  if (!close_result)
  {
    return std::unexpected { std::move(close_result.error()) };
  }
  m_impl->sink.reset();

  auto source = internal::open_file_source(m_impl->path, true, context);
  if (!source)
  {
    return std::unexpected { std::move(source.error()) };
  }

  auto lease = SourceLease::acquire(std::move(*source), context);
  if (!lease)
  {
    return std::unexpected { std::move(lease.error()) };
  }

  m_impl->owns_path = false;
  m_impl.reset();
  return lease;
}

} // namespace xmole2::io

