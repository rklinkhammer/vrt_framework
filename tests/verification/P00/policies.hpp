#pragma once
#include <vita/core/capacity_policy.hpp>
#include <vita/core/version.hpp>
#include <cstddef>

std::size_t small_policy_capacity() noexcept;
std::size_t large_policy_capacity() noexcept;
const void* version_address_a() noexcept;
const void* version_address_b() noexcept;
