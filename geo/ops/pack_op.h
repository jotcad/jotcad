#pragma once
#include "protocols.h"
#include "processor.h"
#include "pack/packaide_engine.h"
#include "footprint_op.h"

namespace jotcad {
namespace geo {

template <typename P = JotVfsProtocol>
struct PackOp : P {
    static constexpr const char* path = "jot/pack";

    static void execute(fs::VFSNode* vfs, const fs::Selector& fulfilling, 
                       std::optional<Shape> in, std::optional<Shape> parts_arg, std::optional<Shape> sheet, 
                       double spacing, double margin, double rotations) {
        
        // 1. Identify input parts (items)
        std::vector<Shape> parts;
        if (in.has_value()) {
            collect_packable_items(in.value(), parts);
        }
        if (parts_arg.has_value()) {
            collect_packable_items(parts_arg.value(), parts);
        }

        if (parts.empty()) {
            vfs->write(fulfilling.with_output("$out"), Shape());
            return;
        }

        // 2. Generate 2D footprints for each part
        std::vector<Shape> footprint_parts;
        for (size_t i = 0; i < parts.size(); ++i) {
            Shape fp = FootprintOp<P>::compute_footprint_shape(vfs, parts[i]);
            fp.add_tag("orig_index", (double)i);
            footprint_parts.push_back(fp);
        }

        // 3. Identify sheets (default to 2000x2000 workbench sheet if none provided)
        std::vector<Shape> sheets;
        bool auto_sheet = false;
        if (sheet.has_value()) {
            collect_packable_items(sheet.value(), sheets);
        }
        if (sheets.empty()) {
            auto_sheet = true;
            Geometry default_sheet_geo;
            double sheet_size = 2000.0;
            default_sheet_geo.vertices = {
                {FT(0), FT(0), FT(0)},
                {FT(sheet_size), FT(0), FT(0)},
                {FT(sheet_size), FT(sheet_size), FT(0)},
                {FT(0), FT(sheet_size), FT(0)}
            };
            default_sheet_geo.faces.push_back({{{0, 1, 2, 3}}});
            Shape default_sheet;
            default_sheet.geometry = vfs->materialize(default_sheet_geo);
            default_sheet.tags["type"] = "surface";
            sheets.push_back(default_sheet);
        }

        // 4. Execute 2D Nesting on Footprints
        pack::PackaideEngine::Config config;
        config.spacing = FT(spacing);
        config.margin = FT(margin);
        config.rotations = (int)rotations;

        pack::PackaideEngine::PackResult res = pack::PackaideEngine::pack(vfs, footprint_parts, sheets, config);

        // 5. Construct Output Assembly, replacing footprints with the original 3D items
        Shape out;
        out.tf = Matrix::identity();
        
        FT current_x_offset = FT(0);
        FT sheet_gap = FT(spacing) * FT(5.0); 

        for (size_t b = 0; b < res.bins.size(); ++b) {
            Shape sheet_group;
            sheet_group.tags["sheet"] = (double)(b + 1);
            sheet_group.tf = Matrix::identity();

            if (!auto_sheet && b < res.remainders.size()) {
                Shape sheet_background;
                sheet_background.geometry = vfs->materialize(res.remainders[b]); 
                sheet_background.tags = sheets[b % sheets.size()].tags;
                sheet_background.tags["sheet"] = (double)(b + 1);
                sheet_background.tf = Matrix::identity(); 
                sheet_group.components.push_back(sheet_background);
            }

            for (const auto& placed_fp : res.bins[b].components) {
                if (placed_fp.tags.contains("orig_index")) {
                    size_t orig_idx = (size_t)placed_fp.tags["orig_index"].get<double>();
                    if (orig_idx < parts.size()) {
                        Shape original_item = parts[orig_idx];
                        apply_transform_recursive(original_item, placed_fp.tf);
                        sheet_group.components.push_back(original_item);
                    }
                }
            }

            FT sw = FT(100.0);
            if (b < res.remainders.size()) {
                auto s_bb = res.remainders[b].bounds();
                sw = FT(s_bb.xmax() - s_bb.xmin());
            }
            if (sw <= FT(0)) sw = FT(100.0);
            
            Matrix layout_tf = Matrix::translate(current_x_offset, FT(0), FT(0));
            for (auto& child : sheet_group.components) {
                apply_transform_recursive(child, layout_tf);
            }

            current_x_offset = current_x_offset + sw + sheet_gap;
            out.components.push_back(std::move(sheet_group));
        }

        if (!res.unplaced.empty()) {
            throw std::runtime_error("Nesting Failure: " + std::to_string(res.unplaced.size()) + 
                                   " part(s) could not fit within the provided sheet(s) GEOMETRICALLY.");
        }
        
        vfs->write(fulfilling.with_output("$out"), out);
    }

    static void apply_transform_recursive(Shape& s, const Matrix& t) {
        s.tf = t * s.tf;
        for (auto& child : s.components) {
            apply_transform_recursive(child, t);
        }
    }

    static void collect_packable_items(const Shape& s, std::vector<Shape>& items) {
        for (const auto& item : s.items()) {
            items.push_back(item);
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
