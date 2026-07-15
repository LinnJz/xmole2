#include <cstddef>
#include <span>
#include <vector>

#include "xmole2/io/source_lease.hpp"

auto main() -> int
{
  auto const context = xmole2::OperationContext {};
  auto source        = xmole2::io::SourceLease::from_buffer(
      std::vector<std::byte> { std::byte { 42 } }, context);
  if (!source)
  {
    return 1;
  }

  auto reader = source->reader(0, context);
  if (!reader)
  {
    return 1;
  }

  auto value = std::byte {};
  auto read  = reader->read_exact(std::span<std::byte> { &value, 1 }, context);
  return read && value == std::byte { 42 } ? 0 : 1;
}
