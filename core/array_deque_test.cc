#include <vector>

#include "fmt/core.h"
#include "core/numeric.h"
#include "core/array_deque.h"
#define CATCH_CONFIG_MAIN
#include "catch.hpp"

TEST_CASE("array_deque basic", "[array_deque]") {
    array_deque<int> deque;
    REQUIRE(deque.capacity() == 0);

    FOR(i, 100) {
        REQUIRE(deque.size() == i);
        deque.push_back(i);
    }
    REQUIRE(deque.capacity() == 128);

    FOR(i, 40) REQUIRE(i == deque.pop_front());

    REQUIRE(deque.size() == 60);
    REQUIRE(deque.capacity() == 128);

    FOR(i, 70) deque.push_back(100 + i);
    REQUIRE(deque.size() == 130);

    FOR(i, 130) REQUIRE(deque[i] == 40 + i);
}

using T = std::string;
T ops;

void Log(int a, std::string_view m) {
    // fmt::print("{} {}\n", a, m);
    ops += fmt::format("{} {}\n", a, m);
}

struct Payload {
    int a;
    Payload() : a(0) { Log(0, "P()"); }
    Payload(int a) : a(a) { Log(a, "P(int)"); }
    Payload(Payload&& o) : a(o.a) { Log(a, "P(P&&)"); o.a = 0; }
    Payload(const Payload& o) : a(o.a) { Log(a, "P(const P&)"); }
    ~Payload() { Log(a, "~P()"); }
    void operator=(Payload&& o) { Log(a, "=(P&&)"); o.a = 0; }
    void operator=(const Payload& o) { Log(a, "=(const P&)"); }
    bool operator==(int o) const { return a == o; }
    bool operator==(const Payload& o) const { return a == o.a; }
};

TEST_CASE("array_deque::array_deque()", "[array_deque]") {
    ops.clear();
    array_deque<Payload> q;
    REQUIRE(ops == "");
}

TEST_CASE("array_deque::array_deque(uint)", "[array_deque]") {
    ops.clear();
    array_deque<Payload> q(3);
    REQUIRE(ops == "0 P()\n0 P()\n0 P()\n");
    REQUIRE(q.size() == 3);
    REQUIRE(q.capacity() == 3);
    REQUIRE(q.front() == 0);
    REQUIRE(q.back() == 0);
}

TEST_CASE("array_deque::array_deque(uint, const T&)", "[array_deque]") {
    const Payload a = 7;
    ops.clear();
    array_deque<Payload> q(3, a);
    REQUIRE(ops == "7 P(const P&)\n7 P(const P&)\n7 P(const P&)\n");
    REQUIRE(q.size() == 3);
    REQUIRE(q.capacity() == 3);
    REQUIRE(q.front() == 7);
    REQUIRE(q.back() == 7);
}

TEST_CASE("array_deque::~array_deque()", "[array_deque]") {
    auto* e = new array_deque<Payload>;
    auto& q = *e;
    q.push_back(4);
    q.push_back(5);
    q.push_back(6);
    q.pop_front();
    q.pop_back();
    REQUIRE(q.size() == 1);
    REQUIRE(q.capacity() == 4);
    REQUIRE(q.front() == 5);
    REQUIRE(q.back() == 5);

    ops.clear();
    delete e;
    REQUIRE(ops == "5 ~P()\n");
}

TEST_CASE("array_deque::shrink_to_fit(uint)", "[array_deque]") {
    array_deque<Payload> q;
    q.push_back(4);
    q.push_back(5);
    q.push_back(6);
    q.pop_front();
    q.pop_back();

    ops.clear();
    q.shrink_to_fit();
    REQUIRE(ops == "5 P(P&&)\n0 ~P()\n");
    REQUIRE(q.size() == 1);
    REQUIRE(q.capacity() == 1);
}

TEST_CASE("array_deque::operator==(const array_deque&)", "[array_deque]") {
    array_deque<Payload> q;
    q.reserve(3);
    q.push_back(4);
    q.push_back(5);
    q.pop_front();

    array_deque<Payload> e;
    e.push_back(5);
    bool r = e == q;
    REQUIRE(r);
    r = q == e;
    REQUIRE(r);

    e.push_back(5);
    r = e == q;
    REQUIRE(!r);
    r = q == e;
    REQUIRE(!r);
}

TEST_CASE("array_deque::begin() end()", "[array_deque]") {
    array_deque<Payload> q;
    q.push_back(4);
    q.push_back(5);
    q.pop_front();
    q.push_back(6);
    q.pop_back();
    q.push_back(7);

    auto it = q.begin();
    REQUIRE(it != q.end());
    REQUIRE(*it == 5);
    it++;
    REQUIRE(it != q.end());
    REQUIRE(*it == 7);
    it++;
    REQUIRE(it == q.end());
}

TEST_CASE("array_deque::begin() end() const", "[array_deque]") {
    array_deque<Payload> q;
    q.push_back(4);
    q.push_back(5);
    q.pop_front();
    q.push_back(6);
    q.pop_back();
    q.push_back(7);

    const auto& cq = q;
    auto it = cq.begin();
    REQUIRE(it != cq.end());
    REQUIRE(*it == 5);
    it++;
    REQUIRE(it != cq.end());
    REQUIRE(*it == 7);
    it++;
    REQUIRE(it == cq.end());
}

TEST_CASE("array_deque::reserve(uint)", "[array_deque]") {
    array_deque<Payload> q;
    q.push_back(4);
    ops.clear();
    q.reserve(1);
    REQUIRE(ops == "");
    REQUIRE(q.size() == 1);
    REQUIRE(q.capacity() == 4);

    ops.clear();
    q.reserve(10);
    REQUIRE(ops == "");
    REQUIRE(q.size() == 1);
    REQUIRE(q.capacity() == 10);
}

TEST_CASE("array_deque::push_back(T&&)", "[array_deque]") {
    array_deque<Payload> q;
    Payload a = 3;
    ops.clear();
    q.push_back(std::move(a));
    REQUIRE(ops == "3 P(P&&)\n");

    REQUIRE(q.size() == 1);
    REQUIRE(q.capacity() == 4);
    REQUIRE(q[0] == 3);
    REQUIRE(q.front() == 3);
    REQUIRE(q.back() == 3);
}

TEST_CASE("array_deque::push_front(T&&)", "[array_deque]") {
    array_deque<Payload> q;
    Payload a = 3;
    ops.clear();
    q.push_front(std::move(a));
    REQUIRE(ops == "3 P(P&&)\n");
    REQUIRE(q.size() == 1);
    REQUIRE(q.capacity() == 4);
    REQUIRE(q[0] == 3);
    REQUIRE(q.front() == 3);
    REQUIRE(q.back() == 3);
}

TEST_CASE("array_deque::push_back(const T&)", "[array_deque]") {
    array_deque<Payload> q;
    const Payload a = 3;
    ops.clear();
    q.push_back(a);
    REQUIRE(ops == "3 P(const P&)\n");
    REQUIRE(q.size() == 1);
    REQUIRE(q.capacity() == 4);
    REQUIRE(q[0] == 3);
    REQUIRE(q.front() == 3);
    REQUIRE(q.back() == 3);
}

TEST_CASE("array_deque::push_front(const T&)", "[array_deque]") {
    array_deque<Payload> q;
    const Payload a = 3;
    ops.clear();
    q.push_front(a);
    REQUIRE(ops == "3 P(const P&)\n");
    REQUIRE(q.size() == 1);
    REQUIRE(q.capacity() == 4);
    REQUIRE(q[0] == 3);
    REQUIRE(q.front() == 3);
    REQUIRE(q.back() == 3);
}
