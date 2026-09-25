#pragma once
#include <vita/profiles/iq/Sdr.hpp>
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
enum class EndpointRole { combined, controller_only, controllee_only };
struct DeviceBackendBinding {
  runtime::transaction::Backend backend{};
  std::shared_ptr<void> owner{};
  std::size_t storage_bytes=0;
  void (*progress)(void*, const runtime::transaction::OperationContext&) noexcept=nullptr;
  bool enabled() const noexcept{return backend.context||backend.validate||backend.validate_plan||backend.begin||backend.simulate||backend.disarm||backend.quiescence||backend.commit||bool(owner)||storage_bytes||progress;}
  bool valid() const noexcept{return !enabled() || (owner&&storage_bytes&&backend.context&&backend.validate&&backend.begin&&backend.quiescence);}
};
enum class QueryField : std::uint8_t { reference_point, sample_rate, state_event, payload_format, center_frequency, bandwidth, gain, streaming };
class QuerySelection {
  std::uint8_t mask_=0;
  bool valid_=true;
public:
  QuerySelection(std::initializer_list<QueryField> fields) noexcept {for(auto field:fields){auto index=static_cast<unsigned>(field);if(index<8)mask_|=1u<<index;else valid_=false;}}
  std::uint8_t mask() const noexcept{return mask_;}
  bool valid() const noexcept{return valid_;}
};
enum class ControlleeKind { iq_source, virtual_register };
struct StreamConfig {
  std::uint32_t sid = 0, controller_id = 0, controllee_id = 0;
  std::uint64_t controller_peer = 1, controllee_peer = 2;
  profiles::iq::SampleFormat format = profiles::iq::SampleFormat::iq16;
  std::uint64_t sample_rate = 1'000'000;
  std::size_t ip_mtu = 1500;
  std::size_t maximum_samples_per_packet = 256;
  std::size_t burst_pairs = profiles::iq::sdr_burst_pairs;
  bool ipv6 = false, trailer = false;
  std::optional<std::uint16_t> trailer_packet_class;
  profiles::iq::SourceProvider source = profiles::iq::default_source();
  runtime::context::ReceiverBinding receiver{};
  ControlleeKind kind=ControlleeKind::iq_source;
  runtime::transaction::TraceBinding trace{};
  profiles::iq::Profile profile=profiles::iq::Profile::generator_v1;
  std::uint64_t center_frequency=100'000'000;
  std::uint64_t bandwidth=100'000;
  GainStages gain{};
  profiles::iq::SdrCapabilities sdr_capabilities{};
  EndpointRole role=EndpointRole::combined;
  DeviceBackendBinding device{};
  std::uint64_t association_generation=1;
};
struct RemoteTargetConfig {
  std::uint32_t sid=0,controller_id=0,controllee_id=0;
  std::uint64_t controller_peer=1,controllee_peer=2;
  profiles::iq::Profile profile=profiles::iq::Profile::generator_v1;
  profiles::iq::SampleFormat format=profiles::iq::SampleFormat::iq16;
  runtime::context::ReceiverBinding receiver{};
  std::uint64_t sample_rate=100'000;
  std::uint64_t association_generation=1;
};
struct CommandOptions {
  bool partial = true, allow_warning = false, allow_error = false,
       validation = true, execution = true, state = true, details = true,
       nack = false, dry_run = false;
  unsigned timing_mode = 0;
  std::optional<runtime::timing::ProtocolTime> execute_at;
  std::uint64_t timeout_ns = 1'000'000'000;
};
struct SdrRadioSettings {
  Hertz bandwidth = *Hertz::from_integer(100'000);
  Hertz center_frequency = *Hertz::from_integer(100'000'000);
  GainStages gain{};
  Hertz sample_rate = *Hertz::from_integer(1'000'000);
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
  config.ip_mtu < 64 || !config.maximum_samples_per_packet ||
      config.maximum_samples_per_packet > 1024 ||
      (config.profile != profiles::iq::Profile::sdr_radio &&
       config.maximum_samples_per_packet > 256))
    return std::unexpected(Error{ErrorCode::invalid_argument});
  const auto overhead =
      (config.ipv6 ? 48u : 28u) + 28u + (config.trailer ? 4u : 0u);
  if (config.ip_mtu <= overhead)
    return std::unexpected(Error{ErrorCode::short_output});
  const auto pair = profiles::iq::bytes_per_pair(config.format);
  if (!pair)
    return std::unexpected(Error{ErrorCode::invalid_argument});
  if(config.profile==profiles::iq::Profile::sdr_radio){
    if(!config.burst_pairs||config.burst_pairs>profiles::iq::sdr_burst_pairs)
      return std::unexpected(Error{ErrorCode::invalid_argument});
    if(config.maximum_samples_per_packet>(config.ip_mtu-overhead)/pair)
      return std::unexpected(Error{ErrorCode::short_output});
    return config.maximum_samples_per_packet;
  }
  auto count =
      std::min<std::size_t>({config.maximum_samples_per_packet,
                 static_cast<std::size_t>(config.sample_rate),
                             (config.ip_mtu - overhead) / pair});
  if (!count)
    return std::unexpected(Error{ErrorCode::short_output});
  return count;
}
} // namespace vita
