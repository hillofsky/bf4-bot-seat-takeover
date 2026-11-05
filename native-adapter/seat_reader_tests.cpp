#include "r63_seat_reader.hpp"
#include <array>
#include <cstring>
#include <iostream>
#include <limits>
#include <stdexcept>
using namespace bf4_takeover;
#define CHECK(x) do { if (!(x)) throw std::runtime_error(#x); } while(false)
template<class T,std::size_t N> void put(std::array<unsigned char,N>& buffer,std::size_t offset,T value) {
    CHECK(offset+sizeof(T)<=N); std::memcpy(buffer.data()+offset,&value,sizeof(value));
}
int main() {
    try {
        std::array<unsigned char,0x1500> player{};
        std::array<unsigned char,0x200> vehicle{},entry{};
        const auto p=reinterpret_cast<std::uintptr_t>(player.data());
        const auto v=reinterpret_cast<std::uintptr_t>(vehicle.data());
        const auto e=reinterpret_cast<std::uintptr_t>(entry.data());
        std::array<std::uintptr_t,1> entries{{e}};
        const auto begin=reinterpret_cast<std::uintptr_t>(entries.data());
        put(vehicle,0x1a0,begin); put(vehicle,0x1a8,begin+8);
        put(entry,0xb8,p); put(player,0x14c8,v); put(player,0x14c0,std::uint32_t{0});
        put(player,0x13cc,std::int32_t{2}); put(player,0x14a8,std::uintptr_t{0x1234});
        // Deliberately different: attached and current-controlled fields are not conflated.
        put(player,0x14e8,std::uintptr_t{0x4567}); put(player,0x14e0,std::uint32_t{7});
        const auto snapshot=r63_layout_detail::read_player(p);
        CHECK(snapshot && snapshot->attached_vehicle==v && snapshot->current_controlled==0x4567);
        CHECK(snapshot->soldier==0x1234 && snapshot->team==2 && snapshot->current_seat==7);
        CHECK(r63_layout_detail::read_vehicle(v)->seats.at(0).player==p);
        CurrentServerImageGuard guard; R63SeatReader reader(guard);
        CHECK(!reader.player(p) && !reader.vehicle(v));
        put(entry,0xb8,std::uintptr_t{0x9999}); CHECK(!r63_layout_detail::read_player(p));
        put(entry,0xb8,p); put(player,0x14c0,std::uint32_t{1});
        CHECK(!r63_layout_detail::read_player(p)); put(player,0x14c0,std::uint32_t{0});
        put(vehicle,0x1a8,begin+65*8); CHECK(!r63_layout_detail::read_vehicle(v));
        put(vehicle,0x1a8,begin-8); CHECK(!r63_layout_detail::read_vehicle(v));
        put(vehicle,0x1a8,begin+1); CHECK(!r63_layout_detail::read_vehicle(v));
        put(vehicle,0x1a8,begin+8); entries[0]=0;
        CHECK(!r63_layout_detail::read_vehicle(v));
        entries[0]=(std::numeric_limits<std::uintptr_t>::max)()-2;
        CHECK(!r63_layout_detail::read_vehicle(v));
        CHECK(!r63_layout_detail::read_player(1));
        std::uintptr_t sum=0;
        CHECK(!r63_layout_detail::checked_address((std::numeric_limits<std::uintptr_t>::max)()-2,3,sum));
        CHECK(!r63_layout_detail::checked_address(0,1,sum));
        put(vehicle,0x1a0,std::uintptr_t{0}); put(vehicle,0x1a8,std::uintptr_t{0});
        CHECK(r63_layout_detail::read_vehicle(v)->seats.empty());
        put(player,0x14c8,std::uintptr_t{0}); CHECK(r63_layout_detail::read_player(p));
        std::cout<<"PASS: read-only R63 synthetic memory, attached-seat consistency, bounded vectors, bad pointers/overflow, guard prerequisite\n";
    } catch(const std::exception& e) { std::cerr<<"FAIL: "<<e.what()<<'\n'; return 1; }
}
