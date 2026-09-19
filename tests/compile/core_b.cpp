#include <vita/core/capacity_policy.hpp>
#include <vita/core/version.hpp>
unsigned core_a() noexcept;
int main() { return core_a() == 3 && vita::capacity_policy<4>::capacity == 4 && vita::version_major == 0 ? 0 : 1; }
