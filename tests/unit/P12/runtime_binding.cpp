#include <cassert>
#include <thread>
#include <vita/adapters/posix_udp/factory.hpp>
#include <vita/profiles/iq/lab.hpp>
#include <vita/runtime/public/runtime.hpp>
using namespace vita;
using namespace vita::adapters::posix_udp;
int main() {
  for (auto family : {Family::ipv4, Family::ipv6}) {
    SocketConfig sc;
    sc.bind = Address::loopback(family);
    auto external_controller = Socket::open(sc),
         external_controllee = Socket::open(sc);
    assert(external_controller && external_controllee);
    FactoryConfig<32, 16> setup;
    for (auto &socket : setup.config.sockets)
      socket.bind = Address::loopback(family);
    PeerBinding c;
    c.local_source = {1, 1};
    c.remote_source = {2, 1};
    c.remote.fill(external_controllee->local_address());
    assert(setup.peers.push_back(c));
    PeerBinding d;
    d.local_source = {2, 1};
    d.remote_source = {1, 1};
    d.remote.fill(external_controller->local_address());
    assert(setup.peers.push_back(d));
    auto config = profiles::iq::lab::config(0xabcdef);
    auto pools = profiles::iq::lab::pools();
    assert(config && pools);
    config->transport = factory(setup);
    auto made =
        VitaRuntime<1, 4, 32, 65536>::create(*config, std::move(*pools));
    assert(made && setup.instance);
    auto &r = **made;
    StreamConfig stream;
    stream.sid = 1;
    stream.controller_id = 2;
    stream.controllee_id = 3;
    stream.ipv6 = family == Family::ipv6;
    auto device = r.add_controllee(stream);
    assert(device);
    auto controller = r.add_controller(*device);
    assert(controller);
    assert(r.observe_pps({0}, {1000, 0}) && device->start());
    std::uint64_t mono = 0;
    std::array<std::byte, 2048> buffer{};
    auto receive = [&](Socket &socket) {
      for (unsigned i = 0; i < 1000; ++i) {
        assert(r.progress({mono}));
        mono += 1000;
        auto packet = socket.receive(buffer);
        if (packet)
          return packet;
        assert(packet.error().retryable);
        std::this_thread::sleep_for(std::chrono::microseconds(100));
      }
      return Result<Received>{
          std::unexpected(Error{ErrorCode::capacity_exhausted})};
    };
    bool context = false, data = false;
    for (unsigned i = 0; i < 10 && (!context || !data); ++i) {
      auto packet = receive(*external_controller);
      assert(packet);
      auto decoded = codec::decode_packet(Bytes{buffer}.first(packet->bytes));
      assert(decoded);
      if (decoded->envelope.envelope.type == codec::PacketType::context)
        context = true;
      else if (codec::is_data(decoded->envelope.envelope.type)) {
        assert(decoded->envelope.payload.size() == 1024);
        assert(codec::detail::load32(decoded->envelope.payload, 0) ==
               0x40000000);
        data = true;
      }
    }
    assert(context && data);
    CommandOptions options;
    options.validation = false;
    options.state = false;
    options.execution = true;
    auto transaction =
        controller->set_sample_rate(*Hertz::from_integer(2'000'000), options);
    assert(transaction);
    auto command_bytes = receive(*external_controllee);
    assert(command_bytes);
    auto command =
        codec::decode_packet(Bytes{buffer}.first(command_bytes->bytes));
    assert(command && codec::is_command(command->envelope.envelope.type) &&
           !command->envelope.envelope.ack);
    auto cam = runtime::transaction::Cam::parse(
        command->envelope.envelope,
        runtime::transaction::Profile::iq_generator_v1);
    assert(cam);
    runtime::transaction::AckRecord response;
    response.request = command->envelope.envelope;
    response.cam = *cam;
    response.kind = runtime::transaction::AckKind::execution;
    response.scheduled_or_executed = true;
    std::array<std::byte, 256> reply{};
    auto encoded = runtime::transaction::encode_response(response, reply, 0);
    assert(encoded);
    std::array<Bytes, 1> parts{Bytes{reply}.first(*encoded)};
    assert(external_controllee->send(
        setup.instance->local_address(Lane::control), parts));
    bool confirmed = false;
    for (unsigned i = 0; i < 1000 && !confirmed; ++i) {
      assert(r.progress({mono}));
      mono += 1000;
      auto observed = controller->observation(*transaction);
      assert(observed);
      confirmed = observed->confirms_execution;
      if (!confirmed)
        std::this_thread::sleep_for(std::chrono::microseconds(100));
    }
    assert(confirmed);
    assert(controller->release(*transaction));
    // The setup observer retains the adapter after Runtime dies; it is
    // detached.
    made->reset();
    assert(!setup.instance->open());
    auto after = setup.instance->progress_next();
    assert(!after || !*after);
  }
}
