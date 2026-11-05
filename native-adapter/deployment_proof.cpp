#include "deployment_proof.hpp"
namespace bf4_takeover {
namespace { HANDLE mapping=nullptr; DeploymentProof* proof=nullptr; volatile LONG writing=0; }
bool open_deployment_proof() {
    mapping=CreateFileMappingW(INVALID_HANDLE_VALUE,nullptr,PAGE_READWRITE,0,sizeof(DeploymentProof),L"Local\\CodexBF4DeployV1");
    if(!mapping)return false;
    proof=static_cast<DeploymentProof*>(MapViewOfFile(mapping,FILE_MAP_WRITE,0,0,sizeof(DeploymentProof)));
    if(!proof){CloseHandle(mapping);mapping=nullptr;return false;}
    if(proof->magic && (proof->magic!=0x37444642 || proof->version!=1)) {
        UnmapViewOfFile(proof);CloseHandle(mapping);proof=nullptr;mapping=nullptr;return false;
    }
    if(proof->server_pid && proof->server_pid!=GetCurrentProcessId()) {
        const HANDLE other=OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION,FALSE,proof->server_pid);
        DWORD code=0;const bool alive=other && GetExitCodeProcess(other,&code) && code==STILL_ACTIVE;
        if(other)CloseHandle(other);
        if(alive){UnmapViewOfFile(proof);CloseHandle(mapping);proof=nullptr;mapping=nullptr;return false;}
    }
    // No old writer remains. Normalize an interrupted previous write.
    InterlockedExchange(&proof->sequence,0);
    publish_deployment_proof(false,0,0);return true;
}
void publish_deployment_proof(bool enabled,unsigned bot_id,int team) noexcept {
    if(!proof)return;
    while(InterlockedCompareExchange(&writing,1,0)) YieldProcessor();
    InterlockedIncrement(&proof->sequence);MemoryBarrier();
    proof->magic=0x37444642;proof->version=1;proof->server_pid=GetCurrentProcessId();
    proof->enabled=enabled?1:0;proof->bot_id=bot_id;proof->team=team;proof->reserved=0;
    proof->updated_tick=GetTickCount64();MemoryBarrier();InterlockedIncrement(&proof->sequence);InterlockedExchange(&writing,0);
}
void close_deployment_proof() noexcept {
    publish_deployment_proof(false,0,0);
    if(proof)UnmapViewOfFile(proof);if(mapping)CloseHandle(mapping);
    proof=nullptr;mapping=nullptr;
}
}
