#include "takeover.hpp"
#include <functional>
#include <iostream>
#include <map>
#include <stdexcept>
#include <string>

using namespace bf4_takeover;
#define CHECK(condition) do { if (!(condition)) throw std::runtime_error( \
    std::string(__func__) + ": " + #condition); } while(false)

const Identity human_id{1, 11}, bot_id{2, 21}, vehicle_id{77, 71};

struct Fake final : Adapter {
    std::map<std::uint64_t, Actor> actors;
    std::map<int, Identity> seats;
    bool vehicle_present = true, allowed = true, refuse_exit = false;
    bool refuse_human_entry = false, throw_after_exit = false;
    bool throw_after_human_entry = false, refuse_restore = false;
    bool report_false_after_human_entry = false;
    bool near_vehicle = true, refuse_switch = false;
    int switches = 0;
    int exits = 0, enters = 0;
    std::function<void()> after_exit;
    std::string chosen_loadout = "native_engine_selected_kit";
    std::uint64_t native_soldier = 0x123456;

    Fake() {
        actors[1] = Actor{human_id, 1, false, false, false, true, {}, -1};
        actors[2] = Actor{bot_id, 1, true, true, true, false, vehicle_id, 0};
        seats[0] = bot_id;
    }
    void deploy() { actors[1].alive = true; actors[1].has_soldier = true; }
    std::optional<Actor> actor(Identity id) override {
        auto found = actors.find(id.id);
        if (found == actors.end() || found->second.identity != id) return {};
        return found->second;
    }
    bool seat_allowed_for_human(Identity v, int seat, int team) override {
        return vehicle_present && v == vehicle_id && allowed && seat >= 0 &&
               seat < 3 && team == 1;
    }
    std::optional<Identity> occupant(Identity v, int seat) override {
        if (!vehicle_present || v != vehicle_id) return {};
        auto found = seats.find(seat);
        return found == seats.end() ? std::optional<Identity>{} : found->second;
    }
    bool exit_bot(Identity bot, Identity v, int seat) override {
        ++exits;
        if (refuse_exit) return false;
        if (occupant(v, seat) != std::optional<Identity>(bot))
            throw std::runtime_error("must never eject a changed occupant");
        seats.erase(seat); actors[bot.id].vehicle.reset(); actors[bot.id].seat = -1;
        if (after_exit) after_exit();
        if (throw_after_exit) throw std::runtime_error("exit completed then threw");
        return true;
    }
    bool enter(Identity who, Identity v, int seat) override {
        ++enters;
        if (occupant(v, seat)) throw std::runtime_error("must never overwrite occupant");
        if (!seat_allowed_for_human(v, seat, 1)) return false;
        if (who == human_id && refuse_human_entry) return false;
        if (who == bot_id && refuse_restore) return false;
        seats[seat] = who; actors[who.id].vehicle = v; actors[who.id].seat = seat;
        if (who == human_id && throw_after_human_entry)
            throw std::runtime_error("entry completed then threw");
        return !(who == human_id && report_false_after_human_entry);
    }
    bool can_enter_from_world(Identity who, Identity v) override {
        return who == human_id && v == vehicle_id && near_vehicle;
    }
    bool switch_seat(Identity who, Identity v, int seat) override {
        ++switches;
        if (refuse_switch) return false;
        const int old_seat = actors.at(who.id).seat;
        const bool result = enter(who, v, seat);
        if (result) seats.erase(old_seat);
        return result;
    }
};

Selection selected(int seat = 0) { return Selection{bot_id, vehicle_id, seat, 100}; }
void enqueue(Takeover& t) { CHECK(t.queue(human_id, selected(), 100) == QueueResult::queued); }
void preserved(const Fake& f) {
    CHECK(f.actors.at(1).alive && f.actors.at(1).has_soldier);
    CHECK(f.native_soldier == 0x123456);
    CHECK(f.chosen_loadout == "native_engine_selected_kit");
}

void native_spawn_success() {
    Fake f; Takeover t(f); enqueue(t);
    CHECK(f.seats[0] == bot_id && f.exits == 0 && f.enters == 0);
    f.deploy(); CHECK(t.on_native_spawn(human_id, 101).result == SpawnResult::transferred);
    CHECK(f.seats[0] == human_id && f.exits == 1 && f.enters == 1);
    CHECK(t.on_native_spawn(human_id, 102).result == SpawnResult::no_request);
    preserved(f);
}

void reject_queue_conditions() {
    for (int reason = 0; reason < 11; ++reason) {
        Fake f; Takeover t(f); auto s = selected();
        switch(reason) {
        case 0: f.actors[1].alive = true; break;
        case 1: f.actors[1].has_soldier = true; break;
        case 2: f.actors[1].allowed_to_spawn = false; break;
        case 3: f.actors[1].team = 0; break;
        case 4: f.actors[1].bot = true; break;
        case 5: f.actors[2].bot = false; break;
        case 6: f.actors[2].team = 2; break;
        case 7: f.allowed = false; break;
        case 8: s.seat = -1; break;
        case 9: s.vehicle.generation++; break;
        case 10: s.created_at = 101; break;
        }
        CHECK(t.queue(human_id, s, 100) != QueueResult::queued);
        CHECK(t.pending_count() == 0 && f.exits == 0);
    }
    Fake f; Takeover t(f);
    CHECK(t.queue(human_id, selected(), 30101) == QueueResult::invalid_selection);
}

void revalidate_after_native_deployment() {
    for (int reason = 0; reason < 10; ++reason) {
        Fake f; Takeover t(f); enqueue(t); f.deploy();
        switch(reason) {
        case 0: f.actors[2].team = 2; break;
        case 1: f.actors[2].identity.generation++; break;
        case 2: f.actors[2].alive = false; break;
        case 3: f.actors[2].bot = false; break;
        case 4: f.actors[2].seat = 1; break;
        case 5: f.actors[2].vehicle = Identity{99, 1}; break;
        case 6: f.vehicle_present = false; break;
        case 7: f.allowed = false; break;
        case 8: f.seats[0] = Identity{3, 31}; break;
        case 9: f.actors.erase(2); break;
        }
        CHECK(t.on_native_spawn(human_id, 101).result == SpawnResult::target_changed);
        CHECK(f.exits == 0 && f.enters == 0);
        preserved(f);
    }
}

void human_state_and_expiration() {
    for (int reason = 0; reason < 5; ++reason) {
        Fake f; Takeover t(f); enqueue(t); f.deploy();
        switch(reason) {
        case 0: f.actors[1].team = 2; break;
        case 1: f.actors[1].alive = false; break;
        case 2: f.actors[1].has_soldier = false; break;
        case 3: f.actors[1].vehicle = Identity{99,1}; break;
        case 4: f.actors[1].bot = true; break;
        }
        CHECK(t.on_native_spawn(human_id,101).result == SpawnResult::human_not_ready);
        CHECK(f.exits == 0);
    }
    for (Tick now : {Tick{99}, Tick{60101}}) {
        Fake f; Takeover t(f); enqueue(t); f.deploy();
        CHECK(t.on_native_spawn(human_id,now).result == SpawnResult::expired);
        CHECK(f.exits == 0); preserved(f);
    }
}

void restoration_and_exceptions() {
    {
        Fake f; Takeover t(f); enqueue(t); f.deploy(); f.refuse_human_entry = true;
        auto result = t.on_native_spawn(human_id,101);
        CHECK(result.result == SpawnResult::transfer_failed && result.bot_restored);
        CHECK(f.seats[0] == bot_id); preserved(f);
    }
    {
        Fake f; Takeover t(f); enqueue(t); f.deploy(); f.throw_after_exit = true;
        auto result = t.on_native_spawn(human_id,101);
        CHECK(result.result == SpawnResult::adapter_error && result.bot_restored);
        CHECK(f.seats[0] == bot_id); preserved(f);
    }
    for (bool returns_false : {false,true}) {
        Fake f; Takeover t(f); enqueue(t); f.deploy();
        f.throw_after_human_entry = !returns_false;
        f.report_false_after_human_entry = returns_false;
        CHECK(t.on_native_spawn(human_id,101).result == SpawnResult::transferred);
        CHECK(f.seats[0] == human_id); preserved(f);
    }
    {
        Fake f; Takeover t(f); enqueue(t); f.deploy();
        f.refuse_human_entry = true; f.refuse_restore = true;
        auto result = t.on_native_spawn(human_id,101);
        CHECK(result.result == SpawnResult::transfer_failed && !result.bot_restored);
        CHECK(f.seats.empty()); preserved(f);
    }
    {
        Fake f; Takeover t(f); enqueue(t); f.deploy(); f.refuse_exit = true;
        CHECK(t.on_native_spawn(human_id,101).result == SpawnResult::exit_failed);
        CHECK(f.seats[0] == bot_id && f.enters == 0); preserved(f);
    }
}

void callbacks_cannot_displace_other_people() {
    {
        Fake f; Takeover t(f); enqueue(t); f.deploy();
        const Identity replacement{3,31};
        f.after_exit = [&]{ f.seats[0] = replacement; };
        CHECK(t.on_native_spawn(human_id,101).result == SpawnResult::target_changed);
        CHECK(f.seats[0] == replacement && f.enters == 0); preserved(f);
    }
    {
        Fake f; Takeover t(f); enqueue(t); f.deploy();
        f.after_exit = [&]{ f.actors[2].team = 2; };
        auto result = t.on_native_spawn(human_id,101);
        CHECK(result.result == SpawnResult::target_changed && !result.bot_restored);
        CHECK(f.enters == 0); preserved(f);
    }
    {
        Fake f; Takeover t(f); enqueue(t); f.deploy();
        f.after_exit = [&]{ CHECK(t.on_native_spawn(human_id,101).result == SpawnResult::busy); };
        CHECK(t.on_native_spawn(human_id,101).result == SpawnResult::transferred);
        CHECK(f.exits == 1); preserved(f);
    }
}

void session_lifecycle_and_seat_safety() {
    Fake f; Takeover t(f); enqueue(t);
    // A stale callback from another generation must not consume the current job.
    CHECK(t.on_native_spawn(Identity{1,10},101).result == SpawnResult::target_changed);
    CHECK(t.pending_count() == 1);
    t.player_left(Identity{1,10}); CHECK(t.pending_count() == 1);
    t.player_left(human_id); CHECK(t.pending_count() == 0);
    enqueue(t); t.level_destroyed(); CHECK(t.pending_count() == 0);

    // An autopilot/forbidden seat cannot be selected, but an allowed gunner can.
    f.allowed = false;
    CHECK(t.queue(human_id,selected(),100) == QueueResult::invalid_selection);
    f.allowed = true; f.seats.erase(0); f.seats[1] = bot_id; f.actors[2].seat = 1;
    const Identity pilot{3,31}; f.seats[0] = pilot;
    CHECK(t.queue(human_id,selected(1),100) == QueueResult::queued);
    f.deploy(); CHECK(t.on_native_spawn(human_id,101).result == SpawnResult::transferred);
    CHECK(f.seats[0] == pilot && f.seats[1] == human_id); preserved(f);
}

void live_driver_takeover() {
    for (bool inside : {false, true}) {
        for (int scenario = 0; scenario < 12; ++scenario) {
            Fake f; Takeover t(f); f.deploy();
            if (inside) {
                f.actors[1].vehicle = vehicle_id; f.actors[1].seat = 1;
                f.seats[1] = human_id;
            }
            auto s = selected();
            switch (scenario) {
            case 1: f.actors[2].bot = false; break;
            case 2: f.actors[2].team = 2; break;
            case 3: f.allowed = false; break;
            case 4: s.seat = 1; break;
            case 5: s.created_at = 102; break;
            case 6: f.actors[1].alive = false; break;
            case 7:
                if (inside) f.actors[1].vehicle = Identity{99, 1};
                else f.near_vehicle = false;
                break;
            case 8: f.refuse_switch = true; f.refuse_human_entry = true; break;
            case 9: f.after_exit = [&]{ f.seats[0] = Identity{3,31}; }; break;
            case 10:
                f.after_exit = [&]{
                    if (inside) { f.seats.erase(1); f.actors[1].vehicle.reset(); }
                    else f.near_vehicle = false;
                }; break;
            case 11:
                f.after_exit = [&]{
                    CHECK(t.take_driver_from_inside(human_id,s,101).result == SpawnResult::busy);
                    CHECK(t.take_driver_from_outside(human_id,s,101).result == SpawnResult::busy);
                }; break;
            }
            const auto result = inside ? t.take_driver_from_inside(human_id,s,101)
                                       : t.take_driver_from_outside(human_id,s,101);
            if (scenario == 0 || scenario == 11) {
                CHECK(result.result == SpawnResult::transferred);
                CHECK(f.seats[0] == human_id);
                CHECK(f.switches == (inside ? 1 : 0));
                if (inside) CHECK(f.seats.count(1) == 0);
            } else {
                CHECK(result.result != SpawnResult::transferred);
                if (scenario < 8) CHECK(f.exits == 0 && f.enters == 0);
                if (scenario == 8 || scenario == 10) {
                    CHECK(result.bot_restored && f.seats[0] == bot_id);
                    if (inside && scenario == 8) CHECK(f.seats[1] == human_id);
                }
                if (scenario == 9) CHECK(f.seats[0] == (Identity{3,31}) && f.enters == 0);
            }
            if (scenario != 6) preserved(f);
        }
    }
}

int main() {
    try {
        native_spawn_success(); reject_queue_conditions();
        revalidate_after_native_deployment(); human_state_and_expiration();
        restoration_and_exceptions(); callbacks_cannot_displace_other_people();
        session_lifecycle_and_seat_safety();
        live_driver_takeover();
        std::cout << "PASS: 8 suites, 60+ takeover scenarios; normal deployment preserved, "
                     "identity/team/seat revalidated, restoration and reentrancy checked.\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "FAIL: " << e.what() << '\n'; return 1;
    }
}
