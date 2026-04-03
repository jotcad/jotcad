#include "../include/vfs_client.h"
#include <iostream>
#include <chrono>
#include <thread>

int main(int argc, char** argv) {
    std::string base_url = (argc > 1) ? argv[1] : "http://localhost:9090/vfs";
    std::string peer_id = "cpp-agent-1";

    std::cout << "[C++ Agent] Starting VFS Client..." << std::endl;
    auto vfs = jotcad::fs::create_rest_client(base_url, peer_id);

    // 1. Setup Inbound Request Handler (Symmetry)
    vfs->on_read([](const std::string& path, const jotcad::fs::json& params) {
        std::cout << "[C++ Agent] Hub is reading from our mailbox: " << path << std::endl;
        std::string mock_data = "Symmetric data from C++";
        return std::vector<uint8_t>(mock_data.begin(), mock_data.end());
    });

    // 2. Watch for Work (Blackboard Agent pattern)
    vfs->watch("geometry/box*", [vfs = vfs.get()](const jotcad::fs::VFSEvent& ev) {
        if (ev.state == "PENDING") {
            std::cout << "[C++ Agent] Saw demand for: " << ev.path << std::endl;
            
            // 1. Acquire Lease
            if (vfs->lease(ev.path, ev.parameters, 5000)) {
                std::cout << "[C++ Agent] Leased: " << ev.path << ". Computing..." << std::endl;
                
                // 2. Mock Compute
                std::string result = "Mesh(box, size=" + ev.parameters.dump() + ")";
                std::vector<uint8_t> data(result.begin(), result.end());
                
                // 3. Write back
                vfs->write(ev.path, ev.parameters, data);
                std::cout << "[C++ Agent] Filled: " << ev.path << std::endl;
            }
        }
    });

    // 3. Keep running
    std::cout << "[C++ Agent] Running. Press Ctrl+C to exit." << std::endl;
    while (true) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    return 0;
}
