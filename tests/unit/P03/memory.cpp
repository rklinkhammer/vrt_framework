#include <vita/memory/memory.hpp>
#include <array>
#include <cassert>
#include <iostream>
using namespace vita;
using namespace vita::memory;
struct alignas(64) Storage { std::array<std::byte,1024> data{}; std::size_t returned{}; };
int main() {
    auto storage=std::make_shared<Storage>();
    BufferSpec spec{storage,storage->data.data(),128,8,64};
    spec.callback_context=storage.get(); spec.on_return=[](void* p,std::size_t) noexcept { ++static_cast<Storage*>(p)->returned; };
    auto pool=ExternalPool::create(std::span(&spec,1)); assert(pool);
    auto alias=*pool; assert(pool->shares_provider_with(alias)); assert(!pool->shares_provider_with(ExternalPool{}));
    alias=ExternalPool{};
    RetentionQuota local(2),global(3);
    RetainedRx held;
    {
        auto lease=pool->acquire({100,64}); assert(lease); assert(lease->set_size(100));
        auto writable=lease->writable_bytes(); assert(writable); (*writable)[20]=std::byte{42};
        RxEnvelope envelope; auto index=envelope.add_buffer(std::move(*lease)); assert(index);
        assert(envelope.set_prologue({*index,0,20})); assert(envelope.append_payload({*index,20,40}));
        assert(envelope.append_payload({*index,60,40}));
        envelope.with_payload([&](BorrowedBytes const& view){ auto retained=view.retain(local,global); assert(retained); held=std::move(*retained); });
        assert(local.active()==1); auto second=envelope.retain(local,global); assert(second);
        assert(!envelope.retain(local,global));
    }
    assert(storage->returned==0); assert(held.fragment(0)->front()==std::byte{42});
    held.reset(); assert(storage->returned==1); assert(local.active()==0 && global.active()==0);
    {
        std::array<BufferRequest,3> req{{{128,64},{128,64},{129,64}}};
        assert(!TxStorage::acquire(*pool,req)); assert(pool->return_count()==3);
        req[2]={128,64}; auto tx=TxStorage::acquire(*pool,req); assert(tx && tx->byte_size()==384);
        auto extra=pool->acquire({128,64}); assert(extra && extra->set_size(128));
        assert(!tx->append(std::move(*extra),0,128)); assert(bool(*extra));
    }
    {
        auto lease=pool->acquire({128,64}); assert(lease && lease->set_size(128));
        RxEnvelope rx; auto i=rx.add_buffer(std::move(*lease)); assert(i);
        for(int n=0;n<16;++n) assert(rx.append_payload({*i,std::size_t(n),1}));
        assert(!rx.append_payload({*i,16,1}));
        RetentionQuota zero(0); assert(!rx.retain(local,zero)); assert(local.active()==0);
        auto handle=rx.retain(local,global); assert(handle); held=std::move(*handle);
    }
    auto weak=std::weak_ptr<Storage>(storage); spec.lifetime.reset(); storage.reset(); pool=ExternalPool{};
    assert(!weak.expired()); held.reset(); assert(weak.expired());
    {
        auto owner=std::make_shared<int>(0); BufferSpec device{owner,nullptr,128,1,64,MemoryDomain::device,0x1000};
        auto dp=ExternalPool::create(std::span(&device,1)); assert(dp);
        assert(!dp->acquire({128,64,MemoryDomain::device,true}));
        auto lease=dp->acquire({128,64,MemoryDomain::device,false}); assert(lease);
        assert(!lease->bytes()); assert(!lease->writable_bytes()); assert(lease->device_address()==0x1000);
    }
    std::cout<<"P03 ownership checks passed; sizes lease="<<sizeof(BufferLease)<<" retained="<<sizeof(RetainedRx)
        <<" rx="<<sizeof(RxEnvelope)<<" tx="<<sizeof(TxStorage)<<" block="<<sizeof(detail::Block)<<'\n';
}
