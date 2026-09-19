#pragma once
#include "capture.hpp"
#include <vita/adapters/posix_udp/socket.hpp>
#include <vita/runtime/stream/routing.hpp>
#include <vita/runtime/transaction/outcomes.hpp>
namespace vita::bench {
struct PeerAckPolicy {
  adapters::posix_udp::Address expected_source;
  std::uint32_t oui = 0, first_sid = 101, last_sid = 105;
  codec::Identifier controller = codec::Identifier::short_id(2);
  codec::Identifier controllee = codec::Identifier::short_id(3);
};
// No per-MID state: every checked observation survives into the immutable raw
// log, including late and duplicate Acks. Full SID/MID send correlation and
// duplicate/conflict checks are performed offline against that log.
inline Result<PeerEvent> capture_ack(const PeerAckPolicy &policy,
                                     const adapters::posix_udp::Address &source,
                                     bool truncated,
                                     const codec::PacketView &packet,
                                     std::uint32_t next_issued_mid,
                                     std::uint64_t observed_ns) noexcept {
  const auto &envelope = packet.envelope.envelope;
  if (truncated || source != policy.expected_source || packet.opaque ||
      envelope.type != codec::PacketType::command || !envelope.ack ||
      envelope.cancel || !envelope.stream_id ||
      *envelope.stream_id < policy.first_sid ||
      *envelope.stream_id > policy.last_sid || !envelope.class_id ||
      envelope.class_id->oui != policy.oui ||
      envelope.class_id->information_class != 1 ||
      envelope.class_id->packet_class != 0x20 || !envelope.command ||
      !envelope.command->message_id ||
      envelope.command->message_id >= next_issued_mid ||
      !runtime::same_identifier(envelope.command->controller,
                                policy.controller) ||
      !runtime::same_identifier(envelope.command->controllee,
                                policy.controllee) ||
      ((envelope.command->cam >> 23) & 3) != 2)
    return std::unexpected(Error{ErrorCode::identity_conflict});
  runtime::transaction::ControllerObserver observer(
      envelope.command->message_id);
  auto checked = observer.receive(packet);
  if (!checked)
    return std::unexpected(checked.error());
  const auto event = observer.observation();
  using Kind = runtime::transaction::ObservationKind;
  const auto phase = event.kind == Kind::validation  ? 2u
                     : event.kind == Kind::execution ? 3u
                     : event.kind == Kind::state     ? 4u
                                                     : 0u;
  if (!phase)
    return std::unexpected(Error{ErrorCode::invalid_argument});
  return PeerEvent{
      observed_ns, *envelope.stream_id,      envelope.command->message_id,
      phase,       event.confirms_execution, envelope.command->cam};
}
} // namespace vita::bench
