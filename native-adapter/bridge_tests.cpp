#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "r63_bindings.hpp"
#include <iostream>
int main() {
    bf4_takeover::R63RuntimeHost empty;
    bf4_takeover::R63EngineBridge bridge(empty);
    if (bridge.prepare()) return 1;
    bf4_takeover::Registry registry;
    bf4_takeover::NativeAdapter adapter(registry);
    if (adapter.activate(bridge.bindings())) return 2;
    bf4_takeover::R63RuntimeHost full;
    full.gameplay_thread_id=GetCurrentThreadId();
    full.gameplay_callback_evidence="test process; not BF4";
    full.actor_status=[](std::uintptr_t)->std::optional<bf4_takeover::ActorStatus>{return {};};
    full.seat_allowed=[](std::uintptr_t,int,int){return false;};
    full.mutation_allowed=[](std::uintptr_t,std::uintptr_t){return false;};
    full.world_entry_allowed=full.mutation_allowed;
    bf4_takeover::R63EngineBridge wrong_image(full);
    if (wrong_image.prepare() || adapter.activate(wrong_image.bindings())) return 3;
    std::cout<<"PASS: native bridge rejects missing host and non-BF4 image; no engine calls\n";
}
