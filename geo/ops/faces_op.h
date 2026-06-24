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
        res.triangulate();
        out.geometry = vfs->materialize<Geometry>(res);
        out.add_tag("type", "faces");
        vfs->write(fulfilling.with_output("$out"), out);
    }
    static std::vector<std::string> argument_keys() { return {"$in"}; }
    static typename P::json schema() {
        return {
            {"path", "jot/asFaces"},
            {"description", "Materializes the subject's faces into a single mesh shape."},
            {"inputs", {{"$in", {{"type", "jot:shape"}}}}},
            {"arguments", nlohmann::json::array()},
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
            if (!in.components.empty()) {
                Shape out = in;
                out.components.clear();
                out.add_tag("type", "faces");
                bool has_non_gaps = false;
                for (const auto& child : in.components) {
                    if (!child.is_gap()) {
                        has_non_gaps = true;
                        break;
                    }
                }
                for (const auto& child : in.components) {
                    if (has_non_gaps && child.is_gap()) continue;
                    fs::Selector faces_sel = fs::Selector("jot/faces", {{"$in", vfs->materialize(child).value}, {"proxy", proxy}}).with_output("$out");
                    Shape child_faces = vfs->read<Shape>(faces_sel);
                    for (const auto& f : child_faces.components) {
                        out.components.push_back(f);
                    }
                }
                vfs->write(fulfilling.with_output("$out"), out);
                return;
            }
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
            std::vector<EK::Point_3> pts;
            for (auto v : mesh.vertices_around_face(h0)) {
                pts.push_back(mesh.point(v));
            }
            
            EK::Plane_3 patch_plane;
            bool found_plane = false;
            if (pts.size() >= 3) {
                for (size_t i = 2; i < pts.size(); ++i) {
                    if (!CGAL::collinear(pts[0], pts[1], pts[i])) {
                        patch_plane = EK::Plane_3(pts[0], pts[1], pts[i]);
                        found_plane = true;
                        break;
                    }
                }
            }
            if (!found_plane) {
                continue; // Skip degenerate face
            }

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

            auto simplify_3d = [](std::vector<EK::Point_3>& pts) {
                if (pts.empty()) return;
                std::vector<EK::Point_3> unique_pts;
                for (const auto& p : pts) {
                    if (unique_pts.empty() || unique_pts.back() != p) {
                        unique_pts.push_back(p);
                    }
                }
                while (unique_pts.size() > 1 && unique_pts.front() == unique_pts.back()) {
                    unique_pts.pop_back();
                }
                if (unique_pts.size() < 3) {
                    pts = std::move(unique_pts);
                    return;
                }
                std::vector<EK::Point_3> next;
                size_t n = unique_pts.size();
                for (size_t i = 0; i < n; ++i) {
                    if (!CGAL::collinear(unique_pts[(i + n - 1) % n], unique_pts[i], unique_pts[(i + 1) % n])) {
                        next.push_back(unique_pts[i]);
                    }
                }
                pts = std::move(next);
            };

            for (const auto& pwh : merged_pwhs) {
                std::vector<EK::Point_3> boundary_pts;
                for (auto it = pwh.outer_boundary().vertices_begin(); it != pwh.outer_boundary().vertices_end(); ++it) {
                    boundary_pts.push_back(rehydrate_tf.transform(EK::Point_3(it->x(), it->y(), 0)));
                }
                simplify_3d(boundary_pts);
                int count = (int)boundary_pts.size();
                if (count < 3) continue;

                FT tx(0), ty(0), tz(0);
                for (const auto& p3 : boundary_pts) {
                    tx += p3.x(); ty += p3.y(); tz += p3.z();
                }

                EK::Point_3 centroid(tx/FT(count), ty/FT(count), tz/FT(count));
                
                EK::Vector_3 X_dir;
                bool found_x = false;
                for (size_t i = 1; i < boundary_pts.size(); ++i) {
                    EK::Vector_3 candidate = boundary_pts[i] - boundary_pts[0];
                    if (candidate.squared_length() > 0) {
                        X_dir = candidate;
                        found_x = true;
                        break;
                    }
                }
                if (!found_x) continue;

                EK::Vector_3 Z_dir = patch_plane.orthogonal_vector();
                EK::Vector_3 Y_dir = CGAL::cross_product(Z_dir, X_dir);

                double dx = std::sqrt(CGAL::to_double(X_dir.squared_length()));
                double dy = std::sqrt(CGAL::to_double(Y_dir.squared_length()));
                double dz = std::sqrt(CGAL::to_double(Z_dir.squared_length()));

                if (dx <= 1e-9 || dy <= 1e-9 || dz <= 1e-9) continue;

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
                    auto add_loop_3d = [&](const std::vector<EK::Point_3>& pts) {
                        std::vector<int> l;
                        for (const auto& p3 : pts) {
                            EK::Point_3 lp = inv_m.transform(p3);
                            l.push_back((int)f_geo.vertices.size());
                            f_geo.vertices.push_back({lp.x(), lp.y(), lp.z()});
                        }
                        return l;
                    };
                    Geometry::Face local_face;
                    local_face.loops.push_back(add_loop_3d(boundary_pts));
                    for (auto hit = pwh.holes_begin(); hit != pwh.holes_end(); ++hit) {
                        std::vector<EK::Point_3> hole_pts;
                        for (auto it = hit->vertices_begin(); it != hit->vertices_end(); ++it) {
                            hole_pts.push_back(rehydrate_tf.transform(EK::Point_3(it->x(), it->y(), 0)));
                        }
                        simplify_3d(hole_pts);
                        if (hole_pts.size() >= 3) {
                            local_face.loops.push_back(add_loop_3d(hole_pts));
                        }
                    }
                    f_geo.faces.push_back(local_face);
                    f_geo.triangulate();
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
            {"inputs", {{"$in", {{"type", "jot:shape"}}}}},
            {"arguments", nlohmann::json::array({
                {{"name", "proxy"}, {"type", "jot:boolean"}, {"default", true}}
            })},
            {"outputs", {{"$out", {{"type", "jot:shape"}}}}}
        };
    }
};

template <typename P = JotVfsProtocol>
struct TopOp : P {
    static constexpr const char* path = "jot/top";
    static void execute(fs::VFSNode* vfs, const fs::Selector& fulfilling, const Shape& in) {
        fs::Selector faces_sel = fs::Selector("jot/faces", {{"$in", vfs->materialize(in).value}}).with_output("$out");
        fs::Selector highest_sel("jot/highest", {
            {"$in", faces_sel},
            {"measure", fs::Selector("jot/facing", {{"vec", std::vector<double>{0.0, 0.0, 1.0}}})},
            {"bucket", 0}
        });
        Shape res = vfs->read<Shape>(highest_sel.with_output("$out"));
        if (res.components.empty()) {
            vfs->write(fulfilling.with_output("$out"), res);
            return;
        }
        size_t best_idx = 0;
        double best_val = CGAL::to_double(res.components[0].tf.t.cartesian(2, 3));
        for (size_t i = 1; i < res.components.size(); ++i) {
            double val = CGAL::to_double(res.components[i].tf.t.cartesian(2, 3));
            if (val > best_val) {
                best_val = val;
                best_idx = i;
            }
        }
        vfs->write(fulfilling.with_output("$out"), res.components[best_idx]);
    }
    static std::vector<std::string> argument_keys() { return {"$in"}; }
    static typename P::json schema() {
        return {
            {"path", "jot/top"},
            {"description", "Returns the top-most face anchor of the subject."},
            {"inputs", {{"$in", {{"type", "jot:shape"}}}}},
            {"outputs", {{"$out", {{"type", "jot:shape"}}}}}
        };
    }
};

template <typename P = JotVfsProtocol>
struct BottomOp : P {
    static constexpr const char* path = "jot/bottom";
    static void execute(fs::VFSNode* vfs, const fs::Selector& fulfilling, const Shape& in) {
        fs::Selector faces_sel = fs::Selector("jot/faces", {{"$in", vfs->materialize(in).value}}).with_output("$out");
        fs::Selector highest_sel("jot/highest", {
            {"$in", faces_sel},
            {"measure", fs::Selector("jot/facing", {{"vec", std::vector<double>{0.0, 0.0, -1.0}}})},
            {"bucket", 0}
        });
        Shape res = vfs->read<Shape>(highest_sel.with_output("$out"));
        if (res.components.empty()) {
            vfs->write(fulfilling.with_output("$out"), res);
            return;
        }
        size_t best_idx = 0;
        double best_val = CGAL::to_double(res.components[0].tf.t.cartesian(2, 3));
        for (size_t i = 1; i < res.components.size(); ++i) {
            double val = CGAL::to_double(res.components[i].tf.t.cartesian(2, 3));
            if (val < best_val) {
                best_val = val;
                best_idx = i;
            }
        }
        vfs->write(fulfilling.with_output("$out"), res.components[best_idx]);
    }
    static std::vector<std::string> argument_keys() { return {"$in"}; }
    static typename P::json schema() {
        return {
            {"path", "jot/bottom"},
            {"description", "Returns the bottom-most face anchor of the subject."},
            {"inputs", {{"$in", {{"type", "jot:shape"}}}}},
            {"outputs", {{"$out", {{"type", "jot:shape"}}}}}
        };
    }
};

template <typename P = JotVfsProtocol>
struct LeftOp : P {
    static constexpr const char* path = "jot/left";
    static void execute(fs::VFSNode* vfs, const fs::Selector& fulfilling, const Shape& in) {
        fs::Selector faces_sel = fs::Selector("jot/faces", {{"$in", vfs->materialize(in).value}}).with_output("$out");
        fs::Selector highest_sel("jot/highest", {
            {"$in", faces_sel},
            {"measure", fs::Selector("jot/facing", {{"vec", std::vector<double>{-1.0, 0.0, 0.0}}})},
            {"bucket", 0}
        });
        Shape res = vfs->read<Shape>(highest_sel.with_output("$out"));
        if (res.components.empty()) {
            vfs->write(fulfilling.with_output("$out"), res);
            return;
        }
        size_t best_idx = 0;
        double best_val = CGAL::to_double(res.components[0].tf.t.cartesian(0, 3));
        for (size_t i = 1; i < res.components.size(); ++i) {
            double val = CGAL::to_double(res.components[i].tf.t.cartesian(0, 3));
            if (val < best_val) {
                best_val = val;
                best_idx = i;
            }
        }
        vfs->write(fulfilling.with_output("$out"), res.components[best_idx]);
    }
    static std::vector<std::string> argument_keys() { return {"$in"}; }
    static typename P::json schema() {
        return {
            {"path", "jot/left"},
            {"description", "Returns the left-most face anchor of the subject (-X direction)."},
            {"inputs", {{"$in", {{"type", "jot:shape"}}}}},
            {"outputs", {{"$out", {{"type", "jot:shape"}}}}}
        };
    }
};

template <typename P = JotVfsProtocol>
struct RightOp : P {
    static constexpr const char* path = "jot/right";
    static void execute(fs::VFSNode* vfs, const fs::Selector& fulfilling, const Shape& in) {
        fs::Selector faces_sel = fs::Selector("jot/faces", {{"$in", vfs->materialize(in).value}}).with_output("$out");
        fs::Selector highest_sel("jot/highest", {
            {"$in", faces_sel},
            {"measure", fs::Selector("jot/facing", {{"vec", std::vector<double>{1.0, 0.0, 0.0}}})},
            {"bucket", 0}
        });
        Shape res = vfs->read<Shape>(highest_sel.with_output("$out"));
        if (res.components.empty()) {
            vfs->write(fulfilling.with_output("$out"), res);
            return;
        }
        size_t best_idx = 0;
        double best_val = CGAL::to_double(res.components[0].tf.t.cartesian(0, 3));
        for (size_t i = 1; i < res.components.size(); ++i) {
            double val = CGAL::to_double(res.components[i].tf.t.cartesian(0, 3));
            if (val > best_val) {
                best_val = val;
                best_idx = i;
            }
        }
        vfs->write(fulfilling.with_output("$out"), res.components[best_idx]);
    }
    static std::vector<std::string> argument_keys() { return {"$in"}; }
    static typename P::json schema() {
        return {
            {"path", "jot/right"},
            {"description", "Returns the right-most face anchor of the subject (+X direction)."},
            {"inputs", {{"$in", {{"type", "jot:shape"}}}}},
            {"outputs", {{"$out", {{"type", "jot:shape"}}}}}
        };
    }
};

template <typename P = JotVfsProtocol>
struct FrontOp : P {
    static constexpr const char* path = "jot/front";
    static void execute(fs::VFSNode* vfs, const fs::Selector& fulfilling, const Shape& in) {
        fs::Selector faces_sel = fs::Selector("jot/faces", {{"$in", vfs->materialize(in).value}}).with_output("$out");
        fs::Selector highest_sel("jot/highest", {
            {"$in", faces_sel},
            {"measure", fs::Selector("jot/facing", {{"vec", std::vector<double>{0.0, -1.0, 0.0}}})},
            {"bucket", 0}
        });
        Shape res = vfs->read<Shape>(highest_sel.with_output("$out"));
        if (res.components.empty()) {
            vfs->write(fulfilling.with_output("$out"), res);
            return;
        }
        size_t best_idx = 0;
        double best_val = CGAL::to_double(res.components[0].tf.t.cartesian(1, 3));
        for (size_t i = 1; i < res.components.size(); ++i) {
            double val = CGAL::to_double(res.components[i].tf.t.cartesian(1, 3));
            if (val < best_val) {
                best_val = val;
                best_idx = i;
            }
        }
        vfs->write(fulfilling.with_output("$out"), res.components[best_idx]);
    }
    static std::vector<std::string> argument_keys() { return {"$in"}; }
    static typename P::json schema() {
        return {
            {"path", "jot/front"},
            {"description", "Returns the front-most face anchor of the subject (-Y direction)."},
            {"inputs", {{"$in", {{"type", "jot:shape"}}}}},
            {"outputs", {{"$out", {{"type", "jot:shape"}}}}}
        };
    }
};

template <typename P = JotVfsProtocol>
struct BackOp : P {
    static constexpr const char* path = "jot/back";
    static void execute(fs::VFSNode* vfs, const fs::Selector& fulfilling, const Shape& in) {
        fs::Selector faces_sel = fs::Selector("jot/faces", {{"$in", vfs->materialize(in).value}}).with_output("$out");
        fs::Selector highest_sel("jot/highest", {
            {"$in", faces_sel},
            {"measure", fs::Selector("jot/facing", {{"vec", std::vector<double>{0.0, 1.0, 0.0}}})},
            {"bucket", 0}
        });
        Shape res = vfs->read<Shape>(highest_sel.with_output("$out"));
        if (res.components.empty()) {
            vfs->write(fulfilling.with_output("$out"), res);
            return;
        }
        size_t best_idx = 0;
        double best_val = CGAL::to_double(res.components[0].tf.t.cartesian(1, 3));
        for (size_t i = 1; i < res.components.size(); ++i) {
            double val = CGAL::to_double(res.components[i].tf.t.cartesian(1, 3));
            if (val > best_val) {
                best_val = val;
                best_idx = i;
            }
        }
        vfs->write(fulfilling.with_output("$out"), res.components[best_idx]);
    }
    static std::vector<std::string> argument_keys() { return {"$in"}; }
    static typename P::json schema() {
        return {
            {"path", "jot/back"},
            {"description", "Returns the back-most face anchor of the subject (+Y direction)."},
            {"inputs", {{"$in", {{"type", "jot:shape"}}}}},
            {"outputs", {{"$out", {{"type", "jot:shape"}}}}}
        };
    }
};

static void faces_init(fs::VFSNode* vfs) {
    Processor::register_op<AsFacesOp<>, Shape>(vfs, "jot/asFaces");
    Processor::register_op<FacesOp<>, Shape, bool>(vfs, "jot/faces");
    Processor::register_op<TopOp<>, Shape>(vfs, "jot/top");
    Processor::register_op<BottomOp<>, Shape>(vfs, "jot/bottom");
    Processor::register_op<LeftOp<>, Shape>(vfs, "jot/left");
    Processor::register_op<RightOp<>, Shape>(vfs, "jot/right");
    Processor::register_op<FrontOp<>, Shape>(vfs, "jot/front");
    Processor::register_op<BackOp<>, Shape>(vfs, "jot/back");
}

} // namespace geo
} // namespace jotcad
