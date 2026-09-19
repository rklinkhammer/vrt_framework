#pragma once
#include <vita/core/error.hpp>
#include <array>
#include <span>
#include <cstdint>
namespace vita {
// Native variable semantic values only; never encoded packet or transport storage.
template<std::size_t N> class SemanticArena {
    std::array<std::uint32_t,N> words_{};
    std::size_t used_ = 0;
public:
    struct Slice { std::size_t offset, count; };
    constexpr Result<Slice> append(std::span<const std::uint32_t> words) noexcept {
        if (words.size() > N - used_) return std::unexpected(Error{ErrorCode::capacity_exhausted, used_, words.size()});
        const Slice slice{used_,words.size()};
        for (auto word : words) words_[used_++] = word;
        return slice;
    }
    constexpr Result<std::span<const std::uint32_t>> view(Slice slice) const noexcept {
        if (slice.offset > used_ || slice.count > used_ - slice.offset) return std::unexpected(Error{ErrorCode::invalid_argument});
        return std::span<const std::uint32_t>{words_.data()+slice.offset,slice.count};
    }
    constexpr std::size_t size() const noexcept { return used_; }
};
} // namespace vita
