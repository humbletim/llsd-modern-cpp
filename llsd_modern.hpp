/**
 * @file llsd_modern.hpp
 * @brief Modern C++ header-only LLSD implementation.
 *
 * This work is a C++17 implementation of the LLSD specification.
 * Its core parsing and formatting logic is heavily inspired by and
 * derived from the Python implementation in 'python-llsd',
 * created by Linden Lab, Inc.
 *
 * Copyright (c) 2025 humbletim
 *
 * This library is licensed under the MIT License.
 *
 * ---
 *
 * The original 'python-llsd' is licensed as follows:
 *
 * Copyright (c) 2006 Linden Research, Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */
#pragma once

#include "nlohmann/json.hpp"
#include <array>
#include <chrono>
#include <ctime>
#include <cstdint>
#include <iomanip>
#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <type_traits>
#include <variant>
#include <vector>
#include <regex>

namespace llsd_modern {

class Value;

namespace detail {
    void _format_binary_recurse(std::ostream& s, const Value& v);
}

class LLUUID {
public:
    LLUUID() : bytes_{} {}
    explicit LLUUID(const std::array<std::uint8_t, 16>& bytes) : bytes_(bytes) {}

    std::string toString() const {
        std::stringstream ss;
        ss << std::hex << std::setfill('0');
        for (int i = 0; i < 4; ++i) ss << std::setw(2) << static_cast<int>(bytes_[i]);
        ss << '-';
        for (int i = 4; i < 6; ++i) ss << std::setw(2) << static_cast<int>(bytes_[i]);
        ss << '-';
        for (int i = 6; i < 8; ++i) ss << std::setw(2) << static_cast<int>(bytes_[i]);
        ss << '-';
        for (int i = 8; i < 10; ++i) ss << std::setw(2) << static_cast<int>(bytes_[i]);
        ss << '-';
        for (int i = 10; i < 16; ++i) ss << std::setw(2) << static_cast<int>(bytes_[i]);
        return ss.str();
    }

private:
    std::array<std::uint8_t, 16> bytes_;
    friend void detail::_format_binary_recurse(std::ostream& s, const Value& v);
};

class LLDate {
public:
    LLDate() : time_point_() {}
    explicit LLDate(const std::chrono::system_clock::time_point& time_point) : time_point_(time_point) {}

    std::string toString() const {
        auto time_t = std::chrono::system_clock::to_time_t(time_point_);
        std::stringstream ss;
        ss << std::put_time(std::gmtime(&time_t), "%Y-%m-%dT%H:%M:%SZ");
        return ss.str();
    }

private:
    std::chrono::system_clock::time_point time_point_;
    friend void detail::_format_binary_recurse(std::ostream& s, const Value& v);
};

class Value;

using Array = std::vector<Value>;
using Map = std::map<std::string, Value>;

struct Undef {};
struct URI { std::string s; };
struct Binary { std::vector<std::uint8_t> b; };

class Value {
public:
    using variant_type = std::variant<
        Undef,
        bool,
        std::int32_t,
        double,
        std::string,
        LLUUID,
        LLDate,
        URI,
        Binary,
        std::unique_ptr<Array>,
        std::unique_ptr<Map>
    >;

    Value() : data(Undef{}) {}

    template<typename T, typename = std::enable_if_t<!std::is_same_v<std::decay_t<T>, Value>>>
    Value(T&& value) : data(std::forward<T>(value)) {}

    Value(const Value& other);
    Value(Value&& other) noexcept = default;

    Value& operator=(const Value& other);
    Value& operator=(Value&& other) noexcept = default;

    variant_type data;
};

// Forward declaration for the parser
Value parse_binary(std::istream& s);

// Deep copy implementation for Value
inline Value::Value(const Value& other) {
    std::visit([&](auto&& arg) {
        using T = std::decay_t<decltype(arg)>;
        if constexpr (std::is_same_v<T, std::unique_ptr<Array>>) {
            if (arg) {
                data = std::make_unique<Array>(*arg);
            } else {
                data = std::unique_ptr<Array>();
            }
        } else if constexpr (std::is_same_v<T, std::unique_ptr<Map>>) {
            if (arg) {
                data = std::make_unique<Map>(*arg);
            } else {
                data = std::unique_ptr<Map>();
            }
        } else {
            data = arg;
        }
    }, other.data);
}

inline Value& Value::operator=(const Value& other) {
    if (this != &other) {
        Value temp(other);
        data = std::move(temp.data);
    }
    return *this;
}

namespace detail {

// Helper to read a specific number of bytes
inline std::vector<char> read_bytes(std::istream& s, size_t n) {
    std::vector<char> bytes(n);
    s.read(bytes.data(), n);
    if (s.gcount() != n) {
        throw std::runtime_error("Unexpected end of stream");
    }
    return bytes;
}

// Helper to read a big-endian integer
inline std::int32_t read_i32_be(std::istream& s) {
    auto bytes = read_bytes(s, 4);
    return (static_cast<std::int32_t>(static_cast<unsigned char>(bytes[0])) << 24) |
           (static_cast<std::int32_t>(static_cast<unsigned char>(bytes[1])) << 16) |
           (static_cast<std::int32_t>(static_cast<unsigned char>(bytes[2])) << 8) |
           (static_cast<std::int32_t>(static_cast<unsigned char>(bytes[3])));
}

// Helper to read a big-endian double
inline double read_double_be(std::istream& s) {
    auto bytes = read_bytes(s, 8);
    union {
        uint64_t i;
        double d;
    } val;
    val.i = 0;
    for(int i = 0; i < 8; ++i) {
        val.i = (val.i << 8) | static_cast<unsigned char>(bytes[i]);
    }
    return val.d;
}

// Helper to read a little-endian double
inline double read_double_le(std::istream& s) {
    auto bytes = read_bytes(s, 8);
    union {
        uint64_t i;
        double d;
    } val;
    val.i = 0;
    for(int i = 7; i >= 0; --i) {
        val.i = (val.i << 8) | static_cast<unsigned char>(bytes[i]);
    }
    return val.d;
}


inline std::string read_string(std::istream& s) {
    auto size = read_i32_be(s);
    if (size < 0) throw std::runtime_error("Invalid string size");
    std::string str(size, '\0');
    s.read(&str[0], size);
    if (s.gcount() != size) throw std::runtime_error("Unexpected end of stream while reading string");
    return str;
}
} // namespace detail

// Forward declaration for the JSON formatter
std::string format_json(const Value& v);
void format_binary(std::ostream& s, const Value& v);

namespace detail {

// Helper to write a big-endian integer
inline void write_i32_be(std::ostream& s, std::int32_t val) {
    s.put((val >> 24) & 0xFF);
    s.put((val >> 16) & 0xFF);
    s.put((val >> 8) & 0xFF);
    s.put(val & 0xFF);
}

// Helper to write a big-endian double
inline void write_double_be(std::ostream& s, double val) {
    union {
        uint64_t i;
        double d;
    } u;
    u.d = val;
    for (int i = 7; i >= 0; --i) {
        s.put((u.i >> (i * 8)) & 0xFF);
    }
}

// Helper to write a little-endian double
inline void write_double_le(std::ostream& s, double val) {
    union {
        uint64_t i;
        double d;
    } u;
    u.d = val;
    for (int i = 0; i < 8; ++i) {
        s.put((u.i >> (i * 8)) & 0xFF);
    }
}

inline void write_string(std::ostream& s, const std::string& str) {
    write_i32_be(s, str.size());
    s.write(str.data(), str.size());
}

// Helper for format_json
nlohmann::json to_json(const Value& v) {
    return std::visit([](auto&& arg) -> nlohmann::json {
        using T = std::decay_t<decltype(arg)>;
        if constexpr (std::is_same_v<T, Undef>) {
            return nullptr;
        } else if constexpr (std::is_same_v<T, LLUUID> || std::is_same_v<T, LLDate>) {
            return arg.toString();
        } else if constexpr (std::is_same_v<T, URI>) {
            return arg.s;
        } else if constexpr (std::is_same_v<T, Binary>) {
            // JSON doesn't have a binary type, so we'll represent it as a base64 string
             static const char* b64 = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
             std::string s = "data:base64,";
             s.reserve(s.length() + ((arg.b.size() + 2) / 3) * 4);
             for (int i = 0; i < arg.b.size(); i += 3) {
                 s += b64[arg.b[i] >> 2];
                 s += b64[((arg.b[i] & 0x3) << 4) | (i + 1 < arg.b.size() ? arg.b[i+1] >> 4 : 0)];
                 s += (i + 1 < arg.b.size() ? b64[((arg.b[i+1] & 0xF) << 2) | (i + 2 < arg.b.size() ? arg.b[i+2] >> 6 : 0)] : '=');
                 s += (i + 2 < arg.b.size() ? b64[arg.b[i+2] & 0x3F] : '=');
             }
             return s;
        } else if constexpr (std::is_same_v<T, std::unique_ptr<Array>>) {
            nlohmann::json arr = nlohmann::json::array();
            if (arg) {
                for (const auto& item : *arg) {
                    arr.push_back(to_json(item));
                }
            }
            return arr;
        } else if constexpr (std::is_same_v<T, std::unique_ptr<Map>>) {
            nlohmann::json obj = nlohmann::json::object();
            if (arg) {
                for (const auto& [key, value] : *arg) {
                    obj[key] = to_json(value);
                }
            }
            return obj;
        } else {
            return arg;
        }
    }, v.data);
}
} // namespace detail


inline std::string format_json(const Value& v) {
    return detail::to_json(v).dump();
}

inline Value parse_binary(std::istream& s) {
    char type_char;
    s.get(type_char);

    switch (type_char) {
        case '{': {
            auto map = std::make_unique<Map>();
            auto size = detail::read_i32_be(s);
            for (int i = 0; i < size; ++i) {
                s.get(type_char);
                if (type_char != 'k') throw std::runtime_error("Expected 'k' for map key");
                std::string key = detail::read_string(s);
                (*map)[key] = parse_binary(s);
            }
            s.get(type_char);
            if (type_char != '}') throw std::runtime_error("Expected '}' to close map");
            return Value(std::move(map));
        }
        case '[': {
            auto array = std::make_unique<Array>();
            auto size = detail::read_i32_be(s);
            array->reserve(size);
            for (int i = 0; i < size; ++i) {
                array->push_back(parse_binary(s));
            }
            s.get(type_char);
            if (type_char != ']') throw std::runtime_error("Expected ']' to close array");
            return Value(std::move(array));
        }
        case '!': return Value(Undef{});
        case '0': return Value(false);
        case '1': return Value(true);
        case 'i': return Value(detail::read_i32_be(s));
        case 'r': return Value(detail::read_double_be(s));
        case 'u': {
            auto bytes = detail::read_bytes(s, 16);
            std::array<std::uint8_t, 16> uuid_bytes;
            std::copy(bytes.begin(), bytes.end(), uuid_bytes.begin());
            return Value(LLUUID(uuid_bytes));
        }
        case 's': return Value(detail::read_string(s));
        case 'l': return Value(URI{detail::read_string(s)});
        case 'd': {
             auto seconds_double = detail::read_double_le(s);
             auto duration = std::chrono::duration<double>(seconds_double);
             auto time_point = std::chrono::system_clock::time_point(std::chrono::duration_cast<std::chrono::system_clock::duration>(duration));
             return Value(LLDate(time_point));
        }
        case 'b': {
            auto size = detail::read_i32_be(s);
            if (size < 0) throw std::runtime_error("Invalid binary size");
            auto bytes = detail::read_bytes(s, size);
            std::vector<std::uint8_t> binary_data(bytes.begin(), bytes.end());
            return Value(Binary{binary_data});
        }
        default:
            throw std::runtime_error("Invalid binary token");
    }
}

namespace detail {
    inline void _format_binary_recurse(std::ostream& s, const Value& v) {
        std::visit([&](auto&& arg) {
            using T = std::decay_t<decltype(arg)>;
            if constexpr (std::is_same_v<T, Undef>) {
                s.put('!');
            } else if constexpr (std::is_same_v<T, bool>) {
                s.put(arg ? '1' : '0');
            } else if constexpr (std::is_same_v<T, std::int32_t>) {
                s.put('i');
                write_i32_be(s, arg);
            } else if constexpr (std::is_same_v<T, double>) {
                s.put('r');
                write_double_be(s, arg);
            } else if constexpr (std::is_same_v<T, std::string>) {
                s.put('s');
                write_string(s, arg);
            } else if constexpr (std::is_same_v<T, LLUUID>) {
                s.put('u');
                s.write(reinterpret_cast<const char*>(arg.bytes_.data()), 16);
            } else if constexpr (std::is_same_v<T, LLDate>) {
                s.put('d');
                auto duration = std::chrono::duration<double>(arg.time_point_.time_since_epoch());
                write_double_le(s, duration.count());
            } else if constexpr (std::is_same_v<T, URI>) {
                s.put('l');
                write_string(s, arg.s);
            } else if constexpr (std::is_same_v<T, Binary>) {
                s.put('b');
                write_i32_be(s, arg.b.size());
                s.write(reinterpret_cast<const char*>(arg.b.data()), arg.b.size());
            } else if constexpr (std::is_same_v<T, std::unique_ptr<Array>>) {
                s.put('[');
                if (arg) {
                    write_i32_be(s, arg->size());
                    for (const auto& item : *arg) {
                        _format_binary_recurse(s, item);
                    }
                } else {
                    write_i32_be(s, 0);
                }
                s.put(']');
            } else if constexpr (std::is_same_v<T, std::unique_ptr<Map>>) {
                s.put('{');
                if (arg) {
                    write_i32_be(s, arg->size());
                    for (const auto& [key, value] : *arg) {
                        s.put('k');
                        write_string(s, key);
                        _format_binary_recurse(s, value);
                    }
                } else {
                    write_i32_be(s, 0);
                }
                s.put('}');
            }
        }, v.data);
    }
} // namespace detail

inline void format_binary(std::ostream& s, const Value& v) {
    detail::_format_binary_recurse(s, v);
}

// Forward declaration for the JSON parser
Value parse_json(const std::string& s);

namespace detail {
    // Helper to decode a base64 string
    inline std::vector<std::uint8_t> from_base64(const std::string& s) {
        static const std::string b64 = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
        std::vector<int> T(256, -1);
        for (int i = 0; i < 64; i++) T[b64[i]] = i;

        std::vector<std::uint8_t> out;
        int val = 0, valb = -8;
        for (char c : s) {
            if (c == '=') break;
            if (T[c] == -1) continue;
            val = (val << 6) + T[c];
            valb += 6;
            if (valb >= 0) {
                out.push_back(static_cast<std::uint8_t>((val >> valb) & 0xFF));
                valb -= 8;
            }
        }
        return out;
    }

    inline Value from_json(const nlohmann::json& j) {
        if (j.is_null()) {
            return Value(Undef{});
        } else if (j.is_boolean()) {
            return Value(j.get<bool>());
        } else if (j.is_number_integer()) {
            return Value(j.get<std::int32_t>());
        } else if (j.is_number_float()) {
            return Value(j.get<double>());
        } else if (j.is_string()) {
            const std::string s = j.get<std::string>();
            // Try to match base64 binary
            if (s.rfind("data:base64,", 0) == 0) {
                return Value(Binary{from_base64(s.substr(12))});
            }
            // Try to match UUID
            static const std::regex uuid_re("^[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}$");
            if (std::regex_match(s, uuid_re)) {
                std::string hex_str = s;
                hex_str.erase(std::remove(hex_str.begin(), hex_str.end(), '-'), hex_str.end());
                std::array<std::uint8_t, 16> bytes;
                for(int i=0; i<16; ++i) {
                    bytes[i] = std::stoi(hex_str.substr(i*2, 2), nullptr, 16);
                }
                return Value(LLUUID(bytes));
            }
            // Try to match Date
            static const std::regex date_re("^\\d{4}-\\d{2}-\\d{2}T\\d{2}:\\d{2}:\\d{2}Z$");
            if (std::regex_match(s, date_re)) {
                std::tm tm = {};
                std::stringstream ss(s);
                ss >> std::get_time(&tm, "%Y-%m-%dT%H:%M:%SZ");
                #ifdef _WIN32
                    static constexpr auto& timegm = _mkgmtime;
                #endif
                auto time_point = std::chrono::system_clock::from_time_t(timegm(&tm));
                return Value(LLDate(time_point));
            }
            return Value(s);
        } else if (j.is_array()) {
            auto array = std::make_unique<Array>();
            for (const auto& item : j) {
                array->push_back(from_json(item));
            }
            return Value(std::move(array));
        } else if (j.is_object()) {
            auto map = std::make_unique<Map>();
            for (auto it = j.begin(); it != j.end(); ++it) {
                (*map)[it.key()] = from_json(it.value());
            }
            return Value(std::move(map));
        }
        throw std::runtime_error("Unknown JSON type");
    }

}

inline Value parse_json(const std::string& s) {
    nlohmann::json j = nlohmann::json::parse(s);
    return detail::from_json(j);
}

} // namespace llsd_modern
