#include <vita/runtime/transaction/outcomes.hpp>
#include <array>
#include <cassert>
using namespace vita;
using namespace vita::codec;
using namespace vita::runtime;
using namespace vita::runtime::transaction;
PacketView response(std::uint32_t cam,std::array<std::byte,24>& bytes,bool cancellation=false) {
    std::array<std::uint32_t,6> words{cancellation?0x65000006u:0x64000006u,1,cam,42,2,3};
    for(std::size_t i=0;i<6;++i)for(unsigned b=0;b<4;++b)bytes[i*4+b]=std::byte(words[i]>>(24-8*b));
    auto view=decode_packet(bytes);assert(view);return std::move(*view);
}
int main() {
    ControllerObserver observer(42);std::array<std::byte,24> bytes{};
    observer.local_send(true);assert(!observer.observation().success&&observer.observation().unknown_remote_outcome);
    assert(observer.receive(response(0xa9100400,bytes)));assert(observer.observation().unknown_remote_outcome);
    assert(observer.receive(response(0xa8880400,bytes)));assert(observer.observation().hypothetical&&!observer.observation().confirms_execution);
    observer.timeout();auto timed=observer.timeout_observation();assert(timed&&timed->kind==ObservationKind::timeout);
    assert(observer.receive(response(0xa9080400,bytes)));assert(observer.observation().kind==ObservationKind::late_response);
    assert(observer.observation().confirms_execution&&!observer.observation().success&&!observer.observation().unknown_remote_outcome);
    assert(observer.timeout_observation()==timed);auto count=observer.observations().size();assert(observer.receive(response(0xa9080400,bytes)));assert(observer.observations().size()==count);
    ControllerObserver fresh(42);assert(fresh.receive(response(0xa9080400,bytes)));assert(fresh.observation().success&&fresh.observation().confirms_execution);
    assert(fresh.receive(response(0xa9080800,bytes)));assert(fresh.observation().contradictory&&!fresh.observation().success);
    ControllerObserver ordinary(42);auto cancellation=ordinary.receive(response(0xa9080400,bytes,true));
    assert(cancellation&&ordinary.observation().cancellation&&ordinary.observation().confirms_cancellation&&!ordinary.observation().confirms_execution);
    ControllerObserver mismatch(43);assert(!mismatch.receive(response(0xa9080400,bytes)));
    AckRecord ack;ack.request.type=PacketType::command;ack.request.stream_id=1;ack.request.command=Command{0xa9080000,42,Identifier::short_id(2),Identifier::short_id(3)};
    ack.cam=*Cam::parse(ack.request,Profile::generic_virtual_test);ack.kind=AckKind::execution;ack.scheduled_or_executed=true;ack.time_known=true;ack.time={100,123};
    std::array<std::byte,256> out{};assert(!encode_response(ack,out));
    ack.request.packet_count=5;ack.epoch=Tsi::gps;auto n=encode_response(ack,out,0);assert(n);auto parsed=decode_envelope(Bytes{out}.first(*n));assert(parsed&&parsed->envelope.timestamp.tsi==Tsi::gps&&parsed->envelope.timestamp.tsf==Tsf::picoseconds&&parsed->envelope.packet_count==0&&ack.request.packet_count==5);
    assert(!encode_response(ack,out,16));
    ack.request.timestamp={Tsi::utc,Tsf::sample_count,1,2};ack.epoch=Tsi::other;n=encode_response(ack,out);assert(n);parsed=decode_envelope(Bytes{out}.first(*n));assert(parsed&&parsed->envelope.timestamp.tsi==Tsi::other&&parsed->envelope.timestamp.fractional==123);
    ack.time_known=false;ack.timing=7;ack.scheduled_or_executed=false;ack.partial=true;
    n=encode_response(ack,out);assert(n);parsed=decode_envelope(Bytes{out}.first(*n));
    assert(parsed&&parsed->envelope.timestamp.tsi==Tsi::none&&((parsed->envelope.command->cam>>12)&7)==7);
    ack.request.timestamp={};n=encode_response(ack,out);assert(n);parsed=decode_envelope(Bytes{out}.first(*n));assert(parsed&&((parsed->envelope.command->cam>>12)&7)==0);
    ack.time_known=true;ack.time.picoseconds=1000000000000ULL;assert(!encode_response(ack,out));ack.time={UINT64_MAX,0};assert(!encode_response(ack,out));
}
