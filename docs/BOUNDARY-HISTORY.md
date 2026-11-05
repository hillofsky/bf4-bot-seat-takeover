# 초기 추상 경계의 기록

이 문서는 초기 설계 단계의 기록입니다. 아래 미구현/미검증 표시는 당시 추상 경계에 대한 설명이며 현재 0.10 적용 상태가 아닙니다. 현재 상태는 루트 README와 VALIDATION.md를 기준으로 합니다.

# Native binding boundary — not a BF4 engine implementation

This portable C++17 boundary compiles on Windows and implements the existing
`Adapter` interface. It has **no BF4 addresses, native ABI definitions, hook
installer, injection code, or DLL entry point** in that boundary. Default activation fails closed.

It requires a complete version-specific host `Bindings` implementation, exact
supported server SHA-256, ABI and gameplay callback provenance, a loaded-image
validator and synchronous native operations. Labels alone do not establish ABI
correctness: trusted native code must verify that separately.

`Registry` maps host-observed creation/destruction callbacks to monotonic opaque
identities. Native address reuse and map changes cannot revive an old identity.
The host must deliver every object lifecycle callback on the gameplay thread,
call both core and registry map cleanup, and deactivate the adapter before engine
unload. Recreating the registry requires recreating the takeover core too.

Unknown occupants and untracked attached vehicles cause errors rather than
appearing as empty seats. A second check prevents evicting a human. Wrong-thread
and invalid-image calls reject before native callbacks. The registered gameplay
thread must itself originate from a verified game callback; this boundary cannot
discover it or authenticate commands.

`server_image_guard.cpp` supplies a real Windows WinAPI/BCrypt disk identity
guard. `inspect_server_image` checks the exact supported disk SHA-256, size,
AMD64 PE headers and version fingerprint. `validate_current_server_image` obtains
the current process main module path/base itself and additionally checks its
loaded header fingerprint. This full inspection hashes the entire 43 MB file;
do not bind it directly to a per-operation callback. It does not verify every runtime code byte, defeat
in-process hooks, or prove function ABIs. Calling it from an external helper
correctly rejects the helper rather than validating another running process.
`image_guard_tests.cpp` hashes the real server file at its configured local path
and verifies that the helper process is rejected. It does not execute the server.

Use a long-lived `CurrentServerImageGuard` for the future native host instead.
Call `initialize()` once during verified gameplay initialization; it hashes the
disk file and retains the loaded module base and 1536-byte header fingerprint.
After successful initialization assign `Bindings::validate_loaded_image` to
`[&guard] { return guard.validate(); }`. `validate()` only checks the current
main-module base and reads/compares those loaded headers; no disk I/O or SHA is
performed for core reads/mutations. It rejects an uninitialized/reset guard.
The host must serialize initialization/reset and adapter calls on the gameplay
thread, deactivate before teardown, and keep the guard alive while captured.
This is startup disk identity plus ongoing loaded-header consistency, not
ongoing disk integrity or runtime instruction/ABI attestation. A disk file change
after initialization or a patch outside the headers is outside its coverage.

`R63SeatReader` is a read-only current-process layout observer and requires that
prepared image guard. It reads the statically recovered player attachment,
controlled-object and team fields, and the vehicle's entry pointer vector
(`vehicle+0x1a0/0x1a8`, 8-byte entries, occupant at `entry+0xb8`). Attached player
seat indices must resolve back to the same player. It checks address arithmetic,
failed/partial reads and a design bound of 64 entries (not an observed engine
maximum). Player and current-controlled state remain separate. A soldier pointer
does not prove life; no bot/alive classification or native mutation is provided.
Reads require a gameplay-thread callback and are not atomic engine snapshots.
The isolated `r63_layout_detail` helpers bypass image validation for synthetic
memory tests only; they remain read-only and are not a native binding.

`seat_reader_tests.cpp` validates actual ReadProcessMemory parsing against
synthetic buffers in its own process, malformed vector bounds, inconsistent
seat occupants, unreadable/overflowing pointers and the production guard gate.
It has not observed a live BF4 player/vehicle through this reader.

`R63VehicleActions` now supplies actual version-specific x64 mutation call sites,
based on the parent investigation's static analysis and passive observation of
normal engine calls. Entry at main-module RVA `0xbcaf50` takes
`(ServerPlayer*, vehicle*, uint32_t seat)` and returns an AL boolean. Physical exit
at RVA `0xbd0510` takes `(ServerPlayer*, byte, byte)`; the wrapper requests the
observed ordinary-exit flags `0,1`. Normal engine calls were observed; these
wrappers themselves have not yet been called in BF4.

Before native calls the wrapper requires the prepared exact-image guard, a
host-issued verified gameplay thread ID, a host mutation-authorization callback,
and a trusted bot predicate. It reads/validates seat state, rejects occupied target
seats, checks executable memory and exact observed prologue bytes, then calls
the native function and verifies attachment/occupancy afterward. `exit_bot`
requires the trusted bot predicate to positively identify the occupant. The
entry function also supports an internal seat change without calling physical
exit on the human. It does not implement generic occupied-seat eviction.

The host must still provide verified callbacks for bot classification, actor
life/permissions, identity lifecycle, selection authentication and gameplay event
delivery. `world_entry_allowed` remains a mandatory host-side check for outside
entry eligibility; it is not guessed from distance. `enter` is a lower-level
operation also used by spawn/restoration paths, so host/core policy must enforce
the appropriate request context. No installer fabricates any of these hooks.

`vehicle_actions_tests.cpp` checks rejection with an uninitialized image guard
and in the actual non-BF4 helper process. No test invokes these native mutation
functions; live wrapper invocation and integration remain unverified.

Remaining integration work: real player/bot/life classification, object lifecycle
callbacks, verified server-thread callback installation, deployment completion,
live validation of the new entry/exit/seat-switch wrappers, normal entry checks, synchronized state,
authenticated selection and integration tests. Queued input events do not meet
the synchronous callback contract.

`tests.cpp` uses **fake bindings only**; successful tests do not establish actual
BF4 functionality. The executable links the actual takeover core and exercises
normal spawn transfer, in-vehicle seat switching, outside entry and distance
revalidation, vehicle destruction in an exit callback, and unknown occupants
before/after exit. Build from this directory using `build.cmd`.
