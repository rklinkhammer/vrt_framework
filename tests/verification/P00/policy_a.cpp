#include "policies.hpp"
static_assert(vita::capacity_policy<7>::capacity == 7);
std::size_t small_policy_capacity() noexcept { return vita::capacity_policy<7>::capacity; }
const void* version_address_a() noexcept { return &vita::version_minor; }
