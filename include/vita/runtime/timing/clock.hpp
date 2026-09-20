#pragma once
#include <vita/runtime/timing/time.hpp>
#include <optional>
namespace vita::runtime::timing {
enum class Epoch : std::uint8_t { utc, gps, other };
enum class ClockState : std::uint8_t { unbound, acquiring, locked, holdover, faulted };
struct ClockBinding {
    Epoch epoch{Epoch::other};
    bool epoch_configured{},qualified{},injected{};
    std::uint64_t calibration_uncertainty_ps{},holdover_drift_ppb{},holdover_limit_ns{2000000000ULL};
};
struct ClockSnapshot {
    ClockState state{ClockState::unbound};
    ProtocolTime time{};
    std::uint64_t uncertainty_ps{},mapping_generation{};
    bool calibrated{},injected{};
    Epoch epoch{Epoch::other};
};
class ProtocolClock {
    ClockBinding binding_{};
    ClockState state_{ClockState::unbound};
    MonoTime capture_{};
    ProtocolTime origin_{};
    std::uint64_t generation_{},pulse_uncertainty_{};
    bool mapped_{};
public:
    Result<void> bind(ClockBinding binding) noexcept {
        if(!binding.epoch_configured || (!binding.qualified && !binding.injected) || !binding.holdover_limit_ns ||
           static_cast<unsigned>(binding.epoch)>static_cast<unsigned>(Epoch::other)) return std::unexpected(Error{ErrorCode::invalid_argument});
        if(generation_==std::numeric_limits<std::uint64_t>::max()) return std::unexpected(Error{ErrorCode::overflow});
        binding_=binding; state_=ClockState::acquiring; mapped_=false; ++generation_; return {};
    }
    // PPS needs associated time-of-day. Binding owns conversion to its declared epoch.
    Result<void> observe_pps(MonoTime capture,std::optional<ProtocolTime> time_of_day,std::uint64_t uncertainty_ps=0) noexcept {
        if(state_==ClockState::unbound) return std::unexpected(Error{ErrorCode::invalid_state});
        if(!time_of_day) { if(!mapped_) state_=ClockState::acquiring; return std::unexpected(Error{ErrorCode::invalid_argument}); }
        if(!valid(*time_of_day) || (mapped_ && capture<capture_)) return std::unexpected(Error{ErrorCode::invalid_argument});
        if(generation_==std::numeric_limits<std::uint64_t>::max() || uncertainty_ps>std::numeric_limits<std::uint64_t>::max()-binding_.calibration_uncertainty_ps)
            return std::unexpected(Error{ErrorCode::overflow});
        // Every replacement mapping has a new generation, so scheduled work revalidates.
        capture_=capture; origin_=*time_of_day; pulse_uncertainty_=uncertainty_ps; mapped_=true; ++generation_; state_=ClockState::locked; return {};
    }
    Result<void> pps_lost(MonoTime now) noexcept {
        if(!mapped_ || now<capture_) return std::unexpected(Error{ErrorCode::invalid_state});
        state_=now.ns-capture_.ns>=binding_.holdover_limit_ns ? ClockState::faulted : ClockState::holdover; return {};
    }
    Result<ClockSnapshot> snapshot(MonoTime now) noexcept {
        if(!mapped_ || now<capture_) return std::unexpected(Error{ErrorCode::invalid_state});
        auto elapsed=now.ns-capture_.ns;
        if(elapsed>=binding_.holdover_limit_ns) state_=ClockState::faulted;
        else if(elapsed>1000000000ULL && state_==ClockState::locked) state_=ClockState::holdover;
        auto time=add(origin_,from_nanoseconds(elapsed)); if(!time) return std::unexpected(time.error());
        // ceil(elapsed_ns * ppb / 1e6) picoseconds; checked decomposition avoids wide products.
        const std::uint64_t whole=elapsed/1000000ULL,remainder=elapsed%1000000ULL,
                    drift=binding_.holdover_drift_ppb;
        if((whole && drift>std::numeric_limits<std::uint64_t>::max()/whole) || (remainder && drift>std::numeric_limits<std::uint64_t>::max()/remainder))
            return std::unexpected(Error{ErrorCode::overflow});
        const std::uint64_t product=remainder*drift;
        const std::uint64_t extra=product/1000000ULL+(product%1000000ULL!=0);
        auto base=binding_.calibration_uncertainty_ps+pulse_uncertainty_;
        if(whole*drift>std::numeric_limits<std::uint64_t>::max()-base || extra>std::numeric_limits<std::uint64_t>::max()-base-whole*drift)
            return std::unexpected(Error{ErrorCode::overflow});
        return ClockSnapshot{state_,*time,base+whole*drift+extra,generation_,state_==ClockState::locked,binding_.injected,binding_.epoch};
    }
    Result<bool> data_start_allowed(MonoTime now) noexcept { auto current=snapshot(now); if(!current) return std::unexpected(current.error()); return current->state==ClockState::locked; }
    Result<bool> data_continue_allowed(MonoTime now) noexcept { auto current=snapshot(now); if(!current) return std::unexpected(current.error()); return current->state==ClockState::locked || current->state==ClockState::holdover; }
    ClockState state() const noexcept { return state_; }
    std::uint64_t generation() const noexcept { return generation_; }
    // Cached accessors require snapshot(now) on the same serialized control domain first.
    bool data_start_allowed() const noexcept { return mapped_ && state_==ClockState::locked; }
    bool data_continue_allowed() const noexcept { return mapped_ && (state_==ClockState::locked || state_==ClockState::holdover); }
    bool mapping_current(std::uint64_t generation) const noexcept { return mapped_ && generation==generation_; }
};
} // namespace vita::runtime::timing
