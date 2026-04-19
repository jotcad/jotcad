#pragma once
#include "impl/protocols.h"
#include "impl/processor.h"
#include "impl/matrix.h"

namespace jotcad {
namespace geo {

template <typename P = JotVfsProtocol>
struct CornersOp : P {
    static constexpr const char* path = "jot/corners";

    static void execute(fs::VFSNode* vfs, const fs::Selector& fulfilling, const Shape& in, bool proxy) {
        if (!in.geometry.has_value()) {
            vfs->write<Shape>(fulfilling, in);
            return;
        }

        // 1. Read input geometry
        Geometry geo = vfs->read<Geometry>(in.geometry.value());

        std::vector<Shape> components;
        
        for (const auto& face : geo.faces) {
            for (const auto& loop : face.loops) {
                if (loop.size() < 2) continue;
                for (size_t i = 0; i < loop.size(); ++i) {
                    int idx_curr = loop[i];
                    int idx_next = loop[(i + 1) % loop.size()];
                    int idx_prev = loop[(i + loop.size() - 1) % loop.size()];

                    auto v_curr = geo.vertices[idx_curr];
                    auto v_next = geo.vertices[idx_next];
                    auto v_prev = geo.vertices[idx_prev];

                    FT dx = v_next.x - v_curr.x;
                    FT dy = v_next.y - v_curr.y;
                    FT dz = v_next.z - v_curr.z;
                    FT sql_next = dx*dx + dy*dy + dz*dz;
                    
                    if (sql_next < 1e-18) continue;
                    FT len_next = FT(std::sqrt(CGAL::to_double(sql_next)));

                    FT ux = dx / len_next;
                    FT uy = dy / len_next;
                    FT uz = dz / len_next;

                    FT nx = 0, ny = 0, nz = 1; 
                    FT vx = ny * uz - nz * uy;
                    FT vy = nz * ux - nx * uz;
                    FT vz = nx * uy - ny * ux;

                    Matrix m(Transformation(ux, vx, nx, v_curr.x,
                                            uy, vy, ny, v_curr.y,
                                            uz, vz, nz, v_curr.z));

                    Shape corner;
                    corner.tf = m.to_vec();
                    corner.add_tag("type", "corner");
                    corner.add_tag("index", (int)i);

                    if (proxy) {
                        Geometry v_geo;
                        v_geo.vertices.push_back({FT(0), FT(0), FT(0)}); 
                        v_geo.vertices.push_back({len_next, FT(0), FT(0)}); 
                        
                        FT p_dx = v_prev.x - v_curr.x;
                        FT p_dy = v_prev.y - v_curr.y;
                        FT p_dz = v_prev.z - v_curr.z;
                        
                        Point_3 local_p = m.inverse().t.transform(Point_3(p_dx, p_dy, p_dz));
                        v_geo.vertices.push_back({local_p.x(), local_p.y(), local_p.z()});

                        v_geo.segments.push_back({0, 1});
                        v_geo.segments.push_back({0, 2});
                        corner.geometry = vfs->write<Geometry>(v_geo);
                    }
                    components.push_back(corner);
                }
            }
        }

        Shape out;
        out.geometry = std::nullopt;
        out.components = components;
        out.add_tag("type", "corners");
        vfs->write<Shape>(fulfilling, out);
    }

    static std::vector<std::string> argument_keys() { return {"$in", "proxy"}; }

    static typename P::json schema() {
        return {
            {"path", "jot/corners"},
            {"description", "Extracts the corners (vertices) of a shape as a group of coordinate frames oriented along the edges."},
            {"arguments", {
                {"$in", {{"type", "jot:shape"}}},
                {"proxy", {{"type", "boolean"}, {"default", true}}}
            }}
        };
    }
};

static void corners_init() {
    Processor::register_op<CornersOp<>, Shape, bool>("jot/corners");
}

} // namespace geo
} // namespace jotcad
