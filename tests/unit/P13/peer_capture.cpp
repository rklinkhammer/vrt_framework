#include "../../../bench/peer_capture.hpp"
#include <cassert>
using namespace vita;
using namespace vita::runtime::transaction;
int main() {
  bench::PeerAckPolicy policy;
  policy.oui = 0xabcdef;
  policy.expected_source.port = 4000;
  codec::Envelope original;
  original.type = codec::PacketType::command;
  original.stream_id = 101;
  original.class_id = codec::ClassId{policy.oui, 1, 0x20};
  original.command =
      codec::Command{0xa91f0000, 1, codec::Identifier::short_id(3),
                     codec::Identifier::short_id(2)};
  auto cam = Cam::parse(original, Profile::generic_virtual_test);
  assert(cam);
  AckRecord response;
  response.request = original;
  response.cam = *cam;
  response.kind = AckKind::execution;
  response.scheduled_or_executed = true;
  std::array<std::byte, 256> wire{};
  auto encoded = encode_response(response, wire);
  assert(encoded);
  codec::DecodeOptions options;
  options.request = codec::RequestContext{0xa91f0000};
  auto packet = codec::decode_packet(Bytes{wire}.first(*encoded), options);
  assert(packet);
  // More than 512 later successful sends never make an old issued MID stale.
  auto captured = bench::capture_ack(policy, policy.expected_source, false,
                                     *packet, 10000, 12345);
  assert(captured && captured->mid == 1 && captured->sid == 101 &&
         captured->kind == 3 && captured->success);
  auto duplicate = bench::capture_ack(policy, policy.expected_source, false,
                                      *packet, 10000, 23456);
  assert(duplicate && duplicate->monotonic_ns == 23456);
  auto wrong = policy.expected_source;
  wrong.port++;
  assert(!bench::capture_ack(policy, wrong, false, *packet, 10000, 1));
  assert(!bench::capture_ack(policy, policy.expected_source, true, *packet,
                             10000, 1));
  assert(!bench::capture_ack(policy, policy.expected_source, false, *packet, 1,
                             1));
  auto modified = *packet;
  modified.envelope.envelope.stream_id = 106;
  assert(!bench::capture_ack(policy, policy.expected_source, false, modified,
                             10000, 1));
  modified = *packet;
  modified.envelope.envelope.class_id->oui = 1;
  assert(!bench::capture_ack(policy, policy.expected_source, false, modified,
                             10000, 1));
  modified = *packet;
  modified.envelope.envelope.command->controller =
      codec::Identifier::short_id(99);
  assert(!bench::capture_ack(policy, policy.expected_source, false, modified,
                             10000, 1));
  modified = *packet;
  modified.envelope.envelope.cancel = true;
  assert(!bench::capture_ack(policy, policy.expected_source, false, modified,
                             10000, 1));
}
