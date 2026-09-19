#include <vita/codec/extensions.hpp>
#include <cassert>
#include <array>
#include <algorithm>
using namespace vita;using namespace vita::codec;using namespace vita::codec::extensions;
struct Counts{unsigned parsed=0,admitted=0,executed=0;bool allow=false;};
static Result<void> parse(void*p,const EnvelopeView&v,WorkBudget&w)noexcept{++static_cast<Counts*>(p)->parsed;auto charge=w.consume(2);if(!charge)return charge;if(v.payload.size()!=4||v.payload[0]!=std::byte{0xde})return std::unexpected(Error{ErrorCode::invalid_argument});return {};}
static Result<void> admit(void*p,ClassKey,const EnvelopeView&)noexcept{auto&c=*static_cast<Counts*>(p);++c.admitted;if(!c.allow)return std::unexpected(Error{ErrorCode::capacity_exhausted});return {};}
static Result<void> execute(void*p,const EnvelopeView&)noexcept{++static_cast<Counts*>(p)->executed;return {};}
static Result<std::size_t> size(void*,const void*,WorkBudget&)noexcept{return 4;}
static Result<void> encode(void*,const void*,MutableBytes bytes,WorkBudget&)noexcept{bytes[0]=std::byte{0xde};bytes[1]=std::byte{0xad};bytes[2]=std::byte{0xbe};bytes[3]=std::byte{0xef};return {};}
static Result<void> fullwire(void*,const EnvelopeView&view,WorkBudget&)noexcept{auto decoded=decode_envelope(view.wire);if(!decoded||decoded->payload_offset!=view.payload_offset||decoded->payload.data()!=view.payload.data()||decoded->envelope.stream_id!=view.envelope.stream_id)return std::unexpected(Error{ErrorCode::invalid_argument});return {};}
static auto literal(){std::array<std::uint32_t,5>w{0x58000005,0x12345678,0x00ff0001,0x43211234,0xdeadbeef};std::array<std::byte,20>b{};for(unsigned i=0;i<5;++i)for(unsigned j=0;j<4;++j)b[4*i+j]=std::byte((w[i]>>(24-j*8))&255);return b;}
int main(){Counts c;Descriptor d;d.key={PacketType::extension_context,0xff0001,0x4321,0x1234};d.context=&c;d.min_payload=d.max_payload=4;d.validate=parse;d.dispatch=execute;Registry<1> r;assert(r.add(d));assert(!r.add(d));auto wire=literal();WorkBudget before;assert(!r.validate(wire,before));r.freeze();assert(!r.add(d));WorkBudget work(4);auto v=r.validate(wire,work);assert(v&&!v->opaque()&&work.remaining()==0&&c.parsed==1&&c.admitted==0&&c.executed==0);assert(v->envelope().envelope.stream_id==0x12345678);assert(!r.dispatch(*v,{}));assert(!r.dispatch(*v,{&c,admit}));assert(c.admitted==1&&c.executed==0);c.allow=true;assert(r.dispatch(*v,{&c,admit}));assert(r.dispatch(*v,{&c,admit}));assert(c.admitted==3&&c.executed==2);
 Registry<1> other;assert(other.add(d));other.freeze();assert(!other.dispatch(*v,{&c,admit}));assert(c.admitted==3);
 for(unsigned n=0;n<wire.size();++n){WorkBudget w;auto p=r.validate(Bytes{wire}.first(n),w);assert(!p&&c.parsed==1);}
 auto unknown=wire;unknown[15]=std::byte{0x35};WorkBudget uw;auto opaque=r.validate(unknown,uw);assert(opaque&&opaque->opaque()&&c.parsed==1);assert(!r.dispatch(*opaque,{&c,admit}));
 auto malformed=wire;malformed[16]=std::byte{0};WorkBudget mw;assert(!r.validate(malformed,mw));assert(c.parsed==2&&c.admitted==3&&c.executed==2);
 WorkBudget exhausted(1);assert(!r.validate(wire,exhausted));assert(c.parsed==2&&exhausted.remaining()==1);WorkBudget callbacklimited(3);assert(!r.validate(wire,callbacklimited));assert(c.parsed==3&&callbacklimited.remaining()==1);
 Registry<1> consistent;auto full=d;full.validate=fullwire;full.measure=size;full.encode=encode;assert(consistent.add(full));consistent.freeze();Envelope env;env.type=PacketType::extension_context;env.stream_id=0x12345678;env.class_id=ClassId{0xff0001,0x4321,0x1234,0};std::array<std::byte,20> output{};WorkBudget encoding;auto encoded=consistent.encode(env,nullptr,{},output,encoding);assert(encoded&&*encoded==wire.size()&&output==wire);WorkBudget checking;assert(consistent.validate(output,checking));
}
