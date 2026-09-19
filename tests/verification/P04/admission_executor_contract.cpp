#include <vita/runtime/execution/admission.hpp>
#include <vita/runtime/execution/executor.hpp>
#include <vita/runtime/completion/ticket.hpp>
#include <array>
#include <atomic>
#include <thread>
using namespace vita;
using namespace vita::runtime;
static void increment(void* p) noexcept { ++*static_cast<unsigned*>(p); }
struct Nested { BoundedExecutor<4>* outer;BoundedExecutor<4>* inner;bool direct{},ancestor{},other{},no_reentry{}; };
static void inner_task(void* p) noexcept {
    auto& n=*static_cast<Nested*>(p);
    auto r=check_blocking_wait(n.outer->domain());n.ancestor=!r && r.error().code==ErrorCode::would_deadlock;
    n.no_reentry=!n.outer->run_one();
}
static void outer_task(void* p) noexcept {
    auto& n=*static_cast<Nested*>(p);
    auto r=check_blocking_wait(n.outer->domain());n.direct=!r && r.error().code==ErrorCode::would_deadlock;
    n.other=bool(check_blocking_wait(n.inner->domain()));
    n.inner->post({inner_task,p});n.inner->run();
}
int main() {
    AdmissionRequest limits;limits.need(Resource::transaction,1).need(Resource::data_queue,1).need(Resource::ordinary_queue,1)
      .need(Resource::cancellation_queue,1).need(Resource::completion,1).need(Resource::duplicate_bytes,4);
    AdmissionPool pool(limits);
    AdmissionRequest data_request;data_request.need(Resource::data_queue);
    auto data_full=pool.acquire(data_request);if(!data_full || pool.acquire(data_request) || pool.used(Resource::ordinary_queue)!=0 || pool.used(Resource::cancellation_queue)!=0)return 16;
    AdmissionRequest ordinary;ordinary.need(Resource::transaction).need(Resource::ordinary_queue).need(Resource::duplicate_bytes,4);
    auto first=pool.acquire(ordinary);if(!first || pool.acquire(ordinary))return 1;
    AdmissionRequest fail;fail.need(Resource::completion).need(Resource::duplicate_bytes,1);
    if(pool.acquire(fail) || pool.used(Resource::completion)!=0)return 2;
    AdmissionRequest cancel;cancel.need(Resource::cancellation_queue);
    auto cancellation=pool.acquire(cancel);if(!cancellation || pool.used(Resource::ordinary_queue)!=1)return 3;
    AdmissionRequest retention;retention.need(Resource::duplicate_bytes,4);
    auto saved=first->transfer(retention);if(!saved || first->held(Resource::duplicate_bytes)!=0)return 4;
    first->reset();if(pool.used(Resource::transaction)!=0 || pool.used(Resource::duplicate_bytes)!=4)return 5;
    saved->reset();if(pool.used(Resource::duplicate_bytes)!=0)return 6;
    pool.close();if(pool.acquire(AdmissionRequest{}))return 7;
    BoundedExecutor<4> executor;unsigned count=0;
    for(unsigned i=0;i<4;++i)if(!executor.post({increment,&count}))return 8;
    if(executor.post({increment,&count}) || executor.pending()!=4 || executor.post({nullptr,&count}))return 9;
    CompletionArena<1> arena;auto ticket=arena.reserve(1);if(!ticket || !ticket->publish({}))return 10;
    unsigned completions=0;
    if(arena.scan([&](CompletionRecord) noexcept {++completions;})!=1 || completions!=1 || executor.pending()!=4)return 11;
    if(executor.run(2)!=2 || count!=2 || executor.pending()!=2)return 12;
    executor.close();if(executor.post({increment,&count}) || executor.run()!=2 || count!=4)return 13;
    BoundedExecutor<4> outer,inner;Nested nested{&outer,&inner};
    if(!outer.post({outer_task,&nested}) || !outer.run_one() || !nested.direct || !nested.other || !nested.ancestor || !nested.no_reentry)return 14;
    if(!check_blocking_wait(outer.domain()) || !check_blocking_wait(nullptr))return 15;
    return 0;
}
