# FULL inline-site state adapter

`full_observer.asm` supplies the public COFF symbol `SeatAddonFullObserver`.
It targets a JMP hook at the verified inline site after CALL 0x88d3b0, where
RSP is 16-byte aligned, R14 holds UI context, RBX holds vehicle, SIL is FULL,
and RAX holds the UI manager. This is not an ordinary function-call entry.

Required C linkage definitions in the integrating DLL:

```cpp
extern "C" {
    void SeatAddonFullObserver(); // address declaration, not a normal C++ callable function
    void* SeatAddonOriginalFull;  // executable trampoline; initialized before attaching
    volatile long SeatAddonFullObserverActive; // Windows LONG, initial value 0
    bool SeatAddonAllowFullVehicle(uintptr_t ui, uintptr_t vehicle) noexcept;
}
```

The observer saves flags before the atomic counter increment, saves volatile
GP registers RAX/RCX/RDX/R8/R9/R10/R11, RSI, XMM0 through XMM5, and MXCSR,
then calls the helper with R14/RBX mapped to RCX/RDX. Its call stack is aligned
and has 32 bytes of shadow space. False leaves every saved state field intact.
True clears only the low byte of restored RSI. It restores RAX including the
native UI manager, decrements the counter before restoring flags, and tail jumps
through `SeatAddonOriginalFull`. The trampoline must execute the original six
bytes `45 33 C9 4C 8B 10` and continue the original native path.

## Build and test

From workspace root:

```bat
work\bf4-takeover\native-client\abi\build_test.cmd
```

The script calls installed VS2017 Professional `vcvars64.bat`, then executes:

```bat
ml64 /nologo /c /Zi /Fo full_observer.obj full_observer.asm
ml64 /nologo /c /Zi /Fo probe.obj probe.asm
cl /nologo /std:c++17 /EHsc /O2 /W4 /WX /Zi test.cpp full_observer.obj probe.obj /Fe:abi_test.exe /link /incremental:no
abi_test.exe
```

For DLL integration, compile only `full_observer.asm` and link its object with
the DLL sources. Neither `probe.asm` nor `test.cpp` belongs in the game DLL.

The actual optimized test passed 10,000 rejection and 10,000 allowance paths.
An assembly caller jumps to the target with RSP aligned to zero; no return
address is synthesized at the inline hook boundary. The baseline jumps directly
to the same continuation. The continuation captures all listed registers,
SIMD state, MXCSR, flags, and RSP, then executes the exact stolen instructions.
The helper verifies UI/vehicle arguments, call alignment, and active count of
one, deliberately clobbers volatile GP/SIMD/MXCSR/flags state, and returns the
selected bool. Rejection matches the baseline byte for byte. Allowance matches
with only SIL cleared. Both preserve the original RAX value and produce the
expected R9=0 and R10=[RAX] after the stolen bytes. The active counter returns
to zero after every iteration.

## Limits

The harness does not call any game addresses or modify a live process. It
establishes the adapter's local machine-state behavior, not game compatibility,
relocation correctness, or gameplay results. Tests are single-threaded.

The helper must obey Win64 nonvolatile-register rules, return normally, and
preserve any thread-local state such as LastError that it changes. No C++/SEH
exception or longjmp may escape. Upper YMM/ZMM state and x87 state are outside
the save set and test scope.

The FRAME annotations describe the adapter's local allocation only. An inline
JMP enters from an existing native frame; these annotations do not describe
that original frame or establish valid cross-hook exception unwinding or
asynchronous stack walking. The final POPFQ/tail-jump restore sequence also
is not a canonical Windows unwind epilog. No unwind guarantee is made.

Counter zero is not sufficient to unload code: instructions before increment
and after decrement still execute inside the adapter. Prevent fresh entries
and inspect suspended threads across the complete adapter/helper/trampoline
ranges before freeing code or unloading the module.
