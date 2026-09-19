#include <vita/codec/packet.hpp>
#include <vita/runtime/transaction/engine.hpp>
#include <vita/runtime/context/receiver.hpp>
#include <cassert>
using namespace vita;using namespace vita::codec;using namespace vita::runtime;using namespace vita::runtime::transaction;
template<class Field> static void check(typename Field::value_type value){
 NativeControlPacket<1024> command;assert(command.set<Field>(value));Envelope envelope;envelope.type=PacketType::command;envelope.stream_id=1;envelope.command=Command{0xa91c0000,42,Identifier::short_id(2),Identifier::short_id(3)};
 std::array<std::byte,2048> wire{};auto count=encode_packet(envelope,command.freeze(),wire);assert(count);auto parsed=decode_packet(Bytes{wire}.first(*count));assert(parsed&&parsed->fields.size()==1);
 AdmissionPool pool(AdmissionPool::reference_capacities());VirtualBackend<> backend;StateSnapshot initial;initial.fields[1].value=*Hertz::from_integer(1);initial.fields[1].validity=Validity::known;Engine<1> engine(pool,backend.binding(),initial);auto h=engine.accept(*parsed,{});assert(!h&&h.error().code==ErrorCode::unsupported_capability);assert(backend.begins()==0&&backend.writes()==0&&std::get<Hertz>(engine.state().fields[1].value)==*Hertz::from_integer(1));
 auto context=*parsed;context.envelope.envelope.type=PacketType::context;context.envelope.envelope.command.reset();context.envelope.envelope.timestamp={Tsi::gps,Tsf::picoseconds,1,0};vita::runtime::context::ReceiverHistory<> history;auto received=history.receive(context,1,{0});assert(!received&&received.error().code==ErrorCode::unsupported_capability);
}
int main(){std::array<std::uint32_t,1> entry{1};check<IndexList>({1,entry});check<PointingVectorStructure>({});check<Spectrum>({});check<SectorStepScan>({});}
