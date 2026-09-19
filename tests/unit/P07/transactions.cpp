#include "support.hpp"
int main(){
    AdmissionPool pool(AdmissionPool::reference_capacities());VirtualBackend<> backend;
    EngineOptions options;options.profile=Profile::generic_virtual_test;options.external_retention=true;
    Engine<4> engine(pool,backend.binding(),initial(),options);RetentionStore<8,32768> store(pool);TransactionManager<4,8,32768> manager(engine,store,pool);
    OperationContext now;now.operation=7;PeerSession peer{11,1};
    auto original=make(false,true);auto accepted=manager.accept(original.view(),now,peer);assert(accepted&&accepted->kind==DuplicateKind::fresh);
    auto retry=manager.accept(original.view(),now,peer);assert(retry&&retry->kind==DuplicateKind::active&&retry->token==accepted->token);assert(manager.release(retry->token));
    auto conflict=make(false,true,3);auto rejected=manager.accept(conflict.view(),now,peer);assert(!rejected&&rejected.error().code==ErrorCode::identity_conflict&&backend.begins()==0);
    assert(manager.progress(now));assert(backend.begins()==1);assert(backend.complete_next());assert(manager.progress(now));assert(backend.begins()==2); // A executed, B armed before cutoff.
    auto cancel=make(true,true);auto cancellation=manager.accept(cancel.view(),now,peer);assert(cancellation);assert(manager.progress(now));assert(backend.pending()==0&&backend.writes()==1);
    auto cx=manager.response(cancellation->token,0);assert(cx&&*cx&&(**cx).cancellation&&(**cx).partial&&(**cx).scheduled_or_executed);
    auto ox=manager.response(accepted->token,1);assert(ox&&*ox&&!(**ox).cancellation&&(**ox).partial&&!(**ox).scheduled_or_executed);
    auto different=make(true,false);auto conflict_cancel=manager.accept(different.view(),now,peer);assert(!conflict_cancel&&conflict_cancel.error().code==ErrorCode::identity_conflict);
    auto replay=manager.accept(cancel.view(),now,peer);assert(replay&&replay->kind==DuplicateKind::replay);auto same=manager.response(replay->token,0);assert(same&&*same&&(**same).partial&&(**same).scheduled_or_executed);assert(manager.release(replay->token));
    std::array<std::byte,256> encoded{};auto n=encode_response(**cx,encoded,3);assert(n);auto parsed=codec::decode_packet(Bytes{encoded}.first(*n),codec::DecodeOptions{codec::RequestContext{0x090d0000}});assert(parsed&&parsed->envelope.envelope.cancel&&parsed->envelope.envelope.packet_count==3);
    assert(manager.release(accepted->token));assert(manager.release(cancellation->token));assert(pool.used(Resource::duplicate_entry)==1);store.expire({29'999'999'999});assert(store.size()==1);store.expire({30'000'000'000});assert(store.size()==0&&pool.used(Resource::duplicate_bytes)==0);
    // First cancellation after engine release does not execute or query a stale handle.
    auto command=make();auto a=manager.accept(command.view(),now,peer);assert(a);assert(manager.progress(now));assert(backend.complete_next());assert(manager.progress(now));auto c=make(true);auto b=manager.accept(c.view(),now,peer);assert(b);now.monotonic.ns=20'000'000'000;assert(manager.progress(now));auto failed=manager.response(b->token,0);assert(failed&&*failed&&(**failed).partial&&!(**failed).scheduled_or_executed);assert(manager.release(a->token));assert(manager.release(b->token));store.expire({49'999'999'999});assert(store.size()==1);store.expire({50'000'000'000});assert(store.size()==0);
}
