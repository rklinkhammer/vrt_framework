#pragma once
#include "endpoint_options.hpp"
#include "virtual_tuner.hpp"
#include <cstdio>
#include <vita/runtime/public/runtime.hpp>

namespace vita::examples::frequency_scan {
using Runtime = VitaRuntime<1, 16, 4096, 8 * 1024 * 1024>;
struct Evidence {
  bool validation = false, state = false, execution = false, failed = false,
       cancel_done = false, cancel_confirmed = false, validation_known = false,
       validation_accepted = false;
  runtime::transaction::Observation executed{};
  static void observe(void *context,
                      const runtime::transaction::Observation &event) noexcept {
    auto &self = *static_cast<Evidence *>(context);
    using K = runtime::transaction::ObservationKind;
    if (event.kind == K::validation) {
      self.validation = true;
      self.validation_known = event.validation_outcome_known;
      self.validation_accepted = event.validation_accepted;
    }
    if (event.kind == K::state)
      self.state = true;
    if (event.kind == K::execution) {
      self.execution = true;
      self.executed = event;
    }
    if (event.kind == K::cancellation_execution) {
      self.cancel_done = true;
      self.cancel_confirmed = event.confirms_cancellation;
    }
    if (event.kind == K::cancellation_timeout)
      self.cancel_done = true;
    if (event.kind == K::timeout || event.kind == K::local_failure ||
        event.contradictory)
      self.failed = true;
  }
};
struct ReceiverCounters {
  std::uint64_t packets = 0, known = 0, drops = 0;
  runtime::context::ReceiverBinding binding() noexcept {
    return {this,
            [](void *p, const runtime::context::BorrowedSignalRx &rx) noexcept {
              auto &self = *static_cast<ReceiverCounters *>(p);
              ++self.packets;
              if (rx.metadata.confidence == runtime::context::Confidence::known)
                ++self.known;
            },
            [](void *p, runtime::context::Confidence) noexcept {
              ++static_cast<ReceiverCounters *>(p)->drops;
            }};
  }
};
class ScanApplication {
  Runtime::Controller controller_;
  profiles::iq::SweepPolicy policy_;
  std::optional<TransactionHandle> handle_;
  Evidence evidence_{};
  bool querying_ = true, validation_printed_ = false, execution_used_ = false,
       state_used_ = false;
  std::uint64_t requested_ = 0, applied_ = 0, rate_, timeout_, confirmed_ = 0,
                submitted_ = 0, dwell_started_ = 0, dwell_, points_;
  bool dwelling_ = false;

public:
  ScanApplication(Runtime::Controller controller,
                  profiles::iq::SweepPolicy policy, const Options &options)
      : controller_(controller), policy_(policy),
        rate_(options.scene.sample_rate), timeout_(options.sweep.timeout_ns),
        dwell_(options.sweep.dwell_ns),
        points_((options.sweep.stop_hz - options.sweep.start_hz) /
                    options.sweep.step_hz +
                1) {}
  Result<void> begin() {
    CommandOptions options;
    options.timeout_ns = timeout_;
    auto query = controller_.query(
        QuerySelection{QueryField::sample_rate, QueryField::center_frequency},
        options);
    if (!query)
      return std::unexpected(query.error());
    handle_ = *query;
    return controller_.observe(*handle_, &evidence_, Evidence::observe);
  }
  Result<bool> progress(runtime::timing::MonoTime now) {
    if (evidence_.validation_known && !evidence_.validation_accepted) {
      std::puts("AckV received: accepted=false; explicit validation rejection "
                "stops submissions");
      return std::unexpected(Error{ErrorCode::invalid_argument});
    }
    if (evidence_.failed)
      return std::unexpected(Error{ErrorCode::invalid_state});
    if (handle_) {
      if (evidence_.validation && !validation_printed_) {
        std::printf("AckV received: accepted=%s; validation evidence only\n",
                    evidence_.validation_known && evidence_.validation_accepted
                        ? "true"
                        : "unknown");
        validation_printed_ = true;
      }
      if (evidence_.state && !state_used_) {
        auto captured = controller_.state(*handle_);
        if (!captured)
          return std::unexpected(captured.error());
        if (*captured) {
          const auto &state = **captured;
          auto rf = state.value<RFReferenceFrequency>();
          if (!rf || state.hypothetical || state.late || rf->q20 < 0 ||
              rf->q20 % (1LL << 20))
            return std::unexpected(Error{ErrorCode::invalid_state});
          if (querying_) {
            auto rate = state.value<SampleRate>();
            if (!rate ||
                rate->q20 != static_cast<std::int64_t>(rate_) * (1LL << 20))
              return std::unexpected(Error{ErrorCode::identity_conflict});
            std::printf("query center_hz=%llu sample_rate_hz=%llu (remote rate "
                        "is not changed)\n",
                        static_cast<unsigned long long>(rf->q20 >> 20),
                        static_cast<unsigned long long>(rate_));
            auto released = controller_.release(*handle_);
            if (!released)
              return std::unexpected(released.error());
            handle_.reset();
            querying_ = false;
          } else {
            applied_ = static_cast<std::uint64_t>(rf->q20 >> 20);
            auto readback = policy_.readback(applied_, true, now);
            if (!readback)
              return std::unexpected(readback.error());
          }
          state_used_ = true;
        }
      }
      if (!querying_ && handle_ && evidence_.execution && !execution_used_) {
        const auto &x = evidence_.executed;
        auto accepted = policy_.execution({x.success && x.confirms_execution,
                                           x.hypothetical, x.partial,
                                           x.unknown_remote_outcome},
                                          now);
        if (!accepted)
          return std::unexpected(accepted.error());
        execution_used_ = true;
      }
      if (handle_ && !querying_ &&
          policy_.phase() == profiles::iq::SweepPhase::dwelling) {
        ++confirmed_;
        dwell_started_ = now.ns;
        dwelling_ = true;
        std::printf(
            "tuned requested_hz=%llu applied_hz=%llu AckX=success "
            "AckS=matching latency_ns=%llu dwell_start_ns=%llu "
            "dwell_configured_ns=%llu sweep=%llu point=%llu "
            "handle=%llu/%zu/%zu/%llu\n",
            static_cast<unsigned long long>(requested_),
            static_cast<unsigned long long>(applied_),
            static_cast<unsigned long long>(now.ns - submitted_),
            static_cast<unsigned long long>(now.ns),
            static_cast<unsigned long long>(dwell_),
            static_cast<unsigned long long>((confirmed_ - 1) / points_ + 1),
            static_cast<unsigned long long>((confirmed_ - 1) % points_ + 1),
            static_cast<unsigned long long>(handle_->runtime_id),
            handle_->stream, handle_->controller.slot,
            static_cast<unsigned long long>(handle_->controller.generation));
        auto released = controller_.release(*handle_);
        if (!released)
          return std::unexpected(released.error());
        handle_.reset();
      }
    }
    if (querying_)
      return false;
    if (dwelling_ && now.ns - dwell_started_ >= dwell_) {
      std::printf("dwell complete requested_hz=%llu configured_ns=%llu "
                  "elapsed_ns=%llu\n",
                  static_cast<unsigned long long>(requested_),
                  static_cast<unsigned long long>(dwell_),
                  static_cast<unsigned long long>(now.ns - dwell_started_));
      dwelling_ = false;
    }
    auto next = policy_.next(now);
    if (!next)
      return std::unexpected(next.error());
    if (*next) {
      submitted_ = now.ns;
      requested_ = **next;
      evidence_ = {};
      validation_printed_ = execution_used_ = state_used_ = false;
      CommandOptions options;
      options.timeout_ns = timeout_;
      options.partial = false;
      auto request = controller_.set_center_frequency(
          *Hertz::from_integer(requested_), options);
      if (!request) {
        policy_.submission_failed();
        return std::unexpected(request.error());
      }
      handle_ = *request;
      auto observed =
          controller_.observe(*handle_, &evidence_, Evidence::observe);
      if (!observed)
        return std::unexpected(observed.error());
      std::printf(
          "request center_hz=%llu sweep=%llu point=%llu "
          "handle=%llu/%zu/%zu/%llu\n",
          static_cast<unsigned long long>(requested_),
          static_cast<unsigned long long>(confirmed_ / points_ + 1),
          static_cast<unsigned long long>(confirmed_ % points_ + 1),
          static_cast<unsigned long long>(handle_->runtime_id), handle_->stream,
          handle_->controller.slot,
          static_cast<unsigned long long>(handle_->controller.generation));
    }
    return policy_.phase() == profiles::iq::SweepPhase::complete;
  }
  Result<bool> cancel_on_operator_stop() {
    if (!handle_ || querying_)
      return false;
    CommandOptions options;
    options.timeout_ns = 200'000'000;
    options.state = false;
    auto cancel = controller_.cancel(
        *handle_, QuerySelection{QueryField::center_frequency}, options);
    if (!cancel)
      return std::unexpected(cancel.error());
    std::puts("operator stop: explicit best-effort RF cancellation requested; "
              "original outcome remains distinct");
    return true;
  }
  bool cancellation_done() const noexcept { return evidence_.cancel_done; }
  bool cancellation_confirmed() const noexcept {
    return evidence_.cancel_confirmed;
  }
  void release() noexcept {
    if (handle_) {
      (void)controller_.release(*handle_);
      handle_.reset();
    }
  }
  std::uint64_t confirmed() const noexcept { return confirmed_; }
};
} // namespace vita::examples::frequency_scan
