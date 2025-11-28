#!/bin/bash

set -e

echo "Running run_ruled_surfaces_complex_geometry_test.sh..."
bash ./run_ruled_surfaces_complex_geometry_test.sh || { echo "run_ruled_surfaces_complex_geometry_test.sh failed"; exit 1; }

echo "Running run_ruled_surfaces_join_strategy_naive_test.sh..."
bash ./run_ruled_surfaces_join_strategy_naive_test.sh || { echo "run_ruled_surfaces_join_strategy_naive_test.sh failed"; exit 1; }

echo "Running run_ruled_surfaces_linear_all_min_area_test.sh..."
bash ./run_ruled_surfaces_linear_all_min_area_test.sh || { echo "run_ruled_surfaces_linear_all_min_area_test.sh failed"; exit 1; }

echo "Running run_ruled_surfaces_linear_slg_min_area_test.sh..."
bash ./run_ruled_surfaces_linear_slg_min_area_test.sh || { echo "run_ruled_surfaces_linear_slg_min_area_test.sh failed"; exit 1; }

echo "Running run_ruled_surfaces_seam_search_all_test.sh..."
bash ./run_ruled_surfaces_seam_search_all_test.sh || { echo "run_ruled_surfaces_seam_search_all_test.sh failed"; exit 1; }

echo "Running run_ruled_surfaces_stitching_sa_min_area_test.sh..."
bash ./run_ruled_surfaces_stitching_sa_min_area_test.sh || { echo "run_ruled_surfaces_stitching_sa_min_area_test.sh failed"; exit 1; }

echo "All tests passed!"
