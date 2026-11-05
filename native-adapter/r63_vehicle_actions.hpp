#pragma once
#include "r63_seat_reader.hpp"
#include <functional>

namespace bf4_takeover {
struct R63ActionHost {
    // Issued by the verified gameplay callback installer, never guessed from
    // process state or supplied by a client/menu. This module installs no hooks.
    std::uint32_t gameplay_thread_id = 0;
    std::function<bool(std::uintptr_t actor,std::uintptr_t vehicle)> mutation_allowed;
    std::function<bool(std::uintptr_t player)> trusted_bot;
    // Still unbound: must implement actual engine interaction range/rules.
    std::function<bool(std::uintptr_t human,std::uintptr_t vehicle)> world_entry_allowed;
};

// Real x64 native entry/exit bindings for the exact R63 image. Not a DLL or hook
// installer. Host remains responsible for identity lifecycle, actor eligibility,
// seat permissions and serialized gameplay-thread invocation.
class R63VehicleActions {
public:
    R63VehicleActions(const CurrentServerImageGuard& guard,const R63SeatReader& reader,
                      R63ActionHost host);
    bool enter(std::uintptr_t player,std::uintptr_t vehicle,std::uint32_t seat) const;
    bool switch_seat(std::uintptr_t human,std::uintptr_t vehicle,std::uint32_t seat) const;
    bool exit_bot(std::uintptr_t bot,std::uintptr_t vehicle,std::uint32_t seat) const;
    bool can_enter_from_world(std::uintptr_t human,std::uintptr_t vehicle) const;
private:
    const CurrentServerImageGuard& guard_;
    const R63SeatReader& reader_;
    R63ActionHost host_;
    bool allowed(std::uintptr_t player,std::uintptr_t vehicle) const;
    bool function_matches(std::uintptr_t rva,const unsigned char* prefix,std::size_t length,
                          std::uintptr_t& address) const;
};
}
