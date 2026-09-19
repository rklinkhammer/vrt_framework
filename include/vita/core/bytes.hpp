#pragma once
#include <cstddef>
#include <span>
namespace vita {
using Bytes = std::span<const std::byte>;
using MutableBytes = std::span<std::byte>;
} // namespace vita
