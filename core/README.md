# Bot seat takeover core (C++17)

This is an engine-independent state machine, **not an installable BF4 mod**.
It follows the working BF3 VU design: select while dead, allow normal native
deployment, then revalidate and transfer the new soldier into an allied bot seat.

The abstract adapter deliberately has no kill, soldier creation, loadout change,
team change, or teleport methods. A failed transfer attempts bot restoration only
when the seat is still vacant and the same living allied bot is still eligible.
Native callbacks must be serialized on the gameplay thread.

Live driver takeover is also implemented in the core:

- `take_driver_from_inside`: living passenger in the same vehicle takes seat 0
  through a native seat switch, without exiting the human soldier.
- `take_driver_from_outside`: living human takes seat 0 after the adapter checks
  ordinary interaction distance and vehicle entry rules, before and after bot exit.

Both accept only a fresh trusted selection of an allied bot driver. Reentrant
transfers return `busy`. Failed seat changes must preserve the original human
seat; an empty driver seat permits bot restoration. Adapter mutations must be
synchronous and state-observable on return; queued input events do not meet this
contract. Native entry, interaction checks, and seat switching remain unbound.

Native BF4 work still required before installation:

- trusted player/vehicle generation identities and bot classification;
- human deployment-complete event with a fully initialized soldier;
- seat permission/occupancy, exit, entry, and post-entry synchronization;
- an authenticated selection UI or command and connection/level lifecycle hooks;
- integration tests in the real game, including high-speed aircraft and gunners.

`tests.cpp` supplies an in-memory adapter to test the real core decision path.
It does not claim to verify any BF4 engine address, ABI, event timing or live UI.
