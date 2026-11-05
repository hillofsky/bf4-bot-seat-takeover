#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cstddef>
#include <intrin.h>
struct InputState {
    uint64_t gp[7], rsi, rbx, r14;
    unsigned char xmm[6][16];
    uint32_t mxcsr, padding;
    uint64_t flags, rsp;
};
static_assert(offsetof(InputState, xmm) == 80, "ASM layout");
static_assert(offsetof(InputState, flags) == 184, "ASM layout");
extern "C" {
void SeatAddonFullObserver();
void InlineContinuation();
void ExerciseInlineSite();
void ClobberMachineState();
void* SeatAddonOriginalFull = reinterpret_cast<void*>(&InlineContinuation);
volatile long SeatAddonFullObserverActive = 0;
void* SelectedTarget = nullptr;
InputState Inputs{};
uint64_t ExpectedRsp = 0, StolenR9 = 0, StolenR10 = 0;
}
static bool allow, helperOk;
extern "C" __declspec(noinline) bool SeatAddonAllowFullVehicle(uintptr_t ui, uintptr_t vehicle) noexcept {
    helperOk = ui == 0x99aabbccddeeff00ULL && vehicle == 0x1122334455667788ULL &&
        SeatAddonFullObserverActive == 1 &&
        (reinterpret_cast<uintptr_t>(_AddressOfReturnAddress()) & 15) == 8;
    ClobberMachineState();
    return allow;
}
static void run(void* target) {
    Inputs = {};
    SelectedTarget = target;
    ExerciseInlineSite();
}
int main() {
    run(reinterpret_cast<void*>(&InlineContinuation));
    const InputState baseline = Inputs;
    if (baseline.rsp != ExpectedRsp || (baseline.rsp & 15)) return 1;
    for (int decision = 0; decision != 2; ++decision) {
        allow = decision != 0;
        InputState expected = baseline;
        if (allow) expected.rsi &= ~uint64_t(0xff);
        for (int i = 0; i < 10000; ++i) {
            helperOk = false;
            run(reinterpret_cast<void*>(&SeatAddonFullObserver));
            if (std::memcmp(&expected, &Inputs, sizeof(Inputs)) || !helperOk ||
                SeatAddonFullObserverActive != 0 || StolenR9 != 0 || StolenR10 != 0x0123456789abcdefULL) {
                std::printf("FAIL decision=%d iteration=%d helper=%d active=%ld\n", decision, i, helperOk, SeatAddonFullObserverActive);
                return 1;
            }
        }
        std::printf("GREEN %s 10000 inline JMPs: GP/XMM0-5/MXCSR/flags/RSP preserved; RAX manager intact; stolen 6 bytes execute; helper aligned; active 1->0\n", allow ? "allow (SIL cleared only)" : "reject (RSI unchanged)");
    }
    return 0;
}
