#pragma once
#include <array>
#include <optional>
#include <vita/codec/packet.hpp>
#include <vita/memory/envelope.hpp>
namespace vita::runtime {
struct PeerSession {
  std::uint64_t peer = 0, generation = 0;
  friend bool operator==(PeerSession, PeerSession) = default;
};
inline bool same_identifier(const codec::Identifier &a,
                            const codec::Identifier &b) noexcept {
  if (a.kind != b.kind)
    return false;
  for (std::size_t i = 0; i < a.size_words(); ++i)
    if (a.words[i] != b.words[i])
      return false;
  return true;
}
inline bool same_class(const std::optional<codec::ClassId> &a,
                       const std::optional<codec::ClassId> &b) noexcept {
  if (a.has_value() != b.has_value())
    return false;
  return !a ||
         (a->oui == b->oui && a->information_class == b->information_class &&
          a->packet_class == b->packet_class);
}
struct RouteKey {
  PeerSession source;
  std::optional<std::uint32_t> stream_id{};
  codec::PacketType type = codec::PacketType::signal;
  std::optional<codec::ClassId> packet_class{};
  codec::Identifier controllee{}, controller{};
};
inline bool same_route(const RouteKey &a, const RouteKey &b) noexcept {
  return a.source == b.source && a.stream_id == b.stream_id &&
         a.type == b.type && same_class(a.packet_class, b.packet_class) &&
         same_identifier(a.controllee, b.controllee) &&
         same_identifier(a.controller, b.controller);
}
struct Route {
  RouteKey key;
  void *context = nullptr;
  void (*receive)(void *, const codec::PacketView &,
                  const memory::RxEnvelope &) noexcept = nullptr;
  // Read-only correlation lookup; no execution or user observation before full
  // parse succeeds.
  std::optional<codec::RequestContext> (*request_context)(
      void *, const codec::Envelope &) noexcept = nullptr;
  Result<void> (*validate_extension)(
      void *, const codec::EnvelopeView &) noexcept = nullptr;
  std::size_t minimum_payload_bytes = 0, maximum_payload_bytes = 65535 * 4;
};
template <std::size_t N = 64> class RouteRegistry {
  std::array<std::optional<Route>, N> routes_{};
  bool frozen_ = false;

public:
  Result<void> add(Route route) noexcept {
    if (frozen_)
      return std::unexpected(Error{ErrorCode::invalid_state});
    if (!route.receive || !route.key.source.generation ||
        static_cast<unsigned>(route.key.type) > 7 ||
        route.minimum_payload_bytes > route.maximum_payload_bytes)
      return std::unexpected(Error{ErrorCode::invalid_argument});
    if (route.key.packet_class && route.key.packet_class->oui > 0xffffff)
      return std::unexpected(Error{ErrorCode::invalid_argument});
    for (const auto *id : {&route.key.controllee, &route.key.controller}) {
      if (static_cast<unsigned>(id->kind) > 2 ||
          (id->kind == codec::IdentifierKind::uuid &&
           !(id->words[0] | id->words[1] | id->words[2] | id->words[3])))
        return std::unexpected(Error{ErrorCode::invalid_argument});
    }
    if (codec::is_extension(route.key.type) &&
        (!route.key.packet_class || !route.validate_extension))
      return std::unexpected(Error{ErrorCode::unsupported_capability});
    const bool sidless =
        route.key.type == codec::PacketType::signal_without_sid ||
        route.key.type == codec::PacketType::extension_without_sid;
    if (sidless == route.key.stream_id.has_value())
      return std::unexpected(Error{ErrorCode::invalid_argument});
    if (!codec::is_command(route.key.type) &&
        (route.key.controllee.kind != codec::IdentifierKind::absent ||
         route.key.controller.kind != codec::IdentifierKind::absent))
      return std::unexpected(Error{ErrorCode::invalid_argument});
    for (const auto &existing : routes_)
      if (existing && same_route(existing->key, route.key))
        return std::unexpected(Error{ErrorCode::invalid_argument});
    for (auto &e : routes_)
      if (!e) {
        e = route;
        return {};
      }
    return std::unexpected(Error{ErrorCode::capacity_exhausted});
  }
  Result<void> install(std::span<const Route> batch) noexcept {
    auto candidate = *this;
    candidate.frozen_ = false;
    for (const auto &route : batch) {
      auto added = candidate.add(route);
      if (!added)
        return added;
    }
    candidate.frozen_ = frozen_;
    *this = std::move(candidate);
    return {};
  }
  std::size_t available() const noexcept {
    std::size_t n = 0;
    for (const auto &e : routes_)
      n += !e;
    return n;
  }
  void detach(void *context) noexcept {
    for (auto &route : routes_)
      if (route && route->context == context) {
        route->context = nullptr;
        route->request_context = nullptr;
        route->receive = [](void *, const codec::PacketView &,
                            const memory::RxEnvelope &) noexcept {};
      }
  }
  void freeze() noexcept { frozen_ = true; }
  Result<const Route *> lookup(PeerSession peer,
                               const codec::Envelope &envelope) const noexcept {
    if (!frozen_)
      return std::unexpected(Error{ErrorCode::invalid_state});
    RouteKey wanted{peer, envelope.stream_id, envelope.type, envelope.class_id};
    if (envelope.command) {
      wanted.controllee = envelope.command->controllee;
      wanted.controller = envelope.command->controller;
    }
    const Route *found = nullptr;
    for (const auto &route : routes_)
      if (route && same_route(route->key, wanted)) {
        if (found)
          return std::unexpected(Error{ErrorCode::invalid_state});
        found = &*route;
      }
    if (!found)
      return std::unexpected(Error{ErrorCode::unsupported_capability});
    return found;
  }
};
} // namespace vita::runtime
