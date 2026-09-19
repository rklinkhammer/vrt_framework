#include <vita/runtime/transaction/engine.hpp>
#include "sink.hpp"
using namespace vita;using namespace vita::codec;using namespace vita::runtime;using namespace vita::runtime::transaction;
int main(){
    ControlPacket p;p.set<SampleRate>(*Hertz::from_integer(2));Envelope e;e.type=PacketType::command;e.stream_id=1;e.command=Command{0xa9000000,1,Identifier::short_id(2),Identifier::short_id(3)};
    std::array<std::byte,256> wire;auto size=encode_packet(e,p.freeze(),wire);if(!size)return 1;auto packet=decode_packet(Bytes{wire}.first(*size));if(!packet)return 2;
    StateSnapshot initial;initial.fields[1].validity=Validity::known;initial.fields[1].value=*Hertz::from_integer(1);
    for(auto resource:{Resource::transaction,Resource::plan_bytes,Resource::completion,Resource::response,Resource::revision,Resource::context_publication,Resource::duplicate_entry,Resource::duplicate_bytes,Resource::ordinary_queue}){
        auto limits=AdmissionPool::reference_capacities();limits.need(resource,0);AdmissionPool pool(limits);VirtualBackend<> backend;VerifySink sink;
        Engine<1> engine(pool,backend.binding(),initial,{Profile::iq_generator_v1,sink.binding()});
        if(engine.accept(*packet,{})||backend.begins()||backend.writes()||sink.state->occupied||engine.state().version)return 3;
        for(std::size_t i=0;i<resource_count;++i)if(pool.used(static_cast<Resource>(i)))return 4;
    }
    {
        AdmissionPool pool(AdmissionPool::reference_capacities());VirtualBackend<> backend;VerifySink sink;sink.state->capacity=0;Engine<1> engine(pool,backend.binding(),initial,{Profile::iq_generator_v1,sink.binding()});
        if(engine.accept(*packet,{})||backend.begins()||backend.writes())return 5;
        for(std::size_t i=0;i<resource_count;++i)if(pool.used(static_cast<Resource>(i)))return 6;
    }
    {
        AdmissionPool pool(AdmissionPool::reference_capacities());VirtualBackend<> backend;VerifySink sink;Engine<1> engine(pool,backend.binding(),initial,{Profile::iq_generator_v1,sink.binding()});
        auto h=engine.accept(*packet,{});if(!h||sink.state->occupied!=1)return 7;
        if(!engine.progress({})||!backend.complete_next()||!engine.progress({})||!*engine.complete(*h))return 8;
        if(!engine.release(*h)||sink.state->invalid||sink.state->count!=1||sink.state->occupied!=1)return 9;
        if(pool.used(Resource::transaction)||pool.used(Resource::completion)||pool.used(Resource::revision)!=1||pool.used(Resource::context_publication)!=1)return 10;
        const auto& saved=*sink.state->records[0];if(saved.event.outcome.status!=FieldStatus::executed||saved.event.state.fields[1].validity!=Validity::known||saved.credits.held(Resource::revision)!=1)return 11;
        sink.clear();if(pool.used(Resource::revision)||pool.used(Resource::context_publication)||sink.state->occupied)return 12;
    }
    return 0;
}
