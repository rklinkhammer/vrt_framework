#pragma once
#include <array>
#include <span>
#include <vita/core/bytes.hpp>
#include <vita/core/error.hpp>
namespace vita::adapters::posix_udp {
enum class Family { ipv4, ipv6 };
struct Address {
  Family family = Family::ipv4;
  std::array<std::byte, 16> bytes{};
  std::uint16_t port = 0;
  std::uint32_t scope = 0;
  bool canonical() const noexcept {
    if (family != Family::ipv4 && family != Family::ipv6)
      return false;
    if (family == Family::ipv4) {
      if (scope)
        return false;
      for (std::size_t i = 4; i < bytes.size(); ++i)
        if (bytes[i] != std::byte{})
          return false;
    }
    return true;
  }
  static Address loopback(Family family, std::uint16_t port = 0) noexcept;
  friend bool operator==(const Address &, const Address &) = default;
};
struct SocketConfig {
  Address bind;
  std::size_t ip_mtu = 1500;
  int send_buffer_bytes = 256 * 1024, receive_buffer_bytes = 256 * 1024;
};
struct SocketStats {
  int send_buffer_bytes = 0, receive_buffer_bytes = 0;
  bool no_fragment = false, nonblocking = false;
};
struct Received {
  Address source;
  std::size_t bytes = 0;
  bool truncated = false;
};
// Compiled syscall boundary. All operations are nonblocking; retryable errors
// preserve caller ownership. No scatter/gather flattening or hidden buffers.
class Socket {
  int fd_ = -1;
  Address local_{};
  SocketStats stats_{};
  explicit Socket(int fd, Address local) noexcept : fd_(fd), local_(local) {}

public:
  Socket() noexcept = default;
  Socket(const Socket &) = delete;
  Socket &operator=(const Socket &) = delete;
  Socket(Socket &&) noexcept;
  Socket &operator=(Socket &&) noexcept;
  ~Socket();
  static Result<Socket> open(const SocketConfig &) noexcept;
  const Address &local_address() const noexcept { return local_; }
  const SocketStats &stats() const noexcept { return stats_; }
  Result<std::size_t> send(const Address &, std::span<const Bytes>) noexcept;
  Result<Received> receive(MutableBytes, bool peek = false) noexcept;
  void close() noexcept;
};
} // namespace vita::adapters::posix_udp
