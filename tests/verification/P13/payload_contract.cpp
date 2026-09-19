#include "../../../bench/payload_source.hpp"
#include "../P10/canonical_oracle.hpp"
#include "../P10/runtime_fixture.hpp"
#include <sys/mman.h>
#include <unistd.h>
#include <vector>
using namespace vita;using namespace vita::bench;using namespace vita::profiles::iq;
static bool canonical(Bytes bytes){if(bytes.size()!=1024)return false;for(unsigned n=0;n<512;++n){const auto got=(unsigned(bytes[2*n])<<8)|unsigned(bytes[2*n+1]);if(got!=verify_p10::iq16[n%32])return false;}return true;}
int main(){runtime::StateSnapshot state;auto dummy=verify_p10::external_pool(2048,4);std::array<std::byte,2048> bytes;{PayloadSource invalid;if(invalid.initialize(static_cast<PayloadMode>(99),dummy))return 19;}
 for(auto mode:{PayloadMode::generated,PayloadMode::precomputed_copy}){PayloadSource source;if(!source.initialize(mode,dummy)||!canonical(source.canonical())||source.initialize(mode,dummy))return 1;for(unsigned n=0;n<5;++n){bytes.fill(std::byte{0x5a});auto window=SampleWriteWindow::create(MutableBytes{bytes}.first(1024),SampleFormat::iq16,n*256,256,state);if(!window||!source.provider().produce(*window)||!window->validate_complete()||!canonical(window->wire()))return 2;for(unsigned k=1024;k<2048;++k)if(bytes[k]!=std::byte{0x5a})return 3;}if(source.calls()!=5||source.payload_write_bytes()!=5120||source.payload_copy_calls()!=(mode==PayloadMode::precomputed_copy?5u:0u))return 4;for(auto spec:std::array<std::array<unsigned,3>,3>{{{{0,1,256}},{{0,0,255}},{{1,0,256}}}}){auto format=static_cast<SampleFormat>(spec[0]);auto window=SampleWriteWindow::create(MutableBytes{bytes}.first(spec[2]*bytes_per_pair(format)),format,spec[1],spec[2],state);if(!window||source.provider().produce(*window)||window->validate_complete())return 5;}}
 const auto page=static_cast<std::size_t>(::sysconf(_SC_PAGESIZE));const auto extent=page*4;auto* raw=static_cast<std::byte*>(::mmap(nullptr,extent,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANON,-1,0));if(raw==MAP_FAILED)return 6;std::shared_ptr<void> owner(raw,[extent](void*p){::munmap(p,extent);});memory::BufferSpec spec{owner,raw,page,4,page,memory::MemoryDomain::cpu};auto made=memory::ExternalPool::create(std::span{&spec,1});if(!made)return 7;auto pool=std::move(*made);PayloadSource source;
 auto busy=pool.acquire({1024});if(!busy||source.initialize(PayloadMode::prefilled_pool,pool)||source.setup_blocks()!=0)return 8;busy={};if(!source.initialize(PayloadMode::prefilled_pool,pool)||source.setup_blocks()!=4)return 9;
 // Verify all four distinct physical blocks before protecting their pages.
 for(unsigned i=0;i<4;++i)if(!canonical(Bytes{raw+i*page,1024}))return 10;
 if(::mprotect(raw,extent,PROT_READ))return 11;
 for(unsigned cycle=0;cycle<10;++cycle){std::vector<memory::BufferLease> leases;for(unsigned i=0;i<4;++i){auto lease=pool.acquire({1024});if(!lease)return 12;leases.push_back(std::move(*lease));auto wire=leases.back().writable_bytes();if(!wire)return 13;auto window=SampleWriteWindow::create(wire->first(1024),SampleFormat::iq16,cycle*1024+i*256,256,state);if(!window||!source.provider().produce(*window)||!window->validate_complete()||!canonical(window->wire()))return 14;}}
 if(source.calls()!=40||source.payload_write_bytes()||source.payload_copy_calls())return 15;
 auto foreign=SampleWriteWindow::create(MutableBytes{bytes}.first(1024),SampleFormat::iq16,0,256,state);if(!foreign||source.provider().produce(*foreign)||foreign->validate_complete())return 16;
 // Checked completion must reject non-finite float wire values without falsely
 // setting the coverage bitmap, while valid externally written bytes complete.
 std::array<std::byte,8> floating{};floating[0]=std::byte{0x7f};floating[1]=std::byte{0x80};auto f=SampleWriteWindow::create(floating,SampleFormat::float32,0,1,state);if(!f||f->complete_from_wire()||f->validate_complete())return 17;floating[0]=floating[1]=std::byte{};if(!f->complete_from_wire()||!f->validate_complete())return 18;
 return 0;
}
