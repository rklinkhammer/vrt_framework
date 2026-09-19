#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
template<std::size_t N> constexpr auto wire_bytes(const std::array<std::uint32_t,N>& words) {
    std::array<std::byte,N*4> out{};
    for(std::size_t i=0;i<N;++i)for(unsigned j=0;j<4;++j)out[i*4+j]=std::byte((words[i]>>(24-j*8))&255);
    return out;
}
inline constexpr auto w1=wire_bytes(std::array<std::uint32_t,7>{0x60000007,1,0xa0040000,1,2,3,0x00200000});
inline constexpr auto w2=wire_bytes(std::array<std::uint32_t,9>{0x60000009,1,0xa90b0000,2,2,3,0x00200000,0x000000f4,0x24000000});
inline constexpr auto w3=wire_bytes(std::array<std::uint32_t,7>{0x61000007,1,0xa9080000,2,2,3,0x00200000});
inline constexpr auto w4=wire_bytes(std::array<std::uint32_t,6>{0x64000006,1,0xa9080400,2,2,3});
inline constexpr auto w5=wire_bytes(std::array<std::uint32_t,4>{0x10000004,1,0x40000000,0x3b21187e});
