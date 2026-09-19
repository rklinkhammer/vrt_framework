#include <cassert>
#include <chrono>
#include <thread>
#include <vita/adapters/posix_udp/udp.hpp>
using namespace vita;
using namespace vita::adapters::posix_udp;
int main() {
  for (auto family : {Family::ipv4, Family::ipv6}) {
    SocketConfig config;
    config.bind = Address::loopback(family);
    auto a = Socket::open(config), b = Socket::open(config);
    assert(a && b);
    std::array<std::byte, 4> bytes{std::byte{1}, std::byte{2}, std::byte{3},
                                   std::byte{4}};
    std::array<Bytes, 2> pieces{Bytes(bytes).first(1), Bytes(bytes).subspan(1)};
    auto sent = a->send(b->local_address(), pieces);
    assert(sent && *sent == 4);
    std::array<std::byte, 4> received{};
    auto rx = b->receive(received);
    for (int attempt = 0; !rx && rx.error().retryable && attempt < 100;
         ++attempt) {
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
      rx = b->receive(received);
    }
    assert(rx && rx->bytes == 4 && !rx->truncated && received == bytes &&
           rx->source == a->local_address());
  }
}
