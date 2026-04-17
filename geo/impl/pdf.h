#pragma once

#include "geometry.h"
#include "color.h"
#include <string>
#include <vector>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <cmath>
#include <json.hpp>

namespace jotcad {
namespace geo {

class PDFWriter {
public:
    struct Config {
        double width = 210.0;   // mm (default A4)
        double height = 297.0;  // mm
        double trim = 5.0;      // mm
        double lineWidth = 0.096;
    };

    struct StyledGeometry {
        Geometry geo;
        nlohmann::json tags;
    };

    PDFWriter() {
        scale_ = 1.0 / 0.352777778; // PostScript points per mm
    }

    explicit PDFWriter(const Config& config) : config_(config) {
        scale_ = 1.0 / 0.352777778;
    }

    void add_geometry(const Geometry& geo, const nlohmann::json& tags) {
        geometries_.push_back({geo, tags});
        update_bbox(geo);
    }

    std::vector<uint8_t> write() {
        if (geometries_.empty() || (min_.x > 1e17)) {
            min_ = {0, 0, 0};
            max_ = {0, 0, 0};
        }

        double width_mm = (max_.x - min_.x);
        double height_mm = (max_.y - min_.y);
        double width_pts = width_mm * scale_;
        double height_pts = height_mm * scale_;

        std::ostringstream ss;
        ss << std::fixed << std::setprecision(9);

        // Header
        double mediaX2 = width_pts + config_.trim * 2 * scale_;
        double mediaY2 = height_pts + config_.trim * 2 * scale_;
        double trimX1 = config_.trim * scale_;
        double trimY1 = config_.trim * scale_;
        double trimX2 = width_pts + config_.trim * scale_;
        double trimY2 = height_pts + config_.trim * scale_;

        ss << "%PDF-1.5\n";
        ss << "1 0 obj << /Pages 2 0 R /Type /Catalog >> endobj\n";
        ss << "2 0 obj << /Count 1 /Kids [ 3 0 R ] /Type /Pages >> endobj\n";
        ss << "3 0 obj <<\n";
        ss << "  /Contents 4 0 R\n";
        ss << "  /MediaBox [ 0 0 " << mediaX2 << " " << mediaY2 << " ]\n";
        ss << "  /TrimBox [ " << trimX1 << " " << trimY1 << " " << trimX2 << " " << trimY2 << " ]\n";
        ss << "  /Parent 2 0 R\n";
        ss << "  /Type /Page\n";
        ss << ">>\n";
        ss << "endobj\n";

        // Contents stream
        std::ostringstream stream;
        stream << std::fixed << std::setprecision(9);
        stream << config_.lineWidth << " w\n";

        for (const auto& sg : geometries_) {
            const auto& geo = sg.geo;
            
            // USE THE GLOBAL COLOR UTILITY
            std::vector<double> rgb = Color::from_tags(sg.tags);
            
            std::string fill_cmd = std::to_string(rgb[0]) + " " + std::to_string(rgb[1]) + " " + std::to_string(rgb[2]) + " rg\n";
            std::string stroke_cmd = std::to_string(rgb[0]) + " " + std::to_string(rgb[1]) + " " + std::to_string(rgb[2]) + " RG\n";

            auto draw_loop = [&](const std::vector<int>& loop) {
                for (size_t i = 0; i < loop.size(); ++i) {
                    int idx = loop[i];
                    if (idx < 0 || idx >= (int)geo.vertices.size()) continue;
                    const auto& v = geo.vertices[idx];
                    double sx = (v.x - min_.x) * scale_ + (config_.trim * scale_);
                    double sy = (v.y - min_.y) * scale_ + (config_.trim * scale_);
                    if (i == 0) stream << sx << " " << sy << " m\n";
                    else stream << sx << " " << sy << " l\n";
                }
                stream << "h\n"; // Close path
            };

            for (const auto& tri : geo.triangles) {
                stream << fill_cmd << stroke_cmd;
                draw_loop({tri[0], tri[1], tri[2]});
                stream << "f\n";
            }

            for (const auto& face : geo.faces) {
                if (face.loops.empty()) continue;
                stream << fill_cmd << stroke_cmd;
                for (const auto& loop : face.loops) {
                    draw_loop(loop);
                }
                stream << "f\n";
            }

            if (!geo.segments.empty()) {
                stream << stroke_cmd;
                Vertex last = {-1e9, -1e9, -1e9};
                bool in_path = false;
                
                for (const auto& seg : geo.segments) {
                    if (seg[0] < 0 || seg[0] >= (int)geo.vertices.size() ||
                        seg[1] < 0 || seg[1] >= (int)geo.vertices.size()) continue;

                    const auto& v1 = geo.vertices[seg[0]];
                    const auto& v2 = geo.vertices[seg[1]];
                    
                    double sx1 = (v1.x - min_.x) * scale_ + (config_.trim * scale_);
                    double sy1 = (v1.y - min_.y) * scale_ + (config_.trim * scale_);
                    double sx2 = (v2.x - min_.x) * scale_ + (config_.trim * scale_);
                    double sy2 = (v2.y - min_.y) * scale_ + (config_.trim * scale_);

                    if (std::abs(v1.x - last.x) > 1e-6 || std::abs(v1.y - last.y) > 1e-6) {
                        if (in_path) stream << "S\n";
                        stream << sx1 << " " << sy1 << " m\n";
                        in_path = true;
                    }
                    stream << sx2 << " " << sy2 << " l\n";
                    last = v2;
                }
                if (in_path) stream << "S\n";
            }
        }

        std::string stream_content = stream.str();
        ss << "4 0 obj << /Length " << stream_content.length() << " >>\n";
        ss << "stream\n" << stream_content << "\nendstream\n";
        ss << "endobj\n";

        ss << "trailer << /Root 1 0 R /Size 4 >>\n";
        ss << "%%EOF\n";

        std::string result = ss.str();
        return std::vector<uint8_t>(result.begin(), result.end());
    }

private:
    void update_bbox(const Geometry& geo) {
        for (const auto& v : geo.vertices) {
            min_.x = std::min(min_.x, v.x);
            min_.y = std::min(min_.y, v.y);
            min_.z = std::min(min_.z, v.z);
            max_.x = std::max(max_.x, v.x);
            max_.y = std::max(max_.y, v.y);
            max_.z = std::max(max_.z, v.z);
        }
    }

    Config config_;
    double scale_;
    std::vector<StyledGeometry> geometries_;
    Vertex min_ = {1e18, 1e18, 1e18};
    Vertex max_ = {-1e18, -1e18, -1e18};
};

} // namespace geo
} // namespace jotcad
