#pragma once
#include <vita/runtime/state/contracts.hpp>
#include <atomic>
#include <limits>
namespace vita::runtime::context {
inline bool same_state(const StateSnapshot& a,const StateSnapshot& b) noexcept {
    if(a.profile!=b.profile)return false;
    for(std::size_t i=0;i<active_state_fields(a.profile);++i)if(a.fields[i].id!=b.fields[i].id||a.fields[i].validity!=b.fields[i].validity||(a.fields[i].validity==Validity::known&&a.fields[i].value!=b.fields[i].value))return false;return true;
}
inline bool required_known(const StateSnapshot& state) noexcept {
    return state.fields[1].validity==Validity::known && state.fields[3].validity==Validity::known &&
           (state.profile==profiles::iq::Profile::generator_v1||state.fields[4].validity==Validity::known) &&
           (state.profile!=profiles::iq::Profile::graphx_radio||
            (state.fields[5].validity==Validity::known&&state.fields[6].validity==Validity::known));
}
enum class Publication : std::uint8_t { pending,accepted,failed };
struct Revision {
    EffectiveEvent event{};AdmissionBundle credits{};
    std::uint64_t id{},reservation{};std::atomic<std::size_t> references{0};
    Publication publication=Publication::pending;bool occupied{},committed{},detached{},data_dependency{};
};
static_assert(sizeof(Revision)<=608);
class RevisionHandle {
    template<std::size_t> friend class RevisionStore;
    std::shared_ptr<void> owner_;Revision* revision_=nullptr;
    void release() noexcept {if(revision_)revision_->references.fetch_sub(1,std::memory_order_acq_rel);revision_=nullptr;owner_.reset();}
public:
    RevisionHandle() noexcept=default;
    RevisionHandle(std::shared_ptr<void> owner,Revision* revision) noexcept:owner_(std::move(owner)),revision_(revision){if(revision_)revision_->references.fetch_add(1,std::memory_order_relaxed);}
    RevisionHandle(const RevisionHandle& other) noexcept:RevisionHandle(other.owner_,other.revision_){}
    RevisionHandle& operator=(const RevisionHandle& other) noexcept {if(this!=&other){release();owner_=other.owner_;revision_=other.revision_;if(revision_)revision_->references.fetch_add(1,std::memory_order_relaxed);}return *this;}
    RevisionHandle(RevisionHandle&& other) noexcept:owner_(std::move(other.owner_)),revision_(std::exchange(other.revision_,nullptr)){}
    RevisionHandle& operator=(RevisionHandle&& other) noexcept {if(this!=&other){release();owner_=std::move(other.owner_);revision_=std::exchange(other.revision_,nullptr);}return *this;}
    ~RevisionHandle(){release();}
    explicit operator bool() const noexcept{return revision_!=nullptr;}
    const EffectiveEvent& event() const noexcept{return revision_->event;}
    std::uint64_t id() const noexcept{return revision_?revision_->id:0;}
    Publication publication() const noexcept{return revision_?revision_->publication:Publication::failed;}
};
// Mutations and publication queries belong to one serialized control/sample domain.
// Immutable event reads and handle retain/release may occur on other threads.
template<std::size_t Capacity=128> class RevisionStore {
    static_assert(Capacity>0);
    struct State {
        std::array<Revision,Capacity> slots{};std::uint64_t generation=1,next_id=1,next_reservation=1;
        Revision* current=nullptr;bool faulted=false;
        void collect() noexcept {for(auto& s:slots)if(s.occupied&&s.committed&& &s!=current&&(s.detached||s.publication!=Publication::pending)&&!s.references.load(std::memory_order_acquire)){s.credits.reset();s.occupied=s.committed=false;}}
    };
    std::shared_ptr<State> state_=std::make_shared<State>();
    static void unused(void* context,std::uint64_t token) noexcept {auto& state=*static_cast<State*>(context);for(auto& s:state.slots)if(s.occupied&&!s.committed&&s.reservation==token)s.occupied=false;}
    static Result<RevisionReservation> reserve_impl(std::shared_ptr<State> state,std::size_t count) noexcept {
        if(!count||count>Capacity)return std::unexpected(Error{ErrorCode::invalid_argument});
        state->collect();std::size_t free=0,reserved=0;for(auto& s:state->slots){free+=!s.occupied;reserved+=s.occupied&&!s.committed;}
        if(free<count)return std::unexpected(Error{ErrorCode::capacity_exhausted});
        if(state->next_reservation==UINT64_MAX||state->next_id>UINT64_MAX-count-reserved)return std::unexpected(Error{ErrorCode::overflow});
        auto token=state->next_reservation++;for(auto& s:state->slots)if(!s.occupied&&count){s.occupied=true;s.committed=s.detached=s.data_dependency=false;s.reservation=token;s.publication=Publication::pending;--count;}
        return RevisionReservation(state,state.get(),token,unused);
    }
    struct BindingState {std::shared_ptr<State> state;};
    std::shared_ptr<BindingState> binding_=std::make_shared<BindingState>(BindingState{state_});
    static Result<RevisionReservation> reserve_thunk(void* context,std::size_t count) noexcept {return reserve_impl(static_cast<BindingState*>(context)->state,count);}
    static void record_thunk(void* context,const EffectiveEvent& event,RevisionReservation& reservation,AdmissionBundle credits) noexcept {
        auto state=static_cast<BindingState*>(context)->state;
        if(event.association_generation!=state->generation)return;
        for(auto& s:state->slots)if(s.occupied&&!s.committed&&s.reservation==reservation.token()){
            if(event.association_generation!=state->generation){s.occupied=false;return;}
            s.event=event;s.credits=std::move(credits);s.id=state->next_id++;s.committed=true;s.publication=Publication::pending;state->current=&s;
            if(!required_known(event.state))state->faulted=true;return;
        }
        state->faulted=true; // adapter contract violation; admission promised a slot
    }
public:
    explicit RevisionStore(std::uint64_t generation=1) {state_->generation=generation;}
    EffectSink binding() noexcept{return {binding_.get(),binding_,reserve_thunk,record_thunk};}
    Result<RevisionReservation> reserve(std::size_t count) noexcept{return reserve_impl(state_,count);}
    Result<RevisionHandle> initial(EffectiveEvent event,AdmissionBundle credits) noexcept {
        auto snapshot_valid=validate_snapshot(event.state);if(!snapshot_valid)return std::unexpected(snapshot_valid.error());
        for(std::size_t i=0;i<active_state_fields(event.state.profile);++i){const auto& field=event.state.fields[i];if(field.id!=state_fields[i])return std::unexpected(Error{ErrorCode::invalid_argument});if(field.validity==Validity::known){auto valid=validate_value(field.id,field.value);if(!valid)return std::unexpected(valid.error());}}
        if(state_->current||event.association_generation!=state_->generation||!required_known(event.state))return std::unexpected(Error{ErrorCode::invalid_state});
        auto reservation=reserve(1);if(!reservation)return std::unexpected(reservation.error());record_thunk(binding_.get(),event,*reservation,std::move(credits));return current();
    }
    Result<RevisionHandle> current() const noexcept {if(!state_->current)return std::unexpected(Error{ErrorCode::invalid_state});return RevisionHandle(state_,state_->current);}
    Result<RevisionHandle> next_publication() const noexcept {
        Revision* next=nullptr;for(auto& s:state_->slots)if(s.committed&&!s.detached&&s.publication==Publication::pending){
            if(!next || (s.event.time_known!=next->event.time_known ? s.event.time_known : s.event.time_known ? s.event.context_time()<next->event.context_time() : s.id>next->id))next=&s;
        }
        if(!next)return std::unexpected(Error{ErrorCode::invalid_state});return RevisionHandle(state_,next);
    }
    bool owns(const RevisionHandle& handle) const noexcept {return handle.owner_.get()==state_.get();}
    Result<void> note_data_dependency(const RevisionHandle& handle) noexcept {
        if(!owns(handle))return std::unexpected(Error{ErrorCode::invalid_argument});
        for(auto& s:state_->slots)if(&s==handle.revision_&&s.committed&&!s.detached){s.data_dependency=true;return {};}
        return std::unexpected(Error{ErrorCode::stale_generation});
    }
    Result<RevisionHandle> publication_group() const noexcept {
        auto first=next_publication();if(!first)return std::unexpected(first.error());auto* final=first->revision_;
        if(!final->event.time_known){for(auto& s:state_->slots)if(s.committed&&!s.detached&&s.publication==Publication::pending&&!s.event.time_known){const auto* indicators=std::get_if<std::uint32_t>(&s.event.state.fields[2].value);if(s.data_dependency||(indicators&&(*indicators&(1u<<12))))return std::unexpected(Error{ErrorCode::identity_conflict});}return RevisionHandle(state_,final);}
        for(auto& s:state_->slots)if(s.committed&&!s.detached&&s.publication==Publication::pending&&s.event.time_known&&final->event.time_known&&s.event.context_time()==final->event.context_time()){
            if(s.event.sample_ordinal!=final->event.sample_ordinal||s.event.ordinal_known!=final->event.ordinal_known)return std::unexpected(Error{ErrorCode::identity_conflict});
            if(s.id>final->id)final=&s;
        }
        for(auto& s:state_->slots)if(s.committed&&!s.detached&&s.publication==Publication::pending&&s.id!=final->id&&s.event.time_known&&final->event.time_known&&s.event.context_time()==final->event.context_time()){
            const auto* indicators=std::get_if<std::uint32_t>(&s.event.state.fields[2].value);
            if((s.data_dependency||(indicators&&(*indicators&(1u<<12))))&&!same_state(s.event.state,final->event.state))return std::unexpected(Error{ErrorCode::identity_conflict});
        }
        return RevisionHandle(state_,final);
    }
    Result<void> accept_group(const RevisionHandle& handle) noexcept {
        if(!owns(handle))return std::unexpected(Error{ErrorCode::invalid_argument});
        if(!handle||handle.event().association_generation!=state_->generation)return std::unexpected(Error{ErrorCode::stale_generation});
        auto selected=publication_group();if(!selected)return std::unexpected(selected.error());
        if(selected->id()!=handle.id())return std::unexpected(Error{ErrorCode::invalid_state});
        for(auto& s:state_->slots)if(s.committed&&!s.detached&&s.publication==Publication::pending&&(s.id==handle.id()||(!s.event.time_known&&!handle.event().time_known)||(s.event.time_known&&handle.event().time_known&&s.event.context_time()==handle.event().context_time()&&s.event.sample_ordinal==handle.event().sample_ordinal)))s.publication=Publication::accepted;
        return {};
    }
    Result<void> mark_publication(const RevisionHandle& handle,Publication status) noexcept {
        if(!owns(handle))return std::unexpected(Error{ErrorCode::invalid_argument});
        for(auto& s:state_->slots)if(s.committed&&!s.detached&&s.id==handle.id()&&s.event.association_generation==state_->generation){s.publication=status;return {};}
        return std::unexpected(Error{ErrorCode::stale_generation});
    }
    void fault() noexcept{state_->faulted=true;}
    bool faulted() const noexcept{return state_->faulted;}
    std::uint64_t generation() const noexcept{return state_->generation;}
    Result<void> detach(std::uint64_t generation) noexcept {
        if(generation<=state_->generation)return std::unexpected(Error{ErrorCode::stale_generation});
        for(auto& s:state_->slots)if(s.occupied){if(s.committed)s.detached=true;else s.occupied=false;}
        state_->current=nullptr;state_->generation=generation;state_->faulted=false;state_->collect();return {};
    }
    void collect() noexcept{state_->collect();}
    std::size_t occupied() const noexcept{std::size_t n=0;for(auto& s:state_->slots)n+=s.occupied;return n;}
    static constexpr std::size_t revision_bytes() noexcept{return sizeof(Revision);}
    static constexpr std::size_t storage_bytes() noexcept{return sizeof(State)+sizeof(BindingState);}
};
} // namespace vita::runtime::context
