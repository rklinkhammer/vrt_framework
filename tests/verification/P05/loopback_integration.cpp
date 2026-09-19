#include <vita/adapters/loopback/loopback.hpp>
#include <vita/codec/samples.hpp>
#include <array>
#include <algorithm>
#include <memory>
using namespace vita;using namespace vita::runtime;using namespace vita::memory;using namespace vita::adapters::loopback;
struct Backing{alignas(64)std::array<std::byte,512> bytes{};unsigned returns{};};
static void returned(void* p,std::size_t)noexcept{++static_cast<Backing*>(p)->returns;}
struct Sink{
    RetentionQuota local{4},global{4};RetainedRx retained;std::array<std::byte,16> wire{};unsigned calls{};bool good{true};
    static void receive(void* p,const codec::PacketView& parsed,const RxEnvelope& rx)noexcept{
        auto& s=*static_cast<Sink*>(p);++s.calls;
        if(parsed.envelope.wire.size()!=16){s.good=false;return;}
        std::copy(parsed.envelope.wire.begin(),parsed.envelope.wire.end(),s.wire.begin());
        auto values=codec::SampleView<std::int16_t>::create(parsed.envelope.payload);
        if(!values || values->size()!=2 || *values->at(0)!=codec::Iq<std::int16_t>{16384,0})s.good=false;
        auto retained=rx.retain(s.local,s.global);if(!retained)s.good=false;else s.retained=std::move(*retained);
    }
};
static int run(bool segmented,std::array<std::byte,16>& result){
    auto tx=std::make_shared<Backing>(),rx=std::make_shared<Backing>(),control_rx=std::make_shared<Backing>(),cancel_rx=std::make_shared<Backing>();
    BufferSpec txspec{tx,tx->bytes.data(),64,8,64,MemoryDomain::cpu,0,returned,tx.get()};
    BufferSpec rxspec{rx,rx->bytes.data(),64,8,64,MemoryDomain::cpu,0,returned,rx.get()};
    BufferSpec controlspec{control_rx,control_rx->bytes.data(),64,8,64};
    BufferSpec cancelspec{cancel_rx,cancel_rx->bytes.data(),64,8,64};
    auto txpool=ExternalPool::create(std::span{&txspec,1}),rxpool=ExternalPool::create(std::span{&rxspec,1}),controlpool=ExternalPool::create(std::span{&controlspec,1}),cancelpool=ExternalPool::create(std::span{&cancelspec,1});if(!txpool||!rxpool||!controlpool||!cancelpool)return 1;
    Sink sink;RouteRegistry<4> routes;CounterRegistry<4> counters;PeerSession peer{5,1};CounterKey counter{9,1,codec::PacketType::signal};
    if(!routes.add(Route{RouteKey{peer,1,codec::PacketType::signal},&sink,Sink::receive})||!counters.add(counter))return 2;
    routes.freeze();counters.freeze();AdmissionRequest limits;limits.need(Resource::completion,4).need(Resource::ordinary_queue,4).need(Resource::data_queue,4).need(Resource::cancellation_queue,1);
    AdmissionPool admission(limits);CompletionArena<4> completions;
    {
        Loopback<4,4,4> transport(*rxpool,*controlpool,*cancelpool,admission,routes,counters);
        auto full=txpool->acquire({64,64});if(!full)return 3;auto writable=full->writable_bytes();if(!writable)return 4;
        const std::array<codec::Iq<std::int16_t>,2> samples{{{16384,0},{15137,6270}}};
        auto samples_packed=codec::pack_iq16(samples,writable->subspan(8));if(!samples_packed)return 5;
        codec::Envelope envelope;envelope.type=codec::PacketType::signal;envelope.stream_id=1;
        auto encoded=codec::encode_envelope(envelope,writable->subspan(8,8),std::nullopt,*writable);if(!encoded||*encoded!=16||!full->set_size(16))return 6;
        TxStorage storage;
        if(segmented){
            auto payload=txpool->acquire({64,64});if(!payload)return 7;
            auto dest=payload->writable_bytes();std::copy_n(writable->begin()+8,8,dest->begin());payload->set_size(8);full->set_size(8);
            if(!storage.append(std::move(*full),0,8)||!storage.append(std::move(*payload),0,8))return 8;
        }else if(!storage.append(std::move(*full),0,16))return 9;
        auto completion=completions.reserve(100);if(!completion)return 10;
        auto accepted=transport.try_send({std::move(storage),std::move(*completion),peer,counter});
        if(!accepted||sink.calls||completions.consume(accepted->slot)||*counters.next(counter)!=1||tx->returns)return 11;
        if(!transport.progress(*accepted)||sink.calls!=1||!sink.good||transport.outstanding()!=0)return 12;
        auto outcome=completions.consume(0);if(!outcome||outcome->operation!=100||outcome->result.status!=CompletionStatus::succeeded)return 13;
        if(tx->returns!=(segmented?2U:1U)||rx->returns!=0||sink.local.active()!=1||transport.progress(*accepted))return 14;
        if(!sink.retained.fragment(0)||sink.retained.fragment(0)->size()!=8)return 15;
    }
    result=sink.wire;rxpool=ExternalPool{};rxspec.lifetime.reset();
    if(!sink.retained.fragment(0)||rx->returns!=0)return 16;
    sink.retained.reset();if(rx->returns!=1||sink.local.active()!=0)return 17;
    return 0;
}
int main(){std::array<std::byte,16> contiguous{},segmented{};if(auto r=run(false,contiguous))return r;if(auto r=run(true,segmented))return r+20;return contiguous==segmented?0:50;}
