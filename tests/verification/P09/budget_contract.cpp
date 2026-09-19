#include <vita/runtime/context/budget.hpp>
using namespace vita;using namespace vita::runtime;using namespace vita::runtime::context;
int main(){BudgetLedger ledger;
 if(!charge_context(ledger,16))return 1;
 const auto revision=16*(RevisionStore<>::storage_bytes()+sizeof(RevisionStore<>));const auto history=16*sizeof(ReceiverHistory<>);const auto queues=16*(sizeof(ContextPublisher<>)+sizeof(ContextReceiver<>)-sizeof(ReceiverHistory<>)+sizeof(memory::detail::QuotaState));
 if(ledger.row(BudgetCategory::revisions).charged!=revision||ledger.row(BudgetCategory::context_history).charged!=history||ledger.row(BudgetCategory::queues).charged!=queues||ledger.charged_bytes()!=revision+history+queues)return 2;
 auto before=ledger.charged_bytes();if(charge_context(ledger,SIZE_MAX)||ledger.charged_bytes()!=before)return 3;
 BudgetLedger tight;if(!tight.charge(BudgetCategory::queues,tight.row(BudgetCategory::queues).reserved))return 4;before=tight.charged_bytes();
 if(charge_context(tight)||tight.charged_bytes()!=before||tight.row(BudgetCategory::revisions).charged||tight.row(BudgetCategory::context_history).charged)return 5;
 return 0;}
