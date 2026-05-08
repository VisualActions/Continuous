// Continuous Engine - small string utility helpers.
#pragma once

#include "continuous/core/Types.h"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <string>
#include <string_view>
#include <vector>

namespace cn::str {

inline std::string to_lower(std::string_view s) {
    std::string out(s);
    std::transform(out.begin(), out.end(), out.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return out;
}

inline std::string to_upper(std::string_view s) {
    std::string out(s);
    std::transform(out.begin(), out.end(), out.begin(),
                   [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
    return out;
}

inline bool ends_with(std::string_view s, std::string_view suffix) {
    return s.size() >= suffix.size() &&
           std::memcmp(s.data() + s.size() - suffix.size(), suffix.data(), suffix.size()) == 0;
}

inline bool starts_with(std::string_view s, std::string_view prefix) {
    return s.size() >= prefix.size() &&
           std::memcmp(s.data(), prefix.data(), prefix.size()) == 0;
}

inline std::vector<std::string_view> split(std::string_view s, char delim) {
    std::vector<std::string_view> out;
    usize start = 0;
    for (usize i = 0; i < s.size(); ++i) {
        if (s[i] == delim) {
            out.emplace_back(s.substr(start, i - start));
            start = i + 1;
        }
    }
    out.emplace_back(s.substr(start));
    return out;
}

inline std::string trim(std::string_view s) {
    auto isspace_ = [](char c) { return std::isspace(static_cast<unsigned char>(c)) != 0; };
    usize a = 0, b = s.size();
    while (a < b && isspace_(s[a])) ++a;
    while (b > a && isspace_(s[b - 1])) --b;
    return std::string(s.substr(a, b - a));
}

// Compile-time FNV-1a 64.
constexpr u64 fnv1a64(std::string_view s, u64 seed = 1469598103934665603ull) {
    u64 h = seed;
    for (char c : s) {
        h ^= static_cast<u8>(c);
        h *= 1099511628211ull;
    }
    return h;
}

constexpr u32 fnv1a32(std::string_view s, u32 seed = 2166136261u) {
    u32 h = seed;
    for (char c : s) {
        h ^= static_cast<u8>(c);
        h *= 16777619u;
    }
    return h;
}

} // namespace cn::str
