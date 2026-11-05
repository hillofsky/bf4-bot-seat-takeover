#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <tlhelp32.h>
#include "client_image.hpp"
#include "deploy_shared.hpp"
#include "ui_thread_policy.hpp"
#include "override_registry.hpp"
#include "../vendor/minhook/include/MinHook.h"
#include <array>
#include <atomic>
#include <cstring>
#include <mutex>

extern "C" {
void SeatAddonFullObserver();
void* SeatAddonOriginalFull=nullptr;
volatile LONG SeatAddonFullObserverActive=0;
}

namespace {
using namespace bf4_deploy_client;
using Handle=std::uintptr_t;
using Render=void(__fastcall*)(Handle,Handle,Handle);
using SpawnVehicle=Handle(__fastcall*)(Handle);
using LocalPlayer=Handle(__fastcall*)();
ClientImage image;
HMODULE self_module=nullptr;
std::mutex lifecycle;
std::mutex shared_state;
OverrideRegistry<32> overrides;
std::atomic<bool> enabled{false};
std::atomic<unsigned> in_flight{0},ui_thread{0};
std::array<std::atomic<unsigned long long>,18> counters{};
std::array<char,64> human_name{};
Render original_render=nullptr;
std::array<void*,2> targets{};
bool installed=false;
HANDLE mapping=nullptr,server_process=nullptr;
const DeployShared* shared=nullptr;
DWORD cached_server=0;
ULONGLONG next_mapping_attempt=0;

struct Invocation {Invocation(){++in_flight;}~Invocation(){--in_flight;}};
template<class T> bool read(Handle base,Handle offset,T& out) noexcept {
    if(!base || offset>UINTPTR_MAX-base || sizeof(T)-1>UINTPTR_MAX-(base+offset)) return false;
    SIZE_T count=0;
    return ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<void*>(base+offset),&out,sizeof(T),&count) && count==sizeof(T);
}
bool write_byte(Handle address,unsigned char value) noexcept {
    SIZE_T count=0;
    return WriteProcessMemory(GetCurrentProcess(),reinterpret_cast<void*>(address),&value,sizeof(value),&count) && count==sizeof(value);
}
bool prefix(Handle address,const unsigned char* expected,std::size_t length) noexcept {
    std::array<unsigned char,32> bytes{};SIZE_T count=0;
    return length<=bytes.size() && ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<void*>(address),bytes.data(),length,&count) &&
        count==length && !std::memcmp(bytes.data(),expected,length);
}
bool configured_human(Handle player) noexcept {
    Handle begin=0,end=0;std::array<char,64> name{};
    if(!read(player,0x18,begin) || !read(player,0x20,end)) return false;
    begin&=~Handle{3};
    if(!begin || end<=begin || end-begin>=name.size()) return false;
    SIZE_T count=0;
    return ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<void*>(begin),name.data(),end-begin+1,&count) &&
        count==end-begin+1 && name[end-begin]==0 && !std::strcmp(name.data(),human_name.data());
}
bool player_identity(Handle player,unsigned& id,int& team) noexcept {
    Handle table=0,get_id=0;
    // Offline and live getter agree: ClientPlayer ID is +14f8, not server +1498.
    return read(player,0,table) && table==image.base()+0x1d17fa0 &&
        read(table,0x28,get_id) && get_id==image.base()+0x856590 &&
        read(player,0x14f8,id) && id<256 && read(player,0x13cc,team) && (team==1 || team==2);
}
void close_shared() noexcept {
    if(shared) UnmapViewOfFile(shared);
    if(mapping) CloseHandle(mapping);
    if(server_process) CloseHandle(server_process);
    shared=nullptr;mapping=nullptr;server_process=nullptr;cached_server=0;next_mapping_attempt=0;
}
bool proof(DeployShared& out) noexcept {
    const auto now=GetTickCount64();
    if(!shared) {
        if(now<next_mapping_attempt) return false;
        next_mapping_attempt=now+1000;
        mapping=OpenFileMappingW(FILE_MAP_READ,FALSE,mapping_name);
        if(!mapping) return false;
        shared=static_cast<const DeployShared*>(MapViewOfFile(mapping,FILE_MAP_READ,0,0,sizeof(DeployShared)));
        if(!shared) {CloseHandle(mapping);mapping=nullptr;return false;}
    }
    const LONG before=shared->sequence;
    if(before&1) return false;
    MemoryBarrier();
    std::memcpy(&out,shared,sizeof(out));
    MemoryBarrier();
    if(before!=shared->sequence || (out.sequence&1) || !fresh_proof(out,now)) return false;
    if(cached_server!=out.server_pid || !server_process) {
        if(server_process) CloseHandle(server_process);
        server_process=OpenProcess(SYNCHRONIZE|PROCESS_QUERY_LIMITED_INFORMATION,FALSE,out.server_pid);
        cached_server=out.server_pid;
    }
    return server_process && WaitForSingleObject(server_process,0)==WAIT_TIMEOUT;
}
bool point_value(const PointKey& key,unsigned char& value) noexcept {
    Handle current1=0,current2=0;
    return read(key.point,0x28,current1) && read(key.point,0x30,current2) &&
        current1==key.key1 && current2==key.key2 && read(key.point,0x13c,value);
}
bool set_point_value(const PointKey& key,unsigned char value) noexcept {
    unsigned char previous=0;
    return point_value(key,previous) && write_byte(key.point+0x13c,value);
}
struct ReadyOverride {
    OverrideLease lease;
    void restore() noexcept {
        if(!lease.valid) return;
        const DWORD error=GetLastError();
        {
            std::lock_guard<std::mutex> lock(shared_state);
            if(overrides.release(lease,point_value,set_point_value)==ReleaseResult::restore_failed) {
                ++counters[7];enabled.store(false);
            }
        }
        SetLastError(error);
    }
    ~ReadyOverride(){restore();}
};
struct VerifiedTarget {PointKey key;Handle vehicle=0;};
// All callers hold shared_state. This classification does not change readiness.
bool verified_target_locked(Handle ui,Handle exact_vehicle,VerifiedTarget& result) {
    DeployShared verified{};
    if(!enabled.load() || !image.validate()) return false;
    if(!proof(verified)) {++counters[9];return false;}
    ++counters[3];
    Handle point=0,table=0;
    PointKey key;
    unsigned char ready=0;
    if(!read(ui,0x680,point) || !point || !read(point,0,table) ||
        table!=image.base()+0x1d1f4a0 || !read(point,0x28,key.key1) || !read(point,0x30,key.key2) ||
        !read(point,0x13c,ready)) {++counters[10];return false;}
    key.point=point;
    if(ready>1) {++counters[10];return false;}
    const auto human=reinterpret_cast<LocalPlayer>(image.base()+0x3e4e60)();
    unsigned human_id=0;int human_team=0;
    if(!player_identity(human,human_id,human_team) || !configured_human(human) || human_team!=verified.team) {++counters[11];return false;}
    const auto vehicle=reinterpret_cast<SpawnVehicle>(image.base()+0x8a5bd0)(point);
    Handle begin=0,end=0,entry=0,occupant=0;
    if(!vehicle || (exact_vehicle && vehicle!=exact_vehicle) || !read(vehicle,0x110,begin) || !read(vehicle,0x118,end) || end<=begin ||
        (end-begin)%8 || (end-begin)/8>64 || !read(begin,0,entry) ||
        !read(entry,0x268,occupant) || !occupant || occupant==human) {++counters[12];return false;}
    unsigned bot_id=0;int bot_team=0;
    if(!player_identity(occupant,bot_id,bot_team) || bot_id!=verified.bot_id || bot_team!=verified.team) {++counters[13];return false;}
    DeployShared latest{};Handle current=0;
    if(!proof(latest) || latest.server_pid!=verified.server_pid || latest.bot_id!=bot_id || latest.team!=bot_team ||
        !read(ui,0x680,current) || current!=point || !read(entry,0x268,current) || current!=occupant ||
        !point_value(key,ready) || ready>1) {++counters[14];return false;}
    ++counters[4];
    result={key,vehicle};return true;
}
void prepare_override(Handle ui,ReadyOverride& change) {
    std::lock_guard<std::mutex> lock(shared_state);
    VerifiedTarget verified;
    if(!verified_target_locked(ui,0,verified)) return;
    unsigned char ready=0;
    if(!point_value(verified.key,ready) || ready>1 || (ready==1 && !overrides.owns(verified.key))) {++counters[10];return;}
    const auto result=overrides.acquire(verified.key,point_value,set_point_value,change.lease);
    if(result==AcquireResult::acquired) ++counters[5];
    else if(result==AcquireResult::write_failed) ++counters[6];
}
void __fastcall render_hook(Handle ui,Handle arguments,Handle render_context) {
    Invocation invocation;
    ++counters[2];
    ReadyOverride change;
    const DWORD error=GetLastError();
    if(accept_ui_callback(ui_thread,GetCurrentThreadId())) {
        try {prepare_override(ui,change);} catch(...) {++counters[6];}
    } else ++counters[8];
    SetLastError(error);
    original_render(ui,arguments,render_context);
    change.restore();
}
bool quiescent() noexcept {
    if(in_flight.load() || InterlockedCompareExchange(&SeatAddonFullObserverActive,0,0)) return false;
    const auto base=reinterpret_cast<Handle>(self_module);
    const auto dos=reinterpret_cast<const IMAGE_DOS_HEADER*>(self_module);
    const auto nt=reinterpret_cast<const IMAGE_NT_HEADERS64*>(base+dos->e_lfanew);
    const auto end=base+nt->OptionalHeader.SizeOfImage;
    const HANDLE snapshot=CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD,0);
    if(snapshot==INVALID_HANDLE_VALUE) return false;
    THREADENTRY32 entry{};entry.dwSize=sizeof(entry);
    bool safe=Thread32First(snapshot,&entry)!=FALSE;
    if(safe) do {
        if(entry.th32OwnerProcessID!=GetCurrentProcessId() || entry.th32ThreadID==GetCurrentThreadId()) continue;
        HANDLE thread=OpenThread(THREAD_SUSPEND_RESUME|THREAD_GET_CONTEXT|THREAD_QUERY_LIMITED_INFORMATION,FALSE,entry.th32ThreadID);
        if(!thread) {safe=false;break;}
        if(SuspendThread(thread)==DWORD(-1)) {CloseHandle(thread);safe=false;break;}
        CONTEXT context{};context.ContextFlags=CONTEXT_CONTROL;
        const bool captured=GetThreadContext(thread,&context)!=FALSE;
        const auto full_trampoline=reinterpret_cast<Handle>(SeatAddonOriginalFull);
        const auto render_trampoline=reinterpret_cast<Handle>(original_render);
        // MinHook x64 memory slots are 64 bytes, including their jump relay.
        const bool in_full=full_trampoline && context.Rip>=full_trampoline && context.Rip<full_trampoline+64;
        const bool in_render=render_trampoline && context.Rip>=render_trampoline && context.Rip<render_trampoline+64;
        const bool inside=captured && ((context.Rip>=base && context.Rip<end) || in_full || in_render);
        const bool resumed=ResumeThread(thread)!=DWORD(-1);
        CloseHandle(thread);
        if(!captured || inside || !resumed) {safe=false;break;}
    } while(Thread32Next(snapshot,&entry));
    CloseHandle(snapshot);return safe && !in_flight.load() && !InterlockedCompareExchange(&SeatAddonFullObserverActive,0,0);
}
bool set_hooks(bool active) {
    for(auto target:targets) {
        if(!target || (active?MH_QueueEnableHook(target):MH_QueueDisableHook(target))!=MH_OK) return false;
    }
    return MH_ApplyQueued()==MH_OK;
}
void remove_hooks() {
    for(auto target:targets) if(target) MH_RemoveHook(target);
    MH_Uninitialize();targets={};SeatAddonOriginalFull=nullptr;original_render=nullptr;
}
bool configuration() {
    std::array<wchar_t,32768> path{};
    const auto length=GetModuleFileNameW(self_module,path.data(),static_cast<DWORD>(path.size()));
    if(!length || length>=path.size()) return false;
    auto dot=wcsrchr(path.data(),L'.');
    if(!dot || static_cast<std::size_t>(dot-path.data())+5>=path.size()) return false;
    wcscpy_s(dot,path.size()-static_cast<std::size_t>(dot-path.data()),L".ini");
    std::array<wchar_t,64> configured{};
    return GetPrivateProfileStringW(L"SeatTakeover",L"HumanName",L"",configured.data(),static_cast<DWORD>(configured.size()),path.data()) &&
        WideCharToMultiByte(CP_UTF8,0,configured.data(),-1,human_name.data(),static_cast<int>(human_name.size()),nullptr,nullptr);
}
}

extern "C" bool SeatAddonAllowFullVehicle(std::uintptr_t ui,std::uintptr_t vehicle) noexcept {
    const DWORD error=GetLastError();
    ++counters[15];
    bool allowed=false;
    try {
        std::lock_guard<std::mutex> lock(shared_state);
        VerifiedTarget verified;
        allowed=vehicle && verified_target_locked(ui,vehicle,verified);
    } catch(...) {++counters[6];}
    ++counters[allowed?16:17];
    SetLastError(error);return allowed;
}

extern "C" __declspec(dllexport) DWORD WINAPI SeatAddonInstall(void*) {
    std::lock_guard<std::mutex> lock(lifecycle);
    if(installed) {
        if(enabled.load()) return 0;
        // A timed-out uninstall leaves the hook allocation owned by this DLL.
        // Resume that same allocation; never treat a disabled module as enabled.
        if(counters[7].load()) return 9;
        if(!set_hooks(true)) return 6;
        enabled.store(true);return 0;
    }
    try {
        if(!image.initialize()) return 1;
        if(!configuration()) return 7;
        constexpr unsigned char render_prefix[]={0x48,0x8b,0xc4,0x55,0x56,0x57,0x41,0x54,0x41,0x55,0x41,0x56,0x41,0x57,0x48,0x8d,0xa8,0xe8,0xfd,0xff,0xff,0x48,0x81,0xec};
        constexpr unsigned char full_prefix[]={0x45,0x33,0xc9,0x4c,0x8b,0x10,0x44,0x0f,0xb6,0xc6,0xba,0x98,0x1b,0x73,0x0d};
        constexpr unsigned char lookup_prefix[]={0x40,0x53,0x48,0x81,0xec,0xa0,0x00,0x00,0x00,0x48,0xc7,0x44,0x24,0x30,0xfe,0xff,0xff,0xff};
        constexpr unsigned char local_prefix[]={0x48,0x8b,0x05,0x19,0xbf,0x28,0x02,0x48,0x85,0xc0,0x75,0x03,0x33,0xc0,0xc3};
        constexpr unsigned char id_prefix[]={0x8b,0x81,0xf8,0x14,0x00,0x00,0xc3};
        if(!prefix(image.base()+0x406aa0,render_prefix,sizeof(render_prefix)) ||
            !prefix(image.base()+0x4064f1,full_prefix,sizeof(full_prefix)) ||
            !prefix(image.base()+0x8a5bd0,lookup_prefix,sizeof(lookup_prefix)) ||
            !prefix(image.base()+0x3e4e60,local_prefix,sizeof(local_prefix)) ||
            !prefix(image.base()+0x856590,id_prefix,sizeof(id_prefix))) return 3;
        if(MH_Initialize()!=MH_OK) return 4;
        targets={reinterpret_cast<void*>(image.base()+0x406aa0),reinterpret_cast<void*>(image.base()+0x4064f1)};
        if(MH_CreateHook(targets[0],reinterpret_cast<void*>(&render_hook),reinterpret_cast<void**>(&original_render))!=MH_OK ||
            MH_CreateHook(targets[1],reinterpret_cast<void*>(&SeatAddonFullObserver),&SeatAddonOriginalFull)!=MH_OK) {
            remove_hooks();return 5;
        }
        // Once activation is attempted, keep allocations owned by this module
        // on failure. A later safe uninstall can retry; never free a busy relay.
        installed=true;
        if(!set_hooks(true)) {enabled.store(false);set_hooks(false);return 6;}
        enabled.store(true);return 0;
    } catch(...) {return 8;}
}
extern "C" __declspec(dllexport) unsigned long long SeatAddonCounter(unsigned slot) {
    if(slot==0) return enabled.load()?1:0;
    if(slot==1) return ui_thread.load();
    if(slot==18) return static_cast<unsigned long long>(InterlockedCompareExchange(&SeatAddonFullObserverActive,0,0));
    return slot<counters.size()?counters[slot].load():0;
}
extern "C" __declspec(dllexport) DWORD WINAPI SeatAddonUninstall(void*) {
    std::lock_guard<std::mutex> lock(lifecycle);
    if(!installed) return 0;
    enabled.store(false);
    if(!set_hooks(false)) return 2;
    const auto start=GetTickCount64();
    while(!quiescent()) {if(GetTickCount64()-start>2000) return 1;Sleep(1);}
    remove_hooks();close_shared();installed=false;ui_thread.store(0);return 0;
}
BOOL WINAPI DllMain(HINSTANCE module,DWORD reason,LPVOID) {if(reason==DLL_PROCESS_ATTACH) self_module=module;return TRUE;}
