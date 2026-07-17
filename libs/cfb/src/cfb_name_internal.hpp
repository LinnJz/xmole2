#pragma once

#include <compare>
#include <string_view>

namespace xmole2::cfb::internal
{

/// Compares CFB directory names using MS-CFB 2.6.4 ordering.
auto compare_directory_names(std::u16string_view left, std::u16string_view right)
    -> std::strong_ordering;

} // namespace xmole2::cfb::internal
