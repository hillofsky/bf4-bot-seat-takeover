#pragma once
#include <windows.h>
#include <cstdint>
#include <cstddef>

namespace bf4_deploy_client {
constexpr wchar_t mapping_name[]=L"Local\\CodexBF4DeployV1";
constexpr std::uint32_t mapping_magic=0x37444642;
struct alignas(8) DeployShared {
    volatile LONG sequence;
    std::uint32_t magic,version,server_pid,enabled,bot_id;
    std::int32_t team;
    std::uint32_t reserved;
    ULONGLONG updated_tick;
};
static_assert(sizeof(DeployShared)==40,"shared protocol size");
static_assert(offsetof(DeployShared,updated_tick)==32,"shared protocol tick");
static_assert(offsetof(DeployShared,bot_id)==20,"shared protocol identity");
inline bool fresh_proof(const DeployShared& p,ULONGLONG now) noexcept {
    return p.magic==mapping_magic && p.version==1 && p.enabled==1 &&
        p.server_pid!=0 && p.bot_id<256 && (p.team==1 || p.team==2) &&
        p.reserved==0 && p.updated_tick<=now && now-p.updated_tick<=2000;
}
}
