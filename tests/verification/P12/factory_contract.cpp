#include "../P10/runtime_fixture.hpp"
using namespace vita;using namespace vita::runtime;using namespace vita::runtime::transport;using namespace verify_p10;
struct Factory {
 unsigned calls=0,destroyed=0,preflight=0,committed=0,detached=0;unsigned mode=0;std::shared_ptr<void> retained;
 static Result<TransportBinding> create(void*p,HostBindings)noexcept {
  auto& f=*static_cast<Factory*>(p);++f.calls;
  auto owner=std::shared_ptr<void>(new unsigned{0},[&f](void*p){delete static_cast<unsigned*>(p);++f.destroyed;});
  TransportBinding result{owner,&f,1024,8,{},
   [](void*,TxSubmission&& v)noexcept->std::expected<TxToken,RejectedSubmission>{return std::unexpected(RejectedSubmission{{ErrorCode::capacity_exhausted},std::move(v)});},
   [](void*)noexcept->Result<bool>{return false;},
   [](void*,TxToken)noexcept{return false;},[](void*)noexcept{},nullptr,
   [](void*p,std::span<const Association>,bool commit)noexcept->Result<void>{auto& f=*static_cast<Factory*>(p);if(commit)++f.committed;else ++f.preflight;return {};},[](void*p)noexcept{++static_cast<Factory*>(p)->detached;}};
  if(f.mode==1)result.metadata_bytes=1025;if(f.mode==2)result.owner.reset();if(f.mode==3)result.associate=nullptr;if(f.mode==4)result.capabilities.completion_is_delivery=true;if(f.mode==5)result.slot_capacity=9;
  if(f.mode==6||f.mode==7)f.retained=owner;if(f.mode==7)result.metadata_bytes=1025;return result;
 }
 TransportFactory binding(){return{this,1024,8,{},create};}
};
int main(){using Runtime=VitaRuntime<1,4,32,65536>;
 {
  Factory factory;auto config=runtime_config();config.transport=factory.binding();config.transport.required_bytes=SIZE_MAX;auto made=Runtime::create(config,external_pools());if(made||factory.calls)return 1;
 }
 for(unsigned mode=1;mode<=5;++mode){Factory factory;factory.mode=mode;auto config=runtime_config();config.transport=factory.binding();auto made=Runtime::create(config,external_pools());if(made||factory.calls!=1||factory.destroyed!=1)return 2;}
 {
  Factory factory;auto config=runtime_config();config.transport=factory.binding();auto made=Runtime::create(config,external_pools());if(!made||factory.calls!=1||factory.destroyed)return 3;
  auto device=(*made)->add_controllee(stream_config());if(!device||factory.preflight!=1||factory.committed!=1)return 4;
  made->reset();if(factory.destroyed!=1)return 5;
 }
 {
  Factory factory;factory.mode=6;auto config=runtime_config();config.transport=factory.binding();auto made=Runtime::create(config,external_pools());if(!made)return 6;made->reset();if(factory.detached!=1||factory.destroyed)return 7;factory.retained.reset();if(factory.destroyed!=1)return 8;
 }
 {
  Factory factory;factory.mode=7;auto config=runtime_config();config.transport=factory.binding();auto made=Runtime::create(config,external_pools());if(made||factory.detached!=1||factory.destroyed)return 9;factory.retained.reset();if(factory.destroyed!=1)return 10;
 }
 return 0;
}
