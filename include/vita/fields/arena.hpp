#pragma once
#include <vita/core/error.hpp>
#include <vita/core/bytes.hpp>
#include <cstring>
#include <type_traits>
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
namespace detail {
template<std::size_t N> class NativeArenaStorage {
    std::array<std::byte,N> bytes_{};
    std::size_t used_ = 0;
public:
    static_assert(N<=UINT32_MAX);
    constexpr void clear() noexcept { used_=0; }
    std::size_t size() const noexcept { return used_; }
    Result<std::uint32_t> append(Bytes bytes) noexcept {
        if(bytes.size()>N-used_)return std::unexpected(Error{ErrorCode::capacity_exhausted,used_,bytes.size()});
        const auto offset=used_;
        if(!bytes.empty())std::memcpy(bytes_.data()+used_,bytes.data(),bytes.size());
        used_+=bytes.size();return static_cast<std::uint32_t>(offset);
    }
    Result<Bytes> view(std::size_t offset,std::size_t count) const noexcept {
        if(offset>used_ || count>used_-offset)return std::unexpected(Error{ErrorCode::invalid_argument});
        return Bytes{bytes_}.subspan(offset,count);
    }
};
template<> class NativeArenaStorage<0> {
public:
    constexpr void clear() noexcept {}
    constexpr std::size_t size() const noexcept { return 0; }
    Result<std::uint32_t> append(Bytes) noexcept { return std::unexpected(Error{ErrorCode::capacity_exhausted}); }
    Result<Bytes> view(std::size_t,std::size_t) const noexcept { return std::unexpected(Error{ErrorCode::invalid_argument}); }
};
template<class T> Bytes object_bytes(const T& object) noexcept {
    static_assert(std::is_trivially_copyable_v<T>);
    return {reinterpret_cast<const std::byte*>(&object),sizeof(T)};
}
} // namespace detail
} // namespace vita
