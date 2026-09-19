#include <vita/memory/memory.hpp>
#include <array>
#include <cstdlib>
#include <limits>
#include <memory>
#include <new>
using namespace vita;
using namespace vita::memory;
static std::size_t allocations=0;
void* operator new(std::size_t size) { ++allocations; if(void* p=std::malloc(size))return p;std::abort(); }
void* operator new[](std::size_t size) { return ::operator new(size); }
void operator delete(void* p) noexcept { std::free(p); }
void operator delete[](void* p) noexcept { std::free(p); }
struct Backing { std::array<std::byte,64> bytes{}; ExternalPool* pool{}; bool observed{},unavailable{}; };
static void observe_return(void* context,std::size_t) noexcept {
    auto& b=*static_cast<Backing*>(context);
    b.observed=b.pool->return_count()==0;
    b.unavailable=!b.pool->acquire({64,1});
}
int main() {
    {
        auto b=std::make_shared<Backing>();
        BufferSpec spec{b,b->bytes.data(),64,1,1,MemoryDomain::cpu,0,observe_return,b.get()};
        auto pool=ExternalPool::create(std::span{&spec,1});if(!pool)return 1;b->pool=&*pool;
        auto lease=pool->acquire({64,1});if(!lease)return 2;
        lease->reset();if(!b->observed || !b->unavailable || pool->return_count()!=1)return 3;
    }
    std::weak_ptr<Backing> weak;
    RetainedRx survivor;
    {
        auto b=std::make_shared<Backing>();weak=b;
        BufferSpec spec{b,b->bytes.data(),64,1,1};
        auto pool=ExternalPool::create(std::span{&spec,1});if(!pool)return 4;
        RetentionQuota local(1),global(1);RxEnvelope rx;
        const auto before=allocations;
        auto lease=pool->acquire({64,1});if(!lease || !lease->set_size(64))return 5;
        auto index=rx.add_buffer(std::move(*lease));if(!index || !rx.append_payload({*index,0,64}))return 6;
        auto retained=rx.retain(local,global);if(!retained)return 7;survivor=std::move(*retained);
        if(allocations!=before)return 8;
    }
    if(weak.expired() || !survivor.fragment(0))return 9;
    survivor.reset();if(!weak.expired())return 10;
    {
        auto owner=std::make_shared<unsigned>(0);
        const auto huge=std::numeric_limits<std::size_t>::max()/2+1;
        std::array specs{BufferSpec{owner,nullptr,huge,1,1,MemoryDomain::device},
                         BufferSpec{owner,nullptr,huge,1,1,MemoryDomain::device}};
        auto pool=ExternalPool::create(specs);if(!pool)return 11;
        auto a=pool->acquire({huge,1,MemoryDomain::device,false});
        auto b=pool->acquire({huge,1,MemoryDomain::device,false});
        if(!a || !b || !a->set_size(huge) || !b->set_size(huge))return 12;
        TxStorage tx;if(!tx.append(std::move(*a),0,huge))return 13;
        auto overflow=tx.append(std::move(*b),0,huge);
        if(overflow || overflow.error().code!=ErrorCode::overflow || !*b || tx.byte_size()!=huge)return 14;
    }
    return 0;
}
