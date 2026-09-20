#include <cassert>
#include <vita/profiles/iq/lab.hpp>
#include <vita/runtime/public/runtime.hpp>
using namespace vita;
int main() {
  auto config = profiles::iq::lab::config(0xabcdef);
  auto pools = profiles::iq::lab::pools();
  assert(config && pools);
  config->clock.epoch = runtime::timing::Epoch::utc;
  auto runtime =
      VitaRuntime<1, 2, 16, 32768>::create(*config, std::move(*pools));
  assert(runtime);
  RemoteTargetConfig target;
  target.sid = 1;
  target.controller_id = 1;
  target.controllee_id = 1;
  target.profile = profiles::iq::Profile::graphx_radio;
  auto controller = (*runtime)->add_remote_controller(target);
  assert(controller);
  assert((*runtime)->observe_pps({0}, {1000, 0}));
  // A loopback transport has no remote peer; endpoint construction must still
  // accept the exact GraphX profile without requiring local trailer settings.

  // Class-ID absence alone must not silently opt a generic Controller into
  // GraphX's state/range policy.
  using namespace vita::runtime::transaction;
  codec::Envelope request;
  request.type = codec::PacketType::command;
  request.stream_id = 1;
  request.command =
      codec::Command{0xa0040000, 1, codec::Identifier::short_id(2),
                     codec::Identifier::short_id(3)};
  QueryPacket query;
  assert(query.select(SampleRate::id));
  std::array<std::byte, 256> bytes{};
  auto size = codec::encode_packet(request, query.freeze(), bytes);
  assert(size);
  auto packet = codec::decode_packet(Bytes{bytes}.first(*size));
  assert(packet);
  auto key = transaction_key(*packet, 1, {7, 1});
  assert(key);
  ControllerRegistry<2> registry;
  auto relationship = registry.register_relationship(*key);
  assert(relationship);
  auto tracked = registry.track(*relationship, *packet, {0}, 1'000'000);
  assert(tracked);
  AckRecord ack;
  ack.request = tracked->envelope;
  ack.cam = *Cam::parse(ack.request, Profile::generic_virtual_test);
  ack.kind = AckKind::state;
  ack.selected_mask = 2;
  ack.state.fields[1] = {SampleRate::id, *Hertz::from_integer(100'000'000),
                         vita::runtime::Validity::known};
  size = encode_response(ack, bytes);
  assert(size);
  assert(registry.receive(1, {7, 1}, Bytes{bytes}.first(*size), {1}));
  auto observed = registry.state_observation(tracked->handle);
  assert(observed && *observed);
  assert((**observed).state.profile == profiles::iq::Profile::generator_v1);
}
