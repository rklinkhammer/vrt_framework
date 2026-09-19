#pragma once
#include <vita/runtime/timing/clock.hpp>
#include <vita/runtime/timing/sample_timeline.hpp>
#include <span>
namespace vita::runtime::timing {
struct TimingCapabilities {
    std::uint64_t device_early_ps{},device_late_ps{},application_early_ps{},application_late_ps{};
    std::uint64_t preparation_lead_ns{},horizon_ns{};
    bool qualified{},injected{};
    static TimingCapabilities deterministic() noexcept { return {1000000,1000000,1000000000,1000000000,1000000,10000000000ULL,false,true}; }
};
struct ExecutionWindow { ProtocolTime earliest{},latest{}; };
struct Boundary { ProtocolTime time{}; std::uint64_t sample_ordinal{},mapping_generation{}; bool committed{}; bool backend_ready{true}; std::uint64_t quantization_uncertainty_ps{}; };
inline Boundary make_boundary(SampleTimeline const& timeline,std::uint64_t mapping_generation,bool backend_ready=true) noexcept {
    auto exact=timeline.exact_time(); return Boundary{exact.time,timeline.ordinal(),mapping_generation,false,backend_ready,exact.numerator ? 1ULL : 0ULL};
}
inline Result<ExecutionWindow> execution_window(unsigned mode,ProtocolTime requested,TimingCapabilities capabilities) noexcept {
    if(mode<1 || mode>4 || !valid(requested)) return std::unexpected(Error{ErrorCode::invalid_argument});
    auto early=capabilities.device_early_ps,late=capabilities.device_late_ps;
    if(mode==2) {
        if(capabilities.application_late_ps>std::numeric_limits<std::uint64_t>::max()-late) return std::unexpected(Error{ErrorCode::overflow});
        late+=capabilities.application_late_ps;
    } else if(mode==3) {
        if(capabilities.application_early_ps>std::numeric_limits<std::uint64_t>::max()-early) return std::unexpected(Error{ErrorCode::overflow});
        early+=capabilities.application_early_ps;
    } else if(mode==4) { early=capabilities.application_early_ps; late=capabilities.application_late_ps; }
    auto first=subtract(requested,from_picoseconds(early)); if(!first) return std::unexpected(first.error());
    auto last=add(requested,from_picoseconds(late)); if(!last) return std::unexpected(last.error());
    return ExecutionWindow{*first,*last};
}
inline Result<bool> effect_interval_allowed(unsigned mode,ProtocolTime requested,ProtocolTime first,ProtocolTime last,TimingCapabilities capabilities) noexcept {
    if(mode>4 || !valid(first)||!valid(last)||last<first) return std::unexpected(Error{ErrorCode::invalid_argument});
    if(mode==0) return true;
    auto window=execution_window(mode,requested,capabilities); if(!window) return std::unexpected(window.error());
    return first>=window->earliest && last<=window->latest;
}
inline Result<Boundary> choose_boundary(unsigned mode,ProtocolTime requested,ClockSnapshot clock,std::span<Boundary const> boundaries,TimingCapabilities capabilities) noexcept {
    if(mode>4 || !valid(clock.time)) return std::unexpected(Error{ErrorCode::invalid_argument});
    if(boundaries.size()>128) return std::unexpected(Error{ErrorCode::resource_limit});
    if(mode && ((!capabilities.qualified && !capabilities.injected) || (clock.state!=ClockState::locked && clock.state!=ClockState::holdover)))
        return std::unexpected(Error{ErrorCode::invalid_state});
    auto ready=add(clock.time,from_nanoseconds(mode ? capabilities.preparation_lead_ns : 0)); if(!ready) return std::unexpected(ready.error());
    auto horizon=mode ? add(clock.time,from_nanoseconds(capabilities.horizon_ns)) : Result<ProtocolTime>{clock.time}; if(!horizon) return std::unexpected(horizon.error());
    if(mode && (!valid(requested) || requested>*horizon)) return std::unexpected(Error{ErrorCode::invalid_argument});
    std::optional<ExecutionWindow> window;
    if(mode) { auto w=execution_window(mode,requested,capabilities); if(!w) return std::unexpected(w.error()); window=*w; }
    std::optional<Boundary> selected;
    Duration distance{};
    for(auto boundary:boundaries) {
        if(!valid(boundary.time) || boundary.committed || !boundary.backend_ready || boundary.mapping_generation!=clock.mapping_generation || boundary.time<*ready || (mode && boundary.time>*horizon)) continue;
        if(mode) {
            if(boundary.quantization_uncertainty_ps>std::numeric_limits<std::uint64_t>::max()-clock.uncertainty_ps)
                return std::unexpected(Error{ErrorCode::overflow});
            auto uncertainty=clock.uncertainty_ps+boundary.quantization_uncertainty_ps;
            auto first=subtract(boundary.time,from_picoseconds(uncertainty));
            auto last=add(boundary.time,from_picoseconds(uncertainty));
            if(!first || !last || *first<window->earliest || *last>window->latest) continue;
            auto d=boundary.time>=requested ? difference(boundary.time,requested) : difference(requested,boundary.time);
            if(!d) return std::unexpected(d.error());
            if(!selected || *d<distance || (*d==distance && boundary.time<selected->time)) { selected=boundary; distance=*d; }
        } else if(!selected || boundary.time<selected->time) selected=boundary;
    }
    if(!selected) return std::unexpected(Error{ErrorCode::invalid_state});
    return *selected;
}
} // namespace vita::runtime::timing
