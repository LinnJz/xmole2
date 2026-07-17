#pragma once

#include <cstdint>
#include <span>
#include <string_view>

#include "xmole2/cfb/compound_file_header.hpp"
#include "xmole2/cfb/error.hpp"

namespace xmole2::cfb::internal
{

[[nodiscard]] auto read_sector(
    io::SourceLease const &source,
    CompoundFileHeader const &header,
    std::uint32_t sector,
    std::span<std::byte> destination,
    OperationContext const &context,
    CfbErrorCode unexpected_end_code,
    std::string_view failure_message) -> Status;

} // namespace xmole2::cfb::internal
