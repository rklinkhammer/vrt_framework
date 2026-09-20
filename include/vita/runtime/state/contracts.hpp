#pragma once
#include <vita/fields/types.hpp>
#include <vita/profiles/iq/profile.hpp>
#include <vita/runtime/timing/scheduling.hpp>
#include <array>
#include <memory>
#include <utility>
#include <vita/runtime/execution/admission.hpp>
namespace vita::runtime {
enum class Validity : std::uint8_t { absent,known,unknown };
struct FieldState { FieldId id{};SemanticValue value{std::uint32_t{0}};Validity validity=Validity::absent; };
inline constexpr std::array baseline_fields{ReferencePoint::id,SampleRate::id,StateEvent::id,DataPayloadFormat::id};
inline constexpr std::size_t state_field_capacity=8,command_field_capacity=4;
inline constexpr std::array state_fields{ReferencePoint::id,SampleRate::id,StateEvent::id,DataPayloadFormat::id,RFReferenceFrequency::id,Bandwidth::id,Gain::id,DiscreteIO32::id};
inline std::size_t field_index(FieldId id) noexcept {for(std::size_t i=0;i<state_fields.size();++i)if(state_fields[i]==id)return i;return state_fields.size();}
struct StateSnapshot {
    std::uint64_t version=0;
    std::array<FieldState,state_field_capacity> fields{{{ReferencePoint::id},{SampleRate::id,Hertz{}},{StateEvent::id},{DataPayloadFormat::id,PayloadFormat{}},{RFReferenceFrequency::id,Hertz{}},{Bandwidth::id,Hertz{}},{Gain::id,GainStages{}},{DiscreteIO32::id}}};
    profiles::iq::Profile profile=profiles::iq::Profile::generator_v1;
};
inline constexpr std::size_t active_state_fields(profiles::iq::Profile profile) noexcept {return profile==profiles::iq::Profile::graphx_radio?8:profile==profiles::iq::Profile::frequency_tunable?5:4;}
inline bool profile_field(profiles::iq::Profile profile,FieldId id) noexcept {
    const auto index=field_index(id);return index<4 || (index==4&&profile!=profiles::iq::Profile::generator_v1) || (index<8&&profile==profiles::iq::Profile::graphx_radio);
}
inline Result<void> validate_snapshot(const StateSnapshot& state) noexcept {
    if(state.profile!=profiles::iq::Profile::generator_v1&&state.profile!=profiles::iq::Profile::frequency_tunable&&state.profile!=profiles::iq::Profile::graphx_radio)return std::unexpected(Error{ErrorCode::invalid_argument});
    for(std::size_t i=0;i<state_field_capacity;++i){const auto& field=state.fields[i];
      if(i>=active_state_fields(state.profile)){if(field.validity!=Validity::absent)return std::unexpected(Error{ErrorCode::unsupported_capability});continue;}
      if(field.id!=state_fields[i]||static_cast<unsigned>(field.validity)>static_cast<unsigned>(Validity::unknown))return std::unexpected(Error{ErrorCode::invalid_argument});
      if(field.validity==Validity::known){auto valid=validate_value(field.id,field.value);if(!valid)return valid;}
      if(i==4&&field.validity==Validity::known){const auto* value=std::get_if<Hertz>(&field.value);constexpr std::int64_t unit=1ll<<20;if(!value||value->q20<static_cast<std::int64_t>(profiles::iq::minimum_center_hz)*unit||value->q20>static_cast<std::int64_t>(profiles::iq::maximum_center_hz)*unit||(state.profile==profiles::iq::Profile::frequency_tunable&&value->q20%unit))return std::unexpected(Error{ErrorCode::invalid_argument});}
    }
    if(state.profile==profiles::iq::Profile::graphx_radio){
      if(state.fields[0].validity==Validity::known){const auto* sid=std::get_if<std::uint32_t>(&state.fields[0].value);if(!sid||*sid<1||*sid>4)return std::unexpected(Error{ErrorCode::invalid_argument});}
      if(state.fields[3].validity==Validity::known){const auto* format=std::get_if<PayloadFormat>(&state.fields[3].value);if(!format||*format!=PayloadFormat{0x200003cf00000000})return std::unexpected(Error{ErrorCode::invalid_argument});}
      constexpr std::int64_t unit=1ll<<20;
      if(state.fields[1].validity==Validity::known){const auto* rate=std::get_if<Hertz>(&state.fields[1].value);if(!rate||rate->q20<1000*unit||rate->q20>2'000'000*unit||rate->q20%unit)return std::unexpected(Error{ErrorCode::invalid_argument});}
      if(state.fields[5].validity==Validity::known){const auto* bw=std::get_if<Hertz>(&state.fields[5].value);if(!bw||bw->q20<=0||bw->q20>2'000'000*unit||(state.fields[1].validity==Validity::known&&bw->q20>std::get<Hertz>(state.fields[1].value).q20))return std::unexpected(Error{ErrorCode::invalid_argument});}
      if(state.fields[6].validity==Validity::known){const auto* gain=std::get_if<GainStages>(&state.fields[6].value);if(!gain||gain->stage2_q7||gain->stage1_q7<-60*128||gain->stage1_q7>60*128)return std::unexpected(Error{ErrorCode::invalid_argument});}
      if(state.fields[7].validity==Validity::known){const auto* streaming=std::get_if<std::uint32_t>(&state.fields[7].value);if(!streaming||(*streaming!=2&&*streaming!=3))return std::unexpected(Error{ErrorCode::invalid_argument});}
    }
    return {};
}
struct Diagnostics { std::uint32_t warnings=0,errors=0; };
struct PlannedField {
    FieldId id{};SemanticValue requested{std::uint32_t{0}},adjusted{std::uint32_t{0}};
    std::uint8_t dependency_mask=0;bool eligible=false;Diagnostics diagnostics{};bool time_known=false,ordinal_known=false;
};
struct ExecutionPlan {
    std::array<PlannedField,5> fields{};std::size_t count=0;
    std::array<std::uint8_t,5> order{};
    std::uint64_t expected_state_version=0,operation=0,association_generation=0;
    timing::Boundary boundary{};
};
enum class FieldStatus : std::uint8_t { pending,executed,failed,cancelled,unknown_effect,not_executed };
struct FieldOutcome {
    FieldId id{};FieldStatus status=FieldStatus::pending;
    SemanticValue value{std::uint32_t{0}};Validity validity=Validity::absent;
    timing::ProtocolTime actual_time{};std::uint64_t sample_ordinal=0,uncertainty_ps=0;
    bool time_known=false,ordinal_known=false,simulated=false;Diagnostics diagnostics{};
};
struct EffectiveEvent {
    StateSnapshot state{};timing::ProtocolTime actual_time{};
    std::uint64_t sample_ordinal=0,source_operation=0,association_generation=0;
    std::uint8_t changed_mask=0;bool time_known=false,ordinal_known=false;FieldOutcome outcome{};
    // GraphX simulated sample epoch; actual_time remains device execution time.
    std::optional<timing::ProtocolTime> sample_epoch{};
    timing::ProtocolTime context_time() const noexcept { return sample_epoch.value_or(actual_time); }
};
class RevisionReservation {
    std::shared_ptr<void> owner_;
    void* context_=nullptr;
    std::uint64_t token_=0;
    void (*release_)(void*,std::uint64_t) noexcept=nullptr;
public:
    RevisionReservation() noexcept=default;
    RevisionReservation(std::shared_ptr<void> owner,void* context,std::uint64_t token,void (*release)(void*,std::uint64_t) noexcept) noexcept
        :owner_(std::move(owner)),context_(context),token_(token),release_(release){}
    RevisionReservation(const RevisionReservation&)=delete;RevisionReservation& operator=(const RevisionReservation&)=delete;
    RevisionReservation(RevisionReservation&& other) noexcept:owner_(std::move(other.owner_)),context_(other.context_),token_(other.token_),release_(std::exchange(other.release_,nullptr)){}
    RevisionReservation& operator=(RevisionReservation&& other) noexcept {if(this!=&other){reset();owner_=std::move(other.owner_);context_=other.context_;token_=other.token_;release_=std::exchange(other.release_,nullptr);}return *this;}
    ~RevisionReservation(){reset();}
    void reset() noexcept {if(release_){auto release=std::exchange(release_,nullptr);release(context_,token_);}owner_.reset();}
    std::uint64_t token() const noexcept{return token_;}
    explicit operator bool() const noexcept{return release_!=nullptr;}
};
struct EffectSink {
    void* context=nullptr;
    std::shared_ptr<void> owner{};
    Result<RevisionReservation> (*reserve)(void*,std::size_t) noexcept=nullptr;
    void (*record)(void*,const EffectiveEvent&,RevisionReservation&,AdmissionBundle) noexcept=nullptr;
};
static_assert(sizeof(EffectiveEvent)<=448);
} // namespace vita::runtime
