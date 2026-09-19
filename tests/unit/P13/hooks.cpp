#include <atomic>
#include <cassert>
#include <cstdlib>
#include <new>
#include <thread>
#include <vita/runtime/transaction/engine.hpp>
static std::size_t allocations = 0;
void *operator new(std::size_t n) {
  ++allocations;
  if (auto *p = std::malloc(n ? n : 1))
    return p;
  std::abort();
}
void *operator new[](std::size_t n) { return ::operator new(n); }
void operator delete(void *p) noexcept { std::free(p); }
void operator delete[](void *p) noexcept { std::free(p); }
using namespace vita;
using namespace vita::runtime;
using namespace vita::runtime::transaction;
struct Sink {
  std::array<TraceEvent, 64> events{};
  std::size_t count = 0;
  bool overflow = false;
  std::atomic<std::uint64_t> clock{10};
  static std::uint64_t now(void *p) noexcept {
    return static_cast<Sink *>(p)->clock.fetch_add(10);
  }
  static void record(void *p, const TraceEvent &event) noexcept {
    auto &sink = *static_cast<Sink *>(p);
    if (sink.count == sink.events.size()) {
      sink.overflow = true;
      return;
    }
    sink.events[sink.count++] = event;
  }
};
StateSnapshot state() {
  StateSnapshot s;
  for (auto id : baseline_fields)
    s.fields[field_index(id)].validity = Validity::known;
  s.fields[0].value = std::uint32_t{1};
  s.fields[1].value = *Hertz::from_integer(1'000'000);
  s.fields[2].value = std::uint32_t{0};
  s.fields[3].value = PayloadFormat{0x200003cf00000000ULL};
  return s;
}
int main() {
  auto sink = std::make_shared<Sink>();
  TraceBinding trace{sink, sink.get(), Sink::now, Sink::record, sizeof(Sink)};
  AdmissionPool admission(AdmissionPool::reference_capacities());
  VirtualBackend<4> backend;
  assert(backend.set_inline_completion(true));
  EngineOptions options;
  options.profile = Profile::generic_virtual_test;
  options.trace = trace;
  Engine<4> engine(admission, backend.binding(), state(), options);
  codec::Envelope envelope;
  envelope.type = codec::PacketType::command;
  envelope.stream_id = 1;
  envelope.command =
      codec::Command{0xa9080000, 7, codec::Identifier::short_id(2),
                     codec::Identifier::short_id(3)};
  ControlPacket control;
  assert(control.set<SampleRate>(*Hertz::from_integer(2'000'000)));
  std::array<std::byte, 256> wire{};
  auto size = codec::encode_packet(envelope, control.freeze(), wire);
  assert(size);
  auto parsed = codec::decode_packet(Bytes{wire}.first(*size));
  assert(parsed);
  OperationContext now;
  now.operation = 11;
  now.trace_peer = 77;
  trace.emit(TraceStage::received, {1, 11, 77, 1, 7});
  auto handle = engine.accept(*parsed, now);
  assert(handle && sink->count == 2 &&
         sink->events[1].stage == TraceStage::validated);
  assert(backend.writes() == 0);
  assert(engine.progress(now));
  assert(backend.writes() == 1 && backend.pending() == 0 && sink->count == 3 &&
         sink->events[2].stage == TraceStage::dispatch);
  assert(std::get<Hertz>(backend.model().fields[1].value) ==
         *Hertz::from_integer(2'000'000));
  assert(!*engine.complete(*handle));
  assert(engine.progress(now) && *engine.complete(*handle));
  assert(sink->count == 5 && sink->events[3].stage == TraceStage::device_done &&
         sink->events[4].stage == TraceStage::recorded);
  for (std::size_t i = 0; i < 5; ++i) {
    assert(sink->events[i].key == TraceKey({1, 11, 77, 1, 7}));
    if (i)
      assert(sink->events[i].monotonic_ns > sink->events[i - 1].monotonic_ns);
  }
  assert(sink->events[4].status == FieldStatus::executed &&
         !sink->events[4].simulated);
  while (true) {
    auto response = engine.take_response(*handle);
    assert(response);
    if (!*response)
      break;
  }
  assert(engine.release(*handle));
  // Multiple fields preserve separate backend stages and terminal-only
  // recording.
  ControlPacket multi;
  assert(multi.set<SampleRate>(*Hertz::from_integer(3'000'000)) &&
         multi.set<StateEvent>(1));
  std::array<std::byte, 256> multi_wire{};
  envelope.command->message_id = 8;
  auto multi_size = codec::encode_packet(envelope, multi.freeze(), multi_wire);
  assert(multi_size);
  auto multi_packet =
      codec::decode_packet(Bytes{multi_wire}.first(*multi_size));
  assert(multi_packet);
  now.operation = 12;
  const auto begin = sink->count;
  auto multi_handle = engine.accept(*multi_packet, now);
  assert(multi_handle);
  assert(engine.progress(now) && engine.progress(now) && engine.progress(now));
  assert(*engine.complete(*multi_handle) && sink->count == begin + 6 &&
         sink->events[begin + 5].stage == TraceStage::recorded);
  while (true) {
    auto response = engine.take_response(*multi_handle);
    assert(response);
    if (!*response)
      break;
  }
  assert(engine.release(*multi_handle));
  const auto allocated = allocations;
  for (unsigned i = 0; i < 100; ++i) {
    now.operation = 100 + i;
    auto repeated = engine.accept(*parsed, now);
    assert(repeated && engine.progress(now) && engine.progress(now));
    while (true) {
      auto response = engine.take_response(*repeated);
      assert(response);
      if (!*response)
        break;
    }
    assert(engine.release(*repeated));
  }
  assert(allocations == allocated && sink->overflow);
  // Rejected admission does not invent validated/dispatch/completion events.
  AdmissionPool empty(AdmissionRequest{});
  Engine<1> denied(empty, backend.binding(), state(), options);
  auto before = sink->count;
  assert(!denied.accept(*parsed, now) && sink->count == before);
  // Cross-thread timestamp publication shares the completion release/acquire.
  CompletionArena<1> tickets;
  auto storage = std::make_shared<ResultStorage<1>>();
  storage->trace = trace;
  auto ticket = tickets.reserve(99);
  assert(ticket);
  AsyncResult capability(ticket->publisher(), storage,
                         &storage->slots[0].outcome, 0,
                         &storage->slots[0].guard, &storage->trace,
                         &storage->slots[0].device_done_ns);
  FieldOutcome outcome;
  outcome.id = SampleRate::id;
  outcome.status = FieldStatus::executed;
  outcome.validity = Validity::known;
  outcome.value = *Hertz::from_integer(3'000'000);
  std::atomic<bool> go{false};
  std::thread producer([capability, outcome, &go] {
    while (!go.load(std::memory_order_acquire)) {
    }
    assert(capability.complete(outcome));
  });
  go.store(true, std::memory_order_release);
  bool seen = false;
  while (!seen)
    tickets.scan([&](CompletionRecord) noexcept {
      assert(storage->slots[0].device_done_ns > 0 &&
             storage->slots[0].outcome.value == outcome.value);
      seen = true;
    });
  producer.join();
  const auto stamp = storage->slots[0].device_done_ns;
  assert(!capability.complete(outcome) &&
         storage->slots[0].device_done_ns == stamp);
  // A late capability retains the clock context independently of the frontend.
  std::weak_ptr<Sink> weak = sink;
  options.trace = {};
  trace = {};
  sink.reset();
  assert(!weak.expired());
}
