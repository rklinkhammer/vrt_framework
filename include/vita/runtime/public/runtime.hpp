#pragma once
#include <vita/adapters/loopback/loopback.hpp>
#include <vita/codec/prologue.hpp>
#include <vita/runtime/public/config.hpp>
#include <vita/runtime/transaction/controller.hpp>
#include <vita/runtime/transaction/manager.hpp>
namespace vita {
template <std::size_t Streams = 16, std::size_t Transactions = 16,
          std::size_t CacheEntries = 4096,
          std::size_t CacheBytes = 8 * 1024 * 1024>
class VitaRuntime {
  static constexpr std::size_t transport_slots = 320, registry_capacity = 128,
                               controller_capacity = 256;
  using Engine = runtime::transaction::Engine<Transactions>;
  using Manager =
      runtime::transaction::TransactionManager<Transactions, CacheEntries,
                                               CacheBytes>;
  using RevisionHandle = runtime::context::RevisionHandle;
  using MonoTime = runtime::timing::MonoTime;
  using ProtocolTime = runtime::timing::ProtocolTime;
  struct Stream;

public:
  class Controllee {
    friend class VitaRuntime;
    VitaRuntime *runtime_;
    std::size_t index_;
    Controllee(VitaRuntime *r, std::size_t i) : runtime_(r), index_(i) {}

  public:
    Result<void> start() { return runtime_->start(index_); }
    Result<void> pause() { return runtime_->stop(index_); }
    Result<void> stop() {
      return pause();
    } // Source-pause compatibility; shutdown drains lifecycle.
    Result<void> recover(RecoveryConfig config,
                         LifecycleCompletion completion = {}) {
      return runtime_->recover_stream(*this, config, completion);
    }
    Result<void> shutdown(StopMode mode = StopMode::graceful,
                          LifecycleCompletion completion = {}) {
      return runtime_->stop_stream(*this, mode, completion);
    }
    LifecycleStatus lifecycle() const noexcept {
      return runtime_->lifecycle_status(index_);
    }
    std::uint32_t sid() const noexcept {
      return runtime_->streams_[index_]->config.sid;
    }
    runtime::StateSnapshot confirmed_state() const noexcept {
      return runtime_->streams_[index_]->engine.state();
    }
    SourceStatus status() const noexcept {
      return runtime_->streams_[index_]->status;
    }
    StreamMetrics metrics() const noexcept {
      return runtime_->streams_[index_]->metrics;
    }
    runtime::timing::SampleTimeline timeline() const noexcept {
      return runtime_->streams_[index_]->timeline;
    }
    std::size_t index() const noexcept { return index_; }
    Result<void> set_source(profiles::iq::SourceProvider source) {
      if (runtime_->progressing_)
        return std::unexpected(Error{ErrorCode::would_deadlock});
      auto &stream = *runtime_->streams_[index_];
      if (!source.callback || (stream.status != SourceStatus::configured &&
                               stream.status != SourceStatus::stopped))
        return std::unexpected(Error{ErrorCode::invalid_state});
      stream.config.source = source;
      return {};
    }
  };
  class Controller {
    friend class VitaRuntime;
    VitaRuntime *runtime_;
    std::size_t index_;
    Controller(VitaRuntime *r, std::size_t i) : runtime_(r), index_(i) {}

  public:
    Result<TransactionHandle> set_sample_rate(Hertz rate,
                                              CommandOptions options = {}) {
      return runtime_->command(index_, rate, 1u << 1, options, false);
    }
    Result<TransactionHandle> query(std::uint8_t fields = 1u << 1,
                                    CommandOptions options = {}) {
      return runtime_->command(index_, {}, fields, options, true);
    }
    Result<void> cancel(TransactionHandle handle, std::uint8_t fields = 1u << 1,
                        CommandOptions options = {}) {
      if (handle.runtime_id != runtime_->runtime_id_ || handle.stream != index_)
        return std::unexpected(Error{ErrorCode::identity_conflict});
      return runtime_->cancel(handle, fields, options);
    }
    Result<runtime::transaction::Observation>
    observation(TransactionHandle handle) const noexcept {
      if (handle.runtime_id != runtime_->runtime_id_ || handle.stream != index_)
        return std::unexpected(Error{ErrorCode::identity_conflict});
      return runtime_->observation(handle);
    }
    Result<MonoTime> deadline(TransactionHandle handle) const noexcept {
      if (handle.runtime_id != runtime_->runtime_id_ || handle.stream != index_)
        return std::unexpected(Error{ErrorCode::identity_conflict});
      return runtime_->controllers_.deadline_for(handle.controller);
    }
    Result<std::optional<runtime::transaction::StateObservation>>
    state(TransactionHandle handle) const noexcept {
      if (handle.runtime_id != runtime_->runtime_id_ || handle.stream != index_)
        return std::unexpected(Error{ErrorCode::identity_conflict});
      return runtime_->controllers_.state_observation(handle.controller);
    }
    Result<void> observe(
        TransactionHandle handle, void *context,
        void (*callback)(void *,
                         const runtime::transaction::Observation &) noexcept) {
      if (handle.runtime_id != runtime_->runtime_id_ || handle.stream != index_)
        return std::unexpected(Error{ErrorCode::identity_conflict});
      return runtime_->observe(handle, context, callback);
    }
    Result<void> release(TransactionHandle handle) {
      if (handle.runtime_id != runtime_->runtime_id_ || handle.stream != index_)
        return std::unexpected(Error{ErrorCode::identity_conflict});
      return runtime_->release(handle);
    }
    Result<WaitResult> wait(TransactionHandle handle, std::uint64_t timeout_ns,
                            WaitEvidence evidence = WaitEvidence::execution) {
      if (handle.runtime_id != runtime_->runtime_id_ || handle.stream != index_)
        return std::unexpected(Error{ErrorCode::identity_conflict});
      return runtime_->wait(handle, timeout_ns, evidence);
    }
  };

private:
  struct TxRecord {
    bool active = false;
    std::uint64_t operation = 0;
    std::optional<runtime::transaction::ControllerHandle> controller;
    RevisionHandle revision;
    runtime::AdmissionBundle extra;
  };
  struct Book {
    std::uint64_t generation = 0;
    std::size_t stream = 0;
    codec::Envelope request;
    std::uint32_t cancel_cam = 0;
    void *context = nullptr;
    void (*callback)(
        void *, const runtime::transaction::Observation &) noexcept = nullptr;
    std::optional<runtime::transaction::Observation> last;
    std::array<runtime::transaction::Observation, 16> delivered{};
    std::size_t delivered_count = 0;
  };
  struct Reply {
    runtime::transaction::RetentionToken token;
    std::size_t read = 0;
  };
  const std::uint64_t runtime_id_ = runtime_identity_source.fetch_add(1);
  RuntimeConfig config_;
  ExternalPools pools_;
  runtime::AdmissionPool admission_{
      runtime::AdmissionPool::reference_capacities()};
  runtime::transaction::RetentionStore<CacheEntries, CacheBytes> retention_{
      admission_};
  runtime::transaction::ControllerRegistry<controller_capacity> controllers_;
  memory::RetentionQuota receive_quota_{1024};
  runtime::RouteRegistry<registry_capacity> routes_;
  runtime::CounterRegistry<registry_capacity> counters_;
  adapters::loopback::Loopback<transport_slots, registry_capacity,
                               registry_capacity>
      transport_;
  runtime::CompletionArena<transport_slots> tx_tickets_;
  std::array<TxRecord, transport_slots> transmissions_{};
  std::array<Book, controller_capacity> books_{};
  std::array<std::unique_ptr<Stream>, Streams> streams_{}, retired_{};
  struct LifecycleOperation {
    LifecycleStatus status;
    RecoveryConfig recovery;
    LifecycleCompletion completion;
    MonoTime deadline;
    bool active = false, recover = false, immediate = false,
         reinitialized = false;
    std::uint32_t original_sid = 0;
  };
  std::array<LifecycleOperation, Streams> lifecycle_{};
  struct IoRecord {
    Stream *bank = nullptr;
    adapters::loopback::TxToken token;
  };
  std::array<IoRecord, transport_slots> io_{};
  std::array<std::uint32_t, registry_capacity / 4> sid_history_{};
  std::size_t sid_count_ = 0;
  std::size_t count_ = 0;
  runtime::timing::ProtocolClock clock_;
  std::optional<runtime::timing::ClockSnapshot> clock_snapshot_;
  MonoTime now_{};
  std::uint64_t next_operation_ = 1;
  bool frozen_ = false, progressing_ = false, shutdown_requested_ = false;
  LifecycleCompletion shutdown_completion_;
  bool shutdown_notified_ = false;
  runtime::BudgetLedger budget_;
  adapters::loopback::Fault next_fault_{}, response_fault_{};

  struct Stream {
    VitaRuntime *owner;
    std::size_t index;
    StreamConfig config;
    std::uint64_t generation = 1;
    bool installed = false, retired = false, auto_complete = true;
    SourceStatus status = SourceStatus::configured;
    StreamMetrics metrics;
    runtime::timing::SampleTimeline timeline, pacing;
    runtime::context::RevisionStore<128> revisions;
    runtime::transaction::VirtualBackend<Transactions> backend;
    Engine engine;
    Manager manager;
    runtime::context::ContextPublisher<128, 64> publisher;
    runtime::context::ContextReceiver<128, 64> receiver;
    runtime::transaction::RelationshipHandle relationship;
    std::array<std::optional<Reply>, Transactions * 2 + 64> replies{};
    struct PacketStamp {
      const std::byte *header = nullptr;
      ProtocolTime last_sample;
    };
    std::array<PacketStamp, 64> stamps{};
    std::optional<ProtocolTime> data_highwater, coverage_floor;
    bool pending_loss = false;
    std::array<runtime::timing::Boundary, Transactions * 2 + 3> boundaries{};
    std::size_t boundary_count = 0;
    Stream(VitaRuntime *r, std::size_t i, StreamConfig c)
        : owner(r), index(i), config(c),
          timeline(*runtime::timing::SampleTimeline::create({}, c.sample_rate)),
          pacing(timeline),
          engine(r->admission_, backend.binding(), initial(c),
                 runtime::transaction::EngineOptions{
                     runtime::transaction::Profile::iq_generator_v1,
                     revisions.binding(), &timeline, true, this,
                     project_observation}),
          manager(engine, r->retention_, r->admission_),
          publisher(revisions, {this, send_context, send_data}),
          receiver(r->receive_quota_, {this, deliver, drop}, 1, r->epoch(),
                   false, c.sid) {}
    static runtime::StateSnapshot initial(const StreamConfig &c) {
      runtime::StateSnapshot state;
      for (auto &field : state.fields)
        field.validity = runtime::Validity::known;
      state.fields[0].value = c.sid;
      state.fields[1].value = *Hertz::from_integer(c.sample_rate);
      state.fields[2].value = std::uint32_t{0};
      state.fields[3].value = profiles::iq::payload_format(c.format);
      return state;
    }
    static runtime::StateSnapshot project_observation(
        void *p, const runtime::StateSnapshot &observed,
        const runtime::transaction::OperationContext &now) noexcept {
      auto &s = *static_cast<Stream *>(p);
      auto result = observed;
      if (result.fields[2].validity == runtime::Validity::known) {
        auto *flags = std::get_if<std::uint32_t>(&result.fields[2].value);
        if (flags) {
          *flags |= (1u << 31) | (1u << 30);
          *flags &= ~((1u << 19) | (1u << 18));
          if (now.clock.calibrated &&
              now.clock.state == runtime::timing::ClockState::locked)
            *flags |= 1u << 19;
          if (s.status == SourceStatus::running &&
              runtime::context::required_known(observed))
            *flags |= 1u << 18;
        }
      }
      return result;
    }
    static void
    deliver(void *p,
            const runtime::context::BorrowedSignalRx &signal) noexcept {
      auto &s = *static_cast<Stream *>(p);
      if (!s.retired && !s.owner->shutdown_requested_ &&
          s.config.receiver.deliver)
        s.config.receiver.deliver(s.config.receiver.context, signal);
    }
    static void drop(void *p,
                     runtime::context::Confidence confidence) noexcept {
      auto &s = *static_cast<Stream *>(p);
      ++s.metrics.receive_drops;
      if (!s.retired && !s.owner->shutdown_requested_ && s.config.receiver.drop)
        s.config.receiver.drop(s.config.receiver.context, confidence);
    }
    static Result<void>
    send_context(void *p,
                 const runtime::context::ContextFrame &frame) noexcept {
      auto &s = *static_cast<Stream *>(p);
      auto &r = *s.owner;
      auto buffer = r.pools_.control.acquire({2048});
      if (!buffer)
        return std::unexpected(buffer.error());
      auto bytes = buffer->writable_bytes();
      if (!bytes)
        return std::unexpected(bytes.error());
      auto envelope = r.envelope(s, codec::PacketType::context);
      auto encoded = runtime::context::encode_context(frame, envelope, *bytes);
      if (!encoded)
        return std::unexpected(encoded.error());
      buffer->set_size(*encoded);
      memory::TxStorage storage;
      auto appended = storage.append(std::move(*buffer), 0, *encoded);
      if (!appended)
        return appended;
      auto sent = r.send(storage, s, codec::PacketType::context, false, {}, {});
      if (sent && frame.observation)
        s.coverage_floor = frame.time;
      return sent;
    }
    static Result<void> send_data(void *p, memory::TxStorage &storage,
                                  const RevisionHandle &revision) noexcept {
      auto &s = *static_cast<Stream *>(p);
      auto header = storage.segment(0);
      if (!header)
        return std::unexpected(header.error());
      PacketStamp *stamp = nullptr;
      for (auto &item : s.stamps)
        if (item.header == header->data()) {
          stamp = &item;
          break;
        }
      if (!stamp)
        return std::unexpected(Error{ErrorCode::invalid_state});
      auto sent = s.owner->send(storage, s, codec::PacketType::signal, false,
                                {}, revision);
      if (sent) {
        if (!s.data_highwater || stamp->last_sample > *s.data_highwater)
          s.data_highwater = stamp->last_sample;
        stamp->header = nullptr;
        ++s.metrics.packets;
      }
      return sent;
    }
    static std::optional<codec::RequestContext>
    request_context(void *p, const codec::Envelope &envelope) noexcept {
      auto &s = *static_cast<Stream *>(p);
      if (!envelope.command)
        return {};
      for (const auto &book : s.owner->books_)
        if (book.generation && book.stream == s.index &&
            book.request.stream_id == s.config.sid && book.request.command &&
            book.request.command->message_id == envelope.command->message_id)
          return codec::RequestContext{
              envelope.cancel ? book.cancel_cam : book.request.command->cam};
      return {};
    }
    static void receive_command(void *p, const codec::PacketView &packet,
                                const memory::RxEnvelope &) noexcept {
      auto &s = *static_cast<Stream *>(p);
      auto &r = *s.owner;
      auto now = r.operation_context(s, packet.envelope.envelope.timestamp);
      auto admitted = s.manager.accept(
          packet, now, {s.config.controller_peer, s.generation});
      if (!admitted) {
        ++s.metrics.receive_drops;
        r.reject(s, packet, admitted.error());
        return;
      }
      for (auto &reply : s.replies)
        if (!reply) {
          reply = Reply{admitted->token};
          return;
        }
      s.manager.release(admitted->token);
      ++s.metrics.receive_drops;
    }
    static void receive_ack(void *p, const codec::PacketView &packet,
                            const memory::RxEnvelope &) noexcept {
      auto &s = *static_cast<Stream *>(p);
      auto received = s.owner->controllers_.receive(
          s.generation, {s.config.controllee_peer, s.generation},
          packet.envelope.wire, s.owner->now_);
      if (!received)
        ++s.metrics.receive_drops;
    }
    static void receive_context(void *p, const codec::PacketView &packet,
                                const memory::RxEnvelope &) noexcept {
      auto &s = *static_cast<Stream *>(p);
      if (s.retired || s.owner->shutdown_requested_ ||
          s.owner->lifecycle_[s.index].status.phase ==
              LifecyclePhase::stopped ||
          s.owner->lifecycle_[s.index].status.phase ==
              LifecyclePhase::quarantined)
        return;
      if (!s.receiver.receive_context(packet, s.generation, s.owner->now_))
        ++s.metrics.receive_drops;
    }
    static void receive_data(void *p, const codec::PacketView &packet,
                             const memory::RxEnvelope &rx) noexcept {
      auto &s = *static_cast<Stream *>(p);
      if (s.retired || s.owner->shutdown_requested_ ||
          s.owner->lifecycle_[s.index].status.phase ==
              LifecyclePhase::stopped ||
          s.owner->lifecycle_[s.index].status.phase ==
              LifecyclePhase::quarantined)
        return;
      const auto &timestamp = packet.envelope.envelope.timestamp;
      if (timestamp.tsi != s.owner->epoch() ||
          timestamp.tsf != codec::Tsf::picoseconds ||
          packet.envelope.payload.size() %
              profiles::iq::bytes_per_pair(s.config.format)) {
        ++s.metrics.receive_drops;
        return;
      }
      if (!s.receiver.receive_data(rx,
                                   {timestamp.integer, timestamp.fractional},
                                   s.generation, s.owner->now_))
        ++s.metrics.receive_drops;
    }
  };
  static adapters::loopback::Capabilities transport_capabilities() noexcept {
    adapters::loopback::Capabilities c;
    c.reserved_control_slots = 64;
    c.reserved_cancellation_slots = 64;
    return c;
  }
  VitaRuntime(RuntimeConfig config, ExternalPools pools)
      : config_(config), pools_(std::move(pools)),
        transport_(pools_.rx_data, pools_.rx_control, pools_.rx_cancellation,
                   admission_, routes_, counters_, transport_capabilities()) {}
  codec::Tsi epoch() const noexcept {
    return static_cast<codec::Tsi>(static_cast<unsigned>(config_.clock.epoch) +
                                   1);
  }
  runtime::CounterKey counter(const Stream &s, codec::PacketType type,
                              bool controller = false) const noexcept {
    return {controller ? s.config.controller_peer : s.config.controllee_peer,
            s.config.sid, type};
  }
  codec::Envelope envelope(Stream &s, codec::PacketType type,
                           bool controller = false) noexcept {
    codec::Envelope e;
    e.type = type;
    e.stream_id = s.config.sid;
    e.class_id = codec::ClassId{
        *config_.oui, 1,
        static_cast<std::uint16_t>(
            type == codec::PacketType::signal
                ? (s.config.trailer
                       ? *s.config.trailer_packet_class
                       : profiles::iq::baseline_packet_class(s.config.format))
            : type == codec::PacketType::context ? 0x10
                                                 : 0x20)};
    if (auto next = counters_.next(counter(s, type, controller)))
      e.packet_count = *next;
    if (type == codec::PacketType::command)
      e.command = codec::Command{
          0, 0, codec::Identifier::short_id(s.config.controllee_id),
          codec::Identifier::short_id(s.config.controller_id)};
    return e;
  }
  Result<void> charge(runtime::BudgetCategory category,
                      std::size_t bytes) noexcept {
    auto row = budget_.row(category);
    if (bytes > row.reserved - row.charged) {
      auto transferred = budget_.transfer_headroom(
          category, bytes - (row.reserved - row.charged));
      if (!transferred)
        return transferred;
    }
    auto charged = budget_.charge(category, bytes);
    if (!charged)
      return charged;
    if (budget_.charged_bytes() > config_.memory_limit)
      return std::unexpected(Error{ErrorCode::capacity_exhausted});
    return {};
  }
  void freeze() noexcept {
    if (!frozen_) {
      routes_.freeze();
      counters_.freeze();
      frozen_ = true;
    }
  }
  Result<void>
  send(memory::TxStorage &storage, Stream &stream, codec::PacketType type,
       bool controller,
       std::optional<runtime::transaction::ControllerHandle> observer,
       RevisionHandle revision, runtime::AdmissionBundle extra = {}) noexcept {
    runtime::AdmissionRequest required;
    required.need(runtime::Resource::completion);
    auto credit = admission_.acquire(required);
    if (!credit)
      return std::unexpected(credit.error());
    if (next_operation_ == UINT64_MAX)
      return std::unexpected(Error{ErrorCode::overflow});
    const auto operation = next_operation_++;
    auto ticket = tx_tickets_.reserve(operation);
    if (!ticket)
      return std::unexpected(ticket.error());
    const auto slot = ticket->publisher().slot();
    auto fault = type == codec::PacketType::command && !controller
                     ? response_fault_
                     : std::exchange(next_fault_, {});
    adapters::loopback::TxSubmission submission{
        std::move(storage),
        std::move(*ticket),
        {controller ? stream.config.controller_peer
                    : stream.config.controllee_peer,
         stream.generation},
        counter(stream, type, controller),
        fault,
        std::move(*credit)};
    auto accepted = transport_.try_send(std::move(submission));
    if (!accepted) {
      storage = std::move(accepted.error().submission.storage);
      if (observer)
        controllers_.local_send(*observer, false);
      return std::unexpected(accepted.error().error);
    }
    io_[accepted->slot] = {&stream, *accepted};
    transmissions_[slot] = {true, operation, observer, std::move(revision),
                            std::move(extra)};
    return {};
  }
  void reject(Stream &stream, const codec::PacketView &packet,
              Error error) noexcept {
    if (!pools_.emergency.block_count() || packet.envelope.envelope.cancel)
      return;
    auto cam = runtime::transaction::Cam::parse(
        packet.envelope.envelope,
        runtime::transaction::Profile::iq_generator_v1);
    if (!cam || (!cam->request_v && !cam->request_x))
      return;
    runtime::AdmissionRequest need;
    need.need(runtime::Resource::emergency_response);
    auto credit = admission_.acquire(need);
    if (!credit)
      return;
    auto buffer = pools_.emergency.acquire({2048});
    if (!buffer)
      return;
    auto bytes = buffer->writable_bytes();
    if (!bytes)
      return;
    runtime::transaction::AckRecord ack;
    ack.request = packet.envelope.envelope;
    ack.cam = *cam;
    ack.kind = cam->request_v ? runtime::transaction::AckKind::validation
                              : runtime::transaction::AckKind::execution;
    ack.partial = true;
    ack.timing = cam->timing ? 7 : 0;
    const auto diagnostic = error.code == ErrorCode::identity_conflict
                                ? (1u << 4)
                                : runtime::transaction::resource_exhausted;
    for (std::size_t i = 0; i < packet.fields.size(); ++i) {
      auto index = runtime::field_index(packet.fields[i].id);
      if (index < 4)
        ack.diagnostics[index].errors =
            diagnostic | runtime::transaction::not_executed;
    }
    auto count = counters_.next(counter(stream, codec::PacketType::command));
    if (!count)
      return;
    auto encoded = runtime::transaction::encode_response(ack, *bytes, *count);
    if (!encoded)
      return;
    buffer->set_size(*encoded);
    memory::TxStorage storage;
    if (!storage.append(std::move(*buffer), 0, *encoded))
      return;
    (void)send(storage, stream, codec::PacketType::command, false, {}, {},
               std::move(*credit));
  }
  // Bounded search over complete packet intervals avoids walking every overdue
  // packet.
  Result<std::uint64_t>
  intervals_on(const runtime::timing::SampleTimeline &timeline,
               ProtocolTime target, std::size_t pairs) const noexcept {
    if (target < timeline.time())
      return std::uint64_t{0};
    std::uint64_t lo = 0, hi = (UINT64_MAX - timeline.ordinal()) / pairs;
    for (unsigned bit = 0; bit < 64 && lo < hi; ++bit) {
      const auto mid = lo + (hi - lo) / 2 + (hi - lo) % 2;
      auto copy = timeline;
      auto advanced = copy.advance(mid * pairs);
      if (advanced && copy.time() <= target)
        lo = mid;
      else
        hi = mid - 1;
    }
    return lo;
  }
  static ProtocolTime mono_protocol(MonoTime now) noexcept {
    return {now.ns / 1'000'000'000, (now.ns % 1'000'000'000) * 1000};
  }
  Result<std::uint64_t> intervals_before(const Stream &s, ProtocolTime target,
                                         std::size_t pairs) const noexcept {
    return intervals_on(s.timeline, target, pairs);
  }
  void append_boundary(Stream &s, std::uint64_t intervals,
                       std::size_t pairs) noexcept {
    if (s.boundary_count == s.boundaries.size())
      return;
    auto timeline = s.timeline;
    if (intervals > UINT64_MAX / pairs || !timeline.advance(intervals * pairs))
      return;
    auto boundary = runtime::timing::make_boundary(
        timeline, clock_snapshot_ ? clock_snapshot_->mapping_generation : 0);
    for (std::size_t i = 0; i < s.boundary_count; ++i)
      if (s.boundaries[i].sample_ordinal == boundary.sample_ordinal)
        return;
    s.boundaries[s.boundary_count++] = boundary;
  }
  runtime::transaction::OperationContext
  operation_context(Stream &s,
                    std::optional<codec::Timestamp> extra = {}) noexcept {
    runtime::transaction::OperationContext now;
    now.monotonic = now_;
    now.operation = next_operation_;
    now.association_generation = s.generation;
    now.timing = config_.timing;
    now.data_running = s.status == SourceStatus::running;
    if (clock_snapshot_)
      now.clock = *clock_snapshot_;
    s.boundary_count = 0;
    auto pairs = packet_samples(s.config);
    if (pairs) {
      append_boundary(s, 0, *pairs);
      auto near = intervals_before(s, now.clock.time, *pairs);
      if (near) {
        append_boundary(s, *near, *pairs);
        append_boundary(s, *near + 1, *pairs);
      }
      auto pending = s.engine.pending_times();
      for (std::size_t i = 0; i < pending.size(); ++i)
        if (pending[i].mode) {
          auto at = intervals_before(s, pending[i].requested, *pairs);
          if (at) {
            append_boundary(s, *at, *pairs);
            append_boundary(s, *at + 1, *pairs);
          }
        }
      if (extra && extra->tsi != codec::Tsi::none) {
        auto at =
            intervals_before(s, {extra->integer, extra->fractional}, *pairs);
        if (at) {
          append_boundary(s, *at, *pairs);
          append_boundary(s, *at + 1, *pairs);
        }
      }
    }
    now.boundaries = {s.boundaries.data(), s.boundary_count};
    return now;
  }
  Result<void> generate(Stream &s) noexcept {
    if (!clock_snapshot_ || s.status != SourceStatus::running)
      return {};
    auto pairs = packet_samples(s.config);
    if (!pairs)
      return std::unexpected(pairs.error());
    if (s.pacing.time() > mono_protocol(now_))
      return {};
    auto obsolete = intervals_on(s.pacing, mono_protocol(now_), *pairs);
    if (!obsolete)
      return std::unexpected(obsolete.error());
    if (s.coverage_floor) {
      auto future = s.timeline;
      future.advance(*obsolete * *pairs);
      if (future.time() < *s.coverage_floor)
        ++*obsolete;
    }
    if (*obsolete) {
      auto skipped = *obsolete * *pairs;
      auto moved = s.timeline.advance(skipped);
      if (!moved)
        return moved;
      moved = s.pacing.advance(skipped);
      if (!moved)
        return moved;
      s.metrics.skipped_packets += *obsolete;
      s.metrics.skipped_samples += skipped;
      s.pending_loss = true;
    }
    if (s.pacing.time() > mono_protocol(now_))
      return {};
    if (s.pending_loss) {
      runtime::AdmissionRequest need;
      need.need(runtime::Resource::revision)
          .need(runtime::Resource::context_publication);
      auto credits = admission_.acquire(need);
      if (!credits)
        return std::unexpected(credits.error());
      auto reservation = s.revisions.reserve(1);
      if (!reservation)
        return std::unexpected(reservation.error());
      auto state = s.engine.state();
      state.fields[2].value =
          std::uint32_t{runtime::context::sample_loss_enable |
                        runtime::context::sample_loss_indicator};
      runtime::EffectiveEvent event;
      event.state = state;
      event.actual_time = s.timeline.time();
      event.sample_ordinal = s.timeline.ordinal();
      event.association_generation = s.generation;
      event.changed_mask = 4;
      event.time_known = event.ordinal_known = true;
      auto binding = s.revisions.binding();
      binding.record(binding.context, event, *reservation, std::move(*credits));
      auto published = s.publisher.progress(now_, *clock_snapshot_);
      if (!published)
        return published;
      s.pending_loss = false;
    }
    auto current = s.revisions.current();
    if (!current)
      return std::unexpected(current.error());
    const auto payload_size =
        *pairs * profiles::iq::bytes_per_pair(s.config.format);
    auto header = pools_.header.acquire({28});
    if (!header)
      return std::unexpected(header.error());
    auto payload = pools_.payload.acquire({payload_size});
    if (!payload)
      return std::unexpected(payload.error());
    std::optional<memory::BufferLease> trailer;
    if (s.config.trailer) {
      auto acquired = pools_.trailer.acquire({4});
      if (!acquired)
        return std::unexpected(acquired.error());
      trailer.emplace(std::move(*acquired));
    }
    auto payload_bytes = payload->writable_bytes();
    if (!payload_bytes)
      return std::unexpected(payload_bytes.error());
    auto window = profiles::iq::SampleWriteWindow::create(
        payload_bytes->first(payload_size), s.config.format,
        s.timeline.ordinal(), *pairs, current->event().state);
    if (!window)
      return std::unexpected(window.error());
    auto produced = s.config.source.produce(*window);
    if (!produced)
      return produced;
    auto complete = window->validate_complete();
    if (!complete)
      return complete;
    auto e = envelope(s, codec::PacketType::signal);
    e.timestamp = {epoch(), codec::Tsf::picoseconds,
                   static_cast<std::uint32_t>(s.timeline.time().seconds),
                   s.timeline.time().picoseconds};
    e.trailer = s.config.trailer;
    if (s.timeline.time().seconds > UINT32_MAX)
      return std::unexpected(Error{ErrorCode::overflow});
    auto head_bytes = header->writable_bytes();
    if (!head_bytes)
      return std::unexpected(head_bytes.error());
    auto written = codec::encode_prologue(e, payload_size, *head_bytes);
    if (!written)
      return std::unexpected(written.error());
    auto last = s.timeline;
    auto last_advanced = last.advance(*pairs - 1);
    if (!last_advanced)
      return last_advanced;
    typename Stream::PacketStamp *stamp = nullptr;
    for (auto &item : s.stamps)
      if (!item.header) {
        stamp = &item;
        break;
      }
    if (!stamp)
      return std::unexpected(Error{ErrorCode::capacity_exhausted});
    stamp->header = head_bytes->data();
    stamp->last_sample = last.time();
    header->set_size(*written);
    payload->set_size(payload_size);
    memory::TxStorage storage;
    auto appended = storage.append(std::move(*header), 0, *written);
    if (!appended)
      return appended;
    appended = storage.append(std::move(*payload), 0, payload_size);
    if (!appended)
      return appended;
    if (trailer) {
      auto bytes = trailer->writable_bytes();
      if (!bytes)
        return std::unexpected(bytes.error());
      auto encoded = codec::encode_trailer(0, *bytes);
      if (!encoded)
        return std::unexpected(encoded.error());
      trailer->set_size(4);
      appended = storage.append(std::move(*trailer), 0, 4);
      if (!appended)
        return appended;
    }
    auto next = s.timeline;
    auto advanced = next.advance(*pairs);
    if (!advanced)
      return advanced;
    s.timeline = next;
    auto pacing = s.pacing.advance(*pairs);
    if (!pacing)
      return pacing;
    auto submitted = s.publisher.submit(std::move(storage), *current, now_);
    if (!submitted) {
      stamp->header = nullptr;
      ++s.metrics.send_failures;
      return submitted;
    }
    s.metrics.samples += *pairs;
    return {};
  }
  Result<void> service(Stream &s) noexcept {
    auto context = operation_context(s);
    for (std::size_t work = 0; work < Transactions * 4 + 1; ++work) {
      auto progressed = s.manager.progress(context);
      if (!progressed)
        return progressed;
      if (!s.backend.pending() || !s.auto_complete)
        break;
      auto completed = s.backend.complete_next();
      if (!completed)
        return std::unexpected(completed.error());
    }
    if (s.engine.faulted()) {
      if (!lifecycle_[s.index].active || s.retired)
        s.status = SourceStatus::faulted;
      s.publisher.backend_fault(s.engine.state());
    }
    const auto *rate = std::get_if<Hertz>(&s.engine.state().fields[1].value);
    if (rate && rate->q20 > 0 && rate->q20 % (1ll << 20) == 0) {
      const auto hz = static_cast<std::uint64_t>(rate->q20 >> 20);
      if (hz != s.timeline.rate()) {
        auto current = s.revisions.current();
        std::uint64_t advance = 0;
        if (current && current->event().ordinal_known &&
            current->event().sample_ordinal >= s.timeline.ordinal())
          advance = current->event().sample_ordinal - s.timeline.ordinal();
        else if (s.status != SourceStatus::running) {
          auto elapsed = intervals_on(s.pacing, mono_protocol(now_), 1);
          if (!elapsed)
            return std::unexpected(elapsed.error());
          advance = *elapsed;
        }
        if (advance) {
          auto moved = s.timeline.advance(advance);
          if (!moved)
            return moved;
          moved = s.pacing.advance(advance);
          if (!moved)
            return moved;
          s.metrics.skipped_samples += advance;
          s.pending_loss = true;
        }
        auto changed = s.timeline.change_rate(hz);
        if (!changed)
          return changed;
        changed = s.pacing.change_rate(hz);
        if (!changed)
          return changed;
        s.config.sample_rate = hz;
      }
    }
    for (auto &reply : s.replies)
      if (reply) {
        for (;;) {
          auto ack = s.manager.response(reply->token, reply->read);
          if (!ack)
            return std::unexpected(ack.error());
          if (!*ack)
            break;
          auto buffer =
              ((**ack).cancellation ? pools_.cancellation : pools_.control)
                  .acquire({2048});
          if (!buffer)
            return std::unexpected(buffer.error());
          auto bytes = buffer->writable_bytes();
          if (!bytes)
            return std::unexpected(bytes.error());
          auto count = counters_.next(counter(s, codec::PacketType::command));
          if (!count)
            return std::unexpected(count.error());
          auto encoded =
              runtime::transaction::encode_response(**ack, *bytes, *count);
          if (!encoded)
            return std::unexpected(encoded.error());
          buffer->set_size(*encoded);
          memory::TxStorage storage;
          auto appended = storage.append(std::move(*buffer), 0, *encoded);
          if (!appended)
            return appended;
          auto sent =
              send(storage, s, codec::PacketType::command, false, {}, {});
          if (!sent)
            return sent;
          ++reply->read;
        }
        auto complete = s.manager.complete(reply->token);
        if (complete && *complete) {
          s.manager.release(reply->token);
          reply.reset();
        }
      }
    s.receiver.progress(now_);
    if (s.status == SourceStatus::running) {
      if (!clock_snapshot_ ||
          (clock_snapshot_->state != runtime::timing::ClockState::locked &&
           clock_snapshot_->state != runtime::timing::ClockState::holdover)) {
        s.status = SourceStatus::clock_unavailable;
        s.publisher.pause();
        return {};
      }
      auto generated = generate(s);
      if (!generated) {
        s.status = SourceStatus::faulted;
        s.publisher.backend_fault(s.engine.state());
        return generated;
      }
      auto published = s.publisher.progress(now_, *clock_snapshot_);
      if (!published) {
        s.status = s.publisher.status() ==
                           runtime::context::StreamStatus::temporal_association
                       ? SourceStatus::temporal_association
                       : SourceStatus::context_unavailable;
        return {};
      }
    }
    return {};
  }
  Result<TransactionHandle> command(std::size_t index, Hertz rate,
                                    std::uint8_t fields, CommandOptions options,
                                    bool query) {
    if (index >= count_ || !fields || (fields & 0xf0) ||
        options.timing_mode > 4)
      return std::unexpected(Error{ErrorCode::invalid_argument});
    freeze();
    auto &s = *streams_[index];
    if (shutdown_requested_ || lifecycle_[index].active || s.retired)
      return std::unexpected(Error{ErrorCode::invalid_state});
    auto e = envelope(s, codec::PacketType::command, true);
    std::uint32_t cam =
        0xa0000000u | (options.partial ? 1u << 27 : 0) |
        (options.allow_warning ? 1u << 26 : 0) |
        (options.allow_error ? 1u << 25 : 0) | (options.nack ? 1u << 22 : 0) |
        (options.validation ? 1u << 20 : 0) |
        (options.execution ? 1u << 19 : 0) | (options.state ? 1u << 18 : 0) |
        (options.details ? 3u << 16 : 0) | (options.timing_mode << 12);
    const unsigned action = query ? 0 : options.dry_run ? 1 : 2;
    cam |= action << 23;
    e.command->cam = cam;
    if (options.timing_mode) {
      if (!options.execute_at || options.execute_at->seconds > UINT32_MAX)
        return std::unexpected(Error{ErrorCode::invalid_argument});
      e.timestamp = {epoch(), codec::Tsf::picoseconds,
                     static_cast<std::uint32_t>(options.execute_at->seconds),
                     options.execute_at->picoseconds};
    }
    auto buffer = pools_.control.acquire({2048});
    if (!buffer)
      return std::unexpected(buffer.error());
    auto bytes = buffer->writable_bytes();
    if (!bytes)
      return std::unexpected(bytes.error());
    Result<std::size_t> encoded;
    if (query) {
      QueryPacket packet;
      for (std::size_t i = 0; i < 4; ++i)
        if (fields & (1u << i)) {
          auto selected = packet.select(runtime::baseline_fields[i]);
          if (!selected)
            return std::unexpected(selected.error());
        }
      encoded = codec::encode_packet(e, packet.freeze(), *bytes);
    } else {
      if (fields != (1u << 1))
        return std::unexpected(Error{ErrorCode::unsupported_capability});
      ControlPacket packet;
      packet.configure(0x20, action);
      auto set = packet.set<SampleRate>(rate);
      if (!set)
        return std::unexpected(set.error());
      encoded = codec::encode_packet(e, packet.freeze(), *bytes);
    }
    if (!encoded)
      return std::unexpected(encoded.error());
    auto parsed = codec::decode_packet(bytes->first(*encoded));
    if (!parsed)
      return std::unexpected(parsed.error());
    auto tracked =
        controllers_.track(s.relationship, *parsed, now_, options.timeout_ns);
    if (!tracked)
      return std::unexpected(tracked.error());
    // Only MID differs after tracking; checked re-encode preserves original
    // meaning.
    e = tracked->envelope;
    if (query) {
      QueryPacket packet;
      for (std::size_t i = 0; i < 4; ++i)
        if (fields & (1u << i))
          packet.select(runtime::baseline_fields[i]);
      encoded = codec::encode_packet(e, packet.freeze(), *bytes);
    } else {
      ControlPacket packet;
      packet.configure(0x20, action);
      packet.set<SampleRate>(rate);
      encoded = codec::encode_packet(e, packet.freeze(), *bytes);
    }
    if (!encoded)
      return std::unexpected(encoded.error());
    buffer->set_size(*encoded);
    memory::TxStorage storage;
    auto appended = storage.append(std::move(*buffer), 0, *encoded);
    if (!appended)
      return std::unexpected(appended.error());
    auto sent =
        send(storage, s, codec::PacketType::command, true, tracked->handle, {});
    if (!sent) {
      controllers_.mark_terminal(tracked->handle, false, now_);
      controllers_.release(tracked->handle);
      return std::unexpected(sent.error());
    }
    books_[tracked->handle.slot] = Book{};
    books_[tracked->handle.slot].generation = tracked->handle.generation;
    books_[tracked->handle.slot].stream = index;
    books_[tracked->handle.slot].request = e;
    return TransactionHandle{tracked->handle, index, runtime_id_};
  }
  Result<void> cancel(TransactionHandle handle, std::uint8_t fields,
                      CommandOptions options) {
    if (handle.runtime_id != runtime_id_ || handle.stream >= count_ ||
        handle.controller.slot >= books_.size() || !fields || (fields & 0xf0))
      return std::unexpected(Error{ErrorCode::invalid_argument});
    auto &book = books_[handle.controller.slot];
    if (book.generation != handle.controller.generation)
      return std::unexpected(Error{ErrorCode::stale_generation});
    auto *bank = streams_[handle.stream].get();
    if (bank->config.sid != book.request.stream_id)
      bank = retired_[handle.stream].get();
    if (bank->config.sid != book.request.stream_id)
      return std::unexpected(Error{ErrorCode::stale_generation});
    auto &s = *bank;
    auto e = book.request;
    e.cancel = true;
    e.ack = false;
    e.command->cam = 0xa0000000u | (2u << 23) | (1u << 27) | (1u << 19) |
                     (options.state ? 1u << 18 : 0) |
                     (options.details ? 3u << 16 : 0) |
                     (options.timing_mode << 12);
    e.packet_count =
        *counters_.next(counter(s, codec::PacketType::command, true));
    e.timestamp = {};
    if (options.timing_mode) {
      if (!options.execute_at || options.execute_at->seconds > UINT32_MAX)
        return std::unexpected(Error{ErrorCode::invalid_argument});
      e.timestamp = {epoch(), codec::Tsf::picoseconds,
                     static_cast<std::uint32_t>(options.execute_at->seconds),
                     options.execute_at->picoseconds};
    }
    auto buffer = pools_.cancellation.acquire({2048});
    if (!buffer)
      return std::unexpected(buffer.error());
    auto bytes = buffer->writable_bytes();
    if (!bytes)
      return std::unexpected(bytes.error());
    CancelPacket packet;
    for (std::size_t i = 0; i < 4; ++i)
      if (fields & (1u << i))
        packet.select(runtime::baseline_fields[i]);
    auto encoded = codec::encode_packet(e, packet.freeze(), *bytes);
    if (!encoded)
      return std::unexpected(encoded.error());
    auto parsed = codec::decode_packet(bytes->first(*encoded));
    if (!parsed)
      return std::unexpected(parsed.error());
    auto registered = controllers_.register_cancel(handle.controller, *parsed,
                                                   now_, options.timeout_ns);
    if (!registered)
      return std::unexpected(registered.error());
    book.cancel_cam = e.command->cam;
    buffer->set_size(*encoded);
    memory::TxStorage storage;
    auto appended = storage.append(std::move(*buffer), 0, *encoded);
    if (!appended)
      return appended;
    return send(storage, s, codec::PacketType::command, true, handle.controller,
                {});
  }
  runtime::transaction::TransactionKey
  relationship_key(const Stream &s, bool controller_side) const noexcept {
    return {
        s.generation,
        {controller_side ? s.config.controllee_peer : s.config.controller_peer,
         s.generation},
        s.config.sid,
        codec::Identifier::short_id(s.config.controller_id),
        codec::Identifier::short_id(s.config.controllee_id),
        0};
  }
  std::size_t outstanding_io(const Stream &s) const noexcept {
    std::size_t n = 0;
    for (const auto &io : io_)
      if (io.bank == &s && transport_.outstanding(io.token))
        ++n;
    return n;
  }
  bool pending_replies(const Stream &s) const noexcept {
    for (const auto &reply : s.replies)
      if (reply)
        return true;
    return false;
  }
  bool reusable(Stream &s) noexcept {
    if (!s.installed)
      return true;
    retention_.expire(now_);
    controllers_.expire(now_);
    return s.engine.safe_to_reset() && !pending_replies(s) &&
           !outstanding_io(s) &&
           !retention_.association_retained(relationship_key(s, false)) &&
           !controllers_.association_retained(relationship_key(s, true));
  }
  Result<void> install_association(Stream &s) noexcept {
    if (routes_.available() < 4 || counters_.available() < 4)
      return std::unexpected(Error{ErrorCode::capacity_exhausted});
    auto key = relationship_key(s, true);
    auto can = controllers_.can_register_relationship(key);
    if (!can)
      return can;
    std::array<runtime::Route, 4> batch;
    const std::array types{
        codec::PacketType::command, codec::PacketType::command,
        codec::PacketType::context, codec::PacketType::signal};
    const std::array callbacks{Stream::receive_command, Stream::receive_ack,
                               Stream::receive_context, Stream::receive_data};
    std::array<runtime::CounterKey, 4> keys;
    for (std::size_t i = 0; i < 4; ++i) {
      auto e = envelope(s, types[i], i == 0);
      auto &route = batch[i];
      route.key = {
          {i == 0 ? s.config.controller_peer : s.config.controllee_peer,
           s.generation},
          s.config.sid,
          types[i],
          e.class_id};
      if (e.command) {
        route.key.controllee = e.command->controllee;
        route.key.controller = e.command->controller;
      }
      route.context = &s;
      route.receive = callbacks[i];
      if (i == 1)
        route.request_context = Stream::request_context;
      keys[i] = counter(s, types[i], i == 0);
    }
    // Fresh SID and fixed four-role shapes make both capacity-preflighted
    // installs infallible.
    auto installed = routes_.install(batch);
    if (!installed)
      return installed;
    installed = counters_.install(keys);
    if (!installed)
      return installed;
    auto registered = controllers_.register_relationship(key);
    if (!registered)
      return std::unexpected(registered.error());
    s.relationship = *registered;
    s.installed = true;
    return {};
  }
  LifecycleStatus lifecycle_status(std::size_t index) const noexcept {
    auto status = lifecycle_[index].status;
    const auto sid = lifecycle_[index].original_sid;
    const auto *bank = streams_[index].get();
    if (bank->config.sid != sid)
      bank = retired_[index].get();
    if (bank->config.sid == sid) {
      const auto drained = bank->engine.drain_status();
      status.effects = drained.active;
      status.io = outstanding_io(*bank);
      status.capabilities = drained.capability_holders;
      status.backend_quiescent =
          drained.backend.known && drained.backend.quiescent;
    }
    return status;
  }
  void finish_lifecycle(std::size_t index, LifecyclePhase phase,
                        ErrorCode error = ErrorCode::invalid_state) noexcept {
    auto &op = lifecycle_[index];
    op.status.phase = phase;
    op.status.error = error;
    op.active = false;
    for (auto &book : books_)
      if (book.stream == index && book.request.stream_id == op.original_sid)
        book.callback = nullptr;
    auto completion = std::exchange(op.completion, {});
    const auto completed_status = op.status;
    if (completion.callback)
      completion.callback(completion.context, completed_status);
  }
  Result<void> progress_lifecycle(std::size_t index) noexcept {
    auto &op = lifecycle_[index];
    if (!op.active)
      return {};
    auto &old = *streams_[index];
    if (op.status.phase == LifecyclePhase::starting) {
      if (old.publisher.published_highwater())
        finish_lifecycle(index, LifecyclePhase::running);
      else if (old.status == SourceStatus::context_unavailable ||
               now_ >= op.deadline) {
        old.status = SourceStatus::faulted;
        finish_lifecycle(index, LifecyclePhase::failed,
                         ErrorCode::capacity_exhausted);
      }
      return {};
    }
    if (op.recover && !op.reinitialized && op.recovery.reinitialize) {
      auto confirmed = op.recovery.reinitialize(
          op.recovery.reinitialize_context, op.recovery.confirmed_state);
      if (!confirmed) {
        old.status = SourceStatus::faulted;
        finish_lifecycle(index, LifecyclePhase::failed, confirmed.error().code);
        return {};
      }
      if (!*confirmed) {
        if (now_ >= op.deadline) {
          old.status = SourceStatus::faulted;
          finish_lifecycle(index, LifecyclePhase::quarantined);
        }
        return {};
      }
      // Callback true is the explicit adapter reinitialization/quiescence
      // contract.
      old.backend.set_quiescence_available(true);
      auto reset_backend = old.backend.reinitialize();
      if (!reset_backend)
        return reset_backend;
      op.reinitialized = true;
      auto accounted = old.manager.progress(operation_context(old));
      if (!accounted)
        return accounted;
    }
    const auto drained = old.engine.drain_status();
    op.status.effects = drained.active;
    op.status.io = outstanding_io(old);
    op.status.capabilities = drained.capability_holders;
    op.status.backend_quiescent =
        drained.backend.known && drained.backend.quiescent;
    if (op.status.effects || op.status.io || !op.status.backend_quiescent ||
        pending_replies(old)) {
      if (op.immediate || now_ >= op.deadline) {
        old.status = SourceStatus::faulted;
        finish_lifecycle(index, LifecyclePhase::quarantined,
                         ErrorCode::invalid_state);
      }
      return {};
    }
    if (!op.recover) {
      old.status = SourceStatus::stopped;
      finish_lifecycle(index, LifecyclePhase::stopped);
      return {};
    }
    op.status.phase = LifecyclePhase::reinitializing;
    op.reinitialized = true;
    auto &next = *retired_[index];
    if (!reusable(next)) {
      old.status = SourceStatus::faulted;
      finish_lifecycle(index, LifecyclePhase::failed,
                       ErrorCode::capacity_exhausted);
      return {};
    }
    auto snapshot = clock_.snapshot(now_);
    if (!snapshot || snapshot->state != runtime::timing::ClockState::locked) {
      old.status = SourceStatus::faulted;
      finish_lifecycle(index, LifecyclePhase::failed, ErrorCode::invalid_state);
      return {};
    }
    const auto generation = old.generation + 1;
    auto reset =
        next.engine.reset_state(op.recovery.confirmed_state, generation);
    if (!reset) {
      finish_lifecycle(index, LifecyclePhase::failed, reset.error().code);
      return {};
    }
    routes_.detach(
        &next); // old keys stay reserved but cannot dispatch to a reused bank.
    next.config = old.config;
    next.config.sid = op.recovery.new_sid;
    next.config.sample_rate = static_cast<std::uint64_t>(
        std::get<Hertz>(op.recovery.confirmed_state.fields[1].value).q20 >> 20);
    for (auto format :
         {profiles::iq::SampleFormat::iq16, profiles::iq::SampleFormat::iq32,
          profiles::iq::SampleFormat::float32})
      if (std::get<PayloadFormat>(
              op.recovery.confirmed_state.fields[3].value) ==
          profiles::iq::payload_format(format))
        next.config.format = format;
    next.generation = generation;
    next.retired = false;
    next.status = SourceStatus::stopped;
    next.data_highwater.reset();
    next.coverage_floor.reset();
    next.pending_loss = false;
    next.metrics = old.metrics;
    for (auto &stamp : next.stamps)
      stamp.header = nullptr;
    next.timeline = old.timeline;
    next.pacing = old.pacing;
    auto elapsed = intervals_on(next.pacing, mono_protocol(now_), 1);
    if (!elapsed)
      return std::unexpected(elapsed.error());
    std::uint64_t skipped = *elapsed;
    auto advanced = next.pacing.advance(skipped);
    if (!advanced)
      return advanced;
    if (next.pacing.time() < mono_protocol(now_)) {
      advanced = next.pacing.advance(1);
      if (!advanced)
        return advanced;
      ++skipped;
    }
    advanced = next.timeline.advance(skipped);
    if (!advanced)
      return advanced;
    auto offset =
        runtime::timing::difference(next.pacing.time(), mono_protocol(now_));
    if (!offset)
      return std::unexpected(offset.error());
    auto anchored = runtime::timing::add(snapshot->time, *offset);
    if (!anchored)
      return std::unexpected(anchored.error());
    advanced = next.timeline.reanchor_mapping(next.timeline.time(), *anchored);
    if (!advanced)
      return advanced;
    advanced = next.timeline.change_rate(next.config.sample_rate);
    if (!advanced)
      return advanced;
    advanced = next.pacing.change_rate(next.config.sample_rate);
    if (!advanced)
      return advanced;
    next.metrics.skipped_samples += skipped;
    next.pending_loss = skipped != 0;
    reset = next.publisher.detach(generation);
    if (!reset)
      return reset;
    reset = next.receiver.detach(generation, epoch(), next.config.sid);
    if (!reset)
      return reset;
    reset = next.manager.reset_after_drain();
    if (!reset)
      return reset;
    auto installed = install_association(next);
    if (!installed) {
      finish_lifecycle(index, LifecyclePhase::failed, installed.error().code);
      return {};
    }
    sid_history_[sid_count_++] = next.config.sid;
    old.retired = true;
    auto detached = old.publisher.detach(generation);
    if (!detached)
      return detached;
    detached = old.receiver.detach(generation, epoch(), old.config.sid);
    if (!detached)
      return detached;
    std::swap(streams_[index], retired_[index]);
    op.status.phase = LifecyclePhase::starting;
    auto started = start(index);
    if (!started) {
      streams_[index]->status = SourceStatus::faulted;
      finish_lifecycle(index, LifecyclePhase::failed, started.error().code);
      return {};
    }
    // start installs the Context gate; publication is progressed before this
    // source can emit Data.
    auto published = streams_[index]->publisher.progress(now_, *snapshot);
    if (!published) {
      streams_[index]->status = SourceStatus::faulted;
      finish_lifecycle(index, LifecyclePhase::failed, published.error().code);
      return {};
    }
    if (streams_[index]->publisher.published_highwater())
      finish_lifecycle(index, LifecyclePhase::running);
    return {};
  }
  Result<void> start(std::size_t index) {
    if (index >= count_)
      return std::unexpected(Error{ErrorCode::invalid_argument});
    auto &s = *streams_[index];
    freeze();
    if (shutdown_requested_ ||
        (!lifecycle_[index].active &&
         (lifecycle_[index].status.phase == LifecyclePhase::stopped ||
          lifecycle_[index].status.phase == LifecyclePhase::quarantined)) ||
        s.status == SourceStatus::recovering ||
        s.status == SourceStatus::quiescing ||
        s.status == SourceStatus::temporal_association ||
        s.status == SourceStatus::faulted || s.engine.faulted())
      return std::unexpected(Error{ErrorCode::invalid_state});
    auto snapshot = clock_.snapshot(now_);
    if (!snapshot || snapshot->state != runtime::timing::ClockState::locked)
      return std::unexpected(Error{ErrorCode::invalid_state});
    clock_snapshot_ = *snapshot;
    if (s.status == SourceStatus::configured) {
      auto timeline = runtime::timing::SampleTimeline::create(
          snapshot->time, s.config.sample_rate);
      if (!timeline)
        return std::unexpected(timeline.error());
      s.timeline = *timeline;
      s.pacing = *runtime::timing::SampleTimeline::create(mono_protocol(now_),
                                                          s.config.sample_rate);
    }
    if (!s.revisions.current()) {
      runtime::AdmissionRequest request;
      request.need(runtime::Resource::revision)
          .need(runtime::Resource::context_publication);
      auto credits = admission_.acquire(request);
      if (!credits)
        return std::unexpected(credits.error());
      runtime::EffectiveEvent event;
      event.state = s.engine.state();
      event.actual_time = snapshot->time;
      event.sample_ordinal = s.timeline.ordinal();
      event.association_generation = s.generation;
      event.time_known = event.ordinal_known = true;
      auto initial = s.revisions.initial(event, std::move(*credits));
      if (!initial)
        return std::unexpected(initial.error());
    }
    s.coverage_floor = snapshot->time;
    auto started = s.publisher.start(now_);
    if (!started)
      return started;
    s.status = SourceStatus::running;
    return {};
  }
  Result<void> stop(std::size_t index) {
    if (index >= count_)
      return std::unexpected(Error{ErrorCode::invalid_argument});
    auto &s = *streams_[index];
    if (s.status == SourceStatus::temporal_association ||
        s.status == SourceStatus::faulted)
      return std::unexpected(Error{ErrorCode::invalid_state});
    s.publisher.pause();
    for (auto &stamp : s.stamps)
      stamp.header = nullptr;
    s.status = SourceStatus::stopped;
    return {};
  }
  Result<runtime::transaction::Observation>
  observation(TransactionHandle handle) const noexcept {
    if (handle.runtime_id != runtime_id_)
      return std::unexpected(Error{ErrorCode::identity_conflict});
    auto observer = controllers_.observer(handle.controller);
    if (!observer)
      return std::unexpected(observer.error());
    return (*observer)->observation();
  }
  Result<void> observe(
      TransactionHandle handle, void *context,
      void (*callback)(void *,
                       const runtime::transaction::Observation &) noexcept) {
    if (handle.runtime_id != runtime_id_ ||
        handle.controller.slot >= books_.size() ||
        books_[handle.controller.slot].generation !=
            handle.controller.generation)
      return std::unexpected(Error{ErrorCode::stale_generation});
    auto &book = books_[handle.controller.slot];
    book.context = context;
    book.callback = callback;
    book.delivered_count = 0;
    return {};
  }
  Result<void> release(TransactionHandle handle) {
    if (handle.runtime_id != runtime_id_ ||
        handle.controller.slot >= books_.size() ||
        books_[handle.controller.slot].generation !=
            handle.controller.generation)
      return std::unexpected(Error{ErrorCode::stale_generation});
    books_[handle.controller.slot].callback = nullptr;
    return controllers_.release(handle.controller);
  }
  Result<WaitResult> wait(TransactionHandle handle, std::uint64_t timeout_ns,
                          WaitEvidence evidence) {
    if (progressing_)
      return std::unexpected(Error{ErrorCode::would_deadlock});
    if (!config_.isolated_lab || !config_.clock.injected)
      return std::unexpected(Error{ErrorCode::unsupported_capability});
    auto end = runtime::timing::deadline(now_, timeout_ns);
    if (!end)
      return std::unexpected(end.error());
    using K = runtime::transaction::ObservationKind;
    const K requested = evidence == WaitEvidence::execution    ? K::execution
                        : evidence == WaitEvidence::state      ? K::state
                        : evidence == WaitEvidence::validation ? K::validation
                        : evidence == WaitEvidence::cancellation_execution
                            ? K::cancellation_execution
                            : K::cancellation_state;
    for (;;) {
      auto observer = controllers_.observer(handle.controller);
      if (!observer)
        return std::unexpected(observer.error());
      for (const auto &event : (*observer)->observations())
        if (event.kind == requested)
          return WaitResult{WaitStatus::evidence_received, event};
      for (const auto &event : (*observer)->observations())
        if (event.kind == (evidence == WaitEvidence::cancellation_execution ||
                                   evidence == WaitEvidence::cancellation_state
                               ? K::cancellation_timeout
                               : K::timeout))
          return WaitResult{WaitStatus::transaction_timeout, event};
      if (now_ >= *end)
        return WaitResult{WaitStatus::wait_budget_expired,
                          (*observer)->observation()};
      auto advanced = run_for(std::min<std::uint64_t>(1000, end->ns - now_.ns));
      if (!advanced)
        return std::unexpected(advanced.error());
    }
  }

public:
  static Result<std::unique_ptr<VitaRuntime>> create(RuntimeConfig config,
                                                     ExternalPools pools) {
    if (!config.oui || *config.oui > 0xffffff ||
        !config.clock.epoch_configured ||
        (!config.clock.qualified && !config.clock.injected) ||
        !config.memory_limit || config.memory_limit > runtime::framework_budget)
      return std::unexpected(Error{ErrorCode::invalid_argument});
    if (config.clock.injected && !config.isolated_lab)
      return std::unexpected(Error{ErrorCode::invalid_argument});
    std::array<memory::ExternalPool *, 10> all{
        &pools.header,     &pools.payload,         &pools.trailer,
        &pools.control,    &pools.cancellation,    &pools.rx_data,
        &pools.rx_control, &pools.rx_cancellation, &pools.emergency,
        &pools.large};
    for (auto *pool : all)
      if (!pool->block_count() &&
          !(pool == &pools.emergency && config.isolated_lab) &&
          pool != &pools.large)
        return std::unexpected(Error{ErrorCode::invalid_argument});
    if (pools.rx_data.shares_provider_with(pools.rx_control) ||
        pools.rx_data.shares_provider_with(pools.rx_cancellation) ||
        pools.rx_control.shares_provider_with(pools.rx_cancellation) ||
        pools.control.shares_provider_with(pools.cancellation))
      return std::unexpected(Error{ErrorCode::invalid_argument});
    for (auto *data : {&pools.header, &pools.payload, &pools.trailer})
      if (data->shares_provider_with(pools.control) ||
          data->shares_provider_with(pools.cancellation))
        return std::unexpected(Error{ErrorCode::invalid_argument});
    for (auto *data :
         {&pools.header, &pools.payload, &pools.trailer, &pools.rx_data})
      for (auto *control : {&pools.control, &pools.cancellation,
                            &pools.rx_control, &pools.rx_cancellation})
        if (data->shares_provider_with(*control))
          return std::unexpected(Error{ErrorCode::invalid_argument});
    for (auto *ordinary : {&pools.control, &pools.rx_control})
      for (auto *cancellation : {&pools.cancellation, &pools.rx_cancellation})
        if (ordinary->shares_provider_with(*cancellation))
          return std::unexpected(Error{ErrorCode::invalid_argument});
    for (std::size_t i = 0; i < 8; ++i)
      if (pools.emergency.shares_provider_with(*all[i]))
        return std::unexpected(Error{ErrorCode::invalid_argument});
    if (!pools.header.supports({28}) || !pools.trailer.supports({4}) ||
        !pools.control.supports({2048}) ||
        !pools.cancellation.supports({2048}) ||
        !pools.rx_control.supports({2048}) ||
        !pools.rx_cancellation.supports({2048}) ||
        (pools.emergency.block_count() && !pools.emergency.supports({2048})))
      return std::unexpected(Error{ErrorCode::unsupported_capability});
    // Count each externally supplied provider once; sharing one provider among
    // compatible TX roles is explicit.
    std::size_t raw = 0, metadata = 0;
    for (std::size_t i = 0; i < all.size(); ++i) {
      if (!all[i]->block_count())
        continue;
      bool duplicate = false;
      for (std::size_t j = 0; j < i; ++j)
        duplicate |= all[i]->shares_provider_with(*all[j]);
      if (duplicate)
        continue;
      auto bytes = all[i]->raw_bytes();
      if (!bytes || *bytes > SIZE_MAX - raw)
        return std::unexpected(Error{ErrorCode::overflow});
      raw += *bytes;
      metadata += all[i]->metadata_bytes() + 128;
    }
    const std::size_t base =
        sizeof(VitaRuntime) +
        runtime::transaction::RetentionStore<CacheEntries,
                                             CacheBytes>::storage_bytes() +
        runtime::transaction::ControllerRegistry<>::storage_bytes() +
        runtime::CompletionArena<transport_slots>::metadata_bytes() +
        transport_slots * runtime::QuiescenceGuard::metadata_bytes() +
        runtime::AdmissionPool::metadata_bytes() + 4096;
    if (raw > config.memory_limit || metadata > config.memory_limit - raw ||
        base > config.memory_limit - raw - metadata)
      return std::unexpected(Error{ErrorCode::capacity_exhausted});
    auto result =
        std::unique_ptr<VitaRuntime>(new VitaRuntime(config, std::move(pools)));
    if (auto bound = result->clock_.bind(config.clock); !bound)
      return std::unexpected(bound.error());
    for (auto item :
         std::array<std::pair<runtime::BudgetCategory, std::size_t>, 4>{
             {{runtime::BudgetCategory::raw_blocks, raw},
              {runtime::BudgetCategory::providers, metadata},
              {runtime::BudgetCategory::duplicate_values, CacheBytes},
              {runtime::BudgetCategory::duplicate_index,
               runtime::transaction::RetentionStore<
                   CacheEntries, CacheBytes>::storage_bytes() -
                   CacheBytes}}}) {
      auto charged = result->charge(item.first, item.second);
      if (!charged)
        return std::unexpected(charged.error());
    }
    auto charged =
        result->charge(runtime::BudgetCategory::adapters_stacks,
                       base - runtime::transaction::RetentionStore<
                                  CacheEntries, CacheBytes>::storage_bytes());
    if (!charged)
      return std::unexpected(charged.error());
    return result;
  }
  Result<Controllee> add_controllee(StreamConfig config) {
    if (frozen_ || count_ == Streams || count_ == 16 || !config.sid ||
        !config.controller_id || !config.controllee_id ||
        !config.source.callback ||
        config.controller_peer == config.controllee_peer)
      return std::unexpected(Error{ErrorCode::invalid_argument});
    if (config.trailer &&
        (!config.trailer_packet_class || *config.trailer_packet_class <= 3 ||
         *config.trailer_packet_class == 0x10 ||
         *config.trailer_packet_class == 0x20))
      return std::unexpected(Error{ErrorCode::invalid_argument});
    auto pairs = packet_samples(config);
    if (!pairs)
      return std::unexpected(pairs.error());
    if (!pools_.payload.supports(
            {*pairs * profiles::iq::bytes_per_pair(config.format)}) ||
        !pools_.rx_data.supports(
            {28 + *pairs * profiles::iq::bytes_per_pair(config.format) +
             (config.trailer ? 4u : 0u)}))
      return std::unexpected(Error{ErrorCode::unsupported_capability});
    for (std::size_t i = 0; i < count_; ++i)
      if (streams_[i]->config.sid == config.sid)
        return std::unexpected(Error{ErrorCode::identity_conflict});
    const auto size =
        sizeof(Stream) + runtime::context::RevisionStore<128>::storage_bytes() +
        sizeof(runtime::transaction::ResultStorage<Transactions * 4>) +
        runtime::CompletionArena<Transactions * 4>::metadata_bytes() + 2048;
    auto old = budget_;
    auto charged = charge(runtime::BudgetCategory::scheduling, size * 2);
    if (!charged) {
      budget_ = old;
      return std::unexpected(charged.error());
    }
    auto stream = std::make_unique<Stream>(this, count_, config);
    auto spare = std::make_unique<Stream>(this, count_, config);
    auto &s = *stream;
    auto make_route =
        [&](bool controller, codec::PacketType type,
            void (*receive)(void *, const codec::PacketView &,
                            const memory::RxEnvelope &) noexcept) {
          auto e = envelope(s, type, controller);
          runtime::Route route;
          route.key = {
              {{controller ? config.controller_peer : config.controllee_peer},
               s.generation},
              config.sid,
              type,
              e.class_id};
          if (e.command) {
            route.key.controllee = e.command->controllee;
            route.key.controller = e.command->controller;
          }
          route.context = &s;
          route.receive = receive;
          if (!controller && type == codec::PacketType::command)
            route.request_context = Stream::request_context;
          return route;
        };
    for (auto route :
         {make_route(true, codec::PacketType::command, Stream::receive_command),
          make_route(false, codec::PacketType::command, Stream::receive_ack),
          make_route(false, codec::PacketType::context,
                     Stream::receive_context),
          make_route(false, codec::PacketType::signal, Stream::receive_data)}) {
      auto added = routes_.add(route);
      if (!added)
        return std::unexpected(added.error());
    }
    for (auto key : {counter(s, codec::PacketType::command, true),
                     counter(s, codec::PacketType::command),
                     counter(s, codec::PacketType::context),
                     counter(s, codec::PacketType::signal)}) {
      auto added = counters_.add(key);
      if (!added)
        return std::unexpected(added.error());
    }
    runtime::transaction::TransactionKey key{
        s.generation,
        {config.controllee_peer, s.generation},
        config.sid,
        codec::Identifier::short_id(config.controller_id),
        codec::Identifier::short_id(config.controllee_id),
        0};
    auto relationship = controllers_.register_relationship(key);
    if (!relationship)
      return std::unexpected(relationship.error());
    s.relationship = *relationship;
    const auto index = count_++;
    s.installed = true;
    sid_history_[sid_count_++] = config.sid;
    retired_[index] = std::move(spare);
    streams_[index] = std::move(stream);
    return Controllee(this, index);
  }
  Result<void> recover_stream(const Controllee &target, RecoveryConfig config,
                              LifecycleCompletion completion = {}) {
    if (target.runtime_ != this || target.index_ >= count_)
      return std::unexpected(Error{ErrorCode::invalid_argument});
    const auto index = target.index_;
    auto &s = *streams_[index];
    if (lifecycle_[index].active || !config.peer_ready || !config.new_sid ||
        s.generation == UINT64_MAX)
      return std::unexpected(Error{ErrorCode::invalid_state});
    for (std::size_t i = 0; i < sid_count_; ++i)
      if (sid_history_[i] == config.new_sid)
        return std::unexpected(Error{ErrorCode::identity_conflict});
    if (sid_count_ == sid_history_.size() || routes_.available() < 4 ||
        counters_.available() < 4 || !reusable(*retired_[index]))
      return std::unexpected(Error{ErrorCode::capacity_exhausted});
    if (!runtime::context::required_known(config.confirmed_state))
      return std::unexpected(Error{ErrorCode::invalid_argument});
    for (std::size_t i = 0; i < 4; ++i) {
      const auto &field = config.confirmed_state.fields[i];
      auto valid = validate_value(field.id, field.value);
      if (field.id != runtime::baseline_fields[i] ||
          field.validity != runtime::Validity::known || !valid)
        return std::unexpected(Error{ErrorCode::invalid_argument});
    }
    const auto *sid =
        std::get_if<std::uint32_t>(&config.confirmed_state.fields[0].value);
    const auto *rate =
        std::get_if<Hertz>(&config.confirmed_state.fields[1].value);
    const auto *format =
        std::get_if<PayloadFormat>(&config.confirmed_state.fields[3].value);
    if (!sid || *sid != config.new_sid || !rate || rate->q20 < (1ll << 20) ||
        rate->q20 > (100'000'000ll << 20) || (rate->q20 % (1ll << 20)) ||
        !format)
      return std::unexpected(Error{ErrorCode::invalid_argument});
    auto candidate = s.config;
    candidate.sid = config.new_sid;
    candidate.sample_rate = static_cast<std::uint64_t>(rate->q20 >> 20);
    bool found = false;
    for (auto f :
         {profiles::iq::SampleFormat::iq16, profiles::iq::SampleFormat::iq32,
          profiles::iq::SampleFormat::float32})
      if (*format == profiles::iq::payload_format(f)) {
        candidate.format = f;
        found = true;
      }
    auto pairs = packet_samples(candidate);
    if (!found || !pairs ||
        !pools_.payload.supports(
            {*pairs * profiles::iq::bytes_per_pair(candidate.format)}) ||
        !pools_.rx_data.supports(
            {28 + *pairs * profiles::iq::bytes_per_pair(candidate.format) +
             (candidate.trailer ? 4u : 0u)}))
      return std::unexpected(Error{ErrorCode::unsupported_capability});
    auto clock_candidate = clock_;
    if (config.clock) {
      if (config.clock->binding.epoch != config_.clock.epoch ||
          config.clock->capture != now_)
        return std::unexpected(Error{ErrorCode::invalid_argument});
      auto bound = clock_candidate.bind(config.clock->binding);
      if (!bound)
        return bound;
      auto observed =
          clock_candidate.observe_pps(config.clock->capture, config.clock->time,
                                      config.clock->uncertainty_ps);
      if (!observed)
        return observed;
    }
    auto snapshot = clock_candidate.snapshot(now_);
    if (!snapshot || snapshot->state != runtime::timing::ClockState::locked)
      return std::unexpected(Error{ErrorCode::invalid_state});
    if (config.clock) {
      auto old_clock = clock_;
      auto old = old_clock.snapshot(now_);
      if (old)
        for (std::size_t i = 0; i < count_; ++i) {
          const auto &stream = *streams_[i];
          if (stream.status == SourceStatus::configured)
            continue;
          auto shifted = stream.timeline;
          auto moved = shifted.reanchor_mapping(old->time, snapshot->time);
          if (!moved)
            return moved;
          if (stream.status == SourceStatus::running &&
              old->time != snapshot->time) {
            auto pairs = packet_samples(stream.config);
            if (!pairs)
              return std::unexpected(pairs.error());
            auto elapsed =
                intervals_on(stream.pacing, mono_protocol(now_), *pairs);
            if (!elapsed)
              return std::unexpected(elapsed.error());
            auto pacing = stream.pacing;
            auto advance = pacing.advance(*elapsed * *pairs);
            if (!advance)
              return advance;
            if (pacing.time() < mono_protocol(now_))
              ++*elapsed;
            if (*elapsed > UINT64_MAX / *pairs)
              return std::unexpected(Error{ErrorCode::overflow});
            advance = shifted.advance(*elapsed * *pairs);
            if (!advance)
              return advance;
          }
        }
    }
    auto deadline = runtime::timing::deadline(now_, 2'000'000'000);
    if (!deadline)
      return std::unexpected(deadline.error());
    auto quiescing = s.manager.request_quiesce(operation_context(s));
    if (!quiescing)
      return quiescing;
    s.publisher.pause();
    s.status = SourceStatus::recovering;
    if (config.clock) {
      auto remapped = observe_pps(config.clock->capture, config.clock->time,
                                  config.clock->uncertainty_ps);
      if (!remapped)
        return remapped;
      clock_ = clock_candidate;
      config_.clock = config.clock->binding;
      clock_snapshot_ = *snapshot;
    }
    lifecycle_[index] = {{LifecyclePhase::quiescing},
                         config,
                         completion,
                         *deadline,
                         true,
                         true,
                         false,
                         false,
                         s.config.sid};
    return {};
  }
  Result<void> stop_stream(const Controllee &target,
                           StopMode mode = StopMode::graceful,
                           LifecycleCompletion completion = {}) {
    if (target.runtime_ != this || target.index_ >= count_)
      return std::unexpected(Error{ErrorCode::invalid_argument});
    auto index = target.index_;
    auto &s = *streams_[index];
    if (lifecycle_[index].active)
      return std::unexpected(Error{ErrorCode::invalid_state});
    auto deadline = runtime::timing::deadline(now_, 2'000'000'000);
    if (!deadline)
      return std::unexpected(deadline.error());
    auto closed = s.manager.request_quiesce(operation_context(s));
    if (!closed)
      return closed;
    s.publisher.pause();
    s.status = SourceStatus::quiescing;
    lifecycle_[index] = {
        {LifecyclePhase::quiescing}, {},    completion,  *deadline, true, false,
        mode == StopMode::immediate, false, s.config.sid};
    return {};
  }
  Result<void>
  configure_virtual_backend(const Controllee &target, FieldId field,
                            runtime::transaction::VirtualRule rule,
                            bool auto_complete = true,
                            bool quiescence_available = true) noexcept {
    if (!config_.isolated_lab || target.runtime_ != this)
      return std::unexpected(Error{ErrorCode::unsupported_capability});
    auto &s = *streams_[target.index_];
    auto configured = s.backend.set_rule(field, rule);
    if (!configured)
      return configured;
    s.auto_complete = auto_complete;
    s.backend.set_quiescence_available(quiescence_available);
    return {};
  }
  Result<std::optional<runtime::transaction::AsyncResult>>
  backend_capability(const Controllee &target) const noexcept {
    if (!config_.isolated_lab || target.runtime_ != this)
      return std::unexpected(Error{ErrorCode::unsupported_capability});
    return streams_[target.index_]->backend.pending_capability();
  }
  Result<void> shutdown(StopMode mode = StopMode::graceful,
                        LifecycleCompletion completion = {}) {
    if (shutdown_requested_)
      return std::unexpected(Error{ErrorCode::invalid_state});
    for (std::size_t i = 0; i < count_; ++i)
      if (lifecycle_[i].active)
        return std::unexpected(Error{ErrorCode::invalid_state});
    for (std::size_t i = 0; i < count_; ++i) {
      auto stopped = stop_stream(Controllee(this, i), mode);
      if (!stopped)
        return stopped;
    }
    shutdown_requested_ = true;
    shutdown_completion_ = completion;
    return {};
  }
  Result<void> prove_transport_quiescent(const Controllee &target) noexcept {
    if (!config_.isolated_lab || target.runtime_ != this)
      return std::unexpected(Error{ErrorCode::unsupported_capability});
    bool found = false;
    for (auto &io : io_)
      if (io.bank && io.bank->index == target.index_ &&
          transport_.outstanding(io.token)) {
        auto proof = transport_.prove_quiescent(io.token);
        if (!proof)
          return proof;
        io.bank = nullptr;
        found = true;
      }
    if (!found)
      return std::unexpected(Error{ErrorCode::invalid_state});
    return {};
  }
  Result<Controller> add_controller(const Controllee &target) {
    if (target.runtime_ != this || target.index_ >= count_)
      return std::unexpected(Error{ErrorCode::invalid_argument});
    return Controller(this, target.index_);
  }
  Result<void> observe_pps(MonoTime capture, ProtocolTime time,
                           std::uint64_t uncertainty_ps = 0) {
    auto old = clock_.snapshot(capture);
    auto updated = clock_.observe_pps(capture, time, uncertainty_ps);
    if (!updated)
      return updated;
    auto current = clock_.snapshot(capture);
    if (!current)
      return std::unexpected(current.error());
    if (old)
      for (std::size_t i = 0; i < count_; ++i) {
        auto &s = *streams_[i];
        if (s.status == SourceStatus::configured)
          continue;
        auto shifted = s.timeline;
        auto moved = shifted.reanchor_mapping(old->time, current->time);
        if (!moved)
          return moved;
        const auto high = s.publisher.published_highwater();
        if ((s.data_highwater && shifted.time() <= *s.data_highwater) ||
            (high && shifted.time() <= *high)) {
          s.status = SourceStatus::temporal_association;
          s.publisher.temporal_fault();
        } else {
          s.timeline = shifted;
          if (s.status == SourceStatus::running && old->time != current->time) {
            auto pairs = packet_samples(s.config);
            if (!pairs)
              return std::unexpected(pairs.error());
            auto elapsed =
                intervals_on(s.pacing, mono_protocol(capture), *pairs);
            if (!elapsed)
              return std::unexpected(elapsed.error());
            auto pacing = s.pacing;
            pacing.advance(*elapsed * *pairs);
            if (pacing.time() < mono_protocol(capture))
              ++*elapsed;
            if (*elapsed) {
              auto count = *elapsed * *pairs;
              auto moved = s.timeline.advance(count);
              if (!moved)
                return moved;
              moved = s.pacing.advance(count);
              if (!moved)
                return moved;
              s.metrics.skipped_packets += *elapsed;
              s.metrics.skipped_samples += count;
              s.pending_loss = true;
            }
            s.publisher.pause();
            for (auto &stamp : s.stamps)
              stamp.header = nullptr;
            s.coverage_floor = current->time;
            auto started = s.publisher.start(capture);
            if (!started)
              return started;
          }
        }
      }
    clock_snapshot_ = *current;
    return {};
  }
  Result<void> progress(MonoTime now) noexcept {
    if (progressing_)
      return std::unexpected(Error{ErrorCode::would_deadlock});
    if (now < now_)
      return std::unexpected(Error{ErrorCode::invalid_argument});
    struct Guard {
      bool &flag;
      ~Guard() { flag = false; }
    } guard{progressing_};
    progressing_ = true;
    now_ = now;
    freeze();
    auto clock = clock_.snapshot(now);
    if (clock)
      clock_snapshot_ = *clock;
    for (std::size_t i = 0; i < transport_slots; ++i) {
      auto done = transport_.progress_next();
      if (!done) {
        if (done.error().code == ErrorCode::would_deadlock)
          return std::unexpected(done.error());
        break;
      }
      if (!*done)
        break;
    }
    tx_tickets_.scan([&](runtime::CompletionRecord completion) noexcept {
      for (auto &tx : transmissions_)
        if (tx.active && tx.operation == completion.operation) {
          if (tx.controller)
            controllers_.local_send(*tx.controller,
                                    completion.result.status ==
                                        runtime::CompletionStatus::succeeded);
          tx = TxRecord{};
          break;
        }
    });
    for (std::size_t i = 0; i < count_; ++i) {
      auto serviced = service(*streams_[i]);
      if (retired_[i]->installed) {
        auto old = service(*retired_[i]);
        if (!old)
          return old;
      }
      if (!serviced)
        return serviced;
    }
    for (std::size_t i = 0; i < transport_slots; ++i) {
      auto done = transport_.progress_next();
      if (!done)
        break;
      if (!*done)
        break;
    }
    tx_tickets_.scan([&](runtime::CompletionRecord completion) noexcept {
      for (auto &tx : transmissions_)
        if (tx.active && tx.operation == completion.operation) {
          if (tx.controller)
            controllers_.local_send(*tx.controller,
                                    completion.result.status ==
                                        runtime::CompletionStatus::succeeded);
          tx = TxRecord{};
          break;
        }
    });
    for (auto &io : io_)
      if (io.bank && !transport_.outstanding(io.token))
        io.bank = nullptr;
    auto advanced = controllers_.advance(now);
    if (!advanced)
      return advanced;
    for (std::size_t i = 0; i < count_; ++i) {
      auto advanced_lifecycle = progress_lifecycle(i);
      if (!advanced_lifecycle)
        return advanced_lifecycle;
    }
    if (shutdown_requested_ && !shutdown_notified_) {
      bool done = true;
      LifecycleStatus aggregate;
      aggregate.phase = LifecyclePhase::stopped;
      aggregate.backend_quiescent = true;
      for (std::size_t i = 0; i < count_; ++i) {
        done &= !lifecycle_[i].active;
        auto status = lifecycle_[i].status;
        aggregate.effects += status.effects;
        aggregate.io += status.io;
        aggregate.capabilities += status.capabilities;
        aggregate.backend_quiescent &= status.backend_quiescent;
        if (status.phase == LifecyclePhase::quarantined ||
            status.phase == LifecyclePhase::failed)
          aggregate.phase = LifecyclePhase::quarantined;
      }
      if (done) {
        shutdown_notified_ = true;
        auto callback = std::exchange(shutdown_completion_, {});
        if (callback.callback)
          callback.callback(callback.context, aggregate);
      }
    }
    for (std::size_t i = 0; i < books_.size(); ++i) {
      auto &book = books_[i];
      if (!book.generation || !book.callback)
        continue;
      auto observer = controllers_.observer({i, book.generation});
      if (observer) {
        auto observations = (*observer)->observations();
        for (std::size_t event = 0;
             event < observations.size() && event < book.delivered.size();
             ++event)
          if (event >= book.delivered_count ||
              book.delivered[event] != observations[event]) {
            book.delivered[event] = observations[event];
            const auto generation = book.generation;
            auto callback = book.callback;
            auto context = book.context;
            callback(context, observations[event]);
            if (book.generation != generation || book.callback != callback ||
                book.context != context)
              break;
          }
        if (book.callback)
          book.delivered_count = observations.size();
      }
    }
    return {};
  }
  Result<void> run_for(std::uint64_t duration_ns,
                       std::uint64_t tick_ns = 1000) {
    if (progressing_)
      return std::unexpected(Error{ErrorCode::would_deadlock});
    if (!config_.isolated_lab || !config_.clock.injected || !tick_ns)
      return std::unexpected(Error{ErrorCode::unsupported_capability});
    auto end = runtime::timing::deadline(now_, duration_ns);
    if (!end)
      return std::unexpected(end.error());
    while (now_ < *end) {
      auto next =
          runtime::timing::deadline(now_, std::min(tick_ns, end->ns - now_.ns));
      if (!next)
        return std::unexpected(next.error());
      auto progressed = progress(*next);
      if (!progressed)
        return progressed;
    }
    return {};
  }
  std::optional<runtime::timing::ClockSnapshot>
  clock_snapshot() const noexcept {
    return clock_snapshot_;
  }
  MonoTime monotonic_now() const noexcept { return now_; }
  const runtime::BudgetLedger &budget() const noexcept { return budget_; }
  void set_response_fault(adapters::loopback::Fault fault) noexcept {
    if (config_.isolated_lab)
      response_fault_ = fault;
  }
  void inject_next_transport_fault(adapters::loopback::Fault fault) noexcept {
    if (config_.isolated_lab)
      next_fault_ = fault;
  }
};
} // namespace vita
