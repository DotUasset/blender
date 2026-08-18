/* SPDX-FileCopyrightText: 2019-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/* PCG16 3D hash — port of Unreal Engine's Rand3DPCG16 (RandomPCG.ush).
 * Kept in signed ints to match this codebase's hash_pcg3d_i convention
 * (gpu_shader_common_hash.glsl), then reinterpreted as uint per-component
 * for the final logical shift, since this dialect has no uint3 vector type. */

int3 hash_pcg16_3d_i(int3 p)
{
  int3 v = p * 1664525 + 1013904223;

  v.x += v.y * v.z;
  v.y += v.z * v.x;
  v.z += v.x * v.y;
  v.x += v.y * v.z;
  v.y += v.z * v.x;
  v.z += v.x * v.y;

  return v;
}

float3 hash_pcg16_3d_to_vec3(int3 p)
{
  int3 h = hash_pcg16_3d_i(p);
  uint x = uint(h.x) >> 16u;
  uint y = uint(h.y) >> 16u;
  uint z = uint(h.z) >> 16u;
  return float3(float(x), float(y), float(z)) * (1.0f / 65535.0f);
}

[[node]]
void node_tex_pcg_noise(float3 vector, float4 &color)
{
  int3 p = int3(floor(vector));
  color = float4(hash_pcg16_3d_to_vec3(p), 1.0f);
}