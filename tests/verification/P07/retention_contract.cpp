#include <vita/runtime/transaction/retention.hpp>
#include "packets.hpp"
using namespace vita;using namespace vita::runtime;using namespace vita::runtime::transaction;using namespace verify_p07;
static TransactionKey key(std::uint32_t mid=42){return {7,{9,1},1,codec::Identifier::short_id(3),codec::Identifier::short_id(2),mid};}
int main(){
    AdmissionPool pool(AdmissionPool::reference_capacities());RetentionStore<2,4096> store(pool);auto original=make(false);auto k=key();
    auto first=store.reserve(k,original.view(),3);if(!first||first->kind!=DuplicateKind::fresh||!store.bind(first->token,Handle{0,1}))return 1;
    auto counted=make(false,42,3,2,0xa91f0000,13);auto repeat=store.reserve(k,counted.view(),3);
    if(!repeat||repeat->kind!=DuplicateKind::active||repeat->token!=first->token)return 2;
    if(!store.release(repeat->token))return 3;
    auto changed=make(false,42,3,3);if(store.reserve(k,changed.view(),3))return 4;
    AckRecord state;state.kind=AckKind::state;state.selected_mask=2;state.state.fields[1].value=*Hertz::from_integer(2);state.state.fields[1].validity=Validity::known;state.time={10,123};state.time_known=true;state.epoch=codec::Tsi::gps;
    if(!store.append(first->token,state)||!store.complete(first->token,{1'000'000}))return 5;
    state.state.fields[1].value=*Hertz::from_integer(99);state.time={99,0};
    auto old=store.response(first->token,0);if(!old||!*old||std::get<Hertz>((**old).state.fields[1].value).q20!=2*(1ll<<20)||(**old).time!=timing::ProtocolTime{10,123})return 6;
    auto cancel=cancellation(42,2);auto cancellation_record=store.reserve(k,cancel.view(),2);if(!cancellation_record||!cancellation_record->token.cancellation||cancellation_record->kind!=DuplicateKind::fresh||store.size()!=1)return 7;
    if(!store.bind(cancellation_record->token))return 8;
    auto other_subset=cancellation(42,1);if(store.reserve(k,other_subset.view(),2))return 9;
    auto changed_cam=cancellation(42,2,0xad0f0000);if(store.reserve(k,changed_cam.view(),2))return 10;
    auto cancel_view=cancel.view();auto timestamped_envelope=cancel_view.envelope.envelope;timestamped_envelope.timestamp={codec::Tsi::gps,codec::Tsf::picoseconds,99,0};Packet timestamped;auto stamp_size=codec::encode_envelope(timestamped_envelope,cancel_view.envelope.payload,std::nullopt,timestamped.bytes);if(!stamp_size)return 32;timestamped.size=*stamp_size;if(store.reserve(k,timestamped.view(),2))return 33;
    auto retry_cancel=cancellation(42,2,0xa90f0000,7);auto attached=store.reserve(k,retry_cancel.view(),2);if(!attached||attached->kind!=DuplicateKind::active)return 11;if(!store.release(attached->token))return 12;
    AckRecord cx;cx.cancellation=true;cx.kind=AckKind::execution;cx.scheduled_or_executed=true;if(!store.append(cancellation_record->token,cx)||!store.complete(cancellation_record->token,{20'000'000'000}))return 13;
    if(!store.release(first->token)||!store.release(cancellation_record->token))return 14;
    store.expire({40'000'000'000});if(store.size()!=1||!store.retain(first->token))return 15;
    // Outstanding original reference holds both records past the latest terminal deadline.
    store.expire({60'000'000'000});if(store.size()!=1)return 16;
    auto replay=store.reserve(k,original.view(),3);if(!replay||replay->kind!=DuplicateKind::replay)return 17;
    old=store.response(replay->token,0);if(!old||!*old||(**old).time!=timing::ProtocolTime{10,123})return 18;
    if(!store.release(replay->token)||!store.release(first->token))return 19;store.expire({60'000'000'000});if(store.size()||pool.used(Resource::duplicate_entry)||pool.used(Resource::duplicate_bytes))return 20;
    if(store.response(first->token,0))return 21;
    {
        RetentionStore<4,52> bytes(pool);auto a=bytes.reserve(k,original.view(),0);if(!a||!bytes.bind(a->token))return 22;
        auto second=make(false,43);if(bytes.reserve(key(43),second.view(),0)||bytes.size()!=1)return 23;
        bytes.expire({UINT64_MAX});if(bytes.size()!=1)return 24;
    }
    {
        RetentionStore<1,4096> entries(pool);auto a=entries.reserve(k,original.view(),0);if(!a||!entries.bind(a->token))return 25;auto second=make(false,43);if(entries.reserve(key(43),second.view(),0))return 26;
        if(!entries.complete(a->token,{100})||!entries.release(a->token))return 27;
        entries.expire({30'000'000'099});if(entries.size()!=1)return 28;entries.expire({30'000'000'100});if(entries.size()!=0)return 29;
    }
    {
        RetentionStore<1,4096> active(pool);auto a=active.reserve(k,original.view(),0);if(!a||!active.bind(a->token)||!active.release(a->token))return 30;active.expire({UINT64_MAX});if(active.size()!=1)return 31;
    }
    return 0;
}
