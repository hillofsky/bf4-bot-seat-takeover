#pragma once

#include <cstdint>
#include <optional>
#include <unordered_map>

namespace bf4_takeover {

using Tick = std::uint64_t;

// A native address or player slot alone is insufficient: both can be reused.
struct Identity {
    std::uint64_t id = 0;
    std::uint64_t generation = 0;
    friend bool operator==(Identity a, Identity b) {
        return a.id == b.id && a.generation == b.generation;
    }
    friend bool operator!=(Identity a, Identity b) { return !(a == b); }
};

struct Actor {
    Identity identity;
    int team = 0;
    bool bot = false;
    bool alive = false;
    bool has_soldier = false;
    bool allowed_to_spawn = false;
    std::optional<Identity> vehicle;
    int seat = -1;
};

// Constructed by a trusted, host-side selection/menu implementation. This core
// does not authenticate client tokens or accept client-supplied native pointers.
struct Selection {
    Identity bot;
    Identity vehicle;
    int seat = -1;
    Tick created_at = 0;
};

// All calls, including queue and on_native_spawn, MUST be serialized on the
// server's gameplay thread. Implementations must resolve identities afresh and
// must not run these methods from an arbitrary worker thread.
class Adapter {
public:
    virtual ~Adapter() = default;
    virtual std::optional<Actor> actor(Identity identity) = 0;
    virtual bool seat_allowed_for_human(Identity vehicle, int seat, int team) = 0;
    virtual std::optional<Identity> occupant(Identity vehicle, int seat) = 0;
    virtual bool exit_bot(Identity bot, Identity vehicle, int seat) = 0;
    virtual bool enter(Identity actor, Identity vehicle, int seat) = 0;
    // Must check the engine's ordinary interaction distance and entry rules.
    virtual bool can_enter_from_world(Identity human, Identity vehicle) = 0;
    // Native in-vehicle seat change: do not exit/recreate the human soldier.
    // On failure leave the human in the original seat. On success update both
    // seats synchronously. All mutation methods must finish before returning;
    // asynchronous native requests need a separate completion-driven adapter.
    virtual bool switch_seat(Identity human, Identity vehicle, int seat) = 0;
    // No create-soldier, kill, loadout, team-change or teleport API is exposed.
};

enum class QueueResult { queued, not_ready, invalid_selection, adapter_error };
enum class SpawnResult {
    no_request, expired, target_changed, human_not_ready, exit_failed,
    transfer_failed, transferred, adapter_error, busy
};

struct SpawnOutcome {
    SpawnResult result;
    bool bot_restored = false;
};

class Takeover {
public:
    explicit Takeover(Adapter& adapter) : adapter_(adapter) {}

    QueueResult queue(Identity human, Selection selection, Tick now);
    // Called AFTER the engine completed normal deployment and created the
    // soldier with the player's chosen equipment. Never a substitute for it.
    SpawnOutcome on_native_spawn(Identity human, Tick now);
    // Both require a living human and a fresh, trusted selection of seat 0.
    SpawnOutcome take_driver_from_inside(Identity human, Selection selection, Tick now);
    SpawnOutcome take_driver_from_outside(Identity human, Selection selection, Tick now);
    void player_left(Identity human);
    void level_destroyed() { pending_.clear(); }
    std::size_t pending_count() const { return pending_.size(); }

private:
    struct Pending { Identity human; Selection selection; int team; Tick queued_at; };
    Adapter& adapter_;
    std::unordered_map<std::uint64_t, Pending> pending_;
    bool transferring_ = false;

    bool valid_target(const Selection& selection, int team);
    bool restore_if_empty(const Pending& request) noexcept;
    SpawnOutcome transfer(const Pending& request, bool inside, bool check_world_entry);
    SpawnOutcome take_driver(Identity human, Selection selection, Tick now, bool inside);
};

} // namespace bf4_takeover
