#pragma once
#include <cstring>
#include <vita/runtime/transport/types.hpp>
namespace vita::runtime::transport {
using namespace vita::codec;
struct Framing {
  Envelope envelope;
  std::size_t payload_offset, payload_bytes, packet_bytes;
};
// Reads only the bounded VRT prologue, never stages the IQ payload.
inline Result<Framing> inspect(const memory::TxStorage &storage) noexcept {
  std::array<std::byte, 68> prefix{};
  std::size_t copied = 0;
  auto ensure = [&](std::size_t wanted) noexcept -> Result<void> {
    if (wanted > prefix.size() || wanted > storage.byte_size())
      return std::unexpected(Error{ErrorCode::short_input, copied, wanted});
    std::size_t start = 0;
    for (std::size_t i = 0; i < storage.segment_count() && copied < wanted;
         ++i) {
      auto part = storage.segment(i);
      if (!part)
        return std::unexpected(part.error());
      if (copied >= start + part->size()) {
        start += part->size();
        continue;
      }
      const auto offset = copied - start,
                 n = std::min(wanted - copied, part->size() - offset);
      std::memcpy(prefix.data() + copied, part->data() + offset, n);
      copied += n;
      start += part->size();
    }
    if (copied != wanted)
      return std::unexpected(Error{ErrorCode::short_input, copied, wanted});
    return {};
  };
  auto header = ensure(4);
  if (!header)
    return std::unexpected(header.error());
  const Bytes wire{prefix};
  const auto total = storage.byte_size();
  if (wire.size() < 4)
    return std::unexpected(Error{ErrorCode::short_input, 0, 4});
  const auto h = codec::detail::load32(wire, 0);
  const auto type = h >> 28;
  const std::size_t size = (h & 0xffff) * 4;
  if (type > 7 || size == 0)
    return std::unexpected(Error{ErrorCode::invalid_argument});
  if (size > total)
    return std::unexpected(Error{ErrorCode::short_input, total, size});
  if (size != total)
    return std::unexpected(Error{ErrorCode::invalid_argument, size});
  Envelope e{};
  e.type = static_cast<PacketType>(type);
  e.packet_count = (h >> 16) & 15;
  e.timestamp.tsi = static_cast<Tsi>((h >> 22) & 3);
  e.timestamp.tsf = static_cast<Tsf>((h >> 20) & 3);
  if (is_data(e.type)) {
    e.trailer = h & (1u << 26);
    e.nd0 = h & (1u << 25);
    e.spectrum = h & (1u << 24);
  } else if (is_command(e.type)) {
    if (h & (1u << 25))
      return std::unexpected(Error{ErrorCode::invalid_argument});
    e.ack = h & (1u << 26);
    e.cancel = h & (1u << 24);
  } else {
    if (h & (1u << 26))
      return std::unexpected(Error{ErrorCode::invalid_argument});
    e.nd0 = h & (1u << 25);
    e.tsm = h & (1u << 24);
  }
  std::size_t offset = 4;
  auto take = [&]() noexcept -> Result<std::uint32_t> {
    auto ready = ensure(offset + 4);
    if (!ready)
      return std::unexpected(ready.error());
    auto value = codec::detail::load32(wire, offset);
    offset += 4;
    return value;
  };
  if (type != 0 && type != 2) {
    auto v = take();
    if (!v)
      return std::unexpected(v.error());
    e.stream_id = *v;
  }
  if (h & (1u << 27)) {
    auto a = take(), b = take();
    if (!a)
      return std::unexpected(a.error());
    if (!b)
      return std::unexpected(b.error());
    if (*a & 0x07000000)
      return std::unexpected(Error{ErrorCode::invalid_argument, offset - 8});
    e.class_id = ClassId{*a & 0xffffff, static_cast<std::uint16_t>(*b >> 16),
                         static_cast<std::uint16_t>(*b),
                         static_cast<std::uint8_t>(*a >> 27)};
  }
  if (e.timestamp.tsi != Tsi::none) {
    auto v = take();
    if (!v)
      return std::unexpected(v.error());
    e.timestamp.integer = *v;
  }
  if (e.timestamp.tsf != Tsf::none) {
    auto a = take(), b = take();
    if (!a)
      return std::unexpected(a.error());
    if (!b)
      return std::unexpected(b.error());
    e.timestamp.fractional = (std::uint64_t{*a} << 32) | *b;
  }
  if (is_command(e.type)) {
    auto cam = take(), mid = take();
    if (!cam)
      return std::unexpected(cam.error());
    if (!mid)
      return std::unexpected(mid.error());
    e.command = Command{*cam, *mid};
    for (unsigned which = 0; which < 2; ++which) {
      auto &id = which ? e.command->controller : e.command->controllee;
      const unsigned enable = which ? 29 : 31, uuid = which ? 28 : 30;
      if (*cam & (1u << enable)) {
        id.kind = (*cam & (1u << uuid)) ? IdentifierKind::uuid
                                        : IdentifierKind::short_id;
        for (std::size_t i = 0; i < id.size_words(); ++i) {
          auto v = take();
          if (!v)
            return std::unexpected(v.error());
          id.words[i] = *v;
        }
      }
    }
  }
  const std::size_t trailer_size = e.trailer ? 4 : 0;
  if (offset > size || trailer_size > size - offset)
    return std::unexpected(
        Error{ErrorCode::short_input, offset, offset + trailer_size});
  const auto payload_bytes = size - offset - trailer_size;
  auto measured = measure_envelope(e, payload_bytes);
  if (!measured)
    return std::unexpected(measured.error());
  return Framing{e, offset, payload_bytes, size};
}
} // namespace vita::runtime::transport
