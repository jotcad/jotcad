# Extrude Operations (geo/ops/extrude)

This directory contains specialized operators and algorithms for sweeping and extruding geometric entities (faces, polylines, segments) along vectors and reference frames.

## Components

- `polyline.h`: Robust 3D polyline and polyloop extrusion with 2D arrangement-based self-intersection regularization, exact rational raycast unprojection, double-quad T-junction elimination, and multi-mesh decomposition.
