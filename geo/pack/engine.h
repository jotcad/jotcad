#pragma once

#include <vector>
#include <cmath>
#include "../data/geometry.h"
#include "../data/shape.h"
#include "../math/matrix.h"
#include "vfs_node.h"

// Define dependencies for libnest2d
#ifndef LIBNEST2D_GEOMETRIES_clipper
#define LIBNEST2D_GEOMETRIES_clipper
#endif
#ifndef LIBNEST2D_OPTIMIZER_nlopt
#define LIBNEST2D_OPTIMIZER_nlopt
#endif

// libnest2d includes
#include <libnest2d/libnest2d.hpp>

namespace jotcad {
namespace geo {
namespace pack {

/**
 * High-level wrapper for 2D nesting.
 */
class Engine {
public:
    struct Config {
        double sheet_width = 1000.0;
        double sheet_height = 1000.0;
        double spacing = 2.0;
        double margin = 5.0;
    };

    /**
     * Packs a list of shapes into a sheet.
     * Updates the 'tf' of each shape to position it correctly.
     */
    static void pack(fs::VFSNode* vfs, std::vector<Shape>& shapes, const Config& config) {
        using namespace libnest2d;

        std::vector<Item> items;
        const double SCALE = 1000000.0; // Clipper units (1.0 = 1,000,000)

        for (size_t i = 0; i < shapes.size(); ++i) {
            auto& shape = shapes[i];
            if (!shape.geometry.has_value()) continue;

            const Geometry geo = vfs->read<Geometry>(shape.geometry.value());
            if (geo.faces.empty()) continue;

            PathImpl contour;
            for (int idx : geo.faces[0].loops[0]) {
                const auto& v = geo.vertices[idx];
                contour.push_back(ClipperLib::IntPoint(
                    static_cast<ClipperLib::cInt>(CGAL::to_double(v.x) * SCALE),
                    static_cast<ClipperLib::cInt>(CGAL::to_double(v.y) * SCALE)
                ));
            }

            HoleStore holes;
            for (size_t l = 1; l < geo.faces[0].loops.size(); ++l) {
                PathImpl hole;
                for (int idx : geo.faces[0].loops[l]) {
                    const auto& v = geo.vertices[idx];
                    hole.push_back(ClipperLib::IntPoint(
                        static_cast<ClipperLib::cInt>(CGAL::to_double(v.x) * SCALE),
                        static_cast<ClipperLib::cInt>(CGAL::to_double(v.y) * SCALE)
                    ));
                }
                holes.push_back(std::move(hole));
            }

            PolygonImpl poly(std::move(contour), std::move(holes));
            
            // Ensure closure
            if (!poly.Contour.empty() && (poly.Contour.front().X != poly.Contour.back().X || poly.Contour.front().Y != poly.Contour.back().Y)) {
                poly.Contour.push_back(poly.Contour.front());
            }
            for (auto& h : poly.Holes) {
                if (!h.empty() && (h.front().X != h.back().X || h.front().Y != h.back().Y)) {
                    h.push_back(h.front());
                }
            }

            // libnest2d Clipper backend expects CLOCKWISE for outer boundaries.
            if (!ClipperLib::Orientation(poly.Contour)) {
                ClipperLib::ReversePath(poly.Contour);
            }

            items.emplace_back(std::move(poly));
        }

        if (items.empty()) return;

        // 2. Perform Nesting
        Box bin(
            static_cast<Coord>(config.sheet_width * SCALE),
            static_cast<Coord>(config.sheet_height * SCALE)
        );

        // Switch to BottomLeftPlacer for better stability in basic packing
        NestConfig<BottomLeftPlacer> ncfg;
        
        nest<BottomLeftPlacer>(items, bin, static_cast<Coord>(config.spacing * SCALE), ncfg);

        // 3. Update Shape Transformations
        for (size_t i = 0; i < items.size(); ++i) {
            const auto& item = items[i];
            auto& shape = shapes[i];

            auto translation = item.translation();
            auto rotation = item.rotation(); // In radians

            double tx = static_cast<double>(translation.X) / SCALE;
            double ty = static_cast<double>(translation.Y) / SCALE;
            double angle = static_cast<double>(rotation);

            double cos_a = std::cos(angle);
            double sin_a = std::sin(angle);

            // Z-axis rotation matrix in 3D
            Matrix placement(CGAL::Aff_transformation_3<EK>(
                FT(cos_a), FT(-sin_a), FT(0), FT(tx),
                FT(sin_a), FT(cos_a),  FT(0), FT(ty),
                FT(0),     FT(0),      FT(1), FT(0)
            ));

            shape.tf = placement;
        }
    }
};

} // namespace pack
} // namespace geo
} // namespace jotcad
