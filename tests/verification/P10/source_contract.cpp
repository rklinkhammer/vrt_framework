#include <vita/profiles/iq/source.hpp>
#include "canonical_oracle.hpp"
#include <limits>
#include <algorithm>
#include <cfenv>
using namespace vita;using namespace vita::profiles::iq;
static std::uint32_t load(Bytes bytes,std::size_t offset,unsigned width){std::uint32_t result=0;for(unsigned i=0;i<width;++i)result=(result<<8)|std::uint32_t(bytes[offset+i]);return result;}
int main(){runtime::StateSnapshot config;std::array<std::byte,2048> storage;
 for(auto format:{SampleFormat::iq16,SampleFormat::iq32,SampleFormat::float32})for(auto ordinal:{std::uint64_t{0},std::uint64_t{17},UINT64_MAX-15}){
  const unsigned width=format==SampleFormat::iq16?2:4;auto bytes=MutableBytes{storage}.first(32*width);auto window=SampleWriteWindow::create(bytes,format,ordinal,16,config);if(!window||!default_source().produce(*window)||!window->validate_complete())return 1;
  for(unsigned i=0;i<32;++i){const auto index=((ordinal%16)*2+i)%32;const auto expected=format==SampleFormat::iq16?verify_p10::iq16[index]:format==SampleFormat::iq32?verify_p10::iq32[index]:verify_p10::float32[index];if(load(bytes,i*width,width)!=expected)return 2;}
 }
 for(auto format:{SampleFormat::iq16,SampleFormat::iq32}){
  const unsigned width=format==SampleFormat::iq16?2:4;const double scale=format==SampleFormat::iq16?32768.0:2147483648.0;auto bytes=MutableBytes{storage}.first(8*2*width);auto window=SampleWriteWindow::create(bytes,format,0,8,config);if(!window)return 3;
  const std::array values{0.5/scale,1.5/scale,2.5/scale,-0.5/scale,-1.5/scale,-2.5/scale,2.0,-2.0};
  const std::array<std::int64_t,8> expected{0,2,2,0,-2,-2,format==SampleFormat::iq16?32767:2147483647,format==SampleFormat::iq16?-32768:-2147483648LL};
  for(unsigned i=0;i<8;++i)if(!window->write(i,values[i],values[i]))return 4;
  if(!window->validate_complete())return 5;
  for(unsigned i=0;i<8;++i){const auto mask=width==2?0xffffULL:0xffffffffULL;for(unsigned q=0;q<2;++q)if(load(bytes,(i*2+q)*width,width)!=(std::uint64_t(expected[i])&mask))return 6;}
 }
 for(auto format:{SampleFormat::iq16,SampleFormat::iq32,SampleFormat::float32}){
  storage.fill(std::byte{0x5a});const auto width=bytes_per_pair(format);auto window=SampleWriteWindow::create(MutableBytes{storage}.first(width),format,0,1,config);if(!window)return 7;
  for(double value:{std::numeric_limits<double>::infinity(),-std::numeric_limits<double>::infinity(),std::numeric_limits<double>::quiet_NaN()})if(window->write(0,0.5,value)||window->write(0,value,0.5))return 8;
  for(auto byte:storage)if(byte!=std::byte{0x5a})return 9;
  if(window->validate_complete()||window->write(1,0,0))return 10;
 }
 {
  auto window=SampleWriteWindow::create(MutableBytes{storage}.first(8),SampleFormat::float32,0,1,config);storage.fill(std::byte{0x5a});if(!window||window->write(0,0,std::numeric_limits<double>::max()))return 11;
  for(auto byte:storage)if(byte!=std::byte{0x5a})return 12;
  if(!window->write(0,-0.0,std::numeric_limits<float>::denorm_min())||!window->validate_complete()||load(storage,0,4)!=0x80000000||load(storage,4,4)!=1)return 13;
 }
 if(SampleWriteWindow::create({},SampleFormat::iq16,0,0,config)||SampleWriteWindow::create(storage,SampleFormat::iq16,0,257,config)||SampleWriteWindow::create(MutableBytes{storage}.first(8),SampleFormat::iq16,UINT64_MAX,2,config)||SampleWriteWindow::create(storage,static_cast<SampleFormat>(3),0,1,config))return 14;
 auto complete=SampleWriteWindow::create(storage,SampleFormat::iq32,0,256,config);if(!complete||!default_source().produce(*complete)||!complete->validate_complete())return 15;
 const auto original_rounding=std::fegetround();for(int rounding:{FE_UPWARD,FE_DOWNWARD,FE_TOWARDZERO}){if(std::fesetround(rounding))return 16;auto window=SampleWriteWindow::create(MutableBytes{storage}.first(128),SampleFormat::float32,0,16,config);if(!window||!default_source().produce(*window))return 17;for(unsigned i=0;i<32;++i)if(load(storage,i*4,4)!=verify_p10::float32[i])return 18;}if(std::fesetround(original_rounding))return 19;
 return 0;
}
