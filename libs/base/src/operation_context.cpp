#include "xmole2/base/operation_context.hpp"

#include <utility>

namespace xmole2
{

CancellationToken::CancellationToken(
    std::shared_ptr<std::atomic_bool const> state) noexcept
    : m_state { std::move(state) }
{
}

auto CancellationToken::is_cancelled() const noexcept -> bool
{
  return m_state && m_state->load(std::memory_order_relaxed);
}

CancellationSource::CancellationSource()
    : m_state { std::make_shared<std::atomic_bool>(false) }
{
}

auto CancellationSource::token() const noexcept -> CancellationToken
{
  return CancellationToken { m_state };
}

auto CancellationSource::request_cancellation() noexcept -> void
{
  m_state->store(true, std::memory_order_relaxed);
}

} // namespace xmole2

