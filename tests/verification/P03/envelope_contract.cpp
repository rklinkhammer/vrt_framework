#include <vita/memory/memory.hpp>
#include <array>
#include <limits>
#include <memory>
#include <type_traits>
using namespace vita;
using namespace vita::memory;
struct Backing { alignas(64) std::array<std::byte,256> bytes{}; std::array<unsigned,4> returns{}; };
static void returned(void* p,std::size_t index) noexcept { ++static_cast<Backing*>(p)->returns[index]; }
static_assert(!std::is_copy_constructible_v<RetainedRx>);
static_assert(!std::is_copy_constructible_v<RxEnvelope>);
static_assert(!std::is_copy_constructible_v<BorrowedBytes>);
int main() {
    auto backing=std::make_shared<Backing>();
    for(std::size_t i=0;i<256;++i) backing->bytes[i]=std::byte(i);
    const auto original=backing->bytes;
    BufferSpec spec{backing,backing->bytes.data(),64,4,64,MemoryDomain::cpu,0,returned,backing.get()};
    auto pool=ExternalPool::create(std::span{&spec,1});
    if(!pool)return 1;
    RxEnvelope rx;
    for(unsigned i=0;i<3;++i) {
        auto lease=pool->acquire({64,64}); if(!lease || !lease->set_size(64))return 2;
        auto index=rx.add_buffer(std::move(*lease)); if(!index || *index!=i)return 3;
    }
    if(!rx.set_prologue({0,0,16}) || !rx.set_trailer({2,60,4}) ||
       !rx.append_payload({1,4,12}) || !rx.append_payload({1,20,16}))return 4;
    if(rx.append_payload({1,63,2}) || rx.append_payload({1,std::numeric_limits<std::size_t>::max(),2}))return 5;
    RetentionQuota local(2), global(2), unavailable(0);
    if(rx.retain(local,unavailable) || local.active()!=0)return 6;
    auto first=rx.with_payload([&](const BorrowedBytes& bytes) { return bytes.retain(local,global); });
    auto second=rx.retain(local,global);
    if(!first || !second || local.active()!=2 || global.active()!=2 || rx.retain(local,global))return 7;
    rx=RxEnvelope{};
    if(backing->returns[0]!=1 || backing->returns[2]!=1 || backing->returns[1]!=0)return 8;
    if(first->fragment_count()!=2 || (*first->fragment(0))[0]!=std::byte{68} || (*second->fragment(1))[0]!=std::byte{84})return 9;
    first->reset();
    if(backing->returns[1]!=0 || local.active()!=1)return 10;
    second->reset();
    if(backing->returns[1]!=1 || local.active()!=0 || global.active()!=0 || backing->bytes!=original)return 11;
    {
        const std::array requests{BufferRequest{64,64},BufferRequest{65,64}};
        auto partial=TxStorage::acquire(*pool,requests);
        if(partial || pool->return_count()!=4)return 12;
    }
    {
        TxStorage tx;
        for(unsigned i=0;i<3;++i) {
            auto lease=pool->acquire({64,64});if(!lease || !lease->set_size(32) || !tx.append(std::move(*lease),0,32))return 13;
        }
        auto fourth=pool->acquire({64,64});if(!fourth || !fourth->set_size(32))return 14;
        if(tx.append(std::move(*fourth),0,32) || !*fourth || tx.byte_size()!=96 || tx.segment_count()!=3)return 15;
        auto accepted=std::move(tx);
        if(tx.segment_count()!=0 || accepted.segment_count()!=3)return 16;
    }
    {
        RxEnvelope fragmented;
        auto lease=pool->acquire({64,64});if(!lease || !lease->set_size(64))return 17;
        auto index=fragmented.add_buffer(std::move(*lease));if(!index)return 18;
        for(unsigned i=0;i<16;++i)if(!fragmented.append_payload({*index,i*4,4}))return 19;
        if(fragmented.append_payload({*index,0,4}) || fragmented.fragment_count()!=16)return 20;
        RetentionQuota same(1);
        auto retained=fragmented.retain(same,same);
        if(!retained || same.active()!=1 || retained->fragment_count()!=16)return 21;
        retained->reset();if(same.active()!=0)return 22;
    }
    if(backing->bytes!=original)return 23;
    return 0;
}
