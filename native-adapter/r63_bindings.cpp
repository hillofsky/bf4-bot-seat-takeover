#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "r63_bindings.hpp"
#include <stdexcept>
#include <utility>

namespace bf4_takeover {
R63EngineBridge::R63EngineBridge(R63RuntimeHost host)
    : host_(std::move(host)),reader_(guard_),actions_(guard_,reader_,action_host()) {}
R63ActionHost R63EngineBridge::action_host() {
    R63ActionHost h;
    h.gameplay_thread_id=host_.gameplay_thread_id;
    h.mutation_allowed=[this](NativeHandle p,NativeHandle v){
        return host_.mutation_allowed && host_.mutation_allowed(p,v);
    };
    h.trusted_bot=[this](NativeHandle p){
        const auto status=host_.actor_status ? host_.actor_status(p) : std::nullopt;
        return status && status->bot;
    };
    h.world_entry_allowed=[this](NativeHandle p,NativeHandle v){
        return host_.world_entry_allowed && host_.world_entry_allowed(p,v);
    };
    return h;
}
bool R63EngineBridge::prepare() {
    if (!host_.gameplay_thread_id || GetCurrentThreadId()!=host_.gameplay_thread_id ||
        host_.gameplay_callback_evidence.empty() || !host_.actor_status ||
        !host_.seat_allowed || !host_.mutation_allowed || !host_.world_entry_allowed)
        return false;
    return guard_.initialize().supported;
}
Bindings R63EngineBridge::bindings() {
    Bindings b;
    b.target_sha256="160ec988aad0632946936a21a9aed1c73f5b906161f2d96b2a085ee65ce183ba";
    b.abi_evidence="R63 entry bcaf50 / exit bd0510; passive native call observations, wrappers require live validation";
    b.gameplay_callback_evidence=host_.gameplay_callback_evidence;
    b.synchronous_mutations=true;
    b.validate_loaded_image=[this]{
        return host_.gameplay_thread_id==GetCurrentThreadId() && guard_.validate();
    };
    b.read_actor=[this](NativeHandle p)->std::optional<NativeActor>{
        if (!host_.actor_status) return {};
        const auto status=host_.actor_status(p);
        const auto seat=reader_.player(p);
        if (!status || !seat) return {};
        NativeActor a;
        a.team=seat->team; a.bot=status->bot; a.alive=status->alive;
        a.has_soldier=seat->soldier!=0; a.allowed_to_spawn=status->allowed_to_spawn;
        a.vehicle=seat->attached_vehicle;
        a.seat=a.vehicle ? static_cast<int>(seat->attached_seat) : -1;
        return a;
    };
    b.seat_allowed=[this](NativeHandle v,int seat,int team){
        const auto seats=reader_.vehicle(v);
        return seats && seat>=0 && static_cast<std::size_t>(seat)<seats->seats.size() &&
            host_.seat_allowed && host_.seat_allowed(v,seat,team);
    };
    b.occupant=[this](NativeHandle v,int seat)->NativeHandle{
        const auto seats=reader_.vehicle(v);
        if (!seats || seat<0 || static_cast<std::size_t>(seat)>=seats->seats.size())
            throw std::runtime_error("native seat unavailable");
        return seats->seats[seat].player;
    };
    b.exit_bot=[this](NativeHandle p,NativeHandle v,int s){
        return s>=0 && actions_.exit_bot(p,v,static_cast<std::uint32_t>(s));
    };
    b.enter=[this](NativeHandle p,NativeHandle v,int s){
        return s>=0 && actions_.enter(p,v,static_cast<std::uint32_t>(s));
    };
    b.can_enter_from_world=[this](NativeHandle p,NativeHandle v){
        return actions_.can_enter_from_world(p,v);
    };
    b.switch_seat=[this](NativeHandle p,NativeHandle v,int s){
        return s>=0 && actions_.switch_seat(p,v,static_cast<std::uint32_t>(s));
    };
    return b;
}
}
