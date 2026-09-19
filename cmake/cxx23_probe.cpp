#include <array>
#include <bit>
#include <concepts>
#include <cstdint>
#include <expected>
#include <span>
static_assert(__cplusplus > 202002L);
static_assert(std::same_as<std::uint32_t, decltype(std::byteswap(std::uint32_t{}))>);
constexpr std::array values{1, 2};
constexpr std::span<const int> view{values};
static_assert(view[1] == 2);
constexpr std::expected<int, int> result{42};
static_assert(result.value() == 42);
static_assert(std::bit_cast<std::uint32_t>(std::int32_t{1}) == 1);
static_assert(std::byteswap(std::uint32_t{0x12345678}) == 0x78563412);
static_assert(std::endian::native == std::endian::little || std::endian::native == std::endian::big);
int main() { return *result == 42 ? 0 : 1; }
