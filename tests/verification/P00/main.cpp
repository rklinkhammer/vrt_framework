#include "policies.hpp"
#include <type_traits>
static_assert(!std::is_same_v<vita::capacity_policy<7>, vita::capacity_policy<4096>>);
int main() {
    if (small_policy_capacity() != 7 || large_policy_capacity() != 4096) return 1;
    if (version_address_a() != version_address_b()) return 2;
    return vita::version_major == 0 && vita::version_minor == 1 ? 0 : 3;
}
