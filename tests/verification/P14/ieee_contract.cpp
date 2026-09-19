#include <vita/codec/ieee_samples.hpp>
#include <array>
#include <cassert>
#include <cstdio>
#include <type_traits>
using namespace vita;
namespace I=vita::codec::ieee;
namespace N=vita::codec::numeric;
static constexpr I::Policy policy(N::Rounding r) {return {N::Precision::allow_rounding,r,I::Overflow::ieee_result,I::NonFinite::propagate,I::NaNPayload::preserve_high};}
int main() {
    static_assert(!std::is_default_constructible_v<I::Policy>);
    for(unsigned raw=0;raw<65536;++raw) {
        auto d=I::decode_exact(I::Format::binary16,raw);assert(d);
        const auto e=(raw>>10)&31,f=raw&1023;
        const auto category=e==31?(f?((f&512)?I::Category::quiet_nan:I::Category::signaling_nan):I::Category::infinity):e||f?I::Category::finite:I::Category::zero;
        assert(d->category==category&&d->negative==bool(raw&32768));
    }
    constexpr std::array modes{N::Rounding::nearest_ties_even,N::Rounding::toward_zero,N::Rounding::toward_negative,N::Rounding::toward_positive};
    for(unsigned mode=0;mode<4;++mode)for(bool sign:{false,true}) {
        // Exact midpoint between half1.0 and next representable half, using exact binary rational.
        auto p=policy(modes[mode]);auto rounded=I::encode_finite(I::Format::binary16,{2049,-11,sign},p);assert(rounded);
        const bool up=mode==3?!sign:mode==2?sign:false;
        assert(rounded->bits==((sign?0x8000u:0u)|0x3c00u|unsigned(up)));assert(rounded->flags.inexact&&!rounded->flags.underflow&&!rounded->flags.overflow);
        p.precision=N::Precision::exact_only;assert(!I::encode_finite(I::Format::binary16,{2049,-11,sign},p));
        p=policy(modes[mode]);auto tiny=I::encode_finite(I::Format::binary16,{1,-25,sign},p);assert(tiny&&tiny->bits==((sign?0x8000u:0u)|unsigned(up))&&tiny->flags.underflow&&tiny->flags.inexact);
        auto exact=I::encode_finite(I::Format::binary16,{1,-24,sign},p);assert(exact&&exact->bits==((sign?0x8000u:0u)|1u)&&!exact->flags.underflow&&!exact->flags.inexact);
        p.overflow=I::Overflow::saturate_finite;auto large=I::encode_finite(I::Format::binary16,{1,100,sign},p);assert(large&&large->bits==((sign?0x8000u:0u)|0x7bffu)&&large->flags.overflow&&large->flags.inexact);
        p.overflow=I::Overflow::error;assert(!I::encode_finite(I::Format::binary16,{1,100,sign},p));
    }
    auto p=policy(N::Rounding::nearest_ties_even);
    auto boundary=I::convert(I::Format::binary64,0x3f0ffc0000000000ull,I::Format::binary16,p);
    assert(boundary&&boundary->bits==0x0400&&boundary->flags.underflow&&boundary->flags.inexact);
    assert(I::convert(I::Format::binary64,0x8000000000000000ull,I::Format::binary16,p)->bits==0x8000);
    // Explicit NaN policies: negative quiet payload with only one discarded low bit.
    constexpr auto nan=0xfff8000000000001ull;
    auto converted=I::convert(I::Format::binary64,nan,I::Format::binary16,p);assert(converted&&converted->bits==0xfe00&&converted->flags.payload_loss&&!converted->flags.invalid);
    p.payload=I::NaNPayload::exact;assert(!I::convert(I::Format::binary64,nan,I::Format::binary16,p));
    p.payload=I::NaNPayload::canonical;converted=I::convert(I::Format::binary64,nan,I::Format::binary16,p);assert(converted&&converted->bits==0x7e00&&converted->flags.payload_loss);
    converted=I::convert(I::Format::binary16,0x7c01,I::Format::binary16,p);assert(converted&&converted->bits==0x7e00&&converted->flags.invalid);
    p.nonfinite=I::NonFinite::reject;assert(!I::convert(I::Format::binary16,0x7c00,I::Format::binary64,p));
    assert(!I::decode_exact(I::Format::binary16,0x10000));assert(!I::decode_exact(static_cast<I::Format>(9),0));
    auto zero=I::decode_exact(I::Format::binary16,0x8000);assert(zero);assert(I::finite_value(*zero)&&!I::finite_value(*zero)->negative);
    auto forged=*zero;forged.finite.magnitude=1;assert(!I::finite_value(forged));
    std::printf("Independent IEEE portable classification/policy cases PASS\n");
}
