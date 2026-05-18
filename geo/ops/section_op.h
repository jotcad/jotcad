#pragma once
#include "protocols.h"
#include "processor.h"
#include "boolean/engine.h"
#include <CGAL/Polygon_mesh_slicer.h>
#include <CGAL/Polygon_2.h>
#include <CGAL/General_polygon_set_2.h>
#include <CGAL/Gps_segment_traits_2.h>

namespace jotcad {
namespace geo {

template <typename P = JotVfsProtocol>
struct SectionOp : P {
    static constexpr const char* path = "jot/section";

    static void collect_planes(const Shape& s, std::vector<Matrix>& frames) {
        // If the shape has geometry or is tagged as a plane, use its transform
        if (s.geometry.has_value() || (s.tags.contains("type") && s.tags["type"] == "plane")) {
            frames.push_back(s.tf);
        }
        for (const auto& child : s.components) {
            collect_planes(child, frames);
        }
        // If it's a completely empty group, use its transform as a plane anyway
        if (!s.geometry.has_value() && s.components.empty()) {
            frames.push_back(s.tf);
        }
    }

    static void execute_single(fs::VFSNode* vfs, const Shape& in, const Matrix& section_tf, Geometry& res_combined) {
        if (!in.geometry.has_value()) return;
        Geometry geo = vfs->read<Geometry>(in.geometry.value());
        
        // Transform geometry to the plane's local space (Inverse mapping)
        Matrix inv_tf = section_tf.inverse();
        geo.apply_tf(inv_tf);
        
        EK::Plane_3 local_plane(EK::Point_3(0, 0, 0), EK::Vector_3(0, 0, 1));
        
        if (!geo.faces.empty()) {
            boolean::Surface_mesh mesh = boolean::Engine::geometry_to_mesh(geo);
            CGAL::Polygon_mesh_slicer<boolean::Surface_mesh, EK> slicer(mesh);
            std::vector<std::vector<EK::Point_3>> polylines;
            slicer(local_plane, std::back_inserter(polylines));

            boolean::General_polygon_set_2 gps;
            for (const auto& poly : polylines) {
                if (poly.size() < 3) continue;
                boolean::Polygon_2 p2d;
                for (const auto& p3d : poly) p2d.push_back(EK::Point_2(p3d.x(), p3d.y()));
                if (p2d.size() < 3) continue;
                if (p2d.is_clockwise_oriented()) p2d.reverse_orientation();
                gps.join(p2d);
            }
            
            Geometry res = boolean::Engine::gps_to_geometry(gps);
            res.apply_tf(section_tf); // Move result back to world space
            
            // Merge into res_combined
            int base = (int)res_combined.vertices.size();
            for (const auto& v : res.vertices) res_combined.vertices.push_back(v);
            for (const auto& f : res.faces) {
                Geometry::Face nf;
                for (const auto& l : f.loops) {
                    std::vector<int> nl;
                    for (int idx : l) nl.push_back(base + idx);
                    nf.loops.push_back(nl);
                }
                res_combined.faces.push_back(nf);
            }
        }
        // TODO: Handle segments and points in section (producing points and empty respectively)
    }

    static void execute(fs::VFSNode* vfs, const fs::Selector& fulfilling, const Shape& in, const std::vector<Shape>& planes) {
        Geometry res_combined;
        
        if (planes.empty()) {
            // Default: Section at local Z=0
            execute_single(vfs, in, Matrix::identity(), res_combined);
        } else {
            std::vector<Matrix> frames;
            for (const auto& p_shape : planes) {
                collect_planes(p_shape, frames);
            }
            for (const auto& f : frames) {
                execute_single(vfs, in, f, res_combined);
            }
        }

        Shape out = in;
        out.geometry = vfs->materialize<Geometry>(res_combined);
        out.add_tag("type", "section");
        vfs->write(fulfilling.with_output("$out"), out);
    }

    static std::vector<std::string> argument_keys() { return {"$in", "planes"}; }
    static typename P::json schema() {
        return {
            {"path", "jot/section"},
            {"description", "Generates 2D cross-sections of the input shape at the specified planes (or local Z=0)."},
            {"arguments", {
                {{"name", "$in"}, {"type", "jot:shape"}, {"affiliate", "$out"}},
                {{"name", "planes"}, {"type", "jot:shapes"}, {"default", nlohmann::json::array()}, {"description", "Shapes whose transforms define the sectioning planes."}}
            }},
            {"outputs", {{"$out", {{"type", "jot:shape"}}}}}
        };
    }
};

static void section_init(fs::VFSNode* vfs) {
    Processor::register_op<SectionOp<>, Shape, std::vector<Shape>>(vfs, "jot/section");
}

} // namespace geo
} // namespace jotcad
