#pragma once
#include <vector>
#include <string>
#include <sstream>
#include <iomanip>
#include <cmath>
#include <algorithm>
#include "geometry.h"

namespace jotcad {
namespace geo {

struct PDFConfig {
    double scale = 2.83464567; // 1mm -> 72/25.4 pts
    double trim = 5.0; // 5mm margin
};

class PDFWriter {
public:
    PDFWriter(const PDFConfig& config = {}) : config_(config) {}

    void add_geometry(const Geometry& geo) {
        if (geo.vertices.empty()) return;
        
        for (const auto& v : geo.vertices) {
            if (!has_bounds_) {
                min_ = max_ = v;
                has_bounds_ = true;
            } else {
                if (v.x < min_.x) min_.x = v.x;
                if (v.y < min_.y) min_.y = v.y;
                if (v.z < min_.z) min_.z = v.z;
                if (v.x > max_.x) max_.x = v.x;
                if (v.y > max_.y) max_.y = v.y;
                if (v.z > max_.z) max_.z = v.z;
            }
        }
        geos_.push_back(geo);
    }

    std::vector<unsigned char> write() {
        if (geos_.empty()) return {};

        double width_mm = CGAL::to_double(max_.x - min_.x);
        double height_mm = CGAL::to_double(max_.y - min_.y);
        
        scale_ = config_.scale;
        double w_pt = (width_mm + 2 * config_.trim) * scale_;
        double h_pt = (height_mm + 2 * config_.trim) * scale_;

        std::stringstream ss;
        ss << "%PDF-1.4\n";
        ss << "1 0 obj << /Type /Catalog /Pages 2 0 R >> endobj\n";
        ss << "2 0 obj << /Type /Pages /Kids [3 0 R] /Count 1 >> endobj\n";
        ss << "3 0 obj << /Type /Page /Parent 2 0 R /MediaBox [0 0 " << w_pt << " " << h_pt << "] /Contents 4 0 R >> endobj\n";
        
        std::stringstream stream;
        stream << "q\n";
        stream << "0.1 w\n"; // 0.1pt line width

        for (const auto& geo : geos_) {
            for (const auto& face : geo.faces) {
                for (const auto& loop : face.loops) {
                    if (loop.empty()) continue;
                    
                    auto get_pt = [&](int idx) {
                        const auto& v = geo.vertices[idx];
                        double sx = CGAL::to_double(v.x - min_.x) * scale_ + (config_.trim * scale_);
                        double sy = CGAL::to_double(v.y - min_.y) * scale_ + (config_.trim * scale_);
                        return std::make_pair(sx, sy);
                    };

                    auto start = get_pt(loop[0]);
                    stream << start.first << " " << start.second << " m\n";
                    for (size_t i = 1; i < loop.size(); ++i) {
                        auto p = get_pt(loop[i]);
                        stream << p.first << " " << p.second << " l\n";
                    }
                    stream << "h S\n";
                }
            }
            
            for (const auto& seg : geo.segments) {
                const auto& v1 = geo.vertices[seg[0]];
                const auto& v2 = geo.vertices[seg[1]];
                
                auto get_v_scaled = [&](const Vertex& v) {
                    double sx = CGAL::to_double(v.x - min_.x) * scale_ + (config_.trim * scale_);
                    double sy = CGAL::to_double(v.y - min_.y) * scale_ + (config_.trim * scale_);
                    return std::make_pair(sx, sy);
                };
                
                auto p1 = get_v_scaled(v1);
                auto p2 = get_v_scaled(v2);
                stream << p1.first << " " << p1.second << " m " << p2.first << " " << p2.second << " l S\n";
            }
        }
        stream << "Q\n";
        
        std::string content = stream.str();
        ss << "4 0 obj << /Length " << content.length() << " >> stream\n" << content << "endstream\nendobj\n";
        ss << "xref\n0 5\n0000000000 65535 f \n";
        ss << "trailer << /Size 5 /Root 1 0 R >>\nstartxref\n%%EOF";

        std::string result = ss.str();
        return std::vector<unsigned char>(result.begin(), result.end());
    }

private:
    PDFConfig config_;
    std::vector<Geometry> geos_;
    Vertex min_, max_; // Corrected type: Vertex
    bool has_bounds_ = false;
    double scale_ = 1.0;
};

} // namespace geo
} // namespace jotcad
