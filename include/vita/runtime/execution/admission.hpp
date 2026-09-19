#pragma once
#include <vita/core/error.hpp>
#include <array>
#include <cstddef>
#include <memory>
#include <mutex>
#include <utility>
namespace vita::runtime {
enum class Resource : std::size_t { transaction, plan_bytes, schedule, revision, completion, response, duplicate_entry, duplicate_bytes, context_publication, ordinary_queue, cancellation_queue, cancellation_response, emergency_response, data_queue, count };
inline constexpr std::size_t resource_count=static_cast<std::size_t>(Resource::count);
struct AdmissionRequest {
    std::array<std::size_t,resource_count> counts{};
    bool valid{true};
    AdmissionRequest& need(Resource r,std::size_t n=1) noexcept {
        auto index=static_cast<std::size_t>(r); if(index>=resource_count) valid=false; else counts[index]=n; return *this;
    }
};
namespace detail {
struct AdmissionState { std::mutex mutex; AdmissionRequest capacities,used; bool open{true}; };
}
class AdmissionPool;
class AdmissionBundle {
    std::shared_ptr<detail::AdmissionState> state_;
    AdmissionRequest held_;
    friend class AdmissionPool;
    AdmissionBundle(std::shared_ptr<detail::AdmissionState> state,AdmissionRequest held) noexcept : state_(std::move(state)),held_(held) {}
public:
    AdmissionBundle() noexcept=default;
    AdmissionBundle(AdmissionBundle const&)=delete;
    AdmissionBundle& operator=(AdmissionBundle const&)=delete;
    AdmissionBundle(AdmissionBundle&& other) noexcept : state_(std::move(other.state_)),held_(other.held_) { other.held_={}; }
    AdmissionBundle& operator=(AdmissionBundle&& other) noexcept { if(this!=&other) { reset(); state_=std::move(other.state_); held_=other.held_; other.held_={}; } return *this; }
    ~AdmissionBundle() { reset(); }
    void reset() noexcept {
        if(!state_) return;
        auto state=std::move(state_); std::lock_guard lock(state->mutex);
        for(std::size_t i=0;i<resource_count;++i) state->used.counts[i]-=held_.counts[i]; held_={};
    }
    // Split credits into a longer-lived outcome/revision owner without changing global usage.
    Result<AdmissionBundle> transfer(AdmissionRequest request) noexcept {
        if(!state_) return std::unexpected(Error{ErrorCode::invalid_state});
        if(!request.valid) return std::unexpected(Error{ErrorCode::invalid_argument});
        for(std::size_t i=0;i<resource_count;++i) if(request.counts[i]>held_.counts[i]) return std::unexpected(Error{ErrorCode::invalid_argument});
        for(std::size_t i=0;i<resource_count;++i) held_.counts[i]-=request.counts[i];
        return AdmissionBundle(state_,request);
    }
    std::size_t held(Resource r) const noexcept { return static_cast<std::size_t>(r)<resource_count ? held_.counts[static_cast<std::size_t>(r)] : 0; }
};
class AdmissionPool {
    std::shared_ptr<detail::AdmissionState> state_;
public:
    explicit AdmissionPool(AdmissionRequest capacities) : state_(std::make_shared<detail::AdmissionState>()) { state_->capacities=capacities; }
    Result<AdmissionBundle> acquire(AdmissionRequest request) noexcept {
        std::lock_guard lock(state_->mutex);
        if(!request.valid || !state_->capacities.valid) return std::unexpected(Error{ErrorCode::invalid_argument});
        if(!state_->open) return std::unexpected(Error{ErrorCode::invalid_state});
        for(std::size_t i=0;i<resource_count;++i) if(request.counts[i]>state_->capacities.counts[i]-state_->used.counts[i]) return std::unexpected(Error{ErrorCode::capacity_exhausted});
        for(std::size_t i=0;i<resource_count;++i) state_->used.counts[i]+=request.counts[i];
        return AdmissionBundle(state_,request);
    }
    bool owns(const AdmissionBundle& bundle) const noexcept {return state_&&bundle.state_.get()==state_.get();}
    std::size_t capacity(Resource r) const noexcept { return static_cast<std::size_t>(r)<resource_count ? state_->capacities.counts[static_cast<std::size_t>(r)] : 0; }
    std::size_t used(Resource r) const noexcept { std::lock_guard lock(state_->mutex); return static_cast<std::size_t>(r)<resource_count ? state_->used.counts[static_cast<std::size_t>(r)] : 0; }
    void close() noexcept { std::lock_guard lock(state_->mutex); state_->open=false; }
    static AdmissionRequest reference_capacities() noexcept {
        AdmissionRequest c;
        c.need(Resource::transaction,256).need(Resource::plan_bytes,2097152).need(Resource::schedule,128)
         .need(Resource::revision,2048).need(Resource::completion,1024).need(Resource::response,768)
         .need(Resource::duplicate_entry,4096).need(Resource::duplicate_bytes,8388608)
         .need(Resource::context_publication,2048).need(Resource::ordinary_queue,256)
         .need(Resource::data_queue,4096).need(Resource::cancellation_queue,64).need(Resource::cancellation_response,64).need(Resource::emergency_response,256);
        return c;
    }
    static constexpr std::size_t metadata_bytes() noexcept { return sizeof(detail::AdmissionState); }
};
} // namespace vita::runtime
