#include <cassert>
#include <cstdio>
#include <vita/profiles/iq/lab.hpp>
#include <vita/runtime/public/runtime.hpp>
int main() {
  auto config = vita::profiles::iq::lab::config(0xabcdef);
  auto counts = vita::profiles::iq::lab::reference_counts();
  auto measured = vita::profiles::iq::lab::measure(counts);
  assert(config && measured && measured->raw_bytes == 30'998'528);
  auto pools = vita::profiles::iq::lab::pools(counts);
  assert(pools);
  auto runtime = vita::VitaRuntime<>::create(*config, std::move(*pools));
  assert(runtime);
  for (std::uint32_t sid = 1; sid <= 16; ++sid) {
    vita::StreamConfig stream;
    stream.sid = sid;
    stream.controller_id = 100 + sid;
    stream.controllee_id = 200 + sid;
    auto device = (*runtime)->add_controllee(stream);
    assert(device);
  }
  const auto &ledger = (*runtime)->budget();
  assert(ledger.charged_bytes() < vita::runtime::framework_budget);
  assert(ledger.reserved_bytes() == vita::runtime::framework_budget);
  std::printf("Reference actual native storage plus declared ownership "
              "overhead: %zu / %zu bytes\n",
              ledger.charged_bytes(), ledger.reserved_bytes());
  for (std::size_t i = 0; i < vita::runtime::budget_category_count; ++i) {
    auto row = ledger.row(static_cast<vita::runtime::BudgetCategory>(i));
    std::printf("category%zu charged%zu reserved%zu\n", i, row.charged,
                row.reserved);
  }
}
