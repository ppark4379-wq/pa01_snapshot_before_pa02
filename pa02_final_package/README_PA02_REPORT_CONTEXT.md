# PA02 Report Context

This package contains the final PA02 experiment results for Cartographer fast_correlative_scan_matcher optimization.

## Core strategy

PA01 optimized score_all() CUDA backend.
PA02 does not count score_all() as a new optimization target.
PA02 keeps src/score_all.cpp, src/assignment_cuda.cu, and CMakeLists.txt fixed.
The PA02 optimization targets are inside src/fast_matcher.cpp.

## Optimized targets

1. Branch()
- Optimization: small-candidate CPU direct scoring / threshold-based CPU-GPU routing
- Purpose: remove repeated CUDA score_all calls for small irregular child candidate sets inside branch-and-bound.

2. MakeLowCands()
- Optimization: direct candidate generation, temporary vector/copy reduction, reserve-based allocation reduction
- Purpose: reduce host-side candidate generation overhead.

## Workloads

W0:
- scan_count = 15
- scan_points = 1081
- candidate_count = 3840

W1:
- scan_count = 15
- scan_points = 1081
- candidate_count = 14415

W2:
- scan_count = 21
- scan_points = 1081
- candidate_count = 54621

## Measurement protocol

L0_relaunch3 and L2_relaunch3 were each run for W0/W1/W2 with run_id 1~3.
Frame-level runtime was logged.
Initial frame_id < 3 was removed as warmup.
Representative metrics: avg, median, std, p95.

## Main results

Total speedup:
- W0: 208.66 ms -> 80.27 ms = 2.60x
- W1: 140.45 ms -> 69.78 ms = 2.01x
- W2: 199.56 ms -> 123.39 ms = 1.62x

Branch speedup:
- W0: 129.07x
- W1: 61.11x
- W2: 50.97x

MakeLowCands speedup:
- W0: 4.47x
- W1: 4.32x
- W2: 8.39x

## Accuracy

All workloads showed:
- ok_ratio = 1.000
- max_pose_dx = 0
- max_pose_dy = 0
- max_pose_dyaw = 0
- max_abs_diff = 0

## Important warning

Do not describe PA02 as re-optimizing score_all().
The score_all CUDA backend is fixed and only used as the existing backend.
PA02's new optimization contribution is Branch() and MakeLowCands() inside fast_matcher.cpp.
