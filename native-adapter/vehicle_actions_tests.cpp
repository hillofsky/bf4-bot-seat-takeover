#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "r63_vehicle_actions.hpp"
#include <iostream>
int main() {
    bf4_takeover::CurrentServerImageGuard guard;
    bf4_takeover::R63SeatReader reader(guard);
    int callbacks=0;
    bf4_takeover::R63ActionHost host;
    host.gameplay_thread_id=GetCurrentThreadId();
    host.mutation_allowed=[&](std::uintptr_t,std::uintptr_t){++callbacks;return true;};
    host.trusted_bot=[&](std::uintptr_t){++callbacks;return true;};
    host.world_entry_allowed=[&](std::uintptr_t,std::uintptr_t){++callbacks;return true;};
    bf4_takeover::R63VehicleActions actions(guard,reader,host);
    if (actions.enter(1,2,0) || actions.switch_seat(1,2,0) || actions.exit_bot(1,2,0) ||
        actions.can_enter_from_world(1,2) || callbacks) {
        std::cerr<<"FAIL: uninitialized guard failed to reject native actions\n";return 1;
    }
    if (guard.initialize().supported || actions.enter(1,2,0) || actions.exit_bot(1,2,0) || callbacks) {
        std::cerr<<"FAIL: helper process reached native callbacks\n";return 1;
    }
    std::cout<<"PASS: native vehicle action bindings reject uninitialized/non-BF4 guard; no native mutation calls executed\n";
}
