#include "ui_thread_policy.hpp"
#include "override_registry.hpp"
#include <atomic>
#include <thread>
#include <vector>
#include <cstdio>
#include <mutex>
#include <array>
using namespace bf4_deploy_client;
#define CHECK(x) do {if(!(x)) {std::printf("FAIL line %d: %s\n",__LINE__,#x);return 1;}} while(false)
struct Fixture {
    PointKey key{1,100,200};
    unsigned char value=0;
    unsigned writes=0;
    bool fail_write=false;
    bool read(const PointKey& candidate,unsigned char& result) {
        if(!(candidate==key)) return false;
        result=value;return true;
    }
    bool write(const PointKey& candidate,unsigned char next) {
        if(!(candidate==key) || fail_write) return false;
        value=next;++writes;return true;
    }
};
int main() {
    std::atomic<unsigned> observed{0},arrived{0},accepted{0};
    std::vector<std::thread> workers;
    for(unsigned i=1;i<=5;++i) workers.emplace_back([&,i] {
        ++arrived;
        while(arrived.load()!=5) std::this_thread::yield();
        if(accept_ui_callback(observed,i)) ++accepted;
    });
    for(auto& worker:workers) worker.join();
    std::printf("UI callback policy: %u/5 accepted\n",accepted.load());
    CHECK(accepted.load()==5);

    OverrideRegistry<> registry;
    Fixture f;
    std::mutex state;
    std::array<std::atomic<bool>,5> release{};
    std::atomic<unsigned> acquired{0},returned{0},errors{0};
    workers.clear();
    auto read=[&](const PointKey& key,unsigned char& value){return f.read(key,value);};
    auto write=[&](const PointKey& key,unsigned char value){return f.write(key,value);};
    for(unsigned i=0;i<5;++i) workers.emplace_back([&,i] {
        OverrideLease lease;
        {
            std::lock_guard<std::mutex> lock(state);
            if(registry.acquire(f.key,read,write,lease)!=AcquireResult::acquired) ++errors;
        }
        ++acquired;
        // Models the original native render outside the registry mutex.
        while(!release[i].load()) std::this_thread::yield();
        {
            std::lock_guard<std::mutex> lock(state);
            if(registry.release(lease,read,write)!=ReleaseResult::released) ++errors;
        }
        ++returned;
    });
    while(acquired.load()!=5) std::this_thread::yield();
    {
        std::lock_guard<std::mutex> lock(state);
        CHECK(!errors.load() && f.value==1 && registry.references(f.key)==5 && f.writes==1);
    }
    for(unsigned i=0;i<5;++i) {
        release[i].store(true);
        while(returned.load()!=i+1) std::this_thread::yield();
        std::lock_guard<std::mutex> lock(state);
        CHECK(f.value==(i==4?0:1));
        CHECK(registry.references(f.key)==4-i);
    }
    for(auto& worker:workers) worker.join();
    CHECK(!errors.load() && f.writes==2 && !registry.owns(f.key));

    OverrideLease lease;
    CHECK(registry.acquire(f.key,read,write,lease)==AcquireResult::acquired);
    auto stale=lease;
    CHECK(registry.release(lease,read,write)==ReleaseResult::released);
    CHECK(registry.acquire(f.key,read,write,lease)==AcquireResult::acquired);
    CHECK(registry.release(stale,read,write)==ReleaseResult::invalid_lease);
    CHECK(f.value==1 && registry.references(f.key)==1);
    const auto old_writes=f.writes;
    ++f.key.key1; // Same address now identifies a new engine object.
    CHECK(registry.release(lease,read,write)==ReleaseResult::object_changed);
    CHECK(f.value==1 && f.writes==old_writes);
    CHECK(registry.acquire(f.key,read,write,lease)==AcquireResult::unowned_ready);
    CHECK(!lease.valid && !registry.owns(f.key));
    f.value=0;f.fail_write=true;
    CHECK(registry.acquire(f.key,read,write,lease)==AcquireResult::write_failed);
    CHECK(!lease.valid && f.value==0 && !registry.owns(f.key));
    f.fail_write=false;
    CHECK(registry.acquire(f.key,read,write,lease)==AcquireResult::acquired);
    f.fail_write=true;
    CHECK(registry.release(lease,read,write)==ReleaseResult::restore_failed);
    CHECK(!lease.valid && !registry.owns(f.key));

    OverrideRegistry<1> bounded;
    f.fail_write=false;f.value=0;
    CHECK(bounded.acquire(f.key,read,write,lease)==AcquireResult::acquired);
    OverrideLease extra;
    PointKey another=f.key;++another.point;
    CHECK(bounded.acquire(another,read,write,extra)==AcquireResult::full);
    CHECK(!extra.valid && bounded.references(f.key)==1);
    CHECK(bounded.release(lease,read,write)==ReleaseResult::released);
    std::puts("GREEN: 5 actual overlapping threads hold readiness until last release; generation reuse, bounded capacity and write failures fail closed");
    return 0;
}
