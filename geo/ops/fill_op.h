#pragma once
#include "protocols.h"
#include "processor.h"
#include "matrix.h"
#include <CGAL/Arrangement_2.h>
#include <CGAL/Arr_segment_traits_2.h>
#include <CGAL/Arr_extended_dcel.h>
#include <map>
#include <queue>

namespace jotcad {
namespace geo {

template <typename P = JotVfsProtocol>
struct FillOp : P {
    static constexpr const char* path = "jot/fill";

    typedef CGAL::Arr_segment_traits_2<EK> Traits_2;
    typedef CGAL::Arr_extended_dcel<Traits_2, int, int, int> Dcel; // Vertex, Halfedge, Face info
    typedef CGAL::Arrangement_2<Traits_2, Dcel> Arrangement;
    typedef Arrangement::Face_handle Face_handle;
    typedef Arrangement::Halfedge_handle Halfedge_handle;

    static void execute(fs::VFSNode* vfs, const fs::Selector& fulfilling, const Shape& in, std::string rule, const Shape& plane_shape) {
        Geometry geo = P::read_shape_geo(vfs, in);
        geo.apply_tf(in.tf);

        // 1. Determine Projection Plane
        EK::Plane_3 plane(0, 0, 1, 0); // Default XY
        if (plane_shape.tags.value("type", "") == "plane") {
            Matrix tf = plane_shape.tf;
            Point_3 origin = tf.t.transform(Point_3(0, 0, 0));
            Vector_3 normal = tf.t.transform(Vector_3(0, 0, 1));
            plane = EK::Plane_3(origin, normal);
        } else {
            auto inferred = geo.find_plane();
            if (inferred) plane = *inferred;
        }

        // 2. Build Arrangement
        Arrangement arr;
        auto add_seg = [&](const Point_3& p1, const Point_3& p2) {
            if (p1 == p2) return;
            CGAL::insert(arr, EK::Segment_2(plane.to_2d(p1), plane.to_2d(p2)));
        };

        for (const auto& seg : geo.segments) {
            add_seg(Point_3(geo.vertices[seg[0]].x, geo.vertices[seg[0]].y, geo.vertices[seg[0]].z),
                    Point_3(geo.vertices[seg[1]].x, geo.vertices[seg[1]].y, geo.vertices[seg[1]].z));
        }
        
        for (const auto& face : geo.faces) {
            for (const auto& loop : face.loops) {
                for (size_t i = 0; i < loop.size(); ++i) {
                    add_seg(Point_3(geo.vertices[loop[i]].x, geo.vertices[loop[i]].y, geo.vertices[loop[i]].z),
                            Point_3(geo.vertices[loop[(i + 1) % loop.size()]].x, geo.vertices[loop[(i + 1) % loop.size()]].y, geo.vertices[loop[(i + 1) % loop.size()]].z));
                }
            }
        }

        // 3. Propagate Winding / Parity
        std::map<Face_handle, int> face_info;
        std::queue<Face_handle> q;
        
        for (auto f = arr.unbounded_faces_begin(); f != arr.unbounded_faces_end(); ++f) {
            face_info[f] = 0;
            q.push(f);
        }

        while (!q.empty()) {
            Face_handle curr = q.front(); q.pop();
            int curr_val = face_info[curr];

            auto process_ccb = [&](auto circ) {
                auto curr_edge = circ;
                do {
                    Face_handle next = curr_edge->twin()->face();
                    if (face_info.find(next) == face_info.end()) {
                        if (rule == "odd") {
                            face_info[next] = 1 - curr_val;
                        } else if (rule == "any" || rule == "pos" || rule == "neg") {
                            // Minimal winding estimation: +1 for outer boundaries
                            face_info[next] = curr_val + 1; 
                        } else {
                            face_info[next] = 1; // "all"
                        }
                        q.push(next);
                    }
                } while (++curr_edge != circ);
            };

            for (auto hole = curr->holes_begin(); hole != curr->holes_end(); ++hole) {
                process_ccb(*hole);
            }
            if (!curr->is_unbounded()) {
                process_ccb(curr->outer_ccb());
            }
        }

        // 4. Extract Faces
        Geometry out_geo;
        for (auto f = arr.faces_begin(); f != arr.faces_end(); ++f) {
            if (f->is_unbounded()) continue;
            
            bool fill = false;
            int val = face_info[f];
            if (rule == "odd") fill = (val % 2 != 0);
            else if (rule == "any") fill = (val != 0);
            else if (rule == "pos") fill = (val > 0);
            else if (rule == "neg") fill = (val < 0);
            else if (rule == "all") fill = true;

            if (fill) {
                Geometry::Face out_face;
                std::vector<int> loop;
                auto circ = f->outer_ccb();
                auto edge = circ;
                do {
                    Point_2 p2 = edge->source()->point();
                    Point_3 p3 = plane.to_3d(p2);
                    loop.push_back(out_geo.vertices.size());
                    out_geo.vertices.push_back({p3.x(), p3.y(), p3.z()});
                } while (++edge != circ);
                out_face.loops.push_back(loop);
                
                for (auto hole = f->holes_begin(); hole != f->holes_end(); ++hole) {
                    std::vector<int> h_loop;
                    auto h_circ = *hole;
                    auto h_edge = h_circ;
                    do {
                        Point_2 p2 = h_edge->source()->point();
                        Point_3 p3 = plane.to_3d(p2);
                        h_loop.push_back(out_geo.vertices.size());
                        out_geo.vertices.push_back({p3.x(), p3.y(), p3.z()});
                    } while (++h_edge != h_circ);
                    out_face.loops.push_back(h_loop);
                }
                out_geo.faces.push_back(out_face);
            }
        }

        Shape out = P::make_shape(vfs, out_geo, {{"type", "surface"}});
        vfs->write(fulfilling.with_output("$out"), out);
    }

    static std::vector<std::string> argument_keys() { return {"$in", "rule", "plane"}; }
    static typename P::json schema() {
        return {
            {"path", "jot/fill"},
            {"description", "Promotes 1D segments to 2D faces using arrangement-based filling rules."},
            {"inputs", {{"$in", {{"type", "jot:shape"}, {"description", "The shape to fill."}}}}},
            {"arguments", {
                {{"name", "rule"}, {"type", "jot:string"}, {"default", "odd"}},
                {{"name", "plane"}, {"type", "jot:shape"}, {"default", {{"tags", {{"type", "plane"}}}, {"tf", "1/1 0/1 0/1 0/1 0/1 1/1 0/1 0/1 0/1 0/1 1/1 0/1 0/1 0/1 0/1 1/1"}}}}
            }},
            {"outputs", {{"$out", {{"type", "jot:shape"}}}}}
        };
    }
};

static void fill_init(fs::VFSNode* vfs) {
    Processor::register_op<FillOp<>, Shape, std::string, Shape>(vfs, "jot/fill");
}

} // namespace geo
} // namespace jotcad
