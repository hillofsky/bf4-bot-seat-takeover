#pragma once
#include <atomic>
namespace bf4_deploy_client {
inline bool accept_ui_callback(std::atomic<unsigned>& observed,unsigned current) noexcept {
    unsigned expected=0;
    observed.compare_exchange_strong(expected,current);
    // The hook entry authenticates the engine callback. UI work migrates across
    // workers; the first observed ID is diagnostic information, not authority.
    return true;
}
}
