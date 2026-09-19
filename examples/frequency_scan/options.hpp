#pragma once
#include <vita/profiles/iq/frequency_scan.hpp>
#include <charconv>
#include <span>
#include <string_view>
namespace vita::examples::frequency_scan {
struct Options { profiles::iq::SweepConfig sweep{}; profiles::iq::SceneConfig scene{}; bool help=false; };
inline Result<Options> parse_options(std::span<const std::string_view> args) noexcept {
    Options out;
    bool explicit_sweeps=false;
    for(std::size_t i=0;i<args.size();++i) {
        const auto key=args[i];
        if(key=="--help"){out.help=true;continue;}
        if(key=="--continuous"){out.sweep.continuous=true;continue;}
        if(i+1==args.size())return std::unexpected(Error{ErrorCode::invalid_argument,i});
        const auto text=args[++i];std::uint64_t value=0;
        const auto parsed=std::from_chars(text.data(),text.data()+text.size(),value);
        if(text.empty() || parsed.ec!=std::errc{} || parsed.ptr!=text.data()+text.size())return std::unexpected(Error{ErrorCode::invalid_argument,i});
        if(key=="--start-hz")out.sweep.start_hz=value;
        else if(key=="--stop-hz")out.sweep.stop_hz=value;
        else if(key=="--step-hz")out.sweep.step_hz=value;
        else if(key=="--sweeps"){out.sweep.sweeps=value;explicit_sweeps=true;}
        else if(key=="--sample-rate-hz")out.scene.sample_rate=value;
        else if(key=="--dwell-ms" || key=="--timeout-ms") {
            if(!value)return std::unexpected(Error{ErrorCode::invalid_argument,i});
            if(value>UINT64_MAX/1'000'000)return std::unexpected(Error{ErrorCode::overflow,i});
            if(key=="--dwell-ms")out.sweep.dwell_ns=value*1'000'000;
            else out.sweep.timeout_ns=value*1'000'000;
        } else return std::unexpected(Error{ErrorCode::invalid_argument,i-1});
    }
    if(explicit_sweeps && out.sweep.continuous)return std::unexpected(Error{ErrorCode::invalid_argument});
    auto sweep=profiles::iq::SweepPolicy::create(out.sweep);if(!sweep)return std::unexpected(sweep.error());
    auto scene=profiles::iq::VirtualRfScene::create(out.scene);if(!scene)return std::unexpected(scene.error());
    return out;
}
inline constexpr std::string_view usage="--start-hz N --stop-hz N --step-hz N --sample-rate-hz N --dwell-ms N --timeout-ms N --sweeps N [--continuous] [--help]";
} // namespace vita::examples::frequency_scan
