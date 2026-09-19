#include <vita/runtime/transaction/retention.hpp>
#include "packets.hpp"
using namespace vita;using namespace vita::runtime;using namespace vita::runtime::transaction;using namespace verify_p07;
int main(){
    auto packet=make(false);auto parsed=packet.view();auto derived=transaction_key(parsed,7,{9,11});
    TransactionKey expected{7,{9,11},1,codec::Identifier::short_id(3),codec::Identifier::short_id(2),42};
    if(!derived||!same_key(*derived,expected))return 1;
    for(unsigned part=0;part<9;++part){auto changed=expected;switch(part){case 0:++changed.binding_generation;break;case 1:++changed.peer.peer;break;case 2:++changed.peer.generation;break;case 3:changed.stream_id.reset();break;case 4:changed.stream_id=0;break;case 5:changed.controller={};break;case 6:changed.controllee={};break;case 7:changed.message_id=0;break;case 8:changed.controller=codec::Identifier::uuid({3,0,0,0});break;}if(same_key(changed,expected))return 2;}
    auto uuid=expected;uuid.controller=codec::Identifier::uuid({1,2,3,4});for(unsigned word=0;word<4;++word){auto changed=uuid;++changed.controller.words[word];if(same_key(changed,uuid))return 3;}
    if(transaction_key(parsed,0,{9,11})||transaction_key(parsed,7,{9,0}))return 4;
    AdmissionPool pool(AdmissionPool::reference_capacities());RetentionStore<4,4096> store(pool);
    auto first=store.reserve(expected,parsed,0);if(!first)return 5;
    auto generation=expected;++generation.binding_generation;auto second=store.reserve(generation,parsed,0);if(!second||second->kind!=DuplicateKind::fresh||second->token==first->token)return 6;
    auto peer=expected;++peer.peer.generation;auto third=store.reserve(peer,parsed,0);if(!third||third->kind!=DuplicateKind::fresh||third->token==first->token)return 7;
    auto wrong=expected;wrong.controller={};if(store.reserve(wrong,parsed,0))return 8;
    return 0;
}
