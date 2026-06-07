#include <cuda_runtime.h>

#include <chrono>
#include <stdio.h>
#include <vector>
#include <algorithm>

__global__
void score_kernel_clean(const unsigned char* grid,
                        int w, int h,
                        const int* px,
                        const int* py,
                        int p,
                        const int* cx,
                        const int* cy,
                        int n,
                        float* score) {
  int i = blockIdx.x * blockDim.x + threadIdx.x;
  if (i >= n) return;

  int sum = 0;
  const int cxi = cx[i];
  const int cyi = cy[i];

  for (int j = 0; j < p; ++j) {
    const int x = px[j] + cxi;
    const int y = py[j] + cyi;

    if (x >= 0 && x < w && y >= 0 && y < h) {
      sum += grid[y * w + x];
    }
  }

  score[i] = static_cast<float>(sum) / (255.0f * static_cast<float>(p));
}

extern "C"
void score_all_cuda(const std::vector<unsigned char>& grid,
                    int w,
                    int h,
                    const std::vector<int>& px,
                    const std::vector<int>& py,
                    const std::vector<int>& cx,
                    const std::vector<int>& cy,
                    std::vector<float>* score) {
  const int n = static_cast<int>(std::min(cx.size(), cy.size()));
  const int p = static_cast<int>(std::min(px.size(), py.size()));

  score->assign(n, 0.0f);

  if (n == 0 || p == 0 || w <= 0 || h <= 0 ||
      grid.size() < static_cast<size_t>(w * h)) {
    return;
  }

  unsigned char* d_grid = nullptr;
  int* d_px = nullptr;
  int* d_py = nullptr;
  int* d_cx = nullptr;
  int* d_cy = nullptr;
  float* d_score = nullptr;

  const size_t grid_size = static_cast<size_t>(w) * h * sizeof(unsigned char);
  const size_t point_size = static_cast<size_t>(p) * sizeof(int);
  const size_t cand_size = static_cast<size_t>(n) * sizeof(int);
  const size_t score_size = static_cast<size_t>(n) * sizeof(float);

  auto start = std::chrono::high_resolution_clock::now();

  cudaMalloc(&d_grid, grid_size);
  cudaMalloc(&d_px, point_size);
  cudaMalloc(&d_py, point_size);
  cudaMalloc(&d_cx, cand_size);
  cudaMalloc(&d_cy, cand_size);
  cudaMalloc(&d_score, score_size);

  cudaMemcpy(d_grid, grid.data(), grid_size, cudaMemcpyHostToDevice);
  cudaMemcpy(d_px, px.data(), point_size, cudaMemcpyHostToDevice);
  cudaMemcpy(d_py, py.data(), point_size, cudaMemcpyHostToDevice);
  cudaMemcpy(d_cx, cx.data(), cand_size, cudaMemcpyHostToDevice);
  cudaMemcpy(d_cy, cy.data(), cand_size, cudaMemcpyHostToDevice);

  const int threads = 256;
  const int blocks = (n + threads - 1) / threads;

  score_kernel_clean<<<blocks, threads>>>(
      d_grid, w, h,
      d_px, d_py, p,
      d_cx, d_cy, n,
      d_score);

  cudaDeviceSynchronize();

  cudaMemcpy(score->data(), d_score, score_size, cudaMemcpyDeviceToHost);

  cudaFree(d_grid);
  cudaFree(d_px);
  cudaFree(d_py);
  cudaFree(d_cx);
  cudaFree(d_cy);
  cudaFree(d_score);

  auto end = std::chrono::high_resolution_clock::now();
  std::chrono::duration<double, std::milli> elapsed = end - start;

  float best_score = 0.0f;
  for (int i = 0; i < n; ++i) {
    if ((*score)[i] > best_score) best_score = (*score)[i];
  }

  printf("[CLEAN_GPU] candidates=%d, scan_points=%d, time=%.6f ms, best_score=%.6f\n",
         n, p, elapsed.count(), best_score);
}
