#include "packet_target.hpp"
extern "C" int LLVMFuzzerTestOneInput(const unsigned char* data, std::size_t size) {
  vita::fuzz::exercise({reinterpret_cast<const std::byte*>(data), size});
  return 0;
}
