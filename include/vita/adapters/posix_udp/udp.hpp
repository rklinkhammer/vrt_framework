#pragma once
#include <cstring>
#include <memory>
#include <optional>
#include <vita/adapters/posix_udp/socket.hpp>
#include <vita/runtime/transport/binding.hpp>
#include <vita/runtime/transport/framing.hpp>
namespace vita::adapters::posix_udp {
using runtime::transport::Capabilities;
using runtime::transport::RejectedSubmission;
using runtime::transport::TxSubmission;
using runtime::transport::TxToken;
enum class Lane : std::size_t { data = 0, control = 1, cancellation = 2 };
struct Config {
  std::array<SocketConfig, 3> sockets{};
  Capabilities capabilities{65507, 3, 1, true, true, false, 1, 1, 256};
};
struct PeerBinding {
  runtime::PeerSession local_source, remote_source;
  std::array<Address, 3> remote;
};
struct Metrics {
  std::uint64_t tx_accepted = 0, tx_completed = 0, tx_failed = 0, tx_retry = 0,
                tx_data_attempts = 0, tx_control_attempts = 0;
  std::uint64_t rx_delivered = 0, rx_malformed = 0, rx_truncated = 0,
                rx_unauthorized = 0, rx_pool_drop = 0, rx_lane_drop = 0,
                rx_mtu_drop = 0, socket_errors = 0;
  int last_errno = 0;
};
// Serialized, bounded transport. Three explicitly configured ports isolate the
// socket queues; they are a deployment binding, not additional VITA fields.
template <std::size_t Slots = 320, std::size_t Routes = 128,
          std::size_t Counters = 128, std::size_t Peers = 64>
class Udp {
  struct Slot {
    std::optional<TxSubmission> submission;
    runtime::AdmissionBundle credits;
    Address destination;
    std::uint64_t generation = 1, sequence = 0;
    bool retired = false;
  };
  std::array<Slot, Slots> slots_{};
  std::array<Socket, 3> sockets_;
  std::array<memory::ExternalPool, 3> pools_;
  runtime::AdmissionPool &admission_;
  runtime::RouteRegistry<Routes> &routes_;
  runtime::CounterRegistry<Counters> &counters_;
  Config config_;
  std::array<std::optional<PeerBinding>, Peers> peers_{};
  std::uint64_t next_sequence_ = 1;
  std::size_t cursor_ = 0, data_attempts_ = 0, control_attempts_ = 0;
  bool open_ = true, progressing_ = false, aborting_ = false, detached_ = false;
  Metrics metrics_;
  Udp(Config config, std::array<Socket, 3> sockets,
      std::array<memory::ExternalPool, 3> pools,
      runtime::AdmissionPool &admission, runtime::RouteRegistry<Routes> &routes,
      runtime::CounterRegistry<Counters> &counters)
      : sockets_(std::move(sockets)), pools_(std::move(pools)),
        admission_(admission), routes_(routes), counters_(counters),
        config_(config) {}
  static Lane lane(codec::PacketType type, bool cancel) noexcept {
    return codec::is_data(type) ? Lane::data
           : cancel             ? Lane::cancellation
                                : Lane::control;
  }
  std::size_t maximum(std::size_t lane) const noexcept {
    const auto &socket = config_.sockets[lane];
    return std::min(config_.capabilities.max_packet_bytes,
                    socket.ip_mtu -
                        (socket.bind.family == Family::ipv4 ? 28u : 48u));
  }
  std::pair<std::size_t, std::size_t> range(std::size_t lane) const noexcept {
    const auto data_end = Slots - config_.capabilities.reserved_control_slots -
                          config_.capabilities.reserved_cancellation_slots;
    const auto control_end =
        Slots - config_.capabilities.reserved_cancellation_slots;
    return lane == 0   ? std::pair{std::size_t{0}, data_end}
           : lane == 1 ? std::pair{data_end, control_end}
                       : std::pair{control_end, Slots};
  }
  void release(Slot &slot) noexcept {
    slot.submission.reset();
    slot.credits.reset();
    if (slot.generation == UINT64_MAX)
      slot.retired = true;
    else
      ++slot.generation;
  }
  Result<bool> transmit(std::size_t lane) noexcept {
    auto [first, last] = range(lane);
    std::size_t selected = Slots;
    std::uint64_t sequence = UINT64_MAX;
    for (auto i = first; i < last; ++i)
      if (slots_[i].submission && slots_[i].sequence < sequence) {
        selected = i;
        sequence = slots_[i].sequence;
      }
    if (selected == Slots)
      return false;
    if (lane == 0) {
      if (data_attempts_ == 64)
        return false;
      ++data_attempts_;
      ++metrics_.tx_data_attempts;
    } else {
      if (control_attempts_ == 32)
        return false;
      ++control_attempts_;
      ++metrics_.tx_control_attempts;
    }
    auto &slot = slots_[selected];
    auto &submission = *slot.submission;
    std::array<Bytes, 3> parts;
    for (std::size_t i = 0; i < submission.storage.segment_count(); ++i) {
      auto bytes = submission.storage.segment(i);
      if (!bytes)
        return std::unexpected(bytes.error());
      parts[i] = *bytes;
    }
    Result<std::size_t> sent =
        aborting_
            ? Result<std::size_t>(
                  std::unexpected(Error{ErrorCode::invalid_state}))
            : sockets_[lane].send(
                  slot.destination,
                  std::span(parts).first(submission.storage.segment_count()));
    if (!sent && sent.error().retryable) {
      ++metrics_.tx_retry;
      return false;
    }
    runtime::CompletionResult result;
    result.value = sent ? *sent : 0;
    if (!sent) {
      result.status = runtime::CompletionStatus::failed;
      result.error = sent.error();
      ++metrics_.tx_failed;
      metrics_.last_errno = sent.error().native_error;
    } else
      ++metrics_.tx_completed;
    const bool published = submission.completion.publish(result);
    release(slot);
    if (!published)
      return std::unexpected(Error{ErrorCode::invalid_state});
    return true;
  }
  void discard(std::size_t lane) noexcept {
    std::array<std::byte, 4> trash{};
    auto ignored = sockets_[lane].receive(trash);
    (void)ignored;
  }
  Result<bool> receive(std::size_t source_lane) noexcept {
    if (!open_)
      return false;
    std::array<std::byte, 4> header;
    auto peek = sockets_[source_lane].receive(header, true);
    if (!peek) {
      if (peek.error().retryable)
        return false;
      ++metrics_.socket_errors;
      metrics_.last_errno = peek.error().native_error;
      return std::unexpected(peek.error());
    }
    bool authorized = false;
    for (const auto &peer : peers_)
      if (peer && peer->remote[source_lane] == peek->source) {
        authorized = true;
        break;
      }
    if (!authorized) {
      discard(source_lane);
      ++metrics_.rx_unauthorized;
      return true;
    }
    if (peek->bytes != 4) {
      discard(source_lane);
      ++metrics_.rx_malformed;
      return true;
    }
    const auto word = codec::detail::load32(header, 0);
    const auto type = static_cast<codec::PacketType>(word >> 28);
    const bool cancellation = codec::is_command(type) && (word & (1u << 24));
    if (static_cast<std::size_t>(lane(type, cancellation)) != source_lane) {
      discard(source_lane);
      ++metrics_.rx_lane_drop;
      return true;
    }
    const std::size_t declared = (word & 65535u) * 4u;
    if (declared < 4) {
      discard(source_lane);
      ++metrics_.rx_malformed;
      return true;
    }
    if (declared > maximum(source_lane)) {
      discard(source_lane);
      ++metrics_.rx_mtu_drop;
      return true;
    }
    auto buffer = pools_[source_lane].acquire({declared});
    if (!buffer) {
      discard(source_lane);
      ++metrics_.rx_pool_drop;
      return true;
    }
    auto writable = buffer->writable_bytes();
    if (!writable) {
      discard(source_lane);
      ++metrics_.rx_pool_drop;
      return true;
    }
    auto received = sockets_[source_lane].receive(writable->first(declared));
    if (!received) {
      if (received.error().retryable)
        return false;
      return std::unexpected(received.error());
    }
    if (received->source != peek->source) {
      ++metrics_.rx_unauthorized;
      return true;
    }
    if (received->truncated) {
      ++metrics_.rx_truncated;
      return true;
    }
    if (received->bytes != declared) {
      ++metrics_.rx_malformed;
      return true;
    }
    auto sized = buffer->set_size(declared);
    if (!sized)
      return std::unexpected(sized.error());
    auto wire = buffer->bytes();
    if (!wire)
      return std::unexpected(wire.error());
    auto envelope = codec::decode_envelope(*wire);
    if (!envelope) {
      ++metrics_.rx_malformed;
      return true;
    }
    const runtime::Route *route = nullptr;
    for (const auto &peer : peers_)
      if (peer && peer->remote[source_lane] == received->source) {
        auto candidate =
            routes_.lookup(peer->remote_source, envelope->envelope);
        if (candidate) {
          if (route && route != *candidate) {
            ++metrics_.rx_unauthorized;
            return true;
          }
          route = *candidate;
        }
      }
    if (!route) {
      ++metrics_.rx_unauthorized;
      return true;
    }
    if(route->before_decode)route->before_decode(route->context,envelope->envelope);
    if (envelope->payload.size() < route->minimum_payload_bytes ||
        envelope->payload.size() > route->maximum_payload_bytes) {
      ++metrics_.rx_malformed;
      return true;
    }
    if (codec::is_extension(envelope->envelope.type)) {
      auto valid = route->validate_extension(route->context, *envelope);
      if (!valid) {
        ++metrics_.rx_malformed;
        return true;
      }
    }
    codec::DecodeOptions options;
    if (route->request_context)
      options.request =
          route->request_context(route->context, envelope->envelope);
    auto packet = codec::decode_packet(*wire, options);
    if (!packet) {
      ++metrics_.rx_malformed;
      return true;
    }
    memory::RxEnvelope rx;
    auto lease = rx.add_buffer(std::move(*buffer));
    if (!lease)
      return std::unexpected(lease.error());
    auto part = rx.set_prologue({*lease, 0, packet->envelope.payload_offset});
    if (!part)
      return std::unexpected(part.error());
    if (!packet->envelope.payload.empty()) {
      part = rx.append_payload({*lease, packet->envelope.payload_offset,
                                packet->envelope.payload.size()});
      if (!part)
        return std::unexpected(part.error());
    }
    if (packet->envelope.trailer) {
      part = rx.set_trailer(
          {*lease,
           packet->envelope.payload_offset + packet->envelope.payload.size(),
           4});
      if (!part)
        return std::unexpected(part.error());
    }
    ++metrics_.rx_delivered;
    route->receive(route->context, *packet, rx);
    return true;
  }

public:
  ~Udp() { detach(); }
  void detach() noexcept {
    if (detached_)
      return;
    open_ = false;
    aborting_ = true;
    detached_ = true;
    for (auto &slot : slots_)
      if (slot.submission) {
        runtime::CompletionResult result;
        result.status = runtime::CompletionStatus::failed;
        result.error = Error{ErrorCode::invalid_state};
        slot.submission->completion.publish(result);
        release(slot);
      }
    for (auto &socket : sockets_)
      socket.close();
  }

  Udp(const Udp &) = delete;
  Udp &operator=(const Udp &) = delete;
  static constexpr std::size_t metadata_bytes() noexcept { return sizeof(Udp); }
  static Result<std::unique_ptr<Udp>>
  create(Config config, std::array<memory::ExternalPool, 3> pools,
         runtime::AdmissionPool &admission,
         runtime::RouteRegistry<Routes> &routes,
         runtime::CounterRegistry<Counters> &counters) {
    const auto &cap = config.capabilities;
    if (!cap.reserved_control_slots || !cap.reserved_cancellation_slots ||
        cap.reserved_control_slots >= Slots ||
        cap.reserved_cancellation_slots >= Slots - cap.reserved_control_slots ||
        !cap.per_stream_data_limit || cap.per_stream_data_limit > 256 ||
        (!cap.max_tx_segments || cap.max_tx_segments > 3) ||
        !cap.max_rx_fragments || cap.max_rx_fragments > 16 ||
        cap.max_packet_bytes < 4 || cap.max_packet_bytes > 65507 ||
        !cap.cpu_required || !cap.copies_to_receive_pool ||
        cap.completion_is_delivery)
      return std::unexpected(Error{ErrorCode::invalid_argument});
    for (std::size_t i = 0; i < 3; ++i) {
      if (!pools[i].supports({4}))
        return std::unexpected(Error{ErrorCode::no_cpu_access});
      for (std::size_t j = 0; j < i; ++j)
        if (pools[i].shares_provider_with(pools[j]))
          return std::unexpected(Error{ErrorCode::invalid_argument});
    }
    std::array<Socket, 3> sockets;
    for (std::size_t i = 0; i < 3; ++i) {
      auto socket = Socket::open(config.sockets[i]);
      if (!socket)
        return std::unexpected(socket.error());
      sockets[i] = std::move(*socket);
    }
    return std::unique_ptr<Udp>(new Udp(config, std::move(sockets),
                                        std::move(pools), admission, routes,
                                        counters));
  }
  const Address &local_address(Lane lane) const noexcept {
    return sockets_[static_cast<std::size_t>(lane)].local_address();
  }
  const SocketStats &socket_stats(Lane lane) const noexcept {
    return sockets_[static_cast<std::size_t>(lane)].stats();
  }
  const Metrics &metrics() const noexcept { return metrics_; }
  const Capabilities &capabilities() const noexcept {
    return config_.capabilities;
  }
  bool open() const noexcept { return open_; }
  void begin_cycle() noexcept {
    if (!progressing_ && !detached_)
      data_attempts_ = control_attempts_ = 0;
  }
  void close() noexcept { open_ = false; }
  void abort() noexcept {
    open_ = false;
    aborting_ = true;
  }
  Result<void> add_peer(PeerBinding peer) noexcept {
    if (!open_ || !peer.local_source.generation ||
        !peer.remote_source.generation)
      return std::unexpected(Error{ErrorCode::invalid_argument});
    for (std::size_t i = 0; i < 3; ++i)
      if (!peer.remote[i].canonical() || !peer.remote[i].port ||
          peer.remote[i].family != config_.sockets[i].bind.family)
        return std::unexpected(Error{ErrorCode::invalid_argument});
    for (const auto &existing : peers_)
      if (existing) {
        if (existing->local_source == peer.local_source)
          return std::unexpected(Error{ErrorCode::identity_conflict});
      }
    for (auto &slot : peers_)
      if (!slot) {
        slot = peer;
        return {};
      }
    return std::unexpected(Error{ErrorCode::capacity_exhausted});
  }
  Result<void> associate(std::span<const runtime::transport::Association> batch,
                         bool commit) noexcept {
    if (!open_)
      return std::unexpected(Error{ErrorCode::invalid_state});
    auto candidate = peers_;
    for (const auto &association : batch) {
      if (!association.sid || !association.local.generation ||
          !association.remote.generation)
        return std::unexpected(Error{ErrorCode::invalid_argument});
      const PeerBinding *prototype = nullptr;
      bool exists = false;
      for (const auto &peer : candidate)
        if (peer && peer->local_source.peer == association.local.peer &&
            peer->remote_source.peer == association.remote.peer) {
          prototype = &*peer;
          if (peer->local_source == association.local &&
              peer->remote_source == association.remote)
            exists = true;
        }
      if (exists)
        continue;
      if (!prototype)
        return std::unexpected(Error{ErrorCode::identity_conflict});
      auto added = *prototype;
      added.local_source = association.local;
      added.remote_source = association.remote;
      bool inserted = false;
      for (auto &peer : candidate)
        if (!peer) {
          peer = added;
          inserted = true;
          break;
        }
      if (!inserted)
        return std::unexpected(Error{ErrorCode::capacity_exhausted});
    }
    if (commit)
      peers_ = std::move(candidate);
    return {};
  }
  bool outstanding(TxToken token) const noexcept {
    return token.slot < Slots &&
           slots_[token.slot].generation == token.generation &&
           slots_[token.slot].submission.has_value();
  }
  std::size_t outstanding() const noexcept {
    std::size_t n = 0;
    for (const auto &slot : slots_)
      n += slot.submission.has_value();
    return n;
  }
  std::expected<TxToken, RejectedSubmission>
  try_send(TxSubmission &&submission) noexcept {
    auto reject =
        [&](Error error) -> std::expected<TxToken, RejectedSubmission> {
      return std::unexpected(RejectedSubmission{error, std::move(submission)});
    };
    if (!open_)
      return reject(Error{ErrorCode::invalid_state});
    if (submission.fault.synchronous_reject || submission.fault.lose ||
        submission.fault.duplicate || submission.fault.fail_completion ||
        submission.fault.hold_quiescence)
      return reject(Error{ErrorCode::unsupported_capability});
    if (!submission.completion.is_reserved() ||
        submission.storage.segment_count() >
            config_.capabilities.max_tx_segments)
      return reject(Error{ErrorCode::invalid_argument});
    const PeerBinding *peer = nullptr;
    for (const auto &existing : peers_)
      if (existing && existing->local_source == submission.source)
        peer = &*existing;
    if (!peer)
      return reject(Error{ErrorCode::identity_conflict});
    auto frame = runtime::transport::inspect(submission.storage);
    if (!frame)
      return reject(frame.error());
    const auto lane_index = static_cast<std::size_t>(
        lane(frame->envelope.type, frame->envelope.cancel));
    if (frame->packet_bytes > maximum(lane_index))
      return reject(Error{ErrorCode::short_output, 0, maximum(lane_index)});
    if (submission.counter.sender != submission.source.peer ||
        submission.counter.stream_id != frame->envelope.stream_id ||
        submission.counter.type != frame->envelope.type)
      return reject(Error{ErrorCode::identity_conflict});
    const bool supplied =
        submission.completion_credit.held(runtime::Resource::completion) != 0;
    if (supplied &&
        (!admission_.owns(submission.completion_credit) ||
         submission.completion_credit.held(runtime::Resource::completion) != 1))
      return reject(Error{ErrorCode::invalid_argument});
    for (std::size_t i = 0; i < runtime::resource_count; ++i)
      if (i != static_cast<std::size_t>(runtime::Resource::completion) &&
          submission.completion_credit.held(static_cast<runtime::Resource>(i)))
        return reject(Error{ErrorCode::invalid_argument});
    auto [first, last] = range(lane_index);
    std::size_t selected = Slots, pending = 0;
    for (auto i = first; i < last; ++i) {
      if (slots_[i].submission &&
          slots_[i].submission->counter == submission.counter)
        ++pending;
      if (!slots_[i].submission && !slots_[i].retired && selected == Slots)
        selected = i;
    }
    if (selected == Slots ||
        (lane_index == 0 &&
         pending >= config_.capabilities.per_stream_data_limit))
      return reject(Error{ErrorCode::capacity_exhausted});
    if (next_sequence_ == UINT64_MAX)
      return reject(Error{ErrorCode::overflow});
    runtime::AdmissionRequest request;
    request.need(runtime::Resource::completion, supplied ? 0 : 1)
        .need(lane_index == 0   ? runtime::Resource::data_queue
              : lane_index == 1 ? runtime::Resource::ordinary_queue
                                : runtime::Resource::cancellation_queue);
    auto credit = admission_.acquire(request);
    if (!credit)
      return reject(credit.error());
    auto committed =
        counters_.accept(submission.counter, frame->envelope.packet_count);
    if (!committed)
      return reject(committed.error());
    auto &slot = slots_[selected];
    slot.destination = peer->remote[lane_index];
    slot.credits = std::move(*credit);
    slot.sequence = next_sequence_++;
    slot.submission.emplace(std::move(submission));
    ++metrics_.tx_accepted;
    return TxToken{selected, slot.generation};
  }
  Result<bool> progress_next() noexcept {
    if (detached_)
      return false;
    if (progressing_)
      return std::unexpected(Error{ErrorCode::would_deadlock});
    struct Guard {
      bool &flag;
      ~Guard() { flag = false; }
    } guard{progressing_};
    progressing_ = true;
    for (std::size_t attempt = 0; attempt < 6; ++attempt) {
      auto current = cursor_;
      cursor_ = (cursor_ + 1) % 6;
      auto result = current % 2 ? receive(current / 2) : transmit(current / 2);
      if (!result)
        return result;
      if (*result)
        return true;
    }
    return false;
  }
};
} // namespace vita::adapters::posix_udp
