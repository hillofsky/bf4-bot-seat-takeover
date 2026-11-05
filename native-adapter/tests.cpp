#include "native_adapter.hpp"
#include <iostream>
#include <stdexcept>
#include <thread>
#include <map>
using namespace bf4_takeover;
#define CHECK(x) do { if (!(x)) throw std::runtime_error(#x); } while(false)
template<class F> bool throws(F f) { try { f(); } catch (...) { return true; } return false; }

struct IntegratedFixture {
    Registry registry;
    NativeAdapter adapter{registry};
    Takeover core{adapter};
    Identity human = registry.created(ObjectKind::player,0x100);
    Identity bot = registry.created(ObjectKind::player,0x200);
    Identity vehicle = registry.created(ObjectKind::vehicle,0x300);
    std::map<NativeHandle,NativeActor> actors;
    std::map<int,NativeHandle> seats;
    bool near_vehicle = true, vehicle_alive = true;
    bool destroy_after_exit = false, unknown_after_exit = false, move_after_exit = false;
    int exits = 0, enters = 0, switches = 0;

    IntegratedFixture() {
        actors[0x100] = NativeActor{1,false,false,false,true,0,-1};
        actors[0x200] = NativeActor{1,true,true,true,false,0x300,0};
        seats[0] = 0x200;
        Bindings b;
        b.target_sha256 = "160ec988aad0632946936a21a9aed1c73f5b906161f2d96b2a085ee65ce183ba";
        b.abi_evidence = "TEST ONLY: fake engine integration";
        b.gameplay_callback_evidence = "TEST ONLY: main test thread";
        b.synchronous_mutations = true;
        b.validate_loaded_image = []{return true;};
        b.read_actor = [&](NativeHandle h)->std::optional<NativeActor> {
            const auto it = actors.find(h);
            if (it == actors.end()) return {};
            return it->second;
        };
        b.seat_allowed = [&](NativeHandle v,int seat,int team) {
            return vehicle_alive && v==0x300 && seat>=0 && seat<2 && team==1;
        };
        b.occupant = [&](NativeHandle v,int seat)->NativeHandle {
            CHECK(vehicle_alive && v==0x300);
            const auto it = seats.find(seat);
            return it==seats.end() ? 0 : it->second;
        };
        b.exit_bot = [&](NativeHandle h,NativeHandle v,int seat) {
            CHECK(h==0x200 && v==0x300 && seats.at(seat)==h);
            ++exits; seats.erase(seat); actors.at(h).vehicle=0; actors.at(h).seat=-1;
            if (destroy_after_exit) {
                registry.destroyed(vehicle); vehicle_alive=false; seats.clear();
                for (auto& a : actors) { a.second.vehicle=0; a.second.seat=-1; }
            }
            if (unknown_after_exit) seats[0]=0x999;
            if (move_after_exit) near_vehicle=false;
            return true;
        };
        b.enter = [&](NativeHandle h,NativeHandle v,int seat) {
            CHECK(vehicle_alive && v==0x300 && !seats.count(seat));
            ++enters; seats[seat]=h; actors.at(h).vehicle=v; actors.at(h).seat=seat;
            return true;
        };
        b.can_enter_from_world = [&](NativeHandle h,NativeHandle v) {
            CHECK(h==0x100 && v==0x300); return near_vehicle && vehicle_alive;
        };
        b.switch_seat = [&](NativeHandle h,NativeHandle v,int seat) {
            CHECK(vehicle_alive && h==0x100 && v==0x300 && !seats.count(seat));
            ++switches; seats.erase(actors.at(h).seat); seats[seat]=h;
            actors.at(h).seat=seat; return true;
        };
        CHECK(adapter.activate(std::move(b)));
    }
    Selection selection() const { return Selection{bot,vehicle,0,100}; }
    void spawn() { actors.at(0x100).alive=true; actors.at(0x100).has_soldier=true; }
    void ride() { spawn(); actors.at(0x100).vehicle=0x300; actors.at(0x100).seat=1; seats[1]=0x100; }
};

void integrated_tests() {
    {
        IntegratedFixture f;
        CHECK(f.core.queue(f.human,f.selection(),100)==QueueResult::queued);
        CHECK(f.exits==0);
        f.spawn();
        CHECK(f.core.on_native_spawn(f.human,101).result==SpawnResult::transferred);
        CHECK(f.seats.at(0)==0x100 && f.exits==1 && f.enters==1);
    }
    {
        IntegratedFixture f; f.ride();
        CHECK(f.core.take_driver_from_inside(f.human,f.selection(),101).result==SpawnResult::transferred);
        CHECK(f.switches==1 && f.enters==0 && f.seats.at(0)==0x100 && !f.seats.count(1));
        CHECK(f.actors.at(0x100).alive && f.actors.at(0x100).has_soldier);
    }
    {
        IntegratedFixture f; f.spawn(); f.near_vehicle=false;
        CHECK(f.core.take_driver_from_outside(f.human,f.selection(),101).result==SpawnResult::human_not_ready);
        CHECK(f.exits==0 && f.enters==0);
        f.near_vehicle=true;
        CHECK(f.core.take_driver_from_outside(f.human,f.selection(),102).result==SpawnResult::transferred);
        CHECK(f.exits==1 && f.enters==1);
    }
    {
        IntegratedFixture f; f.spawn(); f.move_after_exit=true;
        const auto result=f.core.take_driver_from_outside(f.human,f.selection(),101);
        CHECK(result.result==SpawnResult::target_changed && result.bot_restored);
        CHECK(f.seats.at(0)==0x200 && f.actors.at(0x100).vehicle==0);
    }
    {
        IntegratedFixture f; f.spawn(); f.destroy_after_exit=true;
        const auto result=f.core.take_driver_from_outside(f.human,f.selection(),101);
        CHECK(result.result==SpawnResult::adapter_error && !result.bot_restored);
        CHECK(f.enters==0 && !f.registry.resolve(f.vehicle,ObjectKind::vehicle));
    }
    {
        IntegratedFixture f; f.spawn(); f.seats[0]=0x999;
        const auto result=f.core.take_driver_from_outside(f.human,f.selection(),101);
        CHECK(result.result==SpawnResult::adapter_error && f.exits==0 && f.enters==0);
    }
    {
        IntegratedFixture f; f.spawn(); f.unknown_after_exit=true;
        const auto result=f.core.take_driver_from_outside(f.human,f.selection(),101);
        CHECK(result.result==SpawnResult::adapter_error && !result.bot_restored);
        CHECK(f.seats.at(0)==0x999 && f.enters==0);
    }
}
int main() {
    try {
        Registry registry; NativeAdapter adapter(registry);
        CHECK(!adapter.activate({}));
        CHECK(throws([&]{ adapter.occupant({1,1},0); }));
        auto human = registry.created(ObjectKind::player,0x100);
        auto bot = registry.created(ObjectKind::player,0x200);
        auto vehicle = registry.created(ObjectKind::vehicle,0x300);
        bool image_valid = true; NativeHandle seat = 0x200; int exits = 0;
        Bindings b;
        b.target_sha256 = "160ec988aad0632946936a21a9aed1c73f5b906161f2d96b2a085ee65ce183ba";
        b.abi_evidence = "TEST ONLY: no native BF4 calls";
        b.gameplay_callback_evidence = "TEST ONLY: current test thread";
        b.synchronous_mutations = true;
        b.validate_loaded_image = [&]{ return image_valid; };
        b.read_actor = [&](NativeHandle h)->std::optional<NativeActor> {
            return NativeActor{1,h==0x200,true,true,false,h==seat ? NativeHandle{0x300} : 0,
                h==seat ? 0 : -1};
        };
        b.seat_allowed = [](NativeHandle,int s,int t){ return s==0 && t==1; };
        b.occupant = [&](NativeHandle,int){ return seat; };
        b.exit_bot = [&](NativeHandle,NativeHandle,int){ ++exits; seat=0; return true; };
        b.enter = [&](NativeHandle h,NativeHandle,int){ seat=h; return true; };
        b.can_enter_from_world = [](NativeHandle,NativeHandle){ return true; };
        b.switch_seat = [](NativeHandle,NativeHandle,int){ return true; };
        auto incomplete = b; incomplete.enter = {};
        CHECK(!adapter.activate(incomplete));
        auto wrong_image = b; wrong_image.target_sha256 = "bad";
        CHECK(!adapter.activate(wrong_image));
        CHECK(adapter.activate(b));
        bool rejected = false;
        std::thread other([&]{ rejected = throws([&]{ adapter.actor(human); }); });
        other.join(); CHECK(rejected);
        CHECK(adapter.actor(bot)->bot);
        CHECK(!adapter.exit_bot(human,vehicle,0) && exits==0);
        CHECK(adapter.exit_bot(bot,vehicle,0) && exits==1);
        CHECK(adapter.enter(human,vehicle,0));
        CHECK(adapter.occupant(vehicle,0)==std::optional<Identity>(human));
        seat=0x999;
        CHECK(throws([&]{ adapter.occupant(vehicle,0); }));
        CHECK(throws([&]{ adapter.enter(bot,vehicle,0); }));
        image_valid=false;
        CHECK(throws([&]{ adapter.actor(human); }));
        image_valid=true;
        registry.destroyed(bot);
        const auto reused = registry.created(ObjectKind::player,0x200);
        CHECK(reused!=bot && !registry.resolve(bot,ObjectKind::player));
        CHECK(!registry.resolve(reused,ObjectKind::vehicle));
        registry.destroyed(bot); CHECK(registry.resolve(reused,ObjectKind::player));
        registry.level_destroyed();
        CHECK(!registry.resolve(vehicle,ObjectKind::vehicle));
        const auto next = registry.created(ObjectKind::vehicle,0x300);
        CHECK(next!=vehicle);
        adapter.deactivate(); CHECK(!adapter.ready());
        integrated_tests();
        std::cout << "PASS: adapter guards plus actual core integration (spawn, inside, outside, distance recheck, lifecycle destruction, unknown occupants)\n";
    } catch (const std::exception& e) { std::cerr << "FAIL: " << e.what() << '\n'; return 1; }
}
