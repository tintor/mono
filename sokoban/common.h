#pragma once
#include "core/thread.h"
#include "core/vector.h"
#include "core/symbol.h"
#include <map>

using namespace std::chrono_literals;
using std::nullopt;
using std::vector;
using std::array;
using std::lock_guard;
using std::mutex;
using std::thread;
using std::pair;
using std::optional;
using std::map;
using std::variant;

using std::string_view;
using std::string;

#include "core/array_deque.h"

using mt::array_deque;

#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"

using absl::flat_hash_map;
using absl::flat_hash_set;
