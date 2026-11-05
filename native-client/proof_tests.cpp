#define WIN32_LEAN_AND_MEAN
#include "deploy_shared.hpp"
#include <cstdio>
using namespace bf4_deploy_client;
int main() {
    DeployShared p{};
    p.sequence=2;p.magic=mapping_magic;p.version=1;p.server_pid=123;p.enabled=1;p.bot_id=12;p.team=2;p.updated_tick=9000;
    if(!fresh_proof(p,10000) || !fresh_proof(p,11000) || fresh_proof(p,11001) || fresh_proof(p,8999)) return 1;
    p.enabled=0;if(fresh_proof(p,10000)) return 2;p.enabled=1;
    p.version=2;if(fresh_proof(p,10000)) return 3;p.version=1;
    p.bot_id=256;if(fresh_proof(p,10000)) return 4;p.bot_id=12;
    p.team=0;if(fresh_proof(p,10000)) return 5;p.team=2;
    p.reserved=1;if(fresh_proof(p,10000)) return 6;p.reserved=0;
    p.magic=0;if(fresh_proof(p,10000)) return 7;
    std::puts("GREEN: stale/future/disabled/incompatible/invalid-identity proofs rejected; 40-byte layout checked");
    return 0;
}
