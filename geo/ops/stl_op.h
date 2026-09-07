#pragma once
#include "protocols.h"
#include "processor.h"
#include "stl.h"
#include "matrix.h"
#include "data/surface_mesh_geometry.h"
#include "fix/assert_mesh.h"

namespace jotcad {
namespace geo {

template <typename P = JotVfsProtocol>
struct StlOp : P {
    static constexpr const char* path = "jot/stl";

    static void execute(fs::VFSNode* vfs, const fs::Selector& fulfilling, const Shape& in, const std::string& stl_path) {
        STLWriter writer;
        for (const auto& node : in.shapes()) {
            if (!node.has_positive_geometry()) continue;
            try {
                Geometry geo = vfs->read<Geometry>(node.geometry.value());
                geo.apply_tf(node.tf);
                writer.add_geometry(geo);
            } catch (const std::exception& e) {
                std::cerr << "[StlOp] Error reading geometry: " << e.what() << std::endl;
            }
        }
        auto stl_bytes = writer.write_binary();
        
        // Output: STL bytes in the primary '$out' port
        vfs->write(fulfilling.with_output("$out"), stl_bytes);
    }

    static std::vector<std::string> argument_keys() { return {"$in", "path"}; }

    static typename P::json schema() {
        return {
            {"path", "jot/stl"},
            {"description", "Generates a binary STL file from the spatial representation of the input shape."},
            {"inputs", {{"$in", {{"type", "jot:shape"}}}}},
            {"arguments", json::array({
                {{"name", "path"}, {"type", "jot:string"}, {"default", "export.stl"}}
            })},
            {"outputs", {
                {"$out", {{"type", "file"}, {"mimeType", "model/stl"}, {"description", "The generated STL blob."}}}
            }}
        };
    }
};

#include <CGAL/IO/STL.h>
#include <CGAL/Polygon_mesh_processing/repair_polygon_soup.h>
#include <CGAL/Polygon_mesh_processing/orient_polygon_soup.h>
#include <CGAL/Polygon_mesh_processing/polygon_soup_to_polygon_mesh.h>
#include <CGAL/Polygon_mesh_processing/stitch_borders.h>
#include <CGAL/Polygon_mesh_processing/triangulate_faces.h>
#include <sstream>

template <typename P = JotVfsProtocol>
struct StlImportOp : P {
    static constexpr const char* path = "jot/Stl";

    static void execute(fs::VFSNode* vfs, const fs::Selector& fulfilling, const fs::Selector& file) {
        fs::VFSResult file_res = vfs->read<fs::VFSResult>(file);
        if (file_res.data.empty()) {
            throw std::runtime_error("StlImportOp: Empty STL file payload");
        }

        std::string str(reinterpret_cast<const char*>(file_res.data.data()), file_res.data.size());
        std::istringstream is(str, std::ios::binary);

        std::vector<EK::Point_3> points;
        std::vector<std::vector<std::size_t>> facets;
        if (!CGAL::IO::read_STL(is, points, facets)) {
            throw std::runtime_error("Failed to parse STL file via CGAL::IO::read_STL");
        }

        // Repair and orient polygon soup
        CGAL::Polygon_mesh_processing::repair_polygon_soup(points, facets);
        CGAL::Polygon_mesh_processing::orient_polygon_soup(points, facets);

        ExactMesh imported_mesh;
        CGAL::Polygon_mesh_processing::polygon_soup_to_polygon_mesh(points, facets, imported_mesh);
        CGAL::Polygon_mesh_processing::stitch_borders(imported_mesh);
        CGAL::Polygon_mesh_processing::triangulate_faces(imported_mesh);

        // Assert loaded STL is a well-formed 2-manifold with no self-intersections
        fix::assert_well_formed_open_or_closed_mesh(imported_mesh, "StlImportOp: loaded STL");

        bool is_closed = CGAL::is_closed(imported_mesh);
        Geometry geo = to_geometry(imported_mesh);
        Shape out = P::make_shape(vfs, geo, {{"type", is_closed ? "closed" : "surface"}});
        vfs->write(fulfilling.with_output("$out"), out);
    }

    static std::vector<std::string> argument_keys() { return {"file"}; }

    static typename P::json schema() {
        return {
            {"path", "jot/Stl"},
            {"description", "Imports an STL file from the VFS and returns its Shape representation."},
            {"inputs", nlohmann::json::object()},
            {"arguments", json::array({
                {{"name", "file"}, {"type", "jot:file"}}
            })},
            {"outputs", {
                {"$out", {{"type", "jot:shape"}, {"description", "The imported shape."}}}
            }}
        };
    }
};

static void stl_init(fs::VFSNode* vfs) {
    Processor::register_op<StlOp<>, Shape, std::string>(vfs, "jot/stl");
    Processor::register_op<StlImportOp<>, fs::Selector>(vfs, "jot/Stl");
}

} // namespace geo
} // namespace jotcad
