#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "xmole2/io/byte_source.hpp"
#include "xmole2/io/error.hpp"
#include "xmole2/io/source_lease.hpp"
#include "xmole2/zip/error.hpp"
#include "xmole2/zip/zip_archive.hpp"

namespace
{

struct EntrySpec
{
  std::string name;
  std::vector<std::byte> content;
  std::vector<std::byte> encoded;
  std::uint16_t method {};
  std::uint16_t flags {};
  std::optional<std::uint32_t> central_crc;
};

auto bytes_from(std::string_view const text) -> std::vector<std::byte>
{
  auto bytes = std::vector<std::byte>(text.size());
  for (auto index = std::size_t {}; index < text.size(); ++index)
  {
    bytes[index] = static_cast<std::byte>(text[index]);
  }
  return bytes;
}

auto text_from(std::span<std::byte const> const bytes) -> std::string
{
  auto text = std::string(bytes.size(), '\0');
  for (auto index = std::size_t {}; index < bytes.size(); ++index)
  {
    text[index] = static_cast<char>(bytes[index]);
  }
  return text;
}

auto append_u16(std::vector<std::byte> &bytes, std::uint16_t const value) -> void
{
  bytes.push_back(static_cast<std::byte>(value & 0xffU));
  bytes.push_back(static_cast<std::byte>((value >> 8U) & 0xffU));
}

auto append_u32(std::vector<std::byte> &bytes, std::uint32_t const value) -> void
{
  append_u16(bytes, static_cast<std::uint16_t>(value & 0xff'ffU));
  append_u16(bytes, static_cast<std::uint16_t>(value >> 16U));
}

auto append_text(std::vector<std::byte> &bytes, std::string_view const text) -> void
{
  auto const text_bytes = bytes_from(text);
  bytes.insert(bytes.end(), text_bytes.begin(), text_bytes.end());
}

auto crc32(std::span<std::byte const> const bytes) -> std::uint32_t
{
  auto crc = std::uint32_t { 0xff'ff'ff'ffU };
  for (auto const value : bytes)
  {
    crc ^= std::to_integer<std::uint32_t>(value);
    for (auto bit = 0; bit < 8; ++bit)
    {
      auto const mask =
          static_cast<std::uint32_t>(0U - static_cast<std::uint32_t>(crc & 1U));
      crc = (crc >> 1U) ^ (0xed'b8'83'20U & mask);
    }
  }
  return ~crc;
}

auto stored_entry(std::string name, std::string_view const content) -> EntrySpec
{
  auto bytes = bytes_from(content);
  return EntrySpec {
    .name    = std::move(name),
    .content = bytes,
    .encoded = std::move(bytes),
  };
}

auto deflated_hello_entry(std::string name) -> EntrySpec
{
  return EntrySpec {
    .name = std::move(name),
    .content = bytes_from("hello"),
    .encoded = {
      std::byte { 0xcb },
      std::byte { 0x48 },
      std::byte { 0xcd },
      std::byte { 0xc9 },
      std::byte { 0xc9 },
      std::byte { 0x07 },
      std::byte { 0x00 },
    },
    .method = 8,
  };
}

auto make_zip(std::vector<EntrySpec> const &entries) -> std::vector<std::byte>
{
  auto bytes         = std::vector<std::byte> {};
  auto local_offsets = std::vector<std::uint32_t> {};
  local_offsets.reserve(entries.size());

  for (auto const &entry : entries)
  {
    local_offsets.push_back(static_cast<std::uint32_t>(bytes.size()));
    auto const entry_crc = crc32(entry.content);
    append_u32(bytes, 0x04'03'4b'50U);
    append_u16(bytes, 20);
    append_u16(bytes, entry.flags);
    append_u16(bytes, entry.method);
    append_u16(bytes, 0);
    append_u16(bytes, 0);
    append_u32(bytes, entry_crc);
    append_u32(bytes, static_cast<std::uint32_t>(entry.encoded.size()));
    append_u32(bytes, static_cast<std::uint32_t>(entry.content.size()));
    append_u16(bytes, static_cast<std::uint16_t>(entry.name.size()));
    append_u16(bytes, 0);
    append_text(bytes, entry.name);
    bytes.insert(bytes.end(), entry.encoded.begin(), entry.encoded.end());
  }

  auto const central_offset = static_cast<std::uint32_t>(bytes.size());
  for (auto index = std::size_t {}; index < entries.size(); ++index)
  {
    auto const &entry    = entries[index];
    auto const entry_crc = entry.central_crc.value_or(crc32(entry.content));
    append_u32(bytes, 0x02'01'4b'50U);
    append_u16(bytes, 20);
    append_u16(bytes, 20);
    append_u16(bytes, entry.flags);
    append_u16(bytes, entry.method);
    append_u16(bytes, 0);
    append_u16(bytes, 0);
    append_u32(bytes, entry_crc);
    append_u32(bytes, static_cast<std::uint32_t>(entry.encoded.size()));
    append_u32(bytes, static_cast<std::uint32_t>(entry.content.size()));
    append_u16(bytes, static_cast<std::uint16_t>(entry.name.size()));
    append_u16(bytes, 0);
    append_u16(bytes, 0);
    append_u16(bytes, 0);
    append_u16(bytes, 0);
    append_u32(bytes, 0);
    append_u32(bytes, local_offsets[index]);
    append_text(bytes, entry.name);
  }

  auto const central_size = static_cast<std::uint32_t>(bytes.size()) - central_offset;
  append_u32(bytes, 0x06'05'4b'50U);
  append_u16(bytes, 0);
  append_u16(bytes, 0);
  append_u16(bytes, static_cast<std::uint16_t>(entries.size()));
  append_u16(bytes, static_cast<std::uint16_t>(entries.size()));
  append_u32(bytes, central_size);
  append_u32(bytes, central_offset);
  append_u16(bytes, 0);
  return bytes;
}

class CountingSource final : public xmole2::io::ByteSource
{
public:
  CountingSource(std::vector<std::byte> bytes, std::shared_ptr<std::uint64_t> bytes_read)
      : m_bytes { std::move(bytes) }
      , m_bytes_read { std::move(bytes_read) }
  {
  }

  auto size(xmole2::OperationContext const &) const
      -> xmole2::Result<std::uint64_t> override
  {
    return static_cast<std::uint64_t>(m_bytes.size());
  }

  auto read_at(
      std::uint64_t const offset,
      std::span<std::byte> const destination,
      xmole2::OperationContext const &context) const
      -> xmole2::Result<std::size_t> override
  {
    if (context.cancellation.is_cancelled())
    {
      return std::unexpected { xmole2::io::make_error(
          xmole2::io::IoErrorCode::Cancelled, "counting source cancelled") };
    }
    if (offset >= m_bytes.size())
    {
      return std::size_t {};
    }
    auto const count = static_cast<std::size_t>(std::min<std::uint64_t>(
        destination.size(), static_cast<std::uint64_t>(m_bytes.size()) - offset));
    std::memcpy(destination.data(), m_bytes.data() + offset, count);
    *m_bytes_read += count;
    return count;
  }

private:
  std::vector<std::byte> m_bytes;
  std::shared_ptr<std::uint64_t> m_bytes_read;
};

class FaultSource final : public xmole2::io::ByteSource
{
public:
  auto size(xmole2::OperationContext const &) const
      -> xmole2::Result<std::uint64_t> override
  {
    return 1024;
  }

  auto read_at(std::uint64_t, std::span<std::byte>, xmole2::OperationContext const &)
      const -> xmole2::Result<std::size_t> override
  {
    auto error = xmole2::io::make_error(
        xmole2::io::IoErrorCode::ReadFailed, "injected ZIP source failure");
    error.native_code = 4321;
    return std::unexpected { std::move(error) };
  }
};

class ProgressRecorder final : public xmole2::ProgressSink
{
public:
  auto report(xmole2::ProgressUpdate const &update) -> void override
  {
    if (update.phase == "zip.index")
    {
      ++index_updates;
    }
    if (update.phase == "zip.read")
    {
      ++read_updates;
    }
  }

  std::size_t index_updates {};
  std::size_t read_updates {};
};

auto open_archive(std::vector<std::byte> bytes, xmole2::OperationContext const &context)
    -> xmole2::Result<xmole2::zip::ZipArchive>
{
  auto source = xmole2::io::SourceLease::from_buffer(std::move(bytes), context);
  if (!source)
  {
    return std::unexpected { std::move(source.error()) };
  }
  return xmole2::zip::ZipArchive::open(std::move(*source), context);
}

auto test_index_is_lazy_and_searchable() -> bool
{
  auto large_content = std::string(1024 * 1024, 'x');
  auto archive_bytes = make_zip({ stored_entry("large.bin", large_content) });
  auto bytes_read    = std::make_shared<std::uint64_t>();
  auto const context = xmole2::OperationContext {};
  auto source        = xmole2::io::SourceLease::acquire(
      std::make_shared<CountingSource>(std::move(archive_bytes), bytes_read), context);
  if (!source)
  {
    return false;
  }
  auto archive = xmole2::zip::ZipArchive::open(std::move(*source), context);
  if (!archive || archive->entry_count() != 1 || *bytes_read >= 128 * 1024)
  {
    return false;
  }
  auto found   = archive->find_entry("large.bin");
  auto missing = archive->find_entry("missing.bin");
  return found && found->has_value() && (*found)->uncompressed_size == 1024 * 1024 &&
         missing && !missing->has_value();
}

auto test_store_deflate_streaming_and_lifetime() -> bool
{
  auto progress    = ProgressRecorder {};
  auto context     = xmole2::OperationContext {};
  context.progress = &progress;

  auto reader = xmole2::zip::ZipEntryReader {};
  {
    auto archive = open_archive(
        make_zip(
            { stored_entry("plain.txt", "alpha"), deflated_hello_entry("deflated.txt") }),
        context);
    if (!archive || archive->entry_count() != 2)
    {
      return false;
    }
    auto opened = archive->open_entry("deflated.txt", context);
    if (!opened || opened->size() != 5)
    {
      return false;
    }
    reader = std::move(*opened);
  }

  auto output = std::vector<std::byte> {};
  auto buffer = std::array<std::byte, 2> {};
  while (true)
  {
    auto result = reader.read(buffer, context);
    if (!result)
    {
      return false;
    }
    if (*result == 0)
    {
      break;
    }
    output.insert(output.end(), buffer.begin(), buffer.begin() + *result);
  }
  return text_from(output) == "hello" && reader.finished() && reader.position() == 5 &&
         progress.index_updates == 2 && progress.read_updates >= 3;
}

auto test_finish_and_integrity() -> bool
{
  auto const context = xmole2::OperationContext {};
  auto archive       = open_archive(make_zip({ stored_entry("value", "data") }), context);
  if (!archive)
  {
    return false;
  }
  auto reader = archive->open_entry(0, context);
  if (!reader)
  {
    return false;
  }
  auto early = reader->finish(context);
  if (early || early.error().code !=
                   static_cast<std::uint32_t>(xmole2::zip::ZipErrorCode::InvalidArgument))
  {
    return false;
  }

  auto corrupt         = stored_entry("bad-crc", "payload");
  corrupt.central_crc  = 1;
  auto corrupt_archive = open_archive(make_zip({ corrupt }), context);
  if (!corrupt_archive)
  {
    return false;
  }
  auto corrupt_reader = corrupt_archive->open_entry(0, context);
  if (!corrupt_reader)
  {
    return false;
  }
  auto bytes = std::array<std::byte, 7> {};
  auto read  = corrupt_reader->read(bytes, context);
  if (!read || *read != bytes.size())
  {
    return false;
  }
  auto eof = corrupt_reader->read(bytes, context);
  return !eof &&
         eof.error().code ==
             static_cast<std::uint32_t>(xmole2::zip::ZipErrorCode::IntegrityCheckFailed);
}

auto test_budget_and_cancellation() -> bool
{
  auto count_context                   = xmole2::OperationContext {};
  count_context.budget.max_entry_count = 1;
  auto too_many                        = open_archive(
      make_zip({ stored_entry("a", "a"), stored_entry("b", "b") }), count_context);
  if (too_many ||
      too_many.error().code !=
          static_cast<std::uint32_t>(xmole2::zip::ZipErrorCode::ResourceLimitExceeded))
  {
    return false;
  }

  auto expanded_context                      = xmole2::OperationContext {};
  expanded_context.budget.max_expanded_bytes = 4;
  auto too_large =
      open_archive(make_zip({ stored_entry("value", "12345") }), expanded_context);
  if (too_large ||
      too_large.error().code !=
          static_cast<std::uint32_t>(xmole2::zip::ZipErrorCode::ResourceLimitExceeded))
  {
    return false;
  }

  auto memory_context                    = xmole2::OperationContext {};
  memory_context.budget.max_memory_bytes = 1;
  auto ignored_bytes_read                = std::make_shared<std::uint64_t>();
  auto memory_source                     = xmole2::io::SourceLease::acquire(
      std::make_shared<CountingSource>(
          make_zip({ stored_entry("value", "1") }), ignored_bytes_read),
      memory_context);
  if (!memory_source)
  {
    return false;
  }
  auto index_too_large =
      xmole2::zip::ZipArchive::open(std::move(*memory_source), memory_context);
  if (index_too_large ||
      index_too_large.error().code !=
          static_cast<std::uint32_t>(xmole2::zip::ZipErrorCode::ResourceLimitExceeded))
  {
    return false;
  }

  auto const context = xmole2::OperationContext {};
  auto archive = open_archive(make_zip({ stored_entry("value", "12345") }), context);
  if (!archive)
  {
    return false;
  }
  auto read_context                             = xmole2::OperationContext {};
  read_context.budget.max_single_resource_bytes = 4;
  auto limited                                  = archive->open_entry(0, read_context);
  if (limited ||
      limited.error().code !=
          static_cast<std::uint32_t>(xmole2::zip::ZipErrorCode::ResourceLimitExceeded))
  {
    return false;
  }

  auto reader = archive->open_entry(0, context);
  if (!reader)
  {
    return false;
  }
  auto read_cancellation              = xmole2::CancellationSource {};
  auto cancelled_read_context         = xmole2::OperationContext {};
  cancelled_read_context.cancellation = read_cancellation.token();
  read_cancellation.request_cancellation();
  auto value = std::byte {};
  auto cancelled_read =
      reader->read(std::span<std::byte> { &value, 1 }, cancelled_read_context);
  if (cancelled_read ||
      cancelled_read.error().code !=
          static_cast<std::uint32_t>(xmole2::zip::ZipErrorCode::Cancelled))
  {
    return false;
  }

  auto cancellation              = xmole2::CancellationSource {};
  auto cancelled_context         = xmole2::OperationContext {};
  cancelled_context.cancellation = cancellation.token();
  cancellation.request_cancellation();
  auto cancelled = open_archive(make_zip({ stored_entry("a", "a") }), cancelled_context);
  return !cancelled &&
         cancelled.error().code ==
             static_cast<std::uint32_t>(xmole2::zip::ZipErrorCode::Cancelled);
}

auto test_malformed_names_duplicates_and_features() -> bool
{
  auto const context = xmole2::OperationContext {};
  auto malformed     = open_archive(bytes_from("not a zip"), context);
  if (malformed || malformed.error().domain != xmole2::ErrorDomain::Zip ||
      malformed.error().code !=
          static_cast<std::uint32_t>(xmole2::zip::ZipErrorCode::InvalidArchive))
  {
    return false;
  }

  auto duplicate = open_archive(
      make_zip({ stored_entry("same", "a"), stored_entry("same", "b") }), context);
  if (duplicate ||
      duplicate.error().code !=
          static_cast<std::uint32_t>(xmole2::zip::ZipErrorCode::DuplicateEntry))
  {
    return false;
  }

  auto traversal = open_archive(make_zip({ stored_entry("../escape", "a") }), context);
  if (traversal ||
      traversal.error().code !=
          static_cast<std::uint32_t>(xmole2::zip::ZipErrorCode::UnsafeEntryName))
  {
    return false;
  }

  auto encrypted_spec  = stored_entry("encrypted", "a");
  encrypted_spec.flags = 1;
  auto encrypted       = open_archive(make_zip({ encrypted_spec }), context);
  if (!encrypted)
  {
    return false;
  }
  auto encrypted_reader = encrypted->open_entry(0, context);
  if (encrypted_reader ||
      encrypted_reader.error().code !=
          static_cast<std::uint32_t>(xmole2::zip::ZipErrorCode::EncryptedEntry))
  {
    return false;
  }

  auto unsupported_spec   = stored_entry("unsupported", "a");
  unsupported_spec.method = 12;
  auto unsupported        = open_archive(make_zip({ unsupported_spec }), context);
  if (!unsupported)
  {
    return false;
  }
  auto unsupported_reader = unsupported->open_entry(0, context);
  return !unsupported_reader &&
         unsupported_reader.error().code ==
             static_cast<std::uint32_t>(
                 xmole2::zip::ZipErrorCode::UnsupportedCompression);
}

auto test_source_error_chain_is_preserved() -> bool
{
  auto const context = xmole2::OperationContext {};
  auto source =
      xmole2::io::SourceLease::acquire(std::make_shared<FaultSource>(), context);
  if (!source)
  {
    return false;
  }
  auto archive = xmole2::zip::ZipArchive::open(std::move(*source), context);
  return !archive && archive.error().domain == xmole2::ErrorDomain::Zip &&
         archive.error().cause != nullptr && archive.error().cause->native_code == 4321 &&
         archive.error().native_code.has_value();
}

} // namespace

auto main() -> int
{
  if (!test_index_is_lazy_and_searchable())
  {
    return 1;
  }
  if (!test_store_deflate_streaming_and_lifetime())
  {
    return 2;
  }
  if (!test_finish_and_integrity())
  {
    return 3;
  }
  if (!test_budget_and_cancellation())
  {
    return 4;
  }
  if (!test_malformed_names_duplicates_and_features())
  {
    return 5;
  }
  if (!test_source_error_chain_is_preserved())
  {
    return 6;
  }
  return 0;
}
