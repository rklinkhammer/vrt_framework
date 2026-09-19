#include <vita/adapters/posix_udp/socket.hpp>
#include "peer.hpp"
#include <cerrno>
using namespace vita;using namespace vita::adapters::posix_udp;
int main(){for(bool v6:{false}){verify_p12::Peer peer(v6);SocketConfig config;config.bind=Address::loopback(v6?Family::ipv6:Family::ipv4);config.ip_mtu=65535;auto socket=Socket::open(config);if(!socket)return 1;
 std::array<std::byte,65532> oversized{};std::array<Bytes,1> parts{oversized};auto sent=socket->send(Address::loopback(config.bind.family,peer.port()),parts);
 if(sent||sent.error().code!=ErrorCode::short_output||sent.error().native_error!=EMSGSIZE||sent.error().offset!=0||sent.error().retryable||sent.error().stage!=ErrorStage::transport)return 2;
 std::array<std::byte,64> bytes{};if(peer.receive(bytes,0)>=0)return 3;
 socket->close();if(socket->send(Address::loopback(config.bind.family,peer.port()),parts)||socket->receive(bytes))return 4;
 }return 0;}
