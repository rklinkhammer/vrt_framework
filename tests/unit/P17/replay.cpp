#include <cassert>
#include <vita/runtime/transaction/manager.hpp>
using namespace vita;
using namespace vita::runtime;
using namespace vita::runtime::transaction;
int main() {
  AdmissionPool pool(AdmissionPool::reference_capacities());
  VirtualBackend<> backend;
  StateSnapshot state;
  state.profile = profiles::iq::Profile::sdr_radio;
  profiles::iq::SdrCapabilities caps;
  EngineOptions options;
  options.profile = Profile::sdr_radio;
  options.external_retention = true;
  options.sdr_capabilities = &caps;
  Engine<2> engine(pool, backend.binding(), state, options);
  RetentionStore<2, 16384> store(pool);
  TransactionManager<2, 2, 16384> manager(engine, store, pool, true);
  codec::Envelope envelope;
  envelope.type = codec::PacketType::command;
  envelope.stream_id = 1;
  envelope.timestamp = {codec::Tsi::utc, codec::Tsf::picoseconds, 1000, 0};
  envelope.command =
      codec::Command{0xa0140000, 1, codec::Identifier::short_id(1),
                     codec::Identifier::short_id(2)};
  QueryPacket query;
  assert(query.select(Bandwidth::id));
  assert(query.with_attributes(attribute_bit(Attribute::maximum) |
                               attribute_bit(Attribute::minimum)));
  std::array<std::byte, 128> wire{};
  auto size = codec::encode_packet(envelope, query.freeze(), wire);
  assert(size);
  auto packet = codec::decode_packet(Bytes{wire}.first(*size));
  assert(packet);
  OperationContext now;
  now.clock.state = timing::ClockState::locked;
  now.clock.epoch = timing::Epoch::utc;
  now.clock.time = {1000, 0};
  auto first = manager.accept(*packet, now, {7, 1});
  assert(first);
  assert(manager.progress(now));
  auto duplicate = manager.accept(*packet, now, {7, 1});
  assert(duplicate && duplicate->kind == DuplicateKind::replay);
  assert(manager.release(duplicate->token));
  assert(manager.release(first->token));
  store.expire({30'000'000'000});
  assert(store.size() == 0);
  assert(!manager.accept(*packet, now, {7, 1}));
  assert(backend.writes() == 0 && engine.state().version == 0);
  envelope.command->message_id = 2;
  size = codec::encode_packet(envelope, query.freeze(), wire);
  assert(size);
  packet = codec::decode_packet(Bytes{wire}.first(*size));
  assert(packet);
  assert(manager.accept(*packet, now, {7, 1}));
  envelope.command->message_id = 0;
  size = codec::encode_packet(envelope, query.freeze(), wire);
  assert(size);
  packet = codec::decode_packet(Bytes{wire}.first(*size));
  assert(packet);
  assert(!manager.accept(*packet, now, {7, 1}));
}
