#include <cassert>
#include <vita/runtime/transaction/engine.hpp>
using namespace vita;
using namespace vita::runtime;
using namespace vita::runtime::transaction;
struct Device {
  unsigned calls = 0;
  FieldStatus result = FieldStatus::executed;
  static BatchOutcome commit(void *p, const ExecutionPlan &plan,
                             timing::Boundary b) noexcept {
    auto &self = *static_cast<Device *>(p);
    ++self.calls;
    assert(plan.count == 4);
    return {self.result, b.time, true};
  }
};
int main() {
  for (auto result : {FieldStatus::executed, FieldStatus::failed,
                      FieldStatus::unknown_effect})
    for (bool partial : {false, true})
      for (bool invalid : {false, true})
        for (bool started_before_commit : {false, true}) {
          StateSnapshot initial;
          initial.profile = profiles::iq::Profile::sdr_radio;
          initial.fields[3] = {DataPayloadFormat::id,
                               PayloadFormat{0x200003cf00000000},
                               Validity::known};
          AdmissionPool admission(AdmissionPool::reference_capacities());
          Device device;
          device.result = result;
          Backend backend;
          backend.context = &device;
          backend.commit = Device::commit;
          backend.begin = [](void *, const PlannedField &, timing::Boundary,
                             AsyncResult) noexcept -> Result<void> {
            assert(false);
            return {};
          };
          EngineOptions options;
          options.profile = Profile::sdr_radio;
          Engine<2> engine(admission, backend, initial, options);
          codec::Envelope envelope;
          envelope.type = codec::PacketType::command;
          envelope.stream_id = 1;
          envelope.timestamp = {codec::Tsi::utc, codec::Tsf::picoseconds, 1000,
                                0};
          envelope.command = codec::Command{
              0xa11f0000u | (partial ? 1u << 27 : 0), 1,
              codec::Identifier::short_id(2), codec::Identifier::short_id(3)};
          if (started_before_commit) {
            envelope.command->cam |= 1u << 12;
            envelope.timestamp.fractional = 50'000'000'000;
          }
          ControlPacket packet;
          assert(packet.configure(0, 2));
          assert(packet.set<Bandwidth>(
              *Hertz::from_integer(invalid ? 2'000'000 : 100'000)));
          assert(packet.set<RFReferenceFrequency>(
              *Hertz::from_integer(100'000'000)));
          assert(packet.set<Gain>(GainStages{}));
          assert(packet.set<SampleRate>(*Hertz::from_integer(1'000'000)));
          std::array<std::byte, 256> wire{};
          auto size = codec::encode_packet(envelope, packet.freeze(), wire);
          assert(size);
          auto decoded = codec::decode_packet(Bytes{wire}.first(*size));
          assert(decoded);
          OperationContext now;
          now.clock.epoch = timing::Epoch::utc;
          now.clock.state = timing::ClockState::locked;
          now.clock.time = {1000, 0};
          now.timing = timing::TimingCapabilities::deterministic();
          const std::array boundaries{
              timing::Boundary{{1000, 50'000'000'000}, 0, 0, false, true, 0}};
          now.boundaries = boundaries;
          auto handle = engine.accept(*decoded, now);
          assert(handle);
          assert(engine.progress(now));
          if (started_before_commit) {
            now.clock.time = {1000, 50'000'000'000};
            now.data_running = true;
            assert(engine.progress(now));
          }
          assert(*engine.complete(*handle));
          const bool attempted = !partial && !invalid && !started_before_commit;
          assert(device.calls == unsigned(attempted));
          assert(engine.state().version ==
                 unsigned(attempted && result != FieldStatus::failed));
          for (auto i : {1u, 4u, 5u, 6u})
            assert(engine.state().fields[i].validity ==
                   (!attempted || result == FieldStatus::failed
                        ? Validity::absent
                    : result == FieldStatus::executed ? Validity::known
                                                      : Validity::unknown));
          assert(engine.faulted() ==
                 (attempted && result == FieldStatus::unknown_effect));
        }
}
