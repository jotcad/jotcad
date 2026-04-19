#include <iostream>
#include <string>
#include <vector>
#include <iomanip>
#include <sstream>
#include "../../fs/cpp/vendor/json.hpp"
#include "../../fs/cpp/vendor/sha256.h"

using json = nlohmann::json;

std::string vfs_hash256_str(const std::string& data) {
    picosha2::hash256_one_by_one hasher;
    hasher.process(data.begin(), data.end());
    hasher.finish();
    std::vector<uint8_t> hash(32);
    hasher.get_hash_bytes(hash.begin(), hash.end());
    
    std::stringstream ss;
    for (uint8_t b : hash) {
        ss << std::setfill('0') << std::setw(2) << std::hex << (int)b;
    }
    return ss.str();
}

void normalize_json(json& j) {
    if (j.is_number_float()) {
        double v = j.get<double>();
        if (v == (double)(long long)v) {
            j = (long long)v;
        }
    } else if (j.is_object()) {
        for (auto& [key, value] : j.items()) {
            normalize_json(value);
        }
    } else if (j.is_array()) {
        for (auto& element : j) {
            normalize_json(element);
        }
    }
}

std::string get_cid(const std::string& path, const json& parameters) {
    json s = {{"path", path}, {"parameters", parameters}};
    normalize_json(s);
    return vfs_hash256_str(s.dump());
}

struct TestCase {
    std::string name;
    std::string path;
    json parameters;
    std::string expected_cid;
};

int main() {
    std::vector<TestCase> cases = {
        {
            "empty",
            "geo/test",
            json::object(),
            "cb932f0d598aaad75c101df6376df477c895388805bf90044727a712fb8bf2e7"
        },
        {
            "integers",
            "geo/box",
            {{"width", 10}, {"height", 20}},
            "720a69d8ab575e9cc7239e7a29ad0ab599ba1e2900a72780d27ff244de4e3fcf"
        },
        {
            "floats",
            "geo/circle",
            {{"diameter", 10.5}},
            "6c533b232b5fbed3c7c8ad10d61dca6f6975292a5db58b7e3331bd04c8a3d48f"
        },
        {
            "nested",
            "geo/offset",
            {{"kerf", 1}, {"source", {{"path", "geo/box"}, {"parameters", {{"width", 5}}}}}},
            "6ac24561eef57aaf8d9924d6caac17edaa8178d6fa4998c09ac8bd495685706b"
        },
        {
            "integers as floats",
            "geo/box",
            {{"width", 10.0}},
            "47de0d7742a69477cec99b5e64a0783a8c1e51efeab9cf510104b2c55831b293"
        }
    };

    int failed = 0;
    for (const auto& tc : cases) {
        std::string actual = get_cid(tc.path, tc.parameters);
        if (actual == tc.expected_cid) {
            std::cout << "[PASS] " << tc.name << std::endl;
        } else {
            std::cout << "[FAIL] " << tc.name << std::endl;
            std::cout << "  Expected: " << tc.expected_cid << std::endl;
            std::cout << "  Actual:   " << actual << std::endl;
            std::cout << "  Dump:     " << (json({{"path", tc.path}, {"parameters", tc.parameters}}).dump()) << std::endl;
            failed++;
        }
    }

    return failed > 0 ? 1 : 0;
}
