#include <vita/codec/ieee_samples.hpp>
extern "C" {
#include "softfloat.h"
}
#include <cassert>
#include <cstdio>
#include <cstdlib>
using namespace vita::codec;
static unsigned long long comparisons=0;
static void check(ieee::Format from, std::uint64_t bits, ieee::Format to, numeric::Rounding r) {
    softfloat_roundingMode=r==numeric::Rounding::nearest_ties_even?softfloat_round_near_even:
        r==numeric::Rounding::toward_zero?softfloat_round_minMag:
        r==numeric::Rounding::toward_negative?softfloat_round_min:softfloat_round_max;
    softfloat_detectTininess=softfloat_tininess_afterRounding;
    softfloat_exceptionFlags=0;
    std::uint64_t expected=0;
    if(from==ieee::Format::binary16 && to==ieee::Format::binary32)expected=f16_to_f32(float16_t{static_cast<std::uint16_t>(bits)}).v;
    else if(from==ieee::Format::binary16 && to==ieee::Format::binary64)expected=f16_to_f64(float16_t{static_cast<std::uint16_t>(bits)}).v;
    else if(from==ieee::Format::binary32 && to==ieee::Format::binary16)expected=f32_to_f16(float32_t{static_cast<std::uint32_t>(bits)}).v;
    else if(from==ieee::Format::binary32 && to==ieee::Format::binary64)expected=f32_to_f64(float32_t{static_cast<std::uint32_t>(bits)}).v;
    else if(from==ieee::Format::binary64 && to==ieee::Format::binary16)expected=f64_to_f16(float64_t{bits}).v;
    else if(from==ieee::Format::binary64 && to==ieee::Format::binary32)expected=f64_to_f32(float64_t{bits}).v;
    else std::abort();
    const auto flags=softfloat_exceptionFlags;
    const ieee::Policy policy{numeric::Precision::allow_rounding,r,ieee::Overflow::ieee_result,ieee::NonFinite::propagate,ieee::NaNPayload::preserve_high};
    auto result=ieee::convert(from,bits,to,policy);
    if(!result || result->bits!=expected || result->flags.invalid!=bool(flags&softfloat_flag_invalid)
       || result->flags.inexact!=bool(flags&softfloat_flag_inexact)
       || result->flags.overflow!=bool(flags&softfloat_flag_overflow)
       || result->flags.underflow!=bool(flags&softfloat_flag_underflow)) {
        std::fprintf(stderr,"mismatch from%d to%d bits%llx r%d expected%llx flags%u actual%llx flags%i%i%i%i\n",
          int(from),int(to),bits,int(r),expected,unsigned(flags),result?result->bits:0,
          result?result->flags.invalid:0,result?result->flags.inexact:0,result?result->flags.overflow:0,result?result->flags.underflow:0);
        std::abort();
    }
    ++comparisons;
}
int main() {
    for(auto r:{numeric::Rounding::nearest_ties_even,numeric::Rounding::toward_zero,numeric::Rounding::toward_negative,numeric::Rounding::toward_positive}) {
        for(unsigned h=0;h<65536;++h) {
            check(ieee::Format::binary16,h,ieee::Format::binary32,r);
            check(ieee::Format::binary16,h,ieee::Format::binary64,r);
            auto f=f16_to_f32(float16_t{static_cast<std::uint16_t>(h)}).v;
            auto d=f16_to_f64(float16_t{static_cast<std::uint16_t>(h)}).v;
            check(ieee::Format::binary32,f,ieee::Format::binary16,r);
            check(ieee::Format::binary64,d,ieee::Format::binary16,r);
        }
        // Sweep every binary64 exponent and fraction edges, signs, plus fixed-seed patterns.
        for(unsigned e=0;e<2048;++e)for(auto f:{0ULL,1ULL,0x7ffffffffffffULL,0x8000000000000ULL,0xfffffffffffffULL})
            for(auto sign:{0ULL,0x8000000000000000ULL})for(auto dest:{ieee::Format::binary16,ieee::Format::binary32})
                check(ieee::Format::binary64,sign|(std::uint64_t(e)<<52)|f,dest,r);
        std::uint64_t state=0x69c01234ab87def1ULL;
        for(unsigned i=0;i<50000;++i) {
            state^=state<<13; state^=state>>7; state^=state<<17;
            check(ieee::Format::binary64,state,ieee::Format::binary16,r);
            check(ieee::Format::binary64,state,ieee::Format::binary32,r);
            check(ieee::Format::binary32,static_cast<std::uint32_t>(state),ieee::Format::binary16,r);
            check(ieee::Format::binary32,static_cast<std::uint32_t>(state),ieee::Format::binary64,r);
        }
        for(auto edge:{0x3e60000000000000ULL,0x3e70000000000000ULL,0x3f0ffc0000000000ULL,0x3f10000000000000ULL,
                       0x40effc0000000000ULL,0x40effe0000000000ULL,0x3690000000000000ULL,0x380fffffe0000000ULL,
                       0x3810000000000000ULL,0x47efffffe0000000ULL,0x47effffff0000000ULL})
            for(int delta=-4;delta<=4;++delta)for(auto sign:{0ULL,0x8000000000000000ULL})for(auto dest:{ieee::Format::binary16,ieee::Format::binary32})
                check(ieee::Format::binary64,(edge+delta)|sign,dest,r);
    }
    std::printf("%llu conversions matched pinned SoftFloat bits and flags\n",comparisons);
}
