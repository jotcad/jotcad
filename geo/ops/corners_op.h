#pragma once
#include "protocols.h"
#include "processor.h"
#include "matrix.h"

namespace jotcad {
namespace geo {

// Exact-ish rational normalization for stability in boolean ops.
// Uses a rational approximation of the inverse square root.
static std::tuple<FT, FT, FT> get_rational_unit(FT x, FT y, FT z, FT sql) {
    if (sql == 0) return std::make_tuple(FT(0), FT(0), FT(0));
    double dlen = std::sqrt(CGAL::to_double(sql));
    // Represent the inverse length as a high-precision rational.
    // 10^10 is usually enough to avoid most boolean epsilon issues.
    FT inv_len = FT((long long)(1.0 / dlen * 10000000000LL)) / FT(10000000000LL);
    return std::make_tuple(x * inv_len, y * inv_len, z * inv_len);
}

template <typename P = JotVfsProtocol>
struct CornersOp : P {
    static constexpr const char* path = "jot/corners";

    static void execute(fs::VFSNode* vfs, const fs::Selector& fulfilling, const Shape& in) {
        if (!in.geometry.has_value()) {
            vfs->write(fulfilling.with_output("$out"), in);
            return;
        }

        // 1. Read input geometry
        Geometry geo = vfs->read<Geometry>(in.geometry.value());
        Geometry res;
        res.vertices = geo.vertices;
        res.segments = geo.segments;

        Shape out = in;
        out.geometry = vfs->materialize<Geometry>(res);
        out.add_tag("type", "corners");
        
        // Final fulfillment
        vfs->write(fulfilling.with_output("$out"), out);
    }

    static std::vector<std::string> argument_keys() { return {"$in"}; }

    static typename P::json schema() {
        return {
            {"path", "jot/corners"},
            {"description", "Extracts vertices from a shape as a point cloud oriented for joinery."},
            {"arguments", {
                {{"name", "$in"}, {"type", "jot:shape"}, {"affiliate", "$out"}}
            }},
            {"outputs", {{"$out", {{"type", "jot:shape"}}}}}
        };
    }
};

template <typename P = JotVfsProtocol>
struct EachCornerOp : P {
    static constexpr const char* path = "jot/eachCorner";

    static void execute(fs::VFSNode* vfs, const fs::Selector& fulfilling, const Shape& in, bool proxy = true) {
        if (!in.geometry.has_value()) {
            vfs->write(fulfilling.with_output("$out"), in);
            return;
        }

        // 1. Read input geometry
        Geometry geo = vfs->read<Geometry>(in.geometry.value());

        Shape out;
        out.add_tag("type", "corners");

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

                    FT dx_next = v_next.x - v_curr.x;
                    FT dy_next = v_next.y - v_curr.y;
                    FT dz_next = v_next.z - v_curr.z;
                    FT sql_next = dx_next*dx_next + dy_next*dy_next + dz_next*dz_next;

                    FT dx_prev = v_prev.x - v_curr.x;
                    FT dy_prev = v_prev.y - v_curr.y;
                    FT dz_prev = v_prev.z - v_curr.z;
                    FT sql_prev = dx_prev*dx_prev + dy_prev*dy_prev + dz_prev*dz_prev;

                    if (sql_next == 0 || sql_prev == 0) continue;

                    auto [unx, uny, unz] = get_rational_unit(dx_next, dy_next, dz_next, sql_next);
                    auto [upx, upy, upz] = get_rational_unit(dx_prev, dy_prev, dz_prev, sql_prev);

                    // Bisector vector (un-normalized)
                    FT bx = unx + upx;
                    FT by = uny + upy;
                    FT bz = unz + upz;
                    FT blen_sq = bx*bx + by*by + bz*bz;

                    FT ux, uy, uz;
                    if (blen_sq < FT(1) / FT(1000000000000LL)) {
                        // Collinear (180 deg) - use perpendicular to next (inwards for CCW)
                        ux = -uny;
                        uy = unx;
                        uz = unz;
                    } else {
                        // Normalize the bisector to make it the X-axis
                        auto [nbx, nby, nbz] = get_rational_unit(bx, by, bz, blen_sq);
                        ux = nbx; uy = nby; uz = nbz;
                    }

                    FT nx = 0, ny = 0, nz = 1; 
                    FT vx = ny * uz - nz * uy;
                    FT vy = nz * ux - nx * uz;
                    FT vz = nx * uy - ny * ux;

                    Matrix m(Transformation(ux, vx, nx, v_curr.x,
                                            uy, vy, ny, v_curr.y,
                                            uz, vz, nz, v_curr.z));

                    Shape corner;
                    corner.tf = m;
                    corner.add_tag("type", "corner");
                    corner.add_tag("index", (int)i);

                    if (proxy) {
                        Geometry v_geo;
                        v_geo.vertices.push_back({FT(0), FT(0), FT(0)}); 

                        // Edge to next
                        Point_3 local_next = m.inverse().t.transform(Point_3(v_next.x, v_next.y, v_next.z));
                        v_geo.vertices.push_back({local_next.x(), local_next.y(), local_next.z()});

                        // Edge to prev
                        Point_3 local_prev = m.inverse().t.transform(Point_3(v_prev.x, v_prev.y, v_prev.z));
                        v_geo.vertices.push_back({local_prev.x(), local_prev.y(), local_prev.z()});

                        v_geo.segments.push_back({0, 1});
                        v_geo.segments.push_back({0, 2});
                        corner.geometry = vfs->materialize<Geometry>(v_geo);
                    }
                    out.components.push_back(corner);
                }
            }
        }

        vfs->write(fulfilling.with_output("$out"), out);
    }

    static std::vector<std::string> argument_keys() { return {"$in", "proxy"}; }

    static typename P::json schema() {
        return {
            {"path", "jot/eachCorner"},
            {"description", "Extracts vertices from a shape as individual oriented child components."},
            {"arguments", {
                {{"name", "$in"}, {"type", "jot:shape"}, {"affiliate", "$out"}},
                {{"name", "proxy"}, {"type", "jot:boolean"}, {"default", true}}
            }},
            {"outputs", {{"$out", {{"type", "jot:shape"}}}}}
        };
    }
};

template <typename P = JotVfsProtocol>
struct EdgesOp : P {
    static constexpr const char* path = "jot/edges";

    static void execute(fs::VFSNode* vfs, const fs::Selector& fulfilling, const Shape& in) {
        if (!in.geometry.has_value()) {
            vfs->write(fulfilling.with_output("$out"), in);
            return;
        }

        Geometry geo = vfs->read<Geometry>(in.geometry.value());
        Geometry res;
        res.vertices = geo.vertices;
        res.segments = geo.segments;

        Shape out = in;
        out.geometry = vfs->materialize<Geometry>(res);
        out.add_tag("type", "edges");
        
        vfs->write(fulfilling.with_output("$out"), out);
    }

    static std::vector<std::string> argument_keys() { return {"$in"}; }

    static typename P::json schema() {
        return {
            {"path", "jot/edges"},
            {"description", "Extracts edges from a shape as a point cloud oriented for joinery."},
            {"arguments", {
                {{"name", "$in"}, {"type", "jot:shape"}, {"affiliate", "$out"}}
            }},
            {"outputs", {{"$out", {{"type", "jot:shape"}}}}}
        };
    }
};

template <typename P = JotVfsProtocol>
struct EachEdgeOp : P {
    static constexpr const char* path = "jot/eachEdge";

    static void execute(fs::VFSNode* vfs, const fs::Selector& fulfilling, const Shape& in, bool proxy = true) {
        if (!in.geometry.has_value()) {
            vfs->write(fulfilling.with_output("$out"), in);
            return;
        }

        Geometry geo = vfs->read<Geometry>(in.geometry.value());

        Shape out;
        out.add_tag("type", "edges");

        for (const auto& face : geo.faces) {
            for (const auto& loop : face.loops) {
                if (loop.size() < 2) continue;
                for (size_t i = 0; i < loop.size(); ++i) {
                    int idx_curr = loop[i];
                    int idx_next = loop[(i + 1) % loop.size()];

                    auto v_curr = geo.vertices[idx_curr];
                    auto v_next = geo.vertices[idx_next];

                    FT dx = v_next.x - v_curr.x;
                    FT dy = v_next.y - v_curr.y;
                    FT dz = v_next.z - v_curr.z;
                    FT sql = dx*dx + dy*dy + dz*dz;

                    if (sql == 0) continue;
                    
                    // Midpoint
                    FT mx = (v_curr.x + v_next.x) / FT(2);
                    FT my = (v_curr.y + v_next.y) / FT(2);
                    FT mz = (v_curr.z + v_next.z) / FT(2);

                    // Edge vector (normalized) as Y-axis
                    auto [uy, vy, wy] = get_rational_unit(dx, dy, dz, sql);
                    
                    // Length (near-exact)
                    double dlen = std::sqrt(CGAL::to_double(sql));
                    FT len = FT((long long)(dlen * 10000000000LL)) / FT(10000000000LL);

                    // Normal (assumed Z for now)
                    FT nx = 0, ny = 0, nz = 1;

                    // X-axis: N x Y = (0,0,1) x (uy, vy, wy) = (-vy, uy, 0)
                    FT ux = ny * wy - nz * vy;
                    FT vx = nz * uy - nx * wy;
                    FT wx = nx * vy - ny * uy;

                    Matrix m(Transformation(ux, uy, nx, mx,
                                            vx, vy, ny, my,
                                            wx, wy, nz, mz));

                    Shape edge;
                    edge.tf = m;
                    edge.add_tag("type", "edge");
                    edge.add_tag("index", (int)i);

                    if (proxy) {
                        Geometry v_geo;
                        // Segment along Y axis in local space, centered at origin
                        v_geo.vertices.push_back({FT(0), -len/FT(2), FT(0)}); 
                        v_geo.vertices.push_back({FT(0), len/FT(2), FT(0)}); 
                        v_geo.segments.push_back({0, 1});
                        edge.geometry = vfs->materialize<Geometry>(v_geo);
                    }
                    out.components.push_back(edge);
                }
            }
        }

        vfs->write(fulfilling.with_output("$out"), out);
    }

    static std::vector<std::string> argument_keys() { return {"$in", "proxy"}; }

    static typename P::json schema() {
        return {
            {"path", "jot/eachEdge"},
            {"description", "Extracts edges from a shape as individual oriented child components centered at midpoints."},
            {"arguments", {
                {{"name", "$in"}, {"type", "jot:shape"}, {"affiliate", "$out"}},
                {{"name", "proxy"}, {"type", "jot:boolean"}, {"default", true}}
            }},
            {"outputs", {{"$out", {{"type", "jot:shape"}}}}}
        };
    }
};

static void corners_init(fs::VFSNode* vfs) {
    Processor::register_op<CornersOp<>, Shape>(vfs, "jot/corners");
    Processor::register_op<EachCornerOp<>, Shape, bool>(vfs, "jot/eachCorner");
    Processor::register_op<EdgesOp<>, Shape>(vfs, "jot/edges");
    Processor::register_op<EachEdgeOp<>, Shape, bool>(vfs, "jot/eachEdge");
}

} // namespace geo
} // namespace jotcad
