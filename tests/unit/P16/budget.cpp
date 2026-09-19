#include <vita/profiles/iq/lab.hpp>
#include <vita/runtime/public/runtime.hpp>
#include <cassert>
#include <cstdio>
using namespace vita;
int main(){
 runtime::BudgetLedger ledger;const auto cap=ledger.reserved_bytes();const auto before=ledger.row(runtime::BudgetCategory::plans);
 assert(ledger.charge(runtime::BudgetCategory::plans,before.reserved));assert(!ledger.transfer_unused(runtime::BudgetCategory::plans,runtime::BudgetCategory::headroom,1));assert(ledger.row(runtime::BudgetCategory::plans).reserved==before.reserved&&ledger.reserved_bytes()==cap);
 assert(!ledger.transfer_unused(runtime::BudgetCategory::count,runtime::BudgetCategory::headroom,0));assert(!ledger.transfer_unused(runtime::BudgetCategory::plans,runtime::BudgetCategory::plans,0));
 assert(ledger.transfer_unused(runtime::BudgetCategory::queues,runtime::BudgetCategory::headroom,123));assert(ledger.reserved_bytes()==cap);
 auto config=profiles::iq::lab::config(0xabcdef);auto pools=profiles::iq::lab::pools(profiles::iq::lab::reference_counts());assert(config&&pools);
 auto made=VitaRuntime<>::create(*config,std::move(*pools));assert(made);
 for(unsigned i=0;i<16;++i){StreamConfig s;s.sid=i+1;s.controller_id=2;s.controllee_id=3;s.profile=profiles::iq::Profile::frequency_tunable;s.sample_rate=100000;assert((*made)->add_controllee(s));}
 const auto& actual=(*made)->budget();assert(actual.charged_bytes()<=runtime::framework_budget&&actual.reserved_bytes()==runtime::framework_budget);assert(actual.row(runtime::BudgetCategory::plans).charged==0);
 std::printf("full16streams charged%zu cap%zu plan_reservation_transfer%zu\n",actual.charged_bytes(),runtime::framework_budget,before.reserved-actual.row(runtime::BudgetCategory::plans).reserved);
}
