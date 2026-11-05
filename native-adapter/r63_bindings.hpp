#pragma once
#include "native_adapter.hpp"
#include "r63_vehicle_actions.hpp"

namespace bf4_takeover {
struct ActorStatus {
    bool bot=false, alive=false, allowed_to_spawn=false;
};
struct R63RuntimeHost {
    std::uint32_t gameplay_thread_id=0;
    std::string gameplay_callback_evidence;
    // Positive classification/life/lifecycle knowledge from the real host.
    // Missing metadata is rejected, never inferred from names or soldier != 0.
    std::function<std::optional<ActorStatus>(NativeHandle)> actor_status;
    std::function<bool(NativeHandle,int,int)> seat_allowed;
    std::function<bool(NativeHandle,NativeHandle)> mutation_allowed;
    std::function<bool(NativeHandle,NativeHandle)> world_entry_allowed;
};

// Own this object for the lifetime of the Adapter callbacks returned by bindings.
// Creation/destruction, level changes and command delivery remain host events.
class R63EngineBridge {
public:
    explicit R63EngineBridge(R63RuntimeHost host);
    R63EngineBridge(const R63EngineBridge&)=delete;
    R63EngineBridge& operator=(const R63EngineBridge&)=delete;
    bool prepare();
    Bindings bindings();
private:
    R63RuntimeHost host_;
    CurrentServerImageGuard guard_;
    R63SeatReader reader_;
    R63VehicleActions actions_;
    R63ActionHost action_host();
};
}
