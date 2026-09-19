#pragma once
#include <memory>
#include <vita/runtime/transport/types.hpp>
namespace vita::runtime::transport {
struct Association {
  PeerSession local, remote;
  std::uint32_t sid = 0;
};
struct HostBindings {
  AdmissionPool &admission;
  RouteRegistry<128> &routes;
  CounterRegistry<128> &counters;
  memory::ExternalPool rx_data, rx_control, rx_cancellation;
};
// Setup-owned adapter lifetime; callbacks run in Runtime's serialized domain.
// A send rejection returns ownership. Acceptance promises one deferred terminal
// completion; completion is distinct from physical quiescence and delivery.
struct TransportBinding {
  std::shared_ptr<void> owner;
  void *context = nullptr;
  std::size_t metadata_bytes = 0, slot_capacity = 0;
  Capabilities capabilities{};
  std::expected<TxToken, RejectedSubmission> (*send)(
      void *, TxSubmission &&) noexcept = nullptr;
  Result<bool> (*progress)(void *) noexcept = nullptr;
  bool (*pending)(void *, TxToken) noexcept = nullptr;
  void (*close_admission)(void *) noexcept = nullptr;
  Result<void> (*quiescence)(void *, TxToken) noexcept = nullptr;
  // false preflights the whole batch; true commits it atomically.
  Result<void> (*associate)(void *, std::span<const Association>,
                            bool) noexcept = nullptr;
  // Irrevocably removes all borrowed HostBindings before Runtime destruction.
  void (*detach)(void *) noexcept = nullptr;
  // Once per outer Runtime progress, before any adapter service calls.
  void (*begin_cycle)(void *) noexcept = nullptr;
  bool valid() const noexcept {
    return owner && context && metadata_bytes && slot_capacity && send &&
           progress && pending && close_admission;
  }
  auto try_send(TxSubmission &&value) noexcept {
    return send(context, std::move(value));
  }
  Result<bool> progress_next() noexcept { return progress(context); }
  bool outstanding(TxToken token) const noexcept {
    return pending(context, token);
  }
  void close() noexcept { close_admission(context); }
  Result<void> prove_quiescent(TxToken token) noexcept {
    if (!quiescence)
      return std::unexpected(Error{ErrorCode::unsupported_capability});
    return quiescence(context, token);
  }
};
struct TransportFactory {
  void *context = nullptr;
  // Includes adapter object, fixed tables, backing and shared-owner overhead.
  // Declared before creation so Runtime can reject overbudget configurations.
  std::size_t required_bytes = 0, slot_capacity = 0;
  Capabilities capabilities{};
  // An error return must already detach/dispose every object holding
  // HostBindings. Even a rejected successful return must provide a valid detach
  // callback when another owner can retain borrowed host references.
  Result<TransportBinding> (*create)(void *, HostBindings) noexcept = nullptr;
};
} // namespace vita::runtime::transport
