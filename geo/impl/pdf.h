#pragma once

#include "geometry.h"
#include <string>
#include <vector>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <cmath>

namespace jotcad {
namespace geo {

class PDFWriter {
public:
    struct Config {
        double width = 210.0;   // mm
        double height = 297.0;  // mm
        double trim = 5.0;      // mm
        double lineWidth = 0.096;
    };

    PDFWriter() {
        scale_ = 1.0 / 0.352777778;
    }

    explicit PDFWriter(const Config& config) : config_(config) {
        // Point size in mm: 25.4 / 72 = 0.352777778
        scale_ = 1.0 / 0.352777778;
    }

    void add_geometry(const Geometry& geo) {
        geometries_.push_back(geo);
        update_bbox(geo);
    }

    std::vector<uint8_t> write() {
        // Ensure even empty PDFs have a bounding box for trim calculation
        if (geometries_.empty() || (min_.x > 1e17)) {
            min_ = {0, 0, 0};
            max_ = {0, 0, 0};
        }

        double width = (max_.x - min_.x) * scale_;
        double height = (max_.y - min_.y) * scale_;
        
        std::ostringstream ss;
        ss << std::fixed << std::setprecision(9);

        // Header
        double mediaX2 = width + config_.trim * 2 * scale_;
        double mediaY2 = height + config_.trim * 2 * scale_;
        double trimX1 = config_.trim * scale_;
        double trimY1 = config_.trim * scale_;
        double trimX2 = width + config_.trim * scale_;
        double trimY2 = height + config_.trim * scale_;

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

        for (const auto& geo : geometries_) {
            if (geo.segments.empty()) continue;

            Vertex last = {-1e9, -1e9, -1e9};
            bool in_path = false;
            
            for (const auto& seg : geo.segments) {
                if (seg[0] < 0 || seg[0] >= geo.vertices.size() ||
                    seg[1] < 0 || seg[1] >= geo.vertices.size()) continue;

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

        std::string stream_content = stream.str();
        ss << "4 0 obj << /Length " << stream_content.length() << " >>\n";
        ss << "stream\n" << stream_content << "\nendstream\n";
        ss << "endobj\n";

        // Footer
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
    std::vector<Geometry> geometries_;
    Vertex min_ = {1e18, 1e18, 1e18};
    Vertex max_ = {-1e18, -1e18, -1e18};
};

} // namespace geo
} // namespace jotcad
