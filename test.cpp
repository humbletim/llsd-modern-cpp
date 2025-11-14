#include <iostream>
#include <sstream>
#include <cassert>
#include <vector>
#include <string>
#include <map>
#include "llsd_modern.hpp"

void test_undef() {
    std::cout << "Testing Undef" << std::endl;
    std::stringstream ss;
    ss << '!';
    auto val = llsd_modern::parse_binary(ss);
    assert(std::holds_alternative<llsd_modern::Undef>(val.data));
    std::cout << "PASS" << std::endl;
}

void test_bool() {
    std::cout << "Testing Bool" << std::endl;
    std::stringstream ss;
    ss << '1';
    auto val = llsd_modern::parse_binary(ss);
    assert(std::holds_alternative<bool>(val.data));
    assert(std::get<bool>(val.data) == true);

    ss.str("");
    ss.clear();
    ss << '0';
    val = llsd_modern::parse_binary(ss);
    assert(std::holds_alternative<bool>(val.data));
    assert(std::get<bool>(val.data) == false);
    std::cout << "PASS" << std::endl;
}

void test_integer() {
    std::cout << "Testing Integer" << std::endl;
    std::stringstream ss;
    const char data[] = {'i', '\x00', '\x00', '\x01', '\x02'};
    ss.write(data, sizeof(data));
    auto val = llsd_modern::parse_binary(ss);
    assert(std::holds_alternative<std::int32_t>(val.data));
    assert(std::get<std::int32_t>(val.data) == 258);
    std::cout << "PASS" << std::endl;
}

void test_real() {
    std::cout << "Testing Real" << std::endl;
    std::stringstream ss;
    const char data[] = {'r', '\x40', '\x09', '\x21', '\xfb', '\x54', '\x44', '\x2d', '\x18'};
    ss.write(data, sizeof(data));
    auto val = llsd_modern::parse_binary(ss);
    assert(std::holds_alternative<double>(val.data));
    assert(std::get<double>(val.data) > 3.14158 && std::get<double>(val.data) < 3.14160);
    std::cout << "PASS" << std::endl;
}

void test_string() {
    std::cout << "Testing String" << std::endl;
    std::stringstream ss;
    const char data[] = {'s', '\x00', '\x00', '\x00', '\x05', 'h', 'e', 'l', 'l', 'o'};
    ss.write(data, sizeof(data));
    auto val = llsd_modern::parse_binary(ss);
    assert(std::holds_alternative<std::string>(val.data));
    assert(std::get<std::string>(val.data) == "hello");
    std::cout << "PASS" << std::endl;
}

void test_uuid() {
    std::cout << "Testing UUID" << std::endl;
    std::stringstream ss;
    ss << 'u' << "abcdefghijklmnop";
    auto val = llsd_modern::parse_binary(ss);
    assert(std::holds_alternative<llsd_modern::LLUUID>(val.data));
    std::cout << "PASS" << std::endl;
}

void test_uri() {
    std::cout << "Testing URI" << std::endl;
    std::stringstream ss;
    const char data[] = {'l', '\x00', '\x00', '\x00', '\x0b', 'e', 'x', 'a', 'm', 'p', 'l', 'e', '.', 'c', 'o', 'm'};
    ss.write(data, sizeof(data));
    auto val = llsd_modern::parse_binary(ss);
    assert(std::holds_alternative<llsd_modern::URI>(val.data));
    assert(std::get<llsd_modern::URI>(val.data).s == "example.com");
    std::cout << "PASS" << std::endl;
}

void test_map() {
    std::cout << "Testing Map" << std::endl;
    std::stringstream ss;
    const char data[] = {'{', '\x00', '\x00', '\x00', '\x01', 'k', '\x00', '\x00', '\x00', '\x03', 'k', 'e', 'y', 'i', '\x00', '\x00', '\x00', '\x01', '}'};
    ss.write(data, sizeof(data));
    auto val = llsd_modern::parse_binary(ss);
    assert(std::holds_alternative<std::unique_ptr<llsd_modern::Map>>(val.data));
    auto map = std::get<std::unique_ptr<llsd_modern::Map>>(val.data).get();
    assert(map->size() == 1);
    assert(map->count("key") == 1);
    assert(std::holds_alternative<std::int32_t>((*map)["key"].data));
    assert(std::get<std::int32_t>((*map)["key"].data) == 1);
    std::cout << "PASS" << std::endl;
}

void test_array() {
    std::cout << "Testing Array" << std::endl;
    std::stringstream ss;
    const char data[] = {'[', '\x00', '\x00', '\x00', '\x02', 'i', '\x00', '\x00', '\x00', '\x01', 'i', '\x00', '\x00', '\x00', '\x02', ']'};
    ss.write(data, sizeof(data));
    auto val = llsd_modern::parse_binary(ss);
    assert(std::holds_alternative<std::unique_ptr<llsd_modern::Array>>(val.data));
    auto arr = std::get<std::unique_ptr<llsd_modern::Array>>(val.data).get();
    assert(arr->size() == 2);
    assert(std::holds_alternative<std::int32_t>((*arr)[0].data));
    assert(std::get<std::int32_t>((*arr)[0].data) == 1);
    assert(std::holds_alternative<std::int32_t>((*arr)[1].data));
    assert(std::get<std::int32_t>((*arr)[1].data) == 2);
    std::cout << "PASS" << std::endl;
}

void test_cow_and_sharing() {
    std::cout << "Testing COW, Lifetime, and Sharing" << std::endl;
    std::stringstream ss;

    // 1. Create a map: {'key': 1}
    const char data[] = {'{', '\x00', '\x00', '\x00', '\x01', 'k', '\x00', '\x00', '\x00', '\x03', 'k', 'e', 'y', 'i', '\x00', '\x00', '\x00', '\x01', '}'};
    ss.write(data, sizeof(data));

    // 2. Parse the original object.
    // This now holds a unique_ptr to the map.
    auto v_orig = llsd_modern::parse_binary(ss);

    // 3. Create a copy.
    // This tests the (manually-written) Value copy-constructor.
    // It *must* perform a deep copy of the map.
    auto v_copy = v_orig;

    // 4. Move the original object.
    // This tests the (manually-written) Value move-constructor/assignment.
    // v_moved now owns the map v_orig *used* to own.
    // v_orig's internal unique_ptr should now be null.
    auto v_moved = std::move(v_orig);

    // 5. Mutate the copy.
    auto map_copy = std::get<std::unique_ptr<llsd_modern::Map>>(v_copy.data).get();
    (*map_copy)["key"] = llsd_modern::Value(std::int32_t(99));
    (*map_copy)["new_key"] = llsd_modern::Value(std::string("mutation_test"));

    // 6. Access v_moved.
    // **This is the segfault test.** If v_orig's move constructor
    // (or v_copy's copy constructor) was buggy and didn't handle
    // ownership correctly, v_moved might hold a dangling pointer.
    assert(std::holds_alternative<std::unique_ptr<llsd_modern::Map>>(v_moved.data));
    auto map_moved = std::get<std::unique_ptr<llsd_modern::Map>>(v_moved.data).get();
    assert(map_moved != nullptr); // Sanity check

    // 7. Check for side-effects (COW test).
    // The copied map (map_copy) was mutated. The original map (now in map_moved)
    // must not have changed.

    // Check v_moved (the original data)
    assert(map_moved->count("new_key") == 0);
    assert(std::holds_alternative<std::int32_t>((*map_moved)["key"].data));
    assert(std::get<std::int32_t>((*map_moved)["key"].data) == 1); // Must still be 1

    // Check v_copy (the mutated data)
    assert(map_copy->count("new_key") == 1);
    assert(std::get<std::int32_t>((*map_copy)["key"].data) == 99); // Must be 99

    std::cout << "PASS" << std::endl;
}


int main() {
    test_undef();
    test_bool();
    test_integer();
    test_real();
    test_string();
    test_uuid();
    test_uri();
    test_map();
    test_array();
    test_cow_and_sharing();

    return 0;
}
