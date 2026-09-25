#pragma once
#include <vita/runtime/transaction/cam.hpp>
#include <vita/codec/packet.hpp>
#include <vita/profiles/iq/sdr.hpp>
namespace vita::runtime::transaction {
struct AckRecord {
    codec::Envelope request{};Cam cam{};AckKind kind=AckKind::validation;
    std::array<Diagnostics,state_field_capacity> diagnostics{};
    StateSnapshot state{};std::uint8_t selected_mask=0;
    bool partial=false,scheduled_or_executed=false,hypothetical=false,time_known=false;
    unsigned timing=0;timing::ProtocolTime time{};codec::Tsi epoch=codec::Tsi::none;bool cancellation=false;
    const profiles::iq::SdrCapabilities* sdr_capabilities=nullptr;
};
inline Diagnostics summary(const AckRecord& ack) noexcept {Diagnostics result;for(auto d:ack.diagnostics){result.warnings|=d.warnings;result.errors|=d.errors;}return result;}
inline Result<std::size_t> encode_response(const AckRecord& ack,MutableBytes output,unsigned outgoing_packet_count) noexcept {
    if(outgoing_packet_count>15)return std::unexpected(Error{ErrorCode::invalid_argument});
    auto envelope=ack.request;envelope.ack=true;envelope.cancel=ack.cancellation;envelope.packet_count=static_cast<std::uint8_t>(outgoing_packet_count);
    if(!envelope.command || (ack.cancellation && ack.kind==AckKind::validation))return std::unexpected(Error{ErrorCode::invalid_argument});
    const auto sum=summary(ack);
    auto cam=(ack.cam.raw&0xffe00000u)|(ack.kind==AckKind::validation?1u<<20:ack.kind==AckKind::execution?1u<<19:1u<<18);
    cam|=ack.partial?1u<<11:0;cam|=ack.scheduled_or_executed?1u<<10:0;const auto wire_timing=(ack.request.timestamp.tsi==codec::Tsi::none&&ack.request.timestamp.tsf==codec::Tsf::none)?0u:ack.timing;cam|=wire_timing<<12;
    if(ack.kind!=AckKind::state){cam|=sum.warnings?1u<<17:0;cam|=sum.errors?1u<<16:0;}
    envelope.command->cam=cam;
    if(ack.timing>7 || ack.timing==5 || ack.timing==6 || static_cast<unsigned>(ack.epoch)>3)
        return std::unexpected(Error{ErrorCode::invalid_argument});
    if(ack.time_known){
        if(!timing::valid(ack.time))return std::unexpected(Error{ErrorCode::invalid_argument});
        if(ack.time.seconds>UINT32_MAX)return std::unexpected(Error{ErrorCode::overflow});
        if(ack.epoch==codec::Tsi::none)return std::unexpected(Error{ErrorCode::invalid_state});
        envelope.timestamp={ack.epoch,codec::Tsf::picoseconds,static_cast<std::uint32_t>(ack.time.seconds),ack.time.picoseconds};
    }else{envelope.timestamp={};}
    if(ack.kind==AckKind::state) {
        StateAck state;auto configured=state.configure(envelope.class_id?envelope.class_id->packet_class:0,ack.cam.action);if(!configured)return std::unexpected(configured.error());
        if(ack.sdr_capabilities){std::array<FieldId,4> selectors{};std::size_t count=0;for(std::size_t i=0;i<state_field_capacity;++i)if(ack.selected_mask&(1u<<i))selectors[count++]=state_fields[i];auto populated=profiles::iq::populate_sdr_capability_response(state,*ack.sdr_capabilities,std::span<const FieldId>{selectors}.first(count));if(!populated)return std::unexpected(populated.error());return codec::encode_packet(envelope,state.freeze(),output);}
        for(std::size_t i=0;i<state_field_capacity;++i)if((ack.selected_mask&(1u<<i))&&ack.state.fields[i].validity==Validity::known){auto added=state.set_value(ack.state.fields[i].id,ack.state.fields[i].value);if(!added)return std::unexpected(added.error());}
        return codec::encode_packet(envelope,state.freeze(),output);
    }
    DiagnosticAck warnings,errors;
    for(std::size_t i=0;i<state_field_capacity;++i) {
        if(ack.cam.detail_warning&&ack.diagnostics[i].warnings){auto added=warnings.diagnostic(state_fields[i],ack.diagnostics[i].warnings);if(!added)return std::unexpected(added.error());}
        if(ack.cam.detail_error&&ack.diagnostics[i].errors){auto added=errors.diagnostic(state_fields[i],ack.diagnostics[i].errors);if(!added)return std::unexpected(added.error());}
    }
    return codec::encode_diagnostic(envelope,warnings.freeze(),errors.freeze(),codec::RequestContext{ack.cam.raw},output);
}
// Isolated codec tests may use zero; transport integration supplies its sender/SID/type counter.
inline Result<std::size_t> encode_response(const AckRecord& ack,MutableBytes output) noexcept {return encode_response(ack,output,0);}
enum class ObservationKind { unconfirmed,local_send,local_failure,validation,execution,state,timeout,late_response,cancellation_execution,cancellation_state,cancellation_timeout };
struct Observation {
    ObservationKind kind=ObservationKind::unconfirmed;
    bool hypothetical=false,partial=false,success=false,unknown_remote_outcome=true;
    ObservationKind response_kind=ObservationKind::unconfirmed;
    bool confirms_execution=false,contradictory=false;
    bool cancellation=false,confirms_cancellation=false,cancellation_outcome_known=false;
    // AckV admission evidence is distinct from actual execution, including
    // when the response arrives after the local deadline. Accepted means whole,
    // nonpartial admission; false does not prove that no subset had effects.
    bool validation_outcome_known=false,validation_accepted=false;
    friend bool operator==(const Observation&,const Observation&)=default;
};
class ControllerObserver {
    std::uint32_t message_id_=0;
    bool timed_out_=false,cancellation_timed_out_=false;
    Observation current_{};
    // One bounded slot per phase, including distinct late V/X/S and a retained timeout.
    std::array<Observation,16> history_{};
    std::size_t count_=0;
    void record(Observation event) noexcept {
        for(std::size_t i=0;i<count_;++i)if(history_[i].kind==event.kind && history_[i].response_kind==event.response_kind) {
            if(history_[i]!=event) { history_[i].contradictory=true;history_[i].success=false;history_[i].unknown_remote_outcome=true;history_[i].confirms_execution=false;history_[i].confirms_cancellation=false;history_[i].cancellation_outcome_known=false;history_[i].validation_outcome_known=false;history_[i].validation_accepted=false; }
            current_=history_[i];return;
        }
        // The fixed phase-key space is smaller than capacity; no input can add arbitrary keys.
        history_[count_++]=event;current_=event;
    }
public:
    explicit ControllerObserver(std::uint32_t message_id) noexcept:message_id_(message_id){}
    void local_send(bool succeeded) noexcept {record({succeeded?ObservationKind::local_send:ObservationKind::local_failure,false,false,false,true});}
    void timeout() noexcept {if(!timed_out_){timed_out_=true;record({ObservationKind::timeout,false,false,false,true});}}
    void cancellation_timeout() noexcept {if(!cancellation_timed_out_){cancellation_timed_out_=true;Observation event;event.kind=ObservationKind::cancellation_timeout;event.cancellation=true;record(event);}}
    Result<void> receive(const codec::PacketView& response) noexcept {
        const auto& envelope=response.envelope.envelope;
        if(!envelope.ack||!envelope.command||envelope.command->message_id!=message_id_||response.requires_request_context||response.opaque)
            return std::unexpected(Error{ErrorCode::invalid_argument});
        auto valid=codec::validate_cam(envelope);if(!valid)return std::unexpected(valid.error());
        const auto cam=envelope.command->cam;
        const auto action=(cam>>23)&3;
        const bool simulated=action==1,partial=cam&(1u<<11),executed_flag=cam&(1u<<10);
        if(envelope.cancel && ((cam&(1u<<20)) || action!=2))return std::unexpected(Error{ErrorCode::invalid_argument});
        const auto kind=envelope.cancel ? ((cam&(1u<<19))?ObservationKind::cancellation_execution:ObservationKind::cancellation_state) :
            (cam&(1u<<20))?ObservationKind::validation:(cam&(1u<<19))?ObservationKind::execution:ObservationKind::state;
        bool indeterminate=false,validation_error=bool(cam&(1u<<16));
        for(std::size_t i=0;i<response.fields.size();++i)if(response.fields[i].kind==BodyKind::diagnostics) {
            auto diagnostic=response.fields[i].diagnostic();if(!diagnostic)return std::unexpected(diagnostic.error());
            indeterminate|=bool(*diagnostic&state_indeterminate);
            validation_error|=response.fields[i].group==codec::DiagnosticGroup::error&&*diagnostic!=0;
        }
        const bool confirmed=kind==ObservationKind::execution && action==2 && !partial && executed_flag && !indeterminate && ((cam>>12)&7)!=7;
        const bool late=envelope.cancel?cancellation_timed_out_:timed_out_;
        const bool cancel_known=kind==ObservationKind::cancellation_execution&&!indeterminate;
        record({late?ObservationKind::late_response:kind,simulated,partial,confirmed&&!late,!confirmed,kind,confirmed,false,
            envelope.cancel,cancel_known&&executed_flag,cancel_known,
            kind==ObservationKind::validation,
            kind==ObservationKind::validation&&executed_flag&&!partial&&!validation_error&&!indeterminate&&((cam>>12)&7)!=7});
        return {};
    }
    Observation observation() const noexcept{return current_;}
    std::span<const Observation> observations() const noexcept {return {history_.data(),count_};}
    std::optional<Observation> timeout_observation() const noexcept {
        for(std::size_t i=0;i<count_;++i)if(history_[i].kind==ObservationKind::timeout)return history_[i];return {};
    }
    bool timed_out() const noexcept{return timed_out_;}
    bool cancellation_timed_out() const noexcept{return cancellation_timed_out_;}
};
} // namespace vita::runtime::transaction
