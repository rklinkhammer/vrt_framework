#pragma once
#include <vita/fields/types.hpp>
#include <vita/runtime/timing/scheduling.hpp>
#include <array>
#include <memory>
#include <utility>
#include <vita/runtime/execution/admission.hpp>
namespace vita::runtime {
enum class Validity : std::uint8_t { absent,known,unknown };
struct FieldState { FieldId id{};SemanticValue value{std::uint32_t{0}};Validity validity=Validity::absent; };
inline constexpr std::array baseline_fields{ReferencePoint::id,SampleRate::id,StateEvent::id,DataPayloadFormat::id};
inline std::size_t field_index(FieldId id) noexcept {for(std::size_t i=0;i<baseline_fields.size();++i)if(baseline_fields[i]==id)return i;return baseline_fields.size();}
struct StateSnapshot {
    std::uint64_t version=0;
    std::array<FieldState,4> fields{{{ReferencePoint::id},{SampleRate::id,Hertz{}},{StateEvent::id},{DataPayloadFormat::id,PayloadFormat{}}}};
};
struct Diagnostics { std::uint32_t warnings=0,errors=0; };
struct PlannedField {
    FieldId id{};SemanticValue requested{std::uint32_t{0}},adjusted{std::uint32_t{0}};
    std::uint8_t dependency_mask=0;bool eligible=false;Diagnostics diagnostics{};bool time_known=false,ordinal_known=false;
};
struct ExecutionPlan {
    std::array<PlannedField,4> fields{};std::size_t count=0;
    std::array<std::uint8_t,4> order{};
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
