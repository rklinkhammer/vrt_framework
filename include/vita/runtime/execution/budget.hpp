#pragma once
#include <vita/core/error.hpp>
#include <array>
#include <cstddef>
#include <limits>
namespace vita::runtime {
enum class BudgetCategory : std::size_t { raw_blocks, providers, duplicate_values, duplicate_index, plans, transactions, revisions, context_history, queues, completion, scheduling, retention, metrics, adapters_stacks, headroom, count };
inline constexpr std::size_t budget_category_count=static_cast<std::size_t>(BudgetCategory::count);
inline constexpr std::size_t framework_budget=67108864;
inline constexpr std::size_t retention_headroom_transfer=704512;
inline constexpr std::size_t sdr_state_headroom_transfer=99712;
struct BudgetRow { std::size_t reserved{},charged{}; };
class BudgetLedger {
    std::array<BudgetRow,budget_category_count> rows_{};
public:
    BudgetLedger() noexcept {
        constexpr std::array<std::size_t,budget_category_count> reserve={30998528,2621440,8388608,524288,2097152,1048576,1114112+sdr_state_headroom_transfer,1048576,3145728,524288,1048576,262144+retention_headroom_transfer,1048576,4194304,9043968-retention_headroom_transfer-sdr_state_headroom_transfer};
        for(std::size_t i=0;i<rows_.size();++i) rows_[i].reserved=reserve[i];
    }
    Result<void> charge(BudgetCategory category,std::size_t bytes) noexcept {
        auto index=static_cast<std::size_t>(category); if(index>=rows_.size()) return std::unexpected(Error{ErrorCode::invalid_argument});
        auto& row=rows_[index]; if(bytes>row.reserved-row.charged) return std::unexpected(Error{ErrorCode::capacity_exhausted});
        row.charged+=bytes; return {};
    }
    template<class T> Result<void> charge_objects(BudgetCategory category,std::size_t count) noexcept {
        if(count>std::numeric_limits<std::size_t>::max()/sizeof(T)) return std::unexpected(Error{ErrorCode::overflow});
        return charge(category,count*sizeof(T));
    }
    Result<void> transfer_headroom(BudgetCategory target,std::size_t bytes) noexcept {
        auto index=static_cast<std::size_t>(target); if(index>=rows_.size()-1) return std::unexpected(Error{ErrorCode::invalid_argument});
        auto& head=rows_[static_cast<std::size_t>(BudgetCategory::headroom)];
        if(bytes>head.reserved-head.charged) return std::unexpected(Error{ErrorCode::capacity_exhausted});
        head.reserved-=bytes; rows_[index].reserved+=bytes; return {};
    }
    Result<void> transfer_unused(BudgetCategory from,BudgetCategory to,std::size_t bytes) noexcept {
        const auto source=static_cast<std::size_t>(from),target=static_cast<std::size_t>(to);
        if(source>=rows_.size()||target>=rows_.size()||source==target)return std::unexpected(Error{ErrorCode::invalid_argument});
        if(bytes>rows_[source].reserved-rows_[source].charged)return std::unexpected(Error{ErrorCode::capacity_exhausted});
        if(bytes>std::numeric_limits<std::size_t>::max()-rows_[target].reserved)return std::unexpected(Error{ErrorCode::overflow});
        rows_[source].reserved-=bytes;rows_[target].reserved+=bytes;return {};
    }
    BudgetRow row(BudgetCategory category) const noexcept { auto index=static_cast<std::size_t>(category); return index<rows_.size() ? rows_[index] : BudgetRow{}; }
    std::size_t charged_bytes() const noexcept { std::size_t n=0; for(auto r:rows_) n+=r.charged; return n; }
    std::size_t reserved_bytes() const noexcept { std::size_t n=0; for(auto r:rows_) n+=r.reserved; return n; }
};
} // namespace vita::runtime

#include <vita/memory/memory.hpp>
#include <vita/runtime/completion/ticket.hpp>
#include <vita/runtime/execution/arena.hpp>
#include <vita/runtime/execution/executor.hpp>
#include <vita/runtime/execution/admission.hpp>
namespace vita::runtime {
// Actual object-layout projection for currently available reference components.
// No allocations occur here. Future categories remain reserved, not declared implemented.
inline Result<BudgetLedger> reference_budget() noexcept {
    BudgetLedger ledger;
    auto charge=[&](BudgetCategory c,std::size_t n)->Result<void>{ return ledger.charge(c,n); };
    auto ensure=[&](BudgetCategory c,std::size_t n)->Result<void>{
        auto row=ledger.row(c); if(n<=row.reserved) return {};
        return ledger.transfer_headroom(c,n-row.reserved);
    };
    constexpr std::size_t raw=30998528;
    constexpr std::size_t providers=19584*sizeof(memory::detail::Block)+6*(sizeof(memory::detail::PoolState)+sizeof(memory::ExternalPool));
    constexpr std::size_t retained=1024*sizeof(memory::RetainedRx)+17*(sizeof(memory::detail::QuotaState)+sizeof(memory::RetentionQuota));
    constexpr std::size_t completion=CompletionArena<1024>::metadata_bytes()+sizeof(CompletionArena<1024>)+1024*(QuiescenceGuard::metadata_bytes()+sizeof(QuiescenceGuard));
    constexpr std::size_t plans=SlotArena<256,8192>::storage_bytes()+sizeof(SlotArena<256,8192>);
    // Ordinary/cancellation tasks, per-stream data queues, four worker queues, and TX descriptors.
    constexpr std::size_t queues=sizeof(BoundedExecutor<256>)+sizeof(BoundedExecutor<64>)+16*sizeof(BoundedExecutor<256>)+4*sizeof(BoundedExecutor<256>)+4096*sizeof(memory::TxStorage);
    constexpr std::size_t scheduling=AdmissionPool::metadata_bytes()+sizeof(AdmissionPool);
    for(auto item:std::array<std::pair<BudgetCategory,std::size_t>,7>{{
      {BudgetCategory::raw_blocks,raw},{BudgetCategory::providers,providers},{BudgetCategory::retention,retained},
      {BudgetCategory::completion,completion},{BudgetCategory::plans,plans},{BudgetCategory::queues,queues},{BudgetCategory::scheduling,scheduling}}}) {
        if(auto ok=ensure(item.first,item.second); !ok) return std::unexpected(ok.error());
        if(auto ok=charge(item.first,item.second); !ok) return std::unexpected(ok.error());
    }
    return ledger;
}
} // namespace vita::runtime
