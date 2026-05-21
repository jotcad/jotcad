#pragma once
#include "protocols.h"
#include "processor.h"
#include "pack/packaide_engine.h"

namespace jotcad {
namespace geo {

template <typename P = JotVfsProtocol>
struct PackOp : P {
    static constexpr const char* path = "jot/pack";

    static void execute(fs::VFSNode* vfs, const fs::Selector& fulfilling, 
                       std::optional<Shape> in, std::optional<Shape> parts_arg, std::optional<Shape> sheet, 
                       double spacing, double margin, double rotations) {
        
        // 1. Identify input parts
        std::vector<Shape> parts;
        if (in.has_value()) {
            collect_leaf_shapes(in.value(), parts);
        }
        if (parts_arg.has_value()) {
            collect_leaf_shapes(parts_arg.value(), parts);
        }

        if (parts.empty()) {
            vfs->write(fulfilling.with_output("$out"), Shape());
            return;
        }

        // 2. Identify sheets
        std::vector<Shape> sheets;
        if (sheet.has_value()) {
            collect_leaf_shapes(sheet.value(), sheets);
        }

        // 3. Execute Nesting
        pack::PackaideEngine::Config config;
        config.spacing = FT(spacing);
        config.margin = FT(margin);
        config.rotations = (int)rotations;

        pack::PackaideEngine::PackResult res = pack::PackaideEngine::pack(vfs, parts, sheets, config);

        // 4. Construct Output Assembly
        Shape out;
        out.add_tag("type", "group");
        out.tf = Matrix::identity();
        
        FT current_x_offset = FT(0);
        FT sheet_gap = FT(spacing) * FT(5.0); 

        for (size_t b = 0; b < res.bins.size(); ++b) {
            Shape sheet_group = res.bins[b];
            
            Shape sheet_background;
            sheet_background.geometry = vfs->materialize(res.remainders[b]); 
            sheet_background.tags = sheets[b % sheets.size()].tags;
            sheet_background.tags["sheet"] = (double)(b + 1);
            sheet_background.tf = Matrix::identity(); 
            
            sheet_group.components.insert(sheet_group.components.begin(), sheet_background);

            auto s_bb = res.remainders[b].bounds();
            FT sw = FT(s_bb.xmax() - s_bb.xmin());
            
            Matrix layout_tf = Matrix::translate(current_x_offset, FT(0), FT(0));
            
            for (auto& child : sheet_group.components) {
                child.tf = layout_tf * child.tf;
            }
            
            sheet_group.tf = Matrix::identity();

            current_x_offset = current_x_offset + sw + sheet_gap;
            out.components.push_back(std::move(sheet_group));
        }

        if (!res.unplaced.empty()) {
            throw std::runtime_error("Nesting Failure: " + std::to_string(res.unplaced.size()) + 
                                   " part(s) could not fit within the provided sheet(s) GEOMETRICALLY.");
        }
        
        vfs->write(fulfilling.with_output("$out"), out);
    }

    static void collect_leaf_shapes(const Shape& s, std::vector<Shape>& leaves) {
        if (s.geometry.has_value()) {
            leaves.push_back(s);
        } else {
            for (const auto& child : s.components) {
                collect_leaf_shapes(child, leaves);
            }
        }
    }
    
    static std::vector<std::string> argument_keys() { 
        return {"$in", "parts", "sheet", "spacing", "margin", "rotations"}; 
    }
    
    static typename P::json schema() {
        return {
            {"path", "jot/pack"},
            {"description", "Packs multiple shapes into one or more sheets."},
            {"inputs", {{"$in", {{"type", "jot:shape"}, {"optional", true}}}}},
            {"arguments", json::array({
                {{"name", "parts"}, {"type", "jot:shape"}, {"optional", true}},
                {{"name", "sheet"}, {"type", "jot:shape"}, {"optional", true}},
                {{"name", "spacing"}, {"type", "number"}, {"default", 2.0}},
                {{"name", "margin"}, {"type", "number"}, {"default", 0.0}},
                {{"name", "rotations"}, {"type", "number"}, {"default", 1.0}}
            })},
            {"outputs", {{"$out", {{"type", "jot:shape"}}}}}
        };
    }
};

static void pack_init(fs::VFSNode* vfs) {
    Processor::register_op<PackOp<>, std::optional<Shape>, std::optional<Shape>, std::optional<Shape>, double, double, double>(vfs, "jot/pack");
}

} // namespace geo
} // namespace jotcad
