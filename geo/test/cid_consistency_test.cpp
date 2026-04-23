#include <iostream>
#include <string>
#include <vector>
#include "../../fs/cpp/cid.h"

using json = nlohmann::json;

struct TestCase {
    std::string name;
    json val;
    std::string expected_cid;
    std::string expected_safe;
};

int main() {
    std::vector<TestCase> cases = {
        {
            "empty",
            json::object(),
            "49c5feb68d8bef00a7634384c15d91911981d4e65d01687b169b95e28a7ebcc5",
            "BgAAAAA="
        },
        {
            "integers",
            {{"a", 1}, {"b", 2}},
            "d3d36ade80394108d6a7464314c163c2145cf147968b30920efabd51ea688977",
            "BgAAAAIEAAAAAWEDP/AAAAAAAAAEAAAAAWIDQAAAAAAAAAA="
        },
        {
            "floats",
            {{"a", 1.5}, {"b", 2.5}},
            "4d74c61eceea590c5348f7dcff288f805665c66a97c226714cb4beb3ad300787",
            "BgAAAAIEAAAAAWEDP/gAAAAAAAAEAAAAAWIDQAQAAAAAAAA="
        },
        {
            "nested",
            {{"a", {{"b", 1}}}},
            "4bae03292e31ff8318ffe356e4ca9797049cc919072f7d2d11ea722539d69206",
            "BgAAAAEEAAAAAWEGAAAAAQQAAAABYgM/8AAAAAAAAA=="
        },
        {
            "integers as floats",
            {{"a", 1.0}, {"b", 2.0}},
            "d3d36ade80394108d6a7464314c163c2145cf147968b30920efabd51ea688977",
            "BgAAAAIEAAAAAWEDP/AAAAAAAAAEAAAAAWIDQAAAAAAAAAA="
        }
    };

    std::cout << "Verifying Safe-JCB / Base64 Consistency (C++ vs JS)..." << std::endl;

    int failed = 0;
    for (const auto& tc : cases) {
        std::string actual_safe = fs::encode_safe(tc.val);
        std::string actual_cid = fs::vfs_hash256_str(actual_safe);

        if (actual_cid == tc.expected_cid && actual_safe == tc.expected_safe) {
            std::cout << "[PASS] " << tc.name << std::endl;
        } else {
            std::cout << "[FAIL] " << tc.name << std::endl;
            if (actual_safe != tc.expected_safe) {
                std::cout << "  SAFE Mismatch:" << std::endl;
                std::cout << "    Expected: " << tc.expected_safe << std::endl;
                std::cout << "    Actual:   " << actual_safe << std::endl;
            }
            if (actual_cid != tc.expected_cid) {
                std::cout << "  CID Mismatch:" << std::endl;
                std::cout << "    Expected: " << tc.expected_cid << std::endl;
                std::cout << "    Actual:   " << actual_cid << std::endl;
            }
            failed++;
        }
    }

    if (failed == 0) std::cout << "✨ ALL CONSISTENCY TESTS PASSED" << std::endl;
    return failed > 0 ? 1 : 0;
}
