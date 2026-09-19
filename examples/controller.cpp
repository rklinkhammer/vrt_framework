#include <cstdio>
#include <vita/profiles/iq/lab.hpp>
#include <vita/runtime/public/runtime.hpp>
int main() {
  // Explicit isolated fixture OUI; never an externally assigned production
  // identity.
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
  auto target = (*runtime)->add_controllee(binding);
  if (!target)
    return 3;
  auto controller = (*runtime)->add_controller(*target);
  if (!controller)
    return 4;
  auto query = controller->query();
  if (!query || !(*runtime)->run_for(1000))
    return 5;
  auto state = controller->state(*query);
  if (!state || !*state)
    return 6;
  std::printf(
      "Controller observed %.0f samples/s\n",
      double(std::get<vita::Hertz>((*state)->state.fields[1].value).q20) /
          (1 << 20));
}
