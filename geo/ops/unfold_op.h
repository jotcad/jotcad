#pragma once

#include "../core/processor.h"
#include "../unfold/clusterer.h"

namespace jotcad {
namespace geo {

template <typename P = JotVfsProtocol>
struct UnfoldOp : P {
    static constexpr const char* path = "jot/unfold";

    static void execute(fs::VFSNode* vfs, const fs::Selector& fulfilling, Shape subject, double minFold = 1.0, std::string rule = "grow", std::string strategy = "") {
        boolean::ExactMesh mesh;
        collect_and_merge_mesh(vfs, subject, mesh);
        if (mesh.is_empty()) return;

        // Support both strategy and rule for backwards compatibility
        std::string active_rule = rule;
        if (!strategy.empty()) {
            active_rule = strategy;
        }

        std::vector<unfold::UnfoldPatch> patches = unfold::Clusterer::unfold(mesh, minFold, active_rule);

        Shape out;
        out.tags = subject.tags;
        out.tags["type"] = "group";
        
        for (size_t i = 0; i < patches.size(); ++i) {
            const auto& patch = patches[i];

            Shape island_group;
            island_group.tags = subject.tags;
            island_group.tags["name"] = "island_" + std::to_string(i);
            island_group.add_tag("type", "item");
            island_group.add_tag("item:name", "island_" + std::to_string(i));

            // 1. Cut Shape Component (Faces)
            Shape cut_shape;
            cut_shape.tags = subject.tags;
            cut_shape.tags["name"] = "island_" + std::to_string(i) + "_cut";
            cut_shape.tags["type"] = "surface";
            cut_shape.tags["unfold"] = "cut";
            cut_shape.tags["isJot"] = true;
            
            Geometry cut_geo;
            cut_geo.vertices = patch.geometry.vertices;
            cut_geo.faces = patch.geometry.faces;
            cut_shape.geometry = vfs->materialize<Geometry>(cut_geo);
            island_group.components.push_back(std::move(cut_shape));

            // 2. Fold Shape Component (Segments) - only if there are fold lines
            if (!patch.geometry.segments.empty()) {
                Shape fold_shape;
                fold_shape.tags = subject.tags;
                fold_shape.tags["name"] = "island_" + std::to_string(i) + "_fold";
                fold_shape.tags["type"] = "segments";
                fold_shape.tags["unfold"] = "fold";
                fold_shape.tags["isJot"] = true;

                Geometry fold_geo;
                fold_geo.vertices = patch.geometry.vertices;
                fold_geo.segments = patch.geometry.segments;
                fold_shape.geometry = vfs->materialize<Geometry>(fold_geo);
                island_group.components.push_back(std::move(fold_shape));
            }
            
            out.components.push_back(std::move(island_group));
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

    static std::vector<std::string> argument_keys() { return {"$in", "minFold", "rule", "strategy"}; }
    static typename P::json schema() {
        return {
            {"path", "jot/unfold"},
            {"description", "Unfolds a 3D polyhedral mesh into 2D patches."},
            {"inputs", {{"$in", {{"type", "jot:shape"}}}}},
            {"arguments", {
                {{"name", "minFold"}, {"type", "jot:number"}, {"default", 1.0}, {"description", "Elasticity threshold in degrees. Bends under this angle are not marked as fold lines."}},
                {{"name", "rule"}, {"type", "jot:string"}, {"default", "grow"}, {"enum", {"grow", "pair"}}, {"description", "Unfolding rule: 'grow' (sequential) or 'pair' (hierarchical pairwise)."}},
                {{"name", "strategy"}, {"type", "jot:string"}, {"default", ""}, {"optional", true}, {"description", "Legacy alias for 'rule'."}}
            }},
            {"outputs", {{"$out", {{"type", "jot:shape"}}}}}
        };
    }
};

static void unfold_init(fs::VFSNode* vfs) {
    Processor::register_op<UnfoldOp<>, Shape, double, std::string, std::string>(vfs, "jot/unfold");
}

} // namespace geo
} // namespace jotcad
