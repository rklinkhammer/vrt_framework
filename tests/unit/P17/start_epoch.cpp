#include <cassert>
#include <cstdlib>
#include <iostream>
#include <source_location>
#include <vita/profiles/iq/lab.hpp>
#include <vita/runtime/public/runtime.hpp>
#include <vita/runtime/transport/stream_framer.hpp>
using namespace vita;
namespace rt = vita::runtime;
namespace tr = vita::runtime::transaction;
template <class T>
void require(const T &result,
             std::source_location where = std::source_location::current()) {
  if (!result) {
    std::cerr << "setup/progress failure at " << where.line() << "\n";
    if constexpr (requires { result.error().code; })
      std::cerr << "error=" << static_cast<int>(result.error().code) << "\n";
    std::exit(2);
  }
}
struct Device {
  rt::timing::ProtocolTime now{};
  rt::timing::ProtocolTime actual{};
  unsigned starts = 0;
  std::uint64_t reported_delay = 0;
  DeviceBackendBinding binding(const std::shared_ptr<Device> &owner) {
    DeviceBackendBinding b;
    b.owner = owner;
    b.storage_bytes = sizeof(Device) + 128;
    b.backend.context = this;
    b.backend.validate = [](void *, FieldId, SemanticValue value,
                            const rt::StateSnapshot &) noexcept {
      return tr::Validation{value, {}, true};
    };
    b.backend.commit = [](void *p, const rt::ExecutionPlan &,
                          rt::timing::Boundary) noexcept {
      return tr::BatchOutcome{rt::FieldStatus::executed,
                              static_cast<Device *>(p)->now, true};
    };
    b.backend.begin = [](void *p, const rt::PlannedField &field,
                         rt::timing::Boundary,
                         tr::AsyncResult completion) noexcept -> Result<void> {
      auto &self = *static_cast<Device *>(p);
      self.actual = *rt::timing::add(
          self.now, rt::timing::from_picoseconds(self.reported_delay));
      if (std::get<std::uint32_t>(field.adjusted) == 3)
        ++self.starts;
      rt::FieldOutcome outcome;
      outcome.id = field.id;
      outcome.value = field.adjusted;
      outcome.status = rt::FieldStatus::executed;
      outcome.validity = rt::Validity::known;
      outcome.actual_time = self.actual;
      outcome.time_known = true;
      if (!completion.complete(outcome))
        return std::unexpected(Error{ErrorCode::invalid_state});
      return {};
    };
    b.backend.quiescence = [](void *) noexcept {
      return tr::BackendQuiescence{true, true, 0};
    };
    b.progress = [](void *p, const tr::OperationContext &context) noexcept {
      static_cast<Device *>(p)->now = context.clock.time;
    };
    return b;
  }
};

using Time = rt::timing::ProtocolTime;
namespace tx = rt::transport;
struct Capture {
  Time epoch{1000, 50'000'000'000};
  std::uint64_t produced = 0, received = 0;
  unsigned packets = 0;
  bool allow_skip = false;
  Time activation{};
  static Result<void> effective(void *p,
                                const rt::EffectiveEvent &event) noexcept {
    auto &c = *static_cast<Capture *>(p);
    if (event.sample_epoch) {
      assert(*event.sample_epoch == c.epoch);
      assert(event.context_time() == c.epoch);
      c.activation = event.actual_time;
      assert(event.outcome.actual_time == event.actual_time);
    }
    return {};
  }
  std::uint64_t last_first = 0;
  static Result<void> produce(void *p,
                              profiles::iq::SampleWriteWindow &w) noexcept {
    auto &c = *static_cast<Capture *>(p);
    if (c.allow_skip)
      c.produced = w.first_ordinal();
    assert(w.first_ordinal() == c.produced);
    c.last_first = c.produced;
    for (std::size_t i = 0; i < w.count(); ++i)
      require(w.write_iq16(
          i, static_cast<std::int16_t>((c.produced + i) % 30000), -123));
    c.produced += w.count();
    return {};
  }
  static void receive(void *p,
                      const rt::context::BorrowedSignalRx &rx) noexcept {
    auto &c = *static_cast<Capture *>(p);
    if (c.allow_skip)
      c.received = c.last_first;
    // Independent integer oracle: no SampleTimeline or framework time
    // arithmetic.
    const auto ps =
        c.epoch.picoseconds + c.received * 1'000'000'000'000ull / 1'000'003;
    assert(rx.sample_time == Time(c.epoch.seconds + ps / 1'000'000'000'000ull,
                                  ps % 1'000'000'000'000ull));
    assert(rx.metadata.confidence == rt::context::Confidence::known);
    if (c.received % 2050 == 0)
      assert(rx.metadata.effective == rx.sample_time);
    std::size_t pairs = 0;
    for (std::size_t f = 0; f < rx.fragment_count(); ++f) {
      auto bytes = rx.fragment(f);
      require(bytes);
      for (std::size_t i = 0; i < bytes->size(); i += 4) {
        const auto code = (std::to_integer<unsigned>((*bytes)[i]) << 8) |
                          std::to_integer<unsigned>((*bytes)[i + 1]);
        assert(code == (c.received + pairs) % 30000);
        ++pairs;
      }
    }
    assert(pairs == std::min<std::uint64_t>(1024, 2050 - c.received % 2050));
    c.received += pairs;
    ++c.packets;
  }
};
struct Probe {
  std::uint32_t start_mid = 0;
  Time v{}, x{}, s{}, first_context{}, context_floor{1000, 50'000'000'000};
  bool have_context = false, have_x = false;
  unsigned data = 0;
  bool replay = false;
  std::array<std::byte, 128> start_wire{};
  std::size_t start_size = 0;
};
using Adapter = adapters::loopback::Loopback<32, 128, 128>;
constexpr tx::Capabilities caps() {
  tx::Capabilities c;
  c.reserved_control_slots = 16;
  c.reserved_cancellation_slots = 8;
  return c;
}
struct Owner {
  Adapter adapter;
  Probe &probe;
  tx::StreamIngress<128, 128> ingress;
  Owner(tx::HostBindings h, Probe &p)
      : adapter(h.rx_data, h.rx_control, h.rx_cancellation, h.admission,
                h.routes, h.counters, caps()),
        probe(p),
        ingress(h.routes, h.rx_data, h.rx_control, h.rx_cancellation, {1, 1}) {}
};
constexpr auto storage =
    sizeof(Owner) + 32 * rt::QuiescenceGuard::metadata_bytes() + 128;
static tx::TransportFactory transport(Probe &probe) {
  return {
      &probe, storage, 32, caps(),
      [](void *p, tx::HostBindings h) noexcept -> Result<tx::TransportBinding> {
        auto owner = std::make_shared<Owner>(h, *static_cast<Probe *>(p));
        return tx::TransportBinding{
            owner,
            owner.get(),
            storage,
            32,
            caps(),
            [](void *p, tx::TxSubmission &&submission) noexcept
                -> std::expected<tx::TxToken, tx::RejectedSubmission> {
              auto &o = *static_cast<Owner *>(p);
              auto &q = o.probe;
              auto frame = tx::inspect(submission.storage);
              assert(frame);
              const auto &e = frame->envelope;
              Time t{e.timestamp.integer, e.timestamp.fractional};
              if (e.command && !e.ack && !e.cancel &&
                  t == Time(1000, 400'000'000'000))
                q.start_mid = e.command->message_id;
              if (e.type == codec::PacketType::context && !q.have_context &&
                  t >= q.context_floor) {
                q.first_context = t;
                q.have_context = true;
              }
              if (e.type == codec::PacketType::signal) {
                assert(q.have_context);
                ++q.data;
              }
              if (e.command && e.command->message_id == q.start_mid && e.ack &&
                  !e.cancel) {
                if (e.command->cam & (1u << 20))
                  q.v = t;
                if (e.command->cam & (1u << 19)) {
                  q.x = t;
                  q.have_x = true;
                }
                if (e.command->cam & (1u << 18))
                  q.s = t;
              }
              // Duplicate every control request: pending and completed
              // transaction paths must not produce a second device activation
              // or reset the epoch.
              if (e.command && !e.ack)
                submission.fault.duplicate = true;
              if (e.command && !e.ack && e.command->message_id == q.start_mid &&
                  !q.start_size) {
                for (std::size_t i = 0; i < submission.storage.segment_count();
                     ++i) {
                  auto bytes = submission.storage.segment(i);
                  assert(bytes);
                  assert(q.start_size + bytes->size() <= q.start_wire.size());
                  std::memcpy(q.start_wire.data() + q.start_size, bytes->data(),
                              bytes->size());
                  q.start_size += bytes->size();
                }
              }
              return o.adapter.try_send(std::move(submission));
            },
            [](void *p) noexcept {
              auto &o = *static_cast<Owner *>(p);
              if (o.probe.replay) {
                o.probe.replay = false;
                auto r = o.ingress.feed(
                    Bytes{o.probe.start_wire}.first(o.probe.start_size));
                if (!r)
                  return Result<bool>{std::unexpected(r.error())};
              }
              return o.adapter.progress_next();
            },
            [](void *p, tx::TxToken t) noexcept {
              return static_cast<Owner *>(p)->adapter.outstanding(t);
            },
            [](void *p) noexcept { static_cast<Owner *>(p)->adapter.close(); },
            [](void *p, tx::TxToken t) noexcept {
              return static_cast<Owner *>(p)->adapter.prove_quiescent(t);
            },
            [](void *, std::span<const tx::Association>,
               bool) noexcept -> Result<void> { return {}; },
            [](void *p) noexcept { static_cast<Owner *>(p)->adapter.close(); }};
      }};
}
int main() {
  for (unsigned sid = 1; sid <= 4; ++sid) {
    auto config = profiles::iq::lab::config(profiles::iq::sdr_unknown_oui);
    require(config);
    config->clock.epoch = rt::timing::Epoch::utc;
    config->timing.device_early_ps = 0;
    config->timing.device_late_ps = 100'000'000'000;
    Probe probe;
    config->transport = transport(probe);
    profiles::iq::lab::PoolCounts counts;
    counts.payload_bytes = 4096;
    counts.rx_data_bytes = 4160;
    auto pools = profiles::iq::lab::pools(counts);
    require(pools);
    auto made =
        VitaRuntime<1, 16, 32, 65536>::create(*config, std::move(*pools));
    require(made);
    auto &runtime = **made;
    auto device = std::make_shared<Device>();
    Capture capture;
    StreamConfig stream;
    stream.sid = stream.controller_id = stream.controllee_id = sid;
    stream.profile = profiles::iq::Profile::sdr_radio;
    stream.trailer = true;
    stream.ip_mtu = 9000;
    stream.maximum_samples_per_packet = 1024;
    stream.burst_pairs = 2050;
    stream.device = device->binding(device);
    stream.source = {&capture, Capture::produce, Capture::effective};
    stream.receiver = {&capture, Capture::receive};
    auto radio = runtime.add_controllee(stream);
    require(radio);
    auto controller = runtime.add_controller(*radio);
    require(controller);
    require(runtime.observe_pps({0}, {1000, 0}));
    SdrRadioSettings settings;
    settings.sample_rate = *Hertz::from_integer(1'000'003);
    auto configured = controller->configure(settings);
    require(configured);
    require(runtime.run_for(5'000'000));
    const auto initial_skips = radio->metrics().skipped_samples;
    auto start = controller->start(capture.epoch);
    require(start);
    // Configuration is MID1; this start is MID2, independently checked on wire.
    probe.start_mid = 2;
    require(runtime.run_for(44'000'000));
    assert(capture.received == 0);
    assert(probe.v == capture.epoch);
    const std::uint64_t delay =
        std::array<std::uint64_t, 4>{0, 100'000, 500'000, 5'000'000}[sid - 1];
    std::uint64_t now = 50'000'000 + delay;
    for (unsigned i = 0; i < 8; ++i)
      require(runtime.progress({now}));
    assert(probe.have_x &&
           probe.x == Time(1000, 50'000'000'000 + delay * 1000));
    assert(probe.s == probe.x && device->actual == probe.x &&
           capture.activation == probe.x);
    assert(probe.first_context == capture.epoch && device->starts == 1);
    const auto actual = probe.x;
    probe.replay = true;
    for (unsigned i = 0; i < 8; ++i)
      require(runtime.progress({now}));
    assert(device->starts == 1 && probe.x == actual &&
           capture.received == 1024);
    for (unsigned packet = 0; packet < 12 && capture.received < 3 * 2050;
         ++packet) {
      now = 50'000'000 + delay +
            (capture.produced * 1'000'000'000ull + 1'000'002) / 1'000'003;
      for (unsigned i = 0; i < 8; ++i)
        require(runtime.progress({now}));
    }
    assert(capture.received == 3 * 2050 &&
           radio->metrics().skipped_samples == initial_skips &&
           device->starts == 1);
    auto stop = controller->stop();
    require(stop);
    require(runtime.run_for(1'000'000));
    assert(radio->status() == SourceStatus::stopped);
    auto cancelled = controller->start({1000, 150'000'000'000});
    require(cancelled);
    require(
        controller->cancel(*cancelled, QuerySelection{QueryField::streaming}));
    require(runtime.run_for(1'000'000));
    require(runtime.progress({160'000'000}));
    assert(device->starts == 1 && radio->status() == SourceStatus::stopped);
    // A new successful start resets ordinal/epoch, independently of the prior
    // run.
    capture = {};
    capture.epoch = {1000, 200'000'000'000};
    probe.have_context = false;
    probe.context_floor = capture.epoch;
    auto restart = controller->start(capture.epoch);
    require(restart);
    require(runtime.run_for(1'000'000));
    for (unsigned i = 0; i < 8; ++i)
      require(runtime.progress({200'500'000}));
    assert(capture.received > 0 && device->starts == 2 &&
           probe.first_context == capture.epoch);
    // After activation, missed host deadlines retain the bounded skip policy.
    capture.allow_skip = true;
    const auto samples_before = radio->metrics().samples;
    const auto skips_before = radio->metrics().skipped_samples;
    for (unsigned i = 0; i < 8; ++i)
      require(runtime.progress({210'500'000}));
    assert(radio->metrics().samples - samples_before <= 1024);
    assert(radio->metrics().skipped_samples > skips_before);
    stop = controller->stop();
    require(stop);
    require(runtime.run_for(1'000'000));
    const auto received = capture.received;
    auto late = controller->start({1000, 250'000'000'000});
    require(late);
    require(runtime.run_for(1'000'000));
    require(runtime.progress({351'000'000}));
    require(runtime.run_for(1'000'000));
    auto rejected = controller->wait(*late, 0, WaitEvidence::execution);
    require(rejected);
    assert(!rejected->observation.confirms_execution && device->starts == 2 &&
           capture.received == received);
    // Backend reports an out-of-tolerance actual effect despite on-time
    // dispatch.
    device->reported_delay = 200'000'000'000;
    auto bad_effect = controller->start({1000, 400'000'000'000});
    require(bad_effect);
    require(runtime.run_for(1'000'000));
    for (unsigned i = 0; i < 8; ++i)
      require(runtime.progress({400'000'000}));
    auto bad = controller->wait(*bad_effect, 0, WaitEvidence::execution);
    require(bad);
    assert(!bad->observation.confirms_execution &&
           radio->status() == SourceStatus::faulted);
    assert(probe.x == Time(1000, 600'000'000'000));
    assert(device->actual == Time(1000, 600'000'000'000) &&
           capture.received == received);
  }
}
