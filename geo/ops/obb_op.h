#pragma once
#include "protocols.h"
#include "processor.h"
#include "box_op.h"
#include <CGAL/Optimal_bounding_box/oriented_bounding_box.h>

namespace jotcad {
namespace geo {

template <typename P = JotVfsProtocol>
struct OrientedBoundingBoxOp : P {
    static constexpr const char* path = "jot/orientedBoundingBox";

    static void execute(fs::VFSNode* vfs, const fs::Selector& fulfilling, const Shape& in, double grow) {
        std::vector<Point_3> points;

        std::vector<const Shape*> stack = {&in};
        while (!stack.empty()) {
            const Shape* curr = stack.back();
            stack.pop_back();

            if (curr->geometry.has_value()) {
                Geometry geo = vfs->read<Geometry>(*curr->geometry);
                for (const auto& v : geo.vertices) {
                    points.push_back(curr->tf.transform(Point_3(v.x, v.y, v.z)));
                }
            } 
            
            for (const auto& child : curr->components) {
                stack.push_back(&child);
            }
        }

        if (points.empty()) {
            points.push_back(in.tf.transform(Point_3(0, 0, 0)));
        }

        typedef CGAL::Simple_cartesian<double> K_double;
        std::vector<K_double::Point_3> points_double;
        for (const auto& p : points) {
            points_double.emplace_back(CGAL::to_double(p.x()), CGAL::to_double(p.y()), CGAL::to_double(p.z()));
        }

        std::array<K_double::Point_3, 8> obb_vertices_double;
        CGAL::oriented_bounding_box(points_double, obb_vertices_double);

        std::array<Point_3, 8> obb_vertices;
        for (int i = 0; i < 8; ++i) {
            obb_vertices[i] = Point_3(FT(obb_vertices_double[i].x()), FT(obb_vertices_double[i].y()), FT(obb_vertices_double[i].z()));
        }

        // The order of points in OBB follows a specific hexahedron layout:
        // v0-v1 is X, v0-v3 is Y, v0-v5 is Z.
        Point_3 v0 = obb_vertices[0];
        Point_3 v1 = obb_vertices[1];
        Point_3 v3 = obb_vertices[3];
        Point_3 v5 = obb_vertices[5];

        Vector_3 vx = v1 - v0;
        Vector_3 vy = v3 - v0;
        Vector_3 vz = v5 - v0;

        double w = std::sqrt(CGAL::to_double(vx.squared_length()));
        double d = std::sqrt(CGAL::to_double(vy.squared_length()));
        double h = std::sqrt(CGAL::to_double(vz.squared_length()));

        // Handle grow by shifting origin and increasing dimensions
        if (grow != 0.0) {
            Vector_3 ux = (w > 1e-9) ? vx / FT(w) : Vector_3(1, 0, 0);
            Vector_3 uy = (d > 1e-9) ? vy / FT(d) : Vector_3(0, 1, 0);
            Vector_3 uz = (h > 1e-9) ? vz / FT(h) : Vector_3(0, 0, 1);

            v0 = v0 - ux * FT(grow) - uy * FT(grow) - uz * FT(grow);
            w += 2.0 * grow;
            d += 2.0 * grow;
            h += 2.0 * grow;
            
            vx = ux * FT(w);
            vy = uy * FT(d);
            vz = uz * FT(h);
        }

        Geometry res;
        std::string type = "closed";
        bool dx = (w > 1e-9);
        bool dy = (d > 1e-9);
        bool dz = (h > 1e-9);
        int dims = (dx ? 1 : 0) + (dy ? 1 : 0) + (dz ? 1 : 0);

        if (dims == 3) {
            res.vertices.push_back({FT(0), FT(0), FT(0)}); // 0
            res.vertices.push_back({FT(w), FT(0), FT(0)}); // 1
            res.vertices.push_back({FT(w), FT(d), FT(0)}); // 2
            res.vertices.push_back({FT(0), FT(d), FT(0)}); // 3
            res.vertices.push_back({FT(0), FT(0), FT(h)}); // 4
            res.vertices.push_back({FT(w), FT(0), FT(h)}); // 5
            res.vertices.push_back({FT(w), FT(d), FT(h)}); // 6
            res.vertices.push_back({FT(0), FT(d), FT(h)}); // 7

            res.faces.push_back({{{3, 2, 1, 0}}}); // Bottom
            res.faces.push_back({{{4, 5, 6, 7}}}); // Top
            res.faces.push_back({{{0, 1, 5, 4}}}); // Front
            res.faces.push_back({{{1, 2, 6, 5}}}); // Right
            res.faces.push_back({{{2, 3, 7, 6}}}); // Back
            res.faces.push_back({{{3, 0, 4, 7}}}); // Left
            type = "closed";
        } else if (dims == 2) {
            // Pick the two non-zero dimensions
            if (!dz) { // XY Plane
                res.vertices.push_back({FT(0), FT(0), FT(0)});
                res.vertices.push_back({FT(w), FT(0), FT(0)});
                res.vertices.push_back({FT(w), FT(d), FT(0)});
                res.vertices.push_back({FT(0), FT(d), FT(0)});
            } else if (!dy) { // XZ Plane
                res.vertices.push_back({FT(0), FT(0), FT(0)});
                res.vertices.push_back({FT(w), FT(0), FT(0)});
                res.vertices.push_back({FT(w), FT(0), FT(h)});
                res.vertices.push_back({FT(0), FT(0), FT(h)});
            } else { // YZ Plane
                res.vertices.push_back({FT(0), FT(0), FT(0)});
                res.vertices.push_back({FT(0), FT(d), FT(0)});
                res.vertices.push_back({FT(0), FT(d), FT(h)});
                res.vertices.push_back({FT(0), FT(0), FT(h)});
            }
            res.faces.push_back({{{0, 1, 2, 3}}});
            type = "surface";
        } else if (dims == 1) {
            res.vertices.push_back({FT(0), FT(0), FT(0)});
            res.vertices.push_back({FT(w), FT(d), FT(h)});
            res.segments.push_back({0, 1});
            type = "segments";
        } else {
            res.vertices.push_back({FT(0), FT(0), FT(0)});
            res.points.push_back(0);
            type = "points";
        }

        Shape out = P::make_shape(vfs, res, {{"type", type}});
        
        // Matrix construction from basis vectors and origin
        Vector_3 ux = (w > 1e-9) ? vx / FT(w) : Vector_3(1, 0, 0);
        Vector_3 uy = (d > 1e-9) ? vy / FT(d) : Vector_3(0, 1, 0);
        Vector_3 uz = (h > 1e-9) ? vz / FT(h) : Vector_3(0, 0, 1);

        // CGAL Transformation from basis vectors
        Transformation t(
            ux.x(), uy.x(), uz.x(), v0.x(),
            ux.y(), uy.y(), uz.y(), v0.y(),
            ux.z(), uy.z(), uz.z(), v0.z()
        );
        out.tf = Matrix(t);

        vfs->write(fulfilling.with_output("$out"), out);
    }

    static std::vector<std::string> argument_keys() { return {"$in", "grow"}; }
    static typename P::json schema() {
        return {
            {"path", "jot/orientedBoundingBox"},
            {"description", "Returns an oriented bounding box (OBB) for the subject. The OBB is the smallest volume box that encloses the shape, regardless of its orientation."},
            {"inputs", {{"$in", {{"type", "jot:shape"}}}}},
            {"arguments", nlohmann::json::array({
                {{"name", "grow"}, {"type", "jot:number"}, {"default", 0.0}}
            })},
            {"outputs", {{"$out", {{"type", "jot:shape"}}}}}
        };
    }
};

static void obb_init(fs::VFSNode* vfs) {
    Processor::register_op<OrientedBoundingBoxOp<>, Shape, double>(vfs, "jot/orientedBoundingBox");
    Processor::register_op<OrientedBoundingBoxOp<>, Shape, double>(vfs, "jot/obb");
}

} // namespace geo
} // namespace jotcad
