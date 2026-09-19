#include <vita/runtime/transaction/retention.hpp>
#include "../P07/packets.hpp"
using namespace vita;using namespace vita::runtime;using namespace vita::runtime::transaction;using namespace verify_p07;
static TransactionKey key(unsigned mid){return {7,{9,1},1,codec::Identifier::short_id(3),codec::Identifier::short_id(2),mid};}
int main(){
 AdmissionPool pool(AdmissionPool::reference_capacities());
 {constexpr auto block=36+sizeof(AckRecord);RetentionStore<8,block*4+28> store(pool);std::array<RetentionToken,4> tokens;
 for(unsigned n=0;n<4;++n){auto packet=make(false,n+1,2);if(packet.size!=36)return 1;auto reserved=store.reserve(key(n+1),packet.view(),1);if(!reserved||store.last_placement_probes()>n||!store.bind(reserved->token))return 2;tokens[n]=reserved->token;AckRecord response;response.kind=AckKind::state;response.state.fields[1].value=*Hertz::from_integer(n+10);if(!store.append(tokens[n],response)||!store.complete(tokens[n],{n%2?1000000000ull:0ull})||!store.release(tokens[n]))return 3;}
 auto c=cancellation(2,2);if(c.size!=28)return 4;auto cancel=store.reserve(key(2),c.view(),0);if(!cancel||!store.bind(cancel->token)||!store.complete(cancel->token,{1000000000})||!store.release(cancel->token))return 5;
 auto extra=make(false,99,2);const auto charged=pool.used(Resource::duplicate_bytes);if(store.reserve(key(99),extra.view(),0)||pool.used(Resource::duplicate_bytes)!=charged)return 6;
 store.expire({30000000000});if(store.size()!=2)return 7;
 for(unsigned n=0;n<2;++n){auto packet=make(false,n+5,2);auto r=store.reserve(key(n+5),packet.view(),1);if(!r||store.last_placement_probes()>5||!store.bind(r->token))return 8;AckRecord response;response.kind=AckKind::state;response.state.fields[1].value=*Hertz::from_integer(n+50);if(!store.append(r->token,response))return 9;}
 for(unsigned n:{1u,3u}){auto value=store.response(tokens[n],0);if(!value||!*value||(**value).state.fields[1].value!=SemanticValue{*Hertz::from_integer(n+10)})return 10;}
 auto replay=store.reserve(key(2),make(false,2,2).view(),1);if(!replay||replay->kind!=DuplicateKind::replay||!store.release(replay->token))return 11;
 }
 if(pool.used(Resource::duplicate_bytes)||pool.used(Resource::duplicate_entry))return 12;
 {RetentionStore<4096,8*1024*1024> store(pool);std::uint64_t probes=0;for(unsigned n=0;n<3000;++n){auto packet=make(false,n+1,2);auto r=store.reserve(key(n+1),packet.view(),0);if(!r||store.last_placement_probes()>n||!store.bind(r->token)||!store.complete(r->token,{0})||!store.release(r->token))return 13;probes+=store.last_placement_probes();}if(probes!=3000ull*2999/2||pool.used(Resource::duplicate_bytes)!=3000*36)return 14;store.expire({29999999999});if(store.size()!=3000)return 15;store.expire({30000000000});if(store.size()||pool.used(Resource::duplicate_bytes))return 16;auto r=store.reserve(key(4000),make(false,4000,2).view(),0);if(!r||store.last_placement_probes()!=0)return 17;}
 return 0;
}
