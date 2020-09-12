#pragma once
#include "core/thread.h"
#include "core/timestamp.h"
#include "absl/container/flat_hash_map.h"

template <typename State>
struct StateMap {
    constexpr static int SHARDS = 64;

    static int shard(const State& s) { return fmix64(s.boxes.hash() * 7) % SHARDS; }

    void print_sizes() {
        print("states map");
        for (int i = 0; i < SHARDS; i++) {
            locks[i].lock();
            print(" %h", data[i].size());
            locks[i].unlock();
        }
        print("\n");
    }

    void lock(int shard) const {
        Timestamp lock_ts;
        locks[shard].lock();
        overhead += lock_ts.elapsed();
    }

    void lock2(int shard) const {
        Timestamp lock_ts;
        locks[shard].lock();
        overhead2 += lock_ts.elapsed();
    }

    void unlock(int shard) const { locks[shard].unlock(); }

    bool contains(const State& s, int shard) const { return data[shard].find(s) != data[shard].end(); }

    StateInfo get(const State& s, int shard) const { return data[shard].at(s); }

    const StateInfo* query(const State& s, int shard) const {
        auto& d = data[shard];
        auto it = d.find(s);
        if (it == d.end()) return nullptr;
        return &it->second;
    }

    StateInfo* query(const State& s, int shard) {
        auto& d = data[shard];
        auto it = d.find(s);
        if (it == d.end()) return nullptr;
        return &it->second;
    }

    void add(const State& s, const StateInfo& si, int shard) { data[shard].emplace(s, si); }

    long size() const {
        long result = 0;
        for (int i = 0; i < SHARDS; i++) {
            locks[i].lock();
            result += data[i].size();
            locks[i].unlock();
        }
        return result;
    }

    void reset() {
        overhead = 0;
        overhead2 = 0;
        for (auto& d : data) d.clear();
    }

    std::string monitor() const {
        return format("{:.3f} {:.3f}", Timestamp::to_s(overhead), Timestamp::to_s(overhead2));
    }

private:
    mutable array<std::mutex, SHARDS> locks;
    array<absl::flat_hash_map<State, StateInfo>, SHARDS> data;
    mutable std::atomic<long> overhead = 0;
    mutable std::atomic<long> overhead2 = 0;
};
