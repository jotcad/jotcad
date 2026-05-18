#pragma once
#include "protocols.h"
#include "processor.h"
#include "pack/engine.h"

namespace jotcad {
namespace geo {

template <typename P = JotVfsProtocol>
struct PackOp : P {
    static constexpr const char* path = "jot/pack";

    static void execute(fs::VFSNode* vfs, const fs::Selector& fulfilling, 
                       std::optional<Shape> in, std::optional<Shape> parts_arg, std::optional<Shape> sheet, 
                       double spacing, double margin) {
        
        // 1. Identify input parts
        // Collect from both the subject '$in' and the supplemental 'parts' argument.
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
        pack::Engine::Config config;
        config.spacing = spacing;
        config.margin = margin;

        pack::PackResult res = pack::Engine::pack(vfs, parts, sheets, config);

        // 4. Construct Output Assembly
        std::map<int, std::vector<pack::PackResult::Placement>> bin_map;
        for (const auto& p : res.placements) {
            bin_map[p.bin_index].push_back(p);
        }

        Shape out;
        out.tags["type"] = "group";
        
        double current_x_offset = 0;
        double sheet_gap = spacing * 5; 

        int num_bins = sheets.empty() ? 1 : (int)sheets.size();

        for (int b = 0; b < num_bins; ++b) {
            Shape sheet_group;
            sheet_group.tags["type"] = "group";
            sheet_group.tags["name"] = "sheet_" + std::to_string(b);

            if (b < (int)sheets.size()) {
                Shape s = sheets[b];
                s.tf = Matrix::identity(); 
                sheet_group.components.push_back(std::move(s));
            }

            for (const auto& p : bin_map[b]) {
                Shape part = parts[p.part_index];
                part.tf = p.tf; 
                sheet_group.components.push_back(std::move(part));
            }

            double sw = 1000;
            if (b < (int)sheets.size() && sheets[b].geometry.has_value()) {
                auto geo = vfs->read<Geometry>(sheets[b].geometry.value());
                auto box = geo.bounds();
                sw = box.xmax() - box.xmin();
            }
            
            sheet_group.tf = Matrix::identity().translate(FT(current_x_offset), FT(0), FT(0));
            current_x_offset += sw + sheet_gap;

            out.components.push_back(std::move(sheet_group));
        }

        if (!res.unplaced_parts.empty()) {
            throw std::runtime_error("Nesting Failure: " + std::to_string(res.unplaced_parts.size()) + 
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
                {{"name", "margin"}, {"type", "number"}, {"default", 5.0}}
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
