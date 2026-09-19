#include <vita/profiles/iq/lab.hpp>
#include <cassert>
#include <cstdlib>
#include <new>
#include <iostream>
using namespace vita;
using namespace vita::profiles::iq;
static std::size_t allocations = 0;
void* operator new(std::size_t n) { ++allocations; if (auto p=std::malloc(n?n:1)) return p; std::abort(); }
void* operator new[](std::size_t n) { return ::operator new(n); }
void operator delete(void* p) noexcept { std::free(p); }
void operator delete[](void* p) noexcept { std::free(p); }
void* operator new(std::size_t n,std::align_val_t alignment) {
    ++allocations; void* pointer=nullptr;
    if (!posix_memalign(&pointer,static_cast<std::size_t>(alignment),n?n:1)) return pointer;
    std::abort();
}
void* operator new[](std::size_t n,std::align_val_t a) { return ::operator new(n,a); }
void operator delete(void* p,std::align_val_t) noexcept { std::free(p); }
void operator delete[](void* p,std::align_val_t) noexcept { std::free(p); }
int main() {
    const auto initial=allocations;
    auto config=lab::config(0x00a1b2);
    assert(config&&config->oui==0x00a1b2&&config->isolated_lab&&config->clock.injected&&!config->clock.qualified);
    assert(!lab::config(0x1000000));
    lab::PoolCounts counts;
    auto zero=counts; zero.rx_cancellation=0; assert(!lab::pools(zero));
    auto overflow=counts; overflow.header=SIZE_MAX; assert(!lab::pools(overflow));
    auto misaligned=counts; misaligned.payload_bytes=2049; assert(!lab::pools(misaligned));
    auto too_small=counts; too_small.max_setup_bytes=1; assert(!lab::pools(too_small));
    assert(allocations==initial);
    auto expected=lab::measure(counts); assert(expected);
    auto reference=lab::measure(lab::reference_counts()); assert(reference&&reference->raw_bytes==30998528);
    auto created=lab::pools(counts); assert(created&&created->large.block_count()==0);
    auto larger=counts; larger.large=2; auto with_large=lab::pools(larger); assert(with_large&&with_large->large.block_count()==2);
    auto large_lease=with_large->large.acquire({8192,64}); assert(large_lease);
    std::array providers{&created->header,&created->payload,&created->trailer,&created->control,
                         &created->cancellation,&created->rx_data,&created->rx_control,
                         &created->rx_cancellation,&created->emergency};
    std::size_t raw=0,metadata=0;
    for(std::size_t i=0;i<providers.size();++i) {
        for(std::size_t j=0;j<i;++j) assert(!providers[i]->shares_provider_with(*providers[j]));
        auto bytes=providers[i]->raw_bytes(); assert(bytes); raw+=*bytes; metadata+=providers[i]->metadata_bytes();
        auto lease=providers[i]->acquire({1,64}); assert(lease);
        auto mapped=lease->writable_bytes(); assert(mapped&&reinterpret_cast<std::uintptr_t>(mapped->data())%64==0);
    }
    assert(raw==expected->raw_bytes&&metadata==expected->provider_metadata_bytes&&expected->pool_frontend_bytes==sizeof(ExternalPools));
    const auto setup_allocations=allocations;
    for(std::size_t i=0;i<1000;++i) {
        auto lease=created->payload.acquire({128,64}); assert(lease&&lease->set_size(128));
        auto bytes=lease->writable_bytes(); assert(bytes); bytes->front()=std::byte{42};
    }
    assert(allocations==setup_allocations);
    memory::BufferLease surviving;
    {
        auto lease=created->payload.acquire({128,64}); assert(lease&&lease->set_size(128));
        lease->writable_bytes()->front()=std::byte{77}; surviving=std::move(*lease);
    }
    created=ExternalPools{}; // all factory/front-end references die; the lease pins backing
    assert(surviving.bytes()->front()==std::byte{77}); surviving.reset();
    std::cout<<"lab raw="<<raw<<" provider_metadata="<<metadata<<" pool_frontends="<<sizeof(ExternalPools)<<'\n';
}
