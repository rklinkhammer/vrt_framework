#include <vita/profiles/iq/frequency_scan.hpp>
#include <cassert>
#include <array>
#include <cstdlib>
#include <new>
using namespace vita;
using namespace vita::profiles::iq;
static unsigned allocations=0;
void* operator new(std::size_t n){++allocations;if(auto p=std::malloc(n))return p;std::abort();}
void operator delete(void*p)noexcept{std::free(p);}
void operator delete(void*p,std::size_t)noexcept{std::free(p);}
static runtime::StateSnapshot state(std::uint64_t hz){
    runtime::StateSnapshot s;
    s.fields[1]={SampleRate::id,*Hertz::from_integer(100'000),runtime::Validity::known};
    s.fields[runtime::field_index(RFReferenceFrequency::id)]={RFReferenceFrequency::id,*Hertz::from_integer(hz),runtime::Validity::known};return s;
}
static runtime::EffectiveEvent event(std::uint64_t hz,std::uint64_t ordinal){
    runtime::EffectiveEvent e;e.state=state(hz);e.sample_ordinal=ordinal;e.ordinal_known=true;return e;
}
int main(){
    const auto before=allocations;
    auto scene=VirtualRfScene::create();assert(scene);
    assert(scene->effective(event(100'025'000,0)));
    assert(scene->effective(event(100'040'000,3)));
    assert(scene->effective(event(100'065'000,7)));
    std::array<std::byte,4> bytes{};auto config=state(100'065'000);
    auto window=SampleWriteWindow::create(bytes,SampleFormat::iq16,11,1,config);assert(window);
    assert(scene->produce(*window)&&window->validate_complete());
    // At11: .25*3+.10*4-.15*4=.55; after one emitted sample .40.
    assert(scene->phase_numerator()==40'000&&scene->next_ordinal()==12);
    const auto i=static_cast<std::int16_t>((codec::detail::load32(bytes,0)>>16));
    const auto q=static_cast<std::int16_t>(codec::detail::load32(bytes,0));
    assert(i==-15582 && q==-5063);
    assert(!scene->effective(event(100'000'000,10))&&scene->faulted());
    assert(!scene->produce(*window));
    scene=VirtualRfScene::create();assert(scene->effective(event(100'000'000,0))); // upper edge +Fs/2 excluded
    config=state(100'000'000);auto oob=SampleWriteWindow::create(bytes,SampleFormat::iq16,0,1,config);assert(oob&&scene->produce(*oob));
    assert(codec::detail::load32(bytes,0)==0&&scene->phase_numerator()==50'000);
    assert(scene->effective(event(100'100'000,1))); // lower edge -Fs/2 included; phase unchanged
    config=state(100'100'000);auto lower=SampleWriteWindow::create(bytes,SampleFormat::iq16,1,1,config);assert(lower&&scene->produce(*lower));
    assert(codec::detail::load32(bytes,0)==0xc0000000u);
    assert(scene->effective(event(100'250'000,2)));assert(scene->effective(event(100'050'000,9)));
    assert(scene->phase_numerator()==0); // OOB difference -200k advances integer cycles only.
    config=state(100'050'000);auto dc=SampleWriteWindow::create(bytes,SampleFormat::iq16,9,1,config);assert(dc&&scene->produce(*dc));
    assert(codec::detail::load32(bytes,0)==0x40000000u);
    auto bad=event(100'000'000,10);bad.ordinal_known=false;assert(!scene->effective(bad));
    scene=VirtualRfScene::create();assert(scene->effective(event(100'025'000,0)));
    assert(scene->effective(event(100'025'000,UINT64_MAX-1)));assert(scene->next_ordinal()==UINT64_MAX-1);
    config=state(100'025'000);auto last=SampleWriteWindow::create(bytes,SampleFormat::iq16,UINT64_MAX,1,config);assert(last&&!scene->produce(*last));
    auto strict=VirtualRfScene::create();assert(strict&&!strict->effective(event(100'025'000,99)));
    auto recovered=VirtualRfScene::create_for_recovery();assert(recovered);
    config=state(100'025'000);auto recovery_window=SampleWriteWindow::create(bytes,SampleFormat::iq16,99,1,config);assert(recovery_window&&!recovered->produce(*recovery_window));
    assert(recovered->effective(event(100'025'000,99)));assert(recovered->phase_numerator()==0&&recovered->next_ordinal()==99);
    assert(recovered->produce(*recovery_window)&&codec::detail::load32(bytes,0)==0x40000000u);
    assert(recovered->effective(event(100'040'000,103)));assert(recovered->phase_numerator()==0); // four old-center samples
    assert(recovered->effective(event(100'040'000,104)));assert(recovered->phase_numerator()==10'000); // ordinary event preserves phase
    auto invalid_recovery=VirtualRfScene::create_for_recovery();auto invalid_event=event(100'025'000,99);invalid_event.outcome.simulated=true;
    assert(!invalid_recovery->effective(invalid_event)&&invalid_recovery->faulted());assert(!invalid_recovery->effective(event(100'025'000,99)));
    invalid_recovery=VirtualRfScene::create_for_recovery();invalid_event=event(100'025'000,99);invalid_event.state.fields[1].value=*Hertz::from_integer(100001);
    assert(!invalid_recovery->effective(invalid_event));
    assert(!VirtualRfScene::create({0,100'050'000,0.5}));
    auto sweep=SweepPolicy::create();assert(sweep);
    assert(**sweep->next({0})==100'000'000);
    assert(!sweep->next({1})->has_value());
    assert(sweep->readback(100'000'000,true,{10}));assert(sweep->phase()==SweepPhase::awaiting);
    assert(sweep->execution({true,false,false,false},{20}));assert(sweep->deadline_ns()==100'000'020);
    assert(sweep->execution({true,false,false,false},{21})&&sweep->readback(100'000'000,true,{22}));
    assert(sweep->deadline_ns()==100'000'020);
    assert(!sweep->next({100'000'019})->has_value());assert(**sweep->next({100'000'020})==100'025'000);
    assert(!sweep->execution({true,true,false,false},{100'000'021}));assert(sweep->phase()==SweepPhase::failed);
    sweep=SweepPolicy::create();assert(sweep->next({100}));assert(!sweep->next({1'000'000'100}));
    assert(!sweep->execution({true,false,false,false},{1'000'000'101}));
    auto short_config=SweepConfig{};short_config.stop_hz=short_config.start_hz;short_config.dwell_ns=0;
    sweep=SweepPolicy::create(short_config);assert(sweep->next({0}));assert(sweep->execution({true,false,false,false},{1}));
    assert(sweep->readback(100'000'000,true,{2}));assert(!sweep->next({2})->has_value()&&sweep->phase()==SweepPhase::complete);
    short_config.sweeps=2;sweep=SweepPolicy::create(short_config);assert(sweep->next({0}));
    for(std::uint64_t n=0;n<2;++n){assert(sweep->execution({true,false,false,false},{n*3+1}));assert(sweep->readback(100'000'000,true,{n*3+2}));auto next=sweep->next({n*3+2});assert(next&&next->has_value()==(n==0));}
    assert(sweep->phase()==SweepPhase::complete&&sweep->completed_points()==2);
    short_config.continuous=true;sweep=SweepPolicy::create(short_config);assert(sweep->next({0}));
    assert(sweep->execution({true,false,false,false},{1})&&sweep->readback(100'000'000,true,{2}));assert(**sweep->next({2})==100'000'000);
    assert(!sweep->readback(100'000'001,true,{3}));
    assert(allocations==before);
}
