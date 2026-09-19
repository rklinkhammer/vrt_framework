#include <vita/runtime/context/revisions.hpp>
#include <thread>
using namespace vita;using namespace vita::runtime;using namespace vita::runtime::context;
static EffectiveEvent event(unsigned rate,unsigned generation=1){EffectiveEvent e;e.association_generation=generation;e.actual_time={100,rate};e.time_known=e.ordinal_known=true;e.sample_ordinal=rate;for(auto id:baseline_fields)e.state.fields[field_index(id)].validity=Validity::known;e.state.fields[1].value=*Hertz::from_integer(rate);e.state.fields[3].value=PayloadFormat{0x200003cf00000000ULL};return e;}
static AdmissionBundle credits(AdmissionPool& p){return std::move(*p.acquire(AdmissionRequest{}.need(Resource::revision).need(Resource::context_publication)));}
int main(){
    AdmissionPool pool(AdmissionPool::reference_capacities());
    {
        RevisionStore<4> store;auto invalid=event(1);invalid.state.fields[1].value=std::uint32_t{1};auto bad1=store.initial(invalid,credits(pool));if(bad1||store.occupied()||pool.used(Resource::revision))return 21;
        invalid=event(1);invalid.state.fields[1].value=Hertz{-1};auto bad2=store.initial(invalid,credits(pool));if(bad2||store.occupied()||pool.used(Resource::revision))return 22;
        invalid=event(1);invalid.state.fields[3].value=std::uint32_t{1};auto bad3=store.initial(invalid,credits(pool));if(bad3||store.occupied()||pool.used(Resource::revision))return 23;
    }
    {
        RevisionStore<2> store;auto old=store.initial(event(1),credits(pool));if(!old)return 1;
        auto reserve=store.reserve(1);if(!reserve||store.reserve(1))return 2;
        auto sink=store.binding();sink.record(sink.context,event(2),*reserve,credits(pool));reserve->reset();
        auto current=store.current();if(!current||current->id()==old->id()||pool.used(Resource::revision)!=2)return 3;
        if(!store.mark_publication(*old,Publication::accepted))return 4;store.collect();if(store.occupied()!=2)return 5;
        RevisionHandle concurrent=*old;old->operator=(RevisionHandle{});
        std::thread release([handle=std::move(concurrent)]()mutable{if(std::get<Hertz>(handle.event().state.fields[1].value).q20!=(1<<20))std::abort();handle={};});
        store.collect();release.join();store.collect();if(store.occupied()!=1||pool.used(Resource::revision)!=1)return 6;
    }
    if(pool.used(Resource::revision)||pool.used(Resource::context_publication))return 7;
    {
        RevisionStore<4> store,foreign;auto old=store.initial(event(1),credits(pool));auto other=foreign.initial(event(1),credits(pool));
        if(!old||!other||store.mark_publication(*other,Publication::accepted)||store.note_data_dependency(*other)||store.accept_group(*other))return 8;
        auto stale=store.reserve(1);if(!stale||!store.detach(2))return 9;
        auto current=store.initial(event(1,2),credits(pool));if(!current)return 10;
        auto sink=store.binding();sink.record(sink.context,event(2),*stale,credits(pool));
        if(store.faulted()||store.current()->id()!=current->id()||store.accept_group(*old)||current->publication()!=Publication::pending)return 11;
        if(std::get<Hertz>(old->event().state.fields[1].value).q20!=(1<<20))return 12;
    }
    {
        RevisionHandle retained;{RevisionStore<1> store;auto first=store.initial(event(3),credits(pool));if(!first)return 13;retained=*first;}
        if(pool.used(Resource::revision)!=1||std::get<Hertz>(retained.event().state.fields[1].value).q20!=3*(1<<20))return 14;
        retained={};if(pool.used(Resource::revision))return 15;
    }
    for(unsigned dependent=0;dependent<3;++dependent){
        RevisionStore<4> store;auto first_event=event(1);first_event.actual_time={100,0};first_event.sample_ordinal=0;
        if(dependent==2)first_event.state.fields[2].value=std::uint32_t{(1u<<24)|(1u<<12)};
        auto first=store.initial(first_event,credits(pool));if(!first)return 16;
        if(dependent==1&&!store.note_data_dependency(*first))return 17;
        auto next_event=event(2);next_event.actual_time={100,0};next_event.sample_ordinal=0;
        auto reserve=store.reserve(1);if(!reserve)return 18;auto sink=store.binding();sink.record(sink.context,next_event,*reserve,credits(pool));
        auto group=store.publication_group();if(dependent){if(group||store.accept_group(*store.current()))return 19;}
        else {if(!group||group->id()!=store.current()->id()||!store.accept_group(*group)||first->publication()!=Publication::accepted)return 20;}
    }
    return 0;
}
