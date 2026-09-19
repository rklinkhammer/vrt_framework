#pragma once
#include "options.hpp"
#include <array>

namespace vita::examples::frequency_scan {
enum class EndpointRole { controller, controllee };
struct LanePorts {
    std::uint16_t data=0,control=0,cancellation=0;
};
struct EndpointOptions {
    EndpointRole role=EndpointRole::controller;
    Options scan{};
    LanePorts local{},peer{};
    // This helper deliberately has no external-address or production-identity option.
    // The later binding must use these labels in its setup banner.
    static constexpr std::string_view address="127.0.0.1";
    static constexpr bool isolated_lab=true,simulated_pps=true;
};
inline Result<LanePorts> lane_ports(std::uint64_t base) noexcept {
    if(base==0 || base>65533)return std::unexpected(Error{ErrorCode::invalid_argument});
    return LanePorts{static_cast<std::uint16_t>(base),static_cast<std::uint16_t>(base+1),static_cast<std::uint16_t>(base+2)};
}
inline Result<EndpointOptions> parse_endpoint_options(EndpointRole role,std::span<const std::string_view> args) noexcept {
    if(role!=EndpointRole::controller && role!=EndpointRole::controllee)
        return std::unexpected(Error{ErrorCode::invalid_argument});
    EndpointOptions out;out.role=role;
    auto local=lane_ports(role==EndpointRole::controller?41000:42000);
    auto peer=lane_ports(role==EndpointRole::controller?42000:41000);
    std::array<std::string_view,32> scan_args{};
    std::size_t used=0;
    bool local_seen=false,peer_seen=false;
    for(std::size_t i=0;i<args.size();++i) {
        const auto key=args[i];
        if(key=="--local-base-port" || key=="--peer-base-port") {
            auto& seen=key=="--local-base-port"?local_seen:peer_seen;
            if(seen || i+1==args.size())return std::unexpected(Error{ErrorCode::invalid_argument,i});
            seen=true;
            const auto text=args[++i];std::uint64_t base=0;
            const auto parsed=std::from_chars(text.data(),text.data()+text.size(),base);
            if(text.empty() || parsed.ec!=std::errc{} || parsed.ptr!=text.data()+text.size())
                return std::unexpected(Error{ErrorCode::invalid_argument,i});
            auto ports=lane_ports(base);if(!ports)return std::unexpected(ports.error());
            if(key=="--local-base-port")local=ports;else peer=ports;
        } else {
            if(used==scan_args.size())return std::unexpected(Error{ErrorCode::capacity_exhausted});
            scan_args[used++]=key;
            if(key!="--help" && key!="--continuous") {
                if(i+1==args.size())return std::unexpected(Error{ErrorCode::invalid_argument,i});
                if(used==scan_args.size())return std::unexpected(Error{ErrorCode::capacity_exhausted});
                scan_args[used++]=args[++i];
            }
        }
    }
    // Localhost endpoints cannot bind overlapping lane ranges in separate processes.
    if(local->data<=peer->cancellation && peer->data<=local->cancellation)
        return std::unexpected(Error{ErrorCode::invalid_argument});
    auto scan=parse_options(std::span<const std::string_view>{scan_args}.first(used));
    if(!scan)return std::unexpected(scan.error());
    out.scan=*scan;out.local=*local;out.peer=*peer;return out;
}
inline constexpr std::string_view endpoint_usage=
    "Loopback isolated lab only; simulated PPS. --local-base-port N --peer-base-port N; "
    "each base assigns Data, Control/Context, cancellation in that order. "
    "--sample-rate-hz configures a local session/expected format; it does not tune a remote sample rate.";
} // namespace vita::examples::frequency_scan
