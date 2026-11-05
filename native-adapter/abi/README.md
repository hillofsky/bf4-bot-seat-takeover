# Transparent native control observers

This directory is self-contained. It neither injects code nor opens game processes.
The standalone test calls only the local assembly `FakeNative` function.

## Integration interface

Compile `boat_observer.asm` with ML64 and link the resulting object into the addon.
Both procedure names are public COFF symbols (DLL exports need the usual linker
export option if desired):

```cpp
extern "C" {
    void SeatAddonBoatObserver();
    void SeatAddonGroundObserver();
    void* SeatAddonOriginalBoat;    // executable trampoline, initialized before attaching
    void* SeatAddonOriginalGround;  // executable trampoline, initialized before attaching
    volatile long SeatAddonObserverActive; // 32-bit LONG; initialized to zero
    void SeatAddonObserveBoat(void* originalRcx); // implement noexcept; shared by both
}
```

The C++ declarations for observer procedures are address declarations only.
Never call these entry points through a guessed native function signature.
Each hook receives the actual native entry state, calls the observer with its
original RCX, restores state, and jumps indirectly to its corresponding trampoline.
The trampoline therefore receives the original caller's return address and RSP.
There is no C++ call forwarding and no addon code executes after the native return.

Both procedures preserve incoming RFLAGS, RAX/RCX/RDX/R8/R9/R10/R11,
XMM0 through XMM5, and MXCSR. They leave all nonvolatile registers untouched;
the helper must obey the Windows x64 ABI for those registers. The helper call
has 32 bytes of shadow space and 16-byte aligned RSP. The shared active counter
is incremented atomically immediately after PUSHFQ, and decremented atomically
immediately before POPFQ and the tail jump.

## Build and evidence

From the workspace root:

```bat
work\bf4-takeover\native-adapter\abi\build_test.cmd
```

The script calls the installed VS2017 Professional `vcvars64.bat`, then runs:

```bat
ml64 /nologo /c /Zi /Fo boat_observer.obj boat_observer.asm
ml64 /nologo /c /Zi /Fo probe.obj probe.asm
cl /nologo /std:c++17 /EHsc /O2 /W4 /WX /Zi test.cpp boat_observer.obj probe.obj /Fe:abi_test.exe /link /incremental:no
abi_test.exe
```

Observed output on this machine:

```text
OLD RED: input fields changed=14, return state changed=1
THUNKS GREEN: Boat + Ground, 10000 calls each; 7 GP, XMM0-5, MXCSR, RFLAGS, native RSP/return address, raw RAX/XMM0 return preserved; active 1->0
```

The old path uses an actual optimized C++ wrapper forwarding two declared integer
arguments with pre-observation and destructor work after the native call. Its
helper deliberately overwrites volatile GP/SIMD state, flags, and MXCSR. The red
result demonstrates an ABI hazard of that wrapping pattern; it does not prove
which particular register or state caused the live game's behavior.

The green path is compared against a direct call from the same assembly call
instruction. `FakeNative` snapshots entry GP/SIMD state, MXCSR, flags, RSP, and
the return address from the stack, then returns raw integer/SIMD sentinels. The
assembly caller snapshots returned RAX, XMM0, flags, RSP, and MXCSR. Both thunks
match the direct path across 10,000 calls each even with the clobbering helper.
The observer additionally verifies original RCX and an active counter of one;
the caller verifies the counter returns to zero.

## Limits

* This covers the listed SSE/GP state. Upper YMM/ZMM, x87, debug registers, and
  thread-local Windows state such as LastError are outside this harness. The
  production helper must preserve any Windows/thread-local state it changes.
* The helper must return normally and must not throw, longjmp, or allow an SEH
  exception to escape. Such an escape skips restoration and counter decrement.
* MASM FRAME/ALLOCSTACK annotations describe the prologue and allocated body
  frame. The final stack adjustment/counter decrement/POPFQ/tail-jump sequence
  is not a canonical Windows x64 unwindable epilog. Asynchronous unwinding while
  inside that small sequence is not guaranteed. No custom unwind handler is
  supplied or tested.
* Counter zero alone does not authorize unloading: entry instructions before
  increment and final instructions after decrement still execute addon code.
  The installer/uninstaller must prevent new entry and inspect suspended thread
  instruction pointers across the full observer/helper/trampoline ranges.
* The tests are single-threaded and local. They do not prove game stability,
  hook overwrite correctness, instruction relocation correctness, or live
  behavioral equivalence. No production file or game process was modified.
