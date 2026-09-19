#pragma once
#include <vita/runtime/stream/routing.hpp>
#include <vita/runtime/stream/counters.hpp>
#include <vita/runtime/execution/admission.hpp>
#include <vita/runtime/completion/ticket.hpp>
#include <array>
#include <optional>
#include <limits>
#include <cstring>
namespace vita::adapters::loopback {
struct Fault {
    bool synchronous_reject=false,lose=false,duplicate=false,fail_completion=false,hold_quiescence=false;
};
struct TxSubmission {
    memory::TxStorage storage;
    runtime::CompletionToken completion;
    runtime::PeerSession source;
    runtime::CounterKey counter;
    Fault fault{};
    runtime::AdmissionBundle completion_credit;
};
struct TxToken { std::size_t slot=0;std::uint64_t generation=0;friend bool operator==(TxToken,TxToken)=default; };
struct RejectedSubmission { Error error;TxSubmission submission; };
struct Capabilities {
    std::size_t max_packet_bytes=65535*4,max_tx_segments=3,max_rx_fragments=16;
    bool cpu_required=true,copies_to_receive_pool=true,completion_is_delivery=false;
    std::size_t reserved_control_slots=1,reserved_cancellation_slots=1,per_stream_data_limit=256;
};
// Caller-driven deterministic transport. Methods require one serialized execution domain.
// Callbacks may submit more work, but recursively progressing the same token is rejected.
template<std::size_t N=32,std::size_t Routes=64,std::size_t Counters=64> class Loopback {
    struct Slot {
        std::uint64_t generation=1,sequence=0;
        bool processing=false,quarantined=false,retired=false;
        std::optional<TxSubmission> submission;
        memory::BufferLease receive;
        runtime::AdmissionBundle credits;
        runtime::QuiescenceGuard quiescence;
    };
    std::array<Slot,N> slots_{};
    memory::ExternalPool data_pool_,control_pool_,cancellation_pool_;
    runtime::AdmissionPool& admission_;
    runtime::RouteRegistry<Routes>& routes_;
    runtime::CounterRegistry<Counters>& counters_;
    std::uint64_t next_sequence_=1;
    bool open_=true,progressing_=false;
    Capabilities capabilities_{};
    void release_slot(Slot& slot) noexcept {
        slot.receive.reset();slot.submission.reset();slot.credits.reset();slot.processing=false;slot.quarantined=false;
        if(slot.generation==std::numeric_limits<std::uint64_t>::max())slot.retired=true;else ++slot.generation;
    }
    Result<codec::PacketView> parse(runtime::PeerSession source,Bytes bytes) const noexcept {
        auto header=codec::decode_envelope(bytes);if(!header)return std::unexpected(header.error());
        auto route=routes_.lookup(source,header->envelope);if(!route)return std::unexpected(route.error());
        if(header->payload.size()<(*route)->minimum_payload_bytes||header->payload.size()>(*route)->maximum_payload_bytes)return std::unexpected(Error{ErrorCode::unsupported_capability});
        if(codec::is_extension(header->envelope.type)){auto valid=(*route)->validate_extension((*route)->context,*header);if(!valid)return std::unexpected(valid.error());}
        codec::DecodeOptions options{};
        if((*route)->request_context)options.request=(*route)->request_context((*route)->context,header->envelope);
        return codec::decode_packet(bytes,options);
    }
public:
    Loopback(memory::ExternalPool data_pool,memory::ExternalPool control_pool,memory::ExternalPool cancellation_pool,runtime::AdmissionPool& admission,runtime::RouteRegistry<Routes>& routes,
        runtime::CounterRegistry<Counters>& counters,Capabilities capabilities={}) : data_pool_(std::move(data_pool)),control_pool_(std::move(control_pool)),cancellation_pool_(std::move(cancellation_pool)),admission_(admission),routes_(routes),counters_(counters),capabilities_(capabilities) {}
    Loopback(const Loopback&)=delete;Loopback& operator=(const Loopback&)=delete;
    const Capabilities& capabilities() const noexcept {return capabilities_;}
    static constexpr std::size_t metadata_bytes() noexcept {return sizeof(Loopback)+N*runtime::QuiescenceGuard::metadata_bytes();}
    void close() noexcept {open_=false;}
    bool outstanding(TxToken token) const noexcept {return token.slot<slots_.size()&&slots_[token.slot].generation==token.generation&&(slots_[token.slot].submission||slots_[token.slot].quarantined);}
    std::size_t outstanding() const noexcept {std::size_t count=0;for(const auto& slot:slots_)if(slot.submission||slot.quarantined)++count;return count;}
    std::expected<TxToken,RejectedSubmission> try_send(TxSubmission&& submission) noexcept {
        auto reject=[&](Error error)->std::expected<TxToken,RejectedSubmission>{return std::unexpected(RejectedSubmission{error,std::move(submission)});};
        if(!open_)return reject(Error{ErrorCode::invalid_state});
        const bool supplied=submission.completion_credit.held(runtime::Resource::completion)!=0;
        if(supplied){
            if(!admission_.owns(submission.completion_credit)||submission.completion_credit.held(runtime::Resource::completion)!=1)return reject(Error{ErrorCode::invalid_argument});
            for(std::size_t r=0;r<runtime::resource_count;++r)if(r!=static_cast<std::size_t>(runtime::Resource::completion)&&submission.completion_credit.held(static_cast<runtime::Resource>(r)))return reject(Error{ErrorCode::invalid_argument});
        }else for(std::size_t r=0;r<runtime::resource_count;++r)if(submission.completion_credit.held(static_cast<runtime::Resource>(r)))return reject(Error{ErrorCode::invalid_argument});

        if(capabilities_.reserved_control_slots==0 || capabilities_.reserved_cancellation_slots==0 ||
            capabilities_.reserved_control_slots>=N || capabilities_.reserved_cancellation_slots>=N-capabilities_.reserved_control_slots ||
            !capabilities_.per_stream_data_limit || capabilities_.per_stream_data_limit>256 ||
            !capabilities_.max_tx_segments || capabilities_.max_tx_segments>3 || !capabilities_.max_rx_fragments || capabilities_.max_rx_fragments>16 ||
            !data_pool_.block_count() || !control_pool_.block_count() || !cancellation_pool_.block_count() || !capabilities_.cpu_required || !capabilities_.copies_to_receive_pool || capabilities_.completion_is_delivery ||
            data_pool_.shares_provider_with(control_pool_) || data_pool_.shares_provider_with(cancellation_pool_) || control_pool_.shares_provider_with(cancellation_pool_))return reject(Error{ErrorCode::invalid_argument});
        if(submission.fault.synchronous_reject)return reject(Error{ErrorCode::capacity_exhausted});
        if(!submission.completion.is_reserved() || !submission.source.generation || (submission.fault.hold_quiescence&&!submission.fault.fail_completion))return reject(Error{ErrorCode::invalid_argument});
        const auto bytes=submission.storage.byte_size();
        if(!bytes||bytes>capabilities_.max_packet_bytes||submission.storage.segment_count()>capabilities_.max_tx_segments)return reject(Error{ErrorCode::unsupported_capability});
        if(bytes<4)return reject(Error{ErrorCode::short_input,0,4});
        std::array<std::byte,4> header_bytes{};std::size_t copied=0;
        for(std::size_t i=0;i<submission.storage.segment_count()&&copied<4;++i){auto part=submission.storage.segment(i);if(!part)return reject(part.error());const auto n=std::min(std::size_t{4}-copied,part->size());std::memcpy(header_bytes.data()+copied,part->data(),n);copied+=n;}
        const bool data=codec::is_data(submission.counter.type);
        const bool cancellation=codec::is_command(submission.counter.type)&&(codec::detail::load32(header_bytes,0)&(1u<<24));
        const auto data_end=N-capabilities_.reserved_control_slots-capabilities_.reserved_cancellation_slots;
        const auto control_end=N-capabilities_.reserved_cancellation_slots;
        std::size_t index=N;
        for(std::size_t i=data?0:cancellation?control_end:data_end;i<(data?data_end:cancellation?N:control_end);++i)if(!slots_[i].submission&&!slots_[i].quarantined&&!slots_[i].retired){index=i;break;}
        if(data){std::size_t pending=0;for(const auto& s:slots_)if(s.submission&&s.submission->counter==submission.counter)++pending;if(pending>=capabilities_.per_stream_data_limit)return reject(Error{ErrorCode::capacity_exhausted});}
        if(index==N)return reject(Error{ErrorCode::capacity_exhausted});
        if(next_sequence_==std::numeric_limits<std::uint64_t>::max())return reject(Error{ErrorCode::overflow});
        auto buffer=(data?data_pool_:cancellation?cancellation_pool_:control_pool_).acquire(memory::BufferRequest{bytes,1,memory::MemoryDomain::cpu,true});if(!buffer)return reject(buffer.error());
        auto writable=buffer->writable_bytes();if(!writable)return reject(writable.error());
        std::size_t offset=0;
        for(std::size_t i=0;i<submission.storage.segment_count();++i){auto segment=submission.storage.segment(i);if(!segment)return reject(segment.error());std::memcpy(writable->data()+offset,segment->data(),segment->size());offset+=segment->size();}
        auto sized=buffer->set_size(bytes);if(!sized)return reject(sized.error());
        auto parsed=parse(submission.source,writable->first(bytes));if(!parsed)return reject(parsed.error());
        const auto& envelope=parsed->envelope.envelope;
        if(envelope.stream_id!=submission.counter.stream_id||envelope.type!=submission.counter.type)return reject(Error{ErrorCode::invalid_argument});
        auto expected=counters_.next(submission.counter);if(!expected)return reject(expected.error());if(*expected!=envelope.packet_count)return reject(Error{ErrorCode::stale_generation});
        if(!data){
            index=N;const auto begin=envelope.cancel?control_end:data_end;const auto end=envelope.cancel?N:control_end;
            for(std::size_t i=begin;i<end;++i)if(!slots_[i].submission&&!slots_[i].quarantined&&!slots_[i].retired){index=i;break;}
            if(index==N)return reject(Error{ErrorCode::capacity_exhausted});
        }
        runtime::AdmissionRequest request;request.need(runtime::Resource::completion,supplied?0:1).need(data?runtime::Resource::data_queue:envelope.cancel?runtime::Resource::cancellation_queue:runtime::Resource::ordinary_queue);
        auto credits=admission_.acquire(request);if(!credits)return reject(credits.error());
        // No operation after counter commit can reject: moves into an already selected slot are nonthrowing.
        auto committed=counters_.accept(submission.counter,envelope.packet_count);if(!committed)return reject(committed.error());
        auto& slot=slots_[index];slot.receive=std::move(*buffer);slot.credits=std::move(*credits);slot.sequence=next_sequence_++;slot.submission.emplace(std::move(submission));
        return TxToken{index,slot.generation};
    }
    Result<void> progress(TxToken token) noexcept {
        if(progressing_)return std::unexpected(Error{ErrorCode::would_deadlock});
        struct ProgressGuard {bool& flag;explicit ProgressGuard(bool& value):flag(value){flag=true;}~ProgressGuard(){flag=false;}} guard(progressing_);
        if(token.slot>=N)return std::unexpected(Error{ErrorCode::invalid_argument});auto& slot=slots_[token.slot];
        if(slot.generation!=token.generation||!slot.submission||slot.processing||slot.quarantined)return std::unexpected(Error{ErrorCode::stale_generation});
        slot.processing=true;auto& submission=*slot.submission;
        runtime::CompletionResult completion;completion.value=submission.storage.byte_size();
        if(submission.fault.fail_completion){completion.status=runtime::CompletionStatus::failed;completion.error=Error{ErrorCode::callback_failure};}
        if(!submission.fault.fail_completion&&!submission.fault.lose) {
            auto bytes=slot.receive.bytes();
            if(!bytes){slot.processing=false;return std::unexpected(bytes.error());}
            auto parsed=parse(submission.source,*bytes);
            if(!parsed){completion.status=runtime::CompletionStatus::failed;completion.error=parsed.error();}
            else {
                auto route=routes_.lookup(submission.source,parsed->envelope.envelope);
                memory::RxEnvelope rx;auto lease=rx.add_buffer(std::move(slot.receive));
                if(!route||!lease){slot.processing=false;return std::unexpected(Error{ErrorCode::invalid_state});}
                const auto offset=parsed->envelope.payload_offset,length=parsed->envelope.payload.size();
                auto prologue=rx.set_prologue({*lease,0,offset});
                if(!prologue){slot.processing=false;return std::unexpected(prologue.error());}
                if(length){auto payload=rx.append_payload({*lease,offset,length});if(!payload){slot.processing=false;return std::unexpected(payload.error());}}
                if(parsed->envelope.trailer){auto trailer=rx.set_trailer({*lease,offset+length,4});if(!trailer){slot.processing=false;return std::unexpected(trailer.error());}}
                (*route)->receive((*route)->context,*parsed,rx);
                if(submission.fault.duplicate)(*route)->receive((*route)->context,*parsed,rx);
            }
        }
        if(submission.fault.hold_quiescence) {
            auto armed=slot.quiescence.arm(std::move(submission.storage));
            if(!armed){slot.processing=false;return std::unexpected(armed.error());}
            slot.quarantined=true;
        }
        // Local completion is separate from receiver delivery and from device-quiescence evidence.
        const bool published=submission.completion.publish(completion);
        if(slot.quarantined){slot.receive.reset();slot.submission.reset();slot.processing=false;}
        else release_slot(slot);
        if(!published)return std::unexpected(Error{ErrorCode::invalid_state});return {};
    }
    Result<bool> progress_next() noexcept {
        if(progressing_)return std::unexpected(Error{ErrorCode::would_deadlock});
        std::size_t index=N;std::uint64_t sequence=std::numeric_limits<std::uint64_t>::max();
        for(std::size_t i=0;i<N;++i)if(slots_[i].submission&&!slots_[i].processing&&slots_[i].sequence<sequence){index=i;sequence=slots_[i].sequence;}
        if(index==N)return false;auto result=progress(TxToken{index,slots_[index].generation});if(!result)return std::unexpected(result.error());return true;
    }
    Result<void> prove_quiescent(TxToken token) noexcept {
        if(token.slot>=N)return std::unexpected(Error{ErrorCode::invalid_argument});auto& slot=slots_[token.slot];
        if(slot.generation!=token.generation||!slot.quarantined)return std::unexpected(Error{ErrorCode::stale_generation});
        if(!slot.quiescence.prove_quiescent())return std::unexpected(Error{ErrorCode::invalid_state});release_slot(slot);return {};
    }
};
} // namespace vita::adapters::loopback
