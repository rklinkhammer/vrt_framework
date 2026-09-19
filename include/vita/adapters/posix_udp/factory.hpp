#pragma once
#include <vita/adapters/posix_udp/udp.hpp>
namespace vita::adapters::posix_udp {
template <std::size_t Slots = 320, std::size_t Peers = 64>
struct FactoryConfig {
  Config config;
  FixedVector<PeerBinding, Peers> peers;
  // Setup observer only; retaining a shared owner is optional for metrics.
  std::shared_ptr<Udp<Slots, 128, 128, Peers>> instance;
};
template <std::size_t Slots = 320, std::size_t Peers = 64>
runtime::transport::TransportFactory
factory(FactoryConfig<Slots, Peers> &config) noexcept {
  using Adapter = Udp<Slots, 128, 128, Peers>;
  runtime::transport::TransportFactory result;
  result.context = &config;
  result.required_bytes = Adapter::metadata_bytes() + 128;
  result.slot_capacity = Slots;
  result.capabilities = config.config.capabilities;
  result.create = [](void *context,
                     runtime::transport::HostBindings host) noexcept
      -> Result<runtime::transport::TransportBinding> {
    auto &setup = *static_cast<FactoryConfig<Slots, Peers> *>(context);
    auto made = Adapter::create(
        setup.config, {host.rx_data, host.rx_control, host.rx_cancellation},
        host.admission, host.routes, host.counters);
    if (!made)
      return std::unexpected(made.error());
    for (std::size_t i = 0; i < setup.peers.size(); ++i) {
      auto added = (*made)->add_peer(setup.peers[i]);
      if (!added)
        return std::unexpected(added.error());
    }
    std::shared_ptr<Adapter> owner(std::move(*made));
    runtime::transport::TransportBinding binding;
    binding.owner = owner;
    binding.context = owner.get();
    binding.metadata_bytes = Adapter::metadata_bytes() + 128;
    binding.slot_capacity = Slots;
    binding.capabilities = owner->capabilities();
    binding.send = [](void *p, TxSubmission &&tx) noexcept {
      return static_cast<Adapter *>(p)->try_send(std::move(tx));
    };
    binding.progress = [](void *p) noexcept {
      return static_cast<Adapter *>(p)->progress_next();
    };
    binding.pending = [](void *p, TxToken token) noexcept {
      return static_cast<Adapter *>(p)->outstanding(token);
    };
    binding.close_admission = [](void *p) noexcept {
      static_cast<Adapter *>(p)->close();
    };
    binding.associate =
        [](void *p, std::span<const runtime::transport::Association> batch,
           bool commit) noexcept {
          return static_cast<Adapter *>(p)->associate(batch, commit);
        };
    binding.begin_cycle = [](void *p) noexcept {
      static_cast<Adapter *>(p)->begin_cycle();
    };
    binding.detach = [](void *p) noexcept {
      static_cast<Adapter *>(p)->detach();
    };
    setup.instance = std::move(owner);
    return binding;
  };
  return result;
}
} // namespace vita::adapters::posix_udp
