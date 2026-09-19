#include <vita/runtime/transaction/controller.hpp>
#include <cassert>
#include <iostream>
using namespace vita;using namespace vita::codec;using namespace vita::runtime;using namespace vita::runtime::transaction;
int main(){
    Envelope request;request.type=PacketType::command;request.stream_id=1;request.command=Command{0xa9080000,0,Identifier::short_id(2),Identifier::short_id(3)};
    ControlPacket controls;assert(controls.set<SampleRate>(*Hertz::from_integer(1000000)));std::array<std::byte,512> wire{};
    auto size=encode_packet(request,controls.freeze(),wire);assert(size);auto packet=decode_packet(Bytes{wire}.first(*size));assert(packet);
    auto key=transaction_key(*packet,1,{7,1});assert(key);ControllerRegistry<4> registry;
    auto relationship=registry.register_relationship(*key,UINT32_MAX);assert(relationship);
    auto tracked=registry.track(*relationship,*packet,{0},50000000);assert(tracked&&tracked->envelope.command->message_id==UINT32_MAX);
    assert(!registry.track(*relationship,*packet,{0},50000000));auto fresh=*key;fresh.binding_generation=2;fresh.peer.generation=2;assert(!registry.register_relationship(fresh));fresh.stream_id=2;assert(registry.register_relationship(fresh));
    Envelope cancel=tracked->envelope;cancel.cancel=true;CancelPacket selectors;assert(selectors.select<SampleRate>());
    std::array<std::byte,512> cancel_wire{};auto cancel_size=encode_packet(cancel,selectors.freeze(),cancel_wire);assert(cancel_size);auto cancellation=decode_packet(Bytes{cancel_wire}.first(*cancel_size));assert(cancellation);
    assert(*registry.register_cancel(tracked->handle,*cancellation,{10},100)==CancelRegistration::fresh);
    auto deadline=*registry.deadline_for(tracked->handle,true);cancel.packet_count=9;cancel_size=encode_packet(cancel,selectors.freeze(),cancel_wire);cancellation=decode_packet(Bytes{cancel_wire}.first(*cancel_size));
    assert(*registry.register_cancel(tracked->handle,*cancellation,{50},999)==CancelRegistration::retry);assert(*registry.deadline_for(tracked->handle,true)==deadline);
    CancelPacket empty;cancel_size=encode_packet(cancel,empty.freeze(),cancel_wire);cancellation=decode_packet(Bytes{cancel_wire}.first(*cancel_size));assert(!registry.register_cancel(tracked->handle,*cancellation,{60},100));
    assert(registry.advance({110}));assert((*registry.observer(tracked->handle))->cancellation_timed_out());assert(!(*registry.observer(tracked->handle))->timed_out());
    AckRecord ack;ack.request=tracked->envelope;ack.cam=*Cam::parse(ack.request,Profile::generic_virtual_test);ack.kind=AckKind::execution;ack.cancellation=true;ack.scheduled_or_executed=true;
    auto ack_size=encode_response(ack,wire,0);assert(ack_size);assert(!registry.receive(1,{8,1},Bytes{wire}.first(*ack_size),{120}));
    assert(registry.receive(1,{7,1},Bytes{wire}.first(*ack_size),{120}));auto observation=(*registry.observer(tracked->handle))->observation();assert(observation.cancellation&&observation.confirms_cancellation&&!observation.confirms_execution&&!observation.success);
    assert(registry.advance({50000000}));assert((*registry.observer(tracked->handle))->timeout_observation());
    assert(registry.retain(tracked->handle));assert(registry.release(tracked->handle));registry.expire({40000000000});assert(registry.size()==1);
    assert(registry.release(tracked->handle));registry.expire({40000000000});assert(registry.size()==0);
    // A delayed progress pump cannot turn an expired deadline into on-time success.
    ControllerRegistry<2> delayed;auto rel=delayed.register_relationship(*key);assert(rel);
    auto late=delayed.track(*rel,*packet,{0},100);assert(late);ack.request=late->envelope;ack.cancellation=false;
    ack_size=encode_response(ack,wire);assert(ack_size);
    assert(delayed.receive(1,{7,1},Bytes{wire}.first(*ack_size),{100}));
    auto delayed_observer=*delayed.observer(late->handle);assert(delayed_observer->timed_out()&&delayed_observer->observation().confirms_execution&&!delayed_observer->observation().success);
    assert(delayed.release(late->handle));
    assert(delayed.receive(1,{7,1},Bytes{wire}.first(*ack_size),{20000000000ULL}));
    delayed.expire({30000000100ULL});assert(delayed.size()==0); // replay does not extend retention
    std::cout<<"P07 controller record="<<ControllerRegistry<>::record_bytes()<<" storage="<<ControllerRegistry<>::storage_bytes()<<'\n';
}
