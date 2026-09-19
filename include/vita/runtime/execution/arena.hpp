#pragma once
#include <vita/core/error.hpp>
#include <vita/core/bytes.hpp>
#include <array>
#include <cstddef>
#include <memory>
#include <limits>
#include <mutex>
#include <utility>
namespace vita::runtime {
template<std::size_t Count,std::size_t BytesPerSlot> class SlotArena {
    static_assert(Count && BytesPerSlot && Count<=std::numeric_limits<std::size_t>::max()/BytesPerSlot);
    struct alignas(std::max_align_t) Storage { std::array<std::byte,BytesPerSlot> bytes{}; };
    struct State { std::mutex mutex; std::array<Storage,Count> slots{}; std::array<bool,Count> busy{}; };
    std::shared_ptr<State> state_;
public:
    class Lease {
        std::shared_ptr<State> state_;
        std::size_t slot_{};
        friend class SlotArena;
        Lease(std::shared_ptr<State> state,std::size_t slot) noexcept : state_(std::move(state)),slot_(slot) {}
    public:
        Lease() noexcept=default;
        Lease(Lease const&)=delete; Lease& operator=(Lease const&)=delete;
        Lease(Lease&&)=default;
        Lease& operator=(Lease&& other) noexcept { if(this!=&other) { reset(); state_=std::move(other.state_); slot_=other.slot_; } return *this; }
        ~Lease() { reset(); }
        void reset() noexcept { if(state_) { auto state=std::move(state_); std::lock_guard lock(state->mutex); state->busy[slot_]=false; } }
        MutableBytes bytes() noexcept { return state_ ? MutableBytes{state_->slots[slot_].bytes} : MutableBytes{}; }
    };
    SlotArena() : state_(std::make_shared<State>()) {}
    Result<Lease> acquire(std::size_t required) noexcept {
        if(required>BytesPerSlot) return std::unexpected(Error{ErrorCode::capacity_exhausted});
        std::lock_guard lock(state_->mutex);
        for(std::size_t i=0;i<Count;++i) if(!state_->busy[i]) { state_->busy[i]=true; return Lease(state_,i); }
        return std::unexpected(Error{ErrorCode::capacity_exhausted});
    }
    static constexpr std::size_t storage_bytes() noexcept { return sizeof(State); }
    static constexpr std::size_t slot_capacity() noexcept { return BytesPerSlot; }
};
} // namespace vita::runtime

#include <vita/runtime/execution/admission.hpp>
namespace vita::runtime {
template<std::size_t Count,std::size_t BytesPerSlot> struct AdmittedStorage {
    AdmissionBundle credits;
    typename SlotArena<Count,BytesPerSlot>::Lease storage;
    static Result<AdmittedStorage> acquire(AdmissionPool& pool,SlotArena<Count,BytesPerSlot>& arena,
        AdmissionRequest request,std::size_t required) noexcept {
        if(request.counts[static_cast<std::size_t>(Resource::plan_bytes)]<required || pool.capacity(Resource::plan_bytes)>Count*BytesPerSlot)
            return std::unexpected(Error{ErrorCode::invalid_argument});
        auto reserved=pool.acquire(request); if(!reserved) return std::unexpected(reserved.error());
        auto bytes=arena.acquire(required); if(!bytes) return std::unexpected(bytes.error());
        return AdmittedStorage{std::move(*reserved),std::move(*bytes)};
    }
};
} // namespace vita::runtime
