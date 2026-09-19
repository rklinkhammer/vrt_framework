#include "policies.hpp"
static_assert(vita::capacity_policy<4096>::capacity == 4096);
std::size_t large_policy_capacity() noexcept { return vita::capacity_policy<4096>::capacity; }
const void* version_address_b() noexcept { return &vita::version_minor; }
