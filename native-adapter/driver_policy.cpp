#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include "driver_policy.hpp"
#include <cmath>
#include <utility>

namespace bf4_takeover {
namespace {
// Recovered from R63 RVA bcfc20 (ev_077213fc84127e1de49c5a922ed2e04305bf59a0a7bf94df9722a03d6b25aac6).
// Query ABIs require live validation by the host before enabling this policy.
using HealthQuery=float(__fastcall*)(void*);
// Assembly bcfc7b loads requester team into EDX before the indirect call.
using StateQuery=std::uint32_t(__fastcall*)(std::uint32_t,std::uint32_t);
using BooleanQuery=bool(__fastcall*)(void*);
using RoleQuery=std::uint32_t(__fastcall*)(void*);

template<class T> bool read_at(std::uintptr_t base,std::uintptr_t offset,T& value) {
    std::uintptr_t address=0,end=0;
    if (!r63_layout_detail::checked_address(base,offset,address) ||
        !r63_layout_detail::checked_address(address,sizeof(T)-1,end)) return false;
    SIZE_T count=0;
    return ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<LPCVOID>(address),
                             &value,sizeof(value),&count) && count==sizeof(value);
}
bool executable(std::uintptr_t address) {
    MEMORY_BASIC_INFORMATION memory{};
    return address && VirtualQuery(reinterpret_cast<LPCVOID>(address),&memory,sizeof(memory)) &&
        memory.State==MEM_COMMIT && !(memory.Protect&(PAGE_GUARD|PAGE_NOACCESS)) &&
        (memory.Protect&(PAGE_EXECUTE|PAGE_EXECUTE_READ|PAGE_EXECUTE_READWRITE|PAGE_EXECUTE_WRITECOPY));
}
bool fixed_function(std::uintptr_t base,std::uintptr_t rva,std::uintptr_t& result) {
    return r63_layout_detail::checked_address(base,rva,result) && executable(result);
}
}

DriverPolicy::DriverPolicy(const CurrentServerImageGuard& guard,
    std::function<bool(std::uintptr_t)> known_bot,std::function<bool()> verified_gameplay_thread)
    : guard_(guard),reader_(guard),known_bot_(std::move(known_bot)),
      verified_gameplay_thread_(std::move(verified_gameplay_thread)) {}

bool DriverPolicy::eligible(std::uintptr_t vehicle,std::uintptr_t requester,
                            std::uint32_t seat,bool native_spawn_context) const {
    try {
        if (!vehicle || !requester || !known_bot_ ||
            !verified_gameplay_thread_ || !verified_gameplay_thread_() ||
            !guard_.validate() || known_bot_(requester)) return false;
        const auto p=reader_.player(requester);
        const auto seats=reader_.vehicle(vehicle);
        if (!p || (!p->soldier && !native_spawn_context) || (p->team!=1 && p->team!=2) ||
            (p->attached_vehicle && p->attached_vehicle!=vehicle) ||
            !seats || seat>=seats->seats.size()) return false;
        const auto driver=seats->seats[seat].player;
        const auto entry=seats->seats[seat].entry;
        if (!driver || driver==requester || !entry || !known_bot_(driver)) return false;
        const auto bot=reader_.player(driver);
        if (!bot || !bot->soldier || bot->team!=p->team ||
            bot->attached_vehicle!=vehicle || bot->attached_seat!=seat) return false;
        unsigned char requester_ai=0,driver_ai=0;
        if (!read_at(requester,0x13c8,requester_ai) || requester_ai!=0 ||
            !read_at(driver,0x13c8,driver_ai) || driver_ai!=0) return false;

        std::uint32_t flags=0,state_key=0;
        std::uintptr_t health=0,health_vtable=0,health_function=0,state_function=0;
        const auto base=guard_.module_base();
        if (!read_at(vehicle,0x18,flags) || !(flags&(1U<<25)) ||
            !read_at(vehicle,0x1f0,health) || !health ||
            !read_at(health,0,health_vtable) ||
            !read_at(health_vtable,0xe8,health_function) || !executable(health_function) ||
            !read_at(base,0x2603720,state_function) || !executable(state_function) ||
            !read_at(vehicle,0x13c,state_key)) return false;
        const float health_value=reinterpret_cast<HealthQuery>(health_function)(reinterpret_cast<void*>(health));
        // Nonfinite values are rejected conservatively; native code checks != 0.
        if (!std::isfinite(health_value) || health_value==0.0f ||
            reinterpret_cast<StateQuery>(state_function)(state_key,static_cast<std::uint32_t>(p->team))==2) return false;

        std::uintptr_t entry_forbidden=0,entry_role=0,remote_query=0;
        if (!fixed_function(base,0xc596f0,entry_forbidden) ||
            !fixed_function(base,0xb43f10,entry_role) ||
            !fixed_function(base,0xbcfe70,remote_query)) return false;
        if (reinterpret_cast<BooleanQuery>(entry_forbidden)(reinterpret_cast<void*>(entry))) return false;
        std::int32_t entry_mode=0;
        if (!read_at(vehicle,0x1e8,entry_mode)) return false;
        if (entry_mode==1) {
            std::uintptr_t entry_component=0;
            if (!r63_layout_detail::checked_address(entry,0x30,entry_component) ||
                reinterpret_cast<RoleQuery>(entry_role)(reinterpret_cast<void*>(entry_component))!=0)
                return false;
        }

        std::uintptr_t context=0,manager=0,begin=0,end=0;
        if (!read_at(base,0x2c68148,context) || !context ||
            !read_at(context,0x60,manager) || !manager ||
            !read_at(manager,0x270,begin) || !read_at(manager,0x278,end) ||
            end<begin || (end-begin)%sizeof(std::uintptr_t) ||
            (begin%alignof(std::uintptr_t)) || (end-begin)/sizeof(std::uintptr_t)>256)
            return false;
        const auto count=(end-begin)/sizeof(std::uintptr_t);
        for (std::uintptr_t i=0;i<count;++i) {
            std::uintptr_t other=0;
            if (!read_at(begin,i*sizeof(std::uintptr_t),other)) return false;
            if (!other || other==requester) continue;
            // Validate the player slot before the recovered native state query.
            std::uint32_t other_id=0;
            if (!read_at(other,0x1498,other_id) || other_id>255) return false;
            if (!reinterpret_cast<BooleanQuery>(remote_query)(reinterpret_cast<void*>(other))) continue;
            std::uintptr_t attached=0;
            std::uint32_t other_seat=0;
            if (!read_at(other,0x14c8,attached)) return false;
            if (attached!=vehicle) continue;
            if (!read_at(other,0x14c0,other_seat) || other_seat>=seats->seats.size()) return false;
            if (seats->seats[other_seat].entry==entry) return false;
        }

        // Query callbacks must not make an old occupancy/identity look current.
        const auto after_p=reader_.player(requester);
        const auto after_bot=reader_.player(driver);
        const auto after_seats=reader_.vehicle(vehicle);
        return guard_.validate() && known_bot_(driver) && !known_bot_(requester) &&
            after_p && after_p->soldier==p->soldier && after_p->team==p->team &&
            after_p->attached_vehicle==p->attached_vehicle && after_bot &&
            after_bot->soldier==bot->soldier && after_bot->team==p->team &&
            after_bot->attached_vehicle==vehicle && after_bot->attached_seat==seat &&
            after_seats && seat<after_seats->seats.size() &&
            after_seats->seats[seat].entry==entry && after_seats->seats[seat].player==driver;
    } catch (...) {
        return false;
    }
}
}
