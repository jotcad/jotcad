#pragma once
#include <vector>
#include <string>
#include <json.hpp>
#include "processor.h"
#include "../../fs/cpp/include/vfs_node.h"

namespace jotcad {
namespace geo {

/**
 * Base Trait for VFS Operations.
 * Decouples parameter mapping, kernel logic, and VFS response formatting.
 */
template <typename Kernel>
struct VFSOpTraits {
    // 1. Parameter Extraction
    static std::vector<nlohmann::json> get_batches(const nlohmann::json& params) {
        return Processor::cartesian_batch(params, Kernel::batch_keys());
    }

    // 2. VFS Marshalling (Default: Shape Generator Pattern)
    static nlohmann::json marshal_shape(jotcad::fs::VFSNode* vfs, const nlohmann::json& batch_params, const Geometry& geo, const nlohmann::json& tags) {
        std::string mesh_text = geo.encode_text();
        std::vector<uint8_t> mesh_data(mesh_text.begin(), mesh_text.end());
        std::string hash = vfs->write_cid("geo/mesh", mesh_data);

        return {
            {"geometry", {
                {"path", "geo/mesh"},
                {"parameters", {{"cid", hash}}}
            }},
            {"parameters", batch_params},
            {"tags", tags}
        };
    }

    // 3. Execution Wrapper
    static std::vector<uint8_t> execute(jotcad::fs::VFSNode* vfs, const std::string& path, const nlohmann::json& params, const std::vector<std::string>& stack) {
        auto batches = get_batches(params);
        std::vector<nlohmann::json> results;

        for (const auto& batch : batches) {
            Geometry geo;
            nlohmann::json tags = Kernel::default_tags();
            
            // Execute the Pure Kernel
            Kernel::apply(batch, geo, tags);

            // Marshal to VFS
            results.push_back(marshal_shape(vfs, batch, geo, tags));
        }

        nlohmann::json final_out;
        if (results.size() == 1) {
            final_out = results[0];
        } else {
            final_out = {
                {"path", "op/group"},
                {"parameters", {{"items", results}}}
            };
        }

        std::string out_text = final_out.dump();
        return std::vector<uint8_t>(out_text.begin(), out_text.end());
    }
};

/**
 * Convenience Macro to register a Trait-based Op
 */
template <typename Kernel>
void register_vfs_op() {
    Processor::Operation op;
    op.path = Kernel::path();
    op.logic = VFSOpTraits<Kernel>::execute;
    op.schema = Kernel::schema();
    Processor::register_op(op);
}

} // namespace geo
} // namespace jotcad
