#pragma once
#include <vita/runtime/context/publisher.hpp>
namespace vita::runtime::context {
enum class Confidence : std::uint8_t { known,missing,stale,ambiguous,incomplete_delta };
struct MetadataSnapshot {
    StateSnapshot state{};timing::ProtocolTime effective{};Confidence confidence=Confidence::missing;
    std::uint32_t events=0;bool valid_data=false;
};
inline bool same_snapshot(const StateSnapshot& a,const StateSnapshot& b) noexcept {
    if(a.profile!=b.profile)return false;
    for(std::size_t i=0;i<active_state_fields(a.profile);++i)if(a.fields[i].id!=b.fields[i].id||a.fields[i].validity!=b.fields[i].validity||(a.fields[i].validity==Validity::known&&a.fields[i].value!=b.fields[i].value))return false;return true;
}
template<std::size_t Capacity=128> class ReceiverHistory {
    struct Entry {StateSnapshot state{};timing::ProtocolTime effective{};timing::MonoTime arrival{};std::uint32_t events{};bool full=true,conflict=false;};
    std::array<Entry,Capacity> entries_{};std::size_t count_=0;
    profiles::iq::Profile profile_=profiles::iq::Profile::generator_v1;
    std::uint64_t generation_=1;codec::Tsi epoch_=codec::Tsi::none;std::optional<std::uint32_t> reference_{};
public:
    explicit ReceiverHistory(std::uint64_t generation=1,codec::Tsi epoch=codec::Tsi::none,std::optional<std::uint32_t> reference={},profiles::iq::Profile profile=profiles::iq::Profile::generator_v1) noexcept:profile_(profile),generation_(generation),epoch_(epoch),reference_(reference){}
    Result<void> insert(StateSnapshot state,timing::ProtocolTime effective,timing::MonoTime arrival,bool full=true,std::uint32_t events=0) noexcept {
        auto snapshot_valid=validate_snapshot(state);if(!snapshot_valid)return snapshot_valid;
        if(state.profile!=profile_)return std::unexpected(Error{ErrorCode::identity_conflict});
        if(!timing::valid(effective))return std::unexpected(Error{ErrorCode::invalid_argument});
        for(std::size_t i=0;i<active_state_fields(state.profile);++i){const auto& field=state.fields[i];if(field.id!=state_fields[i])return std::unexpected(Error{ErrorCode::invalid_argument});if(field.validity==Validity::known){auto valid=validate_value(field.id,field.value);if(!valid)return std::unexpected(valid.error());}}
        if(state.fields[2].validity==Validity::known){auto* indicators=std::get_if<std::uint32_t>(&state.fields[2].value);if(!indicators)return std::unexpected(Error{ErrorCode::invalid_argument});events|=*indicators&(sample_loss_enable|sample_loss_indicator);*indicators&=~(sample_loss_enable|sample_loss_indicator);}
        std::size_t index=0;while(index<count_&&entries_[index].effective<effective)++index;
        if(index<count_&&entries_[index].effective==effective){auto& existing=entries_[index];if(existing.full!=full||!same_snapshot(existing.state,state)||existing.events!=events)existing.conflict=true;return {};}
        if(count_==Capacity){if(index==0)return std::unexpected(Error{ErrorCode::capacity_exhausted});for(std::size_t i=1;i<count_;++i)entries_[i-1]=entries_[i];--count_;--index;}
        for(std::size_t i=count_;i>index;--i)entries_[i]=entries_[i-1];entries_[index]={state,effective,arrival,events,full,false};++count_;return {};
    }
    Result<void> receive(const codec::PacketView& packet,std::uint64_t generation,timing::MonoTime arrival,bool full=true) noexcept {
        auto const& envelope=packet.envelope.envelope;
        if(generation!=generation_)return std::unexpected(Error{ErrorCode::stale_generation});
        if(envelope.type!=codec::PacketType::context||packet.opaque||envelope.tsm||envelope.timestamp.tsi==codec::Tsi::none||envelope.timestamp.tsf!=codec::Tsf::picoseconds||(epoch_!=codec::Tsi::none&&envelope.timestamp.tsi!=epoch_))return std::unexpected(Error{ErrorCode::unsupported_capability});
        StateSnapshot snapshot;snapshot.profile=profile_;
        for(std::size_t i=0;i<packet.fields.size();++i){const auto& field=packet.fields[i];auto index=field_index(field.id);if(!profile_field(profile_,field.id)||index==state_field_capacity||field.kind!=BodyKind::values||field.attribute!=Attribute::current)return std::unexpected(Error{ErrorCode::unsupported_capability});auto value=field.value();if(!value)return std::unexpected(value.error());snapshot.fields[index]={field.id,*value,Validity::known};}
        if(profile_==profiles::iq::Profile::graphx_radio){
            if(envelope.class_id||envelope.timestamp.tsi!=codec::Tsi::utc||packet.fields.size()!=4)
                return std::unexpected(Error{ErrorCode::invalid_argument});
            for(auto i:{1u,4u,5u,6u})if(snapshot.fields[i].validity!=Validity::known)
                return std::unexpected(Error{ErrorCode::invalid_argument});
            snapshot.fields[0]={ReferencePoint::id,reference_.value_or(*envelope.stream_id),Validity::known};
            snapshot.fields[2]={StateEvent::id,valid_data_enable|valid_data_indicator,Validity::known};
            snapshot.fields[3]={DataPayloadFormat::id,PayloadFormat{0x200003cf00000000},Validity::known};
        }
        if(reference_){auto* reference=std::get_if<std::uint32_t>(&snapshot.fields[0].value);if(snapshot.fields[0].validity!=Validity::known||!reference||*reference!=*reference_)return std::unexpected(Error{ErrorCode::identity_conflict});}
        return insert(snapshot,{envelope.timestamp.integer,envelope.timestamp.fractional},arrival,full);
    }
    MetadataSnapshot resolve(timing::ProtocolTime sample,timing::MonoTime now) const noexcept {
        MetadataSnapshot result;const Entry* last=nullptr;bool anchored=false,ambiguous=false,delta=false;
        for(std::size_t i=0;i<count_&&entries_[i].effective<=sample;++i){const auto& entry=entries_[i];
            if(entry.full){result.state=entry.state;anchored=true;ambiguous=entry.conflict;delta=false;}
            else {for(std::size_t f=0;f<state_field_capacity;++f)if(entry.state.fields[f].validity!=Validity::absent)result.state.fields[f]=entry.state.fields[f];ambiguous|=entry.conflict;delta=true;}
            last=&entry;
        }
        if(!last)return result;
        result.effective=last->effective;result.events=sample==last->effective?last->events:0;
        if(ambiguous){result.confidence=Confidence::ambiguous;return result;}
        if(now.ns<last->arrival.ns||now.ns-last->arrival.ns>=2000000000ULL){result.confidence=Confidence::stale;return result;}
        auto end=timing::add(last->effective,timing::Duration{2,0});if(end&&sample>=*end){result.confidence=Confidence::stale;return result;}
        if(!anchored||!required_known(result.state)){result.confidence=Confidence::missing;return result;}
        if(result.state.fields[2].validity==Validity::known){auto* value=std::get_if<std::uint32_t>(&result.state.fields[2].value);result.valid_data=value&&(*value&valid_data_enable)&&(*value&valid_data_indicator);}
        result.confidence=delta?Confidence::incomplete_delta:Confidence::known;return result;
    }
    void expire(timing::MonoTime now) noexcept {std::size_t keep=0;for(std::size_t i=0;i<count_;++i)if(now.ns<entries_[i].arrival.ns||now.ns-entries_[i].arrival.ns<2000000000ULL)entries_[keep++]=entries_[i];count_=keep;}
    Result<void> detach(std::uint64_t generation,codec::Tsi epoch,std::optional<std::uint32_t> reference={}) noexcept {if(generation<=generation_)return std::unexpected(Error{ErrorCode::stale_generation});count_=0;generation_=generation;epoch_=epoch;reference_=reference;return {};}
    std::uint64_t generation() const noexcept{return generation_;}
    std::size_t size() const noexcept{return count_;}
    static constexpr std::size_t entry_bytes() noexcept{return sizeof(Entry);}
    static constexpr std::size_t storage_bytes() noexcept{return sizeof(ReceiverHistory);}
};
class BorrowedSignalRx {
    const memory::RxEnvelope* envelope_=nullptr;const memory::RetainedRx* retained_=nullptr;
public:
    const MetadataSnapshot& metadata;timing::ProtocolTime sample_time;
    BorrowedSignalRx(const memory::RxEnvelope& envelope,const MetadataSnapshot& state,timing::ProtocolTime time) noexcept:envelope_(&envelope),metadata(state),sample_time(time){}
    BorrowedSignalRx(const memory::RetainedRx& retained,const MetadataSnapshot& state,timing::ProtocolTime time) noexcept:retained_(&retained),metadata(state),sample_time(time){}
    BorrowedSignalRx(const BorrowedSignalRx&)=delete;BorrowedSignalRx& operator=(const BorrowedSignalRx&)=delete;
    std::size_t fragment_count() const noexcept{return envelope_?envelope_->fragment_count():retained_->fragment_count();}
    Result<Bytes> fragment(std::size_t i) const noexcept{return envelope_?envelope_->fragment(i):retained_->fragment(i);}
    Result<memory::RetainedRx> retain(memory::RetentionQuota& consumer,memory::RetentionQuota& global) const noexcept{return envelope_?envelope_->retain(consumer,global):retained_->retain(consumer,global);}
};
struct ReceiverBinding {
    void* context=nullptr;
    void (*deliver)(void*,const BorrowedSignalRx&) noexcept=nullptr;
    void (*drop)(void*,Confidence) noexcept=nullptr;
};
template<std::size_t History=128,std::size_t Waiting=64> class ContextReceiver {
    struct Pending {memory::RetainedRx payload;timing::ProtocolTime sample;timing::MonoTime arrival;};
    ReceiverHistory<History> history_;ReceiverBinding binding_;std::array<std::optional<Pending>,Waiting> waiting_{};
    memory::RetentionQuota local_quota_{Waiting};memory::RetentionQuota& global_quota_;
    bool allow_unknown_=false;std::optional<PayloadFormat> fixed_format_{};std::size_t waiting_count_=0;
    bool deliverable(const MetadataSnapshot& metadata) const noexcept{return metadata.confidence==Confidence::known&&metadata.valid_data;}
public:
    ContextReceiver(memory::RetentionQuota& global,ReceiverBinding binding,std::uint64_t generation=1,codec::Tsi epoch=codec::Tsi::none,bool allow_unknown=false,std::optional<std::uint32_t> reference={},std::optional<PayloadFormat> fixed_format={},profiles::iq::Profile profile=profiles::iq::Profile::generator_v1)
      :history_(generation,epoch,reference,profile),binding_(binding),global_quota_(global),allow_unknown_(allow_unknown),fixed_format_(fixed_format){}
    ReceiverHistory<History>& history() noexcept{return history_;}
    Result<void> receive_data(const memory::RxEnvelope& envelope,timing::ProtocolTime sample,std::uint64_t generation,timing::MonoTime now) noexcept {
        if(generation!=history_.generation())return std::unexpected(Error{ErrorCode::stale_generation});
        if(!binding_.deliver||!timing::valid(sample)||(allow_unknown_&&!fixed_format_))return std::unexpected(Error{ErrorCode::invalid_argument});
        auto metadata=history_.resolve(sample,now);
        if(deliverable(metadata)){BorrowedSignalRx borrowed(envelope,metadata,sample);binding_.deliver(binding_.context,borrowed);return {};}
        if(allow_unknown_&&metadata.confidence!=Confidence::known){for(auto& field:metadata.state.fields)if(field.validity==Validity::known)field.validity=Validity::unknown;metadata.state.fields[3]={DataPayloadFormat::id,*fixed_format_,Validity::known};BorrowedSignalRx borrowed(envelope,metadata,sample);binding_.deliver(binding_.context,borrowed);return {};}
        auto retained=envelope.retain(local_quota_,global_quota_);if(!retained)return std::unexpected(retained.error());
        for(auto& slot:waiting_)if(!slot){slot.emplace(Pending{std::move(*retained),sample,now});++waiting_count_;return {};}
        if(binding_.drop)binding_.drop(binding_.context,metadata.confidence);return std::unexpected(Error{ErrorCode::capacity_exhausted});
    }
    Result<void> receive_context(const codec::PacketView& packet,std::uint64_t generation,timing::MonoTime now,bool full=true) noexcept {auto inserted=history_.receive(packet,generation,now,full);if(!inserted)return inserted;progress(now);return {};}
    void progress(timing::MonoTime now) noexcept {
        for(auto& slot:waiting_)if(slot){auto metadata=history_.resolve(slot->sample,now);
            if(now.ns>=slot->arrival.ns&&now.ns-slot->arrival.ns>=10000000ULL){if(binding_.drop)binding_.drop(binding_.context,metadata.confidence);slot.reset();--waiting_count_;}
            else if(deliverable(metadata)){BorrowedSignalRx borrowed(slot->payload,metadata,slot->sample);binding_.deliver(binding_.context,borrowed);slot.reset();--waiting_count_;}
        }
        history_.expire(now);
    }
    Result<void> detach(std::uint64_t generation,codec::Tsi epoch,std::optional<std::uint32_t> reference={}) noexcept {auto changed=history_.detach(generation,epoch,reference);if(!changed)return changed;for(auto& slot:waiting_)slot.reset();waiting_count_=0;return {};}
    std::size_t waiting() const noexcept{return waiting_count_;}
    static constexpr std::size_t storage_bytes() noexcept{return sizeof(ContextReceiver);}
};
} // namespace vita::runtime::context
