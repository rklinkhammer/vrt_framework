#include <vita/codec/layout.hpp>
#include <cstdlib>
#include <new>
using namespace vita;
static std::size_t allocations=0;
void* operator new(std::size_t size) { ++allocations;if(void* p=std::malloc(size))return p;std::abort(); }
void* operator new[](std::size_t size) { return ::operator new(size); }
void operator delete(void* p) noexcept { std::free(p); }
void operator delete[](void* p) noexcept { std::free(p); }
int main() {
    const auto before=allocations;
    for(unsigned i=0;i<1000;++i) {
        ContextPacket packet;
        if(!packet.set<SampleRate>(*Hertz::from_integer(i+1)) || !packet.set<ReferencePoint>(i))return 1;
        auto frozen=packet.freeze();auto measured=measure(frozen);auto index=index_layout<2>(frozen);
        if(!measured || !index || !packet.remove<ReferencePoint>())return 2;
        if(!validate_measure(frozen,*measured) || validate_measure(packet.freeze(),*measured))return 3;
    }
    return allocations==before ? 0 : 4;
}
