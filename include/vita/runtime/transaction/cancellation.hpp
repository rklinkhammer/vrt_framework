#pragma once
#include <vita/runtime/execution/admission.hpp>
#include <vita/runtime/transaction/outcomes.hpp>
namespace vita::runtime::transaction {
struct CancellationResult {
  FixedVector<AckRecord, 2> responses;
  std::array<bool, 4> cancelled{};
  AdmissionBundle credits;
};
inline Result<std::array<bool, 4>>
cancellation_selectors(const codec::PacketView &packet) noexcept {
  auto valid = codec::validate_cam(packet.envelope.envelope);
  if (!valid || !packet.envelope.envelope.cancel ||
      packet.envelope.envelope.ack || packet.opaque)
    return std::unexpected(Error{ErrorCode::invalid_argument});
  std::array<bool, 4> selected{};
  for (std::size_t i = 0; i < packet.fields.size(); ++i) {
    const auto &view = packet.fields[i];
    if (view.kind != BodyKind::selectors ||
        view.attribute != Attribute::current || field_index(view.id) == 4)
      return std::unexpected(Error{ErrorCode::unsupported_capability});
    selected[field_index(view.id)] = true;
  }
  return selected;
}
inline AdmissionRequest cancellation_resources(std::uint32_t cam) noexcept {
  AdmissionRequest request;
  request.need(Resource::cancellation_queue)
      .need(Resource::cancellation_response,
            std::size_t(bool(cam & (1u << 19))) +
                std::size_t(bool(cam & (1u << 18))));
  return request;
}
inline CancellationResult cancellation_response(
    const codec::PacketView &packet, const StateSnapshot &state,
    const timing::ClockSnapshot &clock, std::array<bool, 4> selected,
    std::array<bool, 4> cancelled, std::array<Diagnostics, 4> diagnostics,
    AdmissionBundle credits, unsigned ack_timing = 0) noexcept {
  CancellationResult result;
  result.credits = std::move(credits);
  result.cancelled = cancelled;
  const auto &envelope = packet.envelope.envelope;
  const auto raw = envelope.command->cam;
  AckRecord ack;
  ack.request = envelope;
  ack.cancellation = true;
  ack.cam.raw = raw;
  ack.cam.action = 2;
  ack.cam.partial = true;
  ack.cam.nack = raw & (1u << 22);
  ack.cam.detail_warning = raw & (1u << 17);
  ack.cam.detail_error = raw & (1u << 16);
  ack.cam.request_x = raw & (1u << 19);
  ack.cam.request_s = raw & (1u << 18);
  ack.kind = AckKind::execution;
  ack.diagnostics = diagnostics;
  ack.state = state;
  ack.timing = ack_timing;
  std::size_t count = 0, success = 0;
  for (std::size_t i = 0; i < 4; ++i) {
    count += selected[i];
    success += cancelled[i];
  }
  ack.partial = success != count;
  ack.scheduled_or_executed = success != 0;
  ack.time_known = clock.state == timing::ClockState::locked ||
                   clock.state == timing::ClockState::holdover;
  ack.time = clock.time;
  ack.epoch = clock.epoch == timing::Epoch::gps   ? codec::Tsi::gps
              : clock.epoch == timing::Epoch::utc ? codec::Tsi::utc
                                                  : codec::Tsi::other;
  if (ack.cam.request_x &&
      (!ack.cam.nack || summary(ack).warnings || summary(ack).errors))
    result.responses.push_back(ack);
  if (ack.cam.request_s) {
    ack.kind = AckKind::state;
    ack.diagnostics = {};
    std::size_t known = 0;
    for (std::size_t i = 0; i < 4; ++i)
      if (selected[i] && state.fields[i].validity == Validity::known) {
        ack.selected_mask |= 1u << i;
        ++known;
      }
    ack.partial = known != count;
    ack.scheduled_or_executed = known != 0;
    result.responses.push_back(ack);
  }
  return result;
}
} // namespace vita::runtime::transaction
