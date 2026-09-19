#pragma once
#include <cstring>
#include <type_traits>
#include <vita/runtime/stream/routing.hpp>
#include <vita/runtime/transaction/engine.hpp>
namespace vita::runtime::transaction {
struct TransactionKey {
  std::uint64_t binding_generation = 0;
  PeerSession peer{};
  std::optional<std::uint32_t> stream_id;
  codec::Identifier controller{}, controllee{};
  std::uint32_t message_id = 0;
};
inline bool same_key(const TransactionKey &a,
                     const TransactionKey &b) noexcept {
  return a.binding_generation == b.binding_generation && a.peer == b.peer &&
         a.stream_id == b.stream_id && a.message_id == b.message_id &&
         same_identifier(a.controller, b.controller) &&
         same_identifier(a.controllee, b.controllee);
}
inline bool same_association(TransactionKey a,TransactionKey b) noexcept {a.message_id=b.message_id=0;return same_key(a,b);}
inline Result<TransactionKey> transaction_key(const codec::PacketView &packet,
                                              std::uint64_t generation,
                                              PeerSession peer) noexcept {
  const auto &e = packet.envelope.envelope;
  if (!generation || !peer.generation || !codec::is_command(e.type) ||
      !e.command)
    return std::unexpected(Error{ErrorCode::invalid_argument});
  return TransactionKey{generation,
                        peer,
                        e.stream_id,
                        e.command->controller,
                        e.command->controllee,
                        e.command->message_id};
}
inline std::byte canonical_command_byte(const codec::PacketView &packet,
                                        std::size_t offset) noexcept {
  auto b = packet.envelope.wire[offset];
  if (offset == 1)
    b &= std::byte{0xf0};
  if (!packet.envelope.envelope.cancel &&
      offset == packet.envelope.payload_offset)
    b &= std::byte{0x7f};
  return b;
}

enum class DuplicateKind { fresh, active, replay };
struct RetentionToken {
  std::size_t slot = 0;
  std::uint64_t generation = 0;
  bool cancellation = false;
  friend bool operator==(RetentionToken, RetentionToken) = default;
};
struct RetentionAdmission {
  RetentionToken token;
  DuplicateKind kind;
};
// One shared store across bindings; all methods run in the runtime's serialized
// domain. D-P07-1: one immutable L=1 cancellation meaning alongside each L=0
// original.
template <std::size_t Entries = 4096,
          std::size_t ByteCapacity = 8 * 1024 * 1024>
class RetentionStore {
  static_assert(Entries > 0 && Entries <= UINT32_MAX / 2 && ByteCapacity > 0 &&
                std::is_trivially_copyable_v<AckRecord>);
  struct Record {
    bool used = false, active = false, terminal = false;
    std::size_t offset = 0, bytes = 0, canonical = 0, response_capacity = 0,
                response_count = 0;
    std::optional<Handle> engine;
    AdmissionBundle credits;
  };
  struct Entry {
    std::uint64_t generation = 1;
    TransactionKey key{};
    std::array<Record, 2> records;
    std::size_t references = 0;
    timing::MonoTime expires{};
  };
  struct Storage {
    std::array<Entry, Entries> entries;
    std::array<std::byte, ByteCapacity> bytes;
    // Sorted by byte offset; records and their retained byte spans never move.
    std::array<std::uint32_t, Entries * 2> occupied{};
    std::size_t occupied_count = 0, placement_probes = 0;
  };
  std::unique_ptr<Storage> storage_ = std::make_unique<Storage>();
  AdmissionPool &admission_;
  Entry *entry(RetentionToken token) noexcept {
    if (token.slot >= Entries)
      return nullptr;
    auto &e = storage_->entries[token.slot];
    return e.records[token.cancellation].used &&
                   e.generation == token.generation
               ? &e
               : nullptr;
  }
  Record& indexed_record(std::uint32_t id) noexcept {
    return storage_->entries[id / 2].records[id % 2];
  }
  void remove_extent(Record& record) noexcept {
    if (!record.used) return;
    for (std::size_t i = 0; i < storage_->occupied_count; ++i) {
      if (&indexed_record(storage_->occupied[i]) != &record) continue;
      for (std::size_t next = i + 1; next < storage_->occupied_count; ++next)
        storage_->occupied[next - 1] = storage_->occupied[next];
      --storage_->occupied_count;
      return;
    }
  }
  void erase(Entry &e) noexcept {
    for (auto &r : e.records) {
      remove_extent(r);
      r = Record{};
    }
    e.references = 0;
    e.expires = {};
    ++e.generation;
  }

public:
  explicit RetentionStore(AdmissionPool &admission) : admission_(admission) {}
  static constexpr std::uint64_t minimum_retention_ns = 30'000'000'000ull;
  static constexpr std::size_t storage_bytes() noexcept {
    return sizeof(Storage);
  }
  // Work counter for reproducible complexity checks, independent of host timing.
  std::size_t last_placement_probes() const noexcept {
    return storage_->placement_probes;
  }
  bool association_retained(const TransactionKey& relationship) const noexcept {
    for(const auto& entry:storage_->entries)if(entry.records[0].used&&same_association(entry.key,relationship))return true;
    return false;
  }
  Result<RetentionAdmission> replay_existing(const TransactionKey& key,const codec::PacketView& packet) noexcept {
    if(packet.envelope.envelope.ack||packet.envelope.wire.size()<4)return std::unexpected(Error{ErrorCode::invalid_argument});
    auto derived=transaction_key(packet,key.binding_generation,key.peer);if(!derived||!same_key(key,*derived))return std::unexpected(Error{ErrorCode::invalid_argument});
    const bool cancellation=packet.envelope.envelope.cancel;
    for(std::size_t i=0;i<Entries;++i){auto& entry=storage_->entries[i];if(!entry.records[0].used||!same_key(entry.key,key))continue;auto& record=entry.records[cancellation];
      if(!record.used)return std::unexpected(Error{ErrorCode::invalid_state});
      if(record.canonical!=packet.envelope.wire.size())return std::unexpected(Error{ErrorCode::identity_conflict});
      for(std::size_t b=0;b<record.canonical;++b)if(storage_->bytes[record.offset+b]!=canonical_command_byte(packet,b))return std::unexpected(Error{ErrorCode::identity_conflict});
      if(entry.references==SIZE_MAX)return std::unexpected(Error{ErrorCode::overflow});
      ++entry.references;return RetentionAdmission{{i,entry.generation,cancellation},record.terminal?DuplicateKind::replay:DuplicateKind::active};
    }
    return std::unexpected(Error{ErrorCode::invalid_state});
  }
  Result<RetentionAdmission> reserve(const TransactionKey &key,
                                     const codec::PacketView &packet,
                                     std::size_t responses = 3) noexcept {
    const bool cancellation = packet.envelope.envelope.cancel;
    if (!key.binding_generation || !key.peer.generation ||
        packet.envelope.wire.size() < 4 ||
        responses > (cancellation ? 2u : 3u) || packet.envelope.envelope.ack)
      return std::unexpected(Error{ErrorCode::invalid_argument});
    auto derived = transaction_key(packet, key.binding_generation, key.peer);
    if (!derived || !same_key(key, *derived))
      return std::unexpected(Error{ErrorCode::invalid_argument});
    std::size_t slot = Entries;
    for (std::size_t i = 0; i < Entries; ++i) {
      auto &e = storage_->entries[i];
      if (!e.records[0].used || !same_key(e.key, key))
        continue;
      slot = i;
      auto &r = e.records[cancellation];
      if (!r.used)
        break;
      if (r.canonical != packet.envelope.wire.size())
        return std::unexpected(Error{ErrorCode::identity_conflict});
      for (std::size_t b = 0; b < r.canonical; ++b)
        if (storage_->bytes[r.offset + b] != canonical_command_byte(packet, b))
          return std::unexpected(Error{ErrorCode::identity_conflict});
      ++e.references;
      return RetentionAdmission{{i, e.generation, cancellation},
                                r.terminal ? DuplicateKind::replay
                                           : DuplicateKind::active};
    }
    if (cancellation && slot == Entries)
      return std::unexpected(Error{ErrorCode::invalid_state});
    const auto canonical = packet.envelope.wire.size();
    if (canonical > ByteCapacity ||
        responses > (ByteCapacity - canonical) / sizeof(AckRecord))
      return std::unexpected(Error{ErrorCode::capacity_exhausted});
    const auto required = canonical + responses * sizeof(AckRecord);
    if (slot == Entries)
      for (std::size_t i = 0; i < Entries; ++i)
        if (!storage_->entries[i].records[0].used &&
            storage_->entries[i].generation != UINT64_MAX) {
          slot = i;
          break;
        }
    if (slot == Entries)
      return std::unexpected(Error{ErrorCode::capacity_exhausted});
    std::size_t offset = 0, insertion = 0;
    storage_->placement_probes = 0;
    // In offset order the first fitting gap can be found in one traversal.
    // Every successful record has a positive canonical size, so at most 2N
    // occupied extents exist and insertion always has a preallocated index slot.
    for (; insertion < storage_->occupied_count; ++insertion) {
      ++storage_->placement_probes;
      const auto& prior = indexed_record(storage_->occupied[insertion]);
      if (required <= prior.offset - offset) break;
      offset = prior.offset + prior.bytes;
    }
    if (offset > ByteCapacity - required)
      return std::unexpected(Error{ErrorCode::capacity_exhausted});
    AdmissionRequest request;
    request.need(Resource::duplicate_entry, cancellation ? 0 : 1)
        .need(Resource::duplicate_bytes, required);
    auto credits = admission_.acquire(request);
    if (!credits)
      return std::unexpected(credits.error());
    auto &e = storage_->entries[slot];
    auto &r = e.records[cancellation];
    r.used = true;
    e.key = key;
    r.offset = offset;
    r.bytes = required;
    r.canonical = canonical;
    r.response_capacity = responses;
    r.response_count = 0;
    ++e.references;
    r.active = r.terminal = false;
    r.credits = std::move(*credits);
    for (auto i = storage_->occupied_count; i > insertion; --i)
      storage_->occupied[i] = storage_->occupied[i - 1];
    storage_->occupied[insertion] = static_cast<std::uint32_t>(slot * 2 + cancellation);
    ++storage_->occupied_count;
    for (std::size_t b = 0; b < canonical; ++b)
      storage_->bytes[offset + b] = canonical_command_byte(packet, b);
    return RetentionAdmission{{slot, e.generation, cancellation},
                              DuplicateKind::fresh};
  }
  Result<void> bind(RetentionToken token,
                    std::optional<Handle> handle = {}) noexcept {
    auto *e = entry(token);
    if (!e)
      return std::unexpected(Error{ErrorCode::stale_generation});
    auto &r = e->records[token.cancellation];
    if (r.active || r.terminal)
      return std::unexpected(Error{ErrorCode::invalid_state});
    r.active = true;
    r.engine = handle;
    return {};
  }
  Result<std::optional<Handle>> engine_handle(RetentionToken token) noexcept {
    auto *e = entry(token);
    if (!e)
      return std::unexpected(Error{ErrorCode::stale_generation});
    return e->records[token.cancellation].engine;
  }
  Result<void> append(RetentionToken token, const AckRecord &ack) noexcept {
    auto *e = entry(token);
    if (!e)
      return std::unexpected(Error{ErrorCode::stale_generation});
    auto &r = e->records[token.cancellation];
    if (!r.active || r.terminal || ack.cancellation != token.cancellation)
      return std::unexpected(Error{ErrorCode::invalid_state});
    if (r.response_count == r.response_capacity)
      return std::unexpected(Error{ErrorCode::capacity_exhausted});
    std::memcpy(storage_->bytes.data() + r.offset + r.canonical +
                    r.response_count * sizeof(AckRecord),
                &ack, sizeof ack);
    ++r.response_count;
    return {};
  }
  Result<std::optional<AckRecord>> response(RetentionToken token,
                                            std::size_t index) noexcept {
    auto *e = entry(token);
    if (!e)
      return std::unexpected(Error{ErrorCode::stale_generation});
    const auto &r = e->records[token.cancellation];
    if (index >= r.response_count)
      return std::optional<AckRecord>{};
    AckRecord ack;
    std::memcpy(&ack,
                storage_->bytes.data() + r.offset + r.canonical +
                    index * sizeof(AckRecord),
                sizeof ack);
    return std::optional<AckRecord>{ack};
  }
  Result<void> complete(RetentionToken token, timing::MonoTime now) noexcept {
    auto *e = entry(token);
    if (!e)
      return std::unexpected(Error{ErrorCode::stale_generation});
    auto &r = e->records[token.cancellation];
    if (!r.active || r.terminal)
      return std::unexpected(Error{ErrorCode::invalid_state});
    auto end = timing::deadline(now, minimum_retention_ns);
    if (!end)
      e->expires = {UINT64_MAX};
    else if (*end > e->expires)
      e->expires = *end;
    r.terminal = true;
    r.active = false;
    r.engine.reset();
    return {};
  }
  Result<Bytes> canonical(RetentionToken token) noexcept {
    auto *e = entry(token);
    if (!e)
      return std::unexpected(Error{ErrorCode::stale_generation});
    const auto &r = e->records[token.cancellation];
    return Bytes{storage_->bytes}.subspan(r.offset, r.canonical);
  }
  Result<bool> terminal(RetentionToken token) noexcept {auto* e=entry(token);if(!e)return std::unexpected(Error{ErrorCode::stale_generation});return e->records[token.cancellation].terminal;}
  Result<void> retain(RetentionToken token) noexcept {
    auto *e = entry(token);
    if (!e)
      return std::unexpected(Error{ErrorCode::stale_generation});
    ++e->references;
    return {};
  }
  Result<void> release(RetentionToken token) noexcept {
    auto *e = entry(token);
    if (!e || !e->references)
      return std::unexpected(Error{ErrorCode::stale_generation});
    --e->references;
    if (!e->references && !e->records[0].active && !e->records[0].terminal)
      erase(*e);
    return {};
  }
  Result<void> rollback(RetentionToken token) noexcept {
    auto *e = entry(token);
    if (!e || !e->references)
      return std::unexpected(Error{ErrorCode::stale_generation});
    auto &r = e->records[token.cancellation];
    if (r.active || r.terminal)
      return std::unexpected(Error{ErrorCode::invalid_state});
    remove_extent(r);
    r = Record{};
    --e->references;
    if (!token.cancellation)
      erase(*e);
    return {};
  }
  void expire(timing::MonoTime now) noexcept {
    for (auto &e : storage_->entries)
      if (e.records[0].used && e.records[0].terminal &&
          (!e.records[1].used || e.records[1].terminal) && !e.references &&
          now >= e.expires)
        erase(e);
  }
  std::size_t size() const noexcept {
    std::size_t count = 0;
    for (const auto &e : storage_->entries)
      count += e.records[0].used;
    return count;
  }
};
} // namespace vita::runtime::transaction
