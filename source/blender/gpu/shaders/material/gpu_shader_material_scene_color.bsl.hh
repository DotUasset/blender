/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "gpu_shader_material_interface.bsl.hh"

/**
 * Scene Color node: samples the already rendered scene color behind a transparent surface.
 * EEVEE only (forward / blended materials). Evaluates to black in any other context.
 */

/* Used when the Vector input is linked. The UV is expected in the [0..1] range. */
[[node]]
void node_scene_color(float3 vec, KernelGlobals &kg, ShadingData &sd, float4 &out_color)
{
#ifdef GPU_VERTEX_SHADER
  out_color = float4(0.0f, 0.0f, 0.0f, 1.0f);
#else
  out_color = float4(node_scene_color_impl(kg, sd, vec.xy, false), 1.0f);
#endif
}

/* Used when the Vector input is not linked: defaults to the screen space UV. */
[[node]]
void node_scene_color_screen_uv(float3 vec, KernelGlobals &kg, ShadingData &sd, float4 &out_color)
{
#ifdef GPU_VERTEX_SHADER
  out_color = float4(0.0f, 0.0f, 0.0f, 1.0f);
#else
  out_color = float4(node_scene_color_impl(kg, sd, float2(0.0f), true), 1.0f);
#endif
}
