#pragma once
#include "core/auto.h"
#include "core/numeric.h"
#include "core/thread.h"

#include "sokoban/common.h"

struct FrozenCell {
    bool alive;
    uint min_push_distance;
    vector<uint> push_distance;
};

struct FrozenLevel {
    vector<FrozenCell*> cells;  // (1:1 index mapping with regular level)
   private:
    bool ready = false;
    friend class FrozenLevels;
};

class FrozenLevels {
   public:
    // thread safe (will block if level is currently being built)
    const FrozenLevel* Get(ulong frozen_boxes) /*const*/ {
        unique_lock g(m_mutex);
        while (true) {
            auto it = m_cache.find(frozen_boxes);
            if (it == m_cache.end()) return nullptr;
            if (it->second->ready) return it->second.get();
            m_cond.wait(g);
        }
    }

    // thread safe (if frozen level doesn't exist, builder will be called and new level inserted)
    const FrozenLevel* Get(ulong frozen_boxes, function<void(FrozenLevel& frozen)> builder) {
        unique_lock g(m_mutex);
        while (true) {
            auto it = m_cache.find(frozen_boxes);
            if (it == m_cache.end()) {
                auto frozen = std::make_unique<FrozenLevel>();
                auto ptr = frozen.get();
                m_cache.emplace(frozen_boxes, std::move(frozen));
                {
                    ON_SCOPE_EXIT(m_mutex.lock());
                    m_mutex.unlock();
                    builder(*ptr);
                }
                ptr->ready = true;
                m_cond.notify_all();
                return ptr;
            }
            if (it->second->ready) return it->second.get();
            m_cond.wait(g);
        }
    }

    /*private:
            ulong Key(const Boxes& boxes) const {
                    ulong key = boxes.words[0];
                    if (boxes.size() > 32)
                            key |= ulong(boxes.words[1]) << 32;
                    ulong mask = (1lu << m_num_goals) - 1;
                    return key & mask;
            }*/

   private:
    flat_hash_map<ulong, unique_ptr<FrozenLevel>> m_cache;
    mutable mutex m_mutex;
    mutable condition_variable m_cond;
};
