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
        
        if (!geo.faces.empty() || !geo.triangles.empty()) {
            boolean::Surface_mesh mesh = boolean::Engine::geometry_to_mesh(geo);
            CGAL::Polygon_mesh_slicer<boolean::Surface_mesh, EK> slicer(mesh);
            std::vector<std::vector<EK::Point_3>> polylines;
            slicer(local_plane, std::back_inserter(polylines));

            std::vector<boolean::Polygon_2> ccw_polygons;
            for (const auto& poly : polylines) {
                if (poly.size() < 3) continue;
                boolean::Polygon_2 p2d;
                for (size_t i = 0; i < poly.size(); ++i) {
                    if (i == poly.size() - 1 && poly[i] == poly[0]) continue;
                    EK::Point_2 pt(poly[i].x(), poly[i].y());
                    if (p2d.size() > 0 && p2d[p2d.size() - 1] == pt) continue;
                    p2d.push_back(pt);
                }
                if (p2d.size() < 3) continue;
                if (!p2d.is_simple()) continue;
                if (p2d.is_clockwise_oriented()) p2d.reverse_orientation();
                ccw_polygons.push_back(p2d);
            }

            size_t n = ccw_polygons.size();
            std::vector<std::vector<size_t>> contained_by(n);
            for (size_t i = 0; i < n; ++i) {
                for (size_t j = 0; j < n; ++j) {
                    if (i == j) continue;
                    if (ccw_polygons[j].bounded_side(ccw_polygons[i].vertex(0)) == CGAL::ON_BOUNDED_SIDE) {
                        contained_by[i].push_back(j);
                    }
                }
            }

            boolean::General_polygon_set_2 gps;
            for (size_t i = 0; i < n; ++i) {
                // Loops contained by an even number of other loops are Islands (outer boundaries)
                if (contained_by[i].size() % 2 == 0) {
                    boolean::Polygon_with_holes_2 pwh(ccw_polygons[i]);
                    // Find direct holes nested inside this island
                    for (size_t h = 0; h < n; ++h) {
                        if (contained_by[h].size() == contained_by[i].size() + 1) {
                            bool is_contained = false;
                            for (size_t parent : contained_by[h]) {
                                if (parent == i) {
                                    is_contained = true;
                                    break;
                                }
                            }
                            if (is_contained) {
                                boolean::Polygon_2 hole = ccw_polygons[h];
                                hole.reverse_orientation(); // Holes in CGAL PWH must be clockwise
                                pwh.add_hole(hole);
                            }
                        }
                    }
                    gps.join(pwh);
                }
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

    static bool section_recursive(fs::VFSNode* vfs, const Shape& in, const std::vector<Matrix>& slicing_planes, Shape& out) {
        out.tf = in.tf;
        out.tags = in.tags;
        
        bool has_any_geometry = false;
        
        // 1. Slice this shape's own geometry if present
        if (in.geometry.has_value()) {
            Geometry geo = vfs->read<Geometry>(in.geometry.value());
            Geometry res_combined_local;

            for (const auto& section_tf : slicing_planes) {
                // Map geometry from in.tf local space to section_tf local space
                Geometry local_geo = geo;
                Matrix to_plane = section_tf.inverse() * in.tf;
                local_geo.apply_tf(to_plane);

                EK::Plane_3 local_plane(EK::Point_3(0, 0, 0), EK::Vector_3(0, 0, 1));
                if (!local_geo.faces.empty() || !local_geo.triangles.empty()) {
                    boolean::Surface_mesh mesh = boolean::Engine::geometry_to_mesh(local_geo);
                    CGAL::Polygon_mesh_slicer<boolean::Surface_mesh, EK> slicer(mesh);
                    std::vector<std::vector<EK::Point_3>> polylines;
                    slicer(local_plane, std::back_inserter(polylines));

                    std::vector<boolean::Polygon_2> ccw_polygons;
                    for (const auto& poly : polylines) {
                        if (poly.size() < 3) continue;
                        boolean::Polygon_2 p2d;
                        for (size_t i = 0; i < poly.size(); ++i) {
                            if (i == poly.size() - 1 && poly[i] == poly[0]) continue;
                            EK::Point_2 pt(poly[i].x(), poly[i].y());
                            if (p2d.size() > 0 && p2d[p2d.size() - 1] == pt) continue;
                            p2d.push_back(pt);
                        }
                        if (p2d.size() < 3) continue;
                        if (!p2d.is_simple()) continue;
                        if (p2d.is_clockwise_oriented()) p2d.reverse_orientation();
                        ccw_polygons.push_back(p2d);
                    }

                    size_t n = ccw_polygons.size();
                    std::vector<std::vector<size_t>> contained_by(n);
                    for (size_t i = 0; i < n; ++i) {
                        for (size_t j = 0; j < n; ++j) {
                            if (i == j) continue;
                            if (ccw_polygons[j].bounded_side(ccw_polygons[i].vertex(0)) == CGAL::ON_BOUNDED_SIDE) {
                                contained_by[i].push_back(j);
                            }
                        }
                    }

                    boolean::General_polygon_set_2 gps;
                    for (size_t i = 0; i < n; ++i) {
                        if (contained_by[i].size() % 2 == 0) {
                            boolean::Polygon_with_holes_2 pwh(ccw_polygons[i]);
                            for (size_t h = 0; h < n; ++h) {
                                if (contained_by[h].size() == contained_by[i].size() + 1) {
                                    bool is_contained = false;
                                    for (size_t parent : contained_by[h]) {
                                        if (parent == i) {
                                            is_contained = true;
                                            break;
                                        }
                                    }
                                    if (is_contained) {
                                        boolean::Polygon_2 hole = ccw_polygons[h];
                                        hole.reverse_orientation();
                                        pwh.add_hole(hole);
                                    }
                                }
                            }
                            gps.join(pwh);
                        }
                    }
                    
                    Geometry sliced_plane_local = boolean::Engine::gps_to_geometry(gps);
                    
                    // Map back to world-space first, then to in.tf local space
                    Matrix to_s_local = in.tf.inverse() * section_tf;
                    sliced_plane_local.apply_tf(to_s_local);
                    
                    // Merge into res_combined_local
                    int base = (int)res_combined_local.vertices.size();
                    for (const auto& v : sliced_plane_local.vertices) res_combined_local.vertices.push_back(v);
                    for (const auto& f : sliced_plane_local.faces) {
                        Geometry::Face nf;
                        for (const auto& l : f.loops) {
                            std::vector<int> nl;
                            for (int idx : l) nl.push_back(base + idx);
                            nf.loops.push_back(nl);
                        }
                        res_combined_local.faces.push_back(nf);
                    }
                }
            }

            if (!res_combined_local.vertices.empty()) {
                out.geometry = vfs->materialize<Geometry>(res_combined_local);
                out.add_tag("type", "surface");
                has_any_geometry = true;
            }
        }

        // 2. Recursively process children
        for (const auto& child : in.components) {
            Shape child_out;
            if (section_recursive(vfs, child, slicing_planes, child_out)) {
                out.components.push_back(child_out);
                has_any_geometry = true;
            }
        }

        return has_any_geometry;
    }

    static void execute(fs::VFSNode* vfs, const fs::Selector& fulfilling, const Shape& in, const std::vector<Shape>& planes) {
        std::vector<Matrix> slicing_planes;
        if (planes.empty()) {
            slicing_planes.push_back(Matrix::identity());
        } else {
            for (const auto& p_shape : planes) {
                collect_planes(p_shape, slicing_planes);
            }
        }

        Shape out;
        bool has_geometry = section_recursive(vfs, in, slicing_planes, out);
        
        // Tag top-level output: a group if it has components, otherwise a surface!
        if (!out.components.empty()) {
            out.add_tag("type", "group");
        } else {
            out.add_tag("type", "surface");
        }
        
        vfs->write(fulfilling.with_output("$out"), out);
    }

    static std::vector<std::string> argument_keys() { return {"$in", "planes"}; }
    static typename P::json schema() {
        return {
            {"path", "jot/section"},
            {"description", "Generates 2D cross-sections of the input shape at the specified planes (or local Z=0)."},
            {"inputs", {{"$in", {{"type", "jot:shape"}}}}},
            {"arguments", json::array({
                {{"name", "planes"}, {"type", "jot:shapes"}, {"default", nlohmann::json::array()}, {"description", "Shapes whose transforms define the sectioning planes."}}
            })},
            {"outputs", {{"$out", {{"type", "jot:shape"}}}}}
        };
    }
};

static void section_init(fs::VFSNode* vfs) {
    Processor::register_op<SectionOp<>, Shape, std::vector<Shape>>(vfs, "jot/section");
}

} // namespace geo
} // namespace jotcad
