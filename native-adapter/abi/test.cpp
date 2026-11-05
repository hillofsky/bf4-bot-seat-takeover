#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cstddef>

struct Inputs {
    uint64_t gp[7];
    unsigned char xmm[6][16];
    uint32_t mxcsr, padding;
    uint64_t flags, rsp, returnAddress;
};
struct Returns {
    uint64_t rax;
    unsigned char xmm0[16];
    uint64_t flags, rsp;
    uint32_t mxcsr, padding;
};
static_assert(offsetof(Inputs, flags) == 160, "ASM layout");
static_assert(offsetof(Returns, flags) == 24, "ASM layout");
extern "C" {
void SeatAddonBoatObserver();
void SeatAddonGroundObserver();
void FakeNative();
void ExerciseTarget();
void ClobberMachineState();
void* SeatAddonOriginalBoat = reinterpret_cast<void*>(&FakeNative);
void* SeatAddonOriginalGround = reinterpret_cast<void*>(&FakeNative);
volatile long SeatAddonObserverActive = 0;
void* SelectedTarget = nullptr;
Inputs NativeInputs{};
Returns NativeReturns{};
uint64_t ExpectedRsp = 0, ExpectedReturn = 0;
}
static uint64_t observedContext;
static long observedActive;
extern "C" __declspec(noinline) void SeatAddonObserveBoat(void* context) {
    observedContext = reinterpret_cast<uint64_t>(context);
    observedActive = SeatAddonObserverActive;
    ClobberMachineState();
}
struct PostWork { ~PostWork() { ClobberMachineState(); } };
extern "C" __declspec(noinline) uint64_t OldWrapper(uint64_t a, uint64_t b) {
    PostWork after;
    SeatAddonObserveBoat(reinterpret_cast<void*>(a));
    return reinterpret_cast<uint64_t(*)(uint64_t, uint64_t)>(SeatAddonOriginalBoat)(a, b);
}

static int differences(const Inputs& a, const Inputs& b) {
    int n = 0;
    for (int i = 0; i < 7; ++i) n += a.gp[i] != b.gp[i];
    for (int i = 0; i < 6; ++i) n += std::memcmp(a.xmm[i], b.xmm[i], 16) != 0;
    n += a.mxcsr != b.mxcsr;
    n += a.flags != b.flags;
    n += a.rsp != b.rsp;
    n += a.returnAddress != b.returnAddress;
    return n;
}
static void run(void* target) {
    NativeInputs = {}; NativeReturns = {};
    SelectedTarget = target;
    ExerciseTarget();
}
int main() {
    run(reinterpret_cast<void*>(&FakeNative));
    const Inputs baseline = NativeInputs;
    const Returns baselineReturn = NativeReturns;
    bool ok = baseline.rsp == ExpectedRsp && baseline.returnAddress == ExpectedReturn;
    run(reinterpret_cast<void*>(&OldWrapper));
    const int oldDifferences = differences(baseline, NativeInputs);
    const bool oldReturnChanged = std::memcmp(&baselineReturn, &NativeReturns, sizeof(Returns)) != 0;
    std::printf("OLD RED: input fields changed=%d, return state changed=%d\n", oldDifferences, oldReturnChanged);
    ok &= oldDifferences > 0 && oldReturnChanged;
    void* targets[] = { reinterpret_cast<void*>(&SeatAddonBoatObserver), reinterpret_cast<void*>(&SeatAddonGroundObserver) };
    for (void* target : targets) for (int i = 0; i < 10000; ++i) {
        run(target);
        if (differences(baseline, NativeInputs) || std::memcmp(&baselineReturn, &NativeReturns, sizeof(Returns)) ||
            observedContext != baseline.gp[1] || observedActive != 1 || SeatAddonObserverActive != 0) {
            std::printf("THUNK FAILED iteration=%d input differences=%d active=%ld\n", i, differences(baseline, NativeInputs), SeatAddonObserverActive);
            return 1;
        }
    }
    std::printf("THUNKS GREEN: Boat + Ground, 10000 calls each; 7 GP, XMM0-5, MXCSR, RFLAGS, native RSP/return address, raw RAX/XMM0 return preserved; active 1->0\n");
    return ok ? 0 : 1;
}
