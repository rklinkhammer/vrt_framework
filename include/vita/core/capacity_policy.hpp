#pragma once

#include <cstddef>

namespace vita {
// Explicit capacity in the type system; no per-translation-unit layout macros.
template<std::size_t Capacity>
struct capacity_policy {
    static_assert(Capacity > 0, "Capacity must be positive");
    static constexpr std::size_t capacity = Capacity;
};
} // namespace vita
