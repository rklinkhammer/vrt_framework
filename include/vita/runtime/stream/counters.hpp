#pragma once
#include <array>
#include <mutex>
#include <optional>
#include <vita/codec/wire.hpp>
namespace vita::runtime {
struct CounterKey {
  std::uint64_t sender = 0;
  std::optional<std::uint32_t> stream_id{};
  codec::PacketType type = codec::PacketType::signal;
  friend bool operator==(const CounterKey &, const CounterKey &) = default;
};
template <std::size_t N = 64> class CounterRegistry {
  struct Entry {
    CounterKey key;
    std::uint8_t next = 0;
  };
  std::array<std::optional<Entry>, N> entries_{};
  mutable std::mutex mutex_;
  bool frozen_ = false;

public:
  Result<void> add(CounterKey key) noexcept {
    std::lock_guard lock(mutex_);
    if (frozen_)
      return std::unexpected(Error{ErrorCode::invalid_state});
    if (static_cast<unsigned>(key.type) > 7 ||
        ((key.type == codec::PacketType::signal_without_sid ||
          key.type == codec::PacketType::extension_without_sid) ==
         key.stream_id.has_value()))
      return std::unexpected(Error{ErrorCode::invalid_argument});
    for (const auto &e : entries_)
      if (e && e->key == key)
        return std::unexpected(Error{ErrorCode::invalid_argument});
    for (auto &e : entries_)
      if (!e) {
        e = Entry{key};
        return {};
      }
    return std::unexpected(Error{ErrorCode::capacity_exhausted});
  }
  Result<void> install(std::span<const CounterKey> batch) noexcept {
    std::lock_guard lock(mutex_);
    auto candidate = entries_;
    for (const auto &key : batch) {
      if (static_cast<unsigned>(key.type) > 7 ||
          ((key.type == codec::PacketType::signal_without_sid ||
            key.type == codec::PacketType::extension_without_sid) ==
           key.stream_id.has_value()))
        return std::unexpected(Error{ErrorCode::invalid_argument});
      for (const auto &e : candidate)
        if (e && e->key == key)
          return std::unexpected(Error{ErrorCode::identity_conflict});
      bool added = false;
      for (auto &e : candidate)
        if (!e) {
          e = Entry{key};
          added = true;
          break;
        }
      if (!added)
        return std::unexpected(Error{ErrorCode::capacity_exhausted});
    }
    entries_ = std::move(candidate);
    return {};
  }
  std::size_t available() const noexcept {
    std::lock_guard lock(mutex_);
    std::size_t n = 0;
    for (const auto &e : entries_)
      n += !e;
    return n;
  }
  void freeze() noexcept {
    std::lock_guard lock(mutex_);
    frozen_ = true;
  }
  Result<std::uint8_t> next(CounterKey key) const noexcept {
    std::lock_guard lock(mutex_);
    if (!frozen_)
      return std::unexpected(Error{ErrorCode::invalid_state});
    for (const auto &e : entries_)
      if (e && e->key == key)
        return e->next;
    return std::unexpected(Error{ErrorCode::invalid_argument});
  }
  // Called exactly at local submission acceptance, never for retransmitted
  // receive duplicates.
  Result<void> accept(CounterKey key, std::uint8_t encoded_count) noexcept {
    std::lock_guard lock(mutex_);
    if (!frozen_)
      return std::unexpected(Error{ErrorCode::invalid_state});
    for (auto &e : entries_)
      if (e && e->key == key) {
        if (e->next != encoded_count)
          return std::unexpected(Error{ErrorCode::stale_generation});
        e->next = static_cast<std::uint8_t>((e->next + 1) & 15);
        return {};
      }
    return std::unexpected(Error{ErrorCode::invalid_argument});
  }
};
} // namespace vita::runtime
