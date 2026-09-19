#pragma once
#include <vita/runtime/execution/arena.hpp>
#include <vita/runtime/completion/ticket.hpp>
namespace vita::runtime {
template<std::size_t Count,std::size_t BytesPerSlot> struct AdmittedOperation {
    AdmissionBundle credits;
    typename SlotArena<Count,BytesPerSlot>::Lease storage;
    CompletionToken completion;
    template<std::size_t Tickets>
    static Result<AdmittedOperation> acquire(AdmissionPool& pool,SlotArena<Count,BytesPerSlot>& arena,
        CompletionArena<Tickets>& tickets,AdmissionRequest request,std::size_t required,std::uint64_t operation) noexcept {
        if(pool.capacity(Resource::completion)>Tickets || !request.counts[static_cast<std::size_t>(Resource::completion)] ||
           request.counts[static_cast<std::size_t>(Resource::completion)]!=1 || pool.capacity(Resource::transaction)>Count)
            return std::unexpected(Error{ErrorCode::invalid_argument});
        auto admitted=AdmittedStorage<Count,BytesPerSlot>::acquire(pool,arena,request,required);
        if(!admitted) return std::unexpected(admitted.error());
        auto ticket=tickets.reserve(operation); if(!ticket) return std::unexpected(ticket.error());
        return AdmittedOperation{std::move(admitted->credits),std::move(admitted->storage),std::move(*ticket)};
    }
};
} // namespace vita::runtime
