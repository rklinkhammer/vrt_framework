#pragma once
#include <vita/runtime/context/revisions.hpp>
#include <vita/codec/packet.hpp>
#include <vita/memory/envelope.hpp>
namespace vita::runtime::context {
inline constexpr std::uint32_t valid_data_enable=1u<<30,valid_data_indicator=1u<<18;
inline constexpr std::uint32_t sample_loss_enable=1u<<24,sample_loss_indicator=1u<<12;
struct ContextFrame {
    StateSnapshot state{};timing::ProtocolTime time{};codec::Tsi epoch=codec::Tsi::none;
    bool time_known=false,change=true,refresh=false,valid=false;
    std::uint64_t revision_id=0,association_generation=0;bool observation=false;
};
inline Result<std::size_t> encode_context(const ContextFrame& frame,codec::Envelope envelope,MutableBytes output) noexcept {
    auto snapshot_valid=validate_snapshot(frame.state);if(!snapshot_valid)return std::unexpected(snapshot_valid.error());
    if(envelope.type!=codec::PacketType::context||!envelope.stream_id||envelope.trailer)return std::unexpected(Error{ErrorCode::invalid_argument});
    if(frame.time_known){if(!timing::valid(frame.time)||frame.time.seconds>UINT32_MAX||frame.epoch==codec::Tsi::none)return std::unexpected(Error{ErrorCode::invalid_argument});envelope.timestamp={frame.epoch,codec::Tsf::picoseconds,static_cast<std::uint32_t>(frame.time.seconds),frame.time.picoseconds};}
    else envelope.timestamp={};
    ContextPacket packet;
    if(frame.state.profile==profiles::iq::Profile::graphx_radio){
        if(envelope.class_id||frame.epoch!=codec::Tsi::utc||!frame.time_known||!frame.valid)
            return std::unexpected(Error{ErrorCode::invalid_argument});
        for(auto i:{5u,4u,6u,1u}){
            if(frame.state.fields[i].validity!=Validity::known)return std::unexpected(Error{ErrorCode::invalid_state});
            auto set=packet.set_value(frame.state.fields[i].id,frame.state.fields[i].value);if(!set)return std::unexpected(set.error());
        }
        return codec::encode_packet(envelope,packet.freeze(),output,frame.change);
    }
    for(std::size_t i=0;i<active_state_fields(frame.state.profile);++i)if(i!=2&&frame.state.fields[i].validity==Validity::known){auto set=packet.set_value(frame.state.fields[i].id,frame.state.fields[i].value);if(!set)return std::unexpected(set.error());}
    std::uint32_t indicators=0;
    if(frame.state.fields[2].validity==Validity::known){const auto* value=std::get_if<std::uint32_t>(&frame.state.fields[2].value);if(!value)return std::unexpected(Error{ErrorCode::invalid_argument});indicators=*value;}
    if(frame.refresh)indicators&=~(sample_loss_enable|sample_loss_indicator);
    indicators|=valid_data_enable;if(frame.valid)indicators|=valid_data_indicator;else indicators&=~valid_data_indicator;
    auto set=packet.set<StateEvent>(indicators);if(!set)return std::unexpected(set.error());
    return codec::encode_packet(envelope,packet.freeze(),output,frame.change);
}
struct PublisherBinding {
    void* context=nullptr;
    // Acceptance means bytes have been copied into owned transport storage and ordered before Data.
    Result<void> (*context_send)(void*,const ContextFrame&) noexcept=nullptr;
    // Consume storage only on success. Rejection preserves the caller's ownership.
    Result<void> (*data_send)(void*,memory::TxStorage&,const RevisionHandle&) noexcept=nullptr;
};
enum class StreamStatus { idle,active,context_unavailable,metadata_unknown,backend_fault,temporal_association };
template<std::size_t Revisions=128,std::size_t Held=64> class ContextPublisher {
    struct Pending {memory::TxStorage storage;RevisionHandle revision;};
    RevisionStore<Revisions>& store_;PublisherBinding binding_;
    std::array<std::optional<Pending>,Held> held_{};std::size_t held_count_=0;
    StreamStatus status_=StreamStatus::idle;std::optional<timing::MonoTime> blocked_since_{};
    timing::MonoTime refresh_at_{};bool refresh_started_=false,invalid_sent_=false,start_gate_=true,starting_run_=true,calibrated_=false;
    std::optional<timing::ProtocolTime> context_highwater_{};
    std::optional<StateSnapshot> fault_snapshot_{},highwater_state_{};
    void stop(StreamStatus status) noexcept {status_=status;for(auto& held:held_)held.reset();held_count_=0;}
    ContextFrame frame(const RevisionHandle& revision,codec::Tsi epoch,bool refresh,timing::ProtocolTime now={}) const noexcept {
        auto const& event=revision.event();ContextFrame result{event.state,refresh?now:event.actual_time,epoch,refresh||event.time_known,!refresh,refresh,required_known(event.state),revision.id(),event.association_generation};
        std::uint32_t indicators=0;if(auto* value=std::get_if<std::uint32_t>(&result.state.fields[2].value);value&&result.state.fields[2].validity==Validity::known)indicators=*value;
        indicators|=(1u<<31)|valid_data_enable;if(calibrated_)indicators|=1u<<19;else indicators&=~(1u<<19);
        if(result.valid)indicators|=valid_data_indicator;else indicators&=~valid_data_indicator;
        if(refresh)indicators&=~(sample_loss_enable|sample_loss_indicator);
        result.state.fields[2]={StateEvent::id,indicators,Validity::known};return result;
    }
public:
    ContextPublisher(RevisionStore<Revisions>& store,PublisherBinding binding) noexcept:store_(store),binding_(binding){}
    Result<void> start(timing::MonoTime now) noexcept {
        auto current=store_.current();if(!current||store_.faulted()||!required_known(current->event().state))return std::unexpected(Error{ErrorCode::invalid_state});
        if(!binding_.context_send||!binding_.data_send||status_!=StreamStatus::idle)return std::unexpected(Error{ErrorCode::invalid_state});
        status_=StreamStatus::active;refresh_at_=now;refresh_started_=false;start_gate_=starting_run_=true;blocked_since_=now;return {};
    }
    void backend_fault(const StateSnapshot& observed) noexcept {fault_snapshot_=observed;store_.fault();stop(StreamStatus::backend_fault);}
    void temporal_fault() noexcept {stop(StreamStatus::temporal_association);}
    std::optional<timing::ProtocolTime> published_highwater() const noexcept {return context_highwater_;}
    Result<void> submit(memory::TxStorage&& storage,RevisionHandle revision,timing::MonoTime now) noexcept {
        if(status_!=StreamStatus::active||!revision||!store_.owns(revision)||revision.event().association_generation!=store_.generation()||store_.faulted()||!required_known(revision.event().state))return std::unexpected(Error{ErrorCode::invalid_state});
        auto dependency=store_.note_data_dependency(revision);if(!dependency)return dependency;
        if(!start_gate_&&revision.publication()==Publication::accepted){auto sent=binding_.data_send(binding_.context,storage,revision);if(sent)return {};return std::unexpected(sent.error());}
        if(!blocked_since_)blocked_since_=now;
        if(held_count_==Held){stop(StreamStatus::context_unavailable);return std::unexpected(Error{ErrorCode::capacity_exhausted});}
        for(auto& slot:held_)if(!slot){slot.emplace(Pending{std::move(storage),std::move(revision)});++held_count_;return {};}
        return std::unexpected(Error{ErrorCode::capacity_exhausted});
    }
    Result<void> progress(timing::MonoTime now,timing::ProtocolTime protocol_now,codec::Tsi epoch,bool clock_usable=false,bool calibrated=false) noexcept {
        calibrated_=calibrated&&clock_usable;
        if(!timing::valid(protocol_now)||epoch==codec::Tsi::none||static_cast<unsigned>(epoch)>3)return std::unexpected(Error{ErrorCode::invalid_argument});
        if(!binding_.context_send||!binding_.data_send)return std::unexpected(Error{ErrorCode::invalid_state});
        if(store_.faulted()&&status_==StreamStatus::active)stop(StreamStatus::metadata_unknown);
        if(status_==StreamStatus::metadata_unknown||status_==StreamStatus::backend_fault){
            if(!invalid_sent_&&clock_usable){if(context_highwater_&&protocol_now<=*context_highwater_)return std::unexpected(Error{ErrorCode::identity_conflict});auto latest=store_.current();if(latest){auto invalid=frame(*latest,epoch,false);if(fault_snapshot_)invalid.state=*fault_snapshot_;invalid.valid=false;invalid.refresh=true;
                std::uint32_t indicators=0;if(auto* value=std::get_if<std::uint32_t>(&invalid.state.fields[2].value);value&&invalid.state.fields[2].validity==Validity::known)indicators=*value;
                indicators=(indicators&~(valid_data_indicator|sample_loss_enable|sample_loss_indicator|(1u<<19)))|valid_data_enable|(1u<<31);if(calibrated_)indicators|=1u<<19;
                invalid.state.fields[2]={StateEvent::id,indicators,Validity::known};invalid.time=protocol_now;invalid.time_known=true;invalid.observation=true;auto sent=binding_.context_send(binding_.context,invalid);invalid_sent_=bool(sent);if(sent){context_highwater_=protocol_now;highwater_state_=invalid.state;}}}
            return std::unexpected(Error{ErrorCode::invalid_state});
        }
        if(!clock_usable)return std::unexpected(Error{ErrorCode::invalid_state});
        if(status_!=StreamStatus::active)return std::unexpected(Error{ErrorCode::invalid_state});
        if(blocked_since_&&now.ns>=blocked_since_->ns&&now.ns-blocked_since_->ns>=10000000ULL){stop(StreamStatus::context_unavailable);return std::unexpected(Error{ErrorCode::capacity_exhausted});}
        for(std::size_t i=0;i<Revisions;++i){auto next=store_.publication_group();if(!next){if(next.error().code==ErrorCode::identity_conflict){temporal_fault();return std::unexpected(next.error());}break;}
            if(!blocked_since_)blocked_since_=now;
            auto outgoing=frame(*next,epoch,false);
            if(!outgoing.time_known){if(!starting_run_){temporal_fault();return std::unexpected(Error{ErrorCode::invalid_state});}auto latest=store_.current();if(!latest)return std::unexpected(latest.error());outgoing=frame(*latest,epoch,true,protocol_now);outgoing.change=true;outgoing.observation=true;}
            if(outgoing.time_known&&context_highwater_&&outgoing.time<=*context_highwater_&&(outgoing.time<*context_highwater_||!highwater_state_||!same_state(outgoing.state,*highwater_state_))){temporal_fault();return std::unexpected(Error{ErrorCode::identity_conflict});}
            auto sent=binding_.context_send(binding_.context,outgoing);if(!sent)return std::unexpected(sent.error());
            if(outgoing.time_known&&(!context_highwater_||outgoing.time>*context_highwater_)){context_highwater_=outgoing.time;highwater_state_=outgoing.state;}
            auto marked=store_.accept_group(*next);if(!marked)return marked;
            refresh_at_=now;refresh_started_=true;
            if(start_gate_){auto current=store_.current();if(current&&outgoing.time_known&&outgoing.time==protocol_now&&outgoing.revision_id==current->id())start_gate_=false;}
        }
        if(start_gate_){
            auto latest=store_.current();if(!latest)return std::unexpected(latest.error());
            if(context_highwater_&&protocol_now<=*context_highwater_){temporal_fault();return std::unexpected(Error{ErrorCode::identity_conflict});}
            auto snapshot=frame(*latest,epoch,true,protocol_now);snapshot.change=true;snapshot.observation=true;
            auto sent=binding_.context_send(binding_.context,snapshot);if(!sent)return std::unexpected(sent.error());
            context_highwater_=protocol_now;highwater_state_=snapshot.state;start_gate_=false;refresh_started_=true;refresh_at_=now;
        }
        for(auto& slot:held_)if(slot&&slot->revision.publication()==Publication::accepted){auto sent=binding_.data_send(binding_.context,slot->storage,slot->revision);if(!sent)return std::unexpected(sent.error());slot.reset();--held_count_;}
        if(refresh_started_&&now.ns>=refresh_at_.ns&&now.ns-refresh_at_.ns>=1000000000ULL){if(context_highwater_&&protocol_now<=*context_highwater_){temporal_fault();return std::unexpected(Error{ErrorCode::identity_conflict});}auto latest=store_.current();if(!latest)return std::unexpected(latest.error());auto sent=binding_.context_send(binding_.context,frame(*latest,epoch,true,protocol_now));if(!sent){if(!blocked_since_)blocked_since_=now;return std::unexpected(sent.error());}refresh_at_=now;if(!context_highwater_||protocol_now>*context_highwater_){context_highwater_=protocol_now;highwater_state_=frame(*latest,epoch,true,protocol_now).state;}}
        blocked_since_.reset();
        starting_run_=false;store_.collect();return {};
    }
    Result<void> progress(timing::MonoTime now,const timing::ClockSnapshot& clock) noexcept {
        const bool usable=clock.state==timing::ClockState::locked||clock.state==timing::ClockState::holdover;
        return progress(now,clock.time,static_cast<codec::Tsi>(static_cast<unsigned>(clock.epoch)+1),usable,clock.calibrated&&clock.state==timing::ClockState::locked);
    }
    void pause() noexcept {if(status_==StreamStatus::active||status_==StreamStatus::context_unavailable){auto current=store_.current();if(current&&!store_.faulted()&&required_known(current->event().state)){stop(StreamStatus::idle);blocked_since_.reset();}}}
    Result<void> detach(std::uint64_t generation) noexcept {auto detached=store_.detach(generation);if(!detached)return detached;stop(StreamStatus::idle);blocked_since_.reset();refresh_started_=invalid_sent_=false;context_highwater_.reset();fault_snapshot_.reset();highwater_state_.reset();return {};}
    StreamStatus status() const noexcept{return status_;}
    std::size_t held() const noexcept{return held_count_;}
    static constexpr std::size_t storage_bytes() noexcept{return sizeof(ContextPublisher);}
};
} // namespace vita::runtime::context
