#include <vita/runtime/completion/ticket.hpp>
#include <vita/runtime/execution/admission.hpp>
#include <vita/runtime/execution/executor.hpp>
#include <vita/runtime/execution/arena.hpp>
#include <vita/runtime/execution/budget.hpp>
#include <vita/runtime/execution/operation.hpp>
#include <cassert>
#include <iostream>
#include <thread>
using namespace vita;
using namespace vita::runtime;
int main() {
    CompletionArena<1> arena(7); auto token=arena.reserve(42); assert(token);
    CompletionToken empty; assert(!empty.active() && !empty.is_reserved());
    assert(token->active() && token->is_reserved());
    auto publisher=token->publisher(); auto writer=publisher.try_claim(); assert(writer);
    assert(!arena.consume(0)); assert(arena.state(0)==TicketState::writing); assert(token->active() && !token->is_reserved());
    CompletionResult result; result.value=123; assert(writer->finish(result));
    auto recorded=arena.consume(0); assert(recorded && recorded->operation==42 && recorded->result.value==123);
    auto next=arena.reserve(43); assert(next); assert(!publisher.publish(result));
    assert(next->publish(result)); assert(arena.consume(0));
    CompletionArena<1> wrapping(max_ticket_generation); { auto last=wrapping.reserve(0); assert(last && last->publish(result)); }
    assert(wrapping.consume(0)); assert(wrapping.state(0)==TicketState::retired); assert(!wrapping.reserve(0));
    CompletionArena<1> abandoned; { auto t=abandoned.reserve(0); assert(t); } assert(abandoned.consume(0)->result.status==CompletionStatus::abandoned);
    CompletionArena<1> race; auto r=race.reserve(1); assert(r); auto p=r->publisher();
    std::atomic<unsigned> winners{0}; std::thread a([&]{if(p.publish(result)) ++winners;}); std::thread b([&]{if(p.publish(result)) ++winners;}); a.join(); b.join(); assert(winners==1);
    std::atomic<unsigned> consumers{0}; std::thread c([&]{if(race.consume(0)) ++consumers;}); std::thread d([&]{if(race.consume(0)) ++consumers;}); c.join(); d.join(); assert(consumers==1);
    { auto capacities=AdmissionPool::reference_capacities(); AdmissionPool isolated(capacities);
      AdmissionRequest data; data.need(Resource::data_queue,4096); auto full_data=isolated.acquire(data); assert(full_data);
      assert(isolated.used(Resource::ordinary_queue)==0 && isolated.used(Resource::cancellation_queue)==0);
      AdmissionRequest control; control.need(Resource::ordinary_queue,256).need(Resource::cancellation_queue,64);
      assert(isolated.acquire(control)); assert(!isolated.acquire(data)); }
    AdmissionRequest caps; caps.need(Resource::transaction,1).need(Resource::plan_bytes,16).need(Resource::cancellation_queue,1);
    AdmissionPool admission(caps); AdmissionRequest req; req.need(Resource::transaction).need(Resource::plan_bytes,16);
    auto admitted=admission.acquire(req); assert(admitted); assert(!admission.acquire(req));
    AdmissionRequest cancel; cancel.need(Resource::cancellation_queue); auto cancellation=admission.acquire(cancel); assert(cancellation);
    AdmissionRequest transfer; transfer.need(Resource::plan_bytes,8); auto held=admitted->transfer(transfer); assert(held); admitted->reset(); assert(admission.used(Resource::plan_bytes)==8); held->reset();
    SlotArena<1,16> values; auto value=values.acquire(16); assert(value); assert(!values.acquire(1)); assert(!values.acquire(17)); value->reset(); assert(values.acquire(16));
    { auto occupied=values.acquire(16); assert(occupied); auto denied=AdmittedStorage<1,16>::acquire(admission,values,req,16);
      assert(!denied); assert(admission.used(Resource::transaction)==0 && admission.used(Resource::plan_bytes)==0); }
    { auto coupled=AdmittedStorage<1,16>::acquire(admission,values,req,16); assert(coupled && coupled->storage.bytes().size()==16); }
    { AdmissionRequest oc; oc.need(Resource::transaction,1).need(Resource::plan_bytes,16).need(Resource::completion,1);
      AdmissionPool opool(oc); SlotArena<1,16> ovalues; CompletionArena<1> otickets; auto busy=otickets.reserve(0); assert(busy);
      auto failed=AdmittedOperation<1,16>::acquire(opool,ovalues,otickets,oc,16,99); assert(!failed);
      assert(opool.used(Resource::plan_bytes)==0 && opool.used(Resource::completion)==0); assert(ovalues.acquire(16));
      assert(busy->publish({})); assert(otickets.consume(0));
      auto operation=AdmittedOperation<1,16>::acquire(opool,ovalues,otickets,oc,16,99); assert(operation);
      assert(operation->completion.publish({})); assert(otickets.consume(0)->operation==99); }
    ControlStrand<2> strand;
    struct Context { ControlStrand<2>* strand; int calls{}; } context{&strand};
    Task task{[](void* ptr) noexcept { auto& ctx=*static_cast<Context*>(ptr); ++ctx.calls; assert(!check_blocking_wait(ctx.strand->domain())); assert(!ctx.strand->run_one()); },&context};
    assert(strand.post(task)); assert(strand.post(task)); assert(!strand.post(task)); assert(context.calls==0); assert(strand.run()==2); assert(context.calls==2); assert(check_blocking_wait(strand.domain()));
    auto projected=reference_budget(); assert(projected && projected->reserved_bytes()==framework_budget);
    BudgetLedger budget; assert(budget.reserved_bytes()==framework_budget); assert(budget.charge_objects<memory::RetainedRx>(BudgetCategory::retention,1024));
    assert(budget.charge(BudgetCategory::completion,CompletionArena<1024>::metadata_bytes())); assert(!budget.charge(BudgetCategory::completion,framework_budget));
    assert(budget.transfer_headroom(BudgetCategory::plans,SlotArena<256,8192>::storage_bytes()-2097152));
    assert(budget.charge(BudgetCategory::plans,SlotArena<256,8192>::storage_bytes()));
    struct alignas(64) Backing { std::array<std::byte,128> bytes; };
    auto backing=std::make_shared<Backing>(); memory::BufferSpec spec{backing,backing->bytes.data(),128,1,64};
    auto pool=memory::ExternalPool::create(std::span(&spec,1)); assert(pool); memory::BufferRequest request{128,64};
    auto tx=memory::TxStorage::acquire(*pool,std::span(&request,1)); assert(tx);
    QuiescenceGuard guard; assert(guard.arm(std::move(*tx))); { auto t=abandoned.reserve(0); assert(t); }
    assert(abandoned.consume(0)->result.status==CompletionStatus::abandoned); assert(pool->return_count()==0 && guard.retained()); auto old_guard=guard; assert(guard.prove_quiescent()); assert(pool->return_count()==1);
    auto tx2=memory::TxStorage::acquire(*pool,std::span(&request,1)); assert(tx2); assert(guard.arm(std::move(*tx2)));
    assert(!old_guard.prove_quiescent()); assert(pool->return_count()==1); assert(guard.prove_quiescent()); assert(pool->return_count()==2);
    std::cout<<"P04 checks passed; slot="<<CompletionArena<1024>::slot_bytes()<<" completion_arena="<<CompletionArena<1024>::metadata_bytes()<<" plan_arena="<<SlotArena<256,8192>::storage_bytes()<<" charged="<<budget.charged_bytes()<<" available_reference_charge="<<projected->charged_bytes()<<'\n';
}
