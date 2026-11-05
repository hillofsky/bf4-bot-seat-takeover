#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include "r63_seat_reader.hpp"
#include <array>
#include <limits>

namespace bf4_takeover {
namespace r63_layout_detail {
bool checked_address(std::uintptr_t base, std::uintptr_t offset, std::uintptr_t& result) {
    if (!base || offset > (std::numeric_limits<std::uintptr_t>::max)()-base) return false;
    result=base+offset; return true;
}
namespace {
bool read_bytes(std::uintptr_t address, void* destination, std::size_t size) {
    if (!address || size > (std::numeric_limits<std::uintptr_t>::max)()-address) return false;
    SIZE_T count=0;
    return ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<LPCVOID>(address),
                             destination,size,&count) && count==size;
}
template<class T> bool field(std::uintptr_t base, std::uintptr_t offset, T& value) {
    std::uintptr_t address=0;
    return checked_address(base,offset,address) && read_bytes(address,&value,sizeof(value));
}
bool vector_bounds(std::uintptr_t vehicle,std::uintptr_t& begin,std::uintptr_t& end) {
    return field(vehicle,0x1a0,begin) && field(vehicle,0x1a8,end) && end>=begin &&
        (end-begin)%sizeof(std::uintptr_t)==0 &&
        (end-begin)/sizeof(std::uintptr_t)<=64 && ((begin!=0) || end==0);
}
}
std::optional<R63VehicleSeats> read_vehicle(std::uintptr_t vehicle) {
    static_assert(sizeof(std::uintptr_t)==8,"R63 is x64 only");
    std::uintptr_t begin=0,end=0;
    if (!vector_bounds(vehicle,begin,end)) return {};
    const auto count=(end-begin)/sizeof(std::uintptr_t);
    std::array<std::uintptr_t,64> entries{};
    if (count && !read_bytes(begin,entries.data(),count*sizeof(std::uintptr_t))) return {};
    R63VehicleSeats snapshot; snapshot.vehicle=vehicle;
    for (std::size_t i=0;i<count;++i) {
        std::uintptr_t occupant=0;
        if (!entries[i] || !field(entries[i],0xb8,occupant)) return {};
        snapshot.seats.push_back(R63Seat{entries[i],occupant});
    }
    // Reject a vector that changed while reading, even on an accidental worker.
    std::uintptr_t check_begin=0,check_end=0;
    if (!vector_bounds(vehicle,check_begin,check_end) || begin!=check_begin || end!=check_end) return {};
    return snapshot;
}
std::optional<R63PlayerSeat> read_player(std::uintptr_t player) {
    R63PlayerSeat snapshot; snapshot.player=player;
    if (!field(player,0x14a8,snapshot.soldier) || !field(player,0x13cc,snapshot.team) ||
        !field(player,0x14c8,snapshot.attached_vehicle) || !field(player,0x14c0,snapshot.attached_seat) ||
        !field(player,0x14e8,snapshot.current_controlled) || !field(player,0x14e0,snapshot.current_seat)) return {};
    if (snapshot.attached_vehicle) {
        const auto seats=read_vehicle(snapshot.attached_vehicle);
        if (!seats || snapshot.attached_seat>=seats->seats.size() ||
            seats->seats[snapshot.attached_seat].player!=player) return {};
        std::uintptr_t occupant=0;
        if (!field(seats->seats[snapshot.attached_seat].entry,0xb8,occupant) || occupant!=player) return {};
    }
    std::uintptr_t attached=0;
    std::uint32_t seat=0;
    if (!field(player,0x14c8,attached) || !field(player,0x14c0,seat) ||
        attached!=snapshot.attached_vehicle || seat!=snapshot.attached_seat) return {};
    return snapshot;
}
}
std::optional<R63VehicleSeats> R63SeatReader::vehicle(std::uintptr_t address) const {
    if (!guard_.validate()) return {};
    auto result=r63_layout_detail::read_vehicle(address);
    if (!guard_.validate()) return {};
    return result;
}
std::optional<R63PlayerSeat> R63SeatReader::player(std::uintptr_t address) const {
    if (!guard_.validate()) return {};
    auto result=r63_layout_detail::read_player(address);
    if (!guard_.validate()) return {};
    return result;
}
}
