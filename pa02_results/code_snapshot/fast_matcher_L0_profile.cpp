#include "cartographer_parallel/fast_matcher.h"

#include "cartographer_parallel/assignment.h"

#include <algorithm>
#include <atomic>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <chrono>
#include <cstdlib>
#include <fstream>
#include <limits>
#include <sstream>
#include <stdexcept>

namespace cartographer_parallel {
namespace {

std::string Trim(const std::string& s) {
  const char* ws = " \t\r\n";
  const std::string::size_type b = s.find_first_not_of(ws);
  if (b == std::string::npos) return "";
  const std::string::size_type e = s.find_last_not_of(ws);
  return s.substr(b, e - b + 1);
}

std::string Unquote(const std::string& s) {
  const std::string v = Trim(s);
  if (v.size() >= 2 &&
      ((v.front() == '"' && v.back() == '"') ||
       (v.front() == '\'' && v.back() == '\''))) {
    return v.substr(1, v.size() - 2);
  }
  return v;
}

std::string Dirname(const std::string& path) {
  const std::string::size_type slash = path.find_last_of('/');
  return slash == std::string::npos ? "." : path.substr(0, slash);
}

bool IsAbs(const std::string& path) {
  return !path.empty() && path[0] == '/';
}

std::string Join(const std::string& dir, const std::string& file) {
  if (file.empty() || IsAbs(file)) return file;
  return dir == "." ? file : dir + "/" + file;
}

std::vector<double> ParseList(std::string v) {
  for (char& c : v) {
    if (c == '[' || c == ']' || c == ',') c = ' ';
  }
  std::istringstream in(v);
  std::vector<double> out;
  double x = 0.0;
  while (in >> x) out.push_back(x);
  return out;
}

std::string PgmToken(std::istream* in) {
  std::string token;
  char c = 0;
  while (in->get(c)) {
    if (std::isspace(static_cast<unsigned char>(c))) continue;
    if (c == '#') {
      in->ignore(std::numeric_limits<std::streamsize>::max(), '\n');
      continue;
    }
    token.push_back(c);
    break;
  }
  while (in->get(c)) {
    if (std::isspace(static_cast<unsigned char>(c))) break;
    if (c == '#') {
      in->ignore(std::numeric_limits<std::streamsize>::max(), '\n');
      break;
    }
    token.push_back(c);
  }
  return token;
}

int ClampInt(const int x, const int lo, const int hi) {
  return std::max(lo, std::min(hi, x));
}

double NormalizeYaw(double yaw) {
  while (yaw > M_PI) yaw -= 2.0 * M_PI;
  while (yaw < -M_PI) yaw += 2.0 * M_PI;
  return yaw;
}


using Clock = std::chrono::high_resolution_clock;

struct Pa02ProfileCounters {
  long long score_all_calls = 0;
  long long branch_calls = 0;
  long long branch_nodes = 0;
  double sort_ms = 0.0;
};

thread_local Pa02ProfileCounters g_pa02_profile;

void ResetPa02ProfileCounters() {
  g_pa02_profile = Pa02ProfileCounters();
}

double ElapsedMs(const Clock::time_point& t0, const Clock::time_point& t1) {
  return std::chrono::duration<double, std::milli>(t1 - t0).count();
}

std::string EnvOr(const char* const key, const std::string& fallback) {
  const char* value = std::getenv(key);
  if (value == nullptr || value[0] == '\0') return fallback;
  return std::string(value);
}

std::string CsvPath(const std::string& filename) {
  const std::string base = EnvOr("PA02_RESULTS_DIR", "pa02_results");
  return base + "/csv/" + filename;
}

bool FileExists(const std::string& path) {
  std::ifstream in(path.c_str());
  return static_cast<bool>(in);
}

long long NextFrameId() {
  static std::atomic<long long> frame_id(0);
  return frame_id.fetch_add(1);
}


void AppendRuntimeRow(const std::string& version,
                      const std::string& git_sha,
                      const std::string& workload,
                      const std::string& run_id,
                      const long long frame_id,
                      const std::string& function,
                      const double time_ms,
                      const int scan_count,
                      const int scan_points,
                      const int candidate_count,
                      const int bounds_count,
                      const long long branch_calls,
                      const long long branch_nodes,
                      const int max_depth,
                      const long long score_all_calls,
                      const double sort_ms,
                      const int output_candidate_count,
                      const double best_score,
                      const double pose_x,
                      const double pose_y,
                      const double pose_yaw,
                      const double checksum) {
  const std::string path = CsvPath("runtime_profile.csv");
  const bool exists = FileExists(path);
  std::ofstream out(path.c_str(), std::ios::app);
  if (!out) return;
  if (!exists) {
    out << "version,git_sha,workload,run_id,frame_id,function,time_ms,"
           "scan_count,scan_points,candidate_count,bounds_count,branch_calls,"
           "branch_nodes,max_depth,score_all_calls,sort_ms,"
           "output_candidate_count,best_score,pose_x,pose_y,pose_yaw,checksum\n";
  }
  out << version << ','
      << git_sha << ','
      << workload << ','
      << run_id << ','
      << frame_id << ','
      << function << ','
      << time_ms << ','
      << scan_count << ','
      << scan_points << ','
      << candidate_count << ','
      << bounds_count << ','
      << branch_calls << ','
      << branch_nodes << ','
      << max_depth << ','
      << score_all_calls << ','
      << sort_ms << ','
      << output_candidate_count << ','
      << best_score << ','
      << pose_x << ','
      << pose_y << ','
      << pose_yaw << ','
      << checksum << '\n';
}

void AppendAccuracyRow(const std::string& version,
                       const std::string& git_sha,
                       const std::string& workload,
                       const std::string& run_id,
                       const long long frame_id,
                       const int ok,
                       const double best_score,
                       const double pose_x,
                       const double pose_y,
                       const double pose_yaw,
                       const int candidate_count,
                       const double top10_checksum,
                       const double score_checksum,
                       const double max_abs_diff,
                       const std::string& note) {
  const std::string path = CsvPath("accuracy_check.csv");
  const bool exists = FileExists(path);
  std::ofstream out(path.c_str(), std::ios::app);
  if (!out) return;
  if (!exists) {
    out << "version,git_sha,workload,run_id,frame_id,ok,best_score,"
           "pose_x,pose_y,pose_yaw,baseline_pose_x,baseline_pose_y,"
           "baseline_pose_yaw,pose_dx,pose_dy,pose_dyaw,candidate_count,"
           "top10_checksum,score_checksum,max_abs_diff,note\n";
  }
  // L0 profiling does not compare against another implementation inside C++.
  // The baseline/diff fields are filled with the current result so that later
  // report scripts can compare versions from CSV.
  out << version << ','
      << git_sha << ','
      << workload << ','
      << run_id << ','
      << frame_id << ','
      << ok << ','
      << best_score << ','
      << pose_x << ','
      << pose_y << ','
      << pose_yaw << ','
      << pose_x << ','
      << pose_y << ','
      << pose_yaw << ','
      << 0 << ','
      << 0 << ','
      << 0 << ','
      << candidate_count << ','
      << top10_checksum << ','
      << score_checksum << ','
      << max_abs_diff << ','
      << note << '\n';
}


}  // namespace

bool FastMatcher::LoadMap(const std::string& yaml_file) {
  std::ifstream yaml(yaml_file);
  if (!yaml) return false;

  std::string image;
  bool negate = false;
  double occupied_thresh = 0.65;
  double free_thresh = 0.196;
  std::string line;
  while (std::getline(yaml, line)) {
    line = line.substr(0, line.find('#'));
    const std::string::size_type colon = line.find(':');
    if (colon == std::string::npos) continue;
    const std::string key = Trim(line.substr(0, colon));
    const std::string val = Trim(line.substr(colon + 1));
    if (key == "image") {
      image = Join(Dirname(yaml_file), Unquote(val));
    } else if (key == "resolution") {
      res_ = std::stod(val);
    } else if (key == "origin") {
      const std::vector<double> origin = ParseList(val);
      if (origin.size() >= 2) {
        ox_ = origin[0];
        oy_ = origin[1];
      }
    } else if (key == "negate") {
      negate = (val == "1" || val == "true" || val == "True");
    } else if (key == "occupied_thresh") {
      occupied_thresh = std::stod(val);
    } else if (key == "free_thresh") {
      free_thresh = std::stod(val);
    }
  }
  (void)occupied_thresh;
  (void)free_thresh;
  if (image.empty()) return false;

  std::ifstream pgm(image, std::ios::binary);
  if (!pgm) return false;
  const std::string magic = PgmToken(&pgm);
  if (magic != "P5" && magic != "P2") return false;
  w_ = std::stoi(PgmToken(&pgm));
  h_ = std::stoi(PgmToken(&pgm));
  const int max_value = std::stoi(PgmToken(&pgm));
  if (w_ <= 0 || h_ <= 0 || max_value <= 0 || max_value > 255) return false;

  std::vector<unsigned char> pixels(w_ * h_, 0);
  if (magic == "P5") {
    pgm.read(reinterpret_cast<char*>(pixels.data()), pixels.size());
    if (pgm.gcount() != static_cast<std::streamsize>(pixels.size())) {
      return false;
    }
  } else {
    for (unsigned char& pixel : pixels) {
      const std::string token = PgmToken(&pgm);
      if (token.empty()) return false;
      pixel = static_cast<unsigned char>(
          ClampInt(std::stoi(token), 0, max_value));
    }
  }

  map_.assign(w_ * h_, 0);
  for (int i = 0; i < w_ * h_; ++i) {
    const double v = static_cast<double>(pixels[i]) / max_value;
    const double occ = negate ? v : (1.0 - v);
    map_[i] = static_cast<unsigned char>(
        ClampInt(static_cast<int>(std::lround(255.0 * occ)), 0, 255));
  }
  grids_ = MakeGridStack();
  return true;
}

void FastMatcher::SetOptions(const MatchOpt& opt) {
  opt_ = opt;
  if (has_map()) grids_ = MakeGridStack();
}

std::vector<FastMatcher::Scan> FastMatcher::MakeScans(
    const std::vector<float>& xs, const std::vector<float>& ys,
    const Pose2& init, int* const num_ang, double* const step) const {
  double max_range = 3.0 * res_;
  for (size_t i = 0; i < xs.size() && i < ys.size(); ++i) {
    max_range = std::max(max_range,
                         std::hypot(static_cast<double>(xs[i]),
                                    static_cast<double>(ys[i])));
  }

  double angle_step = opt_.angular_step;
  if (angle_step <= 0.0) {
    const double c = 1.0 - (res_ * res_) / (2.0 * max_range * max_range);
    angle_step = 0.999 * std::acos(std::max(-1.0, std::min(1.0, c)));
    if (!std::isfinite(angle_step) || angle_step <= 0.0) angle_step = 0.05;
  }
  const int n_ang = std::max(0, static_cast<int>(
                                   std::ceil(opt_.angular_window / angle_step)));
  const int scan_count = 2 * n_ang + 1;
  if (num_ang) *num_ang = n_ang;
  if (step) *step = angle_step;

  std::vector<Scan> scans(scan_count);
  for (int s = 0; s < scan_count; ++s) {
    const double da = (s - n_ang) * angle_step;
    const double yaw = init.yaw + da;
    const double c = std::cos(yaw);
    const double sn = std::sin(yaw);
    scans[s].x.reserve(xs.size());
    scans[s].y.reserve(xs.size());
    for (size_t i = 0; i < xs.size() && i < ys.size(); ++i) {
      const double wx = init.x + c * xs[i] - sn * ys[i];
      const double wy = init.y + sn * xs[i] + c * ys[i];
      const int mx = static_cast<int>(std::floor((wx - ox_) / res_));
      const int row_bottom = static_cast<int>(std::floor((wy - oy_) / res_));
      const int my = h_ - 1 - row_bottom;
      scans[s].x.push_back(mx);
      scans[s].y.push_back(my);
    }
  }
  return scans;
}

std::vector<FastMatcher::Bounds> FastMatcher::MakeBounds(
    const std::vector<Scan>& scans, const double window,
    const bool full_map) const {
  const int lin = static_cast<int>(std::ceil(window / res_));
  std::vector<Bounds> bounds(scans.size());
  for (size_t s = 0; s < scans.size(); ++s) {
    Bounds b;
    if (full_map) {
      b.min_x = std::numeric_limits<int>::lowest() / 4;
      b.max_x = std::numeric_limits<int>::max() / 4;
      b.min_y = std::numeric_limits<int>::lowest() / 4;
      b.max_y = std::numeric_limits<int>::max() / 4;
    } else {
      b.min_x = -lin;
      b.max_x = lin;
      b.min_y = -lin;
      b.max_y = lin;
      // Local/global-window search should stay centered on the initial pose.
      // Out-of-map scan points are already scored as zero in score_all().
      bounds[s] = b;
      continue;
    }

    for (size_t i = 0; i < scans[s].x.size(); ++i) {
      b.min_x = std::max(b.min_x, -scans[s].x[i]);
      b.max_x = std::min(b.max_x, w_ - 1 - scans[s].x[i]);
      b.min_y = std::max(b.min_y, -scans[s].y[i]);
      b.max_y = std::min(b.max_y, h_ - 1 - scans[s].y[i]);
    }
    bounds[s] = b;
  }
  return bounds;
}

std::vector<FastMatcher::Grid> FastMatcher::MakeGridStack() const {
  const int depth = std::max(1, opt_.branch_depth);
  std::vector<Grid> grids;
  grids.reserve(depth);
  for (int level = 0; level < depth; ++level) {
    const int win = 1 << level;
    Grid g;
    g.w = w_;
    g.h = h_;
    g.win = win;
    g.cell.assign(w_ * h_, 0);
    for (int y = 0; y < h_; ++y) {
      for (int x = 0; x < w_; ++x) {
        unsigned char best = 0;
        for (int dy = 0; dy < win && y + dy < h_; ++dy) {
          for (int dx = 0; dx < win && x + dx < w_; ++dx) {
            best = std::max(best, map_[(y + dy) * w_ + (x + dx)]);
          }
        }
        g.cell[y * w_ + x] = best;
      }
    }
    grids.push_back(g);
  }
  return grids;
}

std::vector<FastMatcher::Cand> FastMatcher::MakeLowCands(
    const std::vector<Bounds>& bounds, const int depth) const {
  const int step = 1 << depth;
  std::vector<Cand> out;
  for (size_t s = 0; s < bounds.size(); ++s) {
    if (bounds[s].min_x > bounds[s].max_x ||
        bounds[s].min_y > bounds[s].max_y) {
      continue;
    }
    std::vector<int> cx;
    std::vector<int> cy;
    make_cand(bounds[s].min_x, bounds[s].max_x, bounds[s].min_y,
              bounds[s].max_y, step, &cx, &cy);
    for (size_t i = 0; i < cx.size(); ++i) {
      Cand c;
      c.scan = static_cast<int>(s);
      c.x = cx[i];
      c.y = cy[i];
      out.push_back(c);
    }
  }
  return out;
}

void FastMatcher::Score(const Grid& grid, const std::vector<Scan>& scans,
                        std::vector<Cand>* const cand) const {
  if (cand == nullptr || cand->empty()) return;
  for (size_t s = 0; s < scans.size(); ++s) {
    std::vector<int> ids;
    std::vector<int> cx;
    std::vector<int> cy;
    for (size_t i = 0; i < cand->size(); ++i) {
      if ((*cand)[i].scan == static_cast<int>(s)) {
        ids.push_back(i);
        cx.push_back((*cand)[i].x);
        cy.push_back((*cand)[i].y);
      }
    }
    if (ids.empty()) continue;
    std::vector<float> score;
    ++g_pa02_profile.score_all_calls;
    score_all(grid.cell, grid.w, grid.h, scans[s].x, scans[s].y, cx, cy,
              &score);
    for (size_t i = 0; i < ids.size() && i < score.size(); ++i) {
      (*cand)[ids[i]].score = score[i];
    }
  }
  const Clock::time_point sort_t0 = Clock::now();
  std::sort(cand->begin(), cand->end(),
            [](const Cand& a, const Cand& b) { return a.score > b.score; });
  g_pa02_profile.sort_ms += ElapsedMs(sort_t0, Clock::now());
}

FastMatcher::Cand FastMatcher::Branch(const std::vector<Grid>& grids,
                                      const std::vector<Scan>& scans,
                                      const std::vector<Bounds>& bounds,
                                      const std::vector<Cand>& cand,
                                      const int depth,
                                      const float min_score) const {
  ++g_pa02_profile.branch_calls;
  if (cand.empty()) {
    Cand empty;
    empty.score = 0.0f;
    return empty;
  }
  if (depth == 0) return cand.front();

  Cand best;
  best.score = min_score;
  const int half = 1 << (depth - 1);
  for (const Cand& c : cand) {
    ++g_pa02_profile.branch_nodes;
    if (c.score <= best.score) break;
    std::vector<Cand> child;
    for (const int dx : {0, half}) {
      if (c.x + dx > bounds[c.scan].max_x) continue;
      for (const int dy : {0, half}) {
        if (c.y + dy > bounds[c.scan].max_y) continue;
        Cand next;
        next.scan = c.scan;
        next.x = c.x + dx;
        next.y = c.y + dy;
        child.push_back(next);
      }
    }
    Score(grids[depth - 1], scans, &child);
    const Cand refined = Branch(grids, scans, bounds, child, depth - 1,
                                best.score);
    if (refined.score > best.score) best = refined;
  }
  return best;
}

CandOut FastMatcher::ToOut(const Cand& cand, const Pose2& init,
                           const int num_ang, const double step) const {
  CandOut out;
  out.x = init.x + cand.x * res_;
  out.y = init.y - cand.y * res_;
  out.yaw = NormalizeYaw(init.yaw + (cand.scan - num_ang) * step);
  out.score = cand.score;
  return out;
}

bool FastMatcher::Match(const std::vector<float>& xs,
                        const std::vector<float>& ys, const Pose2& init,
                        const bool global, MatchOut* const out) const {
  return MatchWithWindow(xs, ys, init,
                         global ? opt_.global_window : opt_.linear_window,
                         global && opt_.full_map_search, out);
}

bool FastMatcher::MatchWithWindow(const std::vector<float>& xs,
                                  const std::vector<float>& ys,
                                  const Pose2& init,
                                  const double window,
                                  const bool full_map,
                                  MatchOut* const out) const {
  if (out == nullptr) return false;
  *out = MatchOut();
  if (!has_map() || xs.empty() || ys.empty()) return false;

  ResetPa02ProfileCounters();

  const std::string version = EnvOr("PA02_VERSION", "L0_baseline");
  const std::string git_sha = EnvOr("PA02_GIT_SHA", "unknown");
  const std::string workload = EnvOr("PA02_WORKLOAD", "W_unknown");
  const std::string run_id = EnvOr("PA02_RUN_ID", "0");
  const long long frame_id = NextFrameId();

  const Clock::time_point total_t0 = Clock::now();

  int num_ang = 0;
  double step = 0.0;

  const Clock::time_point scans_t0 = Clock::now();
  const std::vector<Scan> scans = MakeScans(xs, ys, init, &num_ang, &step);
  const double make_scans_ms = ElapsedMs(scans_t0, Clock::now());

  const Clock::time_point bounds_t0 = Clock::now();
  const std::vector<Bounds> bounds = MakeBounds(scans, window, full_map);
  const double make_bounds_ms = ElapsedMs(bounds_t0, Clock::now());

  std::vector<Grid> temp_grids;
  const std::vector<Grid>* grids_ptr = &grids_;
  if (grids_ptr->empty()) {
    temp_grids = MakeGridStack();
    grids_ptr = &temp_grids;
  }
  const std::vector<Grid>& grids = *grids_ptr;
  const int max_depth = static_cast<int>(grids.size()) - 1;

  const Clock::time_point low_t0 = Clock::now();
  std::vector<Cand> coarse = MakeLowCands(bounds, max_depth);
  const double make_low_cands_ms = ElapsedMs(low_t0, Clock::now());
  const int coarse_candidate_count = static_cast<int>(coarse.size());

  const long long score_calls_before = g_pa02_profile.score_all_calls;
  const double sort_ms_before = g_pa02_profile.sort_ms;
  const Clock::time_point score_t0 = Clock::now();
  Score(grids[max_depth], scans, &coarse);
  const double score_stage_ms = ElapsedMs(score_t0, Clock::now());
  const long long score_stage_calls =
      g_pa02_profile.score_all_calls - score_calls_before;
  const double score_stage_sort_ms = g_pa02_profile.sort_ms - sort_ms_before;

  if (coarse.empty()) {
    out->ok = false;
    out->score = 0.0;
    out->pose = init;
    const double total_ms = ElapsedMs(total_t0, Clock::now());
    const int scan_count = static_cast<int>(scans.size());
    const int scan_points =
        scans.empty() ? 0 : static_cast<int>(std::min(scans.front().x.size(),
                                                     scans.front().y.size()));
    const int bounds_count = static_cast<int>(bounds.size());
    AppendRuntimeRow(version, git_sha, workload, run_id, frame_id, "MakeScans",
                     make_scans_ms, scan_count, scan_points, 0, bounds_count,
                     0, 0, max_depth, 0, 0.0, 0, out->score,
                     out->pose.x, out->pose.y, out->pose.yaw, 0.0);
    AppendRuntimeRow(version, git_sha, workload, run_id, frame_id, "MakeBounds",
                     make_bounds_ms, scan_count, scan_points, 0, bounds_count,
                     0, 0, max_depth, 0, 0.0, 0, out->score,
                     out->pose.x, out->pose.y, out->pose.yaw, 0.0);
    AppendRuntimeRow(version, git_sha, workload, run_id, frame_id,
                     "MakeLowCands", make_low_cands_ms, scan_count,
                     scan_points, coarse_candidate_count, bounds_count, 0, 0,
                     max_depth, 0, 0.0, 0, out->score,
                     out->pose.x, out->pose.y, out->pose.yaw, 0.0);
    AppendRuntimeRow(version, git_sha, workload, run_id, frame_id, "Score",
                     score_stage_ms, scan_count, scan_points,
                     coarse_candidate_count, bounds_count, 0, 0, max_depth,
                     score_stage_calls, score_stage_sort_ms, 0, out->score,
                     out->pose.x, out->pose.y, out->pose.yaw, 0.0);
    AppendRuntimeRow(version, git_sha, workload, run_id, frame_id, "Total",
                     total_ms, scan_count, scan_points, coarse_candidate_count,
                     bounds_count, g_pa02_profile.branch_calls,
                     g_pa02_profile.branch_nodes, max_depth,
                     g_pa02_profile.score_all_calls, g_pa02_profile.sort_ms, 0,
                     out->score, out->pose.x, out->pose.y, out->pose.yaw, 0.0);
    AppendAccuracyRow(version, git_sha, workload, run_id, frame_id, 0,
                      out->score, out->pose.x, out->pose.y, out->pose.yaw,
                      coarse_candidate_count, 0.0, 0.0, 0.0, "empty_coarse");
    return false;
  }

  const long long branch_calls_before = g_pa02_profile.branch_calls;
  const long long branch_nodes_before = g_pa02_profile.branch_nodes;
  const long long branch_score_calls_before = g_pa02_profile.score_all_calls;
  const double branch_sort_ms_before = g_pa02_profile.sort_ms;
  const Clock::time_point branch_t0 = Clock::now();
  const Cand best = Branch(grids, scans, bounds, coarse, max_depth,
                           opt_.min_score);
  const double branch_ms = ElapsedMs(branch_t0, Clock::now());
  const long long branch_calls =
      g_pa02_profile.branch_calls - branch_calls_before;
  const long long branch_nodes =
      g_pa02_profile.branch_nodes - branch_nodes_before;
  const long long branch_score_calls =
      g_pa02_profile.score_all_calls - branch_score_calls_before;
  const double branch_sort_ms = g_pa02_profile.sort_ms - branch_sort_ms_before;

  out->ok = best.score > opt_.min_score;
  out->score = best.score;
  out->pose = init;

  const Clock::time_point to_out_t0 = Clock::now();
  if (out->ok) {
    const CandOut best_out = ToOut(best, init, num_ang, step);
    out->pose.x = best_out.x;
    out->pose.y = best_out.y;
    out->pose.yaw = best_out.yaw;
  }

  const int n = std::min(opt_.max_cand, static_cast<int>(coarse.size()));
  out->cand.reserve(n);
  for (int i = 0; i < n; ++i) {
    out->cand.push_back(ToOut(coarse[i], init, num_ang, step));
  }
  const double to_out_ms = ElapsedMs(to_out_t0, Clock::now());
  const double total_ms = ElapsedMs(total_t0, Clock::now());

  const int scan_count = static_cast<int>(scans.size());
  const int scan_points =
        scans.empty() ? 0 : static_cast<int>(std::min(scans.front().x.size(),
                                                     scans.front().y.size()));
  const int bounds_count = static_cast<int>(bounds.size());

  const auto candidate_checksum = [](const std::vector<Cand>& cand,
                                     const int max_items) -> double {
    const int count = std::min(max_items, static_cast<int>(cand.size()));
    double checksum_value = 0.0;
    for (int i = 0; i < count; ++i) {
      const Cand& c = cand[i];
      checksum_value +=
          (i + 1) *
          (c.scan * 1000003.0 + c.x * 9176.0 + c.y * 127.0 +
           std::floor(c.score * 1000000.0 + 0.5));
    }
    return checksum_value;
  };

  const auto score_checksum_all = [](const std::vector<Cand>& cand) -> double {
    double checksum_value = 0.0;
    for (size_t i = 0; i < cand.size(); ++i) {
      const Cand& c = cand[i];
      checksum_value += (static_cast<double>(i) + 1.0) *
                        std::floor(c.score * 1000000.0 + 0.5);
    }
    return checksum_value;
  };

  const double top10_checksum = candidate_checksum(coarse, 10);
  const double score_checksum = score_checksum_all(coarse);
  const double checksum = top10_checksum;

  AppendRuntimeRow(version, git_sha, workload, run_id, frame_id, "MakeScans",
                   make_scans_ms, scan_count, scan_points, 0, bounds_count,
                   0, 0, max_depth, 0, 0.0, n, out->score,
                   out->pose.x, out->pose.y, out->pose.yaw, checksum);
  AppendRuntimeRow(version, git_sha, workload, run_id, frame_id, "MakeBounds",
                   make_bounds_ms, scan_count, scan_points, 0, bounds_count,
                   0, 0, max_depth, 0, 0.0, n, out->score,
                   out->pose.x, out->pose.y, out->pose.yaw, checksum);
  AppendRuntimeRow(version, git_sha, workload, run_id, frame_id, "MakeLowCands",
                   make_low_cands_ms, scan_count, scan_points,
                   coarse_candidate_count, bounds_count, 0, 0, max_depth, 0,
                   0.0, n, out->score, out->pose.x, out->pose.y,
                   out->pose.yaw, checksum);
  AppendRuntimeRow(version, git_sha, workload, run_id, frame_id, "Score",
                   score_stage_ms, scan_count, scan_points,
                   coarse_candidate_count, bounds_count, 0, 0, max_depth,
                   score_stage_calls, score_stage_sort_ms, n, out->score,
                   out->pose.x, out->pose.y, out->pose.yaw, checksum);
  AppendRuntimeRow(version, git_sha, workload, run_id, frame_id, "Branch",
                   branch_ms, scan_count, scan_points,
                   coarse_candidate_count, bounds_count, branch_calls,
                   branch_nodes, max_depth, branch_score_calls, branch_sort_ms,
                   n, out->score, out->pose.x, out->pose.y, out->pose.yaw,
                   checksum);
  AppendRuntimeRow(version, git_sha, workload, run_id, frame_id, "ToOut",
                   to_out_ms, scan_count, scan_points,
                   coarse_candidate_count, bounds_count, 0, 0, max_depth, 0,
                   0.0, n, out->score, out->pose.x, out->pose.y,
                   out->pose.yaw, checksum);
  AppendRuntimeRow(version, git_sha, workload, run_id, frame_id, "Total",
                   total_ms, scan_count, scan_points,
                   coarse_candidate_count, bounds_count,
                   g_pa02_profile.branch_calls, g_pa02_profile.branch_nodes,
                   max_depth, g_pa02_profile.score_all_calls,
                   g_pa02_profile.sort_ms, n, out->score, out->pose.x,
                   out->pose.y, out->pose.yaw, checksum);

  AppendAccuracyRow(version, git_sha, workload, run_id, frame_id,
                    out->ok ? 1 : 0, out->score, out->pose.x, out->pose.y,
                    out->pose.yaw, coarse_candidate_count, top10_checksum,
                    score_checksum, 0.0, "profile_only");

  return out->ok;
}
}  // namespace cartographer_parallel
