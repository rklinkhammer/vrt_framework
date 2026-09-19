#pragma once
#include "replay_wire.hpp"
#include <vita/adapters/posix_udp/udp.hpp>
#include <vita/runtime/context/receiver.hpp>
#include <vita/runtime/transaction/trace.hpp>
#include <vita/profiles/iq/lab.hpp>
namespace vita::bench::receiver {
struct Trace {std::uint32_t sid=0;std::uint64_t ordinal=0,rx_ns=0,validated_ns=0,app_ns=0,consumed_ns=0;unsigned phase=0,status=0;bool known=false;std::size_t bytes=0;std::uint64_t checksum=0;};
struct Stats {std::uint64_t checked=0,contexts=0,context_rejected=0,delivered=0,known=0,bytes=0,checksum=0,invalid=0,metadata_drops=0,pending_overflow=0,measured_packets=0,measured_samples=0,measured_gaps=0,measured_overlap=0,first_ordinal=0,next_ordinal=0;bool measured_seen=false;std::size_t waiting_peak=0;};
struct Config {std::array<adapters::posix_udp::Address,3> local{},sender{};std::size_t streams=4;std::uint64_t rate=1000000,warmup_ns=1000000000,duration_ns=10000000000;};
class Receiver {
  struct Pending {bool used=false;std::uint64_t ordinal=0,rx=0,validated=0;};
  struct Stream {
    Receiver* owner;std::uint32_t sid;std::uint64_t ingress=0;Stats stats;std::array<Pending,64> pending;
    runtime::context::ContextReceiver<128,64> receiver;
    Stream(Receiver& host,std::uint32_t id):owner(&host),sid(id),receiver(host.quota_,{this,deliver,drop},1,codec::Tsi::gps,false,id,profiles::iq::payload_format(profiles::iq::SampleFormat::iq16)){}
    static void before(void* p,const codec::Envelope&) noexcept {static_cast<Stream*>(p)->ingress=runtime::transaction::steady_trace_ns(nullptr);}
    static void drop(void* p,runtime::context::Confidence) noexcept {++static_cast<Stream*>(p)->stats.metadata_drops;}
    static void deliver(void* p,const runtime::context::BorrowedSignalRx& signal) noexcept {
      auto& self=*static_cast<Stream*>(p);const auto app=runtime::transaction::steady_trace_ns(nullptr);
      auto ordinal=sample_ordinal(signal.sample_time,self.owner->config_.rate);if(!ordinal){++self.stats.invalid;return;}
      Pending* pending=nullptr;for(auto& item:self.pending)if(item.used&&item.ordinal==*ordinal){pending=&item;break;}
      if(!pending){++self.stats.invalid;return;}
      auto bytes=signal.fragment(0);const bool known=signal.metadata.confidence==runtime::context::Confidence::known&&signal.metadata.valid_data;
      bool valid=bytes&&signal.fragment_count()==1&&bytes->size()==payload_bytes&&known;
      if(valid)valid=std::equal(bytes->begin(),bytes->end(),self.owner->canonical_.begin());
      const auto hash=valid?checksum(*bytes):0;const auto phase=self.owner->phase(pending->rx);
      self.owner->emit({self.sid,*ordinal,pending->rx,pending->validated,app,runtime::transaction::steady_trace_ns(nullptr),phase,valid?0u:2u,known,bytes?bytes->size():0,hash});pending->used=false;
      if(!valid){++self.stats.invalid;return;}
      ++self.stats.delivered;++self.stats.known;self.stats.bytes+=bytes->size();self.stats.checksum^=hash;
      if(phase==1){++self.stats.measured_packets;self.stats.measured_samples+=pairs;if(self.stats.measured_seen){if(*ordinal>self.stats.next_ordinal)self.stats.measured_gaps+=*ordinal-self.stats.next_ordinal;else if(*ordinal<self.stats.next_ordinal)self.stats.measured_overlap+=self.stats.next_ordinal-*ordinal;}else{self.stats.first_ordinal=*ordinal;self.stats.measured_seen=true;}self.stats.next_ordinal=*ordinal+pairs;}
    }
    static void receive(void* p,const codec::PacketView& packet,const memory::RxEnvelope& payload) noexcept {
      auto& self=*static_cast<Stream*>(p);const auto& envelope=packet.envelope.envelope;auto now=runtime::timing::MonoTime{self.ingress};
      if(envelope.type==codec::PacketType::context){
        bool valid=true;for(std::size_t i=0;i<packet.fields.size();++i){const auto& field=packet.fields[i];if(field.id==SampleRate::id||field.id==DataPayloadFormat::id){auto value=field.value();if(!value){valid=false;break;}if(field.id==SampleRate::id){auto* rate=std::get_if<Hertz>(&*value);valid&=rate&&rate->q20==(static_cast<std::int64_t>(self.owner->config_.rate)<<20);}else{auto* format=std::get_if<PayloadFormat>(&*value);valid&=format&&*format==profiles::iq::payload_format(profiles::iq::SampleFormat::iq16);}}}
        auto accepted=valid?self.receiver.receive_context(packet,1,now):Result<void>{std::unexpected(Error{ErrorCode::invalid_argument})};if(accepted)++self.stats.contexts;else ++self.stats.context_rejected;return;
      }
      if(envelope.timestamp.tsi!=codec::Tsi::gps||envelope.timestamp.tsf!=codec::Tsf::picoseconds||envelope.trailer||packet.envelope.payload.size()!=payload_bytes){++self.stats.invalid;return;}
      auto ordinal=sample_ordinal({envelope.timestamp.integer,envelope.timestamp.fractional},self.owner->config_.rate);if(!ordinal||*ordinal%16){++self.stats.invalid;return;}
      ++self.stats.checked;if(!self.owner->first_rx_)self.owner->first_rx_=self.ingress;
      Pending* selected=nullptr;for(auto& item:self.pending)if(!item.used){selected=&item;break;}
      if(!selected){++self.stats.pending_overflow;self.owner->emit({self.sid,*ordinal,self.ingress,runtime::transaction::steady_trace_ns(nullptr),0,0,self.owner->phase(self.ingress),3,false,0,0});return;}
      *selected={true,*ordinal,self.ingress,runtime::transaction::steady_trace_ns(nullptr)};auto accepted=self.receiver.receive_data(payload,{envelope.timestamp.integer,envelope.timestamp.fractional},1,now);self.stats.waiting_peak=std::max(self.stats.waiting_peak,self.receiver.waiting());
      if(!accepted){++self.stats.invalid;self.owner->emit({self.sid,*ordinal,self.ingress,selected->validated,0,0,self.owner->phase(self.ingress),2,false,0,0});selected->used=false;}
    }
  };
  Config config_;ExternalPools pools_;runtime::AdmissionPool admission_{runtime::AdmissionPool::reference_capacities()};runtime::RouteRegistry<128> routes_;runtime::CounterRegistry<128> counters_;memory::RetentionQuota quota_{256};
  std::array<std::optional<Stream>,4> streams_;std::unique_ptr<adapters::posix_udp::Udp<>> adapter_;std::array<std::byte,payload_bytes> canonical_{};std::uint64_t first_rx_=0;
  void* trace_context_=nullptr;void(*trace_)(void*,const Trace&) noexcept=nullptr;
  void emit(const Trace& value) noexcept{if(trace_)trace_(trace_context_,value);}
public:
  Receiver(Config config,ExternalPools pools):config_(config),pools_(std::move(pools)){}
  static Result<std::unique_ptr<Receiver>> create(Config config,ExternalPools pools,void* context,void(*trace)(void*,const Trace&) noexcept){
    if(!config.streams||config.streams>4||!config.rate||config.rate>100000000||!config.duration_ns||config.duration_ns>86400000000000ULL||config.warmup_ns>3600000000000ULL)return std::unexpected(Error{ErrorCode::invalid_argument});
    for(const auto& sender:config.sender)if(!sender.canonical()||!sender.port)return std::unexpected(Error{ErrorCode::invalid_argument});
    auto out=std::unique_ptr<Receiver>(new Receiver(config,std::move(pools)));out->trace_context_=context;out->trace_=trace;auto filled=canonical(out->canonical_);if(!filled)return std::unexpected(filled.error());
    for(std::size_t i=0;i<config.streams;++i){auto& stream=out->streams_[i].emplace(*out,i+1);for(auto type:{codec::PacketType::signal,codec::PacketType::context}){runtime::Route route;route.key.source={1,1};route.key.stream_id=i+1;route.key.type=type;route.key.packet_class=envelope(i+1,type,0).class_id;route.context=&stream;route.receive=Stream::receive;route.before_decode=Stream::before;if(type==codec::PacketType::signal)route.minimum_payload_bytes=route.maximum_payload_bytes=payload_bytes;auto added=out->routes_.add(route);if(!added)return std::unexpected(added.error());}}
    out->routes_.freeze();adapters::posix_udp::Config socket;for(std::size_t i=0;i<3;++i)socket.sockets[i].bind=config.local[i];auto adapter=adapters::posix_udp::Udp<>::create(socket,{out->pools_.rx_data,out->pools_.rx_control,out->pools_.rx_cancellation},out->admission_,out->routes_,out->counters_);if(!adapter)return std::unexpected(adapter.error());out->adapter_=std::move(*adapter);adapters::posix_udp::PeerBinding peer;peer.local_source={2,1};peer.remote_source={1,1};peer.remote=config.sender;auto added=out->adapter_->add_peer(peer);if(!added)return std::unexpected(added.error());return out;
  }
  unsigned phase(std::uint64_t received) const noexcept {if(!first_rx_||received<first_rx_+config_.warmup_ns)return 0;return received<first_rx_+config_.warmup_ns+config_.duration_ns?1:2;}
  void progress(std::uint64_t now) noexcept {adapter_->begin_cycle();for(unsigned work=0;work<96;++work){auto event=adapter_->progress_next();if(!event||!*event)break;}for(auto& stream:streams_)if(stream){stream->receiver.progress({now});for(auto& pending:stream->pending)if(pending.used&&now>=pending.rx&&now-pending.rx>=10000000ULL){emit({stream->sid,pending.ordinal,pending.rx,pending.validated,0,0,phase(pending.rx),1,false,0,0});pending.used=false;}}}
  std::uint64_t first_rx() const noexcept{return first_rx_;}
  const Stats& stats(std::size_t index) const noexcept{return streams_[index]->stats;}
  const auto& adapter() const noexcept{return *adapter_;}
  static constexpr std::size_t object_bytes() noexcept{return sizeof(Receiver);}
  static constexpr std::size_t auxiliary_bytes() noexcept{return runtime::AdmissionPool::metadata_bytes()+adapters::posix_udp::Udp<>::metadata_bytes()+sizeof(memory::detail::QuotaState)*5+128*7;}
};
} // namespace vita::bench::receiver
