#pragma once

#include <vector>
#include <cmath>
#include <map>
#include <set>
#include <iostream>
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
 * Result of a packing operation.
 */
struct PackResult {
    struct Placement {
        size_t part_index;
        int bin_index;
        Matrix tf;
    };
    std::vector<Placement> placements;
    std::vector<size_t> unplaced_parts;

    struct BinInfo {
        double xmin, ymin, xmax, ymax;
    };
    std::vector<BinInfo> bins;
};

/**
 * High-level wrapper for 2D nesting.
 */
class Engine {
public:
    struct Config {
        double spacing = 2.0;
        double margin = 5.0;
    };

    /**
     * Packs parts into one or more sheets.
     */
    static PackResult pack(fs::VFSNode* vfs, const std::vector<Shape>& parts, const std::vector<Shape>& sheets, const Config& config) {
        using namespace libnest2d;

        const double SCALE = 1000000.0;
        PackResult result;

        // 1. Prepare Items (Parts)
        std::vector<Item> items;
        std::vector<size_t> item_to_part;
        std::vector<std::pair<double, double>> local_offsets;

        for (size_t i = 0; i < parts.size(); ++i) {
            auto poly_info = shape_to_nest_poly(vfs, parts[i], SCALE);
            if (!poly_info.has_value()) continue;

            Item item(std::move(poly_info->first));
            items.push_back(std::move(item));
            item_to_part.push_back(i);
            local_offsets.push_back(poly_info->second);
        }

        if (items.empty()) return result;

        // 2. Prepare Bins (Sheets)
        std::vector<Box> available_bins;
        if (sheets.empty()) {
            double w = 1000 - 2.0 * config.margin;
            double h = 1000 - 2.0 * config.margin;
            if (w < 0) w = 0; if (h < 0) h = 0;
            result.bins.push_back({0, 0, 1000, 1000});
            available_bins.emplace_back(static_cast<Coord>(w * SCALE), static_cast<Coord>(h * SCALE));
        } else {
            for (const auto& s : sheets) {
                double xmin = 0, ymin = 0, xmax = 1000, ymax = 1000;
                if (s.geometry.has_value()) {
                    auto geo = vfs->read<Geometry>(s.geometry.value());
                    auto bbox = geo.bounds();
                    xmin = CGAL::to_double(bbox.xmin());
                    ymin = CGAL::to_double(bbox.ymin());
                    xmax = CGAL::to_double(bbox.xmax());
                    ymax = CGAL::to_double(bbox.ymax());
                }
                result.bins.push_back({xmin, ymin, xmax, ymax});
                double w = xmax - xmin - 2.0 * config.margin;
                double h = ymax - ymin - 2.0 * config.margin;
                if (w < 0) w = 0; if (h < 0) h = 0;
                available_bins.emplace_back(static_cast<Coord>(w * SCALE), static_cast<Coord>(h * SCALE));
            }
        }

        // 3. Perform Nesting (Multi-bin loop)
        for (size_t b = 0; b < available_bins.size(); ++b) {
            std::vector<std::reference_wrapper<Item>> batch;
            std::vector<size_t> batch_indices;
            for (size_t i = 0; i < items.size(); ++i) {
                if (items[i].binId() == BIN_ID_UNSET) {
                    batch.push_back(items[i]);
                    batch_indices.push_back(i);
                }
            }
            if (batch.empty()) break;

            // Use BottomLeftPlacer for stability
            _Nester<BottomLeftPlacer, FirstFitSelection> nester(available_bins[b], static_cast<Coord>(config.spacing * SCALE));
            nester.execute(batch.begin(), batch.end());

            // IMPORTANT: Only accept items placed in bin 0 of THIS nester.
            // Items with binId > 0 overflowed and should be tried in the NEXT sheet.
            for (auto& item_ref : batch) {
                if (item_ref.get().binId() == 0) {
                    item_ref.get().binId(static_cast<int>(b));
                } else {
                    item_ref.get().binId(BIN_ID_UNSET);
                }
            }
        }

        // 4. Collect Results
        std::set<size_t> placed;
        for (size_t i = 0; i < items.size(); ++i) {
            const auto& item = items[i];
            if (item.binId() == BIN_ID_UNSET) continue;

            size_t part_idx = item_to_part[i];
            placed.insert(part_idx);

            auto tr = item.translation();
            auto rot = item.rotation();
            const auto& bin_info = result.bins[item.binId()];
            const auto& local_off = local_offsets[i];

            double tx = static_cast<double>(getX(tr)) / SCALE;
            double ty = static_cast<double>(getY(tr)) / SCALE;
            double angle = -static_cast<double>(rot); 
            double turns = angle / (2.0 * M_PI);

            // 1. Bring original part's local-bbox corner (xmin, ymin) to 0,0
            Matrix m = Matrix::translate(FT(-local_off.first), FT(-local_off.second), FT(0));
            
            // 2. Rotate (Z)
            m = m.rotateZ(turns);
            
            // 3. Place at packed position in world space
            // Top-Left Bias: Use (xmin + tx) and (ymax - ty)
            double world_x = bin_info.xmin + config.margin + tx;
            double world_y = bin_info.ymax - config.margin - ty; 
            m = m.translated(FT(world_x), FT(world_y), FT(0));

            result.placements.push_back({part_idx, item.binId(), m});
        }

        for (size_t i = 0; i < parts.size(); ++i) {
            if (placed.find(i) == placed.end()) {
                result.unplaced_parts.push_back(i);
            }
        }

        return result;
    }

private:
    static std::optional<std::pair<libnest2d::PolygonImpl, std::pair<double, double>>> 
    shape_to_nest_poly(fs::VFSNode* vfs, const Shape& shape, double scale) {
        using namespace libnest2d;
        if (!shape.geometry.has_value()) return std::nullopt;

        const Geometry geo = vfs->read<Geometry>(shape.geometry.value());
        if (geo.faces.empty()) return std::nullopt;

        auto bbox = geo.bounds();
        double xmin = CGAL::to_double(bbox.xmin());
        double ymin = CGAL::to_double(bbox.ymin());

        auto process_loop = [&](const std::vector<int>& loop) {
            PathImpl path;
            for (int idx : loop) {
                if (idx < 0 || idx >= (int)geo.vertices.size()) continue;
                const auto& v = geo.vertices[idx];
                double vx = CGAL::to_double(v.x) - xmin;
                double vy = CGAL::to_double(v.y) - ymin;
                
                path.push_back(ClipperLib::IntPoint(
                    static_cast<ClipperLib::cInt>(vx * scale),
                    static_cast<ClipperLib::cInt>(vy * scale)
                ));
            }
            if (!path.empty() && (path.front().X != path.back().X || path.front().Y != path.back().Y)) {
                path.push_back(path.front());
            }
            return path;
        };

        PathImpl contour = process_loop(geo.faces[0].loops[0]);
        if (contour.empty() || contour.size() < 3) return std::nullopt;

        // libnest2d internal Clipper usage expects Outer contours to be CCW (standard orientation).
        if (!ClipperLib::Orientation(contour)) {
            ClipperLib::ReversePath(contour);
        }

        HoleStore holes;
        for (size_t l = 1; l < geo.faces[0].loops.size(); ++l) {
            PathImpl hole = process_loop(geo.faces[0].loops[l]);
            if (hole.size() >= 3) {
                if (ClipperLib::Orientation(hole)) ClipperLib::ReversePath(hole);
                holes.push_back(std::move(hole));
            }
        }

        return std::make_pair(PolygonImpl(std::move(contour), std::move(holes)), 
                             std::make_pair(xmin, ymin));
    }
};

} // namespace pack
} // namespace geo
} // namespace jotcad
