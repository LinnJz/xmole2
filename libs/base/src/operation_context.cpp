#include "xmole2/base/operation_context.hpp"

#include <atomic>
#include <mutex>
#include <utility>
#include <vector>

namespace xmole2
{

class CancellationState
{
public:
  std::atomic_size_t reference_count { 1 };
  std::atomic_bool cancelled { false };
};

struct CollectingDiagnosticSink::Impl
{
  mutable std::mutex mutex;
  std::vector<Error> diagnostics;
};

namespace
{

auto retain(CancellationState *state) noexcept -> void
{
  if (state != nullptr)
  {
    state->reference_count.fetch_add(1, std::memory_order_relaxed);
  }
}

auto release(CancellationState *state) noexcept -> void
{
  if (state != nullptr &&
      state->reference_count.fetch_sub(1, std::memory_order_acq_rel) == 1)
  {
    delete state;
  }
}

} // namespace

CancellationToken::CancellationToken(CancellationState *state) noexcept
    : m_state { state }
{
  retain(m_state);
}

CancellationToken::CancellationToken(CancellationToken const &other) noexcept
    : m_state { other.m_state }
{
  retain(m_state);
}

auto CancellationToken::operator= (CancellationToken const &other) noexcept
    -> CancellationToken &
{
  retain(other.m_state);
  release(m_state);
  m_state = other.m_state;
  return *this;
}

CancellationToken::CancellationToken(CancellationToken &&other) noexcept
    : m_state { std::exchange(other.m_state, nullptr) }
{
}

auto CancellationToken::operator= (CancellationToken &&other) noexcept
    -> CancellationToken &
{
  if (this != &other)
  {
    release(m_state);
    m_state = std::exchange(other.m_state, nullptr);
  }
  return *this;
}

CancellationToken::~CancellationToken()
{
  release(m_state);
}

auto CancellationToken::is_cancelled() const noexcept -> bool
{
  return m_state != nullptr && m_state->cancelled.load(std::memory_order_acquire);
}

CancellationSource::CancellationSource()
    : m_state { new CancellationState {} }
{
}

CancellationSource::CancellationSource(CancellationSource const &other) noexcept
    : m_state { other.m_state }
{
  retain(m_state);
}

auto CancellationSource::operator= (CancellationSource const &other) noexcept
    -> CancellationSource &
{
  retain(other.m_state);
  release(m_state);
  m_state = other.m_state;
  return *this;
}

CancellationSource::CancellationSource(CancellationSource &&other) noexcept
    : m_state { std::exchange(other.m_state, nullptr) }
{
}

auto CancellationSource::operator= (CancellationSource &&other) noexcept
    -> CancellationSource &
{
  if (this != &other)
  {
    release(m_state);
    m_state = std::exchange(other.m_state, nullptr);
  }
  return *this;
}

CancellationSource::~CancellationSource()
{
  release(m_state);
}

auto CancellationSource::token() const noexcept -> CancellationToken
{
  return CancellationToken { m_state };
}

auto CancellationSource::request_cancellation() noexcept -> void
{
  if (m_state != nullptr)
  {
    m_state->cancelled.store(true, std::memory_order_release);
  }
}

CollectingDiagnosticSink::CollectingDiagnosticSink()
    : m_impl { std::make_unique<Impl>() }
{
}

CollectingDiagnosticSink::CollectingDiagnosticSink(CollectingDiagnosticSink &&other)
    : m_impl { std::make_unique<Impl>() }
{
  m_impl.swap(other.m_impl);
}

auto CollectingDiagnosticSink::operator= (CollectingDiagnosticSink &&other)
    -> CollectingDiagnosticSink &
{
  if (this != &other)
  {
    auto replacement = std::make_unique<Impl>();
    m_impl           = std::move(other.m_impl);
    other.m_impl     = std::move(replacement);
  }
  return *this;
}

CollectingDiagnosticSink::~CollectingDiagnosticSink() = default;

auto CollectingDiagnosticSink::report(Error const &diagnostic) -> void
{
  auto const lock = std::lock_guard { m_impl->mutex };
  m_impl->diagnostics.push_back(diagnostic);
}

auto CollectingDiagnosticSink::snapshot() const -> std::vector<Error>
{
  auto const lock = std::lock_guard { m_impl->mutex };
  return m_impl->diagnostics;
}

auto CollectingDiagnosticSink::take() -> std::vector<Error>
{
  auto const lock  = std::lock_guard { m_impl->mutex };
  auto diagnostics = std::move(m_impl->diagnostics);
  m_impl->diagnostics.clear();
  return diagnostics;
}

auto CollectingDiagnosticSink::clear() -> void
{
  auto const lock = std::lock_guard { m_impl->mutex };
  m_impl->diagnostics.clear();
}

auto CollectingDiagnosticSink::size() const -> std::size_t
{
  auto const lock = std::lock_guard { m_impl->mutex };
  return m_impl->diagnostics.size();
}

auto CollectingDiagnosticSink::empty() const -> bool
{
  return size() == 0;
}

} // namespace xmole2
