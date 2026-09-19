#pragma once
#include <vita/core/error.hpp>
#include <array>
#include <optional>
#include <type_traits>
#include <utility>
namespace vita {
// No allocation; live elements only. Access has the same precondition as std::array.
template<class T, std::size_t N> class FixedVector {
    std::array<std::optional<T>, N> slots_{};
    std::size_t size_ = 0;
public:
    constexpr std::size_t size() const noexcept { return size_; }
    static constexpr std::size_t capacity() noexcept { return N; }
    constexpr bool empty() const noexcept { return size_ == 0; }
    constexpr bool full() const noexcept { return size_ == N; }
    constexpr T& operator[](std::size_t i) noexcept { return *slots_[i]; }
    constexpr const T& operator[](std::size_t i) const noexcept { return *slots_[i]; }
    template<class... Args> requires std::is_nothrow_constructible_v<T, Args...>
    constexpr Result<void> emplace_back(Args&&... args) noexcept {
        if (full()) return std::unexpected(Error{ErrorCode::capacity_exhausted, 0, size_ + 1});
        slots_[size_].emplace(std::forward<Args>(args)...);
        ++size_;
        return {};
    }
    constexpr Result<void> push_back(T value) noexcept requires std::is_nothrow_move_constructible_v<T> {
        return emplace_back(std::move(value));
    }
    constexpr void pop_back() noexcept { if (size_) slots_[--size_].reset(); }
    constexpr void clear() noexcept { while (size_) pop_back(); }
};
} // namespace vita
