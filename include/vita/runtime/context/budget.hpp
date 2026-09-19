#pragma once
#include <vita/runtime/context/receiver.hpp>
#include <vita/runtime/execution/budget.hpp>
namespace vita::runtime::context {
// Count actual contained objects once; excludes allocator/shared_ptr infrastructure.
template<std::size_t Revisions=128,std::size_t History=128,std::size_t Held=64,std::size_t Waiting=64>
Result<void> charge_context(BudgetLedger& ledger,std::size_t streams=1) noexcept {
    constexpr std::array rows{
        std::pair{BudgetCategory::revisions,RevisionStore<Revisions>::storage_bytes()+sizeof(RevisionStore<Revisions>)},
        std::pair{BudgetCategory::context_history,ReceiverHistory<History>::storage_bytes()},
        std::pair{BudgetCategory::queues,ContextPublisher<Revisions,Held>::storage_bytes()+ContextReceiver<History,Waiting>::storage_bytes()-ReceiverHistory<History>::storage_bytes()+sizeof(memory::detail::QuotaState)}
    };
    auto candidate=ledger;
    for(auto [category,bytes]:rows){if(streams>SIZE_MAX/bytes)return std::unexpected(Error{ErrorCode::overflow});auto charged=candidate.charge(category,bytes*streams);if(!charged)return charged;}
    ledger=candidate;return {};
}
} // namespace vita::runtime::context
