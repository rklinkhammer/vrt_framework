#include <vita/codec/ieee_samples.hpp>
#include <array>
#include <cassert>
#include <cstdio>
#include <type_traits>
extern "C" {
#include "softfloat.h"
}
using namespace vita;
namespace I=vita::codec::ieee;
namespace N=vita::codec::numeric;
static constexpr I::Policy policy(N::Rounding r) {return {N::Precision::allow_rounding,r,I::Overflow::ieee_result,I::NonFinite::propagate,I::NaNPayload::preserve_high};}
static void compare(I::Format src,std::uint64_t input,I::Format dst,N::Rounding rounding,std::uint64_t reference,unsigned flags) {
    auto actual=I::convert(src,input,dst,policy(rounding));assert(actual);
    assert(actual->bits==reference);assert(actual->flags.invalid==bool(flags&softfloat_flag_invalid));
    assert(actual->flags.inexact==bool(flags&softfloat_flag_inexact));assert(actual->flags.overflow==bool(flags&softfloat_flag_overflow));
    assert(actual->flags.underflow==bool(flags&softfloat_flag_underflow));
    const auto category=I::decode_exact(src,input)->category;
    if(category==I::Category::finite) {
        auto exact=policy(rounding);exact.precision=N::Precision::exact_only;
        const auto e=I::convert(src,input,dst,exact);assert(bool(e)==!bool(flags&softfloat_flag_inexact));
        if(flags&softfloat_flag_overflow) {
            auto saturate=policy(rounding);saturate.overflow=I::Overflow::saturate_finite;
            auto clamped=I::convert(src,input,dst,saturate);assert(clamped&&clamped->flags.overflow);
            const bool neg=I::decode_exact(src,input)->negative;
            const auto expected=dst==I::Format::binary16?0x7bffull:dst==I::Format::binary32?0x7f7fffffull:0x7fefffffffffffffull;
            const auto sign=neg?(dst==I::Format::binary16?0x8000ull:dst==I::Format::binary32?0x80000000ull:0x8000000000000000ull):0;
            assert(clamped->bits==(expected|sign));
        }
    }
}
int main() {
    static_assert(!std::is_default_constructible_v<I::Policy>);
    constexpr std::array modes{N::Rounding::nearest_ties_even,N::Rounding::toward_zero,N::Rounding::toward_negative,N::Rounding::toward_positive};
    constexpr std::array<unsigned,4> reference_modes{softfloat_round_near_even,softfloat_round_minMag,softfloat_round_min,softfloat_round_max};
    std::size_t count=0;
    softfloat_detectTininess=softfloat_tininess_afterRounding;
    for(unsigned m=0;m<4;++m) {
        softfloat_roundingMode=reference_modes[m];
        for(unsigned raw=0;raw<65536;++raw) {
            const auto decoded=I::decode_exact(I::Format::binary16,raw);assert(decoded);
            const auto e=(raw>>10)&31,f=raw&1023;
            const auto category=e==31?(f?((f&512)?I::Category::quiet_nan:I::Category::signaling_nan):I::Category::infinity):e||f?I::Category::finite:I::Category::zero;
            assert(decoded->category==category&&decoded->negative==bool(raw&32768));
            softfloat_exceptionFlags=0;auto widened=f16_to_f32(float16_t{static_cast<std::uint16_t>(raw)});
            compare(I::Format::binary16,raw,I::Format::binary32,modes[m],widened.v,softfloat_exceptionFlags);++count;
        }
        // Every binary32 exponent, asymmetric mantissa edges, both signs.
        constexpr std::array<std::uint32_t,7> fractions{0,1,0x1fff,0x2000,0x3fffff,0x400001,0x7fffff};
        for(unsigned exponent=0;exponent<256;++exponent)for(auto fraction:fractions)for(unsigned negative=0;negative<2;++negative) {
            const auto raw=(negative<<31)|(exponent<<23)|fraction;
            softfloat_exceptionFlags=0;auto narrow=f32_to_f16(float32_t{raw});
            compare(I::Format::binary32,raw,I::Format::binary16,modes[m],narrow.v,softfloat_exceptionFlags);++count;
        }
        std::uint64_t sequence=0xd177be9ac8436025ull;
        for(unsigned sample=0;sample<25000;++sample) {
            sequence=sequence*6364136223846793005ull+1442695040888963407ull;
            softfloat_exceptionFlags=0;auto half=f64_to_f16(float64_t{sequence});
            compare(I::Format::binary64,sequence,I::Format::binary16,modes[m],half.v,softfloat_exceptionFlags);++count;
            softfloat_exceptionFlags=0;auto single=f64_to_f32(float64_t{sequence});
            compare(I::Format::binary64,sequence,I::Format::binary32,modes[m],single.v,softfloat_exceptionFlags);++count;
        }
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
    std::printf("Independent IEEE reference comparisons: %zu; literal policy/invariant cases PASS\n",count);
}
