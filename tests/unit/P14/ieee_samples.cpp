#include <vita/codec/ieee_samples.hpp>
#include <cassert>
#include <cstdlib>
#include <new>
#include <type_traits>
using namespace vita::codec;
static unsigned allocations=0;
void* operator new(std::size_t n) {++allocations; if(auto p=std::malloc(n))return p; std::abort();}
void operator delete(void* p) noexcept {std::free(p);}
void operator delete(void* p,std::size_t) noexcept {std::free(p);}
constexpr ieee::Policy policy{numeric::Precision::allow_rounding,numeric::Rounding::nearest_ties_even,
                             ieee::Overflow::ieee_result,ieee::NonFinite::propagate,ieee::NaNPayload::preserve_high};
int main() {
    static_assert(!std::is_default_constructible_v<ieee::Policy>);
    const auto before=allocations;
    for(auto format:{ieee::Format::binary16,ieee::Format::binary32,ieee::Format::binary64}) {
        const unsigned width=format==ieee::Format::binary16?16:format==ieee::Format::binary32?32:64;
        auto zero=ieee::decode_exact(format,std::uint64_t{1}<<(width-1));
        assert(zero && zero->category==ieee::Category::zero && zero->negative);
        assert(*ieee::finite_value(*zero)==(numeric::BinaryValue{0,0,false}));
        assert(ieee::encode_finite(format,{0,0,true},policy)->bits==(std::uint64_t{1}<<(width-1)));
    }
    assert(*ieee::finite_value(*ieee::decode_exact(ieee::Format::binary16,1))==(numeric::BinaryValue{1,-24,false}));
    assert(*ieee::finite_value(*ieee::decode_exact(ieee::Format::binary16,0x3c00))==(numeric::BinaryValue{1,0,false}));
    assert(ieee::convert(ieee::Format::binary64,0x8000000000000000ULL,ieee::Format::binary16,policy)->bits==0x8000);
    auto midpoint=ieee::convert(ieee::Format::binary64,0x3f0ffc0000000000ULL,ieee::Format::binary16,policy);
    assert(midpoint && midpoint->bits==0x400 && midpoint->flags.inexact && midpoint->flags.underflow);
    auto exact_subnormal=ieee::encode_finite(ieee::Format::binary16,{1,-24,false},policy);
    assert(exact_subnormal && exact_subnormal->bits==1 && !exact_subnormal->flags.underflow && !exact_subnormal->flags.inexact);
    auto tiny=ieee::encode_finite(ieee::Format::binary16,{1,-32768,true},policy);
    assert(tiny && tiny->bits==0x8000 && tiny->flags.inexact && tiny->flags.underflow);
    auto huge=ieee::encode_finite(ieee::Format::binary16,{1,32767,false},policy);
    assert(huge && huge->bits==0x7c00 && huge->flags.overflow && huge->flags.inexact);
    auto p=policy;p.overflow=ieee::Overflow::saturate_finite;
    assert(ieee::encode_finite(ieee::Format::binary16,{1,32767,true},p)->bits==0xfbff);
    p.overflow=ieee::Overflow::error;assert(!ieee::encode_finite(ieee::Format::binary16,{1,32767,false},p));
    p=policy;p.precision=numeric::Precision::exact_only;
    assert(!ieee::encode_finite(ieee::Format::binary16,{1,-25,false},p));
    assert(!ieee::encode_finite(ieee::Format::binary16,{1,32767,false},p));
    assert(ieee::encode_finite(ieee::Format::binary64,{UINT64_MAX,0,false},policy)->bits==0x43f0000000000000ULL);
    assert(!ieee::encode_finite(ieee::Format::binary64,{UINT64_MAX,0,false},p));
    // Signaling NaN conversion quiets; raw decode does not modify payload/class.
    auto nan=ieee::decode_exact(ieee::Format::binary64,0xfff0000000000001ULL);
    assert(nan && nan->negative && nan->category==ieee::Category::signaling_nan && nan->payload==1 && nan->payload_bits==51);
    assert(!ieee::finite_value(*nan));
    auto q=ieee::convert(ieee::Format::binary64,0xfff0000000000001ULL,ieee::Format::binary16,policy);
    assert(q && q->bits==0xfe00 && q->flags.invalid && q->flags.payload_loss && !q->flags.inexact);
    p=policy;p.payload=ieee::NaNPayload::exact;
    assert(!ieee::convert(ieee::Format::binary64,0xfff0000000000001ULL,ieee::Format::binary16,p));
    q=ieee::convert(ieee::Format::binary16,0xfc01,ieee::Format::binary64,p);
    assert(q && q->bits==0xfff8040000000000ULL && q->flags.invalid && !q->flags.payload_loss);
    p.payload=ieee::NaNPayload::canonical;
    q=ieee::convert(ieee::Format::binary16,0xfc01,ieee::Format::binary32,p);
    assert(q && q->bits==0x7fc00000 && q->flags.invalid && q->flags.payload_loss);
    p.nonfinite=ieee::NonFinite::reject;
    assert(!ieee::convert(ieee::Format::binary16,0x7c00,ieee::Format::binary32,p));
    assert(!ieee::convert(ieee::Format::binary16,0x7e00,ieee::Format::binary32,p));
    assert(!ieee::decode_exact(ieee::Format::binary16,0x10000));
    assert(!ieee::decode_exact(static_cast<ieee::Format>(99),0));
    p=policy;p.payload=static_cast<ieee::NaNPayload>(99);
    assert(!ieee::encode_finite(ieee::Format::binary16,{0,0,false},p));
    auto invalid=*ieee::decode_exact(ieee::Format::binary16,0x3c00);invalid.finite.negative=true;
    assert(!ieee::finite_value(invalid));
    // Every half code preserves sign/payload on widening; numerical conversion quiets signaling NaNs.
    for(unsigned h=0;h<65536;++h) {
        auto v=ieee::decode_exact(ieee::Format::binary16,h);assert(v);
        auto wide=ieee::convert(ieee::Format::binary16,h,ieee::Format::binary64,policy);assert(wide);
        auto back=ieee::convert(ieee::Format::binary64,wide->bits,ieee::Format::binary16,policy);assert(back);
        const unsigned expected=v->category==ieee::Category::signaling_nan?h|0x200:h;
        assert(back->bits==expected);
    }
    assert(allocations==before);
}
