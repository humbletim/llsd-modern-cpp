#include <iostream>
#include <sstream>
#include <cassert>
#include <vector>
#include <string>
#include <map>
#include "llsd_modern.hpp"

// Helper function to create a specific time_point for testing
std::chrono::system_clock::time_point create_test_date() {
    std::tm tm = {};
    tm.tm_year = 2025 - 1900; // 2025
    tm.tm_mon = 10;           // November (0-indexed)
    tm.tm_mday = 15;
    tm.tm_hour = 12;
    tm.tm_min = 30;
    tm.tm_sec = 0;
    return std::chrono::system_clock::from_time_t(timegm(&tm));
}

void test_undef() {
    std::cout << "Testing Undef" << std::endl;
    std::stringstream ss;
    ss << '!';
    auto val = llsd_modern::parse_binary(ss);
    assert(std::holds_alternative<llsd_modern::Undef>(val.data));
    std::cout << "PASS" << std::endl;
}

// This tests the original .oxp -> .json -> .oxp path (B -> J -> B)
void test_binary_to_json_round_trip() {
    std::cout << "Testing Binary -> JSON -> Binary Round Trip" << std::endl;

    // 1. Create a complex Value object from C++
    auto original_map = std::make_unique<llsd_modern::Map>();
    (*original_map)["integer"] = llsd_modern::Value(42);
    (*original_map)["string"] = llsd_modern::Value(std::string("is the answer"));
    (*original_map)["uri"] = llsd_modern::Value(llsd_modern::URI{"http://example.com"});
    (*original_map)["binary"] = llsd_modern::Value(llsd_modern::Binary{{1, 2, 3, 4}}); // Padded
    (*original_map)["date"] = llsd_modern::Value(llsd_modern::LLDate(create_test_date()));
    llsd_modern::Value original_val(std::move(original_map));

    // 2. Format to JSON
    std::string json_str = llsd_modern::format_json(original_val);
    std::cerr << "Intermediate JSON: " << json_str << std::endl;

    // 3. Parse from JSON
    llsd_modern::Value parsed_val = llsd_modern::parse_json(json_str);

    // 4. Format to Binary
    std::stringstream binary_stream(std::ios::in | std::ios::out | std::ios::binary);
    llsd_modern::format_binary(binary_stream, parsed_val);

    // 5. Parse from Binary
    llsd_modern::Value final_val = llsd_modern::parse_binary(binary_stream);

    // 6. Compare
    assert(std::holds_alternative<std::unique_ptr<llsd_modern::Map>>(final_val.data));
    auto final_map = std::get<std::unique_ptr<llsd_modern::Map>>(final_val.data).get();
    assert(final_map->size() == 5);
    assert(std::get<std::int32_t>((*final_map)["integer"].data) == 42);
    assert(std::get<std::string>((*final_map)["string"].data) == "is the answer");
    // URI is parsed as a string (known type loss in JSON)
    assert(std::get<std::string>((*final_map)["uri"].data) == "http://example.com");
    // This locks in the from_base64 padding fix
    assert(std::get<llsd_modern::Binary>((*final_map)["binary"].data).b == std::vector<std::uint8_t>({1, 2, 3, 4}));
    // This locks in the timegm fix
    assert(std::get<llsd_modern::LLDate>((*final_map)["date"].data).toString() == "2025-11-15T12:30:00Z");


    std::cout << "PASS" << std::endl;
}

// This new test locks in the fixes for the J -> B -> J path
void test_json_to_binary_round_trip() {
    std::cout << "Testing JSON -> Binary -> JSON Round Trip" << std::endl;

    // 1. Define an input JSON, including padded binary and UTC date
    //    (using single quotes for C++ string literal)
    std::string input_json =
        "{"
        "\"binary_padded\":\"data:base64,AQIDBA==\","
        "\"date_utc\":\"2025-11-15T12:30:00Z\","
        "\"integer\":123"
        "}";

    // 2. Parse from JSON
    llsd_modern::Value val_from_json = llsd_modern::parse_json(input_json);

    // 3. Format to Binary
    std::stringstream binary_stream(std::ios::in | std::ios::out | std::ios::binary);
    llsd_modern::format_binary(binary_stream, val_from_json);

    // 4. Parse from Binary
    llsd_modern::Value val_from_binary = llsd_modern::parse_binary(binary_stream);

    // 5. Format back to JSON
    std::string output_json = llsd_modern::format_json(val_from_binary);

    // 6. Compare - this asserts the round trip is lossless
    // Note: nlohmann::json sorts keys, so our simple string compare is fine.
    assert(input_json == output_json);

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
    auto v_orig = llsd_modern::parse_binary(ss);

    // 3. Create a copy.
    auto v_copy = v_orig;

    // 4. Move the original object.
    auto v_moved = std::move(v_orig);

    // 5. Mutate the copy.
    auto map_copy = std::get<std::unique_ptr<llsd_modern::Map>>(v_copy.data).get();
    (*map_copy)["key"] = llsd_modern::Value(std::int32_t(99));
    (*map_copy)["new_key"] = llsd_modern::Value(std::string("mutation_test"));

    // 6. Access v_moved.
    assert(std::holds_alternative<std::unique_ptr<llsd_modern::Map>>(v_moved.data));
    auto map_moved = std::get<std::unique_ptr<llsd_modern::Map>>(v_moved.data).get();
    assert(map_moved != nullptr); // Sanity check

    // 7. Check for side-effects (COW test).
    assert(map_moved->count("new_key") == 0);
    assert(std::holds_alternative<std::int32_t>((*map_moved)["key"].data));
    assert(std::get<std::int32_t>((*map_moved)["key"].data) == 1); // Must still be 1

    assert(map_copy->count("new_key") == 1);
    assert(std::get<std::int32_t>((*map_copy)["key"].data) == 99); // Must be 99

    std::cout << "PASS" << std::endl;
}

void test_json_output() {
    std::cout << "Testing JSON Output" << std::endl;
    auto map = std::make_unique<llsd_modern::Map>();
    (*map)["undef"] = llsd_modern::Value(llsd_modern::Undef{});
    (*map)["true"] = llsd_modern::Value(true);
    (*map)["false"] = llsd_modern::Value(false);
    (*map)["integer"] = llsd_modern::Value(123);
    (*map)["real"] = llsd_modern::Value(3.14);
    (*map)["string"] = llsd_modern::Value(std::string("hello"));
    (*map)["uri"] = llsd_modern::Value(llsd_modern::URI{"http://example.com"});
    (*map)["binary"] = llsd_modern::Value(llsd_modern::Binary{{1, 2, 3}});

    auto array = std::make_unique<llsd_modern::Array>();
    array->push_back(llsd_modern::Value(1));
    array->push_back(llsd_modern::Value(std::string("two")));
    (*map)["array"] = llsd_modern::Value(std::move(array));

    llsd_modern::Value val(std::move(map));
    std::string json_str = llsd_modern::format_json(val);

    std::string expected_json = "{\"array\":[1,\"two\"],\"binary\":\"data:base64,AQID\",\"false\":false,\"integer\":123,\"real\":3.14,\"string\":\"hello\",\"true\":true,\"undef\":null,\"uri\":\"http://example.com\"}";
    assert(json_str == expected_json);
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
    test_json_output();
    test_binary_to_json_round_trip();
    test_json_to_binary_round_trip(); // The new "lock-in" test

    return 0;
}
