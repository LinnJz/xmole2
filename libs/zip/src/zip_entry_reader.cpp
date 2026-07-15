#include "xmole2/zip/zip_entry_reader.hpp"

#include <algorithm>
#include <limits>
#include <utility>

#include "backend.hpp"
#include "xmole2/zip/error.hpp"

namespace xmole2::zip
{
namespace
{

auto resource_error(std::string_view const message) -> Error
{
  return make_error(ZipErrorCode::ResourceLimitExceeded, message);
}

auto same_entry(ZipEntry const &entry, internal::BackendEntry const &backend_entry)
    -> bool
{
  return entry.name == backend_entry.name &&
         entry.compressed_size == backend_entry.compressed_size &&
         entry.uncompressed_size == backend_entry.uncompressed_size &&
         entry.crc32 == backend_entry.crc32 &&
         entry.compression_method == backend_entry.compression_method &&
         entry.encrypted == backend_entry.encrypted;
}

} // namespace

struct ZipEntryReader::Impl
{
  ZipEntry entry;
  std::unique_ptr<internal::BackendArchive> backend;
  std::uint64_t position {};
  bool finished {};
  bool terminal_error {};
};

ZipEntryReader::ZipEntryReader() noexcept = default;

ZipEntryReader::ZipEntryReader(std::unique_ptr<Impl> impl)
    : m_impl { std::move(impl) }
{
}

ZipEntryReader::ZipEntryReader(ZipEntryReader &&other) noexcept = default;

auto ZipEntryReader::operator= (ZipEntryReader &&other) noexcept
    -> ZipEntryReader & = default;

ZipEntryReader::~ZipEntryReader() = default;

auto ZipEntryReader::open(
    io::RandomAccessReader source,
    ZipEntry entry,
    std::int64_t const central_directory_offset,
    OperationContext const &context) -> Result<ZipEntryReader>
{
  auto backend = internal::BackendArchive::open(std::move(source), context);
  if (!backend)
  {
    return std::unexpected { std::move(backend.error()) };
  }
  auto status = (*backend)->goto_entry(central_directory_offset, context);
  if (!status)
  {
    return std::unexpected { std::move(status.error()) };
  }
  auto backend_entry = (*backend)->current_entry(context);
  if (!backend_entry)
  {
    return std::unexpected { std::move(backend_entry.error()) };
  }
  if (!same_entry(entry, *backend_entry))
  {
    return std::unexpected { make_error(
        ZipErrorCode::InvalidArchive,
        "indexed ZIP entry changed while opening its stream") };
  }
  status = (*backend)->open_current_entry(context);
  if (!status)
  {
    return std::unexpected { std::move(status.error()) };
  }

  auto impl     = std::make_unique<Impl>();
  impl->entry   = std::move(entry);
  impl->backend = std::move(*backend);
  return ZipEntryReader { std::move(impl) };
}

auto ZipEntryReader::read(
    std::span<std::byte> const destination, OperationContext const &context)
    -> Result<std::size_t>
{
  if (!m_impl || m_impl->terminal_error)
  {
    return std::unexpected { make_error(
        ZipErrorCode::ReaderClosed, "ZIP entry reader is closed") };
  }
  if (m_impl->finished)
  {
    return std::size_t {};
  }
  if (context.cancellation.is_cancelled())
  {
    return std::unexpected { make_error(
        ZipErrorCode::Cancelled, "ZIP operation cancelled") };
  }
  if (m_impl->entry.uncompressed_size > context.budget.max_single_resource_bytes ||
      m_impl->entry.uncompressed_size > context.budget.max_expanded_bytes ||
      destination.size() > context.budget.max_single_resource_bytes)
  {
    return std::unexpected { resource_error(
        "ZIP entry read exceeds the current resource budget") };
  }
  if (m_impl->position == m_impl->entry.uncompressed_size)
  {
    auto status = finish(context);
    if (!status)
    {
      return std::unexpected { std::move(status.error()) };
    }
    return std::size_t {};
  }
  if (destination.empty())
  {
    return std::unexpected { make_error(
        ZipErrorCode::InvalidArgument,
        "ZIP entry read destination must not be empty before EOF") };
  }

  auto const remaining = m_impl->entry.uncompressed_size - m_impl->position;
  auto const request   = static_cast<std::size_t>(std::min<std::uint64_t>(
      remaining,
      std::min<std::uint64_t>(
          destination.size(),
          static_cast<std::uint64_t>(std::numeric_limits<std::int32_t>::max()))));
  auto result          = m_impl->backend->read_entry(destination.first(request), context);
  if (!result)
  {
    m_impl->terminal_error = true;
    return std::unexpected { std::move(result.error()) };
  }
  if (*result == 0)
  {
    m_impl->terminal_error = true;
    return std::unexpected { make_error(
        ZipErrorCode::IntegrityCheckFailed,
        "ZIP entry ended before its declared uncompressed size") };
  }
  if (*result > request || *result > remaining)
  {
    m_impl->terminal_error = true;
    return std::unexpected { make_error(
        ZipErrorCode::InvalidArchive,
        "ZIP backend produced more data than the declared entry size") };
  }
  m_impl->position += *result;

  if (context.progress != nullptr)
  {
    context.progress->report(
        ProgressUpdate { "zip.read", m_impl->position, m_impl->entry.uncompressed_size });
  }
  return result;
}

auto ZipEntryReader::finish(OperationContext const &context) -> Status
{
  if (!m_impl || m_impl->terminal_error)
  {
    return std::unexpected { make_error(
        ZipErrorCode::ReaderClosed, "ZIP entry reader is closed") };
  }
  if (m_impl->finished)
  {
    return {};
  }
  if (context.cancellation.is_cancelled())
  {
    return std::unexpected { make_error(
        ZipErrorCode::Cancelled, "ZIP operation cancelled") };
  }
  if (m_impl->position != m_impl->entry.uncompressed_size)
  {
    return std::unexpected { make_error(
        ZipErrorCode::InvalidArgument,
        "ZIP entry must be fully consumed before finish") };
  }

  auto close_info = m_impl->backend->close_entry(context);
  if (!close_info)
  {
    m_impl->terminal_error = true;
    return std::unexpected { std::move(close_info.error()) };
  }
  if (close_info->crc32 != m_impl->entry.crc32 ||
      close_info->compressed_size != m_impl->entry.compressed_size ||
      close_info->uncompressed_size != m_impl->entry.uncompressed_size)
  {
    m_impl->terminal_error = true;
    return std::unexpected { make_error(
        ZipErrorCode::IntegrityCheckFailed,
        "ZIP data descriptor does not match the indexed entry") };
  }
  m_impl->finished = true;
  return {};
}

auto ZipEntryReader::position() const noexcept -> std::uint64_t
{
  return m_impl ? m_impl->position : 0;
}

auto ZipEntryReader::size() const noexcept -> std::uint64_t
{
  return m_impl ? m_impl->entry.uncompressed_size : 0;
}

auto ZipEntryReader::finished() const noexcept -> bool
{
  return m_impl != nullptr && m_impl->finished;
}

} // namespace xmole2::zip
