#pragma once

#include "../core/processor.h"
#include "../unfold/clusterer.h"

namespace jotcad {
namespace geo {

template <typename P = JotVfsProtocol>
struct UnfoldOp : P {
    static constexpr const char* path = "jot/unfold";

    static void execute(fs::VFSNode* vfs, const fs::Selector& fulfilling, Shape subject) {
        // 1. Convert subject to a single consolidated mesh
        boolean::ExactMesh mesh;
        collect_and_merge_mesh(vfs, subject, Matrix::identity(), mesh);

        if (mesh.is_empty()) return;

        // 2. Perform unfolding
        std::vector<unfold::UnfoldPatch> patches = unfold::Clusterer::unfold(mesh);

        // 3. Build the output assembly
        Shape out;
        out.tags["type"] = "group";
        for (size_t i = 0; i < patches.size(); ++i) {
            Shape island;
            island.tags["name"] = "island_" + std::to_string(i);
            island.tags["type"] = "group";

            const auto& patch = patches[i];

            // A. The 2D faces
            Shape faces;
            faces.tags["type"] = "surface";
            faces.tags["isJot"] = true;
            faces.geometry = vfs->materialize<Geometry>(patch.geometry);
            island.components.push_back(std::move(faces));

            // B. The Fold lines
            Geometry fold_geo;
            Geometry cut_geo;
            
            // We use the patch geometry vertices for both
            fold_geo.vertices = patch.geometry.vertices;
            cut_geo.vertices = patch.geometry.vertices;

            for (const auto& face : patch.geometry.faces) {
                for (const auto& loop : face.loops) {
                    for (size_t j = 0; j < loop.size(); ++j) {
                        int u = loop[j];
                        int v = loop[(j + 1) % loop.size()];
                        auto key = std::make_pair(std::min(u, v), std::max(u, v));
                        
                        auto it = patch.edge_tags.find(key);
                        if (it != patch.edge_tags.end()) {
                            if (it->second == "fold") {
                                fold_geo.segments.push_back({u, v});
                            } else {
                                cut_geo.segments.push_back({u, v});
                            }
                        }
                    }
                }
            }

            if (!fold_geo.segments.empty()) {
                Shape folds;
                folds.tags["type"] = "segments";
                folds.tags["role"] = "fold";
                folds.tags["color"] = "#0000ff"; // Blue for folds
                folds.geometry = vfs->materialize<Geometry>(fold_geo);
                island.components.push_back(std::move(folds));
            }

            if (!cut_geo.segments.empty()) {
                Shape cuts;
                cuts.tags["type"] = "segments";
                cuts.tags["role"] = "cut";
                cuts.tags["color"] = "#ff0000"; // Red for cuts
                cuts.geometry = vfs->materialize<Geometry>(cut_geo);
                island.components.push_back(std::move(cuts));
            }

            out.components.push_back(std::move(island));
        }

        vfs->write(fulfilling.with_output("$out"), out);
    }

    static void collect_and_merge_mesh(fs::VFSNode* vfs, const Shape& s, const Matrix& parent_tf, boolean::ExactMesh& target) {
        Matrix world_tf = parent_tf * s.tf;
        
        if (s.geometry.has_value()) {
            Geometry geo = vfs->read<Geometry>(s.geometry.value());
            boolean::ExactMesh mesh = boolean::Engine::geometry_to_mesh(geo);
            boolean::Engine::transform_mesh(mesh, world_tf);
            
            if (target.is_empty()) {
                target = std::move(mesh);
            } else {
                boolean::Engine::join_mesh_by_mesh(target, mesh);
            }
        }
        for (const auto& child : s.components) {
            collect_and_merge_mesh(vfs, child, world_tf, target);
        }
    }

    static std::vector<std::string> argument_keys() { return {"$in"}; }
    static typename P::json schema() {
        return {
            {"path", "jot/unfold"},
            {"description", "Unfolds a 3D polyhedral mesh into 2D patches."},
            {"arguments", {
                {{"name", "$in"}, {"type", "jot:shape"}}
            }},
            {"outputs", {{"$out", {{"type", "jot:shape"}}}}}
        };
    }
};

static void unfold_init(fs::VFSNode* vfs) {
    Processor::register_op<UnfoldOp<>, Shape>(vfs, "jot/unfold");
}

} // namespace geo
} // namespace jotcad
