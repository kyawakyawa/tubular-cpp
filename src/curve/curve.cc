#include "curve/curve.h"

namespace tubular {
Curve::~Curve() = default;

Curve::Curve(const std::vector<float3>& points,
             const std::vector<float>& radiuses, const bool closed) {
  points_   = points;
  radiuses_ = radiuses;
  closed_   = closed;
  assert(points_.size() == radiuses_.size());
}

float3 Curve::GetPointAt(const float u) const {
  const float t = GetUtoTmapping(u);
  return GetPoint(t);
}

float Curve::GetRadiusAt(const float u) const {
  const float t = GetUtoTmapping(u);
  return GetRadius(t);
}

float3 Curve::GetTangentAt(const float u) const {
  const float t = GetUtoTmapping(u);
  return GetTangent(t);
}
// TODO return value. use pointer?
std::vector<float> Curve::GetLengths(int divisions) const {
  if (divisions < 0) {
    divisions = 200;
  }

  if (!cacheArcLengths_.empty() /* TODO is this ok?*/ &&
      (int(cacheArcLengths_.size()) == divisions + 1) && !needsUpdate_) {
    return cacheArcLengths_;
  }

  needsUpdate_ = false;

  std::vector<float> cache(size_t(divisions + 1));

  float3 current, last = GetPoint(0.f);

  cache[0] = 0.f;

  float sum = 0.f;
  for (int p = 1; p <= divisions; p++) {
    current = GetPoint(1.f * p / divisions);
    sum += vlength(current - last);
    cache[size_t(p)] = sum;
    last             = current;
  }

  cacheArcLengths_ = cache;
  return cache;
}

// select an initial normal vector perpendicular to the first tangent
// vector, and in the direction of the minimum tangent xyz component

static void ComputeNormalAndBinormal(const float3& tangent,
                                     const float3& fix_normal, float3* normal,
                                     float3* binormal) {
  *normal   = float3(0.f);
  *binormal = float3(0.f);

  float3 n;
  if (fabsf(vlength(fix_normal) - 1.f) < kEps) {
    n = fix_normal;
  } else {
    float min      = std::numeric_limits<float>::max();
    const float tx = fabsf(tangent.x());
    const float ty = fabsf(tangent.y());
    const float tz = fabsf(tangent.z());
    if (tx <= min) {
      min = tx;
      n   = float3(1, 0, 0);
    }
    if (ty <= min) {
      min = ty;
      n   = float3(0, 1, 0);
    }
    if (tz <= min) {
      n = float3(0, 0, 1);
    }
  }

  const float3 vec = vnormalized(vcross(tangent, n));
  *normal          = vcross(tangent, vec);
  *binormal        = vcross(tangent, *normal);
}

static void ComputeFrenetFrames(const int segments, const bool closed,
                                std::vector<float3>* tangents,
                                std::vector<float3>* normals,
                                std::vector<float3>* binormals,
                                std::vector<FrenetFrame>* frames) {
  tangents->resize(size_t(segments + 1));
  normals->resize(size_t(segments + 1));
  binormals->resize(size_t(segments + 1));

  // compute the slowly-varying normal and binormal vectors for each segment
  // on the curve

  for (int i = 1; i <= segments; i++) {
    // copy previous
    (*normals)[size_t(i)]   = (*normals)[size_t(i - 1)];
    (*binormals)[size_t(i)] = (*binormals)[size_t(i - 1)];

    // Rotation axis
    float3 axis = vcross((*tangents)[size_t(i - 1)], (*tangents)[size_t(i)]);
    if (vlength(axis) > std::numeric_limits<float>::epsilon()) {
      axis = vnormalized(axis);

      const float dot =
          vdot((*tangents)[size_t(i - 1)], (*tangents)[size_t(i)]);

      // clamp for floating pt errors
      const float theta = acosf(Clamp(dot, -1.f, 1.f));  // rad

      // TODO check
      (*normals)[size_t(i)] =
          ArbitraryAxisRotation(axis, theta, (*normals)[size_t(i)]);
    }

    (*binormals)[size_t(i)] =
        vnormalized(vcross((*tangents)[size_t(i)], (*normals)[size_t(i)]));
  }

  // if the curve is closed, postprocess the vectors so the first and last
  // normal vectors are the same

  if (closed) {
    float theta = acosf(
        Clamp(vdot((*normals)[size_t(0)], (*normals)[size_t(segments)]), -1.f,
              1.f));  // rad
    theta /= segments;

    if (vdot((*tangents)[size_t(0)],
             vcross((*normals)[size_t(0)], (*normals)[size_t(segments)])) >
        0.f) {
      theta = -theta;
    }

    for (int i = 1; i <= segments; i++) {
      (*normals)[size_t(i)] = ArbitraryAxisRotation(
          (*tangents)[size_t(i)], theta * i, (*normals)[size_t(i)]);
      // normals[i] =
      //     (Quaternion.AngleAxis(Mathf.Deg2Rad * theta * i, tangents[i])
      //     *
      //      normals[i]);
      //  TODO Deg2Rad?
      (*binormals)[size_t(i)] =
          vcross((*tangents)[size_t(i)], (*normals)[size_t(i)]);
    }
  }

  int n = int(tangents->size());
  for (int i = 0; i < n; i++) {
    FrenetFrame frame((*tangents)[size_t(i)], (*normals)[size_t(i)],
                      (*binormals)[size_t(i)]);
    frames->emplace_back(frame);
  }
}

void Curve::ComputeFrenetFrames(const int segments, const bool closed,
                                std::vector<FrenetFrame>* frames) const {
  std::vector<float3> tangents(size_t(segments + 1));
  std::vector<float3> normals(size_t(segments + 1));
  std::vector<float3> binormals(size_t(segments + 1));

  float u;

  // compute the tangent vectors for each segment on the curve
  for (int i = 0; i <= segments; i++) {
    u                   = (1.f * i) / segments;
    tangents[size_t(i)] = vnormalized(GetTangentAt(u));
  }

  // select an initial normal vector perpendicular to the first tangent
  // vector, and in the direction of the minimum tangent xyz component

  ComputeNormalAndBinormal(tangents[0], float(0.f), &normals[0], &binormals[0]);

  // compute the slowly-varying normal and binormal vectors for each segment
  // on the curve

  ::tubular::ComputeFrenetFrames(segments, closed, &tangents, &normals,
                                 &binormals, frames);
}

void Curve::ComputeFrenetFramesFixNormal(
    const int segments, const bool closed, const float3& fix_normal,
    std::vector<FrenetFrame>* frames) const {
  std::vector<float3> tangents(size_t(segments + 1));
  std::vector<float3> normals(size_t(segments + 1));
  std::vector<float3> binormals(size_t(segments + 1));

  float u;

  // compute the tangent vectors for each segment on the curve
  for (int i = 0; i <= segments; i++) {
    u                   = (1.f * i) / segments;
    tangents[size_t(i)] = vnormalized(GetTangentAt(u));
  }

  // select an initial normal vector perpendicular to the first tangent
  // vector, and in the direction of the minimum tangent xyz component

  ComputeNormalAndBinormal(tangents[0], vnormalize(fix_normal), &normals[0],
                           &binormals[0]);

  // compute the slowly-varying normal and binormal vectors for each segment
  // on the curve

  ::tubular::ComputeFrenetFrames(segments, closed, &tangents, &normals,
                                 &binormals, frames);
}

float3 Curve::GetTangent(const float t) const {
  const float delta = 0.001f;

  float t1 = t - delta;
  float t2 = t + delta;

  // Capping in case of danger
  if (t1 < 0.f) t1 = 0.f;
  if (t2 > 1.f) t2 = 1.f;

  const float3 pt1 = GetPoint(t1);
  const float3 pt2 = GetPoint(t2);
  return vnormalized(pt2 - pt1);
}

float Curve::GetUtoTmapping(const float u) const {
  const std::vector<float>& arcLengths = GetLengths();
  int i = 0, il = int(arcLengths.size());

  // The targeted u distance value to get
  const float targetArcLength = u * arcLengths[size_t(il - 1)];

  // binary search for the index with largest value smaller than target u
  // distance
  int low = 0, high = il - 1;
  float comparison;

  while (low <= high) {
    i          = int(floorf(low + (high - low) / 2.f));  // TODO is this ok ?
    comparison = arcLengths[size_t(i)] - targetArcLength;

    if (comparison < 0.f) {
      low = i + 1;
    } else if (comparison > 0.f) {
      high = i - 1;
    } else {
      high = i;
      break;
    }
  }

  i = high;

  // if (Mathf.Approximately(arcLengths[i], targetArcLength))
  if (fabsf(arcLengths[size_t(i)] - targetArcLength) < kEps /*TODO*/) {
    return 1.f * i / (il - 1);
  }

  // we could get finer grain at lengths, or use simple interpolation between
  // two points

  const float lengthBefore = arcLengths[size_t(i)];
  const float lengthAfter  = arcLengths[size_t(i + 1)];

  const float segmentLength = lengthAfter - lengthBefore;

  // determine where we are between the 'before' and 'after' points

  const float segmentFraction =
      (targetArcLength - lengthBefore) / segmentLength;

  // add that fractional amount to t
  const float t = 1.f * (i + segmentFraction) / (il - 1);

  return t;
}

}  // namespace tubular
