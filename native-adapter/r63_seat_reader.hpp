#pragma once
#include "server_image_guard.hpp"
#include <cstdint>
#include <optional>
#include <vector>

namespace bf4_takeover {
struct R63Seat {
    std::uintptr_t entry = 0;
    std::uintptr_t player = 0;
};
struct R63VehicleSeats {
    std::uintptr_t vehicle = 0;
    std::vector<R63Seat> seats;
};
struct R63PlayerSeat {
    std::uintptr_t player = 0;
    std::uintptr_t soldier = 0; // Nonzero is NOT a proof of life.
    std::int32_t team = 0;
    std::uintptr_t attached_vehicle = 0;
    std::uint32_t attached_seat = 0;
    std::uintptr_t current_controlled = 0;
    std::uint32_t current_seat = 0;
};

// Read-only, current-process reader for the statically recovered R63 layout.
// Call on the gameplay thread; this is not an atomic engine snapshot.
class R63SeatReader {
public:
    explicit R63SeatReader(const CurrentServerImageGuard& guard) : guard_(guard) {}
    std::optional<R63VehicleSeats> vehicle(std::uintptr_t vehicle) const;
    std::optional<R63PlayerSeat> player(std::uintptr_t player) const;
private:
    const CurrentServerImageGuard& guard_;
};

// Parsing/read helpers are isolated so tests can use real synthetic memory
// buffers without making the supported-game image guard accept a test process.
// These are unguarded READ-ONLY helpers, not engine callbacks or native calls.
namespace r63_layout_detail {
bool checked_address(std::uintptr_t base, std::uintptr_t offset, std::uintptr_t& result);
std::optional<R63VehicleSeats> read_vehicle(std::uintptr_t vehicle);
std::optional<R63PlayerSeat> read_player(std::uintptr_t player);
}
}
