#include <vita/core/capacity_policy.hpp>
#include <vita/core/version.hpp>
unsigned core_a() noexcept { return vita::version_minor + vita::capacity_policy<2>::capacity; }
