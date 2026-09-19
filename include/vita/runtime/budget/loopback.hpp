#pragma once
#include <vita/runtime/execution/budget.hpp>
#include <vita/adapters/loopback/loopback.hpp>
namespace vita::runtime::budget {
// Setup-only composition helpers. Providers, raw buffers and shared registries are
// separate charges; the transport metadata already includes each slot's guard state.
template<std::size_t N=32,std::size_t Routes=64,std::size_t Counters=64>
Result<void> charge_loopback(BudgetLedger& ledger,std::size_t instances=1) noexcept {
    constexpr auto bytes=adapters::loopback::Loopback<N,Routes,Counters>::metadata_bytes();
    if(instances>std::numeric_limits<std::size_t>::max()/bytes) return std::unexpected(Error{ErrorCode::overflow});
    auto candidate=ledger; auto charged=candidate.charge(BudgetCategory::adapters_stacks,instances*bytes);
    if(!charged) return std::unexpected(charged.error()); ledger=candidate; return {};
}
template<std::size_t Routes=64>
Result<void> charge_routes(BudgetLedger& ledger,std::size_t instances=1) noexcept {
    auto candidate=ledger; auto charged=candidate.charge_objects<RouteRegistry<Routes>>(BudgetCategory::scheduling,instances);
    if(!charged) return std::unexpected(charged.error()); ledger=candidate; return {};
}
template<std::size_t Counters=64>
Result<void> charge_counters(BudgetLedger& ledger,std::size_t instances=1) noexcept {
    auto candidate=ledger; auto charged=candidate.charge_objects<CounterRegistry<Counters>>(BudgetCategory::scheduling,instances);
    if(!charged) return std::unexpected(charged.error()); ledger=candidate; return {};
}
template<std::size_t Routes=64,std::size_t Counters=64>
Result<void> charge_registries(BudgetLedger& ledger) noexcept {
    auto candidate=ledger;
    auto routes=charge_routes<Routes>(candidate); if(!routes) return std::unexpected(routes.error());
    auto counters=charge_counters<Counters>(candidate); if(!counters) return std::unexpected(counters.error());
    ledger=candidate; return {};
}
} // namespace vita::runtime::budget
