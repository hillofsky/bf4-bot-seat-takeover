#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <tlhelp32.h>
#include "driver_policy.hpp"
#include "capacity_policy.hpp"
#include "deployment_proof.hpp"
#include "bot_identity.hpp"
#include "../vendor/minhook/include/MinHook.h"
#include <array>
#include <atomic>
#include <cstring>
#include <memory>
#include <mutex>

extern "C" {
void SeatAddonBoatObserver();
void SeatAddonGroundObserver();
void* SeatAddonOriginalBoat=nullptr;
void* SeatAddonOriginalGround=nullptr;
volatile LONG SeatAddonObserverActive=0;
}
using namespace bf4_takeover;
namespace {
using Handle=std::uintptr_t;
using CanEnter=bool(__fastcall*)(Handle,Handle,std::uint32_t);
using SelectEntry=std::uint32_t(__fastcall*)(Handle,Handle,std::uint32_t,Handle);
using NativeEnter=bool(__fastcall*)(Handle,Handle,std::uint32_t);
using NativeCycle=bool(__fastcall*)(Handle);
using SpawnVehicle=bool(__fastcall*)(Handle,Handle);
using FindSeat=int(__fastcall*)(Handle,Handle);
SpawnVehicle original_spawn_vehicle=nullptr;
FindSeat original_find_seat=nullptr;
using MessageHandler=void(__fastcall*)(Handle,Handle);
MessageHandler original_network=nullptr;
struct SpawnContext { Handle player=0,vehicle=0; };
SpawnContext spawning;
struct DeploymentLease {Handle human=0,bot=0,vehicle=0,point=0,human_cookie=0,bot_soldier=0;unsigned bot_id=0;int team=0;ULONGLONG refreshed=0;};
DeploymentLease deployment_lease;
void refresh_deployment_proof();
void clear_deployment_lease() {deployment_lease={};publish_deployment_proof(false,0,0);}
CanEnter original_permission=nullptr;
SelectEntry original_select=nullptr;
CanEnter original_derived=nullptr;
NativeEnter original_enter=nullptr;
NativeCycle original_cycle=nullptr;
CurrentServerImageGuard image;
std::unique_ptr<R63SeatReader> reader;
std::unique_ptr<DriverPolicy> policy;
std::mutex lifecycle;
std::atomic<bool> enabled{false};
std::atomic<unsigned> in_flight{0},game_thread{0};
std::array<std::atomic<unsigned long long>,16> counters{};
struct BotRecord { Handle player=0,context=0; unsigned id=0; ULONGLONG seen=0; BotIdentitySnapshot identity; };
std::array<BotRecord,256> bots{};
std::array<void*,10> targets{};
HMODULE self_module=nullptr;
std::array<char,64> human_name{};
struct Transfer { Handle human=0,bot=0,vehicle=0; bool exclusive=false; };
Transfer active_transfer;
bool installed=false;
struct Invocation { Invocation(){++in_flight;} ~Invocation(){--in_flight;} };

template<class T> bool read(Handle base,Handle offset,T& result) {
    Handle address=0,end=0;
    if (!r63_layout_detail::checked_address(base,offset,address) ||
        !r63_layout_detail::checked_address(address,sizeof(T)-1,end)) return false;
    SIZE_T size=0;
    return ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<void*>(address),
        &result,sizeof(T),&size) && size==sizeof(T);
}
bool gameplay() { return game_thread.load()!=0 && GetCurrentThreadId()==game_thread.load(); }
bool read_name(Handle player,std::array<char,64>& name) {
    Handle begin=0,end=0;
    if (!read(player,0x18,begin) || !read(player,0x20,end)) return false;
    begin&=~Handle{3};
    if (!begin || end<=begin || end-begin>=name.size()) return false;
    name={};SIZE_T count=0;
    return ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<void*>(begin),name.data(),end-begin+1,&count) &&
        count==end-begin+1 && name[end-begin]==0;
}
bool configured_human(Handle player) {
    std::array<char,64> name{};
    return human_name[0] && read_name(player,name) && std::strcmp(name.data(),human_name.data())==0;
}
bool roster_owner(unsigned id,Handle& player) {
    Handle context=0,manager=0,array=0;unsigned count=0;
    return read(image.module_base(),0x2c68148,context) && context &&
        read(context,0x60,manager) && manager && read(manager,0x10,count) && count<=256 && id<count &&
        read(manager,0x928,array) && array && read(array,Handle{id}*8,player);
}
bool known_bot(Handle player) {
    if (!gameplay()) return false;
    unsigned id=0;
    if (!read(player,0x1498,id) || id>=bots.size()) return false;
    auto& b=bots[id];
    if (b.player!=player || !b.context || !b.identity.player) return false;
    BotIdentityLive live;live.player=player;live.id=id;live.now=GetTickCount64();
    if (!read(b.context,0xbf50,live.context_owner) || !roster_owner(id,live.roster_owner) ||
        !read(player,0x1488,live.input_cookie) || !read_name(player,live.name)) return false;
    const bool valid=matches_live_bot(b.identity,live);
    if (!valid) b.identity={};
    return valid;
}
void record_bot(Handle context) {
    Handle player=0; unsigned id=0;
    if (!read(context,0xbf50,player) || !player || !read(player,0x1498,id) || id>=bots.size()) return;
    unsigned expected=0;
    game_thread.compare_exchange_strong(expected,GetCurrentThreadId());
    if (!gameplay()) return;
    auto& b=bots[id];
    if (!b.player) ++counters[2];
    Handle cookie=0,current=0;
    b.player=player;b.context=context;b.id=id;b.seen=GetTickCount64();
    if (!read(player,0x1488,cookie) || !cookie || !roster_owner(id,current) || current!=player) {
        b.identity={};refresh_deployment_proof();return;
    }
    if (b.identity.player!=player || b.identity.context!=context || b.identity.id!=id || b.identity.input_cookie!=cookie) {
        b.identity={};BotIdentitySnapshot identity;
        identity.player=player;identity.context=context;identity.id=id;identity.input_cookie=cookie;identity.observed_at=b.seen;
        if (read_name(player,identity.name)) b.identity=identity;
    }
    refresh_deployment_proof();
}
bool native_spawn_context(Handle vehicle,Handle player) {
    return gameplay() && spawning.player==player && spawning.vehicle==vehicle && player && vehicle;
}
bool candidate(Handle vehicle,Handle player,std::uint32_t seat) {
    if (!enabled.load() || !gameplay() || !reader || known_bot(player) || !configured_human(player)) return false;
    const auto p=reader->player(player);
    const auto v=reader->vehicle(vehicle);
    if (!p || (!p->soldier && !native_spawn_context(vehicle,player)) || (p->team!=1 && p->team!=2) ||
        (p->attached_vehicle && p->attached_vehicle!=vehicle) || !v || seat>=v->seats.size()) return false;
    const auto driver=v->seats[seat].player;
    if (!driver || driver==player || !known_bot(driver)) return false;
    const auto d=reader->player(driver);
    unsigned char requester_ai=0,driver_ai=0;
    return d && d->soldier && d->team==p->team && d->attached_vehicle==vehicle && d->attached_seat==seat &&
        read(player,0x13c8,requester_ai) && requester_ai==0 &&
        read(driver,0x13c8,driver_ai) && driver_ai==0;
}
// Publish only while the exact selection still points at a live bot driver.
// The proof is a UI affordance; native spawn checks still authorize deployment.
void refresh_deployment_proof() {
    if (!deployment_lease.human || !reader || !gameplay()) return;
    auto& lease=deployment_lease;const auto now=GetTickCount64();
    if (now-lease.refreshed<50) return;
    lease.refreshed=now;
    const auto human=reader->player(lease.human),bot=reader->player(lease.bot);
    const auto vehicle=reader->vehicle(lease.vehicle);
    Handle cookie=0,reference=0,object=0;unsigned offset=0;
    const bool valid=enabled.load() && human && bot && vehicle && !vehicle->seats.empty() &&
        !human->soldier && !human->attached_vehicle && human->team==lease.team &&
        configured_human(lease.human) && read(lease.human,0x1488,cookie) && cookie==lease.human_cookie &&
        known_bot(lease.bot) && bot->team==lease.team && bot->soldier==lease.bot_soldier &&
        bot->attached_vehicle==lease.vehicle && bot->attached_seat==0 && vehicle->seats[0].player==lease.bot &&
        read(image.module_base(),0x25e3ff8,offset) && offset<=0x10000 &&
        read(lease.human+offset,0x50,reference) && reference && read(reference,0,object) &&
        object>=8 && object-8==lease.vehicle;
    if (!valid) {clear_deployment_lease();return;}
    publish_deployment_proof(true,lease.bot_id,lease.team);
}
bool __fastcall permission_hook(Handle vehicle,Handle player,std::uint32_t seat) {
    Invocation scope;
    const bool original=original_permission(vehicle,player,seat);
    if (original || !enabled.load() || !gameplay()) return original;
    try {
        if (!candidate(vehicle,player,seat)) return false;
        ++counters[8];
        if (!policy || !policy->eligible(vehicle,player,seat,native_spawn_context(vehicle,player))) return false;
        ++counters[5];
        const auto p=reader->player(player);
        if (p && p->attached_vehicle==vehicle) ++counters[3];
        // The derived vehicle permission function continues after this return.
        return true;
    } catch (...) { ++counters[6]; return false; }
}
bool capacity(Handle vehicle,int& limit) {
    Handle data=0;
    return read(vehicle,0x30,data) && data && read(data,0x250,limit);
}
bool __fastcall derived_hook(Handle vehicle,Handle player,std::uint32_t seat) {
    Invocation scope;
    int limit=0;
    const bool eligible=candidate(vehicle,player,seat) && capacity(vehicle,limit);
    return derived_capacity_permission(eligible,limit,
        [&](){return original_derived(vehicle,player,seat);},
        [&](){return permission_hook(vehicle,player,seat);},
        [&](auto fits) {
            // Recovered R63 message layout from bfef6f..bff026. Send the same
            // native permission event once; retain its message through fit().
            using MessageFunction=void(__fastcall*)(void*);
            using MessageBus=Handle(__fastcall*)(Handle);
            using SendMessage=void(__fastcall*)(Handle,void*);
            alignas(16) std::array<unsigned char,64> message{};
            const auto base=image.module_base();
            reinterpret_cast<MessageFunction>(base+0x1c5b5e0)(message.data());
            struct MessageCleanup {
                Handle function; void* message;
                ~MessageCleanup(){reinterpret_cast<MessageFunction>(function)(message);}
            } cleanup{base+0x7125b0,message.data()};
            std::memcpy(message.data()+0x28,&vehicle,sizeof(vehicle));
            std::memcpy(message.data()+0x30,&seat,sizeof(seat));
            const auto bus=reinterpret_cast<MessageBus>(base+0xc2cf90)(vehicle);
            reinterpret_cast<SendMessage>(base+0x715920)(bus,message.data());
            unsigned char locked=0;
            if (!message[0x34] || !read(vehicle,0x230,locked) || locked) return false;
            return fits();
        },
        [&]() {
            const auto seats=reader->vehicle(vehicle);
            if (!seats || seat>=seats->seats.size()) return false;
            const auto driver=seats->seats[seat].player;
            if (!known_bot(driver)) return false;
            std::vector<Handle> occupants;
            for (const auto& entry:seats->seats) occupants.push_back(entry.player);
            if (!replacement_fits_capacity(limit,occupants,player,driver)) return false;
            ++counters[9];
            return true;
        });
}

bool __fastcall cycle_hook(Handle player) {
    Invocation scope;
    if (enabled.load() && gameplay() && active_transfer.exclusive &&
        active_transfer.bot==player && known_bot(player)) {
        // A full finite-capacity vehicle cannot retain the departing occupant in a spare entry.
        // Native entry's existing fallback performs a complete physical exit.
        return false;
    }
    return original_cycle(player);
}
bool __fastcall enter_hook(Handle player,Handle vehicle,std::uint32_t seat) {
    Invocation scope;
    if (!candidate(vehicle,player,seat)) return original_enter(player,vehicle,seat);
    const auto before=reader->player(player);
    const auto seats=reader->vehicle(vehicle);
    if (!before || !seats || seat>=seats->seats.size()) return original_enter(player,vehicle,seat);
    const auto bot=seats->seats[seat].player;
    const auto old_bot=reader->player(bot);
    int limit=0;
    const Transfer saved=active_transfer;
    active_transfer=Transfer{player,bot,vehicle,[&]() {
        if (!capacity(vehicle,limit)) return false;
        std::vector<Handle> occupants;
        for (const auto& entry:seats->seats) occupants.push_back(entry.player);
        return replacement_requires_eviction(limit,occupants,player);
    }()};
    struct RestoreContext { Transfer value; ~RestoreContext(){active_transfer=value;} } context{saved};
    const bool result=original_enter(player,vehicle,seat);
    const auto after=reader->player(player);
    if (result && after && after->soldier==before->soldier && after->attached_vehicle==vehicle && after->attached_seat==seat) {
        ++counters[7]; return true;
    }
    ++counters[10];
    const auto remaining=reader->vehicle(vehicle);
    const auto bot_after=reader->player(bot);
    if (remaining && seat<remaining->seats.size() && !remaining->seats[seat].player &&
        old_bot && bot_after && old_bot->soldier==bot_after->soldier &&
        !bot_after->attached_vehicle && known_bot(bot)) {
        original_enter(bot,vehicle,seat);
    }
    return result;
}
std::uint32_t __fastcall select_hook(Handle interaction,Handle vehicle,
                                    std::uint32_t seat,Handle player) {
    Invocation scope;
    const auto original=original_select(interaction,vehicle,seat,player);
    if (seat!=0 || original==0 || !enabled.load() || !gameplay()) return original;
    try {
        if (!candidate(vehicle,player,0)) return original;
        const auto p=reader->player(player);
        if (!p || p->attached_vehicle) return original;
        Handle vtable=0,function=0;
        MEMORY_BASIC_INFORMATION memory{};
        if (!read(vehicle,0,vtable) || !read(vtable,0x230,function) ||
            function<image.module_base() || function-image.module_base()>=51347456 ||
            !VirtualQuery(reinterpret_cast<void*>(function),&memory,sizeof(memory)) ||
            memory.State!=MEM_COMMIT || (memory.Protect&(PAGE_GUARD|PAGE_NOACCESS)) ||
            !(memory.Protect&(PAGE_EXECUTE|PAGE_EXECUTE_READ|PAGE_EXECUTE_READWRITE|PAGE_EXECUTE_WRITECOPY)))
            return original;
        // Run the complete derived engine policy, including this module's
        // narrowly extended base AI classification. Do not alter AI flags.
        if (!reinterpret_cast<CanEnter>(function)(vehicle,player,0)) return original;
        ++counters[4];
        // The ordinary E-key handler performs actual seat transfer afterward.
        return 0;
    } catch (...) { ++counters[6]; return original; }
}
// This scope comes only from the engine's ordinary vehicle deployment path.
// Its team, health, spawn timing and loadout creation gates still execute.
bool __fastcall spawn_vehicle_hook(Handle component,Handle vehicle) {
    Invocation scope;
    unsigned offset=0;
    if (!enabled.load() || !gameplay() || !read(image.module_base(),0x25e3ff8,offset) ||
        offset>0x10000 || component<offset || !configured_human(component-offset))
        return original_spawn_vehicle(component,vehicle);
    const SpawnContext saved=spawning;
    spawning={component-offset,vehicle};
    struct Restore { SpawnContext saved; ~Restore(){spawning=saved;} } restore{saved};
    return original_spawn_vehicle(component,vehicle);
}
int __fastcall find_seat_hook(Handle vehicle,Handle player) {
    Invocation scope;
    const int original=original_find_seat(vehicle,player);
    if (original!=-1 || !enabled.load() || !native_spawn_context(vehicle,player) ||
        !candidate(vehicle,player,0)) return original;
    Handle table=0,function=0;
    if (!read(vehicle,0,table) || !read(table,0x230,function) ||
        function<image.module_base() || function-image.module_base()>=51347456) return original;
    // Run the derived permission too; native deployment creates the soldier
    // with its selected equipment, then ordinary entry performs the takeover.
    return reinterpret_cast<CanEnter>(function)(vehicle,player,0)?0:original;
}
// A blocked UI point creates soldiers, not vehicles. Its sibling vehicle
// spawner is resolved with the same getter used by native c39700. Never enable
// the generic point or call its creation callback for this takeover.
void __fastcall network_selection_hook(Handle handler,Handle message) {
    Invocation scope;
    unsigned message_class=0,message_type=0;Handle source_connection=0,source_player=0;
    if (gameplay() && read(message,8,message_class) && message_class==0xc67d277b &&
        read(message,0x0c,message_type) && (message_type==0x71d53cbc || message_type==0xb3db72c3) &&
        read(message,0x28,source_connection) && source_connection &&
        read(source_connection,0xad0,source_player) && configured_human(source_player)) clear_deployment_lease();
    original_network(handler,message);
    if (!enabled.load() || !gameplay()) return;
    try {
        unsigned type=0,kind=0,selected_actor=0;Handle connection=0,player=0,ref=0,tagged=0,live=0;
        if (!read(message,8,kind) || kind!=0xc67d277b || !read(message,0x0c,type) || type!=0x71d53cbc ||
            !read(message,0x60,selected_actor) || selected_actor!=0xffffffff ||
            !read(message,0x28,connection) || !connection ||
            !read(connection,0xad0,player) || !configured_human(player)) return;
        Handle soldier=0;if (!read(player,0x14a8,soldier) || soldier) return;
        using ConnectionAllowed=bool(__fastcall*)(Handle);
        if (!reinterpret_cast<ConnectionAllowed>(image.module_base()+0xbe5cd0)(connection)) return;
        if (!read(message,0x40,ref) || !ref || !read(ref,0,live) || !live ||
            !read(message,0x48,tagged)) return;
        const Handle network=tagged&~Handle{3};if (network<0x38) return;
        const Handle point=network-0x38;Handle table=0;unsigned char ready=0;int team=0;
        const auto requester=reader->player(player);
        if (!read(point,0,table) || table!=image.module_base()+0x1fef1d0 ||
            !read(point,0x24c,ready) || ready || !read(point,0x170,team) ||
            !requester || requester->team!=team) return;
        using GetSpawner=Handle(__fastcall*)(Handle);
        const auto spawner=reinterpret_cast<GetSpawner>(image.module_base()+0xc41700)(point);
        Handle begin=0,end=0,vehicle=0,owner=0;
        if (!spawner || !read(spawner,0x148,begin) || !read(spawner,0x150,end) ||
            end<=begin || (end-begin)%8 || (end-begin)/8>64 ||
            !read(end-8,0,vehicle) || !vehicle || !read(vehicle,0x1e0,owner) || owner!=spawner) return;
        const SpawnContext saved=spawning;spawning={player,vehicle};
        struct Restore { SpawnContext saved; ~Restore(){spawning=saved;} } restore{saved};
        if (!candidate(vehicle,player,0) || !policy || !policy->eligible(vehicle,player,0,true)) return;
        Handle vtable=0,function=0;
        if (!read(vehicle,0,vtable) || !read(vtable,0x230,function) ||
            function<image.module_base() || function-image.module_base()>=51347456 ||
            !reinterpret_cast<CanEnter>(function)(vehicle,player,0)) return;
        using SelectVehicle=void(__fastcall*)(Handle,Handle);
        unsigned offset=0;if (!read(image.module_base(),0x25e3ff8,offset) || offset>0x10000) return;
        Handle selected_reference=0,selected_object=0;
        if (!read(player+offset,0x48,selected_reference)) return;
        if (selected_reference && (!read(selected_reference,0,selected_object) || selected_object)) return;
        reinterpret_cast<SelectVehicle>(image.module_base()+0x25e2b0)(player+offset,vehicle);
        const auto target=reader->vehicle(vehicle);
        unsigned id=0;
        if (!target || target->seats.empty() || !read(target->seats[0].player,0x1498,id)) return;
        const auto bot=reader->player(target->seats[0].player);Handle cookie=0;
        if (!bot || !read(player,0x1488,cookie)) return;
        deployment_lease={player,target->seats[0].player,vehicle,point,cookie,bot->soldier,id,team,0};
        refresh_deployment_proof();
        counters[12].store(id+1);
        counters[14].store(vehicle); counters[15].store(point); ++counters[13];
    } catch (...) {++counters[6];}
}
bool prefix(Handle address,const unsigned char* expected,std::size_t length) {
    std::array<unsigned char,32> bytes{}; SIZE_T count=0;
    return length<=bytes.size() && ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<void*>(address),
        bytes.data(),length,&count) && count==length && std::memcmp(bytes.data(),expected,length)==0;
}
// Hooks are disabled before this scan. Counters cover helpers/native returns;
// the RIP scan covers instructions before increment and after decrement.
bool addon_quiescent() {
    if (in_flight.load() || InterlockedCompareExchange(&SeatAddonObserverActive,0,0)) return false;
    const auto base=reinterpret_cast<Handle>(self_module);
    const auto dos=reinterpret_cast<const IMAGE_DOS_HEADER*>(self_module);
    const auto nt=reinterpret_cast<const IMAGE_NT_HEADERS64*>(base+dos->e_lfanew);
    const auto end=base+nt->OptionalHeader.SizeOfImage;
    const HANDLE snapshot=CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD,0);
    if (snapshot==INVALID_HANDLE_VALUE) return false;
    THREADENTRY32 entry{};entry.dwSize=sizeof(entry);
    bool safe=Thread32First(snapshot,&entry)!=FALSE;
    if (safe) do {
        if (entry.th32OwnerProcessID!=GetCurrentProcessId() || entry.th32ThreadID==GetCurrentThreadId()) continue;
        HANDLE thread=OpenThread(THREAD_SUSPEND_RESUME|THREAD_GET_CONTEXT|THREAD_QUERY_LIMITED_INFORMATION,FALSE,entry.th32ThreadID);
        if (!thread) { safe=false;break; }
        if (SuspendThread(thread)==DWORD(-1)) { CloseHandle(thread);safe=false;break; }
        CONTEXT context{};context.ContextFlags=CONTEXT_CONTROL;
        const bool captured=GetThreadContext(thread,&context)!=FALSE;
        const bool inside=captured && context.Rip>=base && context.Rip<end;
        const bool resumed=ResumeThread(thread)!=DWORD(-1);
        CloseHandle(thread);
        if (!captured || inside || !resumed) {safe=false;break;}
    } while(Thread32Next(snapshot,&entry));
    CloseHandle(snapshot);
    return safe && !in_flight.load() && !InterlockedCompareExchange(&SeatAddonObserverActive,0,0);
}
void remove_hooks() {
    clear_deployment_lease();close_deployment_proof();
    for (auto target:targets) if (target) MH_RemoveHook(target);
    MH_Uninitialize(); targets={};
}
}

// The assembly observer restores machine state and tail-jumps into native code.
// No guessed C++ return ABI or changed native return address is involved.
extern "C" void SeatAddonObserveBoat(std::uintptr_t context) noexcept {
    const DWORD error=GetLastError();
    if (enabled.load()) {try {record_bot(context);} catch (...) {++counters[6];}}
    SetLastError(error);
}

// Explicit entry point: never install hooks while holding the loader lock.
// 0 success; 1 wrong image; 2 bot module not ready/mismatched; 3 native prefix
// mismatch; 4 hook initialization; 5 hook creation; 6 hook activation.
extern "C" __declspec(dllexport) DWORD WINAPI SeatAddonInstall(void*) {
    std::lock_guard<std::mutex> lock(lifecycle);
    if (installed) return 0;
    if (!image.initialize().supported) return 1;
    std::array<wchar_t,32768> ini_path{};
    const auto path_length=GetModuleFileNameW(self_module,ini_path.data(),static_cast<DWORD>(ini_path.size()));
    if (!path_length || path_length>=ini_path.size()) return 7;
    wchar_t* dot=wcsrchr(ini_path.data(),L'.');
    if (!dot || static_cast<std::size_t>(dot-ini_path.data())+5>=ini_path.size()) return 7;
    wcscpy_s(dot,ini_path.size()-static_cast<std::size_t>(dot-ini_path.data()),L".ini");
    std::array<wchar_t,64> configured{};
    if (!GetPrivateProfileStringW(L"SeatTakeover",L"HumanName",L"",configured.data(),static_cast<DWORD>(configured.size()),ini_path.data())) return 7;
    if (!WideCharToMultiByte(CP_UTF8,0,configured.data(),-1,human_name.data(),static_cast<int>(human_name.size()),nullptr,nullptr)) return 7;
    const auto bot_module=GetModuleHandleW(L"DINPUT8.dll");
    if (!bot_module) return 2;
    const auto bot_base=reinterpret_cast<Handle>(bot_module);
    constexpr unsigned char bot_prefix[]={0x48,0x89,0x5c,0x24,0x18,0x55,0x56,0x57,0x41,0x54,0x41,0x55,0x41,0x56,0x41,0x57};
    constexpr unsigned char boat_prefix[]={0x48,0x8b,0xc4,0x48,0x89,0x58,0x18,0x55,0x56,0x57,0x41,0x54,0x41,0x55,0x41,0x56,0x41,0x57,0x48,0x8d,0xa8,0xd8,0xf2,0xff};
    constexpr unsigned char base_prefix[]={0x40,0x53,0x56,0x41,0x57,0x48,0x83,0xec,0x20,0x48,0x8b,0x81,0xa8,0x01,0x00,0x00};
    constexpr unsigned char select_prefix[]={0x40,0x55,0x56,0x41,0x54,0x41,0x55,0x48,0x83,0xec,0x28,0x48,0x8b,0xea};
    constexpr unsigned char derived_prefix[]={0x40,0x57,0x48,0x83,0xec,0x60,0x48,0xc7,0x44,0x24,0x20,0xfe,0xff,0xff,0xff};
    constexpr unsigned char enter_prefix[]={0x48,0x8b,0xc4,0x41,0x56,0x48,0x83,0xec,0x60,0x48,0xc7,0x40,0xb8,0xfe,0xff,0xff};
    constexpr unsigned char network_prefix[]={0x48,0x8b,0xc4,0x55,0x41,0x54,0x41,0x55,0x41,0x56,0x41,0x57,0x48,0x8d,0x68,0xa1};
    constexpr unsigned char getter_prefix[]={0x40,0x53,0x48,0x81,0xec,0xa0,0,0,0,0x48,0xc7,0x44,0x24,0x30,0xfe,0xff};
    constexpr unsigned char spawn_prefix[]={0x48,0x8b,0xc4,0x57,0x48,0x81,0xec,0x90,0,0,0,0x48,0xc7,0x40,0x88,0xfe};
    constexpr unsigned char find_prefix[]={0x41,0x83,0xc8,0xff,0xe9,0x07,0,0,0};
    constexpr unsigned char cycle_prefix[]={0x48,0x89,0x5c,0x24,0x20,0x57,0x48,0x83,0xec,0x20,0x48,0x8b,0xb9,0xc8,0x14,0x00};
    if (!prefix(bot_base+0x54730,bot_prefix,sizeof(bot_prefix)) ||
        !prefix(bot_base+0x60b20,boat_prefix,sizeof(boat_prefix))) return 2;
    const auto base=image.module_base();
    if (!prefix(base+0xbcfc20,base_prefix,sizeof(base_prefix)) ||
        !prefix(base+0xc3e9e0,select_prefix,sizeof(select_prefix)) ||
        !prefix(base+0xbfef40,derived_prefix,sizeof(derived_prefix)) ||
        !prefix(base+0xbcaf50,enter_prefix,sizeof(enter_prefix)) ||
        !prefix(base+0xbc9060,cycle_prefix,sizeof(cycle_prefix)) ||
        !prefix(base+0x25f460,spawn_prefix,sizeof(spawn_prefix)) ||
        !prefix(base+0xbcb720,find_prefix,sizeof(find_prefix)) ||
        !prefix(base+0x237f80,network_prefix,sizeof(network_prefix)) ||
        !prefix(base+0xc41700,getter_prefix,sizeof(getter_prefix))) return 3;
    if (!open_deployment_proof()) return 8;
    if (MH_Initialize()!=MH_OK) {close_deployment_proof();return 4;}
    reader=std::make_unique<R63SeatReader>(image);
    policy=std::make_unique<DriverPolicy>(image,known_bot,gameplay);
    targets={reinterpret_cast<void*>(bot_base+0x54730),reinterpret_cast<void*>(base+0xbcfc20),reinterpret_cast<void*>(base+0xc3e9e0),reinterpret_cast<void*>(base+0xbfef40),reinterpret_cast<void*>(base+0xbcaf50),reinterpret_cast<void*>(base+0xbc9060),reinterpret_cast<void*>(bot_base+0x60b20),reinterpret_cast<void*>(base+0x25f460),reinterpret_cast<void*>(base+0xbcb720),reinterpret_cast<void*>(base+0x237f80)};
    if (MH_CreateHook(targets[0],reinterpret_cast<void*>(&SeatAddonGroundObserver),&SeatAddonOriginalGround)!=MH_OK ||
        MH_CreateHook(targets[1],reinterpret_cast<void*>(&permission_hook),reinterpret_cast<void**>(&original_permission))!=MH_OK ||
        MH_CreateHook(targets[2],reinterpret_cast<void*>(&select_hook),reinterpret_cast<void**>(&original_select))!=MH_OK ||
        MH_CreateHook(targets[3],reinterpret_cast<void*>(&derived_hook),reinterpret_cast<void**>(&original_derived))!=MH_OK ||
        MH_CreateHook(targets[4],reinterpret_cast<void*>(&enter_hook),reinterpret_cast<void**>(&original_enter))!=MH_OK ||
        MH_CreateHook(targets[5],reinterpret_cast<void*>(&cycle_hook),reinterpret_cast<void**>(&original_cycle))!=MH_OK ||
        MH_CreateHook(targets[6],reinterpret_cast<void*>(&SeatAddonBoatObserver),&SeatAddonOriginalBoat)!=MH_OK ||
        MH_CreateHook(targets[7],reinterpret_cast<void*>(&spawn_vehicle_hook),reinterpret_cast<void**>(&original_spawn_vehicle))!=MH_OK ||
        MH_CreateHook(targets[8],reinterpret_cast<void*>(&find_seat_hook),reinterpret_cast<void**>(&original_find_seat))!=MH_OK ||
        MH_CreateHook(targets[9],reinterpret_cast<void*>(&network_selection_hook),reinterpret_cast<void**>(&original_network))!=MH_OK) {
        remove_hooks(); return 5;
    }
    for (auto target:targets) {
        if (MH_QueueEnableHook(target)!=MH_OK) { remove_hooks(); return 6; }
    }
    enabled.store(true);
    if (MH_ApplyQueued()!=MH_OK) { enabled.store(false); remove_hooks(); return 6; }
    installed=true;
    return 0;
}
extern "C" __declspec(dllexport) unsigned long long SeatAddonCounter(unsigned slot) {
    if (slot==0) return enabled.load()?1:0;
    if (slot==1) return game_thread.load();
    return slot<counters.size()?counters[slot].load():0;
}
extern "C" __declspec(dllexport) DWORD WINAPI SeatAddonUninstall(void*) {
    std::lock_guard<std::mutex> lock(lifecycle);
    if (!installed) return 0;
    enabled.store(false);
    publish_deployment_proof(false,0,0);
    for (auto target:targets) if (MH_QueueDisableHook(target)!=MH_OK) return 2;
    if (MH_ApplyQueued()!=MH_OK) return 2;
    const auto start=GetTickCount64();
    while (!addon_quiescent()) { if (GetTickCount64()-start>2000) return 1; Sleep(1); }
    remove_hooks(); policy.reset(); reader.reset(); installed=false;
    bots={}; spawning={}; game_thread.store(0); image.reset();
    return 0;
}
BOOL WINAPI DllMain(HINSTANCE module,DWORD reason,LPVOID) { if(reason==DLL_PROCESS_ATTACH) self_module=module; return TRUE; }
