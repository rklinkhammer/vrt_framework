#include "../P07/support.hpp"
#include <vita/runtime/transaction/retention.hpp>
#include <cstdio>

int main() {
  AdmissionPool pool(AdmissionPool::reference_capacities());
  RetentionStore<> store(pool);
  std::array<RetentionToken, 3000> tokens{};
  std::uint64_t probes = 0;
  for (std::uint32_t i = 0; i < tokens.size(); ++i) {
    auto packet = make(false, i % 2, i + 1, i + 1);
    auto view = packet.view();
    auto key = transaction_key(view, 1, {1,1}); assert(key);
    auto added = store.reserve(*key, view, 3); assert(added);
    assert(store.last_placement_probes() <= i);
    probes += store.last_placement_probes();
    tokens[i] = added->token;
    assert(store.bind(added->token));
    AckRecord ack; ack.kind = AckKind::state; ack.time = {i, i};
    assert(store.append(added->token, ack));
    assert(store.complete(added->token, {i % 2 ? 1000u : 0u}));
    assert(store.release(added->token));
  }
  assert(store.size() == 3000);
  assert(probes == 3000ull * 2999 / 2);
  auto pinned = store.canonical(tokens[1]); assert(pinned);
  const auto* address = pinned->data(); const auto byte = (*pinned)[0];
  // Expire alternating original records and preserve the other immutable bytes.
  store.expire({30'000'000'000ull}); assert(store.size() == 1500);
  assert((*pinned)[0] == byte && pinned->data() == address);
  for (std::uint32_t i=0;i<1500;++i) {
    auto packet=make(false,false,2,4000+i);auto view=packet.view();
    auto key=transaction_key(view,1,{1,1});assert(key);
    auto added=store.reserve(*key,view,0);assert(added);
    assert(store.last_placement_probes() <= 3000);
    // Rollback returns precisely this hole without moving neighboring records.
    assert(store.rollback(added->token));
  }
  auto old=store.response(tokens[1],0);assert(old&&*old&&(**old).time==timing::ProtocolTime(1,1));
  store.expire({30'000'001'000ull});assert(store.size()==0);
  assert(pool.used(Resource::duplicate_entry)==0 && pool.used(Resource::duplicate_bytes)==0);
  // Original + immutable cancellation index records are both removed on expiry.
  auto original=make(false,true,2,9000);auto view=original.view();auto key=transaction_key(view,1,{1,1});assert(key);
  auto a=store.reserve(*key,view,3);assert(a&&store.bind(a->token));
  auto cancel=make(true,true,2,9000);auto c=store.reserve(*key,cancel.view(),2);assert(c);
  assert(store.rollback(c->token));c=store.reserve(*key,cancel.view(),2);assert(c&&store.bind(c->token));
  assert(store.complete(a->token,{0})&&store.complete(c->token,{5}));
  assert(store.release(a->token)&&store.release(c->token));
  store.expire({30'000'000'004ull});assert(store.size()==1);
  store.expire({30'000'000'005ull});assert(store.size()==0);
  std::printf("records=3000 total_placement_probes=%llu storage_bytes=%zu\n",(unsigned long long)probes,RetentionStore<>::storage_bytes());
}
