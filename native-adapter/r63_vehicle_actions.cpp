#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include "r63_vehicle_actions.hpp"
#include <array>
#include <cstring>
#include <utility>

namespace bf4_takeover {
namespace {
constexpr std::uintptr_t entry_rva=0xbcaf50;
constexpr std::uintptr_t exit_rva=0xbd0510;
constexpr unsigned char entry_prefix[]={0x48,0x8b,0xc4,0x41,0x56,0x48,0x83,0xec,
                                       0x60,0x48,0xc7,0x40,0xb8,0xfe,0xff,0xff};
constexpr unsigned char exit_prefix[]={0x48,0x8b,0xc4,0x57,0x48,0x83,0xec,0x60,
                                      0x48,0xc7,0x40,0xb8,0xfe,0xff,0xff,0xff};
using NativeEntry=bool(__fastcall*)(void* player,void* vehicle,std::uint32_t seat);
using NativeExit=bool(__fastcall*)(void* player,unsigned char flag0,unsigned char flag1);
}
R63VehicleActions::R63VehicleActions(const CurrentServerImageGuard& guard,
    const R63SeatReader& reader,R63ActionHost host) : guard_(guard),reader_(reader),host_(std::move(host)) {}
bool R63VehicleActions::allowed(std::uintptr_t player,std::uintptr_t vehicle) const {
    return player && vehicle && host_.gameplay_thread_id &&
        GetCurrentThreadId()==host_.gameplay_thread_id && host_.mutation_allowed &&
        host_.trusted_bot && guard_.validate() && host_.mutation_allowed(player,vehicle);
}
bool R63VehicleActions::function_matches(std::uintptr_t rva,const unsigned char* prefix,
    std::size_t length,std::uintptr_t& address) const {
    if (!guard_.validate() || length>16 ||
        !r63_layout_detail::checked_address(guard_.module_base(),rva,address)) return false;
    MEMORY_BASIC_INFORMATION memory{};
    if (!VirtualQuery(reinterpret_cast<LPCVOID>(address),&memory,sizeof(memory)) ||
        memory.State!=MEM_COMMIT || (memory.Protect&(PAGE_GUARD|PAGE_NOACCESS)) ||
        !(memory.Protect&(PAGE_EXECUTE|PAGE_EXECUTE_READ|PAGE_EXECUTE_READWRITE|PAGE_EXECUTE_WRITECOPY))) return false;
    std::array<unsigned char,16> actual{};
    SIZE_T count=0;
    return ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<LPCVOID>(address),actual.data(),length,&count) &&
        count==length && std::memcmp(actual.data(),prefix,length)==0;
}
bool R63VehicleActions::enter(std::uintptr_t player,std::uintptr_t vehicle,std::uint32_t seat) const {
    if (seat>=64 || !allowed(player,vehicle)) return false;
    const auto p=reader_.player(player);
    const auto v=reader_.vehicle(vehicle);
    if (!p || !p->soldier || !v || seat>=v->seats.size() || v->seats[seat].player ||
        (p->attached_vehicle && p->attached_vehicle!=vehicle)) return false;
    std::uintptr_t function=0;
    if (!function_matches(entry_rva,entry_prefix,sizeof(entry_prefix),function)) return false;
    // Native entry checks vacancy before detach and handles attachment/replication.
    // Never synthesize vehicle/player pointers or write attachment fields directly.
    const bool native_result=reinterpret_cast<NativeEntry>(function)(
        reinterpret_cast<void*>(player),reinterpret_cast<void*>(vehicle),seat);
    const auto after=reader_.player(player);
    const auto seats=reader_.vehicle(vehicle);
    return native_result && after && seats && after->attached_vehicle==vehicle &&
        after->attached_seat==seat && seat<seats->seats.size() && seats->seats[seat].player==player;
}
bool R63VehicleActions::switch_seat(std::uintptr_t human,std::uintptr_t vehicle,std::uint32_t seat) const {
    if (!allowed(human,vehicle) || host_.trusted_bot(human)) return false;
    const auto p=reader_.player(human);
    if (!p || p->attached_vehicle!=vehicle || p->attached_seat==seat) return false;
    // Same native entry function performs internal detach/attach, not physical exit.
    return enter(human,vehicle,seat);
}
bool R63VehicleActions::exit_bot(std::uintptr_t bot,std::uintptr_t vehicle,std::uint32_t seat) const {
    if (seat>=64 || !allowed(bot,vehicle) || !host_.trusted_bot(bot)) return false;
    const auto p=reader_.player(bot);
    const auto v=reader_.vehicle(vehicle);
    if (!p || !p->soldier || p->attached_vehicle!=vehicle || p->attached_seat!=seat ||
        !v || seat>=v->seats.size() || v->seats[seat].player!=bot) return false;
    std::uintptr_t function=0;
    if (!function_matches(exit_rva,exit_prefix,sizeof(exit_prefix),function)) return false;
    const bool native_result=reinterpret_cast<NativeExit>(function)(reinterpret_cast<void*>(bot),0,1);
    const auto after=reader_.player(bot);
    const auto seats=reader_.vehicle(vehicle);
    return native_result && after && !after->attached_vehicle && seats &&
        seat<seats->seats.size() && !seats->seats[seat].player;
}
bool R63VehicleActions::can_enter_from_world(std::uintptr_t human,std::uintptr_t vehicle) const {
    if (!allowed(human,vehicle) || !host_.world_entry_allowed || host_.trusted_bot(human)) return false;
    const auto p=reader_.player(human);
    return p && p->soldier && !p->attached_vehicle && host_.world_entry_allowed(human,vehicle);
}
}
