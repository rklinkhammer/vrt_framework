#include <vita/runtime/transaction/engine.hpp>
#include "sink.hpp"
#include <cstdlib>
#include <new>
using namespace vita;using namespace vita::codec;using namespace vita::runtime;using namespace vita::runtime::transaction;
static std::size_t allocations=0;
void* operator new(std::size_t n){++allocations;if(void* p=std::malloc(n?n:1))return p;std::abort();}
void* operator new[](std::size_t n){return ::operator new(n);}
void* operator new(std::size_t n,std::align_val_t alignment){++allocations;void* p=nullptr;if(!posix_memalign(&p,static_cast<std::size_t>(alignment),n?n:1))return p;std::abort();}
void* operator new[](std::size_t n,std::align_val_t a){return ::operator new(n,a);}
void operator delete(void* p) noexcept{std::free(p);}void operator delete[](void* p) noexcept{std::free(p);}
void operator delete(void* p,std::align_val_t) noexcept{std::free(p);}void operator delete[](void* p,std::align_val_t) noexcept{std::free(p);}
int main(){
    AdmissionPool pool(AdmissionPool::reference_capacities());VirtualBackend<> backend;VerifySink sink;StateSnapshot initial;initial.fields[1].validity=Validity::known;initial.fields[1].value=*Hertz::from_integer(1);Engine<1> engine(pool,backend.binding(),initial,{Profile::iq_generator_v1,sink.binding()});
    Envelope e;e.type=PacketType::command;e.stream_id=1;e.command=Command{0xa91c0000,42,Identifier::short_id(2),Identifier::short_id(3)};std::array<std::byte,256> wire,reply;
    const auto before=allocations;
    for(unsigned iteration=0;iteration<1000;++iteration){
        ControlPacket p;p.set<SampleRate>(*Hertz::from_integer(iteration+2));auto n=encode_packet(e,p.freeze(),wire);if(!n)return 1;auto parsed=decode_packet(Bytes{wire}.first(*n));if(!parsed)return 2;
        auto h=engine.accept(*parsed,{});if(!h||!engine.progress({})||!backend.complete_next()||!engine.progress({})||!*engine.complete(*h))return 3;
        for(;;){auto ack=engine.take_response(*h);if(!ack)return 4;if(!*ack)break;if(!encode_response(**ack,reply))return 5;}
        if(!engine.release(*h))return 6;sink.clear();
    }
    return allocations==before?0:7;
}
