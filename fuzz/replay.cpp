#include "packet_target.hpp"
#include <algorithm>
#include <array>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <vector>

namespace {
std::uint64_t random_word(std::uint64_t& state) noexcept {
  state ^= state << 13; state ^= state >> 7; state ^= state << 17; return state;
}
}
int main(int argc, char** argv) {
  if (argc < 2 || argc > 4) return 2;
  const auto rounds = argc > 2 ? std::strtoull(argv[2], nullptr, 10) : 100000;
  std::uint64_t state = argc > 3 ? std::strtoull(argv[3], nullptr, 0) : 0x564954413439ULL;
  if (!state || rounds > 10000000) return 2;
  std::error_code error;
  std::vector<std::filesystem::path> paths;
  for (std::filesystem::directory_iterator it(argv[1], error), end; !error && it != end; it.increment(error))
    if (it->is_regular_file() && it->path().extension() == ".bin") paths.push_back(it->path());
  if (error || paths.empty() || paths.size() > 256) return 2;
  std::sort(paths.begin(), paths.end());
  std::vector<std::vector<std::byte>> seeds;
  for (const auto& path : paths) {
    const auto size = std::filesystem::file_size(path, error);
    if (error || size > vita::fuzz::max_input_bytes) return 2;
    std::ifstream file(path, std::ios::binary);
    std::vector<std::byte> bytes(size);
    if (size && !file.read(reinterpret_cast<char*>(bytes.data()), size)) return 2;
    vita::fuzz::exercise(bytes);
    seeds.push_back(std::move(bytes));
  }
  std::array<std::byte, vita::fuzz::max_input_bytes> buffer{};
  std::uint64_t checksum = 0;
  for (std::uint64_t round = 0; round < rounds; ++round) {
    const auto& seed = seeds[random_word(state) % seeds.size()];
    std::size_t size = seed.size();
    std::copy(seed.begin(), seed.end(), buffer.begin());
    switch (random_word(state) % 6) {
      case 0: // Truncate at every possible byte boundary across iterations.
        size = size ? random_word(state) % (size + 1) : 0; break;
      case 1: // Arbitrary bit mutations include CIF and CAM selector combinations.
        for (unsigned n = 0, count = 1 + random_word(state) % 16; n < count && size; ++n)
          buffer[random_word(state) % size] ^= std::byte(1u << (random_word(state) % 8));
        break;
      case 2: // Claimed packet length independent of actual datagram extent.
        if (size >= 4) { buffer[2] = std::byte(random_word(state)); buffer[3] = std::byte(random_word(state)); }
        break;
      case 3: // Fresh random datagrams rather than only mutations of valid inputs.
        size = random_word(state) % 8193;
        for (std::size_t n = 0; n < size; ++n) buffer[n] = std::byte(random_word(state));
        break;
      case 4: // Append tail bytes; occasionally exercise the full explicit input cap.
        { auto added = round % 257 == 0 ? buffer.size() - size : std::min<std::size_t>(buffer.size() - size, random_word(state) % 128);
          for (std::size_t n = 0; n < added; ++n) buffer[size+n] = std::byte(random_word(state));
          size += added; }
        break;
      case 5: // Mutate complete words without preserving field/layout constraints.
        if (size >= 4) { const auto at = (random_word(state) % (size / 4)) * 4; const auto word = random_word(state); for (unsigned n=0;n<4;++n) buffer[at+n]=std::byte(word>>(8*n)); }
        break;
    }
    vita::fuzz::exercise(vita::Bytes(buffer).first(size));
    checksum ^= random_word(state) + size;
  }
  std::printf("seeds=%zu mutations=%llu final_rng=%llu checksum=%llu cap=%zu\n", seeds.size(), (unsigned long long)rounds, (unsigned long long)state, (unsigned long long)checksum, buffer.size());
}
