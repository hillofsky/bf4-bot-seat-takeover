#pragma once
#include "takeover.hpp"
#include <functional>
#include <string>
#include <thread>
#include <unordered_map>

namespace bf4_takeover {

// Host-only boundary. This file deliberately contains no guessed BF4 offsets,
// calling conventions, function addresses, injection code, or hook installer.
using NativeHandle = std::uintptr_t;
enum class ObjectKind { player, vehicle };

class Registry {
public:
    Identity created(ObjectKind kind, NativeHandle handle);
    void destroyed(Identity identity);
    void level_destroyed();
    std::optional<NativeHandle> resolve(Identity identity, ObjectKind kind) const;
    std::optional<Identity> identify(NativeHandle handle, ObjectKind kind) const;
private:
    struct Entry { Identity identity; ObjectKind kind; NativeHandle handle; };
    std::uint64_t next_ = 1;
    std::unordered_map<std::uint64_t, Entry> entries_;
};

struct NativeActor {
    int team = 0;
    bool bot = false, alive = false, has_soldier = false, allowed_to_spawn = false;
    NativeHandle vehicle = 0;
    int seat = -1;
};

struct Bindings {
    // Populated only by a future version-specific native binding implementation.
    // Evidence labels are provenance, not proof that an ABI is correct.
    std::string target_sha256;
    std::string abi_evidence;
    std::string gameplay_callback_evidence;
    bool synchronous_mutations = false;
    std::function<bool()> validate_loaded_image;
    std::function<std::optional<NativeActor>(NativeHandle)> read_actor;
    std::function<bool(NativeHandle, int, int)> seat_allowed;
    std::function<NativeHandle(NativeHandle, int)> occupant;
    std::function<bool(NativeHandle, NativeHandle, int)> exit_bot;
    std::function<bool(NativeHandle, NativeHandle, int)> enter;
    std::function<bool(NativeHandle, NativeHandle)> can_enter_from_world;
    std::function<bool(NativeHandle, NativeHandle, int)> switch_seat;
};

// Construct and invoke only from a verified gameplay callback. The thread gate
// rejects other threads; it cannot discover which thread is BF4's gameplay thread.
class NativeAdapter final : public Adapter {
public:
    explicit NativeAdapter(Registry& registry) : registry_(registry) {}
    bool activate(Bindings bindings);
    void deactivate();
    bool ready() const;
    std::optional<Actor> actor(Identity id) override;
    bool seat_allowed_for_human(Identity vehicle, int seat, int team) override;
    std::optional<Identity> occupant(Identity vehicle, int seat) override;
    bool exit_bot(Identity bot, Identity vehicle, int seat) override;
    bool enter(Identity actor, Identity vehicle, int seat) override;
    bool can_enter_from_world(Identity human, Identity vehicle) override;
    bool switch_seat(Identity human, Identity vehicle, int seat) override;
private:
    Registry& registry_;
    Bindings bindings_;
    std::thread::id gameplay_thread_;
    bool active_ = false;
    void require_ready() const;
};
}
