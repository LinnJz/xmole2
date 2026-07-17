#include "cfb_name_internal.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>

#include "unicode_simple_uppercase_generated.hpp"

namespace xmole2::cfb::internal
{
namespace
{

auto simple_uppercase(char16_t const value) -> char16_t
{
  auto const numeric_value = static_cast<std::uint16_t>(value);
  auto const segment       = std::lower_bound(
      kSimpleUppercaseSegments.begin(), kSimpleUppercaseSegments.end(), numeric_value,
      [](SimpleUppercaseSegment const &candidate, std::uint16_t const candidate_value)
  { return candidate.last < candidate_value; });
  if (segment == kSimpleUppercaseSegments.end() || numeric_value < segment->first ||
      ((numeric_value - segment->first) % segment->step) != 0)
  {
    return value;
  }
  return static_cast<char16_t>(static_cast<std::int32_t>(numeric_value) + segment->delta);
}

} // namespace

auto compare_directory_names(
    std::u16string_view const left, std::u16string_view const right)
    -> std::strong_ordering
{
  if (left.size() != right.size())
  {
    return left.size() <=> right.size();
  }
  for (auto index = std::size_t {}; index < left.size(); ++index)
  {
    auto const left_upper  = simple_uppercase(left[index]);
    auto const right_upper = simple_uppercase(right[index]);
    if (left_upper != right_upper)
    {
      return left_upper <=> right_upper;
    }
  }
  return std::strong_ordering::equal;
}

} // namespace xmole2::cfb::internal
