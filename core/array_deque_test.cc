#include "core/array_deque.h"
using namespace mt;

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

std::string ops;

void Log(int a, std::string_view m) {
    std::ostringstream os;
    os << a << ' ' << m << '\n';
    ops += os.str();
}

struct Payload {
    int a;
    std::string str;
    Payload() : a(0) { Log(0, "P()"); }
    Payload(int a) : a(a) { Log(a, "P(int)"); }
    Payload(int a, std::string s) : a(a), str(s) { Log(a, "P(int, string)"); }
    Payload(Payload&& o) : a(o.a) { Log(a, "P(P&&)"); o.a = 0; }
    Payload(const Payload& o) : a(o.a) { Log(a, "P(const P&)"); }
    ~Payload() { Log(a, "~P()"); a = -100; }
    void operator=(Payload&& o) { Log(a, "=(P&&)"); a = o.a; o.a = 0; }
    void operator=(const Payload& o) { Log(a, "=(const P&)"); a = o.a; }
    bool operator==(int o) const { return a == o; }
    bool operator==(const Payload& o) const { return a == o.a; }
};

std::ostream& operator<<(std::ostream& os, const Payload& p) {
    os << p.a;
    return os;
}

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

TEST_CASE("array_deque::array_deque(InputIt, InputIt) empty", "[array_deque]") {
    const Payload a[] = {};
    ops.clear();
    array_deque<Payload> q(a, a);
    REQUIRE(ops == "");
    REQUIRE(q.size() == 0);
    REQUIRE(q.capacity() == 0);
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

TEST_CASE("array_deque::emplace_back(...) inplace", "[array_deque]") {
    array_deque<Payload> q{1};
    q.reserve(2);
    ops.clear();
    q.emplace_back(2, "str");
    REQUIRE(ops == "2 P(int, string)\n");

    REQUIRE(q.size() == 2);
    REQUIRE(q.capacity() == 2);
    REQUIRE(q.front() == 1);
    REQUIRE(q.back() == 2);
    REQUIRE(q.back().str == "str");
}

TEST_CASE("array_deque::emplace_front(...) inplace", "[array_deque]") {
    array_deque<Payload> q{1};
    q.reserve(2);
    ops.clear();
    q.emplace_front(2, "str");
    REQUIRE(ops == "2 P(int, string)\n");

    REQUIRE(q.size() == 2);
    REQUIRE(q.capacity() == 2);
    REQUIRE(q.front() == 2);
    REQUIRE(q.back() == 1);
    REQUIRE(q.front().str == "str");
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
    REQUIRE(ops == "4 ~P()\n5 ~P()\n");
}

TEST_CASE("array_deque::~array_deque() size:2 start:1", "[array_deque]") {
    auto* q = new array_deque<Payload>{4, 5, 6};
    q->pop_front();
    ops.clear();
    delete q;
    REQUIRE(ops == "5 ~P()\n6 ~P()\n");
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

TEST_CASE("array_deque& array_deque::operator=(const array_deque& o)", "[array_deque]") {
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
    array_deque<Payload> q{4, 5};
    q.reserve(3);
    q.pop_front();

    array_deque<Payload> e{5};
    REQUIRE((e == q));
    REQUIRE((q == e));

    e.push_back(5);
    REQUIRE(!(e == q));
    REQUIRE(!(q == e));
}

TEST_CASE("array_deque::operator!=(const array_deque&)", "[array_deque]") {
    array_deque<Payload> q{4, 5};
    q.reserve(3);
    q.pop_front();

    array_deque<Payload> e{5};
    REQUIRE(!(e != q));
    REQUIRE(!(q != e));

    e.push_back(5);
    REQUIRE((e != q));
    REQUIRE((q != e));
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

TEST_CASE("array_deque::rbegin() rend()", "[array_deque]") {
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
}

TEST_CASE("array_deque::crbegin() crend()", "[array_deque]") {
    array_deque<Payload> q;
    q.push_back(4);
    q.push_back(5);
    q.pop_front();
    q.push_back(6);
    q.pop_back();
    q.push_back(7);
    REQUIRE(q.size() == 2);
    REQUIRE(q.capacity() == 4);

    auto it = q.crbegin();
    REQUIRE(it != q.crend());
    REQUIRE(*it == 7);
    it++;
    REQUIRE(it != q.crend());
    REQUIRE(*it == 5);
    it++;
    REQUIRE(it == q.crend());
}

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

    ops.clear();
    q.reserve(10);
    REQUIRE(ops == "5 P(P&&)\n0 ~P()\n");
    REQUIRE(q.size() == 1);
    REQUIRE(q.front() == 5);
    REQUIRE(q.capacity() == 10);
}

TEST_CASE("array_deque::shrink_to_fit() no-op", "[array_deque]") {
    array_deque<Payload> q{4, 5};

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

TEST_CASE("array_deque front_inserter", "[array_deque]") {
    array_deque<int> a{1, 2};
    int v[] = {0, -1};
    std::copy(v, v + 2, std::front_inserter(a));
    REQUIRE(a == array_deque<int>{-1, 0, 1, 2});
}

TEST_CASE("array_deque back_inserter", "[array_deque]") {
    array_deque<int> a{1, 2};
    int v[] = {3, 4};
    std::copy(v, v + 2, std::back_inserter(a));
    REQUIRE(a == array_deque<int>{1, 2, 3, 4});
}

TEST_CASE("array_deque < and <=", "[array_deque]") {
    REQUIRE(array_deque<int>{} < array_deque{1});
    REQUIRE(array_deque<int>{} <= array_deque{1});
    REQUIRE_FALSE(array_deque{1} < array_deque<int>{});
    REQUIRE_FALSE(array_deque{1} <= array_deque<int>{});

    REQUIRE(array_deque{1, 2} < array_deque{2});
    REQUIRE(array_deque{1, 2} <= array_deque{2});
    REQUIRE_FALSE(array_deque{2} < array_deque{1, 2});
    REQUIRE_FALSE(array_deque{2} <= array_deque{1, 2});

    REQUIRE(array_deque{1} < array_deque{1, 2});
    REQUIRE(array_deque{1} <= array_deque{1, 2});
    REQUIRE_FALSE(array_deque{1, 2} < array_deque{1});
    REQUIRE(array_deque{1, 2} <= array_deque{1});

    REQUIRE_FALSE(array_deque{1, 2} < array_deque{1, 2});
    REQUIRE(array_deque{1, 2} <= array_deque{1, 2});
}

TEST_CASE("array_deque > and >=", "[array_deque]") {
    REQUIRE(array_deque{3} > array_deque{2, 3});
    REQUIRE(array_deque{3} >= array_deque{2, 3});
}

TEST_CASE("array_deque::resize(size_t) down", "[array_deque]") {
    array_deque<Payload> q{4, 5, 6, 7};
    q.pop_front();
    q.push_back(8);

    ops.clear();
    q.resize(3);
    REQUIRE(ops == "8 ~P()\n");
    REQUIRE(q.capacity() == 4);
    REQUIRE(q == array_deque<Payload>{5, 6, 7});
}

TEST_CASE("array_deque::resize(size_t) up inplace", "[array_deque]") {
    array_deque<Payload> q{4, 5, 6};
    q.pop_front();
    q.pop_front();
    q.push_back(7);

    ops.clear();
    q.resize(3);
    REQUIRE(ops == "0 P()\n");
    REQUIRE(q.capacity() == 3);
    REQUIRE(q == array_deque<Payload>{6, 7, 0});
}

TEST_CASE("array_deque::resize(size_t) up", "[array_deque]") {
    array_deque<Payload> q{4, 5};
    q.pop_front();
    q.push_back(6);

    ops.clear();
    q.resize(4);
    REQUIRE(ops == "5 P(P&&)\n6 P(P&&)\n0 ~P()\n0 ~P()\n0 P()\n0 P()\n");
    REQUIRE(q.capacity() == 4);
    REQUIRE(q == array_deque<Payload>{5, 6, 0, 0});
}

TEST_CASE("array_deque::resize(size_t, value) up inplace", "[array_deque]") {
    array_deque<Payload> q{4, 5, 6};
    q.pop_front();
    q.pop_front();
    q.push_back(7);

    const Payload a = -1;
    ops.clear();
    q.resize(3, a);
    REQUIRE(ops == "-1 P(const P&)\n");
    REQUIRE(q.capacity() == 3);
    REQUIRE(q == array_deque<Payload>{6, 7, -1});
}

TEST_CASE("array_deque::resize(size_t, value) up", "[array_deque]") {
    array_deque<Payload> q{4, 5};
    q.pop_front();
    q.push_back(6);

    const Payload a = -1;
    ops.clear();
    q.resize(4, a);
    REQUIRE(ops == "5 P(P&&)\n6 P(P&&)\n0 ~P()\n0 ~P()\n-1 P(const P&)\n-1 P(const P&)\n");
    REQUIRE(q.capacity() == 4);
    REQUIRE(q == array_deque<Payload>{5, 6, -1, -1});
}

TEST_CASE("array_deque::clear() size:2", "[array_deque]") {
    array_deque<Payload> q{4, 5, 6};
    q.pop_front();
    ops.clear();
    q.clear();
    REQUIRE(ops == "5 ~P()\n6 ~P()\n");
    REQUIRE(q.capacity() == 3);
    REQUIRE(q.size() == 0);
}

TEST_CASE("array_deque::clear() size:0", "[array_deque]") {
    array_deque<Payload> q;
    ops.clear();
    q.clear();
    REQUIRE(ops == "");
    REQUIRE(q.capacity() == 0);
    REQUIRE(q.size() == 0);
}

// extra calls!
/*TEST_CASE("array_deque::insert(iterator, std::initializer_list<T>) inplace", "[array_deque]") {
    std::initializer_list<Payload> a = {4, 5};
    array_deque<Payload> q{1, 2};
    q.reserve(4);
    ops.clear();
    q.insert(q.begin() + 1, a);
    REQUIRE(ops == "");
    REQUIRE(q.capacity() == 4);
    REQUIRE(q == array_deque<Payload>{1, 4, 5, 2});
}*/

TEST_CASE("array_deque::insert(iterator, std::initializer_list<T>) grow", "[array_deque]") {
    std::initializer_list<Payload> a = {4, 5};
    array_deque<Payload> q{1, 2};
    ops.clear();
    q.insert(q.begin() + 1, a);
    REQUIRE(ops == "1 P(P&&)\n2 P(P&&)\n0 ~P()\n0 ~P()\n4 P(const P&)\n5 P(const P&)\n");
    REQUIRE(q.capacity() == 4);
    REQUIRE(q == array_deque<Payload>{1, 4, 5, 2});
}

// extra call!
/*TEST_CASE("array_deque::insert(iterator, const T&) inplace", "[array_deque]") {
    const Payload a = 4;
    array_deque<Payload> q{1, 2};
    q.reserve(3);
    ops.clear();
    q.insert(q.begin() + 1, a);
    REQUIRE(ops == "2 P(P&&)\n0 =(const P&)\n");
    REQUIRE(q.capacity() == 4);
    REQUIRE(q == array_deque<Payload>{1, 4, 2});
}*/

// extra call!
/*TEST_CASE("array_deque::insert(iterator, T&&) inplace", "[array_deque]") {
    Payload a = 4;
    array_deque<Payload> q{1, 2};
    q.reserve(3);
    ops.clear();
    q.insert(q.begin() + 1, std::move(a));
    REQUIRE(ops == "2 P(P&&)\n4 =(P&&)\n");
    REQUIRE(q.capacity() == 4);
    REQUIRE(q == array_deque<Payload>{1, 4, 2});
}*/

TEST_CASE("array_deque::insert(iterator, T&&)", "[array_deque]") {
    Payload a = 4;
    array_deque<Payload> q{1, 2};
    ops.clear();
    q.insert(q.begin() + 1, std::move(a));
    REQUIRE(ops == "1 P(P&&)\n2 P(P&&)\n0 ~P()\n0 ~P()\n4 P(P&&)\n");
    REQUIRE(q.capacity() == 4);
    REQUIRE(q == array_deque<Payload>{1, 4, 2});
}

TEST_CASE("array_deque::erase(iterator) size:1", "[array_deque]") {
    array_deque<Payload> q{3};
    ops.clear();
    q.erase(q.begin());
    REQUIRE(ops == "3 ~P()\n");
    REQUIRE(q.capacity() == 1);
    REQUIRE(q == array_deque<Payload>{});
}

TEST_CASE("array_deque::erase(iterator) front", "[array_deque]") {
    array_deque<Payload> q{1, 2, 3};
    ops.clear();
    q.erase(q.begin());
    REQUIRE(q.capacity() == 3);
    REQUIRE(ops == "1 ~P()\n");
    REQUIRE(q == array_deque<Payload>{2, 3});
}

TEST_CASE("array_deque::erase(iterator) middle", "[array_deque]") {
    array_deque<Payload> q{1, 2, 3};
    ops.clear();
    q.erase(q.begin() + 1);
    REQUIRE(q.capacity() == 3);
    REQUIRE(ops == "2 =(P&&)\n0 ~P()\n");
    REQUIRE(q == array_deque<Payload>{1, 3});
}

TEST_CASE("array_deque::erase(iterator) back", "[array_deque]") {
    array_deque<Payload> q{1, 2, 3};
    ops.clear();
    q.erase(q.begin() + 2);
    REQUIRE(q.capacity() == 3);
    REQUIRE(ops == "3 ~P()\n");
    REQUIRE(q == array_deque<Payload>{1, 2});
}

TEST_CASE("array_deque::erase(iterator, iterator) front empty", "[array_deque]") {
    array_deque<Payload> q{1, 2, 3, 4};
    ops.clear();
    q.erase(q.begin(), q.begin());
    REQUIRE(q.capacity() == 4);
    REQUIRE(ops == "");
    REQUIRE(q == array_deque<Payload>{1, 2, 3, 4});
}

TEST_CASE("array_deque::erase(iterator, iterator) front", "[array_deque]") {
    array_deque<Payload> q{1, 2, 3, 4};
    ops.clear();
    q.erase(q.begin(), q.begin() + 2);
    REQUIRE(q.capacity() == 4);
    REQUIRE(ops == "1 ~P()\n2 ~P()\n");
    REQUIRE(q == array_deque<Payload>{3, 4});
}

TEST_CASE("array_deque::erase(iterator, iterator) middle", "[array_deque]") {
    array_deque<Payload> q{1, 2, 3, 4};
    ops.clear();
    q.erase(q.begin() + 1, q.begin() + 3);
    REQUIRE(q.capacity() == 4);
    REQUIRE(ops == "2 =(P&&)\n3 ~P()\n0 ~P()\n");
    REQUIRE(q == array_deque<Payload>{1, 4});
}

TEST_CASE("array_deque::erase(iterator, iterator) back empty", "[array_deque]") {
    array_deque<Payload> q{1, 2, 3, 4};
    ops.clear();
    q.erase(q.end(), q.end());
    REQUIRE(q.capacity() == 4);
    REQUIRE(ops == "");
    REQUIRE(q == array_deque<Payload>{1, 2, 3, 4});
}

TEST_CASE("array_deque::erase(iterator, iterator) back", "[array_deque]") {
    array_deque<Payload> q{1, 2, 3, 4};
    ops.clear();
    q.erase(q.begin() + 2, q.end());
    REQUIRE(q.capacity() == 4);
    REQUIRE(ops == "3 ~P()\n4 ~P()\n");
    REQUIRE(q == array_deque<Payload>{1, 2});
}

TEST_CASE("std::erase(array_deque&, value)", "[array_deque]") {
    array_deque<Payload> a{1, 2, 3, 4};
    a.pop_front();
    a.pop_front();
    a.push_back(5);
    a.push_back(6);

    const Payload p = 5;
    ops.clear();
    std::erase(a, p);
    REQUIRE(ops == "5 =(P&&)\n0 ~P()\n");

    REQUIRE(a == array_deque<Payload>{3, 4, 6});
    REQUIRE(a.capacity() == 4);
}

TEST_CASE("std::erase_if(array_deque&, predicate)", "[array_deque]") {
    array_deque<Payload> a{1, 2, 3, 4};
    a.pop_front();
    a.pop_front();
    a.push_back(5);
    a.push_back(6);

    ops.clear();
    std::erase_if(a, [](const auto& e) {return e.a % 2 == 1; });
    REQUIRE(ops == "3 =(P&&)\n0 =(P&&)\n5 ~P()\n0 ~P()\n");

    REQUIRE(a == array_deque<Payload>{4, 6});
    REQUIRE(a.capacity() == 4);
}

TEST_CASE("array_deque::assign(iterator, iterator) empty", "[array_deque]") {
    array_deque<Payload> q{5, 6, 7};
    Payload e[] = {};
    ops.clear();
    q.assign(e, e);
    REQUIRE(q.capacity() == 3);
    REQUIRE(ops == "5 ~P()\n6 ~P()\n7 ~P()\n");
    REQUIRE(q == array_deque<Payload>{});
}

TEST_CASE("array_deque::assign(iterator, iterator) inplace", "[array_deque]") {
    array_deque<Payload> q{5, 6, 7};
    Payload e[] = {2, 3};
    ops.clear();
    q.assign(e, e + 2);
    REQUIRE(q.capacity() == 3);
    REQUIRE(ops == "5 =(const P&)\n6 =(const P&)\n7 ~P()\n");
    REQUIRE(q == array_deque<Payload>{2, 3});
}

// assign(InputIt first, InputIt last) is tested through these 3 cases
TEST_CASE("array_deque::assign(std::initialized_list<T>) empty", "[array_deque]") {
    array_deque<Payload> q{5, 6, 7};
    std::initializer_list<Payload> e = {};
    ops.clear();
    q.assign(e);
    REQUIRE(q.capacity() == 3);
    REQUIRE(q == array_deque<Payload>{});
    REQUIRE(ops == "5 ~P()\n6 ~P()\n7 ~P()\n");
}

TEST_CASE("array_deque::assign(std::initialized_list<T>) inplace", "[array_deque]") {
    array_deque<Payload> q{5, 6, 7};
    std::initializer_list<Payload> e = {2, 3};
    ops.clear();
    q.assign(e);
    REQUIRE(q.capacity() == 3);
    REQUIRE(ops == "5 =(const P&)\n6 =(const P&)\n7 ~P()\n");
    REQUIRE(q == array_deque<Payload>{2, 3});
}

TEST_CASE("array_deque::assign(std::initialized_list<T>) grow", "[array_deque]") {
    array_deque<Payload> q{5};
    std::initializer_list<Payload> e = {2, 3};
    ops.clear();
    q.assign(e);
    REQUIRE(q.capacity() == 2);
    REQUIRE(ops == "5 ~P()\n2 P(const P&)\n3 P(const P&)\n");
    REQUIRE(q == array_deque<Payload>{2, 3});
}

TEST_CASE("array_deque::assign(size_type, const T&) empty", "[array_deque]") {
    array_deque<Payload> q{5, 6, 7};
    const Payload a = -1;
    ops.clear();
    q.assign(0, a);
    REQUIRE(q.capacity() == 3);
    REQUIRE(q == array_deque<Payload>{});
    REQUIRE(ops == "5 ~P()\n6 ~P()\n7 ~P()\n");
}

TEST_CASE("array_deque::assign(size_type, const T&) inplace", "[array_deque]") {
    array_deque<Payload> q{5, 6, 7};
    const Payload a = -1;
    ops.clear();
    q.assign(2, a);
    REQUIRE(q.capacity() == 3);
    REQUIRE(ops == "5 =(const P&)\n6 =(const P&)\n7 ~P()\n");
    REQUIRE(q == array_deque<Payload>{-1, -1});
}

TEST_CASE("array_deque::assign(size_t, const T&) grow", "[array_deque]") {
    array_deque<Payload> q{5};
    const Payload a = -1;
    ops.clear();
    q.assign(2, a);
    REQUIRE(q.capacity() == 2);
    REQUIRE(ops == "5 ~P()\n-1 P(const P&)\n-1 P(const P&)\n");
    REQUIRE(q == array_deque<Payload>{-1, -1});
}

TEST_CASE("array_deque::emplace(iterator, ...) front grow", "[array_deque]") {
    array_deque<Payload> q{4, 5, 6};
    ops.clear();
    q.emplace(q.begin(), -1, "str");
    REQUIRE(q.capacity() == 4);
    REQUIRE(ops == "4 P(P&&)\n5 P(P&&)\n6 P(P&&)\n0 ~P()\n0 ~P()\n0 ~P()\n-1 P(int, string)\n");
    REQUIRE(q == array_deque<Payload>{-1, 4, 5, 6});
    REQUIRE(q.front().str == "str");
}

TEST_CASE("array_deque::emplace(iterator, ...) back grow", "[array_deque]") {
    array_deque<Payload> q{4, 5, 6};
    ops.clear();
    q.emplace(q.end(), -1, "str");
    REQUIRE(q.capacity() == 4);
    REQUIRE(ops == "4 P(P&&)\n5 P(P&&)\n6 P(P&&)\n0 ~P()\n0 ~P()\n0 ~P()\n-1 P(int, string)\n");
    REQUIRE(q == array_deque<Payload>{4, 5, 6, -1});
    REQUIRE(q.back().str == "str");
}

TEST_CASE("array_deque::emplace(iterator, ...) front inplace", "[array_deque]") {
    array_deque<Payload> q{4, 5, 6};
    q.reserve(4);
    ops.clear();
    q.emplace(q.begin(), -1, "str");
    REQUIRE(q.capacity() == 4);
    REQUIRE(ops == "-1 P(int, string)\n");
    REQUIRE(q == array_deque<Payload>{-1, 4, 5, 6});
    REQUIRE(q.front().str == "str");
}

TEST_CASE("array_deque::emplace(iterator, ...) back middle1", "[array_deque]") {
    array_deque<Payload> q{4, 5, 6, 7, 8};
    q.reserve(6);
    ops.clear();
    q.emplace(q.begin() + 2, -1, "str");
    REQUIRE(q.capacity() == 6);
    REQUIRE(ops == "4 P(P&&)\n0 =(P&&)\n0 ~P()\n-1 P(int, string)\n");
    REQUIRE(q == array_deque<Payload>{4, 5, -1, 6, 7, 8});
    REQUIRE(q[2].str == "str");
}

/*TEST_CASE("array_deque::emplace(iterator, ...) back middle2", "[array_deque]") {
    array_deque<Payload> q{4, 5, 6, 7, 8};
    q.reserve(6);
    ops.clear();
    q.emplace(q.begin() + 3, -1, "str");
    REQUIRE(q.capacity() == 6);
    REQUIRE(ops == "8 P(P&&)\n0 =(P&&)\n0 ~P() \n-1 P(int, string)\n");
    REQUIRE(q == array_deque<Payload>{4, 5, 6, -1, 7, 8});
    REQUIRE(q[3].str == "str");
}*/

TEST_CASE("array_deque::emplace(iterator, ...) back inplace", "[array_deque]") {
    array_deque<Payload> q{4, 5, 6};
    q.reserve(4);
    ops.clear();
    std::cout << "EMPLACE end" << std::endl;
    q.emplace(q.end(), -1, "str");
    REQUIRE(q.capacity() == 4);
    REQUIRE(ops == "-1 P(int, string)\n");
    REQUIRE(q == array_deque<Payload>{4, 5, 6, -1});
    REQUIRE(q.back().str == "str");
}
