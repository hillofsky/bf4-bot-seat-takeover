#pragma once
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <cstdint>
#include <cstddef>
namespace bf4_takeover {
struct alignas(8) DeploymentProof {
    volatile LONG sequence;
    std::uint32_t magic,version,server_pid,enabled,bot_id;
    std::int32_t team;
    std::uint32_t reserved;
    ULONGLONG updated_tick;
};
static_assert(sizeof(DeploymentProof)==40 && offsetof(DeploymentProof,updated_tick)==32,"shared layout");
bool open_deployment_proof();
void publish_deployment_proof(bool enabled,unsigned bot_id,int team) noexcept;
void close_deployment_proof() noexcept;
}
