#pragma once
#include "core/thread.h"
#include "core/vector.h"
#include "core/each.h"
#include "core/matrix.h"
#include "core/symbol.h"
#include "core/timestamp.h"

#include <map>
#include <unordered_map>
#include <queue>
#include <atomic>

using namespace std::chrono_literals;
using std::nullopt;
using std::vector;
using std::array;
using std::lock_guard;
using std::unique_lock;
using std::mutex;
using std::thread;
using std::condition_variable;
using std::pair;
using std::optional;
using std::map;
using std::variant;
using std::function;
using std::unique_ptr;
using std::string_view;
using std::string;
using std::priority_queue;
using std::atomic;
using std::numeric_limits;

#include "core/array_deque.h"

using mt::array_deque;

#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"

using absl::flat_hash_map;
using absl::flat_hash_set;
