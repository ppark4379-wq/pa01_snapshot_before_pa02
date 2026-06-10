# PA02 Report Tables

## 1. Workload Definition

| Workload | scan_count | scan_points | candidate_count |
|---|---:|---:|---:|
| W0 | 15 | 1081 | 3840 |
| W1 | 15 | 1081 | 14415 |
| W2 | 21 | 1081 | 54621 |

## 2. End-to-End Runtime

| Workload | L0 avg ms | L2 avg ms | Speedup | L0 median | L2 median | L0 p95 | L2 p95 |
|---|---:|---:|---:|---:|---:|---:|---:|
| W0 | 208.66 | 80.27 | 2.60x | 185.18 | 81.47 | 379.03 | 83.84 |
| W1 | 140.45 | 69.78 | 2.01x | 117.97 | 69.51 | 236.23 | 83.56 |
| W2 | 199.56 | 123.39 | 1.62x | 170.16 | 124.34 | 352.43 | 142.10 |

## 3. Main Optimized Functions

| Function | Workload | L0 avg ms | L2 avg ms | Speedup |
|---|---|---:|---:|---:|
| Branch | W0 | 129.83 | 1.01 | 129.07x |
| Branch | W1 | 73.55 | 1.20 | 61.11x |
| Branch | W2 | 81.94 | 1.61 | 50.97x |
| MakeLowCands | W0 | 0.12 | 0.03 | 4.47x |
| MakeLowCands | W1 | 0.39 | 0.09 | 4.32x |
| MakeLowCands | W2 | 2.87 | 0.34 | 8.39x |

## 4. Score Stage Check

| Workload | L0 Score ms | L2 Score ms | Speedup |
|---|---:|---:|---:|
| W0 | 77.96 | 78.50 | 0.99x |
| W1 | 65.75 | 67.74 | 0.97x |
| W2 | 113.72 | 120.43 | 0.94x |

## 5. Accuracy Summary

| Version | Workload | Samples | ok_ratio | max_pose_dx | max_pose_dy | max_pose_dyaw | max_abs_diff |
|---|---|---:|---:|---:|---:|---:|---:|
| L0_relaunch3 | W0 | 1363 | 1.000 | 0.000000000 | 0.000000000 | 0.000000000 | 0.000000000 |
| L0_relaunch3 | W1 | 2007 | 1.000 | 0.000000000 | 0.000000000 | 0.000000000 | 0.000000000 |
| L0_relaunch3 | W2 | 1413 | 1.000 | 0.000000000 | 0.000000000 | 0.000000000 | 0.000000000 |
| L2_relaunch3 | W0 | 3489 | 1.000 | 0.000000000 | 0.000000000 | 0.000000000 | 0.000000000 |
| L2_relaunch3 | W1 | 3983 | 1.000 | 0.000000000 | 0.000000000 | 0.000000000 | 0.000000000 |
| L2_relaunch3 | W2 | 2275 | 1.000 | 0.000000000 | 0.000000000 | 0.000000000 | 0.000000000 |