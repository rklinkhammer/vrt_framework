#include <cassert>
#include <vita/runtime/transaction/controller.hpp>

using namespace vita;
using namespace vita::codec;
using namespace vita::runtime;
using namespace vita::runtime::transaction;

int main() {
  Envelope request;
  request.type = PacketType::command;
  request.stream_id = 1;
  request.command =
      Command{0xa0040000, 0, Identifier::short_id(2), Identifier::short_id(3)};
  QueryPacket query;
  assert(query.select(RFReferenceFrequency::id));
  assert(query.select(SampleRate::id));
  assert(query.select(Bandwidth::id));
  assert(query.select(Gain::id));
  assert(query.with_attributes(attribute_bit(Attribute::minimum) |
                               attribute_bit(Attribute::maximum)));
  std::array<std::byte, 2048> wire{};
  auto query_size = encode_packet(request, query.freeze(), wire);
  assert(query_size);
  auto packet = decode_packet(Bytes{wire}.first(*query_size));
  assert(packet);

  auto key = transaction_key(*packet, 1, {7, 1});
  assert(key);
  ControllerRegistry<2> registry;
  auto relationship = registry.register_relationship(*key, 42);
  assert(relationship);
  auto tracked = registry.track(*relationship, *packet, {0}, 1'000'000);
  assert(tracked);

  profiles::iq::SdrCapabilities capabilities;
  AckRecord response;
  response.request = tracked->envelope;
  response.cam = *Cam::parse(response.request,
                             runtime::transaction::Profile::sdr_radio);
  response.kind = AckKind::state;
  response.selected_mask = (1u << 1) | (1u << 4) | (1u << 5) | (1u << 6);
  response.sdr_capabilities = &capabilities;
  auto response_size = encode_response(response, wire);
  assert(response_size);
  assert(registry.receive(1, {7, 1}, Bytes{wire}.first(*response_size), {1}));

  capabilities.sample_rate.maximum = *Hertz::from_integer(99'000'000);
  response_size = encode_response(response, wire);
  assert(response_size);
  auto conflicting =
      registry.receive(1, {7, 1}, Bytes{wire}.first(*response_size), {2});
  assert(!conflicting &&
         conflicting.error().code == ErrorCode::identity_conflict);
}