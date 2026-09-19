#if defined(__APPLE__)
#define __APPLE_USE_RFC_3542 1
#endif
#include <arpa/inet.h>
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/uio.h>
#include <unistd.h>
#include <utility>
#include <vita/adapters/posix_udp/socket.hpp>
namespace vita::adapters::posix_udp {
namespace {
Error os_error(int code) noexcept {
  Error error{code == EMSGSIZE ? ErrorCode::short_output
                               : ErrorCode::callback_failure};
  error.native_error = code;
  error.stage = ErrorStage::transport;
  error.retryable = code == EAGAIN || code == EWOULDBLOCK || code == EINTR;
  return error;
}
sockaddr_storage native(const Address &address, socklen_t &length) noexcept {
  sockaddr_storage result{};
  if (address.family == Family::ipv4) {
    sockaddr_in in{};
    in.sin_family = AF_INET;
    in.sin_port = htons(address.port);
    std::memcpy(&in.sin_addr, address.bytes.data(), 4);
    length = sizeof(in);
    std::memcpy(&result, &in, sizeof(in));
  } else {
    sockaddr_in6 in{};
    in.sin6_family = AF_INET6;
    in.sin6_port = htons(address.port);
    in.sin6_scope_id = address.scope;
    std::memcpy(&in.sin6_addr, address.bytes.data(), 16);
    length = sizeof(in);
    std::memcpy(&result, &in, sizeof(in));
  }
  return result;
}
Result<Address> address_of(const sockaddr_storage &value,
                           socklen_t length) noexcept {
  Address result;
  if (value.ss_family == AF_INET && length >= sizeof(sockaddr_in)) {
    sockaddr_in in;
    std::memcpy(&in, &value, sizeof(in));
    result.family = Family::ipv4;
    result.port = ntohs(in.sin_port);
    std::memcpy(result.bytes.data(), &in.sin_addr, 4);
  } else if (value.ss_family == AF_INET6 && length >= sizeof(sockaddr_in6)) {
    sockaddr_in6 in;
    std::memcpy(&in, &value, sizeof(in));
    result.family = Family::ipv6;
    result.port = ntohs(in.sin6_port);
    result.scope = in.sin6_scope_id;
    std::memcpy(result.bytes.data(), &in.sin6_addr, 16);
  } else
    return std::unexpected(Error{ErrorCode::invalid_argument});
  return result;
}
} // namespace
Address Address::loopback(Family family, std::uint16_t port) noexcept {
  Address result;
  result.family = family;
  result.port = port;
  if (family == Family::ipv4) {
    result.bytes[0] = std::byte{127};
    result.bytes[3] = std::byte{1};
  } else
    result.bytes[15] = std::byte{1};
  return result;
}
Socket::Socket(Socket &&other) noexcept
    : fd_(std::exchange(other.fd_, -1)), local_(other.local_),
      stats_(other.stats_) {}
Socket &Socket::operator=(Socket &&other) noexcept {
  if (this != &other) {
    close();
    fd_ = std::exchange(other.fd_, -1);
    local_ = other.local_;
    stats_ = other.stats_;
  }
  return *this;
}
Socket::~Socket() { close(); }
void Socket::close() noexcept {
  if (fd_ >= 0) {
    ::close(fd_);
    fd_ = -1;
  }
}
Result<Socket> Socket::open(const SocketConfig &config) noexcept {
  if (!config.bind.canonical() ||
      config.ip_mtu < (config.bind.family == Family::ipv4 ? 68u : 1280u) ||
      config.ip_mtu > 65535 || config.send_buffer_bytes <= 0 ||
      config.receive_buffer_bytes <= 0)
    return std::unexpected(Error{ErrorCode::invalid_argument});
  const int family = config.bind.family == Family::ipv4 ? AF_INET : AF_INET6;
  int descriptor = ::socket(family, SOCK_DGRAM, 0);
  if (descriptor < 0)
    return std::unexpected(os_error(errno));
  Socket result(descriptor, {});
  const auto flags = fcntl(descriptor, F_GETFL, 0);
  const auto descriptor_flags = fcntl(descriptor, F_GETFD, 0);
  if (flags < 0 || descriptor_flags < 0 ||
      fcntl(descriptor, F_SETFL, flags | O_NONBLOCK) < 0 ||
      fcntl(descriptor, F_SETFD, descriptor_flags | FD_CLOEXEC) < 0)
    return std::unexpected(os_error(errno));
  int one = 1;
  if (family == AF_INET6 &&
      setsockopt(descriptor, IPPROTO_IPV6, IPV6_V6ONLY, &one, sizeof(one)) < 0)
    return std::unexpected(os_error(errno));
#if defined(__linux__)
  int policy = IP_PMTUDISC_DO;
  const int option = family == AF_INET ? IP_MTU_DISCOVER : IPV6_MTU_DISCOVER;
  const int level = family == AF_INET ? IPPROTO_IP : IPPROTO_IPV6;
  if (setsockopt(descriptor, level, option, &policy, sizeof(policy)) < 0)
    return std::unexpected(os_error(errno));
#elif defined(IP_DONTFRAG) && defined(IPV6_DONTFRAG)
  if (setsockopt(descriptor, family == AF_INET ? IPPROTO_IP : IPPROTO_IPV6,
                 family == AF_INET ? IP_DONTFRAG : IPV6_DONTFRAG, &one,
                 sizeof(one)) < 0)
    return std::unexpected(os_error(errno));
#else
  return std::unexpected(Error{ErrorCode::unsupported_capability});
#endif
  if (setsockopt(descriptor, SOL_SOCKET, SO_SNDBUF, &config.send_buffer_bytes,
                 sizeof(int)) < 0 ||
      setsockopt(descriptor, SOL_SOCKET, SO_RCVBUF,
                 &config.receive_buffer_bytes, sizeof(int)) < 0)
    return std::unexpected(os_error(errno));
  socklen_t option_length = sizeof(int);
  if (getsockopt(descriptor, SOL_SOCKET, SO_SNDBUF,
                 &result.stats_.send_buffer_bytes, &option_length) < 0)
    return std::unexpected(os_error(errno));
  option_length = sizeof(int);
  if (getsockopt(descriptor, SOL_SOCKET, SO_RCVBUF,
                 &result.stats_.receive_buffer_bytes, &option_length) < 0)
    return std::unexpected(os_error(errno));
  result.stats_.no_fragment = true;
  result.stats_.nonblocking = true;
  socklen_t length;
  auto address = native(config.bind, length);
  if (::bind(descriptor, reinterpret_cast<const sockaddr *>(&address), length) <
      0)
    return std::unexpected(os_error(errno));
  sockaddr_storage bound{};
  length = sizeof(bound);
  if (getsockname(descriptor, reinterpret_cast<sockaddr *>(&bound), &length) <
      0)
    return std::unexpected(os_error(errno));
  auto local = address_of(bound, length);
  if (!local)
    return std::unexpected(local.error());
  result.local_ = *local;
  return result;
}
Result<std::size_t> Socket::send(const Address &destination,
                                 std::span<const Bytes> segments) noexcept {
  if (fd_ < 0 || !destination.canonical() ||
      destination.family != local_.family || !destination.port ||
      segments.empty() || segments.size() > 3)
    return std::unexpected(Error{ErrorCode::invalid_argument});
  std::array<iovec, 3> vectors{};
  std::size_t total = 0;
  for (std::size_t i = 0; i < segments.size(); ++i) {
    if (segments[i].size() > 65535 - total)
      return std::unexpected(Error{ErrorCode::short_output});
    total += segments[i].size();
    vectors[i] = {const_cast<std::byte *>(segments[i].data()),
                  segments[i].size()};
  }
  socklen_t length;
  auto address = native(destination, length);
  msghdr message{};
  message.msg_name = &address;
  message.msg_namelen = length;
  message.msg_iov = vectors.data();
  message.msg_iovlen = segments.size();
  const auto sent = ::sendmsg(fd_, &message, 0);
  if (sent < 0)
    return std::unexpected(os_error(errno));
  if (static_cast<std::size_t>(sent) != total)
    return std::unexpected(
        Error{ErrorCode::callback_failure, static_cast<std::size_t>(sent)});
  return static_cast<std::size_t>(sent);
}
Result<Received> Socket::receive(MutableBytes output, bool peek) noexcept {
  if (fd_ < 0 || output.empty())
    return std::unexpected(Error{ErrorCode::invalid_argument});
  sockaddr_storage source{};
  iovec vector{output.data(), output.size()};
  msghdr message{};
  message.msg_name = &source;
  message.msg_namelen = sizeof(source);
  message.msg_iov = &vector;
  message.msg_iovlen = 1;
  const auto count = ::recvmsg(fd_, &message, peek ? MSG_PEEK : 0);
  if (count < 0)
    return std::unexpected(os_error(errno));
  auto address = address_of(source, message.msg_namelen);
  if (!address)
    return std::unexpected(address.error());
  return Received{*address,
                  std::min(static_cast<std::size_t>(count), output.size()),
                  bool(message.msg_flags & MSG_TRUNC)};
}
} // namespace vita::adapters::posix_udp
