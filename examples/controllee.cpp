#include <cstdio>
#include <vita/profiles/iq/lab.hpp>
#include <vita/runtime/public/runtime.hpp>
int main() {
  auto config = vita::profiles::iq::lab::config(0xabcdef);
  auto pools = vita::profiles::iq::lab::pools();
  if (!config || !pools)
    return 1;
  auto runtime =
      vita::VitaRuntime<1, 4, 32, 65536>::create(*config, std::move(*pools));
  if (!runtime)
    return 2;
  vita::StreamConfig binding;
  binding.sid = 1;
  binding.controller_id = 2;
  binding.controllee_id = 3;
  auto radio = (*runtime)->add_controllee(binding);
  if (!radio)
    return 3;
  if (!(*runtime)->observe_pps({0}, {1000, 0}) || !radio->start() ||
      !(*runtime)->run_for(1'000'000))
    return 4;
  std::printf("Controllee emitted %llu packets\n",
              static_cast<unsigned long long>(radio->metrics().packets));
}
