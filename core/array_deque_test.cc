#include "core/array_deque.h"

#define CATCH_CONFIG_MAIN
#include "catch.hpp"

TEST_CASE("array_deque basic", "[array_deque]") {
    array_deque<int> deque;
    REQUIRE(deque.capacity() == 0);

    for (int i = 0; i < 100; i++) {
        REQUIRE(deque.size() == i);
        deque.push_back(i);
    }
    REQUIRE(deque.capacity() == 128);

    for (int i = 0; i < 40; i++) {
        REQUIRE(i == deque.front());
        deque.pop_front();
    }

    REQUIRE(deque.size() == 60);
    REQUIRE(deque.capacity() == 128);

    for (int i = 0; i < 70; i++) deque.push_back(100 + i);
    REQUIRE(deque.size() == 130);

    for (int i = 0; i < 130; i++) REQUIRE(deque[i] == 40 + i);
}

using T = std::string;
T ops;

void Log(int a, std::string_view m) {
    std::ostringstream os;
    os << a << ' ' << m << '\n';
    ops += os.str();
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
    REQUIRE(q.size() == 0);
    REQUIRE(q.capacity() == 0);
}

TEST_CASE("array_deque::array_deque(uint) size:0", "[array_deque]") {
    ops.clear();
    array_deque<Payload> q(0);
    REQUIRE(ops == "");
    REQUIRE(q.size() == 0);
    REQUIRE(q.capacity() == 0);
}

TEST_CASE("array_deque::array_deque(uint) size:3", "[array_deque]") {
    ops.clear();
    array_deque<Payload> q(3);
    REQUIRE(ops == "0 P()\n0 P()\n0 P()\n");
    REQUIRE(q.size() == 3);
    REQUIRE(q.capacity() == 3);
    REQUIRE(q.front() == 0);
    REQUIRE(q.back() == 0);
}

TEST_CASE("array_deque::array_deque(uint, const T&) size:0", "[array_deque]") {
    const Payload a = 7;
    ops.clear();
    array_deque<Payload> q(0, a);
    REQUIRE(ops == "");
    REQUIRE(q.size() == 0);
    REQUIRE(q.capacity() == 0);
}

TEST_CASE("array_deque::array_deque(uint, const T&) size:3", "[array_deque]") {
    const Payload a = 7;
    ops.clear();
    array_deque<Payload> q(3, a);
    REQUIRE(ops == "7 P(const P&)\n7 P(const P&)\n7 P(const P&)\n");
    REQUIRE(q.size() == 3);
    REQUIRE(q.capacity() == 3);
    REQUIRE(q.front() == 7);
    REQUIRE(q.back() == 7);
}

TEST_CASE("array_deque::array_deque(InputIt, InputIt)", "[array_deque]") {
    const Payload a[] = {7, 8, 9};
    ops.clear();
    array_deque<Payload> q(a, a + 3);
    REQUIRE(ops == "7 P(const P&)\n8 P(const P&)\n9 P(const P&)\n");
    REQUIRE(q.size() == 3);
    REQUIRE(q.capacity() == 3);
    REQUIRE(q.front() == 7);
    REQUIRE(q.back() == 9);
}

// DISABLED
TEST_CASE("array_deque::array_deque(const array_deque&)", "[.][array_deque]") {
    const array_deque<const Payload> a{7, 8, 9};
    ops.clear();
    array_deque<const Payload> q(a);
    REQUIRE(ops == "7 P(const P&)\n8 P(const P&)\n9 P(const P&)\n");
    REQUIRE(q.size() == 3);
    REQUIRE(q.capacity() == 3);
    REQUIRE(q.front() == 7);
    REQUIRE(q.back() == 9);
}

TEST_CASE("array_deque::array_deque(array_deque&&)", "[array_deque]") {
    array_deque<Payload> a{7, 8, 9};
    ops.clear();
    array_deque<Payload> q(std::move(a));
    REQUIRE(ops == "");
    REQUIRE(q.size() == 3);
    REQUIRE(q.capacity() == 3);
    REQUIRE(q.front() == 7);
    REQUIRE(q.back() == 9);
    REQUIRE(a.size() == 0);
    REQUIRE(a.capacity() == 0);
}

TEST_CASE("array_deque::array_deque(std::initializer_list<>)", "[array_deque]") {
    std::initializer_list<Payload> a = {7, 8, 9};
    ops.clear();
    array_deque<Payload> q(a);
    REQUIRE(ops == "7 P(const P&)\n8 P(const P&)\n9 P(const P&)\n");
    REQUIRE(q.size() == 3);
    REQUIRE(q.capacity() == 3);
    REQUIRE(q.front() == 7);
    REQUIRE(q.back() == 9);
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

TEST_CASE("array_deque::~array_deque() size:0", "[array_deque]") {
    auto* q = new array_deque<Payload>;
    ops.clear();
    delete q;
    REQUIRE(ops == "");
}

TEST_CASE("array_deque::~array_deque() size:1", "[array_deque]") {
    auto* q = new array_deque<Payload>{4};
    ops.clear();
    delete q;
    REQUIRE(ops == "4 ~P()\n");
}

TEST_CASE("array_deque::~array_deque() size:2", "[array_deque]") {
    auto* q = new array_deque<Payload>{4, 5};
    ops.clear();
    delete q;
    REQUIRE(ops == "5 ~P()\n4 ~P()\n");
}

TEST_CASE("array_deque::~array_deque() size:2 start:1", "[array_deque]") {
    auto* q = new array_deque<Payload>{4, 5, 6};
    q->pop_front();
    ops.clear();
    delete q;
    REQUIRE(ops == "6 ~P()\n5 ~P()\n");
}

TEST_CASE("array_deque& array_deque::operator=(const array_deque& o) self", "[array_deque]") {
    array_deque<Payload> a{5, 6, 7};
    auto p = &a[0];
    auto& b = a;
    ops.clear();
    b = a;
    REQUIRE(ops == "");
    REQUIRE(a.size() == 3);
    REQUIRE(a.capacity() == 3);
    REQUIRE(&a[0] == p);
}

// DISABLED
TEST_CASE("array_deque& array_deque::operator=(const array_deque& o)", "[.][array_deque]") {
    array_deque<Payload> a{5, 6, 7, 8};
    a.pop_front();
    a.push_back(9);
    REQUIRE(a.capacity() == 4);
    array_deque<Payload> b;
    ops.clear();
    b = a;
    REQUIRE(ops == "6 P(const P&)\n7 P(const P&)\n8 P(const P&)\n9 P(const P&)\n");

    REQUIRE(a.size() == 4);
    REQUIRE(a.front() == 6);
    REQUIRE(a.back() == 9);

    REQUIRE(b.size() == 4);
    REQUIRE(b.capacity() == 4);
    REQUIRE(b.front() == 6);
    REQUIRE(b.back() == 9);
}

// TODO array_deque& operator=(array_deque&& o)
// TODO array_deque& operator=(std::initializer_list<T> ilist)

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
    REQUIRE((e == q));
    REQUIRE((q == e));

    e.push_back(5);
    REQUIRE(!(e == q));
    REQUIRE(!(q == e));
}

TEST_CASE("array_deque::begin() end()", "[array_deque]") {
    array_deque<Payload> q;
    q.push_back(4);
    q.push_back(5);
    q.pop_front();
    q.push_back(6);
    q.pop_back();
    q.push_back(7);
    REQUIRE(q.size() == 2);
    REQUIRE(q.capacity() == 4);

    auto it = q.begin();
    REQUIRE(it != q.end());
    REQUIRE(*it == 5);
    it++;
    REQUIRE(it != q.end());
    REQUIRE(*it == 7);
    it++;
    REQUIRE(it == q.end());
}

/*TEST_CASE("array_deque::rbegin() rend()", "[array_deque]") {
    array_deque<Payload> q;
    q.push_back(4);
    q.push_back(5);
    q.pop_front();
    q.push_back(6);
    q.pop_back();
    q.push_back(7);
    REQUIRE(q.size() == 2);
    REQUIRE(q.capacity() == 4);

    auto it = q.rbegin();
    REQUIRE(it != q.rend());
    REQUIRE(*it == 7);
    it++;
    REQUIRE(it != q.rend());
    REQUIRE(*it == 5);
    it++;
    REQUIRE(it == q.rend());
}*/

TEST_CASE("array_deque::cbegin() cend()", "[array_deque]") {
    array_deque<Payload> q;
    q.push_back(4);
    q.push_back(5);
    q.pop_front();
    q.push_back(6);
    q.pop_back();
    q.push_back(7);
    REQUIRE(q.size() == 2);
    REQUIRE(q.capacity() == 4);

    const auto& cq = q;
    auto it = cq.cbegin();
    REQUIRE(it != cq.cend());
    REQUIRE(*it == 5);
    it++;
    REQUIRE(it != cq.cend());
    REQUIRE(*it == 7);
    it++;
    REQUIRE(it == cq.cend());
}

TEST_CASE("array_deque::reserve(uint) no-op", "[array_deque]") {
    array_deque<Payload> q{4, 5};
    q.pop_front();
    REQUIRE(q.capacity() == 2);

    ops.clear();
    q.reserve(0);
    REQUIRE(ops == "");
    REQUIRE(q.size() == 1);
    REQUIRE(q.front() == 5);
    REQUIRE(q.capacity() == 2);
}

// TODO wrap around
TEST_CASE("array_deque::reserve(uint) grow", "[array_deque]") {
    array_deque<Payload> q{4, 5};
    q.pop_front();
    REQUIRE(q.capacity() == 2);

    ops.clear();
    q.reserve(10);
    REQUIRE(ops == "5 P(P&&)\n0 ~P()\n");
    REQUIRE(q.size() == 1);
    REQUIRE(q.front() == 5);
    REQUIRE(q.capacity() == 10);
}

TEST_CASE("array_deque::shrink_to_fit() no-op", "[array_deque]") {
    array_deque<Payload> q{4, 5};
    REQUIRE(q.capacity() == 2);

    ops.clear();
    q.shrink_to_fit();
    REQUIRE(ops == "");
    REQUIRE(q.size() == 2);
    REQUIRE(q.front() == 4);
    REQUIRE(q.back() == 5);
    REQUIRE(q.capacity() == 2);
}

// TODO wrap around
TEST_CASE("array_deque::shrink_to_fit() shink", "[array_deque]") {
    array_deque<Payload> q{4, 5};
    q.pop_front();
    REQUIRE(q.capacity() == 2);

    ops.clear();
    q.shrink_to_fit();
    REQUIRE(ops == "5 P(P&&)\n0 ~P()\n");
    REQUIRE(q.size() == 1);
    REQUIRE(q.front() == 5);
    REQUIRE(q.capacity() == 1);
}

TEST_CASE("std::hash<array_deque>", "[array_deque]") {
    std::hash<array_deque<int>> hasher;
    REQUIRE(hasher({3, 3}) != hasher({}));
    REQUIRE(hasher({3, 3}) != hasher({3}));
    REQUIRE(hasher({3, 3}) != hasher({2, 2}));
    REQUIRE(hasher({3, 7}) != hasher({7, 3}));
}

/*TEST_CASE("std::erase(array_deque&, value)", "[array_deque]") {
    array_deque<Payload> a{1, 2, 3, 4};
    a.pop_front();
    a.pop_front();
    a.push_back(5);
    a.push_back(6);

    ops.clear();
    std::erase(a, 5);
    REQUIRE(ops == "");

    REQUIRE(a == array_deque<Payload>{3, 4, 6});
    REQUIRE(a.capacity() == 4);
}*/

/*TEST_CASE("std::erase_if(array_deque&, predicate)", "[array_deque]") {
    array_deque<Payload> a{1, 2, 3, 4};
    a.pop_front();
    a.pop_front();
    a.push_back(5);
    a.push_back(6);

    ops.clear();
    std::erase_if(a, [](auto e) {return e.a % 2 == 1; });
    REQUIRE(ops == "");

    REQUIRE(a == array_deque<Payload>{4, 6});
    REQUIRE(a.capacity() == 4);
}*/

TEST_CASE("array_deque::swap(array_deque&)", "[array_deque]") {
    array_deque<Payload> a{1, 2};
    array_deque<Payload> b{4};

    ops.clear();
    a.swap(b);
    REQUIRE(ops == "");

    REQUIRE(a == array_deque<Payload>{4});
    REQUIRE(b == array_deque<Payload>{1, 2});
}

TEST_CASE("std::swap(array_deque&, array_deque&)", "[array_deque]") {
    array_deque<Payload> a{1, 2};
    array_deque<Payload> b{4};

    ops.clear();
    std::swap(a, b);
    REQUIRE(ops == "");

    REQUIRE(a == array_deque<Payload>{4});
    REQUIRE(b == array_deque<Payload>{1, 2});
}

TEST_CASE("array_deque doubling capacity", "[array_deque]") {
    for (int i = 2; i <= 10; i++) {
        array_deque<int> q(1 << i);
        auto c = q.capacity();
        q.push_front(3);
        REQUIRE(q.capacity() == c + c);
    }
}

// front_inserter
// back_inserter
// < > <= >=
// resize(size_t)
// resize(size_t, T)
// emplace_back
// emplace_front
// erase(it)
// erase(it, it)
// emplace(it, args)
// insert(it, it)
// insert(it, init_list)
// insert(it, const T&)
// insert(it, T&&)
// clear()
// assign(size_type count, const T& value) {
// assign(InputIt first, InputIt last) {
// assign(std::initializer_list<T> ilist) { assign(ilist.begin(), ilist.end()); }
