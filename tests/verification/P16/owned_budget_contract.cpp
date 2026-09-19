#include <vita/profiles/iq/lab.hpp>
#include <vita/runtime/public/runtime.hpp>
#include <cassert>
using namespace vita;using Runtime=VitaRuntime<2,4,32,65536>;
using Backend=runtime::transaction::VirtualBackend<4>;
struct Holder{std::array<Backend,2> backend;};
static std::unique_ptr<Runtime> create(){auto c=profiles::iq::lab::config(0xabcdef);auto p=profiles::iq::lab::pools();assert(c&&p);auto r=Runtime::create(*c,std::move(*p));assert(r);return std::move(*r);}
static StreamConfig config(unsigned sid){StreamConfig c;c.sid=sid;c.controller_id=2;c.controllee_id=3;c.sample_rate=100000;c.profile=profiles::iq::Profile::frequency_tunable;return c;}
int main(){auto plain=create();assert(plain->add_controllee(config(1))&&plain->add_controllee(config(2)));auto owned=create();auto owner=std::make_shared<Holder>();auto first=config(1);first.device={owner->backend[0].binding(),owner,sizeof(Holder),nullptr};assert(owned->add_controllee(first));auto second=config(2);second.device={owner->backend[1].binding(),owner,sizeof(Holder)+1,nullptr};const auto before=owned->budget().charged_bytes();assert(!owned->add_controllee(second)&&owned->budget().charged_bytes()==before);second.device.storage_bytes=sizeof(Holder);assert(owned->add_controllee(second));assert(owned->budget().charged_bytes()==plain->budget().charged_bytes()+sizeof(Holder)+128);assert(owned->budget().reserved_bytes()==runtime::framework_budget);}
