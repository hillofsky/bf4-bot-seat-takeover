#pragma once
#include <array>
#include <cstdint>
namespace bf4_takeover {
struct BotIdentitySnapshot {
    std::uintptr_t player=0,context=0,input_cookie=0;
    std::uint32_t id=0;
    std::array<char,64> name{};
    std::uint64_t observed_at=0;
};
struct BotIdentityLive {
    std::uintptr_t player=0,context_owner=0,roster_owner=0,input_cookie=0;
    std::uint32_t id=0;
    std::array<char,64> name{};
    std::uint64_t now=0;
};
inline bool matches_live_bot(const BotIdentitySnapshot& saved,const BotIdentityLive& live) {
    return saved.player && saved.context && saved.input_cookie && saved.name[0] &&
        saved.player==live.player && saved.player==live.context_owner && saved.player==live.roster_owner &&
        saved.id==live.id && saved.input_cookie==live.input_cookie && saved.name==live.name &&
        live.now>=saved.observed_at;
}
}
