#pragma once

#include "../core/processor.h"
#include "../unfold/clusterer.h"

namespace jotcad {
namespace geo {

template <typename P = JotVfsProtocol>
struct UnfoldOp : P {
    static constexpr const char* path = "jot/unfold";

    static void execute(fs::VFSNode* vfs, const fs::Selector& fulfilling, Shape subject, double minFold = 1.0) {
        boolean::ExactMesh mesh;
        collect_and_merge_mesh(vfs, subject, mesh);
        if (mesh.is_empty()) return;

        std::vector<unfold::UnfoldPatch> patches = unfold::Clusterer::unfold(mesh, minFold);

        Shape out;
        out.tags = subject.tags;
        out.tags["type"] = "group";
        
        for (size_t i = 0; i < patches.size(); ++i) {
            const auto& patch = patches[i];

            Shape island;
            island.tags = subject.tags;
            island.tags["name"] = "island_" + std::to_string(i);
            island.tags["type"] = "surface";
            island.tags["isJot"] = true;
            island.geometry = vfs->materialize<Geometry>(patch.geometry);
            
            out.components.push_back(std::move(island));
        }
        vfs->write(fulfilling.with_output("$out"), out);
    }

    static void collect_and_merge_mesh(fs::VFSNode* vfs, const Shape& s, boolean::ExactMesh& target) {
        if (s.geometry.has_value()) {
            Geometry geo = vfs->read<Geometry>(s.geometry.value());
            boolean::ExactMesh mesh = boolean::Engine::geometry_to_mesh(geo);
            boolean::Engine::transform_mesh(mesh, s.tf);
            if (target.is_empty()) target = std::move(mesh);
            else boolean::Engine::join_mesh_by_mesh(target, mesh);
        }
        for (const auto& child : s.components) collect_and_merge_mesh(vfs, child, target);
    }

    static std::vector<std::string> argument_keys() { return {"$in", "minFold"}; }
    static typename P::json schema() {
        return {
            {"path", "jot/unfold"},
            {"description", "Unfolds a 3D polyhedral mesh into 2D patches."},
            {"inputs", {{"$in", {{"type", "jot:shape"}}}}},
            {"arguments", {
                {{"name", "minFold"}, {"type", "jot:number"}, {"default", 1.0}, {"description", "Elasticity threshold in degrees. Bends under this angle are not marked as fold lines."}}
            }},
            {"outputs", {{"$out", {{"type", "jot:shape"}}}}}
        };
    }
};

static void unfold_init(fs::VFSNode* vfs) {
    Processor::register_op<UnfoldOp<>, Shape, double>(vfs, "jot/unfold");
}

} // namespace geo
} // namespace jotcad
