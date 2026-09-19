#pragma once
#include "replay_wire.hpp"
#include <string>
#include <cmath>
#include <cstdlib>
namespace vita::bench::receiver {
struct Options {std::string bind_ip="127.0.0.1",peer_ip="127.0.0.1",output="artifacts/P13/receiver";std::array<std::uint16_t,3> ports{},peer_ports{};std::size_t streams=4;std::uint64_t rate=1000000;double warmup=1,duration=10;};
inline bool parse(int argc,char** argv,Options& out,bool receive){
  const std::string prefix=receive?"--sender-":"--receiver-";
  for(int i=1;i<argc;++i){std::string key=argv[i];if(i+1==argc)return false;std::string value=argv[++i];if(key=="--bind-ip")out.bind_ip=value;else if(key==prefix+"ip")out.peer_ip=value;else if(key=="--output-dir")out.output=value;else{
    char* end=nullptr;double number=std::strtod(value.c_str(),&end);if(!end||*end||end==value.c_str()||!std::isfinite(number)||number<0)return false;
    if(key=="--duration-seconds")out.duration=number;else if(key=="--warmup-seconds")out.warmup=number;else if(key=="--sample-rate"){if(std::floor(number)!=number||number>100000000)return false;out.rate=static_cast<std::uint64_t>(number);}else if(key=="--streams"){if(std::floor(number)!=number||number>4)return false;out.streams=static_cast<std::size_t>(number);}else{
      if(number>65535||std::floor(number)!=number)return false;bool found=false;const std::array<std::string,3> lanes{"data-port","control-port","cancel-port"};for(unsigned lane=0;lane<3;++lane){if(key=="--"+lanes[lane]){out.ports[lane]=number;found=true;}if(key==prefix+lanes[lane]){out.peer_ports[lane]=number;found=true;}}if(!found)return false;
    }
  }}
  if(!out.streams||!out.rate||out.duration<=0||out.duration>86400||out.warmup>3600)return false;
  for(unsigned i=0;i<3;++i)if(!out.peer_ports[i]||(!receive&&!out.ports[i])||!address(out.bind_ip,out.ports[i])||!address(out.peer_ip,out.peer_ports[i]))return false;
  return true;
}
inline std::uint64_t clock_ns(clockid_t clock) noexcept{timespec value{};clock_gettime(clock,&value);return std::uint64_t(value.tv_sec)*1000000000ULL+value.tv_nsec;}
}
