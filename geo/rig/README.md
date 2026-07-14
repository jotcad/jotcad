# Skeletal Rigging and Skinning Module (`geo/rig`)

This module implements skeletal rigging, joint hierarchies, coordinate-space skinning, and physics-based volume preservation for 3D meshes inside JotCAD.

## Directory Index

* **[joint.h](file:///home/brian/github/jotcad_ez/geo/rig/joint.h)**: Defines hierarchical skeletal joint coordinate frames, forward kinematics, and inverse bind matrices.
* **[weights.h](file:///home/brian/github/jotcad_ez/geo/rig/weights.h)**: Defines structures for mapping mesh vertices to bone weights and indices.
* **[skinning.h](file:///home/brian/github/jotcad_ez/geo/rig/skinning.h)**: Implements coordinate interpolation solvers including Linear Blend Skinning (LBS) and Dual Quaternion Skinning (DQS).
* **[rig_op.h](file:///home/brian/github/jotcad_ez/geo/rig/rig_op.h)**: Implements the `jot/rig` operator, which combines skeletal skinning with Position-Based Dynamics (PBD) to maintain muscle/bulk volume under rotation.

## Invariants & Rules

1. **Weight Normalization**: Skin weights for any single vertex MUST sum exactly to `1.0`. The loader/weight structure must enforce this.
2. **Bind-Pose Duality**: Every joint must maintain its local/world transform in the rest bind-pose to calculate its `Inverse Bind Matrix` ($T_{bind}^{-1}$).
3. **Rigid Transforms**: Dual Quaternion Skinning (DQS) must normalize the blended dual quaternions before transforming vertices to ensure pure rigid-body translation and rotation.
4. **PBD Relaxation**: When using PBD volume preservation, rest spring lengths and struts must be computed in the rest bind-pose *before* any skinning transform is applied.
