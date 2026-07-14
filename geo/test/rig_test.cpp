#include "test_base.h"
#include "rig/joint.h"
#include "rig/weights.h"
#include "rig/skinning.h"
#include "ops/deform_op.h"

using namespace jotcad::geo;
using namespace jotcad::geo::rig;

// Helper to generate a subdivided closed cylinder manifold
Geometry generate_cylinder(double radius, double height, int slices, int segments) {
    Geometry geo;
    double dz = height / slices;
    double dtheta = 2.0 * M_PI / segments;

    // 1. Add vertices along Z-axis slices
    for (int s = 0; s <= slices; ++s) {
        double z = s * dz;
        for (int i = 0; i < segments; ++i) {
            double theta = i * dtheta;
            double x = radius * std::cos(theta);
            double y = radius * std::sin(theta);
            geo.vertices.push_back({x, y, z});
        }
    }

    // 2. Add side face triangles
    for (int s = 0; s < slices; ++s) {
        for (int i = 0; i < segments; ++i) {
            int inext = (i + 1) % segments;
            int v00 = s * segments + i;
            int v01 = s * segments + inext;
            int v10 = (s + 1) * segments + i;
            int v11 = (s + 1) * segments + inext;

            geo.triangles.push_back({v00, v01, v11});
            geo.triangles.push_back({v00, v11, v10});
        }
    }

    // 3. Add bottom cap center and triangles (z = 0)
    int center_bottom = (int)geo.vertices.size();
    geo.vertices.push_back({0.0, 0.0, 0.0});
    for (int i = 0; i < segments; ++i) {
        int inext = (i + 1) % segments;
        geo.triangles.push_back({inext, i, center_bottom});
    }

    // 4. Add top cap center and triangles (z = height)
    int center_top = (int)geo.vertices.size();
    geo.vertices.push_back({0.0, 0.0, height});
    int top_start = slices * segments;
    for (int i = 0; i < segments; ++i) {
        int inext = (i + 1) % segments;
        geo.triangles.push_back({top_start + i, top_start + inext, center_top});
    }

    return geo;
}

int main() {
    MockVFS vfs("rig");
    register_all_ops(&vfs);

    std::cout << "Testing Rigging and Physics-Based Skinning (Option C)..." << std::endl;

    // 1. Generate Cylinder Geometry
    double radius = 1.0;
    double height = 10.0;
    int slices = 20;
    int segments = 16;
    Geometry rest_geo = generate_cylinder(radius, height, slices, segments);
    
    // Verify rest cylinder volume (pi * r^2 * h = 3.14159 * 1 * 10 = ~31.4159)
    DeformOp<>::Mesh rest_mesh = boolean::Engine::geometry_to_mesh_ik(rest_geo);
    double rest_vol = CGAL::to_double(CGAL::Polygon_mesh_processing::volume(rest_mesh));
    std::cout << "  - Rest Cylinder volume: " << rest_vol << " (Expected ~31.4159)" << std::endl;
    assert(std::abs(rest_vol - 3.14159265 * radius * radius * height) < 1.0);

    // 2. Set up Skeleton (2 bones along Z-axis)
    Skeleton skeleton;
    Joint bone0("Base", 0, -1);
    bone0.local_bind_tf = Matrix::identity();
    Joint bone1("Elbow", 1, 0);
    bone1.local_bind_tf = Matrix::translate(0, 0, 5.0);

    skeleton.joints.push_back(bone0);
    skeleton.joints.push_back(bone1);
    skeleton.compute_inverse_bind_matrices();

    // 3. Set up Skin Weights (Smooth transition between z=3.0 and z=7.0)
    SkinWeights weights;
    weights.vertex_weights_list.resize(rest_geo.vertices.size());

    for (size_t i = 0; i < rest_geo.vertices.size(); ++i) {
        double z = CGAL::to_double(rest_geo.vertices[i].z);
        VertexWeights& vw = weights.vertex_weights_list[i];
        
        if (z <= 3.0) {
            vw.influences.push_back({0, 1.0}); // Fully Bone 0
        } else if (z >= 7.0) {
            vw.influences.push_back({1, 1.0}); // Fully Bone 1
        } else {
            // Linear interpolation
            double w1 = (z - 3.0) / 4.0;
            double w0 = 1.0 - w1;
            vw.influences.push_back({0, w0});
            vw.influences.push_back({1, w1});
        }
        vw.normalize();
    }

    // 4. Pose the Skeleton: Rotate Bone 1 by 90 degrees around X-axis (0.25 turns)
    skeleton.joints[0].local_tf = Matrix::identity();
    skeleton.joints[1].local_tf = Matrix::translate(0, 0, 5.0) * Matrix::rotationX(0.25);
    skeleton.update_world_transforms();

    // 5. Apply LBS (Linear Blend Skinning)
    Geometry lbs_geo = rest_geo;
    for (size_t i = 0; i < lbs_geo.vertices.size(); ++i) {
        EK::Point_3 rest_pt(rest_geo.vertices[i].x, rest_geo.vertices[i].y, rest_geo.vertices[i].z);
        EK::Point_3 posed_pt = Skinning::apply_lbs(rest_pt, skeleton, weights.vertex_weights_list[i]);
        lbs_geo.vertices[i] = {posed_pt.x(), posed_pt.y(), posed_pt.z()};
    }

    DeformOp<>::Mesh lbs_mesh = boolean::Engine::geometry_to_mesh_ik(lbs_geo);
    double lbs_vol = CGAL::to_double(CGAL::Polygon_mesh_processing::volume(lbs_mesh));
    std::cout << "  - LBS Skinned volume (Bent 90°): " << lbs_vol << std::endl;
    std::cout << "    - Volume Shrinkage: " << (1.0 - lbs_vol / rest_vol) * 100.0 << "%" << std::endl;

    // 6. Apply PBD Relaxation (Option C) using rest cylinder's spring-strut network
    // Build PBD constraints on the rest mesh structure to compute rest lengths
    auto constraints = DeformOp<>::build_spring_network(rest_mesh);
    std::cout << "  - Generated PBD spring network with " << constraints.size() << " constraints." << std::endl;

    // Relax the LBS mesh using the constraints
    DeformOp<>::execute_pbd(lbs_mesh, constraints, 40);
    CGAL::Polygon_mesh_processing::tangential_relaxation(lbs_mesh);

    double pbd_vol = CGAL::to_double(CGAL::Polygon_mesh_processing::volume(lbs_mesh));
    std::cout << "  - PBD Relaxed volume (Option C): " << pbd_vol << std::endl;
    std::cout << "    - Volume Recovery: " << (pbd_vol - lbs_vol) / (rest_vol - lbs_vol) * 100.0 << "% of lost volume" << std::endl;

    // Verify volume recovery is significant
    assert(pbd_vol > lbs_vol);

    std::cout << "  - Testing JOT/RIG VFS Operator End-to-End..." << std::endl;

    // 7. Assemble the Rig Shape
    Shape rig_shape;
    rig_shape.tags["type"] = "rig";

    Shape solid_shape;
    solid_shape.geometry = vfs.materialize<Geometry>(rest_geo);
    solid_shape.tags["type"] = "solid";
    rig_shape.components.push_back(solid_shape);

    Shape j0;
    j0.tf = Matrix::identity();
    j0.tags = {
        {"type", "joint"},
        {"role", "ghost"},
        {"joint_id", 0},
        {"name", "Base"},
        {"bind_tf", "1 0 0 0 0 1 0 0 0 0 1 0"}
    };

    Shape j1;
    // Posed transform is world-space!
    j1.tf = Matrix::translate(0, 0, 5.0) * Matrix::rotationX(0.25);
    j1.tags = {
        {"type", "joint"},
        {"role", "ghost"},
        {"joint_id", 1},
        {"name", "Elbow"},
        {"bind_tf", "1 0 0 0 0 1 0 0 0 0 1 5"}
    };

    j0.components.push_back(j1);
    rig_shape.components.push_back(j0);

    // 8. Execute jot/rig Selector
    fs::Selector rig_sel = fs::Selector{"jot/rig", {
        {"$in", rig_shape.to_json()},
        {"pbd_iterations", 40}
    }}.with_output("$out");

    Processor::execute(&vfs, rig_sel);
    Shape output_rig = vfs.read<Shape>(rig_sel);

    // Verify output structure
    assert(output_rig.tags.value("type", "") == "rig");
    
    // Find the deformed solid component
    Shape* deformed_solid = nullptr;
    for (auto& comp : output_rig.components) {
        if (comp.tags.value("type", "") == "solid") {
            deformed_solid = &comp;
            break;
        }
    }
    assert(deformed_solid != nullptr);
    assert(deformed_solid->geometry.has_value());

    // Measure the volume of the deformed geometry
    Geometry deformed_geo = vfs.read<Geometry>(deformed_solid->geometry.value());
    DeformOp<>::Mesh deformed_mesh = boolean::Engine::geometry_to_mesh_ik(deformed_geo);
    double deformed_vol = CGAL::to_double(CGAL::Polygon_mesh_processing::volume(deformed_mesh));
    std::cout << "  - Operator Deformed volume: " << deformed_vol << std::endl;
    
    // Ensure the operator deformed volume is greater than the collapsed LBS volume (lbs_vol = ~28.5)
    assert(deformed_vol > lbs_vol);

    std::cout << "✅ ALL Rigging and JOT/RIG Operator Tests Passed" << std::endl;
    return 0;
}
