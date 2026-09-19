#pragma once
#include <vita/runtime/transaction/retention.hpp>
#include <array>
#include <memory>
#include <optional>
namespace vita::runtime::transaction {
struct RelationshipHandle {std::size_t slot{};friend bool operator==(RelationshipHandle,RelationshipHandle)=default;};
struct ControllerHandle {std::size_t slot{};std::uint64_t generation{};friend bool operator==(ControllerHandle,ControllerHandle)=default;};
struct TrackedRequest {ControllerHandle handle;codec::Envelope envelope;};
struct StateObservation {
    StateSnapshot state{};std::uint8_t selected_mask{};codec::Timestamp timestamp{};bool hypothetical{},late{};
};
enum class CancelRegistration { fresh,retry };
// Serialized controller domain. Construction allocates bounded record storage once.
template<std::size_t Records=256,std::size_t MaxCancelBytes=512,std::size_t Relationships=64>
class ControllerRegistry {
    static_assert(Records && MaxCancelBytes>=4 && Relationships);
    struct Relationship {bool used{};TransactionKey identity{};std::uint64_t next_mid{1};};
    struct Record {
        bool used{},ordinary_active{},cancel_registered{},cancel_active{};
        std::uint64_t generation{1};std::size_t references{};
        TransactionKey key{};codec::Envelope request{};std::uint32_t cancel_cam{};
        std::uint8_t requested_fields{},cancel_fields{},seen_ordinary{},seen_cancel{};
        timing::MonoTime deadline{},cancel_deadline{},expires{};
        ControllerObserver observer{0};
        std::array<std::byte,MaxCancelBytes> cancel_meaning{};std::size_t cancel_size{};
        std::optional<StateObservation> ordinary_state{},cancel_state{};
    };
    struct Storage {std::array<Relationship,Relationships> relationships{};std::array<Record,Records> records{};};
    std::unique_ptr<Storage> storage_=std::make_unique<Storage>();
    static bool same_wire_relationship(const TransactionKey& a,const TransactionKey& b) noexcept {
        return a.peer.peer==b.peer.peer && a.stream_id==b.stream_id && same_identifier(a.controller,b.controller) && same_identifier(a.controllee,b.controllee);
    }
    Record* find(ControllerHandle handle) noexcept {
        if(handle.slot>=Records)return nullptr;auto& r=storage_->records[handle.slot];return r.used&&r.generation==handle.generation?&r:nullptr;
    }
    const Record* find(ControllerHandle handle) const noexcept {
        if(handle.slot>=Records)return nullptr;auto const& r=storage_->records[handle.slot];return r.used&&r.generation==handle.generation?&r:nullptr;
    }
    static std::uint8_t requested_responses(std::uint32_t cam) noexcept {return static_cast<std::uint8_t>((cam>>18)&7);}
    static std::byte cancel_byte(const codec::PacketView& packet,std::size_t index) noexcept {
        auto value=packet.envelope.wire[index];return index==1 ? value&std::byte{0xf0} : value;
    }
    Result<void> terminal(Record& r,bool cancellation,timing::MonoTime now) noexcept {
        if(!(cancellation?r.cancel_active:r.ordinary_active))return {};
        auto until=timing::deadline(now,minimum_retention_ns);if(!until)return std::unexpected(until.error());
        if(cancellation)r.cancel_active=false;else r.ordinary_active=false;
        if(*until>r.expires)r.expires=*until;return {};
    }
    static Result<StateObservation> capture_state(const codec::PacketView& packet,const Record& record,bool cancellation) noexcept {
        StateObservation out;out.timestamp=packet.envelope.envelope.timestamp;
        if(out.timestamp.tsf==codec::Tsf::picoseconds && out.timestamp.fractional>=timing::picoseconds_per_second)return std::unexpected(Error{ErrorCode::invalid_argument});
        out.hypothetical=((packet.envelope.envelope.command->cam>>23)&3)==1;
        out.late=cancellation?record.observer.cancellation_timed_out():record.observer.timed_out();
        for(std::size_t i=0;i<4;++i)if((cancellation?record.cancel_fields:record.requested_fields)&(1u<<i))out.state.fields[i].validity=Validity::unknown;
        for(std::size_t i=0;i<packet.fields.size();++i){auto const& field=packet.fields[i];auto index=field_index(field.id);
            if(index==4 || !((cancellation?record.cancel_fields:record.requested_fields)&(1u<<index)) || field.kind!=BodyKind::values || field.attribute!=Attribute::current)return std::unexpected(Error{ErrorCode::unsupported_capability});
            auto value=field.value();if(!value)return std::unexpected(value.error());out.state.fields[index]={field.id,*value,Validity::known};out.selected_mask|=1u<<index;
        }return out;
    }
    static bool same_state(const StateObservation& a,const StateObservation& b) noexcept {
        if(a.selected_mask!=b.selected_mask||a.hypothetical!=b.hypothetical||a.timestamp.tsi!=b.timestamp.tsi||a.timestamp.tsf!=b.timestamp.tsf||a.timestamp.integer!=b.timestamp.integer||a.timestamp.fractional!=b.timestamp.fractional)return false;
        for(std::size_t i=0;i<4;++i)if(a.state.fields[i].id!=b.state.fields[i].id||a.state.fields[i].validity!=b.state.fields[i].validity||(a.state.fields[i].validity==Validity::known&&a.state.fields[i].value!=b.state.fields[i].value))return false;
        return true;
    }
public:
    static constexpr std::uint64_t minimum_retention_ns=30000000000ULL;
    static constexpr std::size_t record_bytes() noexcept{return sizeof(Record);}
    static constexpr std::size_t storage_bytes() noexcept{return sizeof(Storage);}
    bool association_retained(const TransactionKey& relationship) const noexcept {
        for(const auto& record:storage_->records)if(record.used&&same_association(record.key,relationship))return true;
        return false;
    }
    Result<void> can_register_relationship(TransactionKey identity) const noexcept {
        if(!identity.binding_generation||!identity.peer.generation)return std::unexpected(Error{ErrorCode::invalid_argument});identity.message_id=0;
        bool free=false;
        for(const auto& existing:storage_->relationships){if(!existing.used){free=true;continue;}if(same_key(existing.identity,identity))return {};if(same_wire_relationship(existing.identity,identity))return std::unexpected(Error{ErrorCode::identity_conflict});}
        if(!free)return std::unexpected(Error{ErrorCode::capacity_exhausted});return {};
    }
    Result<RelationshipHandle> register_relationship(TransactionKey identity,std::uint32_t next_mid=1) noexcept {
        if(!identity.binding_generation||!identity.peer.generation)return std::unexpected(Error{ErrorCode::invalid_argument});identity.message_id=0;
        for(std::size_t i=0;i<Relationships;++i)if(storage_->relationships[i].used){auto const& existing=storage_->relationships[i];
            if(same_key(existing.identity,identity))return RelationshipHandle{i};
            // Local generation changes do not disambiguate old UDP wire identities.
            if(same_wire_relationship(existing.identity,identity))return std::unexpected(Error{ErrorCode::identity_conflict});
        }
        for(std::size_t i=0;i<Relationships;++i)if(!storage_->relationships[i].used){storage_->relationships[i]={true,identity,next_mid};return RelationshipHandle{i};}
        return std::unexpected(Error{ErrorCode::capacity_exhausted});
    }
    Result<TrackedRequest> track(RelationshipHandle relationship,const codec::PacketView& request,timing::MonoTime now,std::uint64_t timeout_ns) noexcept {
        if(relationship.slot>=Relationships||!storage_->relationships[relationship.slot].used)return std::unexpected(Error{ErrorCode::invalid_argument});
        auto& rel=storage_->relationships[relationship.slot];auto const& e=request.envelope.envelope;
        if(e.type!=codec::PacketType::command||!e.command||e.ack||e.cancel||request.opaque||request.requires_request_context)return std::unexpected(Error{ErrorCode::invalid_argument});
        auto derived=transaction_key(request,rel.identity.binding_generation,rel.identity.peer);if(!derived)return std::unexpected(derived.error());derived->message_id=0;
        if(!same_key(*derived,rel.identity))return std::unexpected(Error{ErrorCode::identity_conflict});
        if(rel.next_mid>UINT32_MAX)return std::unexpected(Error{ErrorCode::resource_limit});
        auto limit=timing::deadline(now,timeout_ns);if(!limit)return std::unexpected(limit.error());
        if(now.ns>UINT64_MAX-minimum_retention_ns||limit->ns>UINT64_MAX-minimum_retention_ns)return std::unexpected(Error{ErrorCode::overflow});
        std::uint8_t selected=0;for(std::size_t i=0;i<request.fields.size();++i){auto index=field_index(request.fields[i].id);if(index==4)return std::unexpected(Error{ErrorCode::unsupported_capability});selected|=1u<<index;}
        for(std::size_t i=0;i<Records;++i){auto& r=storage_->records[i];if(r.used||r.generation==UINT64_MAX)continue;
            auto generation=r.generation;r=Record{};r.generation=generation;r.used=r.ordinary_active=true;r.references=1;
            r.key=rel.identity;r.key.message_id=static_cast<std::uint32_t>(rel.next_mid++);r.request=e;r.request.command->message_id=r.key.message_id;
            r.requested_fields=selected;r.deadline=*limit;r.observer=ControllerObserver(r.key.message_id);
            return TrackedRequest{{i,r.generation},r.request};
        }return std::unexpected(Error{ErrorCode::capacity_exhausted});
    }
    Result<CancelRegistration> register_cancel(ControllerHandle handle,const codec::PacketView& packet,timing::MonoTime now,std::uint64_t timeout_ns) noexcept {
        auto* r=find(handle);if(!r)return std::unexpected(Error{ErrorCode::stale_generation});auto const& e=packet.envelope.envelope;
        if(e.class_id!=r->request.class_id)return std::unexpected(Error{ErrorCode::identity_conflict});
        if(!e.cancel||e.ack||!e.command||packet.opaque||packet.requires_request_context||packet.envelope.wire.size()<4)return std::unexpected(Error{ErrorCode::invalid_argument});
        auto key=transaction_key(packet,r->key.binding_generation,r->key.peer);if(!key||!same_key(*key,r->key))return std::unexpected(Error{ErrorCode::identity_conflict});
        if(r->cancel_registered){
            if(r->cancel_size!=packet.envelope.wire.size())return std::unexpected(Error{ErrorCode::identity_conflict});
            for(std::size_t i=0;i<r->cancel_size;++i)if(r->cancel_meaning[i]!=cancel_byte(packet,i))return std::unexpected(Error{ErrorCode::identity_conflict});
            return CancelRegistration::retry;
        }
        std::uint8_t selected=0;for(std::size_t i=0;i<packet.fields.size();++i){auto index=field_index(packet.fields[i].id);if(index==4)return std::unexpected(Error{ErrorCode::unsupported_capability});selected|=1u<<index;}
        if(selected&~r->requested_fields)return std::unexpected(Error{ErrorCode::invalid_argument});
        if(packet.envelope.wire.size()>MaxCancelBytes)return std::unexpected(Error{ErrorCode::capacity_exhausted});
        auto limit=timing::deadline(now,timeout_ns);if(!limit||limit->ns>UINT64_MAX-minimum_retention_ns)return std::unexpected(Error{ErrorCode::overflow});
        r->cancel_registered=r->cancel_active=true;r->cancel_fields=selected;r->cancel_cam=e.command->cam;r->cancel_deadline=*limit;r->cancel_size=packet.envelope.wire.size();
        for(std::size_t i=0;i<r->cancel_size;++i)r->cancel_meaning[i]=cancel_byte(packet,i);
        return CancelRegistration::fresh;
    }
    Result<void> local_send(ControllerHandle handle,bool succeeded) noexcept {auto* r=find(handle);if(!r)return std::unexpected(Error{ErrorCode::stale_generation});r->observer.local_send(succeeded);return {};}
    Result<ControllerHandle> receive(std::uint64_t binding_generation,PeerSession peer,Bytes wire,timing::MonoTime now) noexcept {
        auto framing=codec::decode_envelope(wire);if(!framing)return std::unexpected(framing.error());auto const& e=framing->envelope;
        if(!e.ack||!e.command)return std::unexpected(Error{ErrorCode::invalid_argument});
        TransactionKey key{binding_generation,peer,e.stream_id,e.command->controller,e.command->controllee,e.command->message_id};
        for(std::size_t i=0;i<Records;++i){auto& r=storage_->records[i];if(!r.used||!same_key(key,r.key))continue;
            if(e.class_id!=r.request.class_id)return std::unexpected(Error{ErrorCode::identity_conflict});
            if(e.cancel&&!r.cancel_registered)return std::unexpected(Error{ErrorCode::invalid_state});
            const auto original=e.cancel?r.cancel_cam:r.request.command->cam;
            if((e.command->cam&0xffe00000u)!=(original&0xffe00000u))return std::unexpected(Error{ErrorCode::identity_conflict});
            const auto phase=static_cast<std::uint8_t>((e.command->cam>>18)&7);
            if(!(phase&requested_responses(original)))return std::unexpected(Error{ErrorCode::invalid_argument});
            auto parsed=codec::decode_packet(wire,codec::DecodeOptions{codec::RequestContext{original}});if(!parsed)return std::unexpected(parsed.error());
            auto& active=e.cancel?r.cancel_active:r.ordinary_active;const auto limit=e.cancel?r.cancel_deadline:r.deadline;
            if(active&&now>=limit){auto done=terminal(r,e.cancel,limit);if(!done)return std::unexpected(done.error());if(e.cancel)r.observer.cancellation_timeout();else r.observer.timeout();}
            std::optional<StateObservation> state;
            if(e.command->cam&(1u<<18)){auto captured=capture_state(*parsed,r,e.cancel);if(!captured)return std::unexpected(captured.error());state=*captured;
                auto& existing=e.cancel?r.cancel_state:r.ordinary_state;if(existing&&!same_state(*existing,*state))return std::unexpected(Error{ErrorCode::identity_conflict});}
            auto observed=r.observer.receive(*parsed);if(!observed)return std::unexpected(observed.error());
            if(state){auto& existing=e.cancel?r.cancel_state:r.ordinary_state;if(!existing)existing=*state;}
            auto& seen=e.cancel?r.seen_cancel:r.seen_ordinary;seen|=static_cast<std::uint8_t>((e.command->cam>>18)&7);
            auto expected=requested_responses(original);
            if((seen&expected)==expected){auto done=terminal(r,e.cancel,now);if(!done)return std::unexpected(done.error());}
            return ControllerHandle{i,r.generation};
        }return std::unexpected(Error{ErrorCode::stale_generation});
    }
    Result<void> advance(timing::MonoTime now) noexcept {
        for(auto& r:storage_->records)if(r.used){
            if(r.ordinary_active&&now>=r.deadline){auto done=terminal(r,false,r.deadline);if(!done)return done;r.observer.timeout();}
            if(r.cancel_active&&now>=r.cancel_deadline){auto done=terminal(r,true,r.cancel_deadline);if(!done)return done;r.observer.cancellation_timeout();}
        }
        return {};
    }
    Result<void> mark_terminal(ControllerHandle handle,bool cancellation,timing::MonoTime now) noexcept {auto* r=find(handle);if(!r)return std::unexpected(Error{ErrorCode::stale_generation});if(cancellation&&!r->cancel_registered)return std::unexpected(Error{ErrorCode::invalid_state});return terminal(*r,cancellation,now);}
    Result<const ControllerObserver*> observer(ControllerHandle handle) const noexcept {auto* r=find(handle);if(!r)return std::unexpected(Error{ErrorCode::stale_generation});return &r->observer;}
    Result<std::optional<StateObservation>> state_observation(ControllerHandle handle,bool cancellation=false) const noexcept {auto* r=find(handle);if(!r)return std::unexpected(Error{ErrorCode::stale_generation});return cancellation?r->cancel_state:r->ordinary_state;}
    Result<timing::MonoTime> deadline_for(ControllerHandle handle,bool cancellation=false) const noexcept {auto* r=find(handle);if(!r)return std::unexpected(Error{ErrorCode::stale_generation});if(cancellation&&!r->cancel_registered)return std::unexpected(Error{ErrorCode::invalid_state});return cancellation?r->cancel_deadline:r->deadline;}
    Result<void> retain(ControllerHandle handle) noexcept {auto* r=find(handle);if(!r)return std::unexpected(Error{ErrorCode::stale_generation});if(r->references==SIZE_MAX)return std::unexpected(Error{ErrorCode::overflow});++r->references;return {};}
    Result<void> release(ControllerHandle handle) noexcept {auto* r=find(handle);if(!r||!r->references)return std::unexpected(Error{ErrorCode::stale_generation});--r->references;return {};}
    void expire(timing::MonoTime now) noexcept {for(auto& r:storage_->records)if(r.used&&!r.ordinary_active&&!r.cancel_active&&!r.references&&now>=r.expires){auto generation=r.generation;r=Record{};r.generation=generation+1;}}
    std::size_t size() const noexcept {std::size_t count=0;for(auto const& r:storage_->records)count+=r.used;return count;}
};
static_assert(ControllerRegistry<>::record_bytes()<=2048);
} // namespace vita::runtime::transaction
