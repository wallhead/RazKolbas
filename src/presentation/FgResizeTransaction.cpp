#include "rk/FgResizeTransaction.hpp"

namespace rk {
Result<HRESULT> FgResizeTransaction::resize(const FgResizeCall& call,
    std::uint64_t nextGeneration,const FgFenceProgress& progress) {
    if(!inputs_.canAdvanceGeneration(nextGeneration,progress))
        return Error{ErrorCode::Unavailable,
            "FG inputs have not completed all provider and Present fences"};
    const auto suspended=coordinator_.suspend();
    if(const auto error=std::get_if<Error>(&suspended))return *error;
    const auto result=lower_.resize(call);
    if(SUCCEEDED(result)&&!inputs_.advanceGeneration(nextGeneration,progress))
        return Error{ErrorCode::Conflict,
            "FG resize succeeded but its input generation could not advance"};
    return result;
}
}
