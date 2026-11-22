/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "gpu_shader_material_interface.bsl.hh"

/**
 * Scene Depth node: linear view depth of the opaque geometry behind a transparent surface.
 * EEVEE only (forward / blended materials). Evaluates to 0 in any other context.
 */

/* Used when the Vector input is linked. The UV is expected in the [0..1] range. */
[[node]]
void node_scene_depth(float3 vec, KernelGlobals &kg, ShadingData &sd, float &out_depth)
{
#ifdef GPU_VERTEX_SHADER
  out_depth = 0.0f;
#else
  out_depth = node_scene_depth_impl(kg, sd, vec.xy, false);
#endif
}

/* Used when the Vector input is not linked: defaults to the screen space UV. */
[[node]]
void node_scene_depth_screen_uv(float3 vec, KernelGlobals &kg, ShadingData &sd, float &out_depth)
{
#ifdef GPU_VERTEX_SHADER
  out_depth = 0.0f;
#else
  out_depth = node_scene_depth_impl(kg, sd, float2(0.0f), true);
#endif
}
