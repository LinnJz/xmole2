#include "backend.hpp"

#include <algorithm>
#include <limits>
#include <minizip-ng/mz.h>
#include <minizip-ng/mz_strm.h>
#include <minizip-ng/mz_zip.h>
#include <optional>
#include <utility>

#include "xmole2/io/error.hpp"
#include "xmole2/zip/error.hpp"

namespace xmole2::zip::internal
{
namespace
{

struct SourceStream
{
  mz_stream stream {};
  io::RandomAccessReader reader;
  OperationContext const *context {};
  std::optional<Error> source_error;
  bool open { true };
};

auto as_source_stream(void *stream) noexcept -> SourceStream *
{
  return static_cast<SourceStream *>(stream);
}

auto source_open(void *stream, char const *, std::int32_t const mode) -> std::int32_t
{
  auto *source = as_source_stream(stream);
  if (source == nullptr || (mode & MZ_OPEN_MODE_WRITE) != 0)
  {
    return MZ_PARAM_ERROR;
  }
  source->open = true;
  return MZ_OK;
}

auto source_is_open(void *stream) -> std::int32_t
{
  auto const *source = as_source_stream(stream);
  return source != nullptr && source->open ? MZ_OK : MZ_STREAM_ERROR;
}

auto source_read(void *stream, void *buffer, std::int32_t const size) -> std::int32_t
{
  auto *source = as_source_stream(stream);
  if (source == nullptr || source->context == nullptr || buffer == nullptr || size < 0)
  {
    return MZ_PARAM_ERROR;
  }

  auto const destination = std::span<std::byte> {
    static_cast<std::byte *>(buffer), static_cast<std::size_t>(size)
  };
  auto result = source->reader.read(destination, *source->context);
  if (!result)
  {
    source->source_error = std::move(result.error());
    return MZ_READ_ERROR;
  }
  if (*result > static_cast<std::size_t>(std::numeric_limits<std::int32_t>::max()))
  {
    return MZ_READ_ERROR;
  }
  return static_cast<std::int32_t>(*result);
}

auto source_write(void *, void const *, std::int32_t) -> std::int32_t
{
  return MZ_WRITE_ERROR;
}

auto source_tell(void *stream) -> std::int64_t
{
  auto const *source = as_source_stream(stream);
  if (source == nullptr ||
      source->reader.position() >
          static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max()))
  {
    return MZ_TELL_ERROR;
  }
  return static_cast<std::int64_t>(source->reader.position());
}

auto apply_signed_offset(std::uint64_t const base, std::int64_t const offset)
    -> std::optional<std::uint64_t>
{
  if (offset >= 0)
  {
    auto const positive = static_cast<std::uint64_t>(offset);
    if (positive > std::numeric_limits<std::uint64_t>::max() - base)
    {
      return std::nullopt;
    }
    return base + positive;
  }

  auto const magnitude = static_cast<std::uint64_t>(-(offset + 1)) + 1;
  if (magnitude > base)
  {
    return std::nullopt;
  }
  return base - magnitude;
}

auto source_seek(void *stream, std::int64_t const offset, std::int32_t const origin)
    -> std::int32_t
{
  auto *source = as_source_stream(stream);
  if (source == nullptr || source->context == nullptr)
  {
    return MZ_PARAM_ERROR;
  }

  auto base = std::uint64_t {};
  if (origin == MZ_SEEK_CUR)
  {
    base = source->reader.position();
  }
  else if (origin == MZ_SEEK_END)
  {
    auto const size = source->reader.size(*source->context);
    if (!size)
    {
      source->source_error = size.error();
      return MZ_SEEK_ERROR;
    }
    base = *size;
  }
  else if (origin != MZ_SEEK_SET)
  {
    return MZ_SEEK_ERROR;
  }

  auto const target = apply_signed_offset(base, offset);
  if (!target)
  {
    return MZ_SEEK_ERROR;
  }
  auto status = source->reader.seek(*target, *source->context);
  if (!status)
  {
    source->source_error = std::move(status.error());
    return MZ_SEEK_ERROR;
  }
  return MZ_OK;
}

auto source_close(void *stream) -> std::int32_t
{
  auto *source = as_source_stream(stream);
  if (source == nullptr)
  {
    return MZ_PARAM_ERROR;
  }
  source->open = false;
  return MZ_OK;
}

auto source_error(void *stream) -> std::int32_t
{
  auto const *source = as_source_stream(stream);
  return source != nullptr && source->source_error ? MZ_READ_ERROR : MZ_OK;
}

auto source_get_property(void *, std::int32_t, std::int64_t *) -> std::int32_t
{
  return MZ_EXIST_ERROR;
}

auto source_set_property(void *, std::int32_t, std::int64_t) -> std::int32_t
{
  return MZ_EXIST_ERROR;
}

auto source_vtable() -> mz_stream_vtbl *
{
  static auto table = mz_stream_vtbl {
    source_open, source_is_open, source_read,         source_write,
    source_tell, source_seek,    source_close,        source_error,
    nullptr,     nullptr,        source_get_property, source_set_property,
  };
  return &table;
}

auto map_source_error(Error const &cause) -> ZipErrorCode
{
  if (cause.domain == ErrorDomain::Io &&
      cause.code == static_cast<std::uint32_t>(io::IoErrorCode::Cancelled))
  {
    return ZipErrorCode::Cancelled;
  }
  if (cause.domain == ErrorDomain::Io &&
      cause.code == static_cast<std::uint32_t>(io::IoErrorCode::ResourceLimitExceeded))
  {
    return ZipErrorCode::ResourceLimitExceeded;
  }
  return ZipErrorCode::ReadFailed;
}

auto backend_error(
    ZipErrorCode code,
    std::string_view const message,
    std::int32_t const native_code,
    std::optional<Error> source_error) -> Error
{
  if (source_error)
  {
    code = map_source_error(*source_error);
  }
  auto error        = make_error(code, message);
  error.native_code = native_code;
  if (source_error)
  {
    error.cause = std::make_shared<Error const>(std::move(*source_error));
  }
  return error;
}

auto map_backend_code(std::int32_t const code, ZipErrorCode const fallback)
    -> ZipErrorCode
{
  if (code == MZ_CRC_ERROR)
  {
    return ZipErrorCode::IntegrityCheckFailed;
  }
  if (code == MZ_SUPPORT_ERROR)
  {
    return ZipErrorCode::UnsupportedCompression;
  }
  if (code == MZ_FORMAT_ERROR || code == MZ_DATA_ERROR || code == MZ_OPEN_ERROR)
  {
    return ZipErrorCode::InvalidArchive;
  }
  return fallback;
}

} // namespace

struct BackendArchive::Impl
{
  explicit Impl(io::RandomAccessReader source)
      : stream {
        .stream = { .vtbl = source_vtable(), .base = nullptr },
        .reader = std::move(source)
  }
  {
  }

  ~Impl()
  {
    if (entry_open)
    {
      static_cast<void>(mz_zip_entry_read_close(handle, nullptr, nullptr, nullptr));
    }
    if (archive_open)
    {
      static_cast<void>(mz_zip_close(handle));
    }
    if (handle != nullptr)
    {
      mz_zip_delete(&handle);
    }
  }

  auto begin(OperationContext const &context) -> void
  {
    stream.context = &context;
    stream.source_error.reset();
  }

  auto end() -> std::optional<Error>
  {
    stream.context = nullptr;
    return std::exchange(stream.source_error, std::nullopt);
  }

  SourceStream stream;
  void *handle {};
  bool archive_open {};
  bool entry_open {};
};

BackendArchive::BackendArchive(std::unique_ptr<Impl> impl)
    : m_impl { std::move(impl) }
{
}

BackendArchive::~BackendArchive() = default;

auto BackendArchive::open(io::RandomAccessReader source, OperationContext const &context)
    -> Result<std::unique_ptr<BackendArchive>>
{
  if (context.cancellation.is_cancelled())
  {
    return std::unexpected { make_error(
        ZipErrorCode::Cancelled, "ZIP operation cancelled") };
  }

  auto source_size = source.size(context);
  if (!source_size)
  {
    auto error  = make_error(ZipErrorCode::ReadFailed, "failed to query ZIP source size");
    error.cause = std::make_shared<Error const>(std::move(source_size.error()));
    return std::unexpected { std::move(error) };
  }
  if (*source_size > static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max()))
  {
    return std::unexpected { make_error(
        ZipErrorCode::ResourceLimitExceeded,
        "ZIP source exceeds the backend addressable range") };
  }

  auto impl    = std::make_unique<Impl>(std::move(source));
  impl->handle = mz_zip_create();
  if (impl->handle == nullptr)
  {
    return std::unexpected { make_error(
        ZipErrorCode::ReadFailed, "failed to create ZIP backend") };
  }

  impl->begin(context);
  auto const result = mz_zip_open(impl->handle, &impl->stream, MZ_OPEN_MODE_READ);
  auto cause        = impl->end();
  if (result != MZ_OK)
  {
    return std::unexpected { backend_error(
        map_backend_code(result, ZipErrorCode::InvalidArchive),
        "failed to open ZIP central directory", result, std::move(cause)) };
  }
  impl->archive_open = true;
  return std::unique_ptr<BackendArchive> { new BackendArchive { std::move(impl) } };
}

auto BackendArchive::reported_entry_count(OperationContext const &context)
    -> Result<std::uint64_t>
{
  if (context.cancellation.is_cancelled())
  {
    return std::unexpected { make_error(
        ZipErrorCode::Cancelled, "ZIP operation cancelled") };
  }
  auto count        = std::uint64_t {};
  auto const result = mz_zip_get_number_entry(m_impl->handle, &count);
  if (result != MZ_OK)
  {
    return std::unexpected { backend_error(
        ZipErrorCode::InvalidArchive, "failed to read ZIP entry count", result,
        std::nullopt) };
  }
  return count;
}

auto BackendArchive::goto_first_entry(OperationContext const &context) -> Result<bool>
{
  if (context.cancellation.is_cancelled())
  {
    return std::unexpected { make_error(
        ZipErrorCode::Cancelled, "ZIP operation cancelled") };
  }
  m_impl->begin(context);
  auto const result = mz_zip_goto_first_entry(m_impl->handle);
  auto cause        = m_impl->end();
  if (result == MZ_END_OF_LIST)
  {
    return false;
  }
  if (result != MZ_OK)
  {
    return std::unexpected { backend_error(
        map_backend_code(result, ZipErrorCode::InvalidArchive),
        "failed to read the first ZIP entry", result, std::move(cause)) };
  }
  return true;
}

auto BackendArchive::goto_next_entry(OperationContext const &context) -> Result<bool>
{
  if (context.cancellation.is_cancelled())
  {
    return std::unexpected { make_error(
        ZipErrorCode::Cancelled, "ZIP operation cancelled") };
  }
  m_impl->begin(context);
  auto const result = mz_zip_goto_next_entry(m_impl->handle);
  auto cause        = m_impl->end();
  if (result == MZ_END_OF_LIST)
  {
    return false;
  }
  if (result != MZ_OK)
  {
    return std::unexpected { backend_error(
        map_backend_code(result, ZipErrorCode::InvalidArchive),
        "failed to read the next ZIP entry", result, std::move(cause)) };
  }
  return true;
}

auto BackendArchive::goto_entry(
    std::int64_t const central_directory_offset, OperationContext const &context)
    -> Status
{
  if (context.cancellation.is_cancelled())
  {
    return std::unexpected { make_error(
        ZipErrorCode::Cancelled, "ZIP operation cancelled") };
  }
  m_impl->begin(context);
  auto const result = mz_zip_goto_entry(m_impl->handle, central_directory_offset);
  auto cause        = m_impl->end();
  if (result != MZ_OK)
  {
    return std::unexpected { backend_error(
        map_backend_code(result, ZipErrorCode::InvalidArchive),
        "failed to locate indexed ZIP entry", result, std::move(cause)) };
  }
  return {};
}

auto BackendArchive::current_entry(OperationContext const &context)
    -> Result<BackendEntry>
{
  if (context.cancellation.is_cancelled())
  {
    return std::unexpected { make_error(
        ZipErrorCode::Cancelled, "ZIP operation cancelled") };
  }

  auto *file        = static_cast<mz_zip_file *>(nullptr);
  auto const result = mz_zip_entry_get_info(m_impl->handle, &file);
  if (result != MZ_OK || file == nullptr || file->filename == nullptr ||
      file->compressed_size < 0 || file->uncompressed_size < 0)
  {
    return std::unexpected { backend_error(
        ZipErrorCode::InvalidArchive, "ZIP entry metadata is invalid", result,
        std::nullopt) };
  }

  auto const directory = mz_zip_entry_is_dir(m_impl->handle) == MZ_OK;
  return BackendEntry {
    .central_directory_offset = mz_zip_get_entry(m_impl->handle),
    .name                     = std::string { file->filename, file->filename_size },
    .compressed_size          = static_cast<std::uint64_t>(file->compressed_size),
    .uncompressed_size        = static_cast<std::uint64_t>(file->uncompressed_size),
    .crc32                    = file->crc,
    .compression_method       = file->compression_method,
    .encrypted                = (file->flag & MZ_ZIP_FLAG_ENCRYPTED) != 0,
    .directory                = directory,
  };
}

auto BackendArchive::open_current_entry(OperationContext const &context) -> Status
{
  if (context.cancellation.is_cancelled())
  {
    return std::unexpected { make_error(
        ZipErrorCode::Cancelled, "ZIP operation cancelled") };
  }
  m_impl->begin(context);
  auto const result = mz_zip_entry_read_open(m_impl->handle, 0, nullptr);
  auto cause        = m_impl->end();
  if (result != MZ_OK)
  {
    return std::unexpected { backend_error(
        map_backend_code(result, ZipErrorCode::ReadFailed),
        "failed to open ZIP entry stream", result, std::move(cause)) };
  }
  m_impl->entry_open = true;
  return {};
}

auto BackendArchive::read_entry(
    std::span<std::byte> const destination, OperationContext const &context)
    -> Result<std::size_t>
{
  if (context.cancellation.is_cancelled())
  {
    return std::unexpected { make_error(
        ZipErrorCode::Cancelled, "ZIP operation cancelled") };
  }
  auto const request = static_cast<std::int32_t>(std::min<std::size_t>(
      destination.size(),
      static_cast<std::size_t>(std::numeric_limits<std::int32_t>::max())));
  m_impl->begin(context);
  auto const result = mz_zip_entry_read(m_impl->handle, destination.data(), request);
  auto cause        = m_impl->end();
  if (result < 0)
  {
    return std::unexpected { backend_error(
        map_backend_code(result, ZipErrorCode::ReadFailed),
        "failed while reading ZIP entry", result, std::move(cause)) };
  }
  return static_cast<std::size_t>(result);
}

auto BackendArchive::close_entry(OperationContext const &context)
    -> Result<BackendCloseInfo>
{
  if (context.cancellation.is_cancelled())
  {
    return std::unexpected { make_error(
        ZipErrorCode::Cancelled, "ZIP operation cancelled") };
  }
  auto crc32             = std::uint32_t {};
  auto compressed_size   = std::int64_t {};
  auto uncompressed_size = std::int64_t {};
  m_impl->begin(context);
  auto const result = mz_zip_entry_read_close(
      m_impl->handle, &crc32, &compressed_size, &uncompressed_size);
  auto cause         = m_impl->end();
  m_impl->entry_open = false;
  if (result != MZ_OK || compressed_size < 0 || uncompressed_size < 0)
  {
    return std::unexpected { backend_error(
        map_backend_code(result, ZipErrorCode::IntegrityCheckFailed),
        "ZIP entry integrity validation failed", result, std::move(cause)) };
  }
  return BackendCloseInfo {
    .crc32             = crc32,
    .compressed_size   = static_cast<std::uint64_t>(compressed_size),
    .uncompressed_size = static_cast<std::uint64_t>(uncompressed_size),
  };
}

} // namespace xmole2::zip::internal
