#include <vita/core/bytes.hpp>
#include <vita/core/error.hpp>
#include <vita/core/fixed_vector.hpp>
#include <cstdint>
#include <type_traits>

struct Lifetime {
    int* live;
    explicit Lifetime(int& count) noexcept : live(&count) { ++*live; }
    Lifetime(const Lifetime&) = delete;
    Lifetime& operator=(const Lifetime&) = delete;
    Lifetime(Lifetime&& other) noexcept : live(other.live) { other.live = nullptr; }
    ~Lifetime() { if (live) --*live; }
};
static_assert(!std::is_copy_constructible_v<vita::FixedVector<Lifetime, 2>>);
static_assert(std::is_nothrow_move_constructible_v<vita::FixedVector<Lifetime, 2>>);
static_assert(std::is_same_v<vita::Bytes::element_type, const std::byte>);
int main() {
    int live = 0;
    {
        vita::FixedVector<Lifetime, 2> values;
        if (!values.empty() || values.capacity() != 2) return 1;
        if (!values.emplace_back(live) || !values.emplace_back(live) || live != 2) return 2;
        auto full = values.emplace_back(live);
        if (full || full.error().code != vita::ErrorCode::capacity_exhausted || live != 2) return 3;
        if (values.size() != 2 || !values.full()) return 4;
        auto moved = std::move(values);
        if (live != 2 || moved.size() != 2) return 5;
        moved.pop_back();
        if (live != 1 || moved.size() != 1) return 6;
        moved.clear();
        if (live != 0 || !moved.empty()) return 7;
    }
    if (live != 0) return 8;
    vita::FixedVector<std::uint32_t, 0> zero;
    if (zero.push_back(1) || !zero.empty() || !zero.full()) return 9;
    return 0;
}
