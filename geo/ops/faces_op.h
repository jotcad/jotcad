#pragma once
#include "protocols.h"
#include "processor.h"
#include "matrix.h"
#include "boolean/engine.h"
#include <CGAL/General_polygon_set_2.h>
#include <CGAL/Gps_segment_traits_2.h>
#include <queue>
#include <set>

namespace jotcad {
namespace geo {

template <typename P = JotVfsProtocol>
struct AsFacesOp : P {
    static constexpr const char* path = "jot/asFaces";
    static void execute(fs::VFSNode* vfs, const fs::Selector& fulfilling, const Shape& in) {
        if (!in.geometry.has_value()) {
            vfs->write(fulfilling.with_output("$out"), in);
            return;
        }
        Geometry geo = vfs->read<Geometry>(in.geometry.value());
        Geometry res;
        res.vertices = geo.vertices;
        res.faces = geo.faces;
        
        Shape out;
        out.tf = in.tf;
        out.geometry = vfs->materialize<Geometry>(res);
        out.add_tag("type", "faces");
        vfs->write(fulfilling.with_output("$out"), out);
    }
    static std::vector<std::string> argument_keys() { return {"$in"}; }
    static typename P::json schema() {
        return {
            {"path", "jot/asFaces"},
            {"description", "Materializes the subject's faces into a single mesh shape."},
            {"arguments", {{{"name", "$in"}, {"type", "jot:shape"}, {"affiliate", "$out"}}}},
            {"outputs", {{"$out", {{"type", "jot:shape"}}}}}
        };
    }
};

template <typename P = JotVfsProtocol>
struct FacesOp : P {
    static constexpr const char* path = "jot/faces";

    typedef boolean::ExactMesh ExactMesh;
    typedef CGAL::Gps_segment_traits_2<EK> Gps_traits_2;
    typedef CGAL::General_polygon_set_2<Gps_traits_2> General_polygon_set_2;
    typedef CGAL::Polygon_2<EK> Polygon_2;
    typedef CGAL::Polygon_with_holes_2<EK> Polygon_with_holes_2;

    static void execute(fs::VFSNode* vfs, const fs::Selector& fulfilling, const Shape& in, bool proxy = true) {
        if (!in.geometry.has_value()) {
            vfs->write(fulfilling.with_output("$out"), in);
            return;
        }
        Geometry geo = vfs->read<Geometry>(in.geometry.value());
        ExactMesh mesh = boolean::Engine::geometry_to_mesh(geo);

        Shape out;
        out.tf = in.tf;
        out.add_tag("type", "faces");

        std::set<ExactMesh::Face_index> visited;
        for (auto f : mesh.faces()) {
            if (visited.count(f)) continue;

            std::vector<ExactMesh::Face_index> patch;
            std::queue<ExactMesh::Face_index> q;
            q.push(f);
            visited.insert(f);
            patch.push_back(f);

            auto h0 = mesh.halfedge(f);
            auto p0 = mesh.point(mesh.source(h0));
            auto p1 = mesh.point(mesh.target(h0));
            auto p2 = mesh.point(mesh.target(mesh.next(h0)));
            EK::Plane_3 patch_plane(p0, p1, p2);

            while (!q.empty()) {
                auto current = q.front();
                q.pop();
                for (auto edge : mesh.halfedges_around_face(mesh.halfedge(current))) {
                    auto opp = mesh.opposite(edge);
                    auto neighbor = mesh.face(opp);
                    if (neighbor == ExactMesh::null_face() || visited.count(neighbor)) continue;
                    bool coplanar = true;
                    for (auto v : mesh.vertices_around_face(mesh.halfedge(neighbor))) {
                        if (!patch_plane.has_on(mesh.point(v))) { coplanar = false; break; }
                    }
                    if (coplanar) {
                        visited.insert(neighbor);
                        patch.push_back(neighbor);
                        q.push(neighbor);
                    }
                }
            }

            Matrix project_tf = Matrix::lookAt(patch_plane.point(), patch_plane.orthogonal_vector());
            Matrix rehydrate_tf = project_tf.inverse();
            General_polygon_set_2 gps;
            for (auto pf : patch) {
                Polygon_2 poly;
                for (auto v : mesh.vertices_around_face(mesh.halfedge(pf))) {
                    auto lp = project_tf.transform(mesh.point(v));
                    poly.push_back(EK::Point_2(lp.x(), lp.y()));
                }
                if (poly.is_empty() || !poly.is_simple()) continue;
                if (poly.is_clockwise_oriented()) poly.reverse_orientation();
                gps.join(Polygon_with_holes_2(poly));
            }

            std::vector<Polygon_with_holes_2> merged_pwhs;
            gps.polygons_with_holes(std::back_inserter(merged_pwhs));

            for (const auto& pwh : merged_pwhs) {
                FT tx(0), ty(0), tz(0);
                int count = 0;
                std::vector<EK::Point_3> boundary_pts;
                for (auto it = pwh.outer_boundary().vertices_begin(); it != pwh.outer_boundary().vertices_end(); ++it) {
                    EK::Point_3 p3 = rehydrate_tf.transform(EK::Point_3(it->x(), it->y(), 0));
                    tx += p3.x(); ty += p3.y(); tz += p3.z();
                    boundary_pts.push_back(p3);
                    count++;
                }
                if (count < 3) continue;
                EK::Point_3 centroid(tx/FT(count), ty/FT(count), tz/FT(count));
                EK::Vector_3 X_dir = boundary_pts[1] - boundary_pts[0];
                EK::Vector_3 Z_dir = patch_plane.orthogonal_vector();
                EK::Vector_3 Y_dir = CGAL::cross_product(Z_dir, X_dir);

                double dx = std::sqrt(CGAL::to_double(X_dir.squared_length()));
                double dy = std::sqrt(CGAL::to_double(Y_dir.squared_length()));
                double dz = std::sqrt(CGAL::to_double(Z_dir.squared_length()));
                FT w(1000000);
                auto scale_v = [&](const EK::Vector_3& v, double len) {
                    double s = 1000000.0 / len;
                    return std::array<FT, 3>{FT(v.x() * s), FT(v.y() * s), FT(v.z() * s)};
                };
                auto nx = scale_v(X_dir, dx);
                auto ny = scale_v(Y_dir, dy);
                auto nz = scale_v(Z_dir, dz);

                Matrix m(Transformation(nx[0], ny[0], nz[0], centroid.x() * w,
                                        nx[1], ny[1], nz[1], centroid.y() * w,
                                        nx[2], ny[2], nz[2], centroid.z() * w, w));

                Shape f_shape;
                f_shape.tf = in.tf * m;
                f_shape.add_tag("type", "surface");

                if (proxy) {
                    Geometry f_geo;
                    Matrix inv_m = m.inverse();
                    auto add_loop = [&](const Polygon_2& poly) {
                        std::vector<int> l;
                        for (auto it = poly.vertices_begin(); it != poly.vertices_end(); ++it) {
                            EK::Point_3 p3 = rehydrate_tf.transform(EK::Point_3(it->x(), it->y(), 0));
                            EK::Point_3 lp = inv_m.transform(p3);
                            l.push_back((int)f_geo.vertices.size());
                            f_geo.vertices.push_back({lp.x(), lp.y(), lp.z()});
                        }
                        return l;
                    };
                    Geometry::Face local_face;
                    local_face.loops.push_back(add_loop(pwh.outer_boundary()));
                    for (auto hit = pwh.holes_begin(); hit != pwh.holes_end(); ++hit) local_face.loops.push_back(add_loop(*hit));
                    f_geo.faces.push_back(local_face);
                    f_shape.geometry = vfs->materialize<Geometry>(f_geo);
                }
                out.components.push_back(f_shape);
            }
        }
        vfs->write(fulfilling.with_output("$out"), out);
    }
    static std::vector<std::string> argument_keys() { return {"$in", "proxy"}; }
    static typename P::json schema() {
        return {
            {"path", "jot/faces"},
            {"description", "Extracts faces from a shape as individual oriented components. Merges contiguous coplanar patches."},
            {"arguments", {
                {{"name", "$in"}, {"type", "jot:shape"}, {"affiliate", "$out"}},
                {{"name", "proxy"}, {"type", "jot:boolean"}, {"default", true}}
            }},
            {"outputs", {{"$out", {{"type", "jot:shape"}}}}}
        };
    }
};

static void faces_init(fs::VFSNode* vfs) {
    Processor::register_op<AsFacesOp<>, Shape>(vfs, "jot/asFaces");
    Processor::register_op<FacesOp<>, Shape, bool>(vfs, "jot/faces");
}

} // namespace geo
} // namespace jotcad
