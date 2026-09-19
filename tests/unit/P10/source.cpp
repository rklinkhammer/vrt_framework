#include <vita/profiles/iq/source.hpp>
#include <array>
#include <cassert>
#include <cfenv>
#include <cstdlib>
#include <new>
using namespace vita;
using namespace vita::profiles::iq;
static std::size_t allocations = 0;
void* operator new(std::size_t n) { ++allocations; if (auto p=std::malloc(n?n:1)) return p; std::abort(); }
void* operator new[](std::size_t n) { return ::operator new(n); }
void operator delete(void* p) noexcept { std::free(p); }
void operator delete[](void* p) noexcept { std::free(p); }
std::uint32_t word(Bytes bytes,std::size_t offset) {
    std::uint32_t value=0;
    for(unsigned i=0;i<4;++i) value=(value<<8)|std::to_integer<std::uint8_t>(bytes[offset+i]);
    return value;
}
int main() {
    runtime::StateSnapshot config;
    std::array<std::byte,128> payload{};
    const auto baseline=allocations;
    assert(bytes_per_pair(SampleFormat::iq16)==4 && bytes_per_pair(SampleFormat::iq32)==8);
    assert(baseline_packet_class(SampleFormat::float32)==3);
    auto window=SampleWriteWindow::create(MutableBytes{payload}.first(64),SampleFormat::iq16,0,16,config);
    assert(window && !window->validate_complete());
    assert(default_source().produce(*window));
    constexpr std::array<std::uint32_t,16> expected{
        0x40000000,0x3b21187e,0x2d412d41,0x187e3b21,
        0x00004000,0xe7823b21,0xd2bf2d41,0xc4df187e,
        0xc0000000,0xc4dfe782,0xd2bfd2bf,0xe782c4df,
        0x0000c000,0x187ec4df,0x2d41d2bf,0x3b21e782
    };
    for(std::size_t i=0;i<16;++i) assert(word(payload,i*4)==expected[i]);
    auto shifted=SampleWriteWindow::create(MutableBytes{payload}.first(8),SampleFormat::iq16,15,2,config);
    assert(shifted && default_source().produce(*shifted));
    assert(word(payload,0)==expected[15] && word(payload,4)==expected[0]);
    auto ties=SampleWriteWindow::create(MutableBytes{payload}.first(16),SampleFormat::iq16,0,4,config);
    assert(ties && ties->write(0,0.5/32768.0,1.5/32768.0));
    assert(ties->write(1,2.5/32768.0,3.5/32768.0));
    assert(ties->write(2,-0.5/32768.0,-1.5/32768.0));
    assert(ties->write(3,3,-3) && ties->validate_complete());
    assert(word(payload,0)==0x00000002 && word(payload,4)==0x00020004);
    assert(word(payload,8)==0x0000fffe && word(payload,12)==0x7fff8000);
    auto wide=SampleWriteWindow::create(MutableBytes{payload}.first(8),SampleFormat::iq32,0,1,config);
    assert(wide && wide->write(0,1.5/2147483648.0,-3.5/2147483648.0));
    assert(word(payload,0)==2 && word(payload,4)==0xfffffffc);
    payload.fill(std::byte{0x55});
    auto invalid=SampleWriteWindow::create(MutableBytes{payload}.first(8),SampleFormat::float32,0,1,config);
    assert(invalid && !invalid->write(0,1,std::numeric_limits<double>::quiet_NaN()));
    assert(word(payload,0)==0x55555555 && !invalid->validate_complete());
    assert(!invalid->write(0,std::numeric_limits<double>::infinity(),0));
    assert(!invalid->write(0,std::numeric_limits<double>::max(),0));
    assert(invalid->write(0,0.5,-0.25) && invalid->validate_complete());
    assert(word(payload,0)==0x3f000000 && word(payload,4)==0xbe800000);
    assert(!SampleWriteWindow::create(MutableBytes{payload}.first(8),SampleFormat::iq16,UINT64_MAX,2,config));
    assert(!SampleWriteWindow::create(MutableBytes{payload}.first(3),SampleFormat::iq16,0,1,config));
    assert(!SampleWriteWindow::create(MutableBytes{payload}.first(4),static_cast<SampleFormat>(99),0,1,config));
    auto final_ordinal=SampleWriteWindow::create(MutableBytes{payload}.first(4),SampleFormat::iq16,UINT64_MAX,1,config);
    assert(final_ordinal && default_source().produce(*final_ordinal) && word(payload,0)==expected[15]);
    auto floats=SampleWriteWindow::create(payload,SampleFormat::float32,0,16,config);
    assert(floats);
    const auto old_round=std::fegetround();
    assert(std::fesetround(FE_UPWARD)==0 && default_source().produce(*floats));
    assert(word(payload,8)==0x3eec835e && word(payload,12)==0x3e43ef15);
    assert(std::fesetround(old_round)==0);
    assert(allocations==baseline);
}
