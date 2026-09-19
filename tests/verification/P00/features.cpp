#include <bit>
#include <concepts>
#include <cstdint>
#include <expected>
#include <span>
#include <tuple>

#ifndef __cpp_lib_expected
#error "P00 verifier requires std::expected"
#endif
static_assert(__cpp_lib_expected >= 202202L);
static_assert(std::byteswap(std::uint32_t{0x12345678}) == 0x78563412);
static_assert(std::bit_cast<std::uint32_t>(std::int32_t{-1}) == 0xffffffffU);
static_assert(std::endian::native == std::endian::little ||
              std::endian::native == std::endian::big);
template<std::integral T> constexpr T twice(T v) { return v + v; }
constexpr auto descriptor = std::tuple{2, 4, 8};
static_assert(twice(std::get<1>(descriptor)) == 8);
constexpr bool expected_works() {
    std::expected<unsigned, unsigned> value{7};
    std::expected<unsigned, unsigned> error{std::unexpected(3U)};
    return *value == 7 && !error && error.error() == 3;
}
static_assert(expected_works());
int main() {
    unsigned data[]{1, 2, 3};
    return std::span<unsigned>{data}.size() == 3 ? 0 : 1;
}
