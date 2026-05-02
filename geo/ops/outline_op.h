#pragma once
#include "protocols.h"
#include "processor.h"

namespace jotcad {
namespace geo {

template <typename P = JotVfsProtocol>
struct OutlineOp : P {
    static constexpr const char* path = "jot/outline";
    static Vector_3 compute_normal(const Geometry& geo, size_t fi) {
        const auto& face = geo.faces[fi];
        if (face.loops.empty() || face.loops[0].size() < 3) return Vector_3(0, 0, 0);
        const auto& loop = face.loops[0];
        Point_3 p0(geo.vertices[loop[0]].x, geo.vertices[loop[0]].y, geo.vertices[loop[0]].z);
        for (size_t i = 1; i + 1 < loop.size(); ++i) {
            Point_3 p1(geo.vertices[loop[i]].x, geo.vertices[loop[i]].y, geo.vertices[loop[i]].z);
            Point_3 p2(geo.vertices[loop[i + 1]].x, geo.vertices[loop[i + 1]].y, geo.vertices[loop[i + 1]].z);
            Vector_3 n = CGAL::cross_product(p1 - p0, p2 - p0);
            if (n.squared_length() > 0) return n;
        }
        return Vector_3(0, 0, 0);
    }

    static void execute(fs::VFSNode* vfs, const fs::Selector& fulfilling, const Shape& in) {
        if (!in.geometry.has_value()) { vfs->write(fulfilling.with_output("$out"), in); return; }
        Geometry geo = vfs->read<Geometry>(in.geometry.value());
        
        // 1. Compute normals for all faces
        std::vector<Vector_3> normals;
        normals.reserve(geo.faces.size());
        for (size_t i = 0; i < geo.faces.size(); ++i) {
            normals.push_back(compute_normal(geo, i));
        }

        // 2. Build edge map: (min_v, max_v) -> [face_indices]
        struct Edge {
            int a, b;
            bool operator<(const Edge& o) const { return a < o.a || (a == o.a && b < o.b); }
        };
        std::map<Edge, std::vector<size_t>> edge_map;
        for (size_t fi = 0; fi < geo.faces.size(); ++fi) {
            for (const auto& loop : geo.faces[fi].loops) {
                for (size_t i = 0; i < loop.size(); ++i) {
                    int v1 = loop[i];
                    int v2 = loop[(i + 1) % loop.size()];
                    if (v1 == v2) continue;
                    edge_map[{std::min(v1, v2), std::max(v1, v2)}].push_back(fi);
                }
            }
        }

        // 3. Filter segments: only keep boundary edges or sharp bends
        Geometry res;
        for (const auto& [edge, faces] : edge_map) {
            bool is_feature = false;
            if (faces.size() != 2) {
                is_feature = true; // Boundary or non-manifold
            } else {
                const auto& n1 = normals[faces[0]];
                const auto& n2 = normals[faces[1]];
                
                if (n1.squared_length() == 0 || n2.squared_length() == 0) {
                    is_feature = true; // Keep edges of degenerate faces just in case
                } else {
                    // Feature if NOT coplanar (same normal direction)
                    bool parallel = CGAL::cross_product(n1, n2).squared_length() == 0;
                    bool same_dir = (n1 * n2) > 0;
                    if (!parallel || !same_dir) {
                        is_feature = true;
                    }
                }
            }

            if (is_feature) {
                int base = (int)res.vertices.size();
                res.vertices.push_back(geo.vertices[edge.a]);
                res.vertices.push_back(geo.vertices[edge.b]);
                res.segments.push_back({base, base + 1});
            }
        }

        Shape out = in;
        out.geometry = vfs->materialize<Geometry>(res);
        vfs->write(fulfilling.with_output("$out"), out);
    }
    static std::vector<std::string> argument_keys() { return {"$in"}; }
    static typename P::json schema() {
        return {
            {"path", "jot/outline"},
            {"description", "Extracts the boundary edges of the input shape as line segments."},
            {"arguments", {{{"name", "$in"}, {"type", "jot:shape"}, {"description", "The shape to outline."}, {"affiliate", "$out"}}}},
            {"outputs", {{"$out", {{"type", "jot:shape"}, {"description", "The resulting wireframe/outline shape."}}}}}
        };
    }
};

static void outline_init(fs::VFSNode* vfs) {
    Processor::register_op<OutlineOp<>, Shape>(vfs, "jot/outline");
}

} // namespace geo
} // namespace jotcad
