#include <vita/runtime/completion/ticket.hpp>
#include <array>
#include <atomic>
#include <optional>
#include <thread>
#include <type_traits>
using namespace vita;
using namespace vita::runtime;
static_assert(!std::is_copy_constructible_v<CompletionToken>);
static_assert(!std::is_copy_constructible_v<CompletionWriter>);
int main() {
    CompletionToken empty;if(empty.active() || empty.is_reserved())return 18;
    CompletionArena<1> arena(7);
    auto token=arena.reserve(42);if(!token || !token->active() || !token->is_reserved())return 1;
    auto old=token->publisher();auto writer=old.try_claim();
    if(!writer || token->is_reserved() || arena.state(0)!=TicketState::writing || arena.consume(0) || old.try_claim())return 2;
    CompletionResult result;result.value=0xdeadbeef;result.effective_monotonic_ns=0x12345678;
    if(!writer->finish(result) || writer->finish(result))return 3;
    auto record=arena.consume(0);
    if(!record || record->operation!=42 || record->result.value!=result.value || record->result.effective_monotonic_ns!=result.effective_monotonic_ns || arena.consume(0))return 4;
    auto next=arena.reserve(43);if(!next || next->publisher().generation()!=8)return 5;
    if(old.publish(result) || !next->publish(result) || !arena.consume(0))return 6;
    {
        auto abandoned=arena.reserve(44);if(!abandoned)return 7;
    }
    auto failure=arena.consume(0);
    if(!failure || failure->operation!=44 || failure->result.status!=CompletionStatus::abandoned)return 8;
    CompletionArena<1> terminal(max_ticket_generation);
    auto last=terminal.reserve(1);if(!last || !last->publish(result) || !terminal.consume(0) || terminal.reserve(2) || terminal.state(0)!=TicketState::retired)return 9;
    {
        CompletionArena<1> stalled;
        auto pending=stalled.reserve(1);if(!pending)return 10;
        { auto incomplete=pending->publisher().try_claim();if(!incomplete)return 11; }
        if(stalled.consume(0) || stalled.reserve(2) || stalled.state(0)!=TicketState::writing)return 12;
    }
    {
        std::optional<CompletionToken> late;
        { CompletionArena<1> transient;auto work=transient.reserve(99);if(!work)return 13;late.emplace(std::move(*work)); }
        if(!late->publish(result))return 14;
    }
    CompletionArena<1> racing;
    auto reserved=racing.reserve(123);if(!reserved)return 15;
    auto publisher=reserved->publisher();std::atomic<unsigned> producers{0},consumers{0},bad{0};
    std::array<std::thread,8> threads;
    for(unsigned i=0;i<threads.size();++i)threads[i]=std::thread([&,i]{
        CompletionResult r;r.value=i;r.effective_monotonic_ns=i^0xfedcba98;
        if(publisher.publish(r))producers.fetch_add(1);
    });
    for(auto& t:threads)t.join();
    for(auto& t:threads)t=std::thread([&]{if(auto r=racing.consume(0)){
        consumers.fetch_add(1);if(r->operation!=123 || r->result.effective_monotonic_ns!=(r->result.value^0xfedcba98))bad.fetch_add(1);
    }});
    for(auto& t:threads)t.join();
    if(producers!=1 || consumers!=1 || bad!=0 || racing.rejected_publications()!=7)return 16;
    CompletionArena<1> sustained;
    std::atomic<unsigned> consumed{0};
    std::thread reader([&]{while(consumed<1000){if(auto r=sustained.consume(0)){
        if(r->operation!=r->result.value || r->result.effective_monotonic_ns!=(r->result.value^0x5555))bad.fetch_add(1);
        consumed.fetch_add(1);
    }else std::this_thread::yield();}});
    for(unsigned i=0;i<1000;++i){
        auto work=sustained.reserve(i);while(!work){std::this_thread::yield();work=sustained.reserve(i);}
        CompletionResult r;r.value=i;r.effective_monotonic_ns=i^0x5555;
        if(!work->publish(r))std::abort();
    }
    reader.join();if(bad!=0 || consumed!=1000)return 17;
    return 0;
}
