#pragma once

#include "ruled_surfaces_base.h"
#include "visitor.h"

namespace ruled_surfaces {

class RuledSurfaceTeeVisitor : public RuledSurfaceVisitor {
 public:
  RuledSurfaceTeeVisitor(RuledSurfaceVisitor& v1, RuledSurfaceVisitor& v2)
      : v1_(v1), v2_(v2) {}

  VisitControl OnSeam(bool is_reversed) override {
    if (v1_.OnSeam(is_reversed) == kStop) return kStop;
    return v2_.OnSeam(is_reversed);
  }
  VisitControl OnStart(const PolygonalChain& p,
                       const PolygonalChain& q) override {
    if (v1_.OnStart(p, q) == kStop) return kStop;
    return v2_.OnStart(p, q);
  }
  VisitControl OnInvalid(const Mesh& mesh) override {
    if (v1_.OnInvalid(mesh) == kStop) return kStop;
    return v2_.OnInvalid(mesh);
  }
  VisitControl OnValidMesh(const Mesh& mesh, double cost) override {
    if (v1_.OnValidMesh(mesh, cost) == kStop) return kStop;
    return v2_.OnValidMesh(mesh, cost);
  }
  VisitControl OnValidMesh(const Mesh& mesh, double cost, int shift) override {
    if (v1_.OnValidMesh(mesh, cost, shift) == kStop) return kStop;
    return v2_.OnValidMesh(mesh, cost, shift);
  }
  void OnFinish(const SolutionStats& stats) override {
    v1_.OnFinish(stats);
    v2_.OnFinish(stats);
  }

 private:
  RuledSurfaceVisitor& v1_;
  RuledSurfaceVisitor& v2_;
};

<<<<<<< HEAD
}  // namespace ruled_surfaces
=======


} // namespace ruled_surfaces


>>>>>>> main
