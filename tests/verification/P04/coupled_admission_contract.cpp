#include <vita/runtime/execution/operation.hpp>
using namespace vita;
using namespace vita::runtime;
int main(){
    AdmissionRequest limits;limits.need(Resource::transaction,1).need(Resource::plan_bytes,64).need(Resource::completion,1);
    AdmissionRequest request;request.need(Resource::transaction,1).need(Resource::plan_bytes,32).need(Resource::completion,1);
    AdmissionPool pool(limits);SlotArena<1,64> arena;CompletionArena<1> tickets;
    {
        auto occupied=arena.acquire(64);if(!occupied)return 1;
        auto denied=AdmittedOperation<1,64>::acquire(pool,arena,tickets,request,32,10);
        if(denied || pool.used(Resource::plan_bytes)!=0 || pool.used(Resource::transaction)!=0 || pool.used(Resource::completion)!=0 || tickets.state(0)!=TicketState::free)return 2;
    }
    {
        auto occupied=tickets.reserve(11);if(!occupied)return 3;
        auto denied=AdmittedOperation<1,64>::acquire(pool,arena,tickets,request,32,12);
        if(denied || pool.used(Resource::plan_bytes)!=0 || pool.used(Resource::transaction)!=0 || pool.used(Resource::completion)!=0)return 4;
        auto restored=arena.acquire(64);if(!restored)return 5;
        occupied->publish({});if(!tickets.consume(0))return 6;
    }
    {
        auto operation=AdmittedOperation<1,64>::acquire(pool,arena,tickets,request,32,13);
        if(!operation || operation->storage.bytes().size()!=64 || pool.used(Resource::plan_bytes)!=32)return 7;
        if(AdmittedOperation<1,64>::acquire(pool,arena,tickets,request,32,14))return 8;
        operation->storage.bytes()[0]=std::byte{0xa5};
        operation->completion.publish({});auto record=tickets.consume(0);
        if(!record || record->operation!=13)return 9;
    }
    if(pool.used(Resource::plan_bytes)!=0 || pool.used(Resource::transaction)!=0 || pool.used(Resource::completion)!=0)return 10;
    limits.need(Resource::plan_bytes,65);AdmissionPool oversized(limits);
    if(AdmittedOperation<1,64>::acquire(oversized,arena,tickets,request,32,15))return 11;
    request.need(Resource::plan_bytes,16);
    if(AdmittedOperation<1,64>::acquire(pool,arena,tickets,request,32,16))return 12;
    return 0;
}
