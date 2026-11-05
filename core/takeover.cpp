#include "takeover.hpp"

namespace bf4_takeover {

bool Takeover::valid_target(const Selection& s, int team) {
    const auto bot = adapter_.actor(s.bot);
    return bot && bot->identity == s.bot && bot->bot && bot->alive &&
        bot->has_soldier && bot->team == team && bot->vehicle &&
        *bot->vehicle == s.vehicle && bot->seat == s.seat && s.seat >= 0 &&
        adapter_.seat_allowed_for_human(s.vehicle, s.seat, team) &&
        adapter_.occupant(s.vehicle, s.seat) == std::optional<Identity>(s.bot);
}

QueueResult Takeover::queue(Identity human, Selection s, Tick now) {
    if (transferring_) return QueueResult::not_ready;
    try {
        const auto p = adapter_.actor(human);
        if (!p || p->identity != human || p->bot || p->team <= 0 ||
            p->alive || p->has_soldier || !p->allowed_to_spawn)
            return QueueResult::not_ready;
        // Reject clock reversal and stale host-side selections without overflow.
        if (now < s.created_at || now - s.created_at > 30000 ||
            !valid_target(s, p->team))
            return QueueResult::invalid_selection;
        pending_.insert_or_assign(human.id, Pending{human, s, p->team, now});
        return QueueResult::queued;
    } catch (...) {
        return QueueResult::adapter_error;
    }
}

bool Takeover::restore_if_empty(const Pending& request) noexcept {
    try {
        const auto bot = adapter_.actor(request.selection.bot);
        if (!bot || bot->identity != request.selection.bot || !bot->bot ||
            !bot->alive || !bot->has_soldier || bot->team != request.team ||
            bot->vehicle ||
            !adapter_.seat_allowed_for_human(request.selection.vehicle,
                                             request.selection.seat, request.team) ||
            adapter_.occupant(request.selection.vehicle, request.selection.seat))
            return false;
        adapter_.enter(request.selection.bot, request.selection.vehicle,
                       request.selection.seat);
        return adapter_.occupant(request.selection.vehicle, request.selection.seat)
            == std::optional<Identity>(request.selection.bot);
    } catch (...) {
        return false;
    }
}

SpawnOutcome Takeover::on_native_spawn(Identity human, Tick now) {
    if (transferring_) return {SpawnResult::busy};
    auto found = pending_.find(human.id);
    if (found == pending_.end()) return {SpawnResult::no_request};
    const Pending request = found->second;
    // A late callback for a disconnected/reused slot must not consume the new
    // connection's pending request.
    if (request.human != human) return {SpawnResult::target_changed};
    // Consume once before any engine callback can re-enter this function.
    pending_.erase(found);
    if (now < request.queued_at || now - request.queued_at > 60000)
        return {SpawnResult::expired};

    return transfer(request, false, false);
}

SpawnOutcome Takeover::take_driver_from_inside(Identity human, Selection s, Tick now) {
    return take_driver(human, s, now, true);
}

SpawnOutcome Takeover::take_driver_from_outside(Identity human, Selection s, Tick now) {
    return take_driver(human, s, now, false);
}

SpawnOutcome Takeover::take_driver(Identity human, Selection s, Tick now, bool inside) {
    if (transferring_) return {SpawnResult::busy};
    if (s.seat != 0 || now < s.created_at || now - s.created_at > 30000)
        return {SpawnResult::target_changed};
    try {
        const auto p = adapter_.actor(human);
        if (!p || p->identity != human || p->bot || p->team <= 0)
            return {SpawnResult::human_not_ready};
        return transfer(Pending{human, s, p->team, now}, inside, !inside);
    } catch (...) {
        return {SpawnResult::adapter_error};
    }
}

SpawnOutcome Takeover::transfer(const Pending& request, bool inside, bool check_world_entry) {
    if (transferring_) return {SpawnResult::busy};
    transferring_ = true;
    struct Reset { bool& flag; ~Reset() { flag = false; } } reset{transferring_};
    const Identity human = request.human;
    int original_seat = -1;
    const auto ready = [&](const std::optional<Actor>& p) {
        if (!p || p->identity != human || p->bot || p->team != request.team ||
            !p->alive || !p->has_soldier) return false;
        if (inside)
            return p->vehicle == std::optional<Identity>(request.selection.vehicle) &&
                p->seat > 0 && (original_seat < 0 || p->seat == original_seat) &&
                adapter_.occupant(request.selection.vehicle, p->seat) ==
                    std::optional<Identity>(human);
        return !p->vehicle && (!check_world_entry ||
            adapter_.can_enter_from_world(human, request.selection.vehicle));
    };

    bool attempted_exit = false;
    try {
        const auto p = adapter_.actor(human);
        if (!ready(p))
            return {SpawnResult::human_not_ready};
        original_seat = p->seat;
        if (!valid_target(request.selection, request.team))
            return {SpawnResult::target_changed};

        attempted_exit = true;
        const bool exited = adapter_.exit_bot(request.selection.bot,
            request.selection.vehicle, request.selection.seat);
        if (!exited) {
            return {SpawnResult::exit_failed, restore_if_empty(request)};
        }
        if (adapter_.occupant(request.selection.vehicle, request.selection.seat))
            return {SpawnResult::target_changed};

        // Exit callbacks may destroy a vehicle, change teams, or place another
        // occupant. Revalidate before a second mutation and never eject humans.
        const auto still_human = adapter_.actor(human);
        const auto still_bot = adapter_.actor(request.selection.bot);
        if (!ready(still_human) ||
            !still_bot || still_bot->identity != request.selection.bot ||
            !still_bot->bot || still_bot->team != request.team ||
            !still_bot->alive || !still_bot->has_soldier || still_bot->vehicle ||
            !adapter_.seat_allowed_for_human(request.selection.vehicle,
                                            request.selection.seat, request.team) ||
            adapter_.occupant(request.selection.vehicle, request.selection.seat)) {
            return {SpawnResult::target_changed, restore_if_empty(request)};
        }

        // Verify engine state rather than trusting the entry API's return code.
        if (inside)
            adapter_.switch_seat(human, request.selection.vehicle, request.selection.seat);
        else
            adapter_.enter(human, request.selection.vehicle, request.selection.seat);
        if (adapter_.occupant(request.selection.vehicle, request.selection.seat)
            == std::optional<Identity>(human))
            return {SpawnResult::transferred};
        return {SpawnResult::transfer_failed, restore_if_empty(request)};
    } catch (...) {
        // An engine wrapper can report an exception after it actually succeeded.
        try {
            if (attempted_exit &&
                adapter_.occupant(request.selection.vehicle, request.selection.seat)
                    == std::optional<Identity>(human))
                return {SpawnResult::transferred};
        } catch (...) {}
        return {SpawnResult::adapter_error,
                attempted_exit && restore_if_empty(request)};
    }
}

void Takeover::player_left(Identity human) {
    const auto p = pending_.find(human.id);
    if (p != pending_.end() && p->second.human == human) pending_.erase(p);
}

} // namespace bf4_takeover
