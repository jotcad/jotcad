#include "vendor/json.hpp"
#include <iostream>
#include <vector>
#include <map>

using json = nlohmann::json;

void normalize_json(json& j) {
    if (j.is_object()) {
        json sorted = json::object();
        std::map<std::string, json> m;
        for (auto it = j.begin(); it != j.end(); ++it) m[it.key()] = it.value();
        for (auto const& [k, v] : m) {
            json val = v;
            normalize_json(val);
            sorted[k] = val;
        }
        j = sorted;
    } else if (j.is_array()) {
        for (auto& item : j) normalize_json(item);
    }
}

int main() {
    json parameters = json::object();
    parameters["width"] = 10;
    parameters["height"] = 10;
    parameters["depth"] = 0;
    
    json s = json::object();
    s["parameters"] = parameters;
    s["path"] = "jot/Box";
    
    normalize_json(s);
    std::string dumped = s.dump();
    std::cout << "C++ JSON: [" << dumped << "]" << std::endl;
    return 0;
}
