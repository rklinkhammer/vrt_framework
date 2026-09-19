#pragma once
#include <array>
#include <cstdint>
namespace vita {
// Ordered native words, matching the four-word UUID representation in §8.2.
// A UUID field's storage is owned by its packet's bounded native arena.
struct UuidValue {
    std::array<std::uint32_t,4> words{};
    friend constexpr bool operator==(const UuidValue&,const UuidValue&) = default;
};
struct CountryCodeValue {
    std::uint16_t code = 0;
    bool iso3166 = false;
    friend constexpr bool operator==(CountryCodeValue,CountryCodeValue) = default;
};
struct EmsDeviceClassValue {
    std::uint16_t class_code = 0;
    std::uint8_t organization = 0;
    bool exciter = false, receiver = false;
    friend constexpr bool operator==(EmsDeviceClassValue,EmsDeviceClassValue) = default;
};
} // namespace vita
