#pragma once

#include "../../fs/cpp/include/vfs_client.h"
#include "geometry.h"
#include <string>
#include <iostream>

namespace jotcad {
namespace geo {

/**
 * Base class for Single-Task Geometry Evaluators.
 */
class Processor {
public:
    struct Context {
        std::string hub_url;
        std::string path;
        nlohmann::json parameters;
        std::string peer_id;
    };

    static Context parse_args(int argc, char** argv) {
        Context ctx;
        for (int i = 1; i < argc; ++i) {
            std::string arg = argv[i];
            if (arg == "--hub" && i + 1 < argc) ctx.hub_url = argv[++i];
            else if (arg == "--path" && i + 1 < argc) ctx.path = argv[++i];
            else if (arg == "--params" && i + 1 < argc) ctx.parameters = nlohmann::json::parse(argv[++i]);
            else if (arg == "--peer" && i + 1 < argc) ctx.peer_id = argv[++i];
        }
        return ctx;
    }

    /**
     * Executes the processor lifecycle.
     */
    template<typename T>
    static int run(int argc, char** argv, T&& logic) {
        auto ctx = parse_args(argc, argv);
        if (ctx.hub_url.empty() || ctx.path.empty()) {
            std::cerr << "Usage: processor --hub <url> --path <path> --params <json>" << std::endl;
            return 1;
        }

        auto vfs = jotcad::fs::create_rest_client(ctx.hub_url, ctx.peer_id.empty() ? "geo-agent" : ctx.peer_id);

        // 1. Lease the work
        if (!vfs->lease(ctx.path, ctx.parameters, 30000)) {
            std::cout << "[Processor] Could not acquire lease for " << ctx.path << ". Skipping." << std::endl;
            return 0;
        }

        try {
            // 2. Execute logic
            logic(vfs.get(), ctx.path, ctx.parameters);
        } catch (const std::exception& e) {
            std::cerr << "[Processor] Error: " << e.what() << std::endl;
            return 1;
        }

        return 0;
    }
};

} // namespace geo
} // namespace jotcad
