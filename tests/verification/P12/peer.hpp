#pragma once
#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <utility>
namespace verify_p12 {
// Independent native peer: never uses the adapter's endpoint, parser, encoder,
// transport counters or ownership paths to construct expected wire observations.
class Peer {
 int fd_=-1; sockaddr_storage address_{};socklen_t length_=0;
public:
 explicit Peer(bool ipv6=false) noexcept {
  fd_=::socket(ipv6?AF_INET6:AF_INET,SOCK_DGRAM,0);if(fd_<0)return;
  if(ipv6){auto* a=reinterpret_cast<sockaddr_in6*>(&address_);a->sin6_family=AF_INET6;a->sin6_addr=in6addr_loopback;length_=sizeof(*a);}
  else{auto* a=reinterpret_cast<sockaddr_in*>(&address_);a->sin_family=AF_INET;a->sin_addr.s_addr=htonl(INADDR_LOOPBACK);length_=sizeof(*a);}
  if(::bind(fd_,reinterpret_cast<sockaddr*>(&address_),length_)<0||::getsockname(fd_,reinterpret_cast<sockaddr*>(&address_),&length_)<0){::close(fd_);fd_=-1;}
 }
 Peer(const Peer&)=delete;Peer& operator=(const Peer&)=delete;
 Peer(Peer&& p)noexcept:fd_(std::exchange(p.fd_,-1)),address_(p.address_),length_(p.length_){}
 ~Peer(){if(fd_>=0)::close(fd_);}
 bool valid()const noexcept{return fd_>=0;}
 int fd()const noexcept{return fd_;}
 std::uint16_t port()const noexcept{return address_.ss_family==AF_INET6?ntohs(reinterpret_cast<const sockaddr_in6*>(&address_)->sin6_port):ntohs(reinterpret_cast<const sockaddr_in*>(&address_)->sin_port);}
 const sockaddr* address()const noexcept{return reinterpret_cast<const sockaddr*>(&address_);}
 socklen_t address_length()const noexcept{return length_;}
 bool send(std::span<const std::byte> bytes,const Peer& target)const noexcept {return ::sendto(fd_,bytes.data(),bytes.size(),0,target.address(),target.address_length())==static_cast<ssize_t>(bytes.size());}
 bool send_to(std::span<const std::byte> bytes,bool ipv6,std::uint16_t port)const noexcept {
  sockaddr_storage destination{};socklen_t length;
  if(ipv6){auto* a=reinterpret_cast<sockaddr_in6*>(&destination);a->sin6_family=AF_INET6;a->sin6_addr=in6addr_loopback;a->sin6_port=htons(port);length=sizeof(*a);}
  else{auto* a=reinterpret_cast<sockaddr_in*>(&destination);a->sin_family=AF_INET;a->sin_addr.s_addr=htonl(INADDR_LOOPBACK);a->sin_port=htons(port);length=sizeof(*a);}
  return ::sendto(fd_,bytes.data(),bytes.size(),0,reinterpret_cast<sockaddr*>(&destination),length)==static_cast<ssize_t>(bytes.size());
 }
 ssize_t receive(std::span<std::byte> out,int timeout_ms=20,int* message_flags=nullptr)const noexcept {
  pollfd ready{fd_,POLLIN,0};if(::poll(&ready,1,timeout_ms)<=0)return -1;
  iovec iov{out.data(),out.size()};msghdr message{};message.msg_iov=&iov;message.msg_iovlen=1;
  const auto received=::recvmsg(fd_,&message,0);if(message_flags)*message_flags=message.msg_flags;return received;
 }
};
// Signal packet, SID 0x01020304, no class/timestamp/trailer, two complete IQ16
// pairs. These literal bytes are independent of the production encoder.
inline constexpr std::array<std::byte,16> signal{
 std::byte{0x10},std::byte{0x00},std::byte{0x00},std::byte{0x04},
 std::byte{0x01},std::byte{0x02},std::byte{0x03},std::byte{0x04},
 std::byte{0x40},std::byte{0x00},std::byte{0x00},std::byte{0x00},
 std::byte{0x3b},std::byte{0x21},std::byte{0x18},std::byte{0x7e}};
} // namespace verify_p12
