#pragma once
#include <vita/core/error.hpp>
#include <compare>
#include <cstdint>
#include <limits>
namespace vita::runtime::timing {
inline constexpr std::uint64_t picoseconds_per_second=1000000000000ULL;
struct ProtocolTime { std::uint64_t seconds{},picoseconds{}; friend auto operator<=>(ProtocolTime,ProtocolTime)=default; };
struct Duration { std::uint64_t seconds{},picoseconds{}; friend auto operator<=>(Duration,Duration)=default; };
struct MonoTime { std::uint64_t ns{}; friend auto operator<=>(MonoTime,MonoTime)=default; };
inline bool valid(ProtocolTime t) noexcept { return t.picoseconds<picoseconds_per_second; }
inline bool valid(Duration d) noexcept { return d.picoseconds<picoseconds_per_second; }
inline Duration from_picoseconds(std::uint64_t ps) noexcept { return {ps/picoseconds_per_second,ps%picoseconds_per_second}; }
inline Duration from_nanoseconds(std::uint64_t ns) noexcept { return {ns/1000000000ULL,(ns%1000000000ULL)*1000}; }
inline Result<ProtocolTime> add(ProtocolTime t,Duration d) noexcept {
    if(!valid(t)||!valid(d)) return std::unexpected(Error{ErrorCode::invalid_argument});
    auto fraction=t.picoseconds+d.picoseconds; auto carry=fraction/picoseconds_per_second;
    if(d.seconds>std::numeric_limits<std::uint64_t>::max()-t.seconds || carry>std::numeric_limits<std::uint64_t>::max()-t.seconds-d.seconds)
        return std::unexpected(Error{ErrorCode::overflow});
    return ProtocolTime{t.seconds+d.seconds+carry,fraction%picoseconds_per_second};
}
inline Result<ProtocolTime> subtract(ProtocolTime t,Duration d) noexcept {
    if(!valid(t)||!valid(d)) return std::unexpected(Error{ErrorCode::invalid_argument});
    if(ProtocolTime{d.seconds,d.picoseconds}>t) return std::unexpected(Error{ErrorCode::overflow});
    if(t.picoseconds<d.picoseconds) return ProtocolTime{t.seconds-d.seconds-1,picoseconds_per_second+t.picoseconds-d.picoseconds};
    return ProtocolTime{t.seconds-d.seconds,t.picoseconds-d.picoseconds};
}
inline Result<Duration> difference(ProtocolTime later,ProtocolTime earlier) noexcept {
    auto t=subtract(later,Duration{earlier.seconds,earlier.picoseconds});
    if(!t) return std::unexpected(t.error()); return Duration{t->seconds,t->picoseconds};
}
inline Result<std::uint64_t> as_picoseconds(Duration d) noexcept {
    if(!valid(d)) return std::unexpected(Error{ErrorCode::invalid_argument});
    if(d.seconds>(std::numeric_limits<std::uint64_t>::max()-d.picoseconds)/picoseconds_per_second) return std::unexpected(Error{ErrorCode::overflow});
    return d.seconds*picoseconds_per_second+d.picoseconds;
}
inline Result<MonoTime> deadline(MonoTime now,std::uint64_t delay_ns) noexcept {
    if(delay_ns>std::numeric_limits<std::uint64_t>::max()-now.ns) return std::unexpected(Error{ErrorCode::overflow});
    return MonoTime{now.ns+delay_ns};
}
struct MonotonicClock {
    MonoTime (*read)(void*) noexcept{}; void* context{};
    Result<MonoTime> now() const noexcept { if(!read) return std::unexpected(Error{ErrorCode::invalid_state}); return read(context); }
};
} // namespace vita::runtime::timing
