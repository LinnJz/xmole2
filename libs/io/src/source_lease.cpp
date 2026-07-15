#include "xmole2/io/source_lease.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <memory>
#include <span>
#include <utility>
#include <vector>

#include "file_io.hpp"
#include "xmole2/io/byte_source.hpp"
#include "xmole2/io/error.hpp"
#include "xmole2/io/temporary_file.hpp"

namespace xmole2::io
{
namespace
{

constexpr auto kDetachBufferBytes = std::uint64_t { 1024 * 1024 };

auto cancelled_error() -> Error
{
  return make_error(IoErrorCode::Cancelled, "I/O operation cancelled");
}

auto resource_error(std::string_view const message) -> Error
{
  return make_error(IoErrorCode::ResourceLimitExceeded, message);
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

class MemoryByteSource final : public ByteSource
{
public:
  explicit MemoryByteSource(std::vector<std::byte> buffer)
      : m_buffer { std::move(buffer) }
  {
  }

  auto size(OperationContext const &context) const -> Result<std::uint64_t> override
  {
    if (context.cancellation.is_cancelled())
    {
      return std::unexpected { cancelled_error() };
    }
    auto const result     = static_cast<std::uint64_t>(m_buffer.size());
    auto const validation = validate_source_size(result, context);
    if (!validation)
    {
      return std::unexpected { validation.error() };
    }
    return result;
  }

  auto read_at(
      std::uint64_t const offset,
      std::span<std::byte> const destination,
      OperationContext const &context) const -> Result<std::size_t> override
  {
    if (context.cancellation.is_cancelled())
    {
      return std::unexpected { cancelled_error() };
    }
    if (destination.size() > context.budget.max_single_resource_bytes)
    {
      return std::unexpected { resource_error(
          "read request exceeds the resource budget") };
    }
    if (offset >= m_buffer.size() || destination.empty())
    {
      return std::size_t {};
    }

    auto const available = static_cast<std::uint64_t>(m_buffer.size()) - offset;
    auto const count =
        static_cast<std::size_t>(std::min<std::uint64_t>(available, destination.size()));
    std::memcpy(destination.data(), m_buffer.data() + offset, count);
    return count;
  }

private:
  std::vector<std::byte> m_buffer;
};

} // namespace

struct RandomAccessReader::Impl
{
  std::shared_ptr<ByteSource> source;
  std::uint64_t position {};
};

RandomAccessReader::RandomAccessReader() noexcept = default;

RandomAccessReader::RandomAccessReader(
    std::shared_ptr<ByteSource> source, std::uint64_t const offset)
    : m_impl { std::make_unique<Impl>(std::move(source), offset) }
{
}

RandomAccessReader::RandomAccessReader(RandomAccessReader &&other) noexcept = default;

auto RandomAccessReader::operator= (RandomAccessReader &&other) noexcept
    -> RandomAccessReader & = default;

RandomAccessReader::~RandomAccessReader() = default;

auto RandomAccessReader::read(
    std::span<std::byte> const destination, OperationContext const &context)
    -> Result<std::size_t>
{
  if (!m_impl)
  {
    return std::unexpected { make_error(
        IoErrorCode::SourceClosed, "reader has no source") };
  }
  if (context.cancellation.is_cancelled())
  {
    return std::unexpected { cancelled_error() };
  }
  if (destination.size() > context.budget.max_single_resource_bytes)
  {
    return std::unexpected { resource_error("read request exceeds the resource budget") };
  }

  auto result = m_impl->source->read_at(m_impl->position, destination, context);
  if (!result)
  {
    return result;
  }
  if (*result > destination.size())
  {
    return std::unexpected { make_error(
        IoErrorCode::ReadFailed, "byte source returned an invalid read length") };
  }
  if (*result > std::numeric_limits<std::uint64_t>::max() - m_impl->position)
  {
    return std::unexpected { make_error(
        IoErrorCode::ReadFailed, "reader position overflow") };
  }
  m_impl->position += *result;
  return result;
}

auto RandomAccessReader::read_exact(
    std::span<std::byte> const destination, OperationContext const &context) -> Status
{
  auto total_read = std::size_t {};
  while (total_read < destination.size())
  {
    auto const result = read(destination.subspan(total_read), context);
    if (!result)
    {
      return std::unexpected { result.error() };
    }
    if (*result == 0)
    {
      return std::unexpected { make_error(
          IoErrorCode::UnexpectedEndOfFile,
          "source ended before the requested bytes were read") };
    }
    total_read += *result;
  }
  return {};
}

auto RandomAccessReader::seek(std::uint64_t const offset, OperationContext const &context)
    -> Status
{
  auto const source_size = size(context);
  if (!source_size)
  {
    return std::unexpected { source_size.error() };
  }
  if (offset > *source_size)
  {
    return std::unexpected { make_error(
        IoErrorCode::OffsetOutOfRange, "reader offset is beyond the source") };
  }
  m_impl->position = offset;
  return {};
}

auto RandomAccessReader::position() const noexcept -> std::uint64_t
{
  return m_impl ? m_impl->position : 0;
}

auto RandomAccessReader::size(OperationContext const &context) const
    -> Result<std::uint64_t>
{
  if (!m_impl)
  {
    return std::unexpected { make_error(
        IoErrorCode::SourceClosed, "reader has no source") };
  }
  auto result = m_impl->source->size(context);
  if (!result)
  {
    return result;
  }
  auto const validation = validate_source_size(*result, context);
  if (!validation)
  {
    return std::unexpected { validation.error() };
  }
  return result;
}

struct SourceLease::Impl
{
  std::shared_ptr<ByteSource> source;
};

SourceLease::SourceLease() noexcept = default;

SourceLease::SourceLease(std::shared_ptr<ByteSource> source)
    : m_impl { std::make_unique<Impl>(std::move(source)) }
{
}

SourceLease::SourceLease(SourceLease &&other) noexcept = default;

auto SourceLease::operator= (SourceLease &&other) noexcept -> SourceLease & = default;

SourceLease::~SourceLease() = default;

auto SourceLease::open_file(
    std::filesystem::path const &path, OperationContext const &context)
    -> Result<SourceLease>
{
  auto source = internal::open_file_source(path, false, context);
  if (!source)
  {
    return std::unexpected { std::move(source.error()) };
  }
  return acquire(std::move(*source), context);
}

auto SourceLease::from_buffer(
    std::vector<std::byte> buffer, OperationContext const &context) -> Result<SourceLease>
{
  if (context.cancellation.is_cancelled())
  {
    return std::unexpected { cancelled_error() };
  }
  auto const size = static_cast<std::uint64_t>(buffer.size());
  auto validation = validate_source_size(size, context);
  if (!validation)
  {
    return std::unexpected { validation.error() };
  }
  if (size > context.budget.max_memory_bytes)
  {
    return std::unexpected { resource_error("owned buffer exceeds the memory budget") };
  }

  auto source = std::make_shared<MemoryByteSource>(std::move(buffer));
  return acquire(std::move(source), context);
}

auto SourceLease::acquire(
    std::unique_ptr<ByteSource> source, OperationContext const &context)
    -> Result<SourceLease>
{
  return acquire(std::shared_ptr<ByteSource> { std::move(source) }, context);
}

auto SourceLease::acquire(
    std::shared_ptr<ByteSource> source, OperationContext const &context)
    -> Result<SourceLease>
{
  if (context.cancellation.is_cancelled())
  {
    return std::unexpected { cancelled_error() };
  }
  if (!source)
  {
    return std::unexpected { make_error(
        IoErrorCode::InvalidArgument, "source must not be null") };
  }

  auto source_size = source->size(context);
  if (!source_size)
  {
    return std::unexpected { std::move(source_size.error()) };
  }
  auto validation = validate_source_size(*source_size, context);
  if (!validation)
  {
    return std::unexpected { validation.error() };
  }
  return SourceLease { std::move(source) };
}

auto SourceLease::valid() const noexcept -> bool
{
  return m_impl != nullptr && m_impl->source != nullptr;
}

auto SourceLease::size(OperationContext const &context) const -> Result<std::uint64_t>
{
  if (!valid())
  {
    return std::unexpected { make_error(
        IoErrorCode::SourceClosed, "source lease is empty") };
  }
  auto result = m_impl->source->size(context);
  if (!result)
  {
    return result;
  }
  auto const validation = validate_source_size(*result, context);
  if (!validation)
  {
    return std::unexpected { validation.error() };
  }
  return result;
}

auto SourceLease::reader(std::uint64_t const offset, OperationContext const &context)
    const -> Result<RandomAccessReader>
{
  auto source_size = size(context);
  if (!source_size)
  {
    return std::unexpected { std::move(source_size.error()) };
  }
  if (offset > *source_size)
  {
    return std::unexpected { make_error(
        IoErrorCode::OffsetOutOfRange, "reader offset is beyond the source") };
  }
  return RandomAccessReader { m_impl->source, offset };
}

auto SourceLease::detach(OperationContext const &context) -> Status
{
  auto source_size = size(context);
  if (!source_size)
  {
    return std::unexpected { std::move(source_size.error()) };
  }

  auto temporary = TemporaryFile::create(context);
  if (!temporary)
  {
    return std::unexpected { std::move(temporary.error()) };
  }

  auto reader_result = reader(0, context);
  if (!reader_result)
  {
    return std::unexpected { std::move(reader_result.error()) };
  }
  auto reader = std::move(*reader_result);

  auto const buffer_limit = std::min(
      { kDetachBufferBytes, context.budget.max_memory_bytes,
        context.budget.max_single_resource_bytes });
  if (*source_size > 0 && buffer_limit == 0)
  {
    return std::unexpected { resource_error("detach has no available memory budget") };
  }
  auto const buffer_size =
      static_cast<std::size_t>(std::min<std::uint64_t>(*source_size, buffer_limit));
  auto buffer         = std::vector<std::byte>(buffer_size);
  auto total_detached = std::uint64_t {};

  while (total_detached < *source_size)
  {
    if (context.cancellation.is_cancelled())
    {
      return std::unexpected { cancelled_error() };
    }

    auto const remaining = *source_size - total_detached;
    auto const chunk_size =
        static_cast<std::size_t>(std::min<std::uint64_t>(remaining, buffer.size()));
    auto const chunk = std::span<std::byte> { buffer }.first(chunk_size);
    auto status      = reader.read_exact(chunk, context);
    if (!status)
    {
      return status;
    }
    status = temporary->sink().write(chunk, context);
    if (!status)
    {
      return status;
    }
    total_detached += chunk_size;

    if (context.progress != nullptr)
    {
      context.progress->report(
          ProgressUpdate { "io.detach", total_detached, *source_size });
    }
  }

  auto detached = std::move(*temporary).seal(context);
  if (!detached)
  {
    return std::unexpected { std::move(detached.error()) };
  }

  *this = std::move(*detached);
  return {};
}

} // namespace xmole2::io
