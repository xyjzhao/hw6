#include "ht.h"
#include "hash.h"
#include <iostream>
#include <string>
#include <stdexcept>
#include <exception>

using namespace std;

// Simple assert with exception on failure
void assert_true(bool cond, const string& msg = "") {
    if (!cond) throw runtime_error(msg);
}

#define TEST_CASE(name) \
    cout << "[TEST] " << name << " ... "; \
    try {

#define END_TEST() \
        cout << "PASS" << endl; \
    } catch (const exception& e) { \
        cout << "FAIL (" << e.what() << ")" << endl; \
    }

// Test 1: Basic insert, find, size
void testBasicInsertFind() {
    HashTable<string,int> ht(0.5, LinearProber<string>());
    ht.insert({"a",1});
    ht.insert({"b",2});
    assert_true(ht.size() == 2, "size should be 2");
    auto p = ht.find("a");
    assert_true(p && p->second == 1, "a should be found with value 1");
    assert_true(ht.find("c") == nullptr, "c should not be found");
}

// Test 2: operator[] and at(), exception
void testAtAndOperatorBrackets() {
    HashTable<string,int> ht(0.5, LinearProber<string>());
    ht.insert({"x",10});
    assert_true(ht.at("x") == 10, "at(x) must be 10");
    ht["x"] = 42;
    assert_true(ht.at("x") == 42, "x should now be 42");
    // operator[] creates if missing
    ht["y"] = 7;
    assert_true(ht.at("y") == 7, "y should be 7");
    // at() on missing throws
    bool threw = false;
    try {
        ht.at("z");
    } catch (const out_of_range&) {
        threw = true;
    }
    assert_true(threw, "at(z) must throw out_of_range");
}

// Test 3: remove and size adjustment
void testRemove() {
    HashTable<string,int> ht(0.5, LinearProber<string>());
    ht.insert({"r1",1});
    ht.insert({"r2",2});
    assert_true(ht.size() == 2);
    ht.remove("r1");
    assert_true(ht.size() == 1, "size should drop to 1");
    assert_true(ht.find("r1") == nullptr, "r1 must be gone");
    // removing non-existent key
    ht.remove("notthere");
    assert_true(ht.size() == 1, "size unchanged when removing missing");
}

// Test 4: resizing and rehashing preserves data
void testResizeRehash() {
    // small threshold to force resize quickly
    HashTable<int,int,LinearProber<int>> ht(0.3, LinearProber<int>());
    int initialCap = 11;
    for (int i = 0; i < 5; i++) {
        ht.insert({i, i*10});
    }
    // 5/11 = .45 > .3 so should have resized at insert 4 or 5
    for (int i = 0; i < 5; i++) {
        auto p = ht.find(i);
        assert_true(p && p->second == i*10, "all keys must remain after resize");
    }
}

// Test 5: collision handling via bad hash
struct BadHash {
    size_t operator()(const string&) const { return 1; }
};
void testCollisionResolution() {
    HashTable<string,int,LinearProber<string>,BadHash> ht(0.6, LinearProber<string>(), BadHash());
    // All keys collide to bucket 1
    ht.insert({"c1",1});
    ht.insert({"c2",2});
    ht.insert({"c3",3});
    assert_true(ht.find("c1") && ht.find("c2") && ht.find("c3"), "collided keys should all be found");
    // test update existing
    ht.insert({"c2",22});
    assert_true(ht.find("c2")->second == 22, "c2 must update to 22");
}

// Test 6: double hashing prober usage
void testDoubleHashProber() {
    DoubleHashProber<string,MyStringHash> dhp;
    HashTable<string,int,DoubleHashProber<string,MyStringHash>> ht(0.6, dhp);
    ht.insert({"alpha",1});
    ht.insert({"beta",2});
    ht.insert({"gamma",3});
    assert_true(ht.find("alpha") && ht.find("beta") && ht.find("gamma"), "double hashing keys found");
}

int main() {
    TEST_CASE("Basic insert/find") testBasicInsertFind(); END_TEST();
    TEST_CASE("at() and operator[]") testAtAndOperatorBrackets(); END_TEST();
    TEST_CASE("Remove behavior") testRemove(); END_TEST();
    TEST_CASE("Resize and rehash") testResizeRehash(); END_TEST();
    TEST_CASE("Collision resolution (linear)") testCollisionResolution(); END_TEST();
    TEST_CASE("Double-hash probing") testDoubleHashProber(); END_TEST();
    return 0;
}
