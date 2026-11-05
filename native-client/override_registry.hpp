#pragma once
#include <array>
#include <cstdint>
#include <cstddef>
#include <limits>

namespace bf4_deploy_client {
struct PointKey {
    std::uintptr_t point=0,key1=0,key2=0;
    friend bool operator==(const PointKey& a,const PointKey& b) noexcept {
        return a.point==b.point && a.key1==b.key1 && a.key2==b.key2;
    }
};
struct OverrideLease {std::size_t slot=0;std::uint64_t generation=0;bool valid=false;};
enum class AcquireResult {acquired,invalid_key,state_changed,unowned_ready,full,write_failed};
enum class ReleaseResult {released,object_changed,invalid_lease,restore_failed};

// Caller holds one mutex for owns/acquire/release and every external read/write.
// Read(key,value) succeeds only while the native object still has these keys.
// No lock is retained by a lease across the original engine render.
template<std::size_t Capacity=32> class OverrideRegistry {
    struct Entry {PointKey key;std::uint64_t generation=0;std::uint32_t references=0;};
    std::array<Entry,Capacity> entries_{};
public:
    bool owns(const PointKey& key) const noexcept {
        for(const auto& e:entries_) if(e.references && e.key==key) return true;
        return false;
    }
    template<class Read,class Write>
    AcquireResult acquire(const PointKey& key,Read&& read,Write&& write,OverrideLease& lease) {
        lease={};
        if(!key.point) return AcquireResult::invalid_key;
        std::size_t selected=Capacity,empty=Capacity;
        for(std::size_t i=0;i<Capacity;++i) {
            const auto& e=entries_[i];
            if(e.references && e.key==key) {selected=i;break;}
            if(!e.references && empty==Capacity) empty=i;
        }
        const bool existing=selected!=Capacity;
        if(!existing) selected=empty;
        if(selected==Capacity || entries_[selected].references==std::numeric_limits<std::uint32_t>::max()) return AcquireResult::full;
        unsigned char value=0;
        if(!read(key,value) || value>1) return AcquireResult::state_changed;
        if(value==1 && !existing) return AcquireResult::unowned_ready;
        if(value==0 && !write(key,1)) return AcquireResult::write_failed;
        auto& e=entries_[selected];
        if(!existing) {
            e.key=key;
            if(++e.generation==0) ++e.generation;
        }
        ++e.references;
        lease={selected,e.generation,true};
        return AcquireResult::acquired;
    }
    template<class Read,class Write>
    ReleaseResult release(OverrideLease& lease,Read&& read,Write&& write) {
        if(!lease.valid || lease.slot>=Capacity) return ReleaseResult::invalid_lease;
        const auto slot=lease.slot;const auto generation=lease.generation;lease.valid=false;
        auto& e=entries_[slot];
        if(!e.references || e.generation!=generation) return ReleaseResult::invalid_lease;
        if(--e.references) return ReleaseResult::released;
        unsigned char value=0;
        if(!read(e.key,value)) return ReleaseResult::object_changed;
        if(value==1 && !write(e.key,0)) return ReleaseResult::restore_failed;
        return ReleaseResult::released;
    }
    std::uint32_t references(const PointKey& key) const noexcept {
        for(const auto& e:entries_) if(e.references && e.key==key) return e.references;
        return 0;
    }
};
}
