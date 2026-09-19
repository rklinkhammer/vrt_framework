#pragma once
#include <vita/profiles/iq/source.hpp>
#include <vita/runtime/context/receiver.hpp>
#include <vita/runtime/execution/budget.hpp>
#include <vita/runtime/transaction/controller.hpp>
#include <vita/runtime/transaction/trace.hpp>
#include <vita/runtime/transport/binding.hpp>
namespace vita {
inline std::atomic<std::uint64_t> runtime_identity_source{1};
struct ExternalPools {
  memory::ExternalPool header, payload, trailer, control, cancellation, rx_data,
      rx_control, rx_cancellation, emergency, large;
};
struct RuntimeConfig {
  std::optional<std::uint32_t> oui;
  runtime::timing::ClockBinding clock;
  runtime::timing::TimingCapabilities timing;
  std::size_t memory_limit = runtime::framework_budget;
  bool isolated_lab = false;
  std::size_t worker_stack_bytes=0;
  runtime::transport::TransportFactory transport{};
};
enum class ControlleeKind { iq_source, virtual_register };
struct StreamConfig {
  std::uint32_t sid = 0, controller_id = 0, controllee_id = 0;
  std::uint64_t controller_peer = 1, controllee_peer = 2;
  profiles::iq::SampleFormat format = profiles::iq::SampleFormat::iq16;
  std::uint64_t sample_rate = 1'000'000;
  std::size_t ip_mtu = 1500;
  bool ipv6 = false, trailer = false;
  std::optional<std::uint16_t> trailer_packet_class;
  profiles::iq::SourceProvider source = profiles::iq::default_source();
  runtime::context::ReceiverBinding receiver{};
  ControlleeKind kind=ControlleeKind::iq_source;
  runtime::transaction::TraceBinding trace{};
};
struct CommandOptions {
  bool partial = true, allow_warning = false, allow_error = false,
       validation = true, execution = true, state = true, details = true,
       nack = false, dry_run = false;
  unsigned timing_mode = 0;
  std::optional<runtime::timing::ProtocolTime> execute_at;
  std::uint64_t timeout_ns = 1'000'000'000;
};
enum class LifecyclePhase {
  idle,
  quiescing,
  reinitializing,
  starting,
  running,
  stopped,
  quarantined,
  failed
};
enum class StopMode { graceful, immediate };
struct LifecycleStatus {
  LifecyclePhase phase = LifecyclePhase::idle;
  std::size_t effects = 0, io = 0, capabilities = 0;
  bool backend_quiescent = false;
  ErrorCode error = ErrorCode::invalid_state;
};
struct LifecycleCompletion {
  void *context = nullptr;
  void (*callback)(void *, const LifecycleStatus &) noexcept = nullptr;
};
struct ClockReplacement {
  runtime::timing::ClockBinding binding;
  runtime::timing::MonoTime capture;
  runtime::timing::ProtocolTime time;
  std::uint64_t uncertainty_ps = 0;
};
struct RecoveryConfig {
  std::uint32_t new_sid = 0;
  runtime::StateSnapshot confirmed_state;
  bool peer_ready = false;

  void *reinitialize_context = nullptr;
  // true is explicit known-state/physical-quiescence confirmation; false
  // remains pending.
  Result<bool> (*reinitialize)(
      void *, const runtime::StateSnapshot &) noexcept = nullptr;
  std::optional<ClockReplacement> clock;
};
enum class WaitEvidence {
  execution,
  state,
  validation,
  cancellation_execution,
  cancellation_state
};
enum class WaitStatus {
  evidence_received,
  transaction_timeout,
  wait_budget_expired
};
struct WaitResult {
  WaitStatus status;
  runtime::transaction::Observation observation;
};
enum class SourceStatus {
  recovering,
  quiescing,
  configured,
  running,
  stopped,
  clock_unavailable,
  context_unavailable,
  faulted,
  temporal_association
};
struct StreamMetrics {
  std::uint64_t packets = 0, samples = 0, skipped_packets = 0,
                skipped_samples = 0, receive_drops = 0, send_failures = 0;
};
struct TransactionHandle {
  runtime::transaction::ControllerHandle controller;
  std::size_t stream = 0;
  std::uint64_t runtime_id = 0;
};
inline Result<std::size_t> packet_samples(const StreamConfig &config) noexcept {
  if (config.sample_rate < 1 || config.sample_rate > 100'000'000 ||
      config.ip_mtu < 64)
    return std::unexpected(Error{ErrorCode::invalid_argument});
  const auto overhead =
      (config.ipv6 ? 48u : 28u) + 28u + (config.trailer ? 4u : 0u);
  if (config.ip_mtu <= overhead)
    return std::unexpected(Error{ErrorCode::short_output});
  const auto pair = profiles::iq::bytes_per_pair(config.format);
  if (!pair)
    return std::unexpected(Error{ErrorCode::invalid_argument});
  auto count =
      std::min<std::size_t>({256, static_cast<std::size_t>(config.sample_rate),
                             (config.ip_mtu - overhead) / pair});
  if (!count)
    return std::unexpected(Error{ErrorCode::short_output});
  return count;
}
} // namespace vita
