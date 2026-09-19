#include <vita/runtime/budget/loopback.hpp>
#include <limits>
using namespace vita;using namespace vita::runtime;using namespace vita::runtime::budget;
int main(){
    BudgetLedger ledger;
    constexpr auto transport=sizeof(adapters::loopback::Loopback<32,64,64>)+32*QuiescenceGuard::metadata_bytes();
    if(!charge_loopback(ledger,2)||!charge_registries(ledger))return 1;
    if(ledger.row(BudgetCategory::adapters_stacks).charged!=2*transport ||
       ledger.row(BudgetCategory::scheduling).charged!=sizeof(RouteRegistry<64>)+sizeof(CounterRegistry<64>) ||
       ledger.charged_bytes()!=2*transport+sizeof(RouteRegistry<64>)+sizeof(CounterRegistry<64>))return 2;
    const auto before=ledger.charged_bytes();
    if(charge_loopback(ledger,std::numeric_limits<std::size_t>::max())||ledger.charged_bytes()!=before)return 3;
    BudgetLedger tight;auto reserved=tight.row(BudgetCategory::scheduling).reserved;
    if(!tight.charge(BudgetCategory::scheduling,reserved-sizeof(RouteRegistry<64>)-sizeof(CounterRegistry<64>)+1))return 4;
    const auto filled=tight.charged_bytes();if(charge_registries(tight)||tight.charged_bytes()!=filled)return 5;
    if(ledger.reserved_bytes()!=67108864||tight.reserved_bytes()!=67108864)return 6;
    return 0;
}
