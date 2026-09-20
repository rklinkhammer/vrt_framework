#include <vita/profiles/iq/lab.hpp>
#include <vita/runtime/public/runtime.hpp>
#include <cassert>
using namespace vita;using namespace vita::runtime;
// Measured 16-stream projection: five query selectors, atomic commit binding,
// replay high-water and bounded supported-value domains; total reservation stays64MiB.
#if defined(__GLIBCXX__)
constexpr std::size_t expected_runtime_charge=53'446'224;
constexpr std::size_t expected_plans_reservation=496'240;
#else
constexpr std::size_t expected_runtime_charge=53'452'064;
constexpr std::size_t expected_plans_reservation=495'728;
#endif
int main(){BudgetLedger ledger;assert(ledger.reserved_bytes()==67108864);assert(ledger.charge(BudgetCategory::plans,100));auto before=ledger;auto donor=ledger.row(BudgetCategory::plans);assert(!ledger.transfer_unused(BudgetCategory::plans,BudgetCategory::headroom,donor.reserved-99));for(std::size_t i=0;i<budget_category_count;++i){auto c=static_cast<BudgetCategory>(i);assert(ledger.row(c).reserved==before.row(c).reserved&&ledger.row(c).charged==before.row(c).charged);}assert(!ledger.transfer_unused(BudgetCategory::plans,BudgetCategory::plans,0));assert(!ledger.transfer_unused(BudgetCategory::count,BudgetCategory::headroom,0));assert(ledger.transfer_unused(BudgetCategory::plans,BudgetCategory::headroom,317456));assert(ledger.row(BudgetCategory::plans).reserved==donor.reserved-317456&&ledger.row(BudgetCategory::plans).charged==100&&ledger.reserved_bytes()==67108864);
 auto projection=reference_budget();assert(projection&&projection->row(BudgetCategory::plans).charged>2'000'000);
	auto config=profiles::iq::lab::config(0xabcdef);auto pools=profiles::iq::lab::pools(profiles::iq::lab::reference_counts());assert(config&&pools);auto runtime=VitaRuntime<>::create(*config,std::move(*pools));assert(runtime);for(unsigned i=0;i<16;++i){StreamConfig stream;stream.sid=i+1;stream.controller_id=2;stream.controllee_id=3;stream.profile=profiles::iq::Profile::frequency_tunable;stream.sample_rate=100000;assert((*runtime)->add_controllee(stream));}assert((*runtime)->budget().reserved_bytes()==67108864);assert((*runtime)->budget().charged_bytes()==expected_runtime_charge);assert((*runtime)->budget().row(BudgetCategory::plans).charged==0);assert((*runtime)->budget().row(BudgetCategory::plans).reserved==expected_plans_reservation);
 auto small=*config;small.memory_limit=1;auto external=profiles::iq::lab::pools();assert(external);assert(!VitaRuntime<>::create(small,std::move(*external)));
}
