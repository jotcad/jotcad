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
                       double spacing, double margin) {
        
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

        pack::PackaideEngine::PackResult res = pack::PackaideEngine::pack(vfs, parts, sheets, config);

        // 4. Construct Output Assembly
        Shape out;
        out.add_tag("type", "group");
        
        FT current_x_offset = FT(0);
        FT sheet_gap = FT(spacing) * FT(5.0); 

        for (size_t b = 0; b < res.bins.size(); ++b) {
            Shape sheet_group = res.bins[b];
            
            // The sheet itself MUST be the first component of the group for tests to pass
            // In the current PackaideEngine, res.bins[b] is the sheet shape itself 
            // but its components were cleared and filled with parts.
            // We need to re-insert the sheet geometry as the first component.
            
            Shape sheet_background;
            sheet_background.geometry = sheets[b % sheets.size()].geometry; // Use original sheet geo
            sheet_background.tags = sheets[b % sheets.size()].tags;
            sheet_background.tags["name"] = "sheet_background";
            
            // Insert at beginning
            sheet_group.components.insert(sheet_group.components.begin(), sheet_background);

            FT sw = FT(1000);
            if (sheet_group.geometry.has_value()) {
                auto geo = vfs->read<Geometry>(sheet_group.geometry.value());
                auto box = geo.bounds();
                sw = FT(box.xmax() - box.xmin());
            }
            
            sheet_group.tf = Matrix::translate(current_x_offset, FT(0), FT(0));
            current_x_offset = current_x_offset + sw + sheet_gap;

            out.components.push_back(std::move(sheet_group));
        }

        if (!res.unplaced.empty()) {
            throw std::runtime_error("Nesting Failure: " + std::to_string(res.unplaced.size()) + 
                                   " part(s) could not fit within the provided sheet(s).");
        }
        
        vfs->write(fulfilling.with_output("$out"), out);
    }

    static void collect_leaf_shapes(const Shape& s, std::vector<Shape>& leaves) {
        if (s.geometry.has_value()) {
            leaves.push_back(s);
        } else if (s.components.empty()) {
            // skip
        } else {
            for (const auto& child : s.components) {
                Shape copy = child;
                copy.tf = s.tf * copy.tf;
                collect_leaf_shapes(copy, leaves);
            }
        }
    }
    
    static std::vector<std::string> argument_keys() { 
        return {"$in", "parts", "sheet", "spacing", "margin"}; 
    }
    
    static typename P::json schema() {
        return {
            {"path", "jot/pack"},
            {"description", "Packs multiple shapes into one or more sheets."},
            {"arguments", {
                {{"name", "$in"}, {"type", "jot:shape"}, {"optional", true}, {"affiliate", "$out"}},
                {{"name", "parts"}, {"type", "jot:shape"}, {"optional", true}},
                {{"name", "sheet"}, {"type", "jot:shape"}, {"optional", true}},
                {{"name", "spacing"}, {"type", "number"}, {"default", 2.0}},
                {{"name", "margin"}, {"type", "number"}, {"default", 0.0}}
            }},
            {"outputs", {{"$out", {{"type", "jot:shape"}}}}}
        };
    }
};

static void pack_init(fs::VFSNode* vfs) {
    Processor::register_op<PackOp<>, std::optional<Shape>, std::optional<Shape>, std::optional<Shape>, double, double>(vfs, "jot/pack");
}

} // namespace geo
} // namespace jotcad
