#include "application.hpp"
#include <chrono>
#include <csignal>
#include <cstdlib>
#include <thread>
#include <vita/profiles/iq/lab.hpp>
#ifdef VITA_SCAN_UDP
#include <vita/adapters/posix_udp/factory.hpp>
#endif

namespace demo = vita::examples::frequency_scan;
namespace {
volatile std::sig_atomic_t interrupted = 0;
void interrupt(int) noexcept { interrupted = 1; }
struct Shutdown {
  bool done = false;
  vita::LifecycleStatus status{};
  static void complete(void *p, const vita::LifecycleStatus &value) noexcept {
    auto &self = *static_cast<Shutdown *>(p);
    self.status = value;
    self.done = true;
  }
};
int error(const char *stage, const vita::Error &value) {
  std::fprintf(stderr,
               "%s failed code=%u native=%d; timeout/failure does not "
               "establish no effect\n",
               stage, unsigned(value.code), value.native_error);
  return 1;
}
} // namespace
int main(int argc, char **argv) {
  // All three programs are isolated-lab examples. No external-address override.
#if defined(VITA_SCAN_CONTROLLEE)
  constexpr auto role = demo::EndpointRole::controllee;
  constexpr bool serves = true, controls = false;
#else
  constexpr auto role = demo::EndpointRole::controller;
  constexpr bool controls = true;
#ifdef VITA_SCAN_UDP
  constexpr bool serves = false;
#else
  constexpr bool serves = true;
#endif
#endif
  std::array<std::string_view, 64> args{};
  std::size_t count = 0;
  bool deterministic = false;
  std::uint64_t duration_ns = 0;
  for (int i = 1; i < argc; ++i) {
    std::string_view key = argv[i];
    if (key == "--deterministic") {
#ifdef VITA_SCAN_UDP
      std::fputs(
          "--deterministic is combined-only; UDP uses real elapsed time\n",
          stderr);
      return 2;
#else
      deterministic = true;
      continue;
#endif
    }
    if (key == "--duration-ms") {
      if (++i == argc)
        return 2;
      const std::string_view value = argv[i];
      std::uint64_t ms = 0;
      auto parsed =
          std::from_chars(value.data(), value.data() + value.size(), ms);
      if (parsed.ec != std::errc{} ||
          parsed.ptr != value.data() + value.size() || !ms ||
          ms > UINT64_MAX / 1'000'000)
        return 2;
      duration_ns = ms * 1'000'000;
      continue;
    }
    if (count == args.size())
      return 2;
    args[count++] = key;
    if (key != "--help" && key != "--continuous") {
      if (++i == argc || count == args.size())
        return 2;
      args[count++] = argv[i];
    }
  }
  auto options = demo::parse_endpoint_options(
      role, std::span<const std::string_view>{args}.first(count));
  if (!options)
    return error("options", options.error());
  if (options->scan.help) {
    std::printf("%.*s\n%.*s\n--duration-ms N; combined only: --deterministic\n",
                int(demo::usage.size()), demo::usage.data(),
                int(demo::endpoint_usage.size()), demo::endpoint_usage.data());
    return 0;
  }
  std::optional<vita::profiles::iq::VirtualRfScene> scene;
  std::optional<vita::profiles::iq::SweepPolicy> policy;
  if constexpr (serves) {
    auto made = vita::profiles::iq::VirtualRfScene::create(options->scan.scene);
    if (!made)
      return 2;
    scene = *made;
  }
  if constexpr (controls) {
    auto made = vita::profiles::iq::SweepPolicy::create(options->scan.sweep);
    if (!made)
      return 2;
    policy = *made;
  }
  auto config = vita::profiles::iq::lab::config(0xabcdef);
  auto pools = vita::profiles::iq::lab::pools();
  if (!config || !pools)
    return 2;
#ifdef VITA_SCAN_UDP
  using namespace vita::adapters::posix_udp;
  FactoryConfig<64, 8> udp;
  udp.config.capabilities.reserved_control_slots = 16;
  udp.config.capabilities.reserved_cancellation_slots = 4;
  const auto local = options->local, peer = options->peer;
  const std::array<std::uint16_t, 3> local_ports{local.data, local.control,
                                                 local.cancellation};
  const std::array<std::uint16_t, 3> peer_ports{peer.data, peer.control,
                                                peer.cancellation};
  PeerBinding binding;
  binding.local_source = {controls ? 1u : 2u, 1};
  binding.remote_source = {controls ? 2u : 1u, 1};
  for (std::size_t i = 0; i < 3; ++i) {
    udp.config.sockets[i].bind =
        Address::loopback(Family::ipv4, local_ports[i]);
    binding.remote[i] = Address::loopback(Family::ipv4, peer_ports[i]);
  }
  if (!udp.peers.push_back(binding))
    return 2;
  config->transport = factory(udp);
#endif
  // Borrowed callback objects precede Runtime, so even setup-error unwinding
  // destroys/detaches Runtime before those objects can be destroyed.
  demo::ReceiverCounters received;
  std::optional<demo::Runtime::Controllee> radio;
  std::optional<demo::Runtime::Controller> controller;
  std::shared_ptr<demo::VirtualTuner> tuner;
  std::optional<demo::ScanApplication> scan;
  Shutdown shutdown;
  auto runtime = demo::Runtime::create(*config, std::move(*pools));
  if (!runtime)
    return error("runtime setup", runtime.error());
  if constexpr (serves) {
    tuner = std::make_shared<demo::VirtualTuner>();
    vita::StreamConfig stream;
    stream.sid = 1;
    stream.controller_id = 2;
    stream.controllee_id = 3;
    stream.profile = vita::profiles::iq::Profile::frequency_tunable;
    stream.sample_rate = options->scan.scene.sample_rate;
    stream.center_frequency = options->scan.sweep.start_hz;
    stream.source = {&*scene,
                     vita::profiles::iq::VirtualRfScene::produce_callback,
                     vita::profiles::iq::VirtualRfScene::effective_callback};
    stream.device = demo::VirtualTuner::binding(tuner);
    if constexpr (controls)
      stream.receiver = received.binding();
    else
      stream.role = vita::EndpointRole::controllee_only;
    auto added = (*runtime)->add_controllee(stream);
    if (!added)
      return error("Controllee setup", added.error());
    radio = *added;
    if constexpr (controls) {
      auto c = (*runtime)->add_controller(*radio);
      if (!c)
        return error("Controller setup", c.error());
      controller = *c;
    }
  } else {
    vita::RemoteTargetConfig target;
    target.sid = 1;
    target.controller_id = 2;
    target.controllee_id = 3;
    target.profile = vita::profiles::iq::Profile::frequency_tunable;
    target.sample_rate = options->scan.scene.sample_rate;
    target.receiver = received.binding();
    auto added = (*runtime)->add_remote_controller(target);
    if (!added)
      return error("remote Controller setup", added.error());
    controller = *added;
  }
  if constexpr (serves) {
    auto clock = (*runtime)->observe_pps({0}, {1000, 0});
    if (!clock)
      return error("simulated PPS", clock.error());
  }
  if (radio) {
    auto started = radio->start();
    if (!started)
      return error("source start", started.error());
  }
  if (controller) {
    scan.emplace(*controller, *policy, options->scan);
    auto begin = scan->begin();
    if (!begin)
      return error("initial named query", begin.error());
  }
  std::signal(SIGINT, interrupt);
  std::signal(SIGTERM, interrupt);
  std::printf("ready role=%s mode=%s isolated_lab=true fixture_oui=abcdef "
              "simulated_pps=%s local_base=%u peer_base=%u framework_bytes=%zu "
              "scene_active=%s scene_type_bytes=%zu sweep_type_bytes=%zu\n",
              serves ? (controls ? "combined" : "controllee") : "controller",
              deterministic ? "deterministic" : "wallclock",
              serves ? "true" : "false", options->local.data,
              options->peer.data, (*runtime)->budget().charged_bytes(),
              serves ? "true" : "false", sizeof(*scene), sizeof(*policy));
  std::fflush(stdout);
  const auto origin = std::chrono::steady_clock::now();
  std::uint64_t now = 0, last_pps = 0;
  int result = 0;
  auto tick = [&]() {
    if (deterministic)
      now += 100'000;
    else
      now = static_cast<std::uint64_t>(
          std::chrono::duration_cast<std::chrono::nanoseconds>(
              std::chrono::steady_clock::now() - origin)
              .count());
    if (serves && now - last_pps >= 1'000'000'000) {
      const auto second = now / 1'000'000'000;
      auto observed =
          (*runtime)->observe_pps({second * 1'000'000'000}, {1000 + second, 0});
      if (!observed) {
        result = error("simulated PPS", observed.error());
        return false;
      }
      last_pps = second * 1'000'000'000;
    }
    auto progress = (*runtime)->progress({now});
    if (!progress && !progress.error().retryable) {
      result = error("progress", progress.error());
      return false;
    }
    return true;
  };
  while (!interrupted && (!duration_ns || now < duration_ns)) {
    if (!tick())
      break;
    if (scan) {
      auto done = scan->progress({now});
      if (!done) {
        result = error("scan", done.error());
        break;
      }
      if (*done)
        break;
    }
    if (radio && radio->status() != vita::SourceStatus::running) {
      std::fputs("source fault; known tuning does not imply IQ delivery\n",
                 stderr);
      result = 1;
      break;
    }
    if (!deterministic)
      std::this_thread::sleep_for(std::chrono::microseconds(100));
  }
  // Stop admission, continue owner progress, and wait for actual lifecycle
  // proof.
  if (scan && !result && (interrupted || (duration_ns && now >= duration_ns))) {
    auto cancelled = scan->cancel_on_operator_stop();
    if (!cancelled)
      result = error("explicit cancellation admission", cancelled.error());
    else if (*cancelled) {
      const auto until = now + 250'000'000;
      while (!scan->cancellation_done() && now < until) {
        if (!tick())
          break;
        if (!deterministic)
          std::this_thread::sleep_for(std::chrono::microseconds(100));
      }
      std::printf("cancellation observed=%s confirmed=%s (not remote physical "
                  "quiescence)\n",
                  scan->cancellation_done() ? "true" : "false",
                  scan->cancellation_confirmed() ? "true" : "false");
    }
  }
  if (scan)
    scan->release();
  auto closing = (*runtime)->shutdown(vita::StopMode::graceful,
                                      {&shutdown, Shutdown::complete});
  if (!closing) {
    error("shutdown", closing.error());
    std::fflush(nullptr);
    // Isolated virtual-process failure: do not unwind callback owners without
    // proof. This is NOT a hardware shutdown policy or quiescence assertion.
    std::_Exit(1);
  }
  const auto deadline = now + 2'100'000'000;
  while (!shutdown.done && now < deadline) {
    if (!tick())
      break;
    if (!deterministic)
      std::this_thread::sleep_for(std::chrono::microseconds(100));
  }
  if (!shutdown.done ||
      shutdown.status.phase == vita::LifecyclePhase::quarantined ||
      shutdown.status.phase == vita::LifecyclePhase::failed) {
    std::fputs("shutdown lacks clean quiescence proof; terminating isolated "
               "virtual process without owner unwinding\n",
               stderr);
    std::fflush(nullptr);
    std::_Exit(1);
  }
  std::printf(
      "summary confirmed=%llu received_iq=%llu known_iq=%llu "
      "receiver_drops=%llu backend_writes=%llu shutdown=%u\n",
      static_cast<unsigned long long>(scan ? scan->confirmed() : 0),
      static_cast<unsigned long long>(received.packets),
      static_cast<unsigned long long>(received.known),
      static_cast<unsigned long long>(received.drops),
      static_cast<unsigned long long>(tuner ? tuner->model.writes() : 0),
      unsigned(shutdown.status.phase));
  runtime
      ->reset(); // All borrowed callback contexts remain alive through detach.
  return result;
}
