/* SPDX-FileCopyrightText: 2019-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/* PCG Jacobian Simplex — port of Unreal Engine's JacobianSimplex_ALU + SimplexCorners +
 * SimplexSmooth/SimplexDSmooth + MGradient (Random.ush), using Rand3DPCG16.
 * bTiling always false. No matrix types used. */

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

float3 pcg_mgradient_direction(uint r)
{
  float x = float(r & 0x8000u) * (1.0f / 16384.0f) - 1.0f;
  float y = float(r & 0x4000u) * (1.0f / 8192.0f) - 1.0f;
  float z = float(r & 0x2000u) * (1.0f / 4096.0f) - 1.0f;
  return float3(x, y, z);
}

void pcg_simplex_corners(float3 v, out float3 T0, out float3 T1, out float3 T2, out float3 T3)
{
  float3 tet = floor(v + float3(v.x / 3.0f, v.y / 3.0f, v.z / 3.0f));
  float3 base = tet - float3(tet.x / 6.0f, tet.y / 6.0f, tet.z / 6.0f);
  float3 f = v - base;

  float3 f_yzx = float3(f.y, f.z, f.x);
  float3 g = float3(f.x >= f_yzx.x ? 1.0f : 0.0f,
                    f.y >= f_yzx.y ? 1.0f : 0.0f,
                    f.z >= f_yzx.z ? 1.0f : 0.0f);
  float3 h = float3(1.0f - g.z, 1.0f - g.x, 1.0f - g.y);

  float3 a1 = float3(min(g.x, h.x), min(g.y, h.y), min(g.z, h.z)) - (1.0f / 6.0f);
  float3 a2 = float3(max(g.x, h.x), max(g.y, h.y), max(g.z, h.z)) - (1.0f / 3.0f);

  T0 = base;
  T1 = base + a1;
  T2 = base + a2;
  T3 = base + 0.5f;
}

[[node]]
void node_tex_pcg_jacobian_simplex(float3 vector,
                                   float4 &outNoise,
                                   float4 &outGradient,
                                   float4 &outCurl)
{
  float3 T[4];
  pcg_simplex_corners(vector, T[0], T[1], T[2], T[3]);

  float3 fv[4];
  float3 gvec[3][4];
  float grad[3][4];

  for (int c = 0; c < 4; c++) {
    fv[c] = vector - T[c];

    float3 scaled = 6.0f * T[c] + 0.5f;
    int3 ip = int3(floor(scaled));
    int3 rand = pcg16_hash_i(ip);

    gvec[0][c] = pcg_mgradient_direction(uint(rand.x));
    gvec[1][c] = pcg_mgradient_direction(uint(rand.y));
    gvec[2][c] = pcg_mgradient_direction(uint(rand.z));

    grad[0][c] = dot(gvec[0][c], fv[c]);
    grad[1][c] = dot(gvec[1][c], fv[c]);
    grad[2][c] = dot(gvec[2][c], fv[c]);
  }

  const float scale = 1024.0f / 375.0f;
  float sv[4], dsx[4], dsy[4], dsz[4];

  for (int c = 0; c < 4; c++) {
    float d = dot(fv[c], fv[c]);
    float s = clamp(2.0f * d, 0.0f, 1.0f);

    sv[c] = scale + s * (-3.0f * scale + s * (3.0f * scale - s * scale));

    float dsscale = -12.0f * scale + s * (24.0f * scale - s * 12.0f * scale);
    dsx[c] = dsscale * fv[c].x;
    dsy[c] = dsscale * fv[c].y;
    dsz[c] = dsscale * fv[c].z;
  }

  float3 gradOut[3];
  float noiseOut[3];

  for (int k = 0; k < 3; k++) {
    float gx = 0.0f, gy = 0.0f, gz = 0.0f, w = 0.0f;
    for (int c = 0; c < 4; c++) {
      gx += sv[c] * gvec[k][c].x + dsx[c] * grad[k][c];
      gy += sv[c] * gvec[k][c].y + dsy[c] * grad[k][c];
      gz += sv[c] * gvec[k][c].z + dsz[c] * grad[k][c];
      w += sv[c] * grad[k][c];
    }
    gradOut[k] = float3(gx, gy, gz);
    noiseOut[k] = w;
  }

  outNoise = float4(noiseOut[0], noiseOut[1], noiseOut[2], 1.0f);
  outGradient = float4(gradOut[0], noiseOut[0]);
  outCurl = float4(gradOut[2].y - gradOut[1].z,
                   gradOut[0].z - gradOut[2].x,
                   gradOut[1].x - gradOut[0].y,
                   1.0f);
}