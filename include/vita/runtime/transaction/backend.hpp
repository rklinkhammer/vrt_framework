#pragma once
#include <array>
#include <memory>
#include <optional>
#include <vita/runtime/completion/ticket.hpp>
#include <vita/runtime/transaction/cam.hpp>
#include <vita/runtime/transaction/trace.hpp>
namespace vita::runtime::transaction {
struct ResultGuard {
  std::atomic<std::size_t> holders{0};
  std::atomic<bool> cancelled{false}, contradiction{false};
  std::uint64_t association_generation = 0;
  FieldId field{};
};
template <std::size_t N> struct ResultStorage {
  struct Slot {
    FieldOutcome outcome{};
    std::uint64_t device_done_ns = 0;
    std::uint64_t generation = 0;
    bool busy = false;
    ResultGuard guard;
  };
  std::array<Slot, N> slots{};
  TraceBinding trace;
  std::shared_ptr<void> backend_owner;
};
// Capability pins result storage and ticket arena. The claim always precedes
// external payload writes.
class AsyncResult {
  CompletionPublisher publisher_;
  std::shared_ptr<void> lifetime_;
  FieldOutcome *output_ = nullptr;
  std::size_t index_ = 0;
  ResultGuard *guard_ = nullptr;
  const TraceBinding *trace_ = nullptr;
  std::uint64_t *device_done_ns_ = nullptr;
  void retain() noexcept {
    if (guard_)
      guard_->holders.fetch_add(1, std::memory_order_relaxed);
  }
  void release() noexcept {
    if (guard_)
      guard_->holders.fetch_sub(1, std::memory_order_acq_rel);
    guard_ = nullptr;
  }

public:
  AsyncResult() noexcept = default;
  AsyncResult(CompletionPublisher publisher, std::shared_ptr<void> lifetime,
              FieldOutcome *output, std::size_t index,
              ResultGuard *guard = nullptr, const TraceBinding *trace = nullptr,
              std::uint64_t *device_done_ns = nullptr) noexcept
      : publisher_(std::move(publisher)), lifetime_(std::move(lifetime)),
        output_(output), index_(index), guard_(guard), trace_(trace),
        device_done_ns_(device_done_ns) {
    retain();
  }
  AsyncResult(const AsyncResult &other) noexcept
      : publisher_(other.publisher_), lifetime_(other.lifetime_),
        output_(other.output_), index_(other.index_), guard_(other.guard_),
        trace_(other.trace_), device_done_ns_(other.device_done_ns_) {
    retain();
  }
  AsyncResult &operator=(const AsyncResult &other) noexcept {
    if (this != &other) {
      release();
      publisher_ = other.publisher_;
      lifetime_ = other.lifetime_;
      output_ = other.output_;
      index_ = other.index_;
      guard_ = other.guard_;
      trace_ = other.trace_;
      device_done_ns_ = other.device_done_ns_;
      retain();
    }
    return *this;
  }
  AsyncResult(AsyncResult &&other) noexcept
      : publisher_(std::move(other.publisher_)),
        lifetime_(std::move(other.lifetime_)), output_(other.output_),
        index_(other.index_), guard_(std::exchange(other.guard_, nullptr)),
        trace_(other.trace_), device_done_ns_(other.device_done_ns_) {}
  AsyncResult &operator=(AsyncResult &&other) noexcept {
    if (this != &other) {
      release();
      publisher_ = std::move(other.publisher_);
      lifetime_ = std::move(other.lifetime_);
      output_ = other.output_;
      index_ = other.index_;
      guard_ = std::exchange(other.guard_, nullptr);
      trace_ = other.trace_;
      device_done_ns_ = other.device_done_ns_;
    }
    return *this;
  }
  ~AsyncResult() { release(); }
  bool complete(FieldOutcome outcome) const noexcept {
    auto writer = publisher_.try_claim();
    if (!writer) {
      if (guard_ && (outcome.status == FieldStatus::executed ||
                     outcome.status == FieldStatus::unknown_effect))
        guard_->contradiction.store(true, std::memory_order_release);
      return false;
    }
    if (guard_ && outcome.status == FieldStatus::cancelled)
      guard_->cancelled.store(true, std::memory_order_release);
    *output_ = outcome;
    if (trace_ && trace_->enabled() && device_done_ns_)
      *device_done_ns_ = trace_->now_ns(trace_->context);
    CompletionResult ready;
    ready.value = index_;
    return writer->finish(ready);
  }
  bool same_operation(const AsyncResult &other) const noexcept {
    return publisher_.slot() == other.publisher_.slot() &&
           publisher_.generation() == other.publisher_.generation() &&
           output_ == other.output_;
  }
  bool reserved() const noexcept { return publisher_.is_reserved(); }
  bool synthetic_failure() const noexcept {
    CompletionResult failed;
    failed.status = CompletionStatus::failed;
    failed.error = Error{ErrorCode::callback_failure};
    return publisher_.publish(failed);
  }
};
enum class DisarmResult { cancelled, not_cancelled, unknown_effect };
struct BackendQuiescence {
  bool known = false, quiescent = false;
  std::size_t pending = 0;
};
struct Backend {
  void *context = nullptr;
  Validation (*validate)(void *, FieldId, SemanticValue,
                         const StateSnapshot &) noexcept = nullptr;
  Result<void> (*validate_plan)(void *, const ExecutionPlan &,
                                const StateSnapshot &) noexcept = nullptr;
  Result<void> (*begin)(void *, const PlannedField &, timing::Boundary,
                        AsyncResult) noexcept = nullptr;
  FieldOutcome (*simulate)(void *, const PlannedField &, const StateSnapshot &,
                           timing::Boundary) noexcept = nullptr;
  DisarmResult (*disarm)(void *, const AsyncResult &) noexcept = nullptr;
  BackendQuiescence (*quiescence)(void *) noexcept = nullptr;
};
struct VirtualRule {
  bool supported = true, resolvable = true;
  Diagnostics diagnostics{};
  std::optional<SemanticValue> adjusted{};
  std::uint8_t dependencies = 0;
  FieldStatus completion = FieldStatus::executed;
};
template <std::size_t Pending = 16> class VirtualBackend {
  struct Work {
    PlannedField field;
    timing::Boundary boundary;
    AsyncResult completion;
  };
  std::array<VirtualRule, state_field_capacity> rules_{};
  std::array<std::optional<Work>, Pending> pending_{};
  std::size_t begins_ = 0, writes_ = 0;
  bool reversible_ = true, quiescence_available_ = true,
       inline_completion_ = false;
  StateSnapshot model_{};
  static Validation validate_thunk(void *self, FieldId id, SemanticValue value,
                                   const StateSnapshot &) noexcept {
    auto &backend = *static_cast<VirtualBackend *>(self);
    const auto index = field_index(id);
    if (index == state_field_capacity)
      return Validation{value, {0, unsupported}, false};
    const auto &rule = backend.rules_[index];
    auto diagnostics = rule.diagnostics;
    if (!rule.supported)
      diagnostics.errors |= unsupported;
    return Validation{rule.adjusted.value_or(value), diagnostics,
                      rule.supported && rule.resolvable, rule.dependencies};
  }
  static Result<void> begin_thunk(void *self, const PlannedField &field,
                                  timing::Boundary boundary,
                                  AsyncResult result) noexcept {
    auto &backend = *static_cast<VirtualBackend *>(self);
    if (backend.inline_completion_) {
      auto outcome = backend.outcome(field, boundary, false);
      if ((outcome.status == FieldStatus::executed ||
           outcome.status == FieldStatus::unknown_effect) &&
          backend.model_.version == UINT64_MAX)
        return std::unexpected(Error{ErrorCode::overflow});
      ++backend.begins_;
      backend.record_model(outcome);
      if (!result.complete(outcome))
        return std::unexpected(Error{ErrorCode::invalid_state});
      return {};
    }
    for (auto &pending : backend.pending_)
      if (!pending) {
        pending = Work{field, boundary, std::move(result)};
        ++backend.begins_;
        return {};
      }
    return std::unexpected(Error{ErrorCode::capacity_exhausted});
  }
  static FieldOutcome simulate_thunk(void *self, const PlannedField &field,
                                     const StateSnapshot &,
                                     timing::Boundary boundary) noexcept {
    auto &backend = *static_cast<VirtualBackend *>(self);
    return backend.outcome(field, boundary, true);
  }
  static DisarmResult disarm_thunk(void *self,
                                   const AsyncResult &result) noexcept {
    auto &backend = *static_cast<VirtualBackend *>(self);
    if (!backend.reversible_)
      return DisarmResult::not_cancelled;
    for (auto &work : backend.pending_)
      if (work && work->completion.same_operation(result)) {
        if (!result.reserved())
          return DisarmResult::not_cancelled;
        work.reset();
        return DisarmResult::cancelled;
      }
    return DisarmResult::not_cancelled;
  }
  FieldOutcome outcome(const PlannedField &field, timing::Boundary boundary,
                       bool simulated) const noexcept {
    FieldOutcome out;
    out.id = field.id;
    out.value = field.adjusted;
    out.validity = Validity::known;
    out.actual_time = boundary.time;
    out.sample_ordinal = boundary.sample_ordinal;
    out.time_known = field.time_known;
    out.ordinal_known = field.ordinal_known;
    out.simulated = simulated;
    out.status = rules_[field_index(field.id)].completion;
    if (out.status == FieldStatus::failed)
      out.diagnostics.errors = device_failure | not_executed;
    if (out.status == FieldStatus::unknown_effect) {
      out.validity = Validity::unknown;
      out.diagnostics.errors =
          device_failure | state_indeterminate | not_executed;
    }
    return out;
  }
  void record_model(const FieldOutcome &outcome) noexcept {
    if (outcome.status == FieldStatus::executed ||
        outcome.status == FieldStatus::unknown_effect) {
      const auto index = field_index(outcome.id);
      model_.fields[index].id = outcome.id;
      if (outcome.status == FieldStatus::executed)
        model_.fields[index].value = outcome.value;
      model_.fields[index].validity = outcome.status == FieldStatus::executed
                                          ? Validity::known
                                          : Validity::unknown;
      ++model_.version;
      ++writes_;
    }
  }

public:
  Result<void> set_inline_completion(bool enabled) noexcept {
    if (pending())
      return std::unexpected(Error{ErrorCode::invalid_state});
    inline_completion_ = enabled;
    return {};
  }
  const StateSnapshot &model() const noexcept { return model_; }
  Backend binding() noexcept {
    return {this,
            validate_thunk,
            nullptr,
            begin_thunk,
            simulate_thunk,
            disarm_thunk,
            [](void *p) noexcept {
              return static_cast<VirtualBackend *>(p)->quiescence();
            }};
  }
  BackendQuiescence quiescence() const noexcept {
    const auto work = pending();
    return {quiescence_available_, quiescence_available_ && work == 0, work};
  }
  void set_quiescence_available(bool available) noexcept {
    quiescence_available_ = available;
  }
  Result<void> reinitialize() noexcept {
    if (!quiescence_available_)
      return std::unexpected(Error{ErrorCode::unsupported_capability});
    // This virtual model has no physical write before complete_next. Explicit
    // reinitialization discards pending modeled work and reports no effect.
    for (auto &work : pending_)
      if (work) {
        FieldOutcome outcome;
        outcome.id = work->field.id;
        outcome.status = FieldStatus::cancelled;
        outcome.diagnostics.errors = not_executed;
        work->completion.complete(outcome);
        work.reset();
      }
    return {};
  }
  Result<void> set_rule(FieldId id, VirtualRule rule) noexcept {
    const auto index = field_index(id);
    if (index == state_field_capacity || (rule.diagnostics.warnings & rule.diagnostics.errors) ||
        (rule.dependencies & 0xe0))
      return std::unexpected(Error{ErrorCode::invalid_argument});
    if (rule.completion != FieldStatus::executed &&
        rule.completion != FieldStatus::failed &&
        rule.completion != FieldStatus::unknown_effect)
      return std::unexpected(Error{ErrorCode::invalid_argument});
    rules_[index] = rule;
    return {};
  }
  void set_reversible(bool value) noexcept { reversible_ = value; }
  std::size_t begins() const noexcept { return begins_; }
  std::size_t writes() const noexcept { return writes_; }
  std::size_t pending() const noexcept {
    std::size_t n = 0;
    for (const auto &p : pending_)
      if (p)
        ++n;
    return n;
  }
  Result<bool>
  complete_next(std::optional<FieldOutcome> override = {}) noexcept {
    for (auto &work : pending_)
      if (work) {
        auto value =
            override.value_or(outcome(work->field, work->boundary, false));
        // Effect modeling occurs only for this explicit backend completion,
        // never on send/admission.
        if (value.status == FieldStatus::executed ||
            value.status == FieldStatus::unknown_effect)
          ++writes_;
        const bool published = work->completion.complete(value);
        work.reset();
        return published;
      }
    return false;
  }
  std::optional<AsyncResult> pending_capability() const noexcept {
    for (const auto &work : pending_)
      if (work)
        return work->completion;
    return {};
  }
};
} // namespace vita::runtime::transaction
