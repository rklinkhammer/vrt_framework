#include <array>
#include <cassert>
#include <cstring>
#include <vita/profiles/iq/lab.hpp>
#include <vita/runtime/transport/stream_framer.hpp>
using namespace vita;
using namespace vita::codec;
using namespace vita::runtime::transport;
struct Received {
  std::array<std::size_t, 4> sizes{};
  std::size_t count = 0;
  static Result<void> accept(void *context, Bytes packet) noexcept {
    auto &received = *static_cast<Received *>(context);
    if (received.count == received.sizes.size())
      return std::unexpected(Error{ErrorCode::capacity_exhausted});
    received.sizes[received.count++] = packet.size();
    return {};
  }
};
struct Routed {
  std::size_t packets = 0, payload_bytes = 0;
  static void accept(void *context, const PacketView &packet,
                     const memory::RxEnvelope &received) noexcept {
    auto &routed = *static_cast<Routed *>(context);
    assert(packet.envelope.envelope.type == PacketType::signal);
    for (std::size_t i = 0; i < received.fragment_count(); ++i) {
      auto payload = received.fragment(i);
      assert(payload);
      routed.payload_bytes += payload->size();
    }
    ++routed.packets;
  }
};
static std::array<std::byte, 12> packet(std::uint8_t count) {
  Envelope envelope;
  envelope.type = PacketType::signal;
  envelope.stream_id = 1;
  envelope.packet_count = count;
  std::array<std::byte, 4> payload{};
  std::array<std::byte, 12> wire{};
  auto size = encode_envelope(envelope, payload, {}, wire);
  assert(size && *size == wire.size());
  return wire;
}
int main() {
  auto first = packet(0), second = packet(1);
  Received received;
  StreamFramer<64> framer(32);
  for (auto byte : first) {
    auto delivered = framer.feed(Bytes{&byte, 1}, &received, Received::accept);
    assert(delivered);
  }
  assert(received.count == 1 && received.sizes[0] == 12 && !framer.stalled());

  std::array<std::byte, 24> coalesced{};
  std::memcpy(coalesced.data(), first.data(), first.size());
  std::memcpy(coalesced.data() + first.size(), second.data(), second.size());
  auto delivered = framer.feed(coalesced, &received, Received::accept);
  assert(delivered && *delivered == 2 && received.count == 3);

  auto partial =
      framer.feed(Bytes{first}.first(7), &received, Received::accept);
  assert(partial && *partial == 0 && framer.stalled() &&
         framer.awaiting_bytes() == 5);
  auto disconnected = framer.disconnect();
  assert(!disconnected && disconnected.error().code == ErrorCode::short_input &&
         !framer.stalled());
  framer.reconnect();
  assert(framer.feed(first, &received, Received::accept) &&
         received.count == 4);

  std::array<std::byte, 4> oversize{std::byte{0x10}, std::byte{0}, std::byte{0},
                                    std::byte{9}};
  auto rejected = framer.feed(oversize, &received, Received::accept);
  assert(!rejected && rejected.error().code == ErrorCode::invalid_argument &&
         !framer.stalled());
  framer.shutdown();
  assert(framer.closed() && !framer.feed(first, &received, Received::accept));

  // Sink failure is terminal: the caller must close the connection and cannot
  // replay an unknown suffix of a coalesced read.
  framer.reconnect();
  assert(!framer.feed(coalesced, &received, Received::accept));
  assert(framer.closed() && !framer.feed(first, &received, Received::accept));
  for (auto length : {0u, 1u, 2u, 0xffffu}) {
    framer.reconnect();
    auto bad = first;
    bad[2] = std::byte(length >> 8);
    bad[3] = std::byte(length);
    assert(!framer.feed(bad, &received, Received::accept));
    assert(framer.closed());
  }
  // A host deadline closes a peer stalled in either header or body; reconnect
  // drops every old byte and never dispatches the old prefix.
  for (auto length : {1u, 3u, 7u}) {
    framer.reconnect();
    assert(
        framer.feed(Bytes{first}.first(length), &received, Received::accept));
    assert(framer.stalled());
    framer.shutdown();
    assert(!framer.stalled() && framer.closed());
  }
  auto pools = profiles::iq::lab::pools();
  assert(pools);
  runtime::RouteRegistry<4> routes;
  Routed routed;
  runtime::Route route;
  route.key = {{9, 1}, 1, PacketType::signal};
  route.context = &routed;
  route.receive = Routed::accept;
  route.minimum_payload_bytes = route.maximum_payload_bytes = 4;
  assert(routes.add(route));
  routes.freeze();
  StreamIngress<64, 4> ingress(routes, std::move(pools->rx_data),
                               std::move(pools->rx_control),
                               std::move(pools->rx_cancellation), {9, 1}, 32);
  assert(ingress.feed(Bytes{first}.first(3)));
  assert(ingress.stalled() && routed.packets == 0);
  auto routed_count = ingress.feed(Bytes{first}.subspan(3));
  assert(routed_count && *routed_count == 1 && routed.packets == 1 &&
         routed.payload_bytes == 4);
  assert(ingress.disconnect());
  assert(ingress.reconnect({9, 2}));
  assert(!ingress.feed(first));
  ingress.shutdown();
  assert(ingress.closed());
}
