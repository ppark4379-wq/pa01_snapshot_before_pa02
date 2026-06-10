# PA02 추가 실험: GPU 시간 분해 + CPU-vs-GPU Crossover

## 실험 목적
Branch() CPU direct scoring 선택의 타당성을 측정으로 증명.
기존 rejected_or_excluded_targets.csv의 "Rejected by structure" 근거를 수치로 보강.

## 실행 환경
- 하드웨어: Jetson Nano (Maxwell, 1 SM, 128 CUDA cores)
- 커널: Linux 4.9.253-tegra
- OS: Ubuntu 18.04.6 LTS / CUDA 10.2.300
- 실행: Docker /root/catkin_ws
- Map/Grid: 467x314 cells, resolution 0.05m
- 측정: warmup 3회 제외, 10회 평균
- 벤치마크 파일: src/pa02_gpu_bench.cu (독립 실행, ROS 불필요)

## 파일 설명

### gpu_bench_partA_breakdown_N3840.csv
- N=3840 (W0 candidate_count), P=1081 scan points
- GPU 1회 호출을 cudaEvent로 5구간 분해
- 컬럼: alloc_ms / h2d_ms / kernel_ms / d2h_ms / free_ms / total_ms
- 핵심 결과: kernel 86.6%, 메모리overhead 13.4%

### gpu_bench_partB_crossover_sweep.csv  
- N=16~54621 sweep (실제 W0/W1/W2 포함)
- CPU direct scoring vs GPU total/kernel 비교
- 핵심 결과: crossover N≈1725 (보간)
  - N<=1024: CPU 승 (N=16에서 GPU가 36.2x 느림, alloc overhead 79.5%)
  - N>=3840: GPU 승

### gpu_bench_partC_blocksize_sweep.csv
- N=3840에서 block size 64/128/256/512 비교
- 핵심 결과: 최대 차이 0.037ms(0.5%), block size 둔감
- 현재 default(256) 실질적으로 최적

## 보고서 활용
- Branch() CPU 선택 근거 (3p): crossover N≈1725, Branch workload N<=16
- GPU 시간 분해표 (3p): malloc/H2D/kernel/D2H 분해
- 기각 실험 보강 (5p): 측정값으로 GPU offloading 기각 근거 추가
