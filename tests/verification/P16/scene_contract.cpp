#include <vita/profiles/iq/frequency_scan.hpp>
#include <array>
#include <cassert>
#include <bit>
#include <cstdlib>
#include <new>
using namespace vita;using namespace vita::profiles::iq;using namespace vita::runtime;
static unsigned allocations=0;
void*operator new(std::size_t n){++allocations;auto*p=std::malloc(n?n:1);if(!p)std::abort();return p;}void*operator new[](std::size_t n){return ::operator new(n);}void operator delete(void*p)noexcept{std::free(p);}void operator delete[](void*p)noexcept{std::free(p);}
void*operator new(std::size_t n,std::align_val_t a){++allocations;void*p=nullptr;if(posix_memalign(&p,static_cast<std::size_t>(a),n?n:1))std::abort();return p;}void operator delete(void*p,std::align_val_t)noexcept{std::free(p);}
static StateSnapshot state(std::uint64_t hz){StateSnapshot s;s.profile=Profile::frequency_tunable;s.fields[field_index(SampleRate::id)]={SampleRate::id,*Hertz::from_integer(100'000),Validity::known};s.fields[field_index(RFReferenceFrequency::id)]={RFReferenceFrequency::id,*Hertz::from_integer(hz),Validity::known};return s;}
static EffectiveEvent event(std::uint64_t ordinal,std::uint64_t hz){EffectiveEvent e;e.state=state(hz);e.sample_ordinal=ordinal;e.ordinal_known=true;e.association_generation=1;e.changed_mask=1u<<field_index(RFReferenceFrequency::id);e.outcome.id=RFReferenceFrequency::id;e.outcome.status=FieldStatus::executed;return e;}
static std::int16_t component(Bytes b,std::size_t word){auto raw=(std::uint16_t(std::to_integer<unsigned>(b[word*2]))<<8)|std::to_integer<unsigned>(b[word*2+1]);return std::bit_cast<std::int16_t>(static_cast<std::uint16_t>(raw));}
int main(){
 for(auto center:{100'025'000ull,100'050'000ull,100'075'000ull}){
  auto scene=VirtualRfScene::create();assert(scene&&scene->effective(event(0,center)));auto s=state(center);std::array<std::byte,16> data{};auto window=SampleWriteWindow::create(data,SampleFormat::iq16,0,4,s);assert(window&&scene->produce(*window)&&window->validate_complete());
  constexpr std::array<std::int16_t,8> positive{16384,0,0,16384,-16384,0,0,-16384},dc{16384,0,16384,0,16384,0,16384,0},negative{16384,0,0,-16384,-16384,0,0,16384};const auto&expected=center==100'025'000?positive:center==100'050'000?dc:negative;for(unsigned i=0;i<8;++i)assert(component(data,i)==expected[i]);assert(scene->next_ordinal()==4);
 }
 // Upper Nyquist is suppressed; phase still evolves. Lower Nyquist is admitted.
 auto edge=VirtualRfScene::create();assert(edge&&edge->effective(event(0,100'000'000)));auto s=state(100'000'000);std::array<std::byte,12> silent{};auto w=SampleWriteWindow::create(silent,SampleFormat::iq16,0,3,s);assert(w&&edge->produce(*w));for(auto b:silent)assert(b==std::byte{});assert(edge->phase_numerator()==50'000);assert(edge->effective(event(3,100'100'000)));s=state(100'100'000);std::array<std::byte,8> nyquist{};auto n=SampleWriteWindow::create(nyquist,SampleFormat::iq16,3,2,s);assert(n&&edge->produce(*n));assert(component(nyquist,0)==-16384&&component(nyquist,1)==0&&component(nyquist,2)==16384&&component(nyquist,3)==0);
 // Multiple committed centers must be integrated even with no intervening produce calls.
 auto skipped=VirtualRfScene::create();assert(skipped&&skipped->effective(event(0,100'025'000)));assert(skipped->effective(event(3,100'040'000)));assert(skipped->effective(event(7,100'065'000)));assert(skipped->effective(event(11,100'065'000)));assert(skipped->next_ordinal()==11&&skipped->phase_numerator()==55'000);
 // An out-of-band interval contributes its LO phase rather than resetting on reentry.
 auto hidden=VirtualRfScene::create();assert(hidden&&hidden->effective(event(0,100'025'000)));assert(hidden->effective(event(3,100'180'000)));assert(hidden->effective(event(7,100'050'000)));assert(hidden->phase_numerator()==55'000); // .75 - 1.3*4 = -4.45 cycles.
 assert(hidden->effective(event(7,100'025'000)));assert(hidden->phase_numerator()==55'000);assert(!hidden->effective(event(6,100'025'000))&&hidden->faulted());
 auto large=VirtualRfScene::create();assert(large&&large->effective(event(0,100'025'000)));constexpr auto ordinal=UINT64_MAX-256;assert(large->effective(event(ordinal,100'025'000)));assert(large->phase_numerator()==(ordinal%4)*25'000);
 auto invalid=VirtualRfScene::create();auto missing=event(0,100'050'000);missing.state.fields[field_index(RFReferenceFrequency::id)].validity=Validity::unknown;assert(invalid&&!invalid->effective(missing)&&invalid->faulted());
 auto simulation=VirtualRfScene::create();auto simulated=event(0,100'050'000);simulated.outcome.simulated=true;assert(simulation&&!simulation->effective(simulated));
 assert(!VirtualRfScene::create({0,100'050'000,0.5}));assert(!VirtualRfScene::create({100'000,100'050'000,2}));
 // Recovery uses an explicitly new epoch at the actual retained ordinal.
 auto ordinary_origin=VirtualRfScene::create();assert(ordinary_origin&&!ordinary_origin->effective(event(123,100'025'000)));
 auto fresh=VirtualRfScene::create_for_recovery();assert(fresh);auto fresh_state=state(100'025'000);std::array<std::byte,4> preinit{};auto prewindow=SampleWriteWindow::create(preinit,SampleFormat::iq16,123,1,fresh_state);assert(prewindow&&!fresh->produce(*prewindow));assert(fresh->effective(event(123,100'025'000))&&fresh->phase_numerator()==0);assert(fresh->effective(event(126,100'040'000))&&fresh->phase_numerator()==75'000);assert(fresh->effective(event(130,100'050'000))&&fresh->phase_numerator()==15'000);
 auto bad_recovery=VirtualRfScene::create_for_recovery();auto unknown_initial=event(123,100'025'000);unknown_initial.state.fields[4].validity=Validity::unknown;assert(bad_recovery&&!bad_recovery->effective(unknown_initial)&&bad_recovery->faulted());assert(!bad_recovery->effective(event(123,100'025'000)));
 assert(allocations==0);auto*p=::operator new(1);::operator delete(p);p=::operator new(64,std::align_val_t{64});::operator delete(p,std::align_val_t{64});assert(allocations==2);
}
