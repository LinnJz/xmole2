#include "xmole2/base/operation_context.hpp"

#include <atomic>
#include <utility>

namespace xmole2
{

class CancellationState
{
public:
  std::atomic_size_t reference_count { 1 };
  std::atomic_bool cancelled { false };
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
  return m_state != nullptr && m_state->cancelled.load(std::memory_order_relaxed);
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
    m_state->cancelled.store(true, std::memory_order_relaxed);
  }
}

} // namespace xmole2
