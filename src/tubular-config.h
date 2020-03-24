#ifndef TUBULAR_TUBULAR_CONFIG_H_
#define TUBULAR_TUBULAR_CONFIG_H_
#include <stdint.h>

#include <array>
#include <limits>
#include <string>

namespace tubular {

struct TubularConfig {
  std::string xpd_filepath    = "";
  std::string cyhair_filepath = "";
  std::string obj_filepath    = "";

  int max_segments    = 10;
  int radial_segments = 4;
  float radius_scale  = 1.f;
  float user_radius   = -1.f;  // If user_radius is negative, tubular use the
                               // original thicknesses.

  uint32_t max_strands = 0;  // 0 -> use all strands

  float tile_ratio = 4.0;

  std::array<float, 3> fix_normal;

  bool one_side_plane = false;  // radial_segments == 2 only

  float culling_y_min = std::numeric_limits<float>::infinity();
  float culling_y_max = -std::numeric_limits<float>::infinity();
};

}  // namespace tubular
#endif  // TUBULAR_TUBULAR_CONFIG_H_
