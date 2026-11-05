#include "deployment_proof.hpp"
#include <atomic>
#include <thread>
#include <cstring>
#include <cstdio>
#include <cstdlib>
using namespace bf4_takeover;
void check(bool value){if(!value){std::puts("RED deployment proof");std::abort();}}
int main(){
    check(open_deployment_proof());
    HANDLE client=OpenFileMappingW(FILE_MAP_READ,FALSE,L"Local\\CodexBF4DeployV1");check(client!=nullptr);
    const auto view=static_cast<const DeploymentProof*>(MapViewOfFile(client,FILE_MAP_READ,0,0,sizeof(DeploymentProof)));check(view!=nullptr);
    check(view->magic==0x37444642 && view->version==1 && !view->enabled);
    std::atomic<unsigned> done{0};
    auto writer=[&](unsigned id,int team){for(unsigned i=0;i<20000;++i)publish_deployment_proof(true,id,team);++done;};
    std::thread first(writer,17,1),second(writer,18,2);unsigned samples=0;
    while(done.load()!=2){
        const LONG before=view->sequence;if(before&1)continue;
        MemoryBarrier();DeploymentProof copy{};std::memcpy(&copy,view,sizeof(copy));MemoryBarrier();
        if(before!=view->sequence || (copy.sequence&1))continue;
        check(copy.magic==0x37444642 && copy.version==1 && copy.server_pid==GetCurrentProcessId());
        if(copy.enabled)check((copy.bot_id==17 && copy.team==1)||(copy.bot_id==18 && copy.team==2));
        ++samples;
    }
    first.join();second.join();close_deployment_proof();check(!view->enabled);
    check(open_deployment_proof());check(!view->enabled);close_deployment_proof();
    UnmapViewOfFile(view);CloseHandle(client);check(samples>0);
    std::printf("GREEN: read-only shared view, 40000 concurrent publications, %u coherent reads, disable/reopen\n",samples);
}
