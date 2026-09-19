#include <vita/runtime/transaction/backend.hpp>
#include <thread>
#include <atomic>
using namespace vita;using namespace vita::runtime;using namespace vita::runtime::transaction;
int main(){
    CompletionArena<1> arena;auto storage=std::make_shared<ResultStorage<1>>();
    auto ticket=arena.reserve(1);if(!ticket)return 1;
    auto& output=storage->slots[0].outcome;output.sample_ordinal=1234;
    AsyncResult stale{ticket->publisher(),storage,&output,0};
    if(!stale.synthetic_failure())return 2;
    FieldOutcome poison;poison.sample_ordinal=9999;
    if(stale.complete(poison)||output.sample_ordinal!=1234)return 3;
    auto failure=arena.consume(0);if(!failure||failure->result.status!=CompletionStatus::failed)return 4;
    auto next=arena.reserve(2);if(!next)return 5;
    if(stale.complete(poison)||output.sample_ordinal!=1234)return 6;
    AsyncResult current{next->publisher(),storage,&output,0};
    std::atomic<unsigned> wins=0;
    std::array<std::thread,8> writers;
    for(unsigned n=0;n<writers.size();++n)writers[n]=std::thread([&,n]{FieldOutcome o;o.sample_ordinal=n;o.uncertainty_ps=n^0xabcdef;if(current.complete(o))++wins;});
    for(auto& thread:writers)thread.join();
    auto success=arena.consume(0);
    if(!success||wins!=1||output.uncertainty_ps!=(output.sample_ordinal^0xabcdef))return 7;
    AsyncResult late;
    std::weak_ptr<ResultStorage<1>> weak;
    {
        CompletionArena<1> transient;auto owned=std::make_shared<ResultStorage<1>>();weak=owned;
        auto pending=transient.reserve(3);if(!pending)return 8;
        late=AsyncResult{pending->publisher(),owned,&owned->slots[0].outcome,0};
        // Token destruction publishes abandonment. Late capability keeps both arenas alive but loses claim.
    }
    if(weak.expired()||late.complete(poison))return 9;
    late={};if(!weak.expired())return 10;
    return 0;
}
