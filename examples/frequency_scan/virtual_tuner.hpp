#pragma once
#include <vita/runtime/public/config.hpp>

namespace vita::examples::frequency_scan {
// Replace this model with SDK callbacks; keep Runtime's single transaction
// engine. The model has zero physical settling delay. Completion reports the
// boundary selected by the engine, not merely the return of a device setter.
struct VirtualTuner {
  runtime::transaction::VirtualBackend<16> model;
  static void
  progress(void *context,
           const runtime::transaction::OperationContext &) noexcept {
    auto &backend =
        *static_cast<runtime::transaction::VirtualBackend<16> *>(context);
    if (backend.pending())
      (void)backend.complete_next();
  }
  static DeviceBackendBinding
  binding(const std::shared_ptr<VirtualTuner> &owner) noexcept {
    // binding() supplies validate/begin/simulate/disarm/quiescence callbacks.
    // Their context is model; the shared owner pins that member's storage.
    return {owner->model.binding(), owner, sizeof(VirtualTuner), progress};
  }
};
} // namespace vita::examples::frequency_scan
