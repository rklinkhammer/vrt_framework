#include <vita/runtime/public/config.hpp>
using namespace vita;using namespace vita::profiles::iq;
int main(){
 struct Case{SampleFormat format;std::uint64_t rate;std::size_t mtu;bool ipv6,trailer;std::size_t expected;};
 constexpr Case cases[]={{SampleFormat::iq16,1000000,1500,false,false,256},{SampleFormat::iq32,1000000,1500,false,false,180},{SampleFormat::float32,1000000,1500,true,false,178},{SampleFormat::iq32,1000000,1500,false,true,180},{SampleFormat::iq32,1000000,1500,true,true,177},{SampleFormat::iq16,1000000,100,false,false,11},{SampleFormat::iq16,1000000,100,false,true,10},{SampleFormat::iq16,1,1500,false,false,1},{SampleFormat::iq16,7,1500,false,false,7},{SampleFormat::float32,179,1500,true,false,178}};
 for(auto c:cases){StreamConfig config;config.format=c.format;config.sample_rate=c.rate;config.ip_mtu=c.mtu;config.ipv6=c.ipv6;config.trailer=c.trailer;auto n=packet_samples(config);if(!n||*n!=c.expected)return 1;const auto pair=c.format==SampleFormat::iq16?4u:8u;const auto datagram=28+*n*pair+(c.trailer?4:0);if(datagram+(c.ipv6?48:28)>c.mtu)return 2;}
 StreamConfig invalid;invalid.sample_rate=0;if(packet_samples(invalid))return 3;invalid.sample_rate=100000001;if(packet_samples(invalid))return 4;invalid.sample_rate=1;invalid.ipv6=true;invalid.ip_mtu=76;if(packet_samples(invalid))return 5;invalid.ip_mtu=83;invalid.format=SampleFormat::iq32;if(packet_samples(invalid))return 6;invalid.ip_mtu=84;if(!packet_samples(invalid)||*packet_samples(invalid)!=1)return 7;
 return 0;}
