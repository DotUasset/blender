/* SPDX-FileCopyrightText: 2019-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/* PCG Voronoi — port of Unreal Engine's VoronoiNoise3D_ALU + VoronoiCornerSample +
 * VoronoiCompare (Random.ush), using the Rand3DPCG16 hash. bTiling is always false. */

int3 pcg16_hash_i(int3 p)
{
  int3 v = p * 1664525 + 1013904223;

  v.x += v.y * v.z;
  v.y += v.z * v.x;
  v.z += v.x * v.y;
  v.x += v.y * v.z;
  v.y += v.z * v.x;
  v.z += v.x * v.y;

  uint x = uint(v.x) >> 16u;
  uint y = uint(v.y) >> 16u;
  uint z = uint(v.z) >> 16u;
  return int3(int(x), int(y), int(z));
}

float3 pcg_voronoi_corner_sample(float3 pos, float quality)
{
  int3 ip = int3(pos);
  int3 h = pcg16_hash_i(ip);
  float3 noise = float3(h) * (1.0f / 65535.0f) - 0.5f;

  if (quality <= 2.0f) {
    return normalize(noise) * 0.2588f;
  }
  if (quality == 3.0f) {
    return normalize(noise) * 0.3090f;
  }
  return noise;
}

float4 pcg_voronoi_compare(float4 minval, float3 candidate, float3 offset)
{
  float newdist = dot(offset, offset);
  if (newdist > minval.w) {
    return minval;
  }
  return float4(candidate, newdist);
}

[[node]]
void node_tex_pcg_voronoi(float3 vector, float quality, float3 &outPosition, float &outDistance)
{
  float3 fv = vector - floor(vector);
  float3 fv2 = (vector + 0.5f) - floor(vector + 0.5f);
  float3 iv = floor(vector);
  float3 iv2 = floor(vector + 0.5f);

  float4 mindist = float4(0.0f, 0.0f, 0.0f, 100.0f);
  float3 offset, p;

  if (quality == 3.0f) {
    for (offset.x = -1.0f; offset.x <= 1.0f; offset.x += 1.0f) {
      for (offset.y = -1.0f; offset.y <= 1.0f; offset.y += 1.0f) {
        for (offset.z = -1.0f; offset.z <= 1.0f; offset.z += 1.0f) {
          p = offset + pcg_voronoi_corner_sample(iv2 + offset, quality);
          mindist = pcg_voronoi_compare(mindist, iv2 + p, fv2 - p);
        }
      }
    }
  }
  else {
    for (offset.x = 0.0f; offset.x <= 1.0f; offset.x += 1.0f) {
      for (offset.y = 0.0f; offset.y <= 1.0f; offset.y += 1.0f) {
        for (offset.z = 0.0f; offset.z <= 1.0f; offset.z += 1.0f) {
          p = offset + pcg_voronoi_corner_sample(iv + offset, quality);
          mindist = pcg_voronoi_compare(mindist, iv + p, fv - p);

          if (quality == 2.0f) {
            p = offset + pcg_voronoi_corner_sample(iv2 + offset + 467.0f, quality);
            mindist = pcg_voronoi_compare(mindist, iv2 + p, fv2 - p);
          }
        }
      }
    }
  }

  if (quality >= 4.0f) {
    for (offset.x = -1.0f; offset.x <= 2.0f; offset.x += 3.0f) {
      for (offset.y = 0.0f; offset.y <= 1.0f; offset.y += 1.0f) {
        for (offset.z = 0.0f; offset.z <= 1.0f; offset.z += 1.0f) {
          p = offset + pcg_voronoi_corner_sample(iv + offset, quality);
          mindist = pcg_voronoi_compare(mindist, iv + p, fv - p);

          float3 offset_yzx = float3(offset.y, offset.z, offset.x);
          p = offset_yzx + pcg_voronoi_corner_sample(iv + offset_yzx, quality);
          mindist = pcg_voronoi_compare(mindist, iv + p, fv - p);

          float3 offset_zxy = float3(offset.z, offset.x, offset.y);
          p = offset_zxy + pcg_voronoi_corner_sample(iv + offset_zxy, quality);
          mindist = pcg_voronoi_compare(mindist, iv + p, fv - p);
        }
      }
    }
  }

  outPosition = mindist.xyz;
  outDistance = sqrt(mindist.w);
}