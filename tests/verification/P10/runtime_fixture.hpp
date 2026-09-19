#pragma once
#include <vita/runtime/public/runtime.hpp>
#include <cstdlib>
#include <new>
namespace verify_p10 {
inline vita::memory::ExternalPool external_pool(std::size_t bytes,std::size_t count){
    void* raw=::operator new(bytes*count,std::align_val_t{64});std::shared_ptr<void> owner(raw,[](void*p){::operator delete(p,std::align_val_t{64});});
    vita::memory::BufferSpec spec{owner,static_cast<std::byte*>(raw),bytes,count,64,vita::memory::MemoryDomain::cpu};auto result=vita::memory::ExternalPool::create(std::span{&spec,1});if(!result)std::abort();return std::move(*result);
}
inline vita::ExternalPools external_pools(){return{external_pool(64,64),external_pool(2048,64),external_pool(64,64),external_pool(8192,32),external_pool(8192,8),external_pool(2048,64),external_pool(8192,32),external_pool(8192,8),external_pool(8192,8)};}
inline vita::RuntimeConfig runtime_config(){vita::RuntimeConfig c;c.oui=0x00a1b2;c.isolated_lab=true;c.clock={vita::runtime::timing::Epoch::gps,true,false,true,0,0,2000000000};c.timing=vita::runtime::timing::TimingCapabilities::deterministic();return c;}
inline vita::StreamConfig stream_config(){vita::StreamConfig c;c.sid=1;c.controller_id=2;c.controllee_id=3;return c;}
}
