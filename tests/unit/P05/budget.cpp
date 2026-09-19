#include <vita/runtime/budget/loopback.hpp>
#include <cassert>
#include <iostream>
using namespace vita;
using namespace vita::runtime;
int main() {
    auto reference=reference_budget(); assert(reference); auto& ledger=*reference; auto before=ledger.charged_bytes();
    constexpr auto adapter=adapters::loopback::Loopback<>::metadata_bytes();
    constexpr auto routes=sizeof(RouteRegistry<64>),counters=sizeof(CounterRegistry<64>);
    assert(budget::charge_loopback<>(ledger)); assert(budget::charge_registries<>(ledger));
    assert(ledger.charged_bytes()==before+adapter+routes+counters);
    assert(budget::charge_loopback<>(ledger)); assert(ledger.charged_bytes()==before+2*adapter+routes+counters);
    auto snapshot=ledger.charged_bytes(); assert(!budget::charge_loopback<>(ledger,SIZE_MAX)); assert(ledger.charged_bytes()==snapshot);
    BudgetLedger short_routes; auto reserve=short_routes.row(BudgetCategory::scheduling).reserved;
    assert(short_routes.charge(BudgetCategory::scheduling,reserve-routes)); auto prior=short_routes.charged_bytes();
    assert(!budget::charge_registries<>(short_routes)); assert(short_routes.charged_bytes()==prior);
    BudgetLedger short_adapter; assert(short_adapter.charge(BudgetCategory::adapters_stacks,short_adapter.row(BudgetCategory::adapters_stacks).reserved));
    prior=short_adapter.charged_bytes(); assert(!budget::charge_loopback<>(short_adapter)); assert(short_adapter.charged_bytes()==prior);
    assert(ledger.reserved_bytes()==framework_budget);
    std::cout<<"P05 budget projection: adapter="<<adapter<<" routes="<<routes<<" counters="<<counters<<" total="<<adapter+routes+counters<<'\n';
}
