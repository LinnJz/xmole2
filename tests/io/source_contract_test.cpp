#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <string_view>
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

  auto cancellation    = xmole2::CancellationSource {};
  auto context         = xmole2::OperationContext {};
  context.cancellation = cancellation.token();
  cancellation.request_cancellation();

  auto cancelled = xmole2::io::SourceLease::from_buffer(make_bytes("data"), context);
  return !cancelled &&
         cancelled.error().code ==
             static_cast<std::uint32_t>(xmole2::io::IoErrorCode::Cancelled);
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
         test_fault_error_chain_is_preserved() &&
         test_invalid_source_contract_is_rejected() &&
         test_detach_cancellation_preserves_source();
}
