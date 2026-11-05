#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include "r63_vehicle_actions.hpp"
#include <atomic>

using namespace bf4_takeover;
namespace {
CurrentServerImageGuard guard;
std::atomic<bool> initialized{false};
std::atomic<bool> attempted{false};
}

// Diagnostic only. The host must use a positively identified bot-control
// callback. This DLL installs no hooks and never selects a human or vehicle.
extern "C" __declspec(dllexport) unsigned ProbeInit() {
    if (initialized.load()) return 1;
    const auto result=guard.initialize();
    initialized.store(result.supported);
    return result.supported ? 1U : 0U;
}

// 0 not ready, 1 exit refused, 2 restored, 3 restoration failed,
// 4 guard/thread rejected, 5 already attempted, 6 exception.
extern "C" __declspec(dllexport) unsigned ProbeExitRestoreBot(
    std::uintptr_t context,unsigned verified_thread) {
    if (!initialized.load() || !guard.validate() || !verified_thread ||
        GetCurrentThreadId()!=verified_thread) return 4;
    if (attempted.load()) return 5;
    try {
        std::uintptr_t pointer_address=0,bot=0;
        SIZE_T count=0;
        if (!r63_layout_detail::checked_address(context,0xbf50,pointer_address) ||
            !ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<void*>(pointer_address),
                               &bot,sizeof(bot),&count) || count!=sizeof(bot) || !bot) return 0;
        R63SeatReader reader(guard);
        const auto before=reader.player(bot);
        if (!before || !before->soldier || !before->attached_vehicle ||
            before->attached_seat!=0) return 0;
        const auto vehicle=before->attached_vehicle;
        const auto seats=reader.vehicle(vehicle);
        if (!seats || seats->seats.empty() || seats->seats[0].player!=bot) return 0;
        // Do not disturb any passengers during this diagnostic.
        for (std::size_t i=1;i<seats->seats.size();++i)
            if (seats->seats[i].player) return 0;
        bool expected=false;
        if (!attempted.compare_exchange_strong(expected,true)) return 5;
        R63ActionHost host;
        host.gameplay_thread_id=verified_thread;
        host.trusted_bot=[bot](std::uintptr_t p){return p==bot;};
        host.mutation_allowed=[bot,vehicle](std::uintptr_t p,std::uintptr_t v){
            return p==bot && v==vehicle;
        };
        R63VehicleActions actions(guard,reader,std::move(host));
        const bool exited=actions.exit_bot(bot,vehicle,0);
        const auto after_exit=reader.player(bot);
        const auto empty=reader.vehicle(vehicle);
        if (!after_exit || !empty || empty->seats.empty()) return exited ? 3U : 1U;
        if (after_exit->attached_vehicle==vehicle && empty->seats[0].player==bot)
            return 1;
        if (after_exit->attached_vehicle || empty->seats[0].player ||
            after_exit->soldier!=before->soldier) return 3;
        const bool restored=actions.enter(bot,vehicle,0);
        const auto after=reader.player(bot);
        return restored && after && after->soldier==before->soldier ? 2U : 3U;
    } catch (...) { return 6; }
}

BOOL WINAPI DllMain(HINSTANCE,DWORD,LPVOID) { return TRUE; }
