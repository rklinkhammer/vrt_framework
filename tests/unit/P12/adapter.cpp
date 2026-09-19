#include <cassert>
#include <thread>
#include <vita/adapters/posix_udp/factory.hpp>
#include <vita/profiles/iq/lab.hpp>
using namespace vita;
using namespace vita::adapters::posix_udp;
struct Capture {
  std::size_t count = 0;
  static void receive(void *p, const codec::PacketView &packet,
                      const memory::RxEnvelope &) noexcept {
    auto &self = *static_cast<Capture *>(p);
    ++self.count;
    assert(packet.envelope.payload.size() == 4);
  }
};
int main() {
  for (auto family : {Family::ipv4, Family::ipv6}) {
    auto pools = profiles::iq::lab::pools();
    assert(pools);
    runtime::AdmissionPool admission(
        runtime::AdmissionPool::reference_capacities());
    runtime::RouteRegistry<8> routes;
    runtime::CounterRegistry<8> counters;
    Capture captured;
    runtime::Route route;
    route.key = {{1, 1}, 7, codec::PacketType::signal};
    route.context = &captured;
    route.receive = Capture::receive;
    assert(routes.add(route));
    routes.freeze();
    assert(counters.add({2, 7, codec::PacketType::signal}));
    counters.freeze();
    Config config;
    for (auto &socket : config.sockets)
      socket.bind = Address::loopback(family);
    auto adapter = Udp<4, 8, 8, 8>::create(
        config, {pools->rx_data, pools->rx_control, pools->rx_cancellation},
        admission, routes, counters);
    assert(adapter);
    SocketConfig native_config;
    native_config.bind = Address::loopback(family);
    auto peer = Socket::open(native_config);
    assert(peer);
    PeerBinding binding;
    binding.local_source = {2, 1};
    binding.remote_source = {1, 1};
    binding.remote.fill(peer->local_address());
    assert((*adapter)->add_peer(binding));
    codec::Envelope envelope;
    envelope.type = codec::PacketType::signal;
    envelope.stream_id = 7;
    std::array<std::byte, 4> payload{};
    std::array<std::byte, 64> wire{};
    auto encoded = codec::encode_envelope(envelope, payload, {}, wire);
    assert(encoded);
    std::array<Bytes, 1> packet{Bytes(wire).first(*encoded)};
    assert(peer->send((*adapter)->local_address(Lane::data), packet));
    for (int i = 0; i < 100 && !captured.count; ++i) {
      assert((*adapter)->progress_next());
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    assert(captured.count == 1);
    auto lease = pools->payload.acquire({*encoded});
    assert(lease);
    auto bytes = lease->writable_bytes();
    assert(bytes);
    std::memcpy(bytes->data(), wire.data(), *encoded);
    assert(lease->set_size(*encoded));
    memory::TxStorage storage;
    assert(storage.append(std::move(*lease), 0, *encoded));
    runtime::CompletionArena<4> completions;
    auto ticket = completions.reserve(1);
    assert(ticket);
    auto accepted = (*adapter)->try_send({std::move(storage),
                                          std::move(*ticket),
                                          {2, 1},
                                          {2, 7, codec::PacketType::signal},
                                          {},
                                          {}});
    assert(accepted);
    assert((*adapter)->progress_next());
    assert(!(*adapter)->outstanding(*accepted));
    std::size_t complete = 0;
    completions.scan([&](runtime::CompletionRecord record) noexcept {
      assert(record.result.status == runtime::CompletionStatus::succeeded);
      ++complete;
    });
    assert(complete == 1);
    std::array<std::byte, 64> received{};
    auto rx = peer->receive(received);
    for (int i = 0; !rx && rx.error().retryable && i < 100; ++i) {
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
      rx = peer->receive(received);
    }
    assert(rx && rx->bytes == *encoded &&
           std::memcmp(received.data(), wire.data(), *encoded) == 0);
    (*adapter)->close();
    assert(!(*adapter)->open());
    assert((*adapter)->socket_stats(Lane::data).no_fragment);
  }
}
