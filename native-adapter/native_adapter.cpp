#include "native_adapter.hpp"
#include <stdexcept>
#include <utility>

namespace bf4_takeover {
Identity Registry::created(ObjectKind kind, NativeHandle handle) {
    if (!handle || identify(handle, kind)) throw std::invalid_argument("duplicate/null object");
    if (next_ == 0) throw std::overflow_error("identity space exhausted");
    const Identity id{next_++, 1};
    entries_.emplace(id.id, Entry{id,kind,handle});
    return id;
}
void Registry::destroyed(Identity id) {
    const auto it = entries_.find(id.id);
    if (it != entries_.end() && it->second.identity == id) entries_.erase(it);
}
void Registry::level_destroyed() { entries_.clear(); } // Never reset next_: address reuse is distinct.
std::optional<NativeHandle> Registry::resolve(Identity id, ObjectKind kind) const {
    const auto it = entries_.find(id.id);
    if (it == entries_.end() || it->second.identity != id || it->second.kind != kind) return {};
    return it->second.handle;
}
std::optional<Identity> Registry::identify(NativeHandle handle, ObjectKind kind) const {
    for (const auto& p : entries_)
        if (p.second.handle == handle && p.second.kind == kind) return p.second.identity;
    return {};
}
bool NativeAdapter::activate(Bindings b) {
    // Activation is host code, never a user command or a client-supplied address.
    static const std::string expected =
        "160ec988aad0632946936a21a9aed1c73f5b906161f2d96b2a085ee65ce183ba";
    deactivate();
    if (b.target_sha256 != expected || b.abi_evidence.empty() ||
        b.gameplay_callback_evidence.empty() || !b.synchronous_mutations ||
        !b.validate_loaded_image || !b.read_actor || !b.seat_allowed || !b.occupant ||
        !b.exit_bot || !b.enter || !b.can_enter_from_world || !b.switch_seat) return false;
    try { if (!b.validate_loaded_image()) return false; } catch (...) { return false; }
    gameplay_thread_ = std::this_thread::get_id();
    bindings_ = std::move(b); active_ = true;
    return true;
}
void NativeAdapter::deactivate() { active_ = false; bindings_ = {}; gameplay_thread_ = {}; }
bool NativeAdapter::ready() const {
    return active_ && gameplay_thread_ == std::this_thread::get_id();
}
void NativeAdapter::require_ready() const {
    if (!ready()) throw std::runtime_error("native binding inactive or wrong thread");
    if (!bindings_.validate_loaded_image()) throw std::runtime_error("native image no longer valid");
}
std::optional<Actor> NativeAdapter::actor(Identity id) {
    require_ready();
    const auto h = registry_.resolve(id, ObjectKind::player);
    if (!h) return {};
    const auto p = bindings_.read_actor(*h);
    if (!p) return {};
    std::optional<Identity> v;
    if (p->vehicle) {
        v = registry_.identify(p->vehicle, ObjectKind::vehicle);
        if (!v) throw std::runtime_error("untracked native vehicle");
    }
    return Actor{id,p->team,p->bot,p->alive,p->has_soldier,p->allowed_to_spawn,v,p->seat};
}
bool NativeAdapter::seat_allowed_for_human(Identity vehicle, int seat, int team) {
    require_ready(); const auto v = registry_.resolve(vehicle,ObjectKind::vehicle);
    return v && seat >= 0 && team > 0 && bindings_.seat_allowed(*v,seat,team);
}
std::optional<Identity> NativeAdapter::occupant(Identity vehicle, int seat) {
    require_ready(); const auto v = registry_.resolve(vehicle,ObjectKind::vehicle);
    if (!v) throw std::runtime_error("vehicle destroyed");
    const auto h = bindings_.occupant(*v,seat);
    if (!h) return {};
    const auto id = registry_.identify(h,ObjectKind::player);
    if (!id) throw std::runtime_error("untracked occupant is not an empty seat");
    return id;
}
bool NativeAdapter::exit_bot(Identity bot, Identity vehicle, int seat) {
    require_ready(); const auto p = actor(bot);
    const auto h = registry_.resolve(bot,ObjectKind::player);
    const auto v = registry_.resolve(vehicle,ObjectKind::vehicle);
    return p && h && v && p->bot && p->alive && p->has_soldier &&
        p->vehicle == std::optional<Identity>(vehicle) && p->seat == seat &&
        occupant(vehicle,seat) == std::optional<Identity>(bot) && bindings_.exit_bot(*h,*v,seat);
}
bool NativeAdapter::enter(Identity id, Identity vehicle, int seat) {
    require_ready(); const auto p = actor(id);
    const auto h = registry_.resolve(id,ObjectKind::player);
    const auto v = registry_.resolve(vehicle,ObjectKind::vehicle);
    return p && h && v && p->alive && p->has_soldier && !p->vehicle &&
        seat_allowed_for_human(vehicle,seat,p->team) && !occupant(vehicle,seat) &&
        bindings_.enter(*h,*v,seat);
}
bool NativeAdapter::can_enter_from_world(Identity id, Identity vehicle) {
    require_ready(); const auto p = actor(id);
    const auto h = registry_.resolve(id,ObjectKind::player);
    const auto v = registry_.resolve(vehicle,ObjectKind::vehicle);
    return p && h && v && !p->bot && p->alive && p->has_soldier && !p->vehicle &&
        bindings_.can_enter_from_world(*h,*v);
}
bool NativeAdapter::switch_seat(Identity id, Identity vehicle, int seat) {
    require_ready(); const auto p = actor(id);
    const auto h = registry_.resolve(id,ObjectKind::player);
    const auto v = registry_.resolve(vehicle,ObjectKind::vehicle);
    return p && h && v && !p->bot && p->alive && p->has_soldier &&
        p->vehicle == std::optional<Identity>(vehicle) && p->seat != seat &&
        occupant(vehicle,p->seat) == std::optional<Identity>(id) &&
        seat_allowed_for_human(vehicle,seat,p->team) && !occupant(vehicle,seat) &&
        bindings_.switch_seat(*h,*v,seat);
}
}
