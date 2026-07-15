#include <algorithm>
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

#include "xmole2/io/byte_source.hpp"
#include "xmole2/io/error.hpp"
#include "xmole2/io/source_lease.hpp"

namespace
{

auto make_bytes(std::string_view const text) -> std::vector<std::byte>
{
  auto bytes = std::vector<std::byte>(text.size());
  for (std::size_t index = 0; index < text.size(); ++index)
  {
    bytes[index] = static_cast<std::byte>(text[index]);
  }
  return bytes;
}

auto as_string(std::span<std::byte const> const bytes) -> std::string
{
  auto text = std::string(bytes.size(), '\0');
  for (std::size_t index = 0; index < bytes.size(); ++index)
  {
    text[index] = static_cast<char>(bytes[index]);
  }
  return text;
}

class FaultSource final : public xmole2::io::ByteSource
{
public:
  auto size(xmole2::OperationContext const &) const
      -> xmole2::Result<std::uint64_t> override
  {
    return 4;
  }

  auto read_at(std::uint64_t, std::span<std::byte>, xmole2::OperationContext const &)
      const -> xmole2::Result<std::size_t> override
  {
    auto cause = xmole2::make_error(xmole2::ErrorDomain::Io, 901, "native read failure");
    cause.native_code = 1234;

    auto error = xmole2::io::make_error(
        xmole2::io::IoErrorCode::ReadFailed, "fault source read failed");
    error.cause = std::make_shared<xmole2::Error const>(std::move(cause));
    return std::unexpected { std::move(error) };
  }
};

class InvalidLengthSource final : public xmole2::io::ByteSource
{
public:
  auto size(xmole2::OperationContext const &) const
      -> xmole2::Result<std::uint64_t> override
  {
    return 4;
  }

  auto read_at(
      std::uint64_t,
      std::span<std::byte> destination,
      xmole2::OperationContext const &) const -> xmole2::Result<std::size_t> override
  {
    return destination.size() + 1;
  }
};

class CountingSizeSource final : public xmole2::io::ByteSource
{
public:
  auto size(xmole2::OperationContext const &) const
      -> xmole2::Result<std::uint64_t> override
  {
    m_size_call_count.fetch_add(1, std::memory_order_relaxed);
    return 4;
  }

  auto read_at(
      std::uint64_t const offset,
      std::span<std::byte> destination,
      xmole2::OperationContext const &) const -> xmole2::Result<std::size_t> override
  {
    auto const bytes = make_bytes("size");
    if (offset >= bytes.size() || destination.empty())
    {
      return std::size_t {};
    }
    auto const count = std::min<std::size_t>(
        destination.size(), bytes.size() - static_cast<std::size_t>(offset));
    std::copy_n(
        bytes.begin() + static_cast<std::size_t>(offset), count, destination.begin());
    return count;
  }

  [[nodiscard]] auto size_call_count() const noexcept -> std::size_t
  {
    return m_size_call_count.load(std::memory_order_relaxed);
  }

private:
  mutable std::atomic_size_t m_size_call_count {};
};

class LargeOffsetSource final : public xmole2::io::ByteSource
{
public:
  static constexpr auto kMarkerOffset = (std::uint64_t { 1 } << 32U) + 17;
  static constexpr auto kSourceSize   = kMarkerOffset + 1;

  explicit LargeOffsetSource(std::shared_ptr<std::atomic_uint64_t> observed_offset)
      : m_observed_offset { std::move(observed_offset) }
  {
  }

  auto size(xmole2::OperationContext const &) const
      -> xmole2::Result<std::uint64_t> override
  {
    return kSourceSize;
  }

  auto read_at(
      std::uint64_t const offset,
      std::span<std::byte> destination,
      xmole2::OperationContext const &) const -> xmole2::Result<std::size_t> override
  {
    m_observed_offset->store(offset, std::memory_order_relaxed);
    if (offset != kMarkerOffset || destination.empty())
    {
      return std::size_t {};
    }
    destination[0] = std::byte { 0x5a };
    return 1;
  }

private:
  std::shared_ptr<std::atomic_uint64_t> m_observed_offset;
};

class MoveOnlyOwnedSource final : public xmole2::io::ByteSource
{
public:
  explicit MoveOnlyOwnedSource(std::shared_ptr<bool> destroyed)
      : m_destroyed { std::move(destroyed) }
  {
  }

  MoveOnlyOwnedSource(MoveOnlyOwnedSource const &)                      = delete;
  auto operator= (MoveOnlyOwnedSource const &) -> MoveOnlyOwnedSource & = delete;

  ~MoveOnlyOwnedSource() override { *m_destroyed = true; }

  auto size(xmole2::OperationContext const &) const
      -> xmole2::Result<std::uint64_t> override
  {
    return 3;
  }

  auto read_at(
      std::uint64_t const offset,
      std::span<std::byte> destination,
      xmole2::OperationContext const &) const -> xmole2::Result<std::size_t> override
  {
    auto const bytes = make_bytes("own");
    if (offset >= bytes.size() || destination.empty())
    {
      return std::size_t {};
    }
    auto const count = std::min<std::size_t>(
        destination.size(), bytes.size() - static_cast<std::size_t>(offset));
    std::copy_n(
        bytes.begin() + static_cast<std::size_t>(offset), count, destination.begin());
    return count;
  }

private:
  std::shared_ptr<bool> m_destroyed;
};

class CancellingProgressSink final : public xmole2::ProgressSink
{
public:
  explicit CancellingProgressSink(xmole2::CancellationSource &source)
      : m_source { source }
  {
  }

  auto report(xmole2::ProgressUpdate const &) -> void override
  {
    m_source.request_cancellation();
  }

private:
  xmole2::CancellationSource &m_source;
};

auto test_memory_source_and_random_reader() -> bool
{
  auto const context = xmole2::OperationContext {};
  auto lease_result = xmole2::io::SourceLease::from_buffer(make_bytes("abcdef"), context);
  if (!lease_result)
  {
    return false;
  }

  auto lease         = std::move(*lease_result);
  auto reader_result = lease.reader(2, context);
  if (!reader_result)
  {
    return false;
  }

  auto reader = std::move(*reader_result);
  lease       = xmole2::io::SourceLease {};
  auto bytes  = std::array<std::byte, 3> {};
  if (!reader.read_exact(bytes, context) || as_string(bytes) != "cde" ||
      reader.position() != 5)
  {
    return false;
  }

  auto tail = std::array<std::byte, 2> {};
  auto read = reader.read(tail, context);
  if (!read || *read != 1 || as_string(std::span { tail }.first(1)) != "f")
  {
    return false;
  }

  auto const eof = reader.read(tail, context);
  if (!eof || *eof != 0)
  {
    return false;
  }

  auto exact = reader.read_exact(tail, context);
  if (exact ||
      exact.error().code !=
          static_cast<std::uint32_t>(xmole2::io::IoErrorCode::UnexpectedEndOfFile))
  {
    return false;
  }

  auto const seek = reader.seek(7, context);
  return !seek &&
         seek.error().code ==
             static_cast<std::uint32_t>(xmole2::io::IoErrorCode::OffsetOutOfRange);
}

auto test_budget_and_cancellation() -> bool
{
  auto limited_context                   = xmole2::OperationContext {};
  limited_context.budget.max_input_bytes = 3;
  auto limited =
      xmole2::io::SourceLease::from_buffer(make_bytes("four"), limited_context);
  if (limited ||
      limited.error().code !=
          static_cast<std::uint32_t>(xmole2::io::IoErrorCode::ResourceLimitExceeded))
  {
    return false;
  }

  auto single_resource_context                             = xmole2::OperationContext {};
  single_resource_context.budget.max_single_resource_bytes = 3;
  auto single_resource =
      xmole2::io::SourceLease::from_buffer(make_bytes("four"), single_resource_context);
  if (single_resource ||
      single_resource.error().code !=
          static_cast<std::uint32_t>(xmole2::io::IoErrorCode::ResourceLimitExceeded))
  {
    return false;
  }

  auto memory_context                    = xmole2::OperationContext {};
  memory_context.budget.max_memory_bytes = 3;
  auto memory = xmole2::io::SourceLease::from_buffer(make_bytes("four"), memory_context);
  if (memory ||
      memory.error().code !=
          static_cast<std::uint32_t>(xmole2::io::IoErrorCode::ResourceLimitExceeded))
  {
    return false;
  }

  auto cancellation    = xmole2::CancellationSource {};
  auto context         = xmole2::OperationContext {};
  context.cancellation = cancellation.token();
  cancellation.request_cancellation();

  auto cancelled = xmole2::io::SourceLease::from_buffer(make_bytes("data"), context);
  return !cancelled &&
         cancelled.error().code ==
             static_cast<std::uint32_t>(xmole2::io::IoErrorCode::Cancelled);
}

auto test_stable_size_is_queried_once() -> bool
{
  auto source        = std::make_shared<CountingSizeSource>();
  auto const context = xmole2::OperationContext {};
  auto lease         = xmole2::io::SourceLease::acquire(source, context);
  if (!lease || source->size_call_count() != 1 || !lease->size(context) ||
      !lease->size(context))
  {
    return false;
  }

  auto reader = lease->reader(0, context);
  return reader && reader->size(context) && reader->seek(2, context) &&
         source->size_call_count() == 1;
}

auto test_independent_readers_are_concurrent() -> bool
{
  auto const context = xmole2::OperationContext {};
  auto lease = xmole2::io::SourceLease::from_buffer(make_bytes("parallel"), context);
  if (!lease)
  {
    return false;
  }

  constexpr auto kThreadCount = std::size_t { 4 };
  auto succeeded              = std::array<std::atomic_bool, kThreadCount> {};
  auto workers                = std::vector<std::jthread> {};
  workers.reserve(kThreadCount);
  for (auto index = std::size_t {}; index < kThreadCount; ++index)
  {
    workers.emplace_back([&lease, &context, &succeeded, index]
    {
      auto reader = lease->reader(0, context);
      auto bytes  = std::array<std::byte, 8> {};
      succeeded[index].store(
          reader && reader->read_exact(bytes, context) && as_string(bytes) == "parallel",
          std::memory_order_relaxed);
    });
  }
  workers.clear();

  return std::all_of(succeeded.begin(), succeeded.end(), [](auto const &value) {
    return value.load(std::memory_order_relaxed);
  });
}

auto test_64_bit_offset() -> bool
{
  auto observed_offset           = std::make_shared<std::atomic_uint64_t>();
  auto source                    = std::make_shared<LargeOffsetSource>(observed_offset);
  auto context                   = xmole2::OperationContext {};
  context.budget.max_input_bytes = LargeOffsetSource::kSourceSize;
  context.budget.max_single_resource_bytes = LargeOffsetSource::kSourceSize;

  auto lease = xmole2::io::SourceLease::acquire(source, context);
  if (!lease)
  {
    return false;
  }
  auto reader = lease->reader(LargeOffsetSource::kMarkerOffset, context);
  auto marker = std::byte {};
  return reader && reader->read_exact(std::span<std::byte> { &marker, 1 }, context) &&
         marker == std::byte { 0x5a } &&
         observed_offset->load(std::memory_order_relaxed) ==
             LargeOffsetSource::kMarkerOffset;
}

auto test_fault_error_chain_is_preserved() -> bool
{
  auto const context = xmole2::OperationContext {};
  auto lease_result =
      xmole2::io::SourceLease::acquire(std::make_shared<FaultSource>(), context);
  if (!lease_result)
  {
    return false;
  }

  auto reader_result = lease_result->reader(0, context);
  if (!reader_result)
  {
    return false;
  }

  auto buffer = std::array<std::byte, 1> {};
  auto result = reader_result->read(buffer, context);
  return !result && result.error().cause != nullptr &&
         result.error().cause->native_code == 1234;
}

auto test_invalid_source_contract_is_rejected() -> bool
{
  auto const context = xmole2::OperationContext {};
  auto lease =
      xmole2::io::SourceLease::acquire(std::make_shared<InvalidLengthSource>(), context);
  if (!lease)
  {
    return false;
  }

  auto reader = lease->reader(0, context);
  if (!reader)
  {
    return false;
  }

  auto buffer = std::array<std::byte, 1> {};
  auto result = reader->read(buffer, context);
  return !result && result.error().code ==
                        static_cast<std::uint32_t>(xmole2::io::IoErrorCode::ReadFailed);
}

auto test_unique_source_ownership_outlives_lease() -> bool
{
  auto destroyed = std::make_shared<bool>(false);
  auto source    = std::unique_ptr<xmole2::io::ByteSource> {
    std::make_unique<MoveOnlyOwnedSource>(destroyed)
  };
  auto const context = xmole2::OperationContext {};
  auto lease         = xmole2::io::SourceLease::acquire(std::move(source), context);
  if (!lease || source != nullptr || *destroyed)
  {
    return false;
  }

  auto reader = lease->reader(0, context);
  if (!reader)
  {
    return false;
  }
  *lease = xmole2::io::SourceLease {};

  auto bytes = std::array<std::byte, 3> {};
  if (*destroyed || !reader->read_exact(bytes, context) || as_string(bytes) != "own")
  {
    return false;
  }
  *reader = xmole2::io::RandomAccessReader {};
  return *destroyed;
}

auto test_shared_source_ownership_outlives_lease() -> bool
{
  auto destroyed     = std::make_shared<bool>(false);
  auto source        = std::make_shared<MoveOnlyOwnedSource>(destroyed);
  auto weak          = std::weak_ptr<MoveOnlyOwnedSource> { source };
  auto const context = xmole2::OperationContext {};
  auto lease         = xmole2::io::SourceLease::acquire(source, context);
  source.reset();
  if (!lease || weak.expired() || *destroyed)
  {
    return false;
  }

  auto reader = lease->reader(0, context);
  if (!reader)
  {
    return false;
  }
  *lease = xmole2::io::SourceLease {};
  if (weak.expired() || *destroyed)
  {
    return false;
  }

  *reader = xmole2::io::RandomAccessReader {};
  return weak.expired() && *destroyed;
}

auto test_detach_cancellation_preserves_source() -> bool
{
  auto buffer                = std::vector<std::byte>((1024 * 1024) + 1, std::byte { 7 });
  auto const initial_context = xmole2::OperationContext {};
  auto lease = xmole2::io::SourceLease::from_buffer(std::move(buffer), initial_context);
  if (!lease)
  {
    return false;
  }

  auto cancellation    = xmole2::CancellationSource {};
  auto progress        = CancellingProgressSink { cancellation };
  auto context         = xmole2::OperationContext {};
  context.cancellation = cancellation.token();
  context.progress     = &progress;

  auto detached = lease->detach(context);
  if (detached ||
      detached.error().code !=
          static_cast<std::uint32_t>(xmole2::io::IoErrorCode::Cancelled))
  {
    return false;
  }

  auto reader = lease->reader(0, initial_context);
  auto value  = std::byte {};
  return reader &&
         reader->read_exact(std::span<std::byte> { &value, 1 }, initial_context) &&
         value == std::byte { 7 };
}

} // namespace

auto run_source_contract_tests() -> bool
{
  return test_memory_source_and_random_reader() && test_budget_and_cancellation() &&
         test_stable_size_is_queried_once() &&
         test_independent_readers_are_concurrent() && test_64_bit_offset() &&
         test_fault_error_chain_is_preserved() &&
         test_invalid_source_contract_is_rejected() &&
         test_unique_source_ownership_outlives_lease() &&
         test_shared_source_ownership_outlives_lease() &&
         test_detach_cancellation_preserves_source();
}
