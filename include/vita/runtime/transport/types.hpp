#pragma once
#include <vita/runtime/completion/ticket.hpp>
#include <vita/runtime/execution/admission.hpp>
#include <vita/runtime/stream/counters.hpp>
#include <vita/runtime/stream/routing.hpp>
namespace vita::runtime::transport {
struct Fault {
  bool synchronous_reject = false, lose = false, duplicate = false,
       fail_completion = false, hold_quiescence = false;
};
struct TxSubmission {
  memory::TxStorage storage;
  runtime::CompletionToken completion;
  runtime::PeerSession source;
  runtime::CounterKey counter;
  Fault fault{};
  runtime::AdmissionBundle completion_credit;
};
struct TxToken {
  std::size_t slot = 0;
  std::uint64_t generation = 0;
  friend bool operator==(TxToken, TxToken) = default;
};
struct RejectedSubmission {
  Error error;
  TxSubmission submission;
};
struct Capabilities {
  friend bool operator==(const Capabilities &, const Capabilities &) = default;
  std::size_t max_packet_bytes = 65535 * 4, max_tx_segments = 3,
              max_rx_fragments = 16;
  bool cpu_required = true, copies_to_receive_pool = true,
       completion_is_delivery = false;
  std::size_t reserved_control_slots = 1, reserved_cancellation_slots = 1,
              per_stream_data_limit = 256;
};
} // namespace vita::runtime::transport
