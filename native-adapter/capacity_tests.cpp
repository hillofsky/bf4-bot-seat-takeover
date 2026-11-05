#include "capacity_policy.hpp"
#include <algorithm>
#include <iostream>
#include <vector>
using Handle=std::uintptr_t;
using namespace bf4_takeover;
namespace {
int failures=0;
void check(bool value,const char* name) {
    if (!value) {std::cerr<<"RED: "<<name<<'\n';++failures;}
}
struct PermissionFixture {
    bool eligible=true,original_result=false,base_allowed=true,event_allowed=true,locked=false;
    int limit=2,original_calls=0,base_calls=0,event_calls=0,fit_calls=0;
    bool event_alive=false;
    Handle incoming=20,replaced=10;
    std::vector<Handle> occupants{10,30};
    bool run() {
        return derived_capacity_permission(eligible,limit,
            [&](){++original_calls;return original_result;},
            [&](){++base_calls;return base_allowed;},
            [&](auto fit){
                ++event_calls;event_alive=true;
                struct Cleanup {bool& alive;~Cleanup(){alive=false;}} cleanup{event_alive};
                if (!event_allowed || locked) return false;
                return fit();
            },
            [&](){++fit_calls;check(event_alive,"native event lifetime covers capacity query");
                return replacement_fits_capacity(limit,occupants,incoming,replaced);});
    }
};
struct EntryFixture {
    int limit=2;
    Handle incoming=20,replaced=10;
    std::vector<Handle> seats{10,30,0};
    bool bot_exited=false,bot_cycled=false;
    void enter() {
        const bool exclusive=replacement_requires_eviction(limit,seats,incoming);
        auto free=std::find(seats.begin(),seats.end(),Handle{0});
        // Native entry first tries cycling the old occupant. A spare physical
        // entry can exist even when the vehicle's simultaneous-player cap is full.
        if (!exclusive && free!=seats.end()) {*free=replaced;bot_cycled=true;}
        else bot_exited=true;
        for (auto& seat:seats) if (seat==incoming) seat=0;
        seats[0]=incoming;
    }
    int occupied() const {return static_cast<int>(std::count_if(seats.begin(),seats.end(),[](Handle p){return p!=0;}));}
};
}
int main() {
    PermissionFixture full;
    check(full.run(),"production dispatch permits full finite capacity-two replacement");
    check(full.original_calls==0 && full.base_calls==1 && full.event_calls==1 && full.fit_calls==1,
          "replacement dispatch checks base and sends one event, without original probe");
    check(!full.event_alive,"event cleaned after capacity query");
    for (const int limit:{0,-1,-2}) {
        PermissionFixture f;f.limit=limit;f.original_result=true;
        check(f.run() && f.original_calls==1 && !f.base_calls && !f.event_calls && !f.fit_calls,
              "nonpositive limit preserves one original call and no replacement events");
    }
    {PermissionFixture f;f.eligible=false;f.original_result=true;
     check(f.run() && f.original_calls==1 && !f.base_calls && !f.event_calls,
           "ineligible human/enemy/identity path delegates original once");}
    {PermissionFixture f;f.base_allowed=false;
     check(!f.run() && !f.original_calls && f.base_calls==1 && !f.event_calls && !f.fit_calls,
           "base health/team/forbidden permission denial preserved");}
    {PermissionFixture f;f.event_allowed=false;
     check(!f.run() && f.event_calls==1 && !f.fit_calls && !f.event_alive,
           "native permission event denial preserved and cleaned");}
    {PermissionFixture f;f.locked=true;
     check(!f.run() && f.event_calls==1 && !f.fit_calls && !f.event_alive,
           "vehicle lock denial preserved and cleaned");}
    {PermissionFixture f;f.occupants={10,30,40};
     check(!f.run(),"replacement does not hide other occupants above capacity");}
    {PermissionFixture f;f.limit=1;f.occupants={10,0};
     check(f.run(),"capacity-one M142 replacement retained");}
    {PermissionFixture f;f.limit=3;f.occupants={10,30,40};
     check(f.run(),"full finite capacity-three replacement allowed");}

    {EntryFixture f;f.enter();
     check(f.bot_exited && !f.bot_cycled && f.occupied()==2,
           "outside full capacity-two takeover ejects bot despite spare physical seat");}
    {EntryFixture f;f.seats={10,20,0};f.enter();
     check(!f.bot_exited && f.bot_cycled && f.occupied()==2,
           "inside legal capacity-two switch can retain bot without adding occupant");}
    {EntryFixture f;f.seats={10,20,30,0};f.enter();
     check(f.bot_exited && f.occupied()==2,
           "already overfull inside switch removes replaced bot");}
    {EntryFixture f;f.limit=3;f.enter();
     check(!f.bot_exited && f.bot_cycled && f.occupied()==3,
           "outside nonfull finite vehicle can retain bot");}
    {EntryFixture f;f.limit=-1;f.enter();
     check(!f.bot_exited && f.bot_cycled,"unlimited capacity preserves native cycling");}
    {EntryFixture f;f.limit=1;f.seats={10,0};f.enter();
     check(f.bot_exited && f.occupied()==1,"M142 spare weapon entry stays within cap one");}

    check(replacement_fits_capacity(1,{10,10},20,10),"replaced bot aliases excluded");
    check(!replacement_fits_capacity(1,{10},20,20),"requester cannot replace itself");
    if (failures) {std::cerr<<failures<<" failing checks\n";return 1;}
    std::cout<<"GREEN: production capacity dispatch, native permission/event/lock gates, and stateful seat eviction\n";
    return 0;
}
