#pragma once
#include <optional>
#include <vita/core/fixed_vector.hpp>
#include <vita/runtime/execution/admission.hpp>
#include <vita/runtime/stream/routing.hpp>
#include <vita/runtime/transaction/backend.hpp>
#include <vita/runtime/transaction/cancellation.hpp>
#include <vita/runtime/transaction/outcomes.hpp>
namespace vita::runtime::transaction {
struct OperationContext {
  std::uint64_t operation = 0, association_generation = 1;
  timing::MonoTime monotonic{};
  timing::ClockSnapshot clock{};
  std::span<const timing::Boundary> boundaries{};
  timing::TimingCapabilities timing{};
  bool data_running = false;
  std::uint64_t trace_peer = 0;
};
struct Handle {
  std::size_t slot = 0;
  std::uint64_t generation = 0;
  friend bool operator==(Handle, Handle) = default;
};
struct Request {
  codec::Envelope envelope{};
  Cam cam{};
  std::array<FieldId, 5> fields{};
  std::array<SemanticValue, 5> values{};
  std::array<bool, 5> unsupported_attributes{};
  std::array<std::uint32_t, 5> attributes{};
  std::size_t count = 0;
  bool capability_query = false;
};
inline Result<Request> request_from(const codec::PacketView &packet,
                                    Profile profile) noexcept {
  auto cam = Cam::parse(packet.envelope.envelope, profile);
  if (!cam)
    return std::unexpected(cam.error());
  if(profile==Profile::sdr_radio){
    const auto& e=packet.envelope.envelope;
    if(e.class_id||!e.stream_id||*e.stream_id<1||*e.stream_id>4||
       e.timestamp.tsi!=codec::Tsi::utc||e.timestamp.tsf!=codec::Tsf::picoseconds||
       !e.command->message_id||packet.envelope.wire.size()>1024)
      return std::unexpected(Error{ErrorCode::unsupported_capability});
  }
  Request request;
  request.envelope = packet.envelope.envelope;
  request.cam = *cam;
  for (std::size_t i = 0; i < packet.fields.size(); ++i) {
    const auto &view = packet.fields[i];
    if(field_index(view.id)==state_field_capacity)return std::unexpected(Error{ErrorCode::unsupported_capability});
    std::size_t index = 0;
    while (index < request.count && request.fields[index] != view.id)
      ++index;
    if (index == request.count) {
      if (request.count == (profile==Profile::sdr_radio && request.cam.action==0?5u:4u))
        return std::unexpected(Error{ErrorCode::resource_limit});
      request.fields[index] = view.id;
      request.values[index] = codec::placeholder(view.id);
      ++request.count;
    }
    request.attributes[index] |= attribute_bit(view.attribute);
    if(profile==Profile::sdr_radio&&!profiles::iq::sdr_capability_field(view.id)&&view.id!=DiscreteIO32::id)request.unsupported_attributes[index]=true;
    const bool capability_attribute =
        view.attribute == Attribute::minimum ||
        view.attribute == Attribute::maximum;
    if (profile == Profile::sdr_radio && request.cam.action == 0 &&
        view.kind == BodyKind::selectors && capability_attribute) {
      request.capability_query = true;
      if (!profiles::iq::sdr_capability_field(view.id))
        request.unsupported_attributes[index] = true;
    } else if (view.attribute != Attribute::current ||
               (view.id == RFReferenceFrequency::id &&
                profile != Profile::iq_frequency_tunable &&
                profile != Profile::sdr_radio))
      request.unsupported_attributes[index] = true;
    else if (view.kind == BodyKind::values) {
      auto value = view.value();
      if (!value)
        return std::unexpected(value.error());
      request.values[index] = *value;
    }
  }
  if(profile==Profile::sdr_radio&&!request.count)return std::unexpected(Error{ErrorCode::invalid_argument});
  if (request.capability_query) {
    const auto required = attribute_bit(Attribute::minimum) |
                          attribute_bit(Attribute::maximum);
    for (std::size_t i = 0; i < request.count; ++i)
      if (request.attributes[i] != required)
        request.unsupported_attributes[i] = true;
  }
  return request;
}
inline void mark_not_executed(Diagnostics &d) noexcept {
  if (d.warnings && !d.errors)
    d.warnings |= not_executed;
  else
    d.errors |= not_executed;
}
struct EngineOptions {
  Profile profile = Profile::iq_generator_v1;
  EffectSink effects{};
  const timing::SampleTimeline *timeline = nullptr;
  bool external_retention = false;
  void *observation_context = nullptr;
  StateSnapshot (*observe)(void *, const StateSnapshot &,
                           const OperationContext &) noexcept = nullptr;
  TraceBinding trace{};
  std::shared_ptr<void> backend_owner{};
  const profiles::iq::SdrCapabilities *sdr_capabilities = nullptr;
};
struct PendingEffectBoundary { timing::Boundary boundary{};bool ordinal_known=false; };
struct EngineDrainStatus {
  std::size_t active = 0, running = 0, responses = 0, capability_holders = 0,
              tickets_pending = 0;
  BackendQuiescence backend{};
  bool uncertain = false;
};
template <std::size_t Transactions = 8> class Engine {
  static_assert(Transactions > 0);
  static constexpr std::size_t result_count = Transactions * 4;
  struct PlanningStorage {
    Request request{};
    ExecutionPlan execution{};
    TraceKey trace_key{};
    std::array<FieldOutcome, 5> outcomes{};
    std::array<CompletionToken, 4> tokens{};
    std::array<std::uint64_t, 4> ticket_operations{};
    timing::TimingCapabilities timing{};
    timing::ProtocolTime requested_time{};
    bool time_known = false, data_running = false, schedule_valid = true;
    std::array<bool, 5> cancelled{};
    RevisionReservation revisions;
  };
  struct TransactionRecord {
    bool active = false, complete = false, revalidated = false, running = false;
    std::uint64_t generation = 1, sequence = 0;
    std::size_t step = 0, running_field = 0, response_read = 0;
    FixedVector<AckRecord, 3> responses;
    StateSnapshot hypothetical{};
  };
  struct Slot {
    PlanningStorage plan;
    TransactionRecord record;
    AdmissionBundle credits;
  };
  std::array<Slot, Transactions> slots_{};
  CompletionArena<result_count> tickets_;
  std::shared_ptr<ResultStorage<result_count>> results_ =
      std::make_shared<ResultStorage<result_count>>();
  AdmissionPool &admission_;
  Backend backend_;
  EngineOptions options_;
  StateSnapshot state_{};
  std::uint64_t next_ticket_operation_ = 1, next_sequence_ = 1,
                association_generation_ = 0;
  std::optional<std::size_t> effect_owner_{};
  bool faulted_ = false, quiescing_ = false;
  bool progressing_ = false;
  void observe_backend_faults() noexcept {
    for (auto &result : results_->slots) {
      auto &guard = result.guard;
      if (guard.cancelled.load(std::memory_order_acquire) &&
          guard.contradiction.exchange(false, std::memory_order_acq_rel) &&
          guard.association_generation == association_generation_) {
        faulted_ = true;
        const auto field = field_index(guard.field);
        if (field < state_field_capacity && state_.fields[field].validity != Validity::unknown) {
          state_.fields[field].validity = Validity::unknown;
          if (state_.version != UINT64_MAX)
            ++state_.version;
        }
      }
    }
  }
  Result<void> validate_plan(Slot &slot, const StateSnapshot &state) noexcept {
    auto &plan = slot.plan.execution;
    const auto &request = slot.plan.request;
    plan.count = request.count;
    plan.expected_state_version = state.version;
    bool all = true;
    bool has_rate=false,has_rf=false,has_bandwidth=false,has_gain=false,has_discrete=false;
    const Hertz* requested_rate=nullptr;const Hertz* requested_bandwidth=nullptr;
    for(std::size_t i=0;i<request.count;++i){has_rate|=request.fields[i]==SampleRate::id;has_rf|=request.fields[i]==RFReferenceFrequency::id;has_bandwidth|=request.fields[i]==Bandwidth::id;has_gain|=request.fields[i]==Gain::id;has_discrete|=request.fields[i]==DiscreteIO32::id;if(request.fields[i]==SampleRate::id)requested_rate=std::get_if<Hertz>(&request.values[i]);if(request.fields[i]==Bandwidth::id)requested_bandwidth=std::get_if<Hertz>(&request.values[i]);}
    const bool mixed=options_.profile==Profile::iq_frequency_tunable&&request.cam.action!=0&&has_rate&&has_rf;
    const bool sdr_configuration=request.count==4&&has_rate&&has_rf&&has_bandwidth&&has_gain;
    const bool sdr_lifecycle=request.count==1&&has_discrete;
    bool armed=false;
    for(const auto& other:slots_)if(&other!=&slot&&other.record.active&&!other.record.complete&&other.plan.request.cam.action==2&&other.plan.request.count==1&&other.plan.request.fields[0]==DiscreteIO32::id){
      const auto* value=std::get_if<std::uint32_t>(&other.plan.request.values[0]);
      armed|=value&&*value==3&&other.plan.execution.fields[0].eligible&&!other.plan.cancelled[0];
    }
    const bool configured=state.fields[1].validity==Validity::known&&state.fields[4].validity==Validity::known&&state.fields[5].validity==Validity::known&&state.fields[6].validity==Validity::known;
    const bool incomplete_Sdr=options_.profile==Profile::sdr_radio&&request.cam.action!=0&&
      (!(sdr_configuration||sdr_lifecycle)||(sdr_configuration&&(slot.plan.data_running||armed)));
    const bool incompatible_Sdr=requested_rate&&requested_bandwidth&&requested_bandwidth->q20>requested_rate->q20;
    for (std::size_t i = 0; i < request.count; ++i) {
      Validation v{request.values[i]};
      if (request.unsupported_attributes[i] || (options_.profile==Profile::sdr_radio&&!profiles::iq::sdr_capability_field(request.fields[i])&&request.fields[i]!=DiscreteIO32::id)) {
        v.diagnostics.errors = unsupported;
        v.resolvable = false;
      } else if (request.cam.action != 0) {
        if (options_.profile == Profile::iq_generator_v1)
          v = iq_validate(request.fields[i], request.values[i]);
        else if(options_.profile==Profile::iq_frequency_tunable)
          v=tunable_validate(request.fields[i],request.values[i]);
        else if(options_.profile==Profile::sdr_radio)
          v=sdr_validate(request.fields[i],request.values[i]);
        if(v.resolvable&&options_.profile==Profile::sdr_radio&&options_.sdr_capabilities){const auto& capabilities=*options_.sdr_capabilities;bool supported=true;
          if(request.fields[i]==RFReferenceFrequency::id){const auto* value=std::get_if<Hertz>(&request.values[i]);supported=value&&capabilities.center_values.accepts(value->q20)&&value->q20>=capabilities.center_frequency.minimum.q20&&value->q20<=capabilities.center_frequency.maximum.q20;}
          else if(request.fields[i]==SampleRate::id){const auto* value=std::get_if<Hertz>(&request.values[i]);supported=value&&capabilities.sample_rate_values.accepts(value->q20)&&value->q20>=capabilities.sample_rate.minimum.q20&&value->q20<=capabilities.sample_rate.maximum.q20;}
          else if(request.fields[i]==Bandwidth::id){const auto* value=std::get_if<Hertz>(&request.values[i]);supported=value&&capabilities.bandwidth_values.accepts(value->q20)&&value->q20>=capabilities.bandwidth.minimum.q20&&value->q20<=capabilities.bandwidth.maximum.q20;}
          else if(request.fields[i]==Gain::id){const auto* value=std::get_if<GainStages>(&request.values[i]);supported=value&&capabilities.gain_values.accepts(value->stage1_q7)&&value->stage1_q7>=capabilities.gain.minimum.stage1_q7&&value->stage1_q7<=capabilities.gain.maximum.stage1_q7&&value->stage2_q7>=capabilities.gain.minimum.stage2_q7&&value->stage2_q7<=capabilities.gain.maximum.stage2_q7;}
          if(!supported){v.resolvable=false;v.diagnostics.errors|=range_error;}}
        if (v.resolvable && backend_.validate) {
          auto extra = backend_.validate(backend_.context, request.fields[i],
                                         v.adjusted, state);
          if(options_.profile==Profile::sdr_radio&&extra.adjusted!=v.adjusted){extra.resolvable=false;extra.diagnostics.errors|=precision;}
          v.adjusted = extra.adjusted;
          v.diagnostics.warnings |= extra.diagnostics.warnings;
          v.diagnostics.errors |= extra.diagnostics.errors;
          v.resolvable = v.resolvable && extra.resolvable;
          v.dependencies = extra.dependencies;
        }
        if (v.resolvable) {
          auto native = validate_value(request.fields[i], v.adjusted);
          if(options_.profile==Profile::iq_frequency_tunable&&!tunable_validate(request.fields[i],v.adjusted).resolvable)native=std::unexpected(Error{ErrorCode::invalid_argument});
          if(options_.profile==Profile::sdr_radio&&!sdr_validate(request.fields[i],v.adjusted).resolvable)native=std::unexpected(Error{ErrorCode::invalid_argument});
          if (!native) {
            v.diagnostics.errors |= invalid_value;
            v.resolvable = false;
          }
        }
      }
      if(mixed){v.resolvable=false;v.diagnostics.errors|=unsupported;}
      if(options_.profile==Profile::sdr_radio&&request.cam.action!=0&&request.cam.partial){v.resolvable=false;v.diagnostics.errors|=unsupported;}
      if(options_.profile==Profile::sdr_radio&&sdr_configuration&&request.cam.action==2&&!backend_.commit){v.resolvable=false;v.diagnostics.errors|=unsupported;}
      if(incomplete_Sdr){v.resolvable=false;v.diagnostics.errors|=unsupported;}
      if(options_.profile==Profile::sdr_radio&&request.cam.action!=0&&request.fields[i]==DiscreteIO32::id){const auto* discrete=std::get_if<std::uint32_t>(&request.values[i]);const bool start=discrete&&*discrete==3;if(!discrete||(start&&(slot.plan.data_running||armed||!configured))||(start&&request.cam.timing!=1)||(!start&&request.cam.timing!=0)){v.resolvable=false;v.diagnostics.errors|=timing_error;}}
      if(incompatible_Sdr&&request.fields[i]==Bandwidth::id){v.resolvable=false;v.diagnostics.errors|=range_error;}
      if(options_.profile==Profile::sdr_radio&&sdr_configuration&&requested_rate&&requested_bandwidth&&options_.sdr_capabilities&&!options_.sdr_capabilities->supports(*requested_bandwidth,*requested_rate)&&request.fields[i]==Bandwidth::id){v.resolvable=false;v.diagnostics.errors|=range_error;}
      plan.fields[i] = {request.fields[i],
                        request.values[i],
                        v.adjusted,
                        0,
                        eligible(request.cam, v.diagnostics, v.resolvable),
                        v.diagnostics,
                        slot.plan.time_known,
                        slot.plan.data_running};
      for (std::size_t target = 0; target < state_field_capacity; ++target)
        if (v.dependencies & (1u << target)) {
          bool found = false;
          for (std::size_t j = 0; j < request.count; ++j)
            if (request.fields[j] == state_fields[target]) {
              plan.fields[i].dependency_mask |= 1u << j;
              found = true;
            }
          if (!found && state.fields[target].validity != Validity::known) {
            plan.fields[i].eligible = false;
            plan.fields[i].diagnostics.errors |= dependency_blocked;
          }
        }
    }
    // Stable bounded topological order; cycles reject before backend begin.
    std::uint8_t visited = 0;
    for (std::size_t position = 0; position < plan.count; ++position) {
      bool found = false;
      for (std::size_t i = 0; i < plan.count; ++i)
        if (!(visited & (1u << i)) &&
            !(plan.fields[i].dependency_mask & ~visited)) {
          plan.order[position] = static_cast<std::uint8_t>(i);
          visited |= 1u << i;
          found = true;
          break;
        }
      if (!found)
        return std::unexpected(Error{ErrorCode::invalid_argument});
    }
    for (std::size_t position = 0; position < plan.count; ++position) {
      auto &field = plan.fields[plan.order[position]];
      for (std::size_t dependency = 0; dependency < plan.count; ++dependency)
        if ((field.dependency_mask & (1u << dependency)) &&
            !plan.fields[dependency].eligible) {
          field.eligible = false;
          field.diagnostics.errors |= dependency_blocked;
        }
      all = all && field.eligible;
    }
    if (request.cam.action != 0 && backend_.validate_plan) {
      auto whole = backend_.validate_plan(backend_.context, plan, state);
      if (!whole) {
        all = false;
        for (auto &field : plan.fields) {
          field.eligible = false;
          field.diagnostics.errors |= invalid_value;
        }
      }
    }
    if ((!request.cam.partial || options_.profile==Profile::sdr_radio) && !all)
      for (std::size_t i = 0; i < plan.count; ++i)
        plan.fields[i].eligible = false;
    for (std::size_t i = 0; i < plan.count; ++i)
      if (!plan.fields[i].eligible)
        mark_not_executed(plan.fields[i].diagnostics);
    return {};
  }
  void validate_boundary_state(Slot &slot) noexcept {
    if (!options_.timeline || slot.plan.request.cam.action == 0)
      return;
    for (std::size_t i = 0; i < slot.plan.execution.count; ++i) {
      auto &field = slot.plan.execution.fields[i];
      if (!field.eligible || field.id != SampleRate::id)
        continue;
      auto timeline = *options_.timeline;
      const auto q = std::get<Hertz>(field.adjusted).q20;
      bool valid = true;
      if (slot.plan.data_running) {
        const auto ordinal = slot.plan.execution.boundary.sample_ordinal;
        if (ordinal < timeline.ordinal() ||
            slot.plan.execution.boundary.committed)
          valid = false;
        else {
          auto advanced = timeline.advance(ordinal - timeline.ordinal());
          valid = bool(advanced) &&
                  timeline.time() == slot.plan.execution.boundary.time;
        }
      }
      if (valid)
        valid = q > 0 && q % (1ll << 20) == 0 &&
                bool(timeline.change_rate(static_cast<std::uint64_t>(q >> 20)));
      if (!valid) {
        field.eligible = false;
        field.diagnostics.errors |= resource_exhausted | not_executed;
      }
    }
    bool all = true;
    for (std::size_t p = 0; p < slot.plan.execution.count; ++p) {
      auto &field = slot.plan.execution.fields[slot.plan.execution.order[p]];
      for (std::size_t d = 0; d < slot.plan.execution.count; ++d)
        if ((field.dependency_mask & (1u << d)) &&
            !slot.plan.execution.fields[d].eligible) {
          field.eligible = false;
          field.diagnostics.errors |= dependency_blocked | not_executed;
        }
      all = all && field.eligible;
    }
    if ((!slot.plan.request.cam.partial || options_.profile==Profile::sdr_radio) && !all)
      for (std::size_t i = 0; i < slot.plan.execution.count; ++i) {
        slot.plan.execution.fields[i].eligible = false;
        mark_not_executed(slot.plan.execution.fields[i].diagnostics);
      }
  }
  void queue_ack(Slot &slot, AckKind kind, const StateSnapshot &observed,
                 const OperationContext &now) noexcept {
    AckRecord ack;
    ack.epoch = now.clock.epoch == timing::Epoch::gps   ? codec::Tsi::gps
                : now.clock.epoch == timing::Epoch::utc ? codec::Tsi::utc
                                                        : codec::Tsi::other;
    ack.request = slot.plan.request.envelope;
    ack.cam = slot.plan.request.cam;
    ack.kind = kind;
    ack.hypothetical = ack.cam.action == 1;
    ack.state =
        kind == AckKind::state && options_.observe
            ? options_.observe(options_.observation_context, observed, now)
            : observed;
    std::size_t successful = 0;
    bool timing_failure = false;
    for (std::size_t i = 0; i < slot.plan.execution.count; ++i) {
      const auto &field = slot.plan.execution.fields[i];
      const auto index = field_index(field.id);
      if (kind == AckKind::validation) {
        ack.diagnostics[index] = field.diagnostics;
        successful += field.eligible;
        timing_failure |= bool(field.diagnostics.errors & timing_error);
      } else if (kind == AckKind::execution) {
        ack.diagnostics[index] = slot.plan.outcomes[i].diagnostics;
        successful += slot.plan.outcomes[i].status == FieldStatus::executed;
        timing_failure |= bool(ack.diagnostics[index].errors & timing_error);
        const auto &outcome = slot.plan.outcomes[i];
        if (outcome.time_known &&
            (!ack.time_known || ack.time < outcome.actual_time)) {
          ack.time = outcome.actual_time;
          ack.time_known = true;
        }
      } else if (!slot.plan.request.unsupported_attributes[i] &&
                 (slot.plan.request.capability_query || observed.fields[index].validity == Validity::known)) {
        ack.selected_mask |= 1u << index;
        ++successful;
      }
    }
    if (kind == AckKind::state && ack.selected_mask &&
        slot.plan.request.capability_query)
      ack.sdr_capabilities = options_.sdr_capabilities;
    const auto count = slot.plan.execution.count;
    ack.partial = successful != count || timing_failure;
    if (kind == AckKind::state)
      ack.scheduled_or_executed = successful != 0;
    else
      ack.scheduled_or_executed = successful == count && !timing_failure;
    if (kind == AckKind::execution && (ack.cam.action == 0 || count == 0)) {
      ack.partial = false;
      ack.scheduled_or_executed = false;
    }
    if (kind == AckKind::validation && slot.plan.time_known &&
        !timing_failure && slot.plan.schedule_valid) {
      ack.time = slot.plan.execution.boundary.time;
      ack.time_known = true;
    }
    if (kind == AckKind::state &&
        (now.clock.state == timing::ClockState::locked ||
         now.clock.state == timing::ClockState::holdover)) {
      ack.time = now.clock.time;
      ack.time_known = true;
    }
    // A rejected Sdr operation has no effective time. Preserve the profile's
    // UTC/picosecond envelope using diagnostic observation time, without SchX.
    if(options_.profile==Profile::sdr_radio&&!ack.time_known&&
       (now.clock.state==timing::ClockState::locked||now.clock.state==timing::ClockState::holdover)){
      ack.time=now.clock.time;ack.time_known=true;
    }
    ack.timing = timing_failure ? 7 : ack.cam.timing;
    if (should_emit(ack.cam, kind, summary(ack))) {
      auto pushed = slot.record.responses.push_back(std::move(ack));
      (void)pushed;
    }
  }
  void finish(Slot &slot, const OperationContext &now) noexcept {
    const auto &observed =
        slot.plan.request.cam.action == 1 ? slot.record.hypothetical : state_;
    queue_ack(slot, AckKind::execution, observed, now);
    queue_ack(slot, AckKind::state, observed, now);
    slot.record.complete = true;
    slot.plan.revisions.reset();
    FieldStatus terminal = slot.plan.execution.count
                               ? FieldStatus::executed
                               : FieldStatus::not_executed;
    for (std::size_t i = 0; i < slot.plan.execution.count; ++i)
      if (slot.plan.outcomes[i].status != FieldStatus::executed) {
        terminal = slot.plan.outcomes[i].status;
        break;
      }
    options_.trace.emit(TraceStage::recorded, slot.plan.trace_key, {}, terminal,
                        slot.plan.request.cam.action == 1);
    if (effect_owner_ && &slots_[*effect_owner_] == &slot)
      effect_owner_.reset();
  }
  void apply_outcome(Slot &slot, std::size_t field, FieldOutcome outcome,
                     const OperationContext &now) noexcept {
    auto &planned = slot.plan.execution.fields[field];
    const auto index = field_index(planned.id);
    if (outcome.id != planned.id || outcome.status == FieldStatus::pending ||
        static_cast<unsigned>(outcome.status) >
            static_cast<unsigned>(FieldStatus::not_executed) ||
        (outcome.status == FieldStatus::executed &&
         outcome.validity != Validity::known) ||
        (outcome.status == FieldStatus::unknown_effect &&
         outcome.validity != Validity::unknown) ||
        (outcome.time_known && !timing::valid(outcome.actual_time))) {
      outcome = {};
      outcome.id = planned.id;
      outcome.status = FieldStatus::unknown_effect;
      outcome.validity = Validity::unknown;
      outcome.diagnostics.errors = device_failure | state_indeterminate;
    }
    outcome.simulated = slot.plan.request.cam.action == 1;
    if (outcome.status == FieldStatus::unknown_effect && !outcome.simulated)
      faulted_ = true;
    outcome.diagnostics.warnings |= planned.diagnostics.warnings;
    outcome.diagnostics.errors |= planned.diagnostics.errors;
    if (outcome.status == FieldStatus::executed) {
      auto valid = validate_value(planned.id, outcome.value);
      if(options_.profile==Profile::iq_frequency_tunable&&!tunable_validate(planned.id,outcome.value).resolvable)valid=std::unexpected(Error{ErrorCode::invalid_argument});
      if (!valid) {
        outcome.status = FieldStatus::unknown_effect;
        outcome.validity = Validity::unknown;
        outcome.diagnostics.errors |= device_failure | state_indeterminate;
      }
    }
    if (outcome.status == FieldStatus::unknown_effect && !outcome.simulated)
      faulted_ = true;
    if (outcome.status != FieldStatus::executed)
      mark_not_executed(outcome.diagnostics);
    if (quiescing_ && slot.plan.request.cam.timing &&
        outcome.status != FieldStatus::executed)
      outcome.diagnostics.errors |= timing_error;
    if (slot.plan.request.cam.timing &&
        outcome.status == FieldStatus::executed) {
      bool allowed = false;
      if (outcome.time_known) {
        auto early =
            timing::subtract(outcome.actual_time,
                             timing::from_picoseconds(outcome.uncertainty_ps));
        auto late =
            timing::add(outcome.actual_time,
                        timing::from_picoseconds(outcome.uncertainty_ps));
        if (early && late) {
          auto fit = timing::effect_interval_allowed(
              slot.plan.request.cam.timing, slot.plan.requested_time, *early,
              *late, slot.plan.timing);
          allowed = fit && *fit;
        }
      }
      if (!allowed)
        outcome.diagnostics.errors |= timing_error;
    }
    auto &state = outcome.simulated ? slot.record.hypothetical : state_;
    if (outcome.status == FieldStatus::executed ||
        outcome.status == FieldStatus::unknown_effect) {
      auto next = state.fields[index];
      next.id = planned.id;
      next.validity = outcome.status == FieldStatus::unknown_effect
                          ? Validity::unknown
                          : Validity::known;
      if (next.validity == Validity::known)
        next.value = outcome.value;
      if (next.validity != state.fields[index].validity ||
          (next.validity == Validity::known &&
           next.value != state.fields[index].value)) {
        state.fields[index] = next;
        ++state.version;
        if (!outcome.simulated && options_.effects.record) {
          EffectiveEvent event{state,
                               outcome.actual_time,
                               outcome.sample_ordinal,
                               slot.plan.execution.operation,
                               slot.plan.execution.association_generation,
                               static_cast<std::uint8_t>(1u << index),
                               outcome.time_known,
                               outcome.ordinal_known,
                               outcome};
          if (state.profile == profiles::iq::Profile::sdr_radio &&
              planned.id == DiscreteIO32::id &&
              outcome.status == FieldStatus::executed &&
              std::get_if<std::uint32_t>(&outcome.value) &&
              std::get<std::uint32_t>(outcome.value) == 3)
            event.sample_epoch = slot.plan.requested_time;
          AdmissionRequest allocation;
          allocation.need(Resource::revision)
              .need(Resource::context_publication);
          auto credits = slot.credits.transfer(allocation);
          if (credits)
            options_.effects.record(options_.effects.context, event,
                                    slot.plan.revisions, std::move(*credits));
        }
      }
    }
    slot.plan.outcomes[field] = outcome;
    slot.record.running = false;
    ++slot.record.step;
    if (outcome.status != FieldStatus::executed &&
        !slot.plan.request.cam.partial) {
      for (std::size_t p = slot.record.step; p < slot.plan.execution.count;
           ++p) {
        auto f = slot.plan.execution.order[p];
        slot.plan.execution.fields[f].eligible = false;
        slot.plan.execution.fields[f].diagnostics.errors |= not_executed;
      }
    }
    (void)now;
  }
  void consume(CompletionRecord record, const OperationContext &now) noexcept {
    for (std::size_t slot_index = 0; slot_index < Transactions; ++slot_index) {
      auto &slot = slots_[slot_index];
      if (!slot.record.active || !slot.record.running)
        continue;
      const auto field = slot.record.running_field;
      if (slot.plan.ticket_operations[field] != record.operation)
        continue;
      FieldOutcome outcome;
      outcome.id = slot.plan.execution.fields[field].id;
      const auto result_index = slot_index * 4 + field;
      if (record.result.status == CompletionStatus::succeeded &&
          record.result.value == result_index) {
        outcome = results_->slots[result_index].outcome;
        options_.trace.emit_at(TraceStage::device_done, slot.plan.trace_key,
                               results_->slots[result_index].device_done_ns,
                               outcome.id, outcome.status, outcome.simulated);
      } else {
        outcome.status = FieldStatus::unknown_effect;
        outcome.validity = Validity::unknown;
        outcome.diagnostics.errors = device_failure | state_indeterminate;
      }
      apply_outcome(slot, field, outcome, now);
      return;
    }
  }

public:
  Engine(AdmissionPool &admission, Backend backend, StateSnapshot initial,
         EngineOptions options = {})
      : admission_(admission), backend_(backend), options_(options),
        state_(initial) {
    results_->trace = options_.trace;
    results_->backend_owner = options_.backend_owner;
  }
  const StateSnapshot &state() const noexcept { return state_; }
  std::optional<PendingEffectBoundary> pending_effect_boundary() const noexcept {
    std::optional<PendingEffectBoundary> first;
    for(const auto& slot:slots_){if(!slot.record.active||slot.record.complete||slot.plan.request.cam.action!=2)continue;
      bool pending=false;for(std::size_t i=0;i<slot.plan.execution.count;++i)pending|=slot.plan.execution.fields[i].eligible&&slot.plan.outcomes[i].status==FieldStatus::pending;
      if(!pending)continue;
      PendingEffectBoundary candidate{slot.plan.execution.boundary,slot.plan.data_running};
      if(!candidate.ordinal_known)return candidate;
      if(!first||candidate.boundary.sample_ordinal<first->boundary.sample_ordinal)first=candidate;
    }return first;
  }

  bool faulted() const noexcept { return faulted_; }
  bool quiescing() const noexcept { return quiescing_; }
  std::uint64_t association_generation() const noexcept {
    return association_generation_;
  }
  EngineDrainStatus drain_status() const noexcept {
    EngineDrainStatus status;
    status.uncertain = faulted_;
    for (const auto &slot : slots_) {
      status.active += slot.record.active;
      status.running += slot.record.active && slot.record.running;
      if (slot.record.active)
        status.responses +=
            slot.record.responses.size() - slot.record.response_read;
    }
    for (std::size_t i = 0; i < result_count; ++i) {
      status.capability_holders +=
          results_->slots[i].guard.holders.load(std::memory_order_acquire);
      const auto state = tickets_.state(i);
      status.tickets_pending +=
          state != TicketState::free && state != TicketState::retired;
      status.uncertain |= results_->slots[i].guard.contradiction.load(
          std::memory_order_acquire);
    }
    if (backend_.quiescence)
      status.backend = backend_.quiescence(backend_.context);
    return status;
  }
  bool safe_to_reset() const noexcept {
    const auto status = drain_status();
    return !progressing_ && !status.active && !status.responses &&
           !status.capability_holders && !status.tickets_pending &&
           status.backend.known && status.backend.quiescent &&
           status.backend.pending == 0;
  }
  Result<void> reset_state(const StateSnapshot &confirmed,
                           std::uint64_t generation) noexcept {
    auto snapshot_valid=validate_snapshot(confirmed);if(!snapshot_valid)return snapshot_valid;
    if (!generation || generation <= association_generation_)
      return std::unexpected(Error{ErrorCode::stale_generation});
    for (std::size_t i = 0; i < active_state_fields(confirmed.profile); ++i) {
      const auto &field = confirmed.fields[i];
      if (field.id != state_fields[i] ||
          static_cast<unsigned>(field.validity) >
              static_cast<unsigned>(Validity::unknown) ||
          field.validity == Validity::unknown)
        return std::unexpected(Error{ErrorCode::invalid_argument});
      if (field.validity == Validity::known) {
        auto valid = validate_value(field.id, field.value);
        if (!valid)
          return std::unexpected(valid.error());
      }
    }
    if (options_.profile != Profile::generic_virtual_test &&
        (confirmed.fields[1].validity != Validity::known ||
         confirmed.fields[3].validity != Validity::known ||
         (options_.profile==Profile::iq_frequency_tunable&&confirmed.fields[4].validity!=Validity::known)))
      return std::unexpected(Error{ErrorCode::invalid_state});
    if((options_.profile==Profile::iq_frequency_tunable)!=(confirmed.profile==profiles::iq::Profile::frequency_tunable))return std::unexpected(Error{ErrorCode::invalid_argument});
    if (!safe_to_reset())
      return std::unexpected(Error{ErrorCode::invalid_state});
    state_ = confirmed;
    association_generation_ = generation;
    faulted_ = quiescing_ = false;
    effect_owner_.reset();
    for (auto &result : results_->slots) {
      result.guard.cancelled.store(false, std::memory_order_release);
      result.guard.contradiction.store(false, std::memory_order_release);
    }
    return {};
  }
  Result<void> request_quiesce(const OperationContext &now) noexcept {
    if (progressing_)
      return std::unexpected(Error{ErrorCode::would_deadlock});
    if (!now.association_generation ||
        (association_generation_ &&
         now.association_generation != association_generation_))
      return std::unexpected(Error{ErrorCode::stale_generation});
    quiescing_ = true;
    tickets_.scan(
        [&](CompletionRecord record) noexcept { consume(record, now); });
    for (std::size_t index = 0; index < Transactions; ++index) {
      auto &slot = slots_[index];
      if (!slot.record.active || slot.record.complete)
        continue;
      for (std::size_t field = 0; field < slot.plan.execution.count; ++field)
        if (slot.plan.outcomes[field].status == FieldStatus::pending) {
          if (slot.record.running && slot.record.running_field == field) {
            if (slot.plan.cancelled[field])
              continue; // one disarm attempt per local shutdown
            slot.plan.cancelled[field] = true;
            const auto result_index = index * 4 + field;
            AsyncResult capability{
                slot.plan.tokens[field].publisher(),          results_,
                &results_->slots[result_index].outcome,       result_index,
                &results_->slots[result_index].guard,         &results_->trace,
                &results_->slots[result_index].device_done_ns};
            const auto disarmed =
                backend_.disarm ? backend_.disarm(backend_.context, capability)
                                : DisarmResult::not_cancelled;
            if (disarmed == DisarmResult::cancelled) {
              results_->slots[result_index].guard.cancelled.store(
                  true, std::memory_order_release);
              FieldOutcome outcome;
              outcome.id = slot.plan.execution.fields[field].id;
              outcome.status = FieldStatus::cancelled;
              outcome.diagnostics.errors =
                  not_executed |
                  (slot.plan.request.cam.timing ? timing_error : 0);
              if (!capability.complete(outcome)) {
                results_->slots[result_index].guard.contradiction.store(
                    true, std::memory_order_release);
                faulted_ = true;
              }
            } else if (disarmed == DisarmResult::unknown_effect) {
              FieldOutcome outcome;
              outcome.id = slot.plan.execution.fields[field].id;
              outcome.status = FieldStatus::unknown_effect;
              outcome.validity = Validity::unknown;
              outcome.diagnostics.errors =
                  device_failure | state_indeterminate |
                  (slot.plan.request.cam.timing ? timing_error : 0);
              capability.complete(outcome);
            }
          } else
            slot.plan.cancelled[field] = true;
        }
    }
    return progress(now);
  }
  struct PendingTime {
    timing::ProtocolTime requested;
    unsigned mode;
  };
  FixedVector<PendingTime, Transactions> pending_times() const noexcept {
    FixedVector<PendingTime, Transactions> result;
    for (const auto &slot : slots_)
      if (slot.record.active && !slot.record.complete &&
          slot.plan.request.cam.action == 2)
        result.push_back(
            {slot.plan.requested_time, slot.plan.request.cam.timing});
    return result;
  }
  bool external_retention() const noexcept {
    return options_.external_retention;
  }
  static constexpr std::size_t plan_bytes() noexcept {
    return sizeof(PlanningStorage);
  }
  static constexpr std::size_t record_bytes() noexcept {
    return sizeof(TransactionRecord);
  }
  Result<Handle> accept(const codec::PacketView &packet,
                        const OperationContext &now) noexcept {
    observe_backend_faults();
    auto current_state_valid=validate_snapshot(state_);
    if(!current_state_valid||(options_.profile==Profile::iq_frequency_tunable)!=(state_.profile==profiles::iq::Profile::frequency_tunable))return std::unexpected(Error{ErrorCode::invalid_argument});
    if (!options_.trace.valid())
      return std::unexpected(Error{ErrorCode::invalid_argument});
    if (quiescing_)
      return std::unexpected(Error{ErrorCode::invalid_state});
    if (!now.association_generation ||
        bool(options_.effects.reserve) != bool(options_.effects.record))
      return std::unexpected(Error{ErrorCode::invalid_state});
    if (association_generation_ &&
        association_generation_ != now.association_generation)
      return std::unexpected(Error{ErrorCode::invalid_state});
    auto request = request_from(packet, options_.profile);
    if (!request)
      return std::unexpected(request.error());
    if ((request->cam.action == 2 && !backend_.begin) ||
        (request->cam.action == 1 && !backend_.simulate))
      return std::unexpected(Error{ErrorCode::unsupported_capability});
    if (faulted_ && request->cam.action == 2)
      return std::unexpected(Error{ErrorCode::invalid_state});
    if (request->cam.timing) {
      const auto epoch = now.clock.epoch == timing::Epoch::gps ? codec::Tsi::gps
                         : now.clock.epoch == timing::Epoch::utc
                             ? codec::Tsi::utc
                             : codec::Tsi::other;
      if (request->envelope.timestamp.tsf != codec::Tsf::picoseconds ||
          request->envelope.timestamp.tsi != epoch)
        return std::unexpected(Error{ErrorCode::unsupported_capability});
    }
    std::size_t index = Transactions;
    for (std::size_t i = 0; i < Transactions; ++i)
      if (!slots_[i].record.active &&
          slots_[i].record.generation != UINT64_MAX) {
        bool pinned = false;
        for (std::size_t f = 0; f < 4; ++f)
          pinned |= results_->slots[i * 4 + f].guard.holders.load(
                        std::memory_order_acquire) != 0 ||
                    (results_->slots[i * 4 + f].guard.cancelled.load(
                         std::memory_order_acquire) &&
                     results_->slots[i * 4 + f].guard.contradiction.load(
                         std::memory_order_acquire));
        if (!pinned) {
          index = i;
          break;
        }
      }
    observe_backend_faults();
    if (faulted_ && request->cam.action == 2)
      return std::unexpected(Error{ErrorCode::invalid_state});
    if (index == Transactions)
      return std::unexpected(Error{ErrorCode::capacity_exhausted});
    if (state_.version > UINT64_MAX - 4 ||
        next_ticket_operation_ > UINT64_MAX - 4 || next_sequence_ == UINT64_MAX)
      return std::unexpected(Error{ErrorCode::overflow});
    AdmissionRequest required;
    required.need(Resource::transaction)
        .need(Resource::plan_bytes, sizeof(PlanningStorage))
        .need(Resource::completion, 4)
        .need(Resource::response, 3)
        .need(Resource::ordinary_queue);
    if (!options_.external_retention)
      required.need(Resource::duplicate_entry)
          .need(Resource::duplicate_bytes, 2048);
    if (request->cam.action == 2)
      required.need(Resource::revision, request->count)
          .need(Resource::context_publication, request->count);
    if (request->cam.timing && request->cam.action == 2)
      required.need(Resource::schedule);
    auto credits = admission_.acquire(required);
    if (!credits)
      return std::unexpected(credits.error());
    auto &slot = slots_[index];
    slot.plan = PlanningStorage{};
    slot.record.responses.clear();
    slot.record.complete = false;
    slot.record.revalidated = false;
    slot.record.running = false;
    slot.record.step = slot.record.response_read = 0;
    slot.record.hypothetical = state_;
    if (options_.effects.reserve && request->cam.action == 2 &&
        request->count) {
      auto revisions =
          options_.effects.reserve(options_.effects.context, request->count);
      if (!revisions)
        return std::unexpected(revisions.error());
      slot.plan.revisions = std::move(*revisions);
    }
    slot.plan.request = *request;
    slot.plan.trace_key = {now.association_generation, now.operation,
                           now.trace_peer,
                           request->envelope.stream_id.value_or(0),
                           request->envelope.command->message_id};
    slot.plan.execution.operation = now.operation;
    slot.plan.execution.association_generation = now.association_generation;
    slot.plan.timing = now.timing;
    const bool immediate_stop=options_.profile==Profile::sdr_radio&&request->count==1&&request->fields[0]==DiscreteIO32::id&&request->values[0]==SemanticValue{std::uint32_t{2}};
    slot.plan.data_running = now.data_running&&!immediate_stop;
    slot.plan.time_known = now.clock.state == timing::ClockState::locked ||
                           now.clock.state == timing::ClockState::holdover;
    slot.plan.requested_time = {request->envelope.timestamp.integer,
                                request->envelope.timestamp.fractional};
    auto validated = validate_plan(slot, state_);
    if (!validated) {
      slot.plan.revisions.reset();
      return std::unexpected(validated.error());
    }
    slot.plan.execution.boundary = {
        now.clock.time, 0, now.clock.mapping_generation, false, true, 0};
    if (request->cam.action!=0 && (slot.plan.data_running || request->cam.timing)) {
      const auto *discrete = request->count == 1 &&
                     request->fields[0] == DiscreteIO32::id
                   ? std::get_if<std::uint32_t>(&request->values[0])
                   : nullptr;
      const bool sdr_start = options_.profile == Profile::sdr_radio &&
                  !now.data_running && discrete && *discrete == 3;
      const std::array requested_boundary{timing::Boundary{
        slot.plan.requested_time, 0, now.clock.mapping_generation, false,
        true, 0}};
      const std::span<const timing::Boundary> boundaries =
        sdr_start ? requested_boundary : now.boundaries;
      auto chosen = timing::choose_boundary(
        request->cam.timing, slot.plan.requested_time, now.clock, boundaries,
        now.timing);
      if (chosen)
        slot.plan.execution.boundary = *chosen;
      else {
        slot.plan.schedule_valid = false;
        for (std::size_t i = 0; i < slot.plan.execution.count; ++i) {
          slot.plan.execution.fields[i].eligible = false;
          slot.plan.execution.fields[i].diagnostics.errors |=
              timing_error | not_executed;
        }
      }
    }
    if(options_.profile==Profile::sdr_radio&&request->cam.action!=0&&request->cam.timing){
      auto earliest=timing::add(now.clock.time,timing::Duration{0,20'000'000'000});
      auto latest=timing::add(now.clock.time,timing::Duration{10,0});
      if(!earliest||!latest||slot.plan.requested_time < *earliest || slot.plan.requested_time > *latest){
        slot.plan.schedule_valid=false;
        for(std::size_t i=0;i<slot.plan.execution.count;++i){slot.plan.execution.fields[i].eligible=false;slot.plan.execution.fields[i].diagnostics.errors|=timing_error|not_executed;}
      }
    }
    // Stop disarms scheduled starts in this serialized domain before they can
    // reach the physical backend. Cancellation uses the ordinary terminal path.
    if(options_.profile==Profile::sdr_radio&&request->cam.action==2&&request->count==1&&request->fields[0]==DiscreteIO32::id&&request->values[0]==SemanticValue{std::uint32_t{2}}&&slot.plan.execution.fields[0].eligible){
      for(auto& other:slots_)if(&other!=&slot&&other.record.active&&!other.record.complete&&!other.record.running&&other.plan.request.count==1&&other.plan.request.fields[0]==DiscreteIO32::id&&other.plan.request.values[0]==SemanticValue{std::uint32_t{3}}){
        other.plan.cancelled[0]=true;other.plan.schedule_valid=false;
      }
    }
    validate_boundary_state(slot);
    for (std::size_t i = 0; i < 4; ++i) {
      const auto operation = next_ticket_operation_++;
      auto ticket = tickets_.reserve(operation);
      if (!ticket) {
        for (auto &token : slot.plan.tokens) {
          const auto own = token.is_reserved();
          const auto ticket_slot = token.publisher().slot();
          token = CompletionToken{};
          if (own)
            tickets_.consume(ticket_slot);
        }
        slot.plan.revisions.reset();
        return std::unexpected(ticket.error());
      }
      slot.plan.tokens[i] = std::move(*ticket);
      slot.plan.ticket_operations[i] = operation;
      results_->slots[index * 4 + i].busy = true;
      results_->slots[index * 4 + i].generation = slot.record.generation;
      auto &guard = results_->slots[index * 4 + i].guard;
      guard.cancelled.store(false, std::memory_order_relaxed);
      guard.contradiction.store(false, std::memory_order_relaxed);
      guard.association_generation = now.association_generation;
      guard.field = i < slot.plan.execution.count
                        ? slot.plan.execution.fields[i].id
                        : FieldId{};
    }
    association_generation_ = now.association_generation;
    slot.credits = std::move(*credits);
    slot.record.active = true;
    slot.record.sequence = next_sequence_++;
    options_.trace.emit(TraceStage::validated, slot.plan.trace_key);
    queue_ack(slot, AckKind::validation, state_, now);
    return Handle{index, slot.record.generation};
  }
  Result<void> progress(const OperationContext &now) noexcept {
    if (progressing_)
      return std::unexpected(Error{ErrorCode::would_deadlock});
    struct Guard {
      bool &value;
      ~Guard() { value = false; }
    } guard{progressing_};
    progressing_ = true;
    observe_backend_faults();
    tickets_.scan(
        [&](CompletionRecord record) noexcept { consume(record, now); });
    bool running = false;
    for (const auto &slot : slots_)
      running |= slot.record.active && slot.record.running;
    if (!effect_owner_) {
      std::uint64_t earliest = UINT64_MAX;
      timing::ProtocolTime earliest_boundary{};
      for (std::size_t i = 0; i < Transactions; ++i) {
        const auto &candidate = slots_[i];
        if (!candidate.record.active || candidate.record.complete ||
            candidate.plan.request.cam.action != 2)
          continue;
        const bool scheduled =
            candidate.plan.request.cam.timing || candidate.plan.data_running;
        const auto boundary =
            scheduled ? candidate.plan.execution.boundary.time : now.clock.time;
        const bool clock_qualified =
            now.clock.state == timing::ClockState::locked ||
            now.clock.state == timing::ClockState::holdover;
        if (scheduled && candidate.plan.schedule_valid && clock_qualified &&
            boundary > now.clock.time)
          continue;
        if (!effect_owner_ || boundary < earliest_boundary ||
            (boundary == earliest_boundary &&
             candidate.record.sequence < earliest)) {
          earliest = candidate.record.sequence;
          earliest_boundary = boundary;
          effect_owner_ = i;
        }
      }
    }
    for (std::size_t index = 0; index < Transactions; ++index) {
      auto &slot = slots_[index];
      if (!slot.record.active || slot.record.complete)
        continue;
      const auto action = slot.plan.request.cam.action;
      if (quiescing_ && !slot.record.running) {
        for (std::size_t field = 0; field < slot.plan.execution.count; ++field)
          if (slot.plan.outcomes[field].status == FieldStatus::pending) {
            auto &outcome = slot.plan.outcomes[field];
            outcome.id = slot.plan.execution.fields[field].id;
            outcome.status = FieldStatus::cancelled;
            outcome.diagnostics.errors =
                not_executed |
                (slot.plan.request.cam.timing ? timing_error : 0);
          }
        slot.record.step = slot.plan.execution.count;
        finish(slot, now);
        continue;
      }
      if (running && action != 0)
        continue;
      if (action == 2 && (!effect_owner_ || *effect_owner_ != index))
        continue;
      if (action == 0) {
        finish(slot, now);
        continue;
      }
      if (!slot.record.revalidated) {
        const auto admitted=slot.plan.execution;
        const auto& request=slot.plan.request;
        const bool immediate_stop=options_.profile==Profile::sdr_radio&&request.count==1&&request.fields[0]==DiscreteIO32::id&&request.values[0]==SemanticValue{std::uint32_t{2}};
        slot.plan.data_running=now.data_running&&!immediate_stop;
        auto validation = validate_plan(slot, state_);
        for(std::size_t i=0;i<slot.plan.execution.count;++i)if(!admitted.fields[i].eligible){slot.plan.execution.fields[i].eligible=false;slot.plan.execution.fields[i].diagnostics=admitted.fields[i].diagnostics;}
        if (!validation)
          for (std::size_t i = 0; i < slot.plan.execution.count; ++i) {
            slot.plan.execution.fields[i].eligible = false;
            slot.plan.execution.fields[i].diagnostics.errors |=
                invalid_value | not_executed;
          }
        if (faulted_ && action == 2)
          for (std::size_t i = 0; i < slot.plan.execution.count; ++i) {
            slot.plan.execution.fields[i].eligible = false;
            slot.plan.execution.fields[i].diagnostics.errors |=
                state_indeterminate | not_executed;
          }
        slot.record.hypothetical = state_;
        slot.record.revalidated = true;
      }
      if (slot.plan.request.cam.timing || slot.plan.data_running) {
        const auto *discrete = slot.plan.request.count == 1 &&
                   slot.plan.request.fields[0] ==
                   DiscreteIO32::id
                   ? std::get_if<std::uint32_t>(
                     &slot.plan.request.values[0])
                   : nullptr;
        const bool sdr_start = options_.profile == Profile::sdr_radio &&
                  !slot.plan.data_running && discrete &&
                  *discrete == 3;
        const bool qualified =
            (slot.plan.request.cam.timing == 0 && !slot.plan.data_running) ||
            ((now.clock.state == timing::ClockState::locked ||
              now.clock.state == timing::ClockState::holdover) &&
             (slot.plan.request.cam.timing == 0 || slot.plan.timing.qualified ||
              slot.plan.timing.injected));
        // A stopped SDR starts at an absolute protocol epoch, not an existing
        // sample boundary. PPS updates may remap the clock while it waits.
        // Preserve the admitted epoch; the qualified-clock and actual-effect
        // interval checks below still reject early/late execution.
        if (sdr_start && slot.plan.schedule_valid && qualified)
          slot.plan.execution.boundary.mapping_generation =
              now.clock.mapping_generation;
        bool boundary_available = false;
        if (sdr_start) {
          boundary_available = slot.plan.execution.boundary.time ==
                                   slot.plan.requested_time &&
                               slot.plan.execution.boundary.mapping_generation ==
                                   now.clock.mapping_generation;
        } else {
          for (const auto &boundary : now.boundaries)
            if (boundary.sample_ordinal ==
                    slot.plan.execution.boundary.sample_ordinal &&
                boundary.time == slot.plan.execution.boundary.time &&
                boundary.mapping_generation == now.clock.mapping_generation &&
                !boundary.committed && boundary.backend_ready)
              boundary_available = true;
        }
        if (!boundary_available)
          slot.plan.schedule_valid = false;
        if (!slot.plan.schedule_valid ||
            slot.plan.execution.boundary.mapping_generation !=
                now.clock.mapping_generation) {
            const std::array requested_boundary{timing::Boundary{
              slot.plan.requested_time, 0, now.clock.mapping_generation, false,
              true, 0}};
            const std::span<const timing::Boundary> boundaries =
              sdr_start ? requested_boundary : now.boundaries;
            auto chosen = timing::choose_boundary(
              slot.plan.request.cam.timing, slot.plan.requested_time, now.clock,
              boundaries, slot.plan.timing);
          slot.plan.schedule_valid = bool(chosen);
          if (chosen)
            slot.plan.execution.boundary = *chosen;
        }
        if (!qualified)
          slot.plan.schedule_valid = false;
        if (slot.plan.schedule_valid && action == 2 &&
            slot.plan.execution.boundary.time > now.clock.time &&
            [&]{for(std::size_t i=0;i<slot.plan.execution.count;++i)if(slot.plan.execution.fields[i].eligible&&!slot.plan.cancelled[i])return true;return false;}()) {
          effect_owner_.reset();
          continue;
        }
        if (slot.plan.schedule_valid && action == 2 &&
            slot.plan.request.cam.timing) {
          auto first = timing::subtract(
              now.clock.time,
              timing::from_picoseconds(now.clock.uncertainty_ps));
          auto last =
              timing::add(now.clock.time,
                          timing::from_picoseconds(now.clock.uncertainty_ps));
          auto fits = first && last ? timing::effect_interval_allowed(
                                          slot.plan.request.cam.timing,
                                          slot.plan.requested_time, *first,
                                          *last, slot.plan.timing)
                                    : Result<bool>{false};
          if (!fits || !*fits)
            slot.plan.schedule_valid = false;
        }
        if (!slot.plan.schedule_valid)
          for (std::size_t i = 0; i < slot.plan.execution.count; ++i) {
            slot.plan.execution.fields[i].eligible = false;
            slot.plan.execution.fields[i].diagnostics.errors |=
                timing_error | not_executed;
          }
      }
      validate_boundary_state(slot);
      if(options_.profile==Profile::sdr_radio&&slot.plan.execution.count==4&&action==2){
        bool ready=backend_.commit!=nullptr;
        for(std::size_t i=0;i<4;++i)ready&=slot.plan.execution.fields[i].eligible&&!slot.plan.cancelled[i];
        if(ready){
          // Admission may precede dispatch by seconds. Recheck the complete
          // operation against the live lifecycle before any physical effect.
          slot.plan.data_running=now.data_running;
          auto current=validate_plan(slot,state_);ready=bool(current);
          for(std::size_t i=0;i<4;++i)ready&=slot.plan.execution.fields[i].eligible;
        }
        if(ready){
          auto boundary=slot.plan.execution.boundary;boundary.time=now.clock.time;
          auto batch=backend_.commit(backend_.context,slot.plan.execution,boundary);
          if(batch.status!=FieldStatus::executed&&batch.status!=FieldStatus::failed&&batch.status!=FieldStatus::unknown_effect)
            batch.status=FieldStatus::unknown_effect;
          const auto previous=state_;
          // apply_outcome performs the usual validation and diagnostics; suppress
          // intermediate publication until the complete batch has been recorded.
          auto effects=options_.effects;options_.effects={};
          for(std::size_t i=0;i<4;++i){
            FieldOutcome out;out.id=slot.plan.execution.fields[i].id;
            out.value=slot.plan.execution.fields[i].adjusted;out.status=batch.status;
            out.validity=batch.status==FieldStatus::executed?Validity::known:batch.status==FieldStatus::unknown_effect?Validity::unknown:Validity::absent;
            out.actual_time=batch.actual_time;out.time_known=batch.time_known;out.uncertainty_ps=batch.uncertainty_ps;
            if(batch.status!=FieldStatus::executed)out.diagnostics.errors=device_failure|(batch.status==FieldStatus::unknown_effect?state_indeterminate:0);
            apply_outcome(slot,i,out,now);
          }
          options_.effects=effects;
          if(state_.version!=previous.version){
            state_.version=previous.version+1;
            if(effects.record){
              EffectiveEvent event{state_,batch.actual_time,0,slot.plan.execution.operation,slot.plan.execution.association_generation,0x72,batch.time_known,false,slot.plan.outcomes[0]};
              AdmissionRequest need;need.need(Resource::revision).need(Resource::context_publication);
              auto credit=slot.credits.transfer(need);
              if(credit)effects.record(effects.context,event,slot.plan.revisions,std::move(*credit));
            }
          }
          finish(slot,now);continue;
        }
        for(std::size_t i=0;i<4;++i){slot.plan.execution.fields[i].eligible=false;mark_not_executed(slot.plan.execution.fields[i].diagnostics);}
      }
      while (slot.record.step < slot.plan.execution.count) {
        const auto field = slot.plan.execution.order[slot.record.step];
        auto &plan = slot.plan.execution.fields[field];
        bool dependencies = true;
        for (std::size_t d = 0; d < slot.plan.execution.count; ++d)
          if ((plan.dependency_mask & (1u << d)) &&
              slot.plan.outcomes[d].status != FieldStatus::executed)
            dependencies = false;
        if (slot.plan.cancelled[field]) {
          slot.plan.outcomes[field].id = plan.id;
          slot.plan.outcomes[field].status = FieldStatus::cancelled;
          slot.plan.outcomes[field].diagnostics.errors = not_executed;
          ++slot.record.step;
          continue;
        }
        if (!dependencies) {
          plan.eligible = false;
          plan.diagnostics.errors |= dependency_blocked | not_executed;
        }
        if (!plan.eligible) {
          FieldOutcome skipped;
          skipped.id = plan.id;
          skipped.status = FieldStatus::not_executed;
          skipped.diagnostics = plan.diagnostics;
          slot.plan.outcomes[field] = skipped;
          ++slot.record.step;
          continue;
        }
        if (action == 1) {
          auto outcome = backend_.simulate(backend_.context, plan,
                                           slot.record.hypothetical,
                                           slot.plan.execution.boundary);
          apply_outcome(slot, field, outcome, now);
          continue;
        }
        if (state_.version > UINT64_MAX - 4)
          return std::unexpected(Error{ErrorCode::overflow});
        const auto result_index = index * 4 + field;
        AsyncResult capability{
            slot.plan.tokens[field].publisher(),          results_,
            &results_->slots[result_index].outcome,       result_index,
            &results_->slots[result_index].guard,         &results_->trace,
            &results_->slots[result_index].device_done_ns};
        slot.record.running = true;
        slot.record.running_field = field;
        auto actual_boundary = slot.plan.execution.boundary;
        if (slot.plan.time_known)
          actual_boundary.time = now.clock.time;
        options_.trace.emit(TraceStage::dispatch, slot.plan.trace_key, plan.id);
        auto begun =
            backend_.begin(backend_.context, plan, actual_boundary, capability);
        if (!begun) {
          FieldOutcome failed;
          failed.id = plan.id;
          failed.status = FieldStatus::failed;
          failed.diagnostics.errors = device_failure | not_executed;
          capability.complete(failed);
        }
        return {};
      }
      finish(slot, now);
    }
    return {};
  }
  Result<CancellationResult> cancel(Handle handle,
                                    const codec::PacketView &packet,
                                    const OperationContext &now,
                                    AdmissionBundle reserved = {}) noexcept {
    if (progressing_)
      return std::unexpected(Error{ErrorCode::would_deadlock});
    if (handle.slot >= Transactions)
      return std::unexpected(Error{ErrorCode::invalid_argument});
    auto &slot = slots_[handle.slot];
    if (!slot.record.active || slot.record.generation != handle.generation)
      return std::unexpected(Error{ErrorCode::stale_generation});
    const auto &envelope = packet.envelope.envelope;
    auto valid = codec::validate_cam(envelope);
    if (!valid || !envelope.cancel || envelope.ack || !envelope.command ||
        !slot.plan.request.envelope.command || packet.opaque)
      return std::unexpected(Error{ErrorCode::invalid_argument});
    const auto &original = *slot.plan.request.envelope.command;
    const auto &command = *envelope.command;
    if (command.message_id != original.message_id ||
        envelope.stream_id != slot.plan.request.envelope.stream_id ||
        !same_identifier(command.controller, original.controller) ||
        !same_identifier(command.controllee, original.controllee) ||
        now.association_generation !=
            slot.plan.execution.association_generation)
      return std::unexpected(Error{ErrorCode::invalid_argument});
    std::array<bool, state_field_capacity> selected{};
    for (std::size_t view_index = 0; view_index < packet.fields.size();
         ++view_index) {
      const auto &view = packet.fields[view_index];
      if (view.kind != BodyKind::selectors ||
          view.attribute != Attribute::current)
        return std::unexpected(Error{ErrorCode::unsupported_capability});
      auto index = field_index(view.id);
      if (index == state_field_capacity)
        return std::unexpected(Error{ErrorCode::unsupported_capability});
      if (!selected[index]) {
        selected[index] = true;
      }
    }
    if (slot.plan.request.cam.action != 2 ||
        !same_class(envelope.class_id, slot.plan.request.envelope.class_id))
      return std::unexpected(Error{ErrorCode::invalid_argument});
    for (std::size_t index = 0; index < state_field_capacity; ++index)
      if (selected[index]) {
        bool present = false;
        for (std::size_t i = 0; i < slot.plan.execution.count; ++i)
          present |= slot.plan.execution.fields[i].id == state_fields[index];
        if (!present)
          return std::unexpected(Error{ErrorCode::invalid_argument});
      }
    const auto raw = command.cam;
    const bool req_x = raw & (1u << 19), req_s = raw & (1u << 18);
    AdmissionRequest required;
    required.need(Resource::cancellation_queue)
        .need(Resource::cancellation_response,
              std::size_t(req_x) + std::size_t(req_s));
    if (reserved.held(Resource::cancellation_queue) < 1 ||
        reserved.held(Resource::cancellation_response) <
            std::size_t(req_x) + std::size_t(req_s)) {
      if (reserved.held(Resource::cancellation_queue))
        return std::unexpected(Error{ErrorCode::invalid_argument});
      auto credits = admission_.acquire(required);
      if (!credits)
        return std::unexpected(credits.error());
      reserved = std::move(*credits);
    }
    CancellationResult result;
    result.credits = std::move(reserved);
    const auto mode = (raw >> 12) & 7;
    if (mode) {
      const auto epoch = now.clock.epoch == timing::Epoch::gps ? codec::Tsi::gps
                         : now.clock.epoch == timing::Epoch::utc
                             ? codec::Tsi::utc
                             : codec::Tsi::other;
      auto first = timing::subtract(
          now.clock.time, timing::from_picoseconds(now.clock.uncertainty_ps));
      auto last = timing::add(
          now.clock.time, timing::from_picoseconds(now.clock.uncertainty_ps));
      const timing::ProtocolTime requested{envelope.timestamp.integer,
                                           envelope.timestamp.fractional};
      auto fit = first && last ? timing::effect_interval_allowed(
                                     mode, requested, *first, *last, now.timing)
                               : Result<bool>{false};
      if (envelope.timestamp.tsi != epoch ||
          envelope.timestamp.tsf != codec::Tsf::picoseconds || !fit || !*fit ||
          (now.clock.state != timing::ClockState::locked &&
           now.clock.state != timing::ClockState::holdover)) {
        std::array<Diagnostics, state_field_capacity> errors{};
        for (std::size_t i = 0; i < state_field_capacity; ++i)
          if (selected[i])
            errors[i].errors = not_executed;
        return cancellation_response(packet, state_, now.clock, selected, {},
                                     errors, std::move(result.credits), 7);
      }
    }
    // Resolve ready execution first; a completed write cannot become
    // cancellation success.
    tickets_.scan(
        [&](CompletionRecord record) noexcept { consume(record, now); });
    std::array<Diagnostics, state_field_capacity> diagnostics{};
    for (std::size_t index = 0; index < state_field_capacity; ++index)
      if (selected[index]) {
        std::size_t field = slot.plan.execution.count;
        for (std::size_t i = 0; i < slot.plan.execution.count; ++i)
          if (slot.plan.execution.fields[i].id == state_fields[index])
            field = i;
        bool cancelled = false;
        if (field < slot.plan.execution.count && !slot.record.complete &&
            slot.plan.outcomes[field].status == FieldStatus::pending) {
          if (slot.record.running && slot.record.running_field == field) {
            const auto result_index = handle.slot * 4 + field;
            AsyncResult capability{
                slot.plan.tokens[field].publisher(),          results_,
                &results_->slots[result_index].outcome,       result_index,
                &results_->slots[result_index].guard,         &results_->trace,
                &results_->slots[result_index].device_done_ns};
            const auto disarmed =
                backend_.disarm ? backend_.disarm(backend_.context, capability)
                                : DisarmResult::not_cancelled;
            if (disarmed == DisarmResult::cancelled) {
              results_->slots[result_index].guard.cancelled.store(
                  true, std::memory_order_release);
              FieldOutcome outcome;
              outcome.id = state_fields[index];
              outcome.status = FieldStatus::cancelled;
              outcome.diagnostics.errors = not_executed;
              cancelled = capability.complete(outcome);
              if (!cancelled) {
                results_->slots[result_index].guard.contradiction.store(
                    true, std::memory_order_release);
                faulted_ = true;
                diagnostics[index].errors |= device_failure;
              }
            } else if (disarmed == DisarmResult::unknown_effect) {
              FieldOutcome outcome;
              outcome.id = state_fields[index];
              outcome.status = FieldStatus::unknown_effect;
              outcome.validity = Validity::unknown;
              outcome.diagnostics.errors = device_failure | state_indeterminate;
              capability.complete(outcome);
              diagnostics[index].errors |= device_failure;
            }
          } else {
            slot.plan.cancelled[field] = true;
            cancelled = true;
          }
        }
        if (cancelled) {
          result.cancelled[index] = true;
        } else
          diagnostics[index].errors |= not_executed;
      }
    tickets_.scan(
        [&](CompletionRecord record) noexcept { consume(record, now); });
    return cancellation_response(packet, state_, now.clock, selected,
                                 result.cancelled, diagnostics,
                                 std::move(result.credits), mode);
  }
  Result<std::optional<AckRecord>> take_response(Handle handle) noexcept {
    if (handle.slot >= Transactions)
      return std::unexpected(Error{ErrorCode::invalid_argument});
    auto &slot = slots_[handle.slot];
    if (!slot.record.active || slot.record.generation != handle.generation)
      return std::unexpected(Error{ErrorCode::stale_generation});
    if (slot.record.response_read == slot.record.responses.size())
      return std::optional<AckRecord>{};
    return std::optional<AckRecord>{
        slot.record.responses[slot.record.response_read++]};
  }
  Result<bool> complete(Handle handle) const noexcept {
    if (handle.slot >= Transactions || !slots_[handle.slot].record.active ||
        slots_[handle.slot].record.generation != handle.generation)
      return std::unexpected(Error{ErrorCode::stale_generation});
    return slots_[handle.slot].record.complete;
  }
  Result<std::array<FieldOutcome, 4>> outcomes(Handle handle) const noexcept {
    if (handle.slot >= Transactions || !slots_[handle.slot].record.active ||
        slots_[handle.slot].record.generation != handle.generation)
      return std::unexpected(Error{ErrorCode::stale_generation});
    std::array<FieldOutcome,4> result;std::copy_n(slots_[handle.slot].plan.outcomes.begin(),4,result.begin());return result;
  }
  Result<void> release(Handle handle) noexcept {
    if (handle.slot >= Transactions)
      return std::unexpected(Error{ErrorCode::invalid_argument});
    auto &slot = slots_[handle.slot];
    if (!slot.record.active || slot.record.generation != handle.generation ||
        !slot.record.complete ||
        slot.record.response_read != slot.record.responses.size())
      return std::unexpected(Error{ErrorCode::invalid_state});
    slot.record.active = false;
    for (auto &token : slot.plan.tokens) {
      const auto own = token.is_reserved();
      const auto ticket_slot = token.publisher().slot();
      token = CompletionToken{};
      if (own)
        tickets_.consume(ticket_slot);
    }
    slot.credits.reset();
    for (std::size_t i = 0; i < 4; ++i)
      results_->slots[handle.slot * 4 + i].busy = false;
    ++slot.record.generation;
    return {};
  }
};
static_assert(Engine<>::plan_bytes() <= 8192);
static_assert(Engine<>::record_bytes() <= 2048);
} // namespace vita::runtime::transaction
