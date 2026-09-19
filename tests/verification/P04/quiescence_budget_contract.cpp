#include <vita/runtime/completion/ticket.hpp>
#include <vita/runtime/execution/budget.hpp>
#include <array>
#include <limits>
#include <memory>
using namespace vita;
using namespace vita::memory;
using namespace vita::runtime;
struct Backing { std::array<std::byte,128> bytes{};unsigned returns{}; };
static void returned(void* p,std::size_t) noexcept {++static_cast<Backing*>(p)->returns;}
int main() {
    auto b=std::make_shared<Backing>();
    BufferSpec spec{b,b->bytes.data(),64,2,1,MemoryDomain::cpu,0,returned,b.get()};
    auto pool=ExternalPool::create(std::span{&spec,1});if(!pool)return 1;
    QuiescenceGuard guard;
    const std::array requests{BufferRequest{64,1}};
    auto storage=TxStorage::acquire(*pool,requests);if(!storage || !guard.arm(std::move(*storage)))return 2;
    auto old_callback=guard;
    CompletionArena<1> arena;
    {auto abandoned=arena.reserve(1);if(!abandoned)return 3;}
    auto result=arena.consume(0);
    if(!result || result->result.status!=CompletionStatus::abandoned || !guard.retained() || b->returns!=0)return 4;
    if(!guard.prove_quiescent() || b->returns!=1 || guard.retained())return 5;
    auto second=TxStorage::acquire(*pool,requests);if(!second || !guard.arm(std::move(*second)))return 6;
    if(old_callback.prove_quiescent() || b->returns!=1 || !guard.retained())return 7;
    auto current_callback=guard;
    if(!current_callback.prove_quiescent() || b->returns!=2 || guard.retained())return 8;
    BudgetLedger ledger;
    if(ledger.reserved_bytes()!=67108864 || ledger.charged_bytes()!=0)return 9;
    const auto retention=ledger.row(BudgetCategory::retention);
    if(retention.reserved!=262144+704512 || !ledger.charge_objects<RetainedRx>(BudgetCategory::retention,1024))return 10;
    if(ledger.row(BudgetCategory::retention).charged!=sizeof(RetainedRx)*1024)return 11;
    if(ledger.charge_objects<RetainedRx>(BudgetCategory::retention,std::numeric_limits<std::size_t>::max()))return 12;
    const auto before=ledger.charged_bytes();
    if(ledger.charge(BudgetCategory::retention,retention.reserved) || ledger.charged_bytes()!=before)return 13;
    auto available=ledger.row(BudgetCategory::headroom).reserved;
    if(!ledger.transfer_headroom(BudgetCategory::completion,available) || ledger.reserved_bytes()!=67108864 || ledger.transfer_headroom(BudgetCategory::completion,1))return 14;
    if(ledger.charge(BudgetCategory::count,1))return 15;
    auto actual=reference_budget();
    if(!actual || actual->reserved_bytes()!=67108864 || actual->charged_bytes()>=67108864)return 16;
    if(actual->row(BudgetCategory::completion).charged<CompletionArena<1024>::metadata_bytes() ||
       actual->row(BudgetCategory::plans).charged<SlotArena<256,8192>::storage_bytes() ||
       actual->row(BudgetCategory::retention).charged<sizeof(RetainedRx)*1024)return 17;
    return 0;
}
