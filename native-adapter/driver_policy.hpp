#pragma once
#include "r63_seat_reader.hpp"
#include <functional>

namespace bf4_takeover {

// Called only after the original R63 base CanEnter returned false, from the
// real native interaction/seat-selection path on its verified gameplay thread.
// This does not establish actor life or authenticate input: the runtime host
// must preserve those native event gates. A soldier pointer is not proof of life.
// known_bot must positively identify a current bot through lifecycle evidence.
class DriverPolicy {
public:
    DriverPolicy(const CurrentServerImageGuard& guard,
                 std::function<bool(std::uintptr_t)> known_bot,
                 std::function<bool()> verified_gameplay_thread);

    // Re-evaluates the recovered base policy with only this extension:
    // a positively identified allied mod bot in the requested seat counts as an AI occupant.
    // No AI flags, occupancy fields, arguments or return values are written.
    // The caller must still run/preserve the vehicle's derived permission gates.
    bool eligible(std::uintptr_t vehicle,std::uintptr_t requester,
                  std::uint32_t seat,bool native_spawn_context=false) const;

private:
    const CurrentServerImageGuard& guard_;
    R63SeatReader reader_;
    std::function<bool(std::uintptr_t)> known_bot_;
    std::function<bool()> verified_gameplay_thread_;
};
}
