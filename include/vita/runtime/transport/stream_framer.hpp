#pragma once
#include <array>
#include <cstring>
#include <vita/codec/wire.hpp>
#include <vita/memory/envelope.hpp>
#include <vita/runtime/stream/routing.hpp>

namespace vita::runtime::transport {
template <std::size_t Capacity = 65535 * 4> class StreamFramer {
  static_assert(Capacity >= 4);
  std::array<std::byte, Capacity> storage_{};
  std::size_t maximum_packet_bytes_ = Capacity;
  std::size_t buffered_ = 0;
  std::size_t expected_ = 0;
  bool closed_ = false;

  Result<void> inspect_header() noexcept {
    if (buffered_ < 4 || expected_)
      return {};
    const Bytes prefix{storage_.data(), 4};
    const auto header = codec::detail::load32(prefix, 0);
    const auto packet_bytes = static_cast<std::size_t>(header & 0xffffu) * 4;
    if ((header >> 28) > 7 || packet_bytes < 4 ||
        packet_bytes > maximum_packet_bytes_ || packet_bytes > Capacity)
      return std::unexpected(Error{ErrorCode::invalid_argument});
    expected_ = packet_bytes;
    return {};
  }

public:
  using PacketSink = Result<void> (*)(void *, Bytes) noexcept;

  explicit StreamFramer(std::size_t maximum_packet_bytes = Capacity) noexcept
      : maximum_packet_bytes_(maximum_packet_bytes) {}

  Result<std::size_t> feed(Bytes input, void *context,
                           PacketSink sink) noexcept {
    if (closed_ || !sink || maximum_packet_bytes_ < 4 ||
        maximum_packet_bytes_ > Capacity || maximum_packet_bytes_ % 4)
      return std::unexpected(Error{ErrorCode::invalid_state});
    std::size_t delivered = 0;
    while (!input.empty()) {
      const auto target = expected_ ? expected_ : std::size_t{4};
      const auto count = std::min(input.size(), target - buffered_);
      std::memcpy(storage_.data() + buffered_, input.data(), count);
      buffered_ += count;
      input = input.subspan(count);
      auto header = inspect_header();
      if (!header) {
        shutdown();
        return std::unexpected(header.error());
      }
      if (!expected_ || buffered_ != expected_)
        continue;
      auto packet = codec::decode_envelope(Bytes{storage_.data(), expected_});
      if (!packet) {
        auto error = packet.error();
        shutdown();
        return std::unexpected(error);
      }
      const auto packet_bytes = expected_;
      buffered_ = expected_ = 0;
      auto accepted = sink(context, Bytes{storage_.data(), packet_bytes});
      if (!accepted) {
        shutdown();
        return std::unexpected(accepted.error());
      }
      ++delivered;
    }
    return delivered;
  }

  Result<void> disconnect() noexcept {
    closed_ = true;
    if (buffered_) {
      const auto expected = expected_ ? expected_ : std::size_t{4};
      const auto received = buffered_;
      reset();
      return std::unexpected(Error{ErrorCode::short_input, received, expected});
    }
    return {};
  }
  void reconnect() noexcept {
    closed_ = false;
    reset();
  }
  void shutdown() noexcept {
    closed_ = true;
    reset();
  }
  void reset() noexcept { buffered_ = expected_ = 0; }
  std::size_t buffered_bytes() const noexcept { return buffered_; }
  std::size_t awaiting_bytes() const noexcept {
    return (expected_ ? expected_ : std::size_t{4}) - buffered_;
  }
  bool stalled() const noexcept { return buffered_ != 0; }
  bool closed() const noexcept { return closed_; }
};

template <std::size_t Capacity = 65535 * 4, std::size_t Routes = 64>
class StreamIngress {
  StreamFramer<Capacity> framer_;
  runtime::RouteRegistry<Routes> &routes_;
  memory::ExternalPool data_, control_, cancellation_;
  runtime::PeerSession peer_;

  static Result<void> route_packet(void *context, Bytes wire) noexcept {
    return static_cast<StreamIngress *>(context)->route_packet(wire);
  }
  Result<void> route_packet(Bytes wire) noexcept {
    auto header = codec::decode_envelope(wire);
    if (!header)
      return std::unexpected(header.error());
    auto route = routes_.lookup(peer_, header->envelope);
    if (!route)
      return std::unexpected(route.error());
    if ((*route)->before_decode)
      (*route)->before_decode((*route)->context, header->envelope);
    if (header->payload.size() < (*route)->minimum_payload_bytes ||
        header->payload.size() > (*route)->maximum_payload_bytes)
      return std::unexpected(Error{ErrorCode::unsupported_capability});
    if (codec::is_extension(header->envelope.type)) {
      auto valid = (*route)->validate_extension((*route)->context, *header);
      if (!valid)
        return std::unexpected(valid.error());
    }
    codec::DecodeOptions options{};
    if ((*route)->request_context)
      options.request =
          (*route)->request_context((*route)->context, header->envelope);
    auto decoded = codec::decode_packet(wire, options);
    if (!decoded)
      return std::unexpected(decoded.error());
    const bool data = codec::is_data(header->envelope.type);
    auto &pool = data                      ? data_
                 : header->envelope.cancel ? cancellation_
                                           : control_;
    auto buffer =
        pool.acquire({wire.size(), 1, memory::MemoryDomain::cpu, true});
    if (!buffer)
      return std::unexpected(buffer.error());
    auto writable = buffer->writable_bytes();
    if (!writable)
      return std::unexpected(writable.error());
    std::memcpy(writable->data(), wire.data(), wire.size());
    auto sized = buffer->set_size(wire.size());
    if (!sized)
      return sized;
    auto leased = buffer->bytes();
    if (!leased)
      return std::unexpected(leased.error());
    auto stable = codec::decode_packet(*leased, options);
    if (!stable)
      return std::unexpected(stable.error());
    memory::RxEnvelope received;
    auto lease = received.add_buffer(std::move(*buffer));
    if (!lease)
      return std::unexpected(lease.error());
    const auto offset = stable->envelope.payload_offset;
    auto prologue = received.set_prologue({*lease, 0, offset});
    if (!prologue)
      return prologue;
    if (!stable->envelope.payload.empty()) {
      auto payload = received.append_payload(
          {*lease, offset, stable->envelope.payload.size()});
      if (!payload)
        return payload;
    }
    if (stable->envelope.trailer) {
      auto trailer = received.set_trailer(
          {*lease, offset + stable->envelope.payload.size(), 4});
      if (!trailer)
        return trailer;
    }
    (*route)->receive((*route)->context, *stable, received);
    return {};
  }

public:
  StreamIngress(runtime::RouteRegistry<Routes> &routes,
                memory::ExternalPool data, memory::ExternalPool control,
                memory::ExternalPool cancellation, runtime::PeerSession peer,
                std::size_t maximum_packet_bytes = Capacity) noexcept
      : framer_(maximum_packet_bytes), routes_(routes), data_(std::move(data)),
        control_(std::move(control)), cancellation_(std::move(cancellation)),
        peer_(peer) {}

  Result<std::size_t> feed(Bytes input) noexcept {
    if (!peer_.generation)
      return std::unexpected(Error{ErrorCode::invalid_state});
    return framer_.feed(input, this, route_packet);
  }
  Result<void> disconnect() noexcept { return framer_.disconnect(); }
  Result<void> reconnect(runtime::PeerSession peer) noexcept {
    if (!peer.generation)
      return std::unexpected(Error{ErrorCode::invalid_argument});
    peer_ = peer;
    framer_.reconnect();
    return {};
  }
  void shutdown() noexcept { framer_.shutdown(); }
  bool stalled() const noexcept { return framer_.stalled(); }
  bool closed() const noexcept { return framer_.closed(); }
};
} // namespace vita::runtime::transport
