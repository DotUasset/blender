/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup shdnodes
 */

#include "node_shader_util.hh"

namespace blender {

namespace nodes::node_shader_scene_depth_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Vector>("Vector"_ustr)
      .description("Normalized (0-1) coordinates to sample from. Defaults to screen space UVs")
      .hide_value();
  b.add_output<decl::Float>("Depth"_ustr)
      .description("Distance along the view axis of the opaque geometry behind the surface");
}

static int node_shader_gpu_scene_depth(GPUMaterial *mat,
                                       bNode *node,
                                       bNodeExecData * /*execdata*/,
                                       GPUNodeStack *in,
                                       GPUNodeStack *out)
{
  /* EEVEE binds the previous layer HiZ only for materials flagged as transparent
   * (forward pipeline). Reuse that flag instead of introducing a new one. */
  GPU_material_flag_set(mat, GPU_MATFLAG_TRANSPARENT);

  const char *function_name = in[0].link ? "node_scene_depth" : "node_scene_depth_screen_uv";
  return GPU_stack_link(
      mat, node, function_name, in, out, GPU_kernel_globals(), GPU_shading_data());
}

}  // namespace nodes::node_shader_scene_depth_cc

/* node type definition */
void register_node_type_sh_scene_depth()
{
  namespace file_ns = nodes::node_shader_scene_depth_cc;

  static bke::bNodeType ntype;

  sh_node_type_base(&ntype, "ShaderNodeSceneDepth"_ustr, SH_NODE_SCENE_DEPTH);
  ntype.ui_name = "Scene Depth";
  ntype.ui_description =
      "Distance from the camera to the opaque geometry rendered behind the surface, for soft "
      "edges and depth based effects.\nNote: only supported in EEVEE, with Blended materials";
  ntype.enum_name_legacy = "SCENE_DEPTH";
  ntype.nclass = NODE_CLASS_INPUT;
  ntype.declare = file_ns::node_declare;
  ntype.add_ui_poll = object_eevee_shader_nodes_poll;
  ntype.gpu_fn = file_ns::node_shader_gpu_scene_depth;

  bke::node_register_type(ntype);
}

}  // namespace blender
