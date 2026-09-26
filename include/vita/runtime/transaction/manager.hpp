#pragma once
#include <vita/runtime/transaction/retention.hpp>
namespace vita::runtime::transaction {
// The public transaction orchestration path. One manager per Controllee uses a
// runtime-wide store. All calls execute on the same serialized domain as its
// Engine and RetentionStore.
struct ManagerDrainStatus {std::size_t active=0,scheduled_cancellations=0;};
template <std::size_t Transactions = 8, std::size_t Entries = 4096,
          std::size_t BytesCapacity = 8 * 1024 * 1024,
          std::size_t CancelSlots = 64>
class TransactionManager {
  struct Active {
    RetentionToken token;
    Handle handle;
  };
  struct PendingCancel {
    RetentionToken token;
    AdmissionBundle credits;
    timing::ProtocolTime requested;
    timing::TimingCapabilities timing;
    std::uint64_t association_generation = 0, mapping_generation = 0;
    bool timing_valid = true;
  };
  Engine<Transactions> &engine_;
  RetentionStore<Entries, BytesCapacity> &retention_;
  AdmissionPool &admission_;
  std::array<std::optional<Active>, Transactions> active_{};
  std::array<std::optional<PendingCancel>, CancelSlots> cancellations_{};
  bool progressing_ = false,closed_=false,quiescing_=false;
  std::uint64_t closed_generation_=0;
  bool monotonic_ids_=false;std::uint32_t highwater_id_=0;

  Result<void> finish_cancel(PendingCancel &pending,
                             const OperationContext &now) noexcept {
    auto canonical = retention_.canonical(pending.token);
    if (!canonical)
      return std::unexpected(canonical.error());
    auto packet = codec::decode_packet(*canonical);
    if (!packet)
      return std::unexpected(packet.error());
    auto selectors = cancellation_selectors(*packet);
    if (!selectors)
      return std::unexpected(selectors.error());
    const auto mode = (packet->envelope.envelope.command->cam >> 12) & 7;
    bool timing_allowed = true;
    if (mode) {
      auto first = timing::subtract(
          now.clock.time, timing::from_picoseconds(now.clock.uncertainty_ps));
      auto last = timing::add(
          now.clock.time, timing::from_picoseconds(now.clock.uncertainty_ps));
      auto fit =
          first && last
              ? timing::effect_interval_allowed(mode, pending.requested, *first,
                                                *last, pending.timing)
              : Result<bool>{false};
      timing_allowed = pending.timing_valid && fit && *fit &&
                       (now.clock.state == timing::ClockState::locked ||
                        now.clock.state == timing::ClockState::holdover);
    }
    auto original = pending.token;
    original.cancellation = false;
    auto handle = retention_.engine_handle(original);
    if (!handle)
      return std::unexpected(handle.error());
    CancellationResult result;
    if (timing_allowed && *handle) {
      auto context = now;
      context.association_generation = pending.association_generation;
      context.timing = pending.timing;
      auto cancelled = engine_.cancel(**handle, *packet, context,
                                      std::move(pending.credits));
      if (!cancelled)
        return std::unexpected(cancelled.error());
      result = std::move(*cancelled);
    } else {
      std::array<Diagnostics, state_field_capacity> diagnostics{};
      for (std::size_t i = 0; i < state_field_capacity; ++i)
        if ((*selectors)[i])
          diagnostics[i].errors = not_executed;
      result = cancellation_response(
          *packet, engine_.state(), now.clock, *selectors, {}, diagnostics,
          std::move(pending.credits), timing_allowed ? mode : 7);
    }
    // The stored semantic result owns its historical state/time. Retries never
    // observe state again.
    for (std::size_t i = 0; i < result.responses.size(); ++i) {
      auto stored = retention_.append(pending.token, result.responses[i]);
      if (!stored)
        return std::unexpected(stored.error());
    }
    auto done = retention_.complete(pending.token, now.monotonic);
    if (!done)
      return done;
    return retention_.release(
        pending.token); // release the manager's active reference
  }

public:
  TransactionManager(Engine<Transactions> &engine,
                     RetentionStore<Entries, BytesCapacity> &retention,
                     AdmissionPool &admission,bool monotonic_ids=false) noexcept
      : engine_(engine), retention_(retention), admission_(admission),monotonic_ids_(monotonic_ids) {}

  std::uint32_t admitted_message_id() const noexcept { return highwater_id_; }

  void close_admission() noexcept {if(!closed_)closed_generation_=engine_.association_generation();closed_=true;}
  ManagerDrainStatus drain_status() const noexcept {
    ManagerDrainStatus status;for(const auto& active:active_)status.active+=bool(active);for(const auto& pending:cancellations_)status.scheduled_cancellations+=bool(pending);return status;
  }
  Result<void> request_quiesce(const OperationContext& now) noexcept {
    if(progressing_)return std::unexpected(Error{ErrorCode::would_deadlock});
    if(!now.association_generation||(engine_.association_generation()&&now.association_generation!=engine_.association_generation()))return std::unexpected(Error{ErrorCode::stale_generation});
    close_admission();quiescing_=true;auto requested=engine_.request_quiesce(now);if(!requested)return requested;return progress(now);
  }
  Result<void> reset_after_drain() noexcept {
    const auto status=drain_status();
    if(progressing_||status.active||status.scheduled_cancellations||engine_.quiescing()||!engine_.safe_to_reset()||(closed_&&engine_.association_generation()<=closed_generation_))return std::unexpected(Error{ErrorCode::invalid_state});
    closed_=quiescing_=false;return {};
  }
  Result<RetentionAdmission> accept(const codec::PacketView &packet,
                                    const OperationContext &now,
                                    PeerSession peer) noexcept {
    if (progressing_ || !engine_.external_retention())
      return std::unexpected(Error{ErrorCode::invalid_state});
    auto key = transaction_key(packet, now.association_generation, peer);
    if (!key)
      return std::unexpected(key.error());
    if(closed_)return retention_.replay_existing(*key,packet);
    const bool cancellation = packet.envelope.envelope.cancel;
    const auto mid=packet.envelope.envelope.command->message_id;
    if(monotonic_ids_&&!mid)return std::unexpected(Error{ErrorCode::invalid_argument});
    if(monotonic_ids_&&!cancellation&&mid<=highwater_id_)return retention_.replay_existing(*key,packet);
    auto reserved = retention_.reserve(*key, packet, cancellation ? 2 : 3);
    if (!reserved)
      return std::unexpected(reserved.error());
    if (reserved->kind != DuplicateKind::fresh)
      return *reserved;
    auto rollback = [&](Error error) -> Result<RetentionAdmission> {
      retention_.rollback(reserved->token);
      return std::unexpected(error);
    };
    if (!cancellation) {
      std::size_t slot = Transactions;
      for (std::size_t i = 0; i < Transactions; ++i)
        if (!active_[i]) {
          slot = i;
          break;
        }
      if (slot == Transactions)
        return rollback(Error{ErrorCode::capacity_exhausted});
      auto admitted = engine_.accept(packet, now);
      if (!admitted)
        return rollback(admitted.error());
      retention_.bind(reserved->token, *admitted);
      retention_.retain(reserved->token);
      active_[slot] = Active{reserved->token, *admitted};
      if(monotonic_ids_)highwater_id_=mid;
      return *reserved;
    }
    auto selectors = cancellation_selectors(packet);
    if (!selectors)
      return rollback(selectors.error());
    auto original = reserved->token;
    original.cancellation = false;
    auto original_bytes = retention_.canonical(original);
    if (!original_bytes)
      return rollback(original_bytes.error());
    auto original_packet = codec::decode_packet(*original_bytes);
    if (!original_packet)
      return rollback(original_packet.error());
    const auto &envelope = packet.envelope.envelope;
    if (!same_class(envelope.class_id,
                    original_packet->envelope.envelope.class_id) ||
        ((original_packet->envelope.envelope.command->cam >> 23) & 3) != 2)
      return rollback(Error{ErrorCode::invalid_argument});
    for (std::size_t i = 0; i < state_field_capacity; ++i)
      if ((*selectors)[i]) {
        bool present = false;
        for (std::size_t j = 0; j < original_packet->fields.size(); ++j)
          present |= original_packet->fields[j].id == state_fields[i];
        if (!present)
          return rollback(Error{ErrorCode::invalid_argument});
      }
    const auto mode = (envelope.command->cam >> 12) & 7;
    if (mode) {
      const auto epoch = now.clock.epoch == timing::Epoch::gps ? codec::Tsi::gps
                         : now.clock.epoch == timing::Epoch::utc
                             ? codec::Tsi::utc
                             : codec::Tsi::other;
      if (envelope.timestamp.tsi != epoch ||
          envelope.timestamp.tsf != codec::Tsf::picoseconds ||
          (!now.timing.injected && !now.timing.qualified))
        return rollback(Error{ErrorCode::unsupported_capability});
      const timing::ProtocolTime requested{envelope.timestamp.integer,
                                           envelope.timestamp.fractional};
      const timing::Boundary boundary{
          requested, 0, now.clock.mapping_generation, false, true, 0};
      auto scheduled = timing::choose_boundary(mode, requested, now.clock,
                                               {&boundary, 1}, now.timing);
      if (!scheduled)
        return rollback(scheduled.error());
    }
    std::size_t slot = CancelSlots;
    for (std::size_t i = 0; i < CancelSlots; ++i)
      if (!cancellations_[i]) {
        slot = i;
        break;
      }
    if (slot == CancelSlots)
      return rollback(Error{ErrorCode::capacity_exhausted});
    auto credits =
        admission_.acquire(cancellation_resources(envelope.command->cam));
    if (!credits)
      return rollback(credits.error());
    retention_.bind(reserved->token);
    retention_.retain(reserved->token);
    cancellations_[slot] = PendingCancel{
        reserved->token,
        std::move(*credits),
        {envelope.timestamp.integer, envelope.timestamp.fractional},
        now.timing,
        now.association_generation,
        now.clock.mapping_generation,
        true};
    return *reserved;
  }

  Result<void> progress(const OperationContext &now) noexcept {
    if (progressing_)
      return std::unexpected(Error{ErrorCode::would_deadlock});
    struct Guard {
      bool &flag;
      ~Guard() { flag = false; }
    } guard{progressing_};
    progressing_ = true;
    // Reserved cancellation ingress is drained independently of ordinary queue
    // capacity.
    for (auto &pending : cancellations_)
      if (pending) {
        auto wire = retention_.canonical(pending->token);
        if (!wire)
          return std::unexpected(wire.error());
        auto view = codec::decode_packet(*wire);
        if (!view)
          return std::unexpected(view.error());
        const auto mode = (view->envelope.envelope.command->cam >> 12) & 7;
        const bool qualified = now.clock.state == timing::ClockState::locked ||
                               now.clock.state == timing::ClockState::holdover;
        if (mode &&
            pending->mapping_generation != now.clock.mapping_generation) {
          const timing::Boundary boundary{pending->requested,
                                          0,
                                          now.clock.mapping_generation,
                                          false,
                                          true,
                                          0};
          auto scheduled =
              timing::choose_boundary(mode, pending->requested, now.clock,
                                      {&boundary, 1}, pending->timing);
          pending->timing_valid = bool(scheduled);
          pending->mapping_generation = now.clock.mapping_generation;
        }
        if(quiescing_)pending->timing_valid=false;
        if (mode && qualified && pending->timing_valid &&
            now.clock.time < pending->requested)
          continue;
        auto done = finish_cancel(*pending, now);
        if (!done)
          return done;
        pending.reset();
      }
    auto advanced = engine_.progress(now);
    if (!advanced)
      return advanced;
    for (auto &active : active_)
      if (active) {
        for (;;) {
          auto response = engine_.take_response(active->handle);
          if (!response)
            return std::unexpected(response.error());
          if (!*response)
            break;
          auto stored = retention_.append(active->token, **response);
          if (!stored)
            return stored;
        }
        auto done = engine_.complete(active->handle);
        if (!done)
          return std::unexpected(done.error());
        if (*done) {
          auto retained = retention_.complete(active->token, now.monotonic);
          if (!retained)
            return retained;
          auto released = engine_.release(active->handle);
          if (!released)
            return released;
          retention_.release(active->token);
          active.reset();
        }
      }
    retention_.expire(now.monotonic);
    return {};
  }
  Result<std::optional<AckRecord>> response(RetentionToken token,
                                            std::size_t index) noexcept {
    return retention_.response(token, index);
  }
  Result<bool> complete(RetentionToken token) noexcept {return retention_.terminal(token);}
  Result<void> release(RetentionToken token) noexcept {
    return retention_.release(token);
  }
};
} // namespace vita::runtime::transaction
