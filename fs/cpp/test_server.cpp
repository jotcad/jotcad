#include "vfs_node.h"
#include <iostream>
#include <string>
#include <sstream>

using namespace fs;

int main(int argc, char** argv) {
    std::string node_id = "cpp-node";
    int port = 9090;
    std::string storage_dir = "./vfs_storage";

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--id" && i + 1 < argc) node_id = argv[++i];
        else if (arg == "--port" && i + 1 < argc) port = std::stoi(argv[++i]);
        else if (arg == "--storage" && i + 1 < argc) storage_dir = argv[++i];
    }

    VFSNode::Config config;
    config.id = node_id;
    config.port = port;
    config.storage_dir = storage_dir;

    if (const char* env_neighbors = std::getenv("NEIGHBORS")) {
        std::stringstream ss(env_neighbors);
        std::string token;
        while (std::getline(ss, token, ',')) {
            if (!token.empty()) {
                config.neighbors.push_back(token);
            }
        }
    }

    std::cout << "[TestServer] Starting " << node_id << " on port " << port << std::endl;
    VFSNode node(config);
    node.listen(); // Blocks until server stops

    return 0;
}
