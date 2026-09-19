#pragma once
#include <vita/runtime/timing/time.hpp>
#include <numeric>
namespace vita::runtime::timing {
struct ExactSampleTime { ProtocolTime time{}; std::uint64_t numerator{},denominator{1}; };
class SampleTimeline {
    ExactSampleTime current_{};
    std::uint64_t rate_{1},ordinal_{},revision_{};
    static Result<std::uint64_t> common_denominator(std::uint64_t a,std::uint64_t b) noexcept {
        auto factor=a/std::gcd(a,b);
        if(factor>std::numeric_limits<std::uint64_t>::max()/b) return std::unexpected(Error{ErrorCode::resource_limit});
        return factor*b;
    }
public:
    static Result<SampleTimeline> create(ProtocolTime origin,std::uint64_t rate) noexcept {
        if(!valid(origin)||!rate||rate>100000000) return std::unexpected(Error{ErrorCode::invalid_argument});
        SampleTimeline out; out.current_.time=origin; out.rate_=rate; return out;
    }
    Result<void> reanchor_mapping(ProtocolTime old_now,ProtocolTime new_now) noexcept {
        auto delta=new_now>=old_now?difference(new_now,old_now):difference(old_now,new_now);
        if(!delta)return std::unexpected(delta.error());
        auto shifted=new_now>=old_now?add(current_.time,*delta):subtract(current_.time,*delta);
        if(!shifted)return std::unexpected(shifted.error());current_.time=*shifted;return {};
    }
    ExactSampleTime exact_time() const noexcept { return current_; }
    ProtocolTime time() const noexcept { return current_.time; }
    std::uint64_t ordinal() const noexcept { return ordinal_; }
    std::uint64_t rate() const noexcept { return rate_; }
    std::uint64_t revision() const noexcept { return revision_; }
    Result<void> change_rate(std::uint64_t rate) noexcept {
        if(!rate||rate>100000000) return std::unexpected(Error{ErrorCode::invalid_argument});
        if(revision_==std::numeric_limits<std::uint64_t>::max()) return std::unexpected(Error{ErrorCode::overflow});
        // Ensure the new segment can preserve its inherited exact fractional endpoint.
        auto denominator=common_denominator(current_.denominator,rate/std::gcd(picoseconds_per_second,rate)); if(!denominator) return std::unexpected(denominator.error());
        rate_=rate; ++revision_; return {};
    }
    Result<void> advance(std::uint64_t samples) noexcept {
        if(samples>std::numeric_limits<std::uint64_t>::max()-ordinal_) return std::unexpected(Error{ErrorCode::overflow});
        auto seconds=samples/rate_,remaining=samples%rate_;
        auto quotient=picoseconds_per_second/rate_,step_remainder=picoseconds_per_second%rate_;
        // Products bounded by 1e12 and (1e8-1)^2 respectively, no wide integer extension.
        auto remainder_product=remaining*step_remainder;
        auto fraction=remaining*quotient+remainder_product/rate_;
        auto residue=remainder_product%rate_;
        auto divisor=std::gcd(picoseconds_per_second,rate_),step_denominator=rate_/divisor;
        auto denominator=common_denominator(current_.denominator,step_denominator); if(!denominator) return std::unexpected(denominator.error());
        auto a=current_.numerator*(*denominator/current_.denominator),b=(residue/divisor)*(*denominator/step_denominator);
        bool carry=a>=*denominator-b; auto numerator=carry ? a-(*denominator-b) : a+b;
        auto next=add(current_.time,Duration{seconds,fraction}); if(!next) return std::unexpected(next.error());
        if(carry) { next=add(*next,Duration{0,1}); if(!next) return std::unexpected(next.error()); }
        auto gcd=std::gcd(numerator,*denominator);
        current_={*next,numerator/gcd,*denominator/gcd}; ordinal_+=samples; return {};
    }
};
} // namespace vita::runtime::timing
