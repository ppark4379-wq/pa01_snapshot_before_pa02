#include "cartographer_parallel/assignment.h"

#include <vector>

namespace cartographer_parallel {

extern "C"
void score_all_cuda(const std::vector<unsigned char>& grid,
                    int w,
                    int h,
                    const std::vector<int>& px,
                    const std::vector<int>& py,
                    const std::vector<int>& cx,
                    const std::vector<int>& cy,
                    std::vector<float>* score);

void make_cand(const int min_x, const int max_x,
               const int min_y, const int max_y,
               const int step,
               std::vector<int>* const cx,
               std::vector<int>* const cy) {
  if (cx == nullptr || cy == nullptr || step <= 0) return;

  for (int x = min_x; x <= max_x; x += step) {
    for (int y = min_y; y <= max_y; y += step) {
      cx->push_back(x);
      cy->push_back(y);
    }
  }
}

void score_all(const std::vector<unsigned char>& grid,
               const int w,
               const int h,
               const std::vector<int>& px,
               const std::vector<int>& py,
               const std::vector<int>& cx,
               const std::vector<int>& cy,
               std::vector<float>* const score) {
  score_all_cuda(grid, w, h, px, py, cx, cy, score);
}

}  // namespace cartographer_parallel
