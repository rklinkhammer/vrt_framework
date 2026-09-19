#include <vita/codec/packet.hpp>
#include <cstdlib>
#include <new>
#include <cassert>
using namespace vita;
static std::size_t allocations=0;
void* operator new(std::size_t n){++allocations;if(void*p=std::malloc(n?n:1))return p;std::abort();}
void* operator new[](std::size_t n){return ::operator new(n);}
void* operator new(std::size_t n,std::align_val_t a){++allocations;void*p=nullptr;if(!posix_memalign(&p,std::size_t(a),n?n:1))return p;std::abort();}
void* operator new[](std::size_t n,std::align_val_t a){return ::operator new(n,a);}
void operator delete(void*p)noexcept{std::free(p);}void operator delete[](void*p)noexcept{std::free(p);}
void operator delete(void*p,std::align_val_t)noexcept{std::free(p);}void operator delete[](void*p,std::align_val_t)noexcept{std::free(p);}
int main(){
    auto* ordinary=::operator new(32);auto* aligned=::operator new(64,std::align_val_t{64});assert(allocations==2);::operator delete(ordinary);::operator delete(aligned,std::align_val_t{64});
    const auto before=allocations;
    for(unsigned i=0;i<1000;++i){ContextPacket p;
        assert(p.set<Bandwidth>(Hertz{static_cast<std::int64_t>(i)}));assert(p.set<IFReferenceFrequency>(Hertz{-1}));assert(p.set<RFReferenceFrequency>(Hertz{1}));assert(p.set<RFReferenceFrequencyOffset>(Hertz{-2}));assert(p.set<IFBandOffset>(Hertz{2}));assert(p.set<ReferenceLevel>(DecibelsQ7{-1}));assert(p.set<Gain>(GainStages{-1,1}));assert(p.set<OverRangeCount>(i));assert(p.set<TimestampAdjustment>(Femtoseconds{-1}));assert(p.set<TimestampCalibrationTime>(i));assert(p.set<Temperature>(CelsiusQ6{1}));assert(p.set<DeviceIdentifier>(DeviceIdentifierValue{0xffffff,1}));assert(p.set<EphemerisReferenceId>(i));
        auto snapshot=p.freeze();auto measured=vita::measure(snapshot);assert(measured);std::array<std::byte,256> out{};codec::Envelope e;e.type=codec::PacketType::context;e.stream_id=1;auto encoded=codec::encode_packet(e,snapshot,out);assert(encoded);auto parsed=codec::decode_packet(Bytes{out}.first(*encoded));assert(parsed&&parsed->fields.size()==13);for(std::size_t f=0;f<parsed->fields.size();++f)assert(parsed->fields[f].value());
    }
    assert(allocations==before);
}
