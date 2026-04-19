#include "test_base.h"
#include "../impl/processor.h"
#include <iostream>
#include <cassert>

using namespace jotcad::geo;

void verify_schema(const std::string& path, const nlohmann::json& schema) {
    std::cout << "Verifying Schema: " << path << "..." << std::endl;
    
    // Core requirements for mesh discovery
    assert(schema.contains("path") && "Schema missing 'path'");
    assert(schema.at("path") == path && "Schema path mismatch");
    assert(schema.contains("arguments") && "Schema missing 'arguments' block");
    
    // Check specific aliases
    if (path == "jot/on") {
        assert(schema.contains("path") && schema["path"] == "jot/on");
    }
    if (path == "jot/rotateX") {
        bool found_alias = false;
        if (schema.contains("aliases")) {
            for (const auto& a : schema["aliases"]) if (a == "jot/rx") found_alias = true;
        }
        assert(found_alias && "rotateX missing alias jot/rx");
    }
    
    std::cout << "  - PASS" << std::endl;
}

int main() {
    register_all_ops();
    
    const auto& registry = Processor::registry_instance();
    assert(!registry.empty() && "Registry is empty!");

    for (const auto& [path, op] : registry) {
        verify_schema(path, op.schema);
    }

    std::cout << "\n✨ ALL SCHEMAS ARE WELL-FORMED" << std::endl;
    return 0;
}
