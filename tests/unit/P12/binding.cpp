#include <cassert>
#include <vita/profiles/iq/lab.hpp>
#include <vita/runtime/public/runtime.hpp>
#include <vita/runtime/transport/framing.hpp>
using namespace vita;
namespace tx = vita::runtime::transport;
using Adapter = adapters::loopback::Loopback<16, 128, 128>;
struct FactoryState {
  std::size_t creates = 0, checks = 0, commits = 0, detached = 0;
  bool wrong_metadata = false, reject_association = false;
};
struct Owner {
  Adapter adapter;
  FactoryState *state;
  Owner(tx::HostBindings h, FactoryState *s)
      : adapter(h.rx_data, h.rx_control, h.rx_cancellation, h.admission,
                h.routes, h.counters),
        state(s) {}
};
constexpr auto adapter_bytes =
    sizeof(Owner) + 16 * runtime::QuiescenceGuard::metadata_bytes() + 128;
tx::TransportFactory factory(FactoryState &state) {
  return {
      &state,
      adapter_bytes,
      16,
      {},
      [](void *p,
         tx::HostBindings host) noexcept -> Result<tx::TransportBinding> {
        auto &s = *static_cast<FactoryState *>(p);
        ++s.creates;
        auto owner = std::make_shared<Owner>(std::move(host), &s);
        return tx::TransportBinding{
            owner,
            owner.get(),
            adapter_bytes + (s.wrong_metadata ? 1 : 0),
            16,
            {},
            [](void *p, tx::TxSubmission &&x) noexcept {
              return static_cast<Owner *>(p)->adapter.try_send(std::move(x));
            },
            [](void *p) noexcept {
              return static_cast<Owner *>(p)->adapter.progress_next();
            },
            [](void *p, tx::TxToken t) noexcept {
              return static_cast<Owner *>(p)->adapter.outstanding(t);
            },
            [](void *p) noexcept { static_cast<Owner *>(p)->adapter.close(); },
            [](void *p, tx::TxToken t) noexcept {
              return static_cast<Owner *>(p)->adapter.prove_quiescent(t);
            },
            [](void *p, std::span<const tx::Association> batch,
               bool commit) noexcept -> Result<void> {
              auto &s = *static_cast<Owner *>(p)->state;
              assert(batch.size() == 2 && batch[0].sid == batch[1].sid);
              if (s.reject_association)
                return std::unexpected(Error{ErrorCode::capacity_exhausted});
              commit ? ++s.commits : ++s.checks;
              return {};
            },
            [](void *p) noexcept {
              auto &owner = *static_cast<Owner *>(p);
              ++owner.state->detached;
              owner.adapter.close();
            }};
      }};
}
int main() {
  auto config = profiles::iq::lab::config(0xabcdef);
  auto pools = profiles::iq::lab::pools();
  assert(config && pools);
  FactoryState state;
  config->transport = factory(state);
  {
    auto excessive = *config;
    excessive.transport.required_bytes = SIZE_MAX;
    auto failed = VitaRuntime<1, 4, 32, 65536>::create(excessive, *pools);
    assert(!failed && state.creates == 0);
  }
  {
    state.wrong_metadata = true;
    auto failed = VitaRuntime<1, 4, 32, 65536>::create(*config, *pools);
    assert(!failed && state.detached == 1);
    state.wrong_metadata = false;
  }
  auto made = VitaRuntime<1, 4, 32, 65536>::create(*config, *pools);
  assert(made);
  auto &r = **made;
  StreamConfig stream;
  stream.sid = 1;
  stream.controller_id = 2;
  stream.controllee_id = 3;
  const auto budget = r.budget().charged_bytes();
  state.reject_association = true;
  assert(!r.add_controllee(stream) && r.budget().charged_bytes() == budget);
  state.reject_association = false;
  auto endpoint = r.add_controllee(stream);
  assert(endpoint && state.commits == 1);
  assert(r.observe_pps({0}, {1000, 0}) && endpoint->start() && r.progress({0}));
  RecoveryConfig recovery;
  recovery.new_sid = 2;
  recovery.peer_ready = true;
  recovery.confirmed_state = endpoint->confirmed_state();
  recovery.confirmed_state.fields[0].value = std::uint32_t{2};
  assert(endpoint->recover(recovery) && r.progress({1000}));
  assert(endpoint->sid() == 2 && state.commits == 2);
  // Fully split prologue still validates without gathering the IQ payload.
  auto h = pools->header.acquire({28}),
       payload = pools->payload.acquire({1024}),
       trailer = pools->trailer.acquire({4});
  assert(h && payload && trailer);
  codec::Envelope e;
  e.type = codec::PacketType::signal;
  e.stream_id = 3;
  e.class_id = codec::ClassId{0xabcdef, 1, 1};
  e.timestamp = {codec::Tsi::gps, codec::Tsf::picoseconds, 5, 0};
  e.trailer = true;
  auto hw = h->writable_bytes();
  assert(hw && codec::encode_prologue(e, 1024, *hw));
  assert(h->set_size(28) && payload->set_size(1024) && trailer->set_size(4));
  memory::TxStorage storage;
  assert(storage.append(std::move(*h), 0, 28) &&
         storage.append(std::move(*payload), 0, 1024) &&
         storage.append(std::move(*trailer), 0, 4));
  auto parsed = tx::inspect(storage);
  assert(parsed && parsed->packet_bytes == 1056 &&
         parsed->payload_offset == 28 && parsed->payload_bytes == 1024);
}
