#pragma once
#include <chrono>
#include <memory>
#include <vita/runtime/state/contracts.hpp>
namespace vita::runtime::transaction {
enum class TraceStage { received, validated, dispatch, device_done, recorded };
struct TraceKey {
  std::uint64_t association_generation = 0, operation = 0, peer = 0;
  std::uint32_t sid = 0, mid = 0;
  friend bool operator==(const TraceKey &, const TraceKey &) = default;
};
struct TraceEvent {
  TraceKey key;
  TraceStage stage = TraceStage::received;
  std::uint64_t monotonic_ns = 0;
  FieldId
      field{}; // Per-field dispatch/completion; zero for whole-command events.
  FieldStatus status = FieldStatus::pending;
  bool simulated = false;
};
inline std::uint64_t steady_trace_ns(void *) noexcept {
  return static_cast<std::uint64_t>(
      std::chrono::duration_cast<std::chrono::nanoseconds>(
          std::chrono::steady_clock::now().time_since_epoch())
          .count());
}
// All storage is provided at setup. record runs on the serialized Engine
// domain; now_ns may run on a backend completion thread and must be
// thread-safe/bounded. A sink must report overflow explicitly; dropped trace
// data invalidates a run.
struct TraceBinding {
  std::shared_ptr<void> owner;
  void *context = nullptr;
  std::uint64_t (*now_ns)(void *) noexcept = nullptr;
  void (*record)(void *, const TraceEvent &) noexcept = nullptr;
  // Actual setup-owned sink/staging allocation, including ownership overhead.
  // Zero is allowed only for standalone caller-accounted use. Runtime requires
  // positive bytes and charges each shared owner once with consistent aliases.
  std::size_t storage_bytes = 0;
  bool enabled() const noexcept { return owner && context && now_ns && record; }
  bool valid() const noexcept {
    return enabled() ||
           (!owner && !context && !now_ns && !record && !storage_bytes);
  }
  void emit(TraceStage stage, TraceKey key, FieldId field = {},
            FieldStatus status = FieldStatus::pending,
            bool simulated = false) const noexcept {
    if (enabled())
      record(context,
             TraceEvent{key, stage, now_ns(context), field, status, simulated});
  }
  void emit_at(TraceStage stage, TraceKey key, std::uint64_t timestamp,
               FieldId field = {}, FieldStatus status = FieldStatus::pending,
               bool simulated = false) const noexcept {
    if (enabled())
      record(context,
             TraceEvent{key, stage, timestamp, field, status, simulated});
  }
};
} // namespace vita::runtime::transaction
