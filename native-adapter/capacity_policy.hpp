#pragma once
#include <cstdint>
#include <vector>
namespace bf4_takeover {
// Shared by the production derived-permission hook and regression fixtures.
// The event callback owns its native message until fit() finishes, preserving
// the original message cleanup order without sending the event twice.
template<class Original,class BasePermission,class Event,class Fits>
bool derived_capacity_permission(bool eligible,int limit,Original original,
                                 BasePermission base_permission,Event event,Fits fits) {
    if (!eligible || limit<=0) return original();
    if (!base_permission()) return false;
    return event(fits);
}

inline bool replacement_requires_eviction(int limit,
                                          const std::vector<std::uintptr_t>& occupants,
                                          std::uintptr_t incoming) {
    if (limit<=0 || !incoming) return false;
    int occupied_without_incoming=0;
    for (const auto player:occupants)
        if (player && player!=incoming) ++occupied_without_incoming;
    // Keeping the bot and then adding an outside/dead incoming player would
    // exceed the cap. Existing incoming occupants do not add another player.
    return occupied_without_incoming>=limit;
}

inline bool replacement_fits_capacity(int limit,const std::vector<std::uintptr_t>& occupants,
                                     std::uintptr_t incoming,std::uintptr_t replaced_bot) {
    if (!incoming || !replaced_bot || incoming==replaced_bot || limit<=0) return false;
    int others=0;
    for (const auto player:occupants)
        if (player && player!=incoming && player!=replaced_bot) ++others;
    return others<limit;
}
}
