#include <vita/runtime/transaction/outcomes.hpp>
using namespace vita;using namespace vita::codec;using namespace vita::runtime::transaction;
static auto packet(std::uint32_t cam){
    std::array<std::byte,24> out{};std::array<std::uint32_t,6> words{0x64000006,1,cam,42,2,3};
    for(unsigned i=0;i<words.size();++i)for(unsigned b=0;b<4;++b)out[i*4+b]=std::byte((words[i]>>(24-b*8))&255);
    return out;
}
int main(){
    ControllerObserver observer(42);if(observer.observation().success||!observer.observation().unknown_remote_outcome)return 1;
    observer.local_send(true);if(observer.observation().success||!observer.observation().unknown_remote_outcome)return 2;
    auto vbytes=packet(0xa9100400);auto v=decode_packet(vbytes);if(!v||!observer.receive(*v))return 3;
    if(observer.observation().kind!=ObservationKind::validation||observer.observation().success||!observer.observation().unknown_remote_outcome)return 4;
    ControllerObserver simulator(42);auto simulation=packet(0xa8880400);auto dry=decode_packet(simulation);if(!dry||!simulator.receive(*dry))return 5;
    if(!simulator.observation().hypothetical||simulator.observation().success||!simulator.observation().unknown_remote_outcome)return 6;
    ControllerObserver partial_observer(42);auto partial=packet(0xa9080800);auto p=decode_packet(partial);if(!p||!partial_observer.receive(*p)||partial_observer.observation().success||!partial_observer.observation().partial)return 7;
    observer.timeout();auto preserved=observer.timeout_observation();if(!preserved||!observer.timed_out())return 12; if(observer.observation().kind!=ObservationKind::timeout||observer.observation().success||!observer.observation().unknown_remote_outcome)return 8;
    auto completed=packet(0xa9080400);auto x=decode_packet(completed);if(!x||!observer.receive(*x)||observer.observation().kind!=ObservationKind::late_response)return 9;
    if(observer.observation().success||!observer.observation().confirms_execution||observer.timeout_observation()!=preserved)return 13;
    ControllerObserver fresh(42);if(!fresh.receive(*x)||!fresh.observation().success)return 10;
    ControllerObserver mismatch(43);if(mismatch.receive(*x))return 11;
    return 0;
}
