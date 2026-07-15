#include "xmole2/zip/zip_archive.hpp"

#include <limits>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "backend.hpp"
#include "xmole2/zip/error.hpp"

namespace xmole2::zip
{
namespace
{

struct IndexedEntry
{
  ZipEntry entry;
  std::int64_t central_directory_offset {};
};

auto is_ascii_letter(char const value) noexcept -> bool
{
  return (value >= 'A' && value <= 'Z') || (value >= 'a' && value <= 'z');
}

auto is_safe_entry_name(std::string_view const name) -> bool
{
  if (name.empty() || name.front() == '/' || name.front() == '\\' ||
      name.find('\0') != std::string_view::npos ||
      name.find('\\') != std::string_view::npos ||
      (name.size() >= 2 && is_ascii_letter(name[0]) && name[1] == ':'))
  {
    return false;
  }

  auto segment_start = std::size_t {};
  while (segment_start < name.size())
  {
    auto const separator = name.find('/', segment_start);
    auto const segment_end =
        separator == std::string_view::npos ? name.size() : separator;
    auto const segment = name.substr(segment_start, segment_end - segment_start);
    if (segment.empty() || segment == "." || segment == "..")
    {
      return false;
    }
    if (separator == std::string_view::npos || separator + 1 == name.size())
    {
      break;
    }
    segment_start = separator + 1;
  }
  return true;
}

auto add_with_limit(
    std::uint64_t &total, std::uint64_t const value, std::uint64_t const limit) -> bool
{
  if (value > std::numeric_limits<std::uint64_t>::max() - total)
  {
    return false;
  }
  total += value;
  return total <= limit;
}

auto resource_error(std::string_view const message) -> Error
{
  return make_error(ZipErrorCode::ResourceLimitExceeded, message);
}

auto validate_read_budget(ZipEntry const &entry, OperationContext const &context)
    -> Status
{
  if (context.cancellation.is_cancelled())
  {
    return std::unexpected { make_error(
        ZipErrorCode::Cancelled, "ZIP operation cancelled") };
  }
  if (entry.uncompressed_size > context.budget.max_single_resource_bytes ||
      entry.uncompressed_size > context.budget.max_expanded_bytes)
  {
    return std::unexpected { resource_error(
        "ZIP entry exceeds the current decompression budget") };
  }
  return {};
}

} // namespace

struct ZipArchive::Impl
{
  explicit Impl(io::SourceLease stable_source)
      : source { std::move(stable_source) }
  {
  }

  io::SourceLease source;
  std::vector<IndexedEntry> entries;
  std::unordered_map<std::string, std::size_t> entries_by_name;
};

ZipArchive::ZipArchive() noexcept = default;

ZipArchive::ZipArchive(std::unique_ptr<Impl> impl)
    : m_impl { std::move(impl) }
{
}

ZipArchive::ZipArchive(ZipArchive &&other) noexcept = default;

auto ZipArchive::operator= (ZipArchive &&other) noexcept -> ZipArchive & = default;

ZipArchive::~ZipArchive() = default;

auto ZipArchive::open(io::SourceLease source, OperationContext const &context)
    -> Result<ZipArchive>
{
  if (context.cancellation.is_cancelled())
  {
    return std::unexpected { make_error(
        ZipErrorCode::Cancelled, "ZIP operation cancelled") };
  }
  if (!source.valid())
  {
    return std::unexpected { make_error(
        ZipErrorCode::InvalidArgument, "ZIP source lease is empty") };
  }

  auto source_size = source.size(context);
  if (!source_size)
  {
    auto error  = make_error(ZipErrorCode::ReadFailed, "failed to query ZIP source size");
    error.cause = std::make_shared<Error const>(std::move(source_size.error()));
    return std::unexpected { std::move(error) };
  }

  auto source_reader = source.reader(0, context);
  if (!source_reader)
  {
    auto error =
        make_error(ZipErrorCode::ReadFailed, "failed to create ZIP source reader");
    error.cause = std::make_shared<Error const>(std::move(source_reader.error()));
    return std::unexpected { std::move(error) };
  }
  auto backend = internal::BackendArchive::open(std::move(*source_reader), context);
  if (!backend)
  {
    return std::unexpected { std::move(backend.error()) };
  }

  auto reported_count = (*backend)->reported_entry_count(context);
  if (!reported_count)
  {
    return std::unexpected { std::move(reported_count.error()) };
  }
  if (*reported_count > context.budget.max_entry_count ||
      *reported_count > std::numeric_limits<std::size_t>::max())
  {
    return std::unexpected { resource_error(
        "ZIP entry count exceeds the resource budget") };
  }

  auto const base_entry_memory = static_cast<std::uint64_t>(sizeof(IndexedEntry) * 2);
  if (*reported_count != 0 &&
      (base_entry_memory > context.budget.max_memory_bytes ||
       *reported_count > context.budget.max_memory_bytes / base_entry_memory))
  {
    return std::unexpected { resource_error("ZIP index exceeds the memory budget") };
  }

  auto impl = std::make_unique<Impl>(std::move(source));
  impl->entries.reserve(static_cast<std::size_t>(*reported_count));
  impl->entries_by_name.reserve(static_cast<std::size_t>(*reported_count));

  auto expanded_total  = std::uint64_t {};
  auto memory_estimate = std::uint64_t {};
  auto has_entry       = (*backend)->goto_first_entry(context);
  if (!has_entry)
  {
    return std::unexpected { std::move(has_entry.error()) };
  }

  while (*has_entry)
  {
    if (impl->entries.size() >= context.budget.max_entry_count)
    {
      return std::unexpected { resource_error(
          "ZIP entry count exceeds the resource budget") };
    }

    auto backend_entry = (*backend)->current_entry(context);
    if (!backend_entry)
    {
      return std::unexpected { std::move(backend_entry.error()) };
    }
    if (backend_entry->central_directory_offset < 0 ||
        backend_entry->compressed_size > *source_size)
    {
      return std::unexpected { make_error(
          ZipErrorCode::InvalidArchive, "ZIP entry offsets or sizes are invalid") };
    }
    if (backend_entry->name.size() > context.budget.max_path_length)
    {
      return std::unexpected { resource_error("ZIP entry name exceeds the path budget") };
    }
    if (!is_safe_entry_name(backend_entry->name))
    {
      return std::unexpected { make_error(
          ZipErrorCode::UnsafeEntryName, "ZIP entry name is unsafe") };
    }
    if (backend_entry->uncompressed_size > context.budget.max_single_resource_bytes)
    {
      return std::unexpected { resource_error(
          "ZIP entry exceeds the single-resource budget") };
    }
    if (!add_with_limit(
            expanded_total, backend_entry->uncompressed_size,
            context.budget.max_expanded_bytes))
    {
      return std::unexpected { resource_error(
          "ZIP expanded size exceeds the resource budget") };
    }

    auto const entry_memory =
        base_entry_memory + (static_cast<std::uint64_t>(backend_entry->name.size()) * 2);
    if (!add_with_limit(memory_estimate, entry_memory, context.budget.max_memory_bytes))
    {
      return std::unexpected { resource_error("ZIP index exceeds the memory budget") };
    }

    auto const index    = impl->entries.size();
    auto const inserted = impl->entries_by_name.emplace(backend_entry->name, index);
    if (!inserted.second)
    {
      return std::unexpected { make_error(
          ZipErrorCode::DuplicateEntry, "ZIP contains duplicate entry names") };
    }

    impl->entries.push_back(
        IndexedEntry {
            .entry =
                ZipEntry {
                          .index              = index,
                          .name               = std::move(backend_entry->name),
                          .compressed_size    = backend_entry->compressed_size,
                          .uncompressed_size  = backend_entry->uncompressed_size,
                          .crc32              = backend_entry->crc32,
                          .compression_method = backend_entry->compression_method,
                          .encrypted          = backend_entry->encrypted,
                          .directory          = backend_entry->directory,
                          },
            .central_directory_offset = backend_entry->central_directory_offset,
    });

    if (context.progress != nullptr)
    {
      context.progress->report(
          ProgressUpdate { "zip.index", static_cast<std::uint64_t>(impl->entries.size()),
                           *reported_count });
    }

    has_entry = (*backend)->goto_next_entry(context);
    if (!has_entry)
    {
      return std::unexpected { std::move(has_entry.error()) };
    }
  }

  if (impl->entries.size() != *reported_count)
  {
    return std::unexpected { make_error(
        ZipErrorCode::InvalidArchive,
        "ZIP directory entry count does not match its end record") };
  }
  return ZipArchive { std::move(impl) };
}

auto ZipArchive::entry_count() const noexcept -> std::size_t
{
  return m_impl ? m_impl->entries.size() : 0;
}

auto ZipArchive::entry_at(std::size_t const index) const -> Result<ZipEntry>
{
  if (!m_impl)
  {
    return std::unexpected { make_error(
        ZipErrorCode::InvalidArgument, "ZIP archive is empty") };
  }
  if (index >= m_impl->entries.size())
  {
    return std::unexpected { make_error(
        ZipErrorCode::EntryNotFound, "ZIP entry index is out of range") };
  }
  return m_impl->entries[index].entry;
}

auto ZipArchive::find_entry(std::string_view const name) const
    -> Result<std::optional<ZipEntry>>
{
  if (!m_impl)
  {
    return std::unexpected { make_error(
        ZipErrorCode::InvalidArgument, "ZIP archive is empty") };
  }
  auto const found = m_impl->entries_by_name.find(std::string { name });
  if (found == m_impl->entries_by_name.end())
  {
    return std::optional<ZipEntry> {};
  }
  return std::optional<ZipEntry> { m_impl->entries[found->second].entry };
}

auto ZipArchive::open_entry(std::size_t const index, OperationContext const &context)
    const -> Result<ZipEntryReader>
{
  if (!m_impl)
  {
    return std::unexpected { make_error(
        ZipErrorCode::InvalidArgument, "ZIP archive is empty") };
  }
  if (index >= m_impl->entries.size())
  {
    return std::unexpected { make_error(
        ZipErrorCode::EntryNotFound, "ZIP entry index is out of range") };
  }

  auto const &indexed = m_impl->entries[index];
  auto validation     = validate_read_budget(indexed.entry, context);
  if (!validation)
  {
    return std::unexpected { std::move(validation.error()) };
  }
  if (indexed.entry.encrypted)
  {
    return std::unexpected { make_error(
        ZipErrorCode::EncryptedEntry,
        "encrypted ZIP entries are not read by this layer") };
  }
  if (indexed.entry.compression_method != 0 && indexed.entry.compression_method != 8)
  {
    return std::unexpected { make_error(
        ZipErrorCode::UnsupportedCompression,
        "ZIP entry uses an unsupported compression method") };
  }

  auto source_reader = m_impl->source.reader(0, context);
  if (!source_reader)
  {
    auto error =
        make_error(ZipErrorCode::ReadFailed, "failed to create ZIP entry reader");
    error.cause = std::make_shared<Error const>(std::move(source_reader.error()));
    return std::unexpected { std::move(error) };
  }
  return ZipEntryReader::open(
      std::move(*source_reader), indexed.entry, indexed.central_directory_offset,
      context);
}

auto ZipArchive::open_entry(std::string_view const name, OperationContext const &context)
    const -> Result<ZipEntryReader>
{
  if (!m_impl)
  {
    return std::unexpected { make_error(
        ZipErrorCode::InvalidArgument, "ZIP archive is empty") };
  }
  auto const found = m_impl->entries_by_name.find(std::string { name });
  if (found == m_impl->entries_by_name.end())
  {
    return std::unexpected { make_error(
        ZipErrorCode::EntryNotFound, "ZIP entry name was not found") };
  }
  return open_entry(found->second, context);
}

} // namespace xmole2::zip
