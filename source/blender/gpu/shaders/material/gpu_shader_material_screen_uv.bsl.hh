/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "gpu_shader_material_interface.bsl.hh"

/**
 * Screen UV node: outputs the screen space UV coordinates (0-1 range) matching
 * what the Scene Color node uses when its Vector input is unconnected.
 * EEVEE only (forward / blended materials). Evaluates to (0, 0, 0) in any other context.
 */

[[node]]
void node_screen_uv(KernelGlobals &kg, ShadingData &sd, float3 &out_vec)
{
#ifdef GPU_VERTEX_SHADER
  out_vec = float3(0.0f, 0.0f, 0.0f);
#else
  out_vec = node_screen_uv_impl(kg, sd);
#endif
}